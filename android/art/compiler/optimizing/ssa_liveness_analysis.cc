/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ssa_liveness_analysis.h"

#include "base/bit_vector-inl.h"
#include "code_generator.h"
#include "linear_order.h"
#include "nodes.h"

namespace art {

void SsaLivenessAnalysis::Analyze() {
  // Compute the linear order directly in the graph's data structure
  // (there are no more following graph mutations).
  LinearizeGraph(graph_, &graph_->linear_order_);

  // Liveness analysis.
  NumberInstructions();
  ComputeLiveness();
}

void SsaLivenessAnalysis::NumberInstructions() {
  int ssa_index = 0;
  size_t lifetime_position = 0;
  // Each instruction gets a lifetime position, and a block gets a lifetime
  // start and end position. Non-phi instructions have a distinct lifetime position than
  // the block they are in. Phi instructions have the lifetime start of their block as
  // lifetime position.
  //
  // Because the register allocator will insert moves in the graph, we need
  // to differentiate between the start and end of an instruction. Adding 2 to
  // the lifetime position for each instruction ensures the start of an
  // instruction is different than the end of the previous instruction.
  for (HBasicBlock* block : graph_->GetLinearOrder()) {
    block->SetLifetimeStart(lifetime_position);

    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HInstruction* current = inst_it.Current();
      codegen_->AllocateLocations(current);
      LocationSummary* locations = current->GetLocations();
      if (locations != nullptr && locations->Out().IsValid()) {
        instructions_from_ssa_index_.push_back(current);
        current->SetSsaIndex(ssa_index++);
        current->SetLiveInterval(
            LiveInterval::MakeInterval(allocator_, current->GetType(), current));
      }
      current->SetLifetimePosition(lifetime_position);
    }
    lifetime_position += 2;

    // Add a null marker to notify we are starting a block.
    instructions_from_lifetime_position_.push_back(nullptr);

    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      HInstruction* current = inst_it.Current();
      codegen_->AllocateLocations(current);
      LocationSummary* locations = current->GetLocations();
      if (locations != nullptr && locations->Out().IsValid()) {
        instructions_from_ssa_index_.push_back(current);
        current->SetSsaIndex(ssa_index++);
        current->SetLiveInterval(
            LiveInterval::MakeInterval(allocator_, current->GetType(), current));
      }
      instructions_from_lifetime_position_.push_back(current);
      current->SetLifetimePosition(lifetime_position);
      lifetime_position += 2;
    }

    block->SetLifetimeEnd(lifetime_position);
  }
  number_of_ssa_values_ = ssa_index;
}

void SsaLivenessAnalysis::ComputeLiveness() {
  for (HBasicBlock* block : graph_->GetLinearOrder()) {
    block_infos_[block->GetBlockId()] =
        new (allocator_) BlockInfo(allocator_, *block, number_of_ssa_values_);
  }

  // Compute the live ranges, as well as the initial live_in, live_out, and kill sets.
  // This method does not handle backward branches for the sets, therefore live_in
  // and live_out sets are not yet correct.
  ComputeLiveRanges();

  // Do a fixed point calculation to take into account backward branches,
  // that will update live_in of loop headers, and therefore live_out and live_in
  // of blocks in the loop.
  ComputeLiveInAndLiveOutSets();
}

static void RecursivelyProcessInputs(HInstruction* current,
                                     HInstruction* actual_user,
                                     BitVector* live_in) {
  HInputsRef inputs = current->GetInputs();
  for (size_t i = 0; i < inputs.size(); ++i) {
    HInstruction* input = inputs[i];
    bool has_in_location = current->GetLocations()->InAt(i).IsValid();
    bool has_out_location = input->GetLocations()->Out().IsValid();

    if (has_in_location) {
      DCHECK(has_out_location)
          << "Instruction " << current->DebugName() << current->GetId()
          << " expects an input value at index " << i << " but "
          << input->DebugName() << input->GetId() << " does not produce one.";
      DCHECK(input->HasSsaIndex());
      // `input` generates a result used by `current`. Add use and update
      // the live-in set.
      input->GetLiveInterval()->AddUse(current, /* environment */ nullptr, i, actual_user);
      live_in->SetBit(input->GetSsaIndex());
    } else if (has_out_location) {
      // `input` generates a result but it is not used by `current`.
    } else {
      // `input` is inlined into `current`. Walk over its inputs and record
      // uses at `current`.
      DCHECK(input->IsEmittedAtUseSite());
      // Check that the inlined input is not a phi. Recursing on loop phis could
      // lead to an infinite loop.
      DCHECK(!input->IsPhi());
      RecursivelyProcessInputs(input, actual_user, live_in);
    }
  }
}

void SsaLivenessAnalysis::ComputeLiveRanges() {
  // Do a post order visit, adding inputs of instructions live in the block where
  // that instruction is defined, and killing instructions that are being visited.
  for (HBasicBlock* block : ReverseRange(graph_->GetLinearOrder())) {
    BitVector* kill = GetKillSet(*block);
    BitVector* live_in = GetLiveInSet(*block);

    // Set phi inputs of successors of this block corresponding to this block
    // as live_in.
    for (HBasicBlock* successor : block->GetSuccessors()) {
      live_in->Union(GetLiveInSet(*successor));
      if (successor->IsCatchBlock()) {
        // Inputs of catch phis will be kept alive through their environment
        // uses, allowing the runtime to copy their values to the corresponding
        // catch phi spill slots when an exception is thrown.
        // The only instructions which may not be recorded in the environments
        // are constants created by the SSA builder as typed equivalents of
        // untyped constants from the bytecode, or phis with only such constants
        // as inputs (verified by GraphChecker). Their raw binary value must
        // therefore be the same and we only need to keep alive one.
      } else {
        size_t phi_input_index = successor->GetPredecessorIndexOf(block);
        for (HInstructionIterator phi_it(successor->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
          HInstruction* phi = phi_it.Current();
          HInstruction* input = phi->InputAt(phi_input_index);
          input->GetLiveInterval()->AddPhiUse(phi, phi_input_index, block);
          // A phi input whose last user is the phi dies at the end of the predecessor block,
          // and not at the phi's lifetime position.
          live_in->SetBit(input->GetSsaIndex());
        }
      }
    }

    // Add a range that covers this block to all instructions live_in because of successors.
    // Instructions defined in this block will have their start of the range adjusted.
    for (uint32_t idx : live_in->Indexes()) {
      HInstruction* current = GetInstructionFromSsaIndex(idx);
      current->GetLiveInterval()->AddRange(block->GetLifetimeStart(), block->GetLifetimeEnd());
    }

    for (HBackwardInstructionIterator back_it(block->GetInstructions()); !back_it.Done();
         back_it.Advance()) {
      HInstruction* current = back_it.Current();
      if (current->HasSsaIndex()) {
        // Kill the instruction and shorten its interval.
        kill->SetBit(current->GetSsaIndex());
        live_in->ClearBit(current->GetSsaIndex());
        current->GetLiveInterval()->SetFrom(current->GetLifetimePosition());
      }

      // Process the environment first, because we know their uses come after
      // or at the same liveness position of inputs.
      for (HEnvironment* environment = current->GetEnvironment();
           environment != nullptr;
           environment = environment->GetParent()) {
        // Handle environment uses. See statements (b) and (c) of the
        // SsaLivenessAnalysis.
        for (size_t i = 0, e = environment->Size(); i < e; ++i) {
          HInstruction* instruction = environment->GetInstructionAt(i);
          bool should_be_live = ShouldBeLiveForEnvironment(current, instruction);
          if (should_be_live) {
            CHECK(instruction->HasSsaIndex()) << instruction->DebugName();
            live_in->SetBit(instruction->GetSsaIndex());
          }
          if (instruction != nullptr) {
            instruction->GetLiveInterval()->AddUse(
                current, environment, i, /* actual_user */ nullptr, should_be_live);
          }
        }
      }

      // Process inputs of instructions.
      if (current->IsEmittedAtUseSite()) {
        if (kIsDebugBuild) {
          DCHECK(!current->GetLocations()->Out().IsValid());
          for (const HUseListNode<HInstruction*>& use : current->GetUses()) {
            HInstruction* user = use.GetUser();
            size_t index = use.GetIndex();
            DCHECK(!user->GetLocations()->InAt(index).IsValid());
          }
          DCHECK(!current->HasEnvironmentUses());
        }
      } else {
        RecursivelyProcessInputs(current, current, live_in);
      }
    }

    // Kill phis defined in this block.
    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HInstruction* current = inst_it.Current();
      if (current->HasSsaIndex()) {
        kill->SetBit(current->GetSsaIndex());
        live_in->ClearBit(current->GetSsaIndex());
        LiveInterval* interval = current->GetLiveInterval();
        DCHECK((interval->GetFirstRange() == nullptr)
               || (interval->GetStart() == current->GetLifetimePosition()));
        interval->SetFrom(current->GetLifetimePosition());
      }
    }

    if (block->IsLoopHeader()) {
      if (kIsDebugBuild) {
        CheckNoLiveInIrreducibleLoop(*block);
      }
      size_t last_position = block->GetLoopInformation()->GetLifetimeEnd();
      // For all live_in instructions at the loop header, we need to create a range
      // that covers the full loop.
      for (uint32_t idx : live_in->Indexes()) {
        HInstruction* current = GetInstructionFromSsaIndex(idx);
        current->GetLiveInterval()->AddLoopRange(block->GetLifetimeStart(), last_position);
      }
    }
  }
}

void SsaLivenessAnalysis::ComputeLiveInAndLiveOutSets() {
  bool changed;
  do {
    changed = false;

    for (const HBasicBlock* block : graph_->GetPostOrder()) {
      // The live_in set depends on the kill set (which does not
      // change in this loop), and the live_out set.  If the live_out
      // set does not change, there is no need to update the live_in set.
      if (UpdateLiveOut(*block) && UpdateLiveIn(*block)) {
        if (kIsDebugBuild) {
          CheckNoLiveInIrreducibleLoop(*block);
        }
        changed = true;
      }
    }
  } while (changed);
}

bool SsaLivenessAnalysis::UpdateLiveOut(const HBasicBlock& block) {
  BitVector* live_out = GetLiveOutSet(block);
  bool changed = false;
  // The live_out set of a block is the union of live_in sets of its successors.
  for (HBasicBlock* successor : block.GetSuccessors()) {
    if (live_out->Union(GetLiveInSet(*successor))) {
      changed = true;
    }
  }
  return changed;
}


bool SsaLivenessAnalysis::UpdateLiveIn(const HBasicBlock& block) {
  BitVector* live_out = GetLiveOutSet(block);
  BitVector* kill = GetKillSet(block);
  BitVector* live_in = GetLiveInSet(block);
  // If live_out is updated (because of backward branches), we need to make
  // sure instructions in live_out are also in live_in, unless they are killed
  // by this block.
  return live_in->UnionIfNotIn(live_out, kill);
}

void LiveInterval::DumpWithContext(std::ostream& stream,
                                   const CodeGenerator& codegen) const {
  Dump(stream);
  if (IsFixed()) {
    stream << ", register:" << GetRegister() << "(";
    if (IsFloatingPoint()) {
      codegen.DumpFloatingPointRegister(stream, GetRegister());
    } else {
      codegen.DumpCoreRegister(stream, GetRegister());
    }
    stream << ")";
  } else {
    stream << ", spill slot:" << GetSpillSlot();
  }
  stream << ", requires_register:" << (GetDefinedBy() != nullptr && RequiresRegister());
  if (GetParent()->GetDefinedBy() != nullptr) {
    stream << ", defined_by:" << GetParent()->GetDefinedBy()->GetKind();
    stream << "(" << GetParent()->GetDefinedBy()->GetLifetimePosition() << ")";
  }
}

static int RegisterOrLowRegister(Location location) {
  return location.IsPair() ? location.low() : location.reg();
}

int LiveInterval::FindFirstRegisterHint(size_t* free_until,
                                        const SsaLivenessAnalysis& liveness) const {
  DCHECK(!IsHighInterval());
  if (IsTemp()) return kNoRegister;

  if (GetParent() == this && defined_by_ != nullptr) {
    // This is the first interval for the instruction. Try to find
    // a register based on its definition.
    DCHECK_EQ(defined_by_->GetLiveInterval(), this);
    int hint = FindHintAtDefinition();
    if (hint != kNoRegister && free_until[hint] > GetStart()) {
      return hint;
    }
  }

  if (IsSplit() && liveness.IsAtBlockBoundary(GetStart() / 2)) {
    // If the start of this interval is at a block boundary, we look at the
    // location of the interval in blocks preceding the block this interval
    // starts at. If one location is a register we return it as a hint. This
    // will avoid a move between the two blocks.
    HBasicBlock* block = liveness.GetBlockFromPosition(GetStart() / 2);
    size_t next_register_use = FirstRegisterUse();
    for (HBasicBlock* predecessor : block->GetPredecessors()) {
      size_t position = predecessor->GetLifetimeEnd() - 1;
      // We know positions above GetStart() do not have a location yet.
      if (position < GetStart()) {
        LiveInterval* existing = GetParent()->GetSiblingAt(position);
        if (existing != nullptr
            && existing->HasRegister()
            // It's worth using that register if it is available until
            // the next use.
            && (free_until[existing->GetRegister()] >= next_register_use)) {
          return existing->GetRegister();
        }
      }
    }
  }

  size_t start = GetStart();
  size_t end = GetEnd();
  for (const UsePosition& use : GetUses()) {
    size_t use_position = use.GetPosition();
    if (use_position > end) {
      break;
    }
    if (use_position >= start && !use.IsSynthesized()) {
      HInstruction* user = use.GetUser();
      size_t input_index = use.GetInputIndex();
      if (user->IsPhi()) {
        // If the phi has a register, try to use the same.
        Location phi_location = user->GetLiveInterval()->ToLocation();
        if (phi_location.IsRegisterKind()) {
          DCHECK(SameRegisterKind(phi_location));
          int reg = RegisterOrLowRegister(phi_location);
          if (free_until[reg] >= use_position) {
            return reg;
          }
        }
        // If the instruction dies at the phi assignment, we can try having the
        // same register.
        if (end == user->GetBlock()->GetPredecessors()[input_index]->GetLifetimeEnd()) {
          HInputsRef inputs = user->GetInputs();
          for (size_t i = 0; i < inputs.size(); ++i) {
            if (i == input_index) {
              continue;
            }
            Location location = inputs[i]->GetLiveInterval()->GetLocationAt(
                user->GetBlock()->GetPredecessors()[i]->GetLifetimeEnd() - 1);
            if (location.IsRegisterKind()) {
              int reg = RegisterOrLowRegister(location);
              if (free_until[reg] >= use_position) {
                return reg;
              }
            }
          }
        }
      } else {
        // If the instruction is expected in a register, try to use it.
        LocationSummary* locations = user->GetLocations();
        Location expected = locations->InAt(use.GetInputIndex());
        // We use the user's lifetime position - 1 (and not `use_position`) because the
        // register is blocked at the beginning of the user.
        size_t position = user->GetLifetimePosition() - 1;
        if (expected.IsRegisterKind()) {
          DCHECK(SameRegisterKind(expected));
          int reg = RegisterOrLowRegister(expected);
          if (free_until[reg] >= position) {
            return reg;
          }
        }
      }
    }
  }

  return kNoRegister;
}

int LiveInterval::FindHintAtDefinition() const {
  if (defined_by_->IsPhi()) {
    // Try to use the same register as one of the inputs.
    const ArenaVector<HBasicBlock*>& predecessors = defined_by_->GetBlock()->GetPredecessors();
    HInputsRef inputs = defined_by_->GetInputs();
    for (size_t i = 0; i < inputs.size(); ++i) {
      size_t end = predecessors[i]->GetLifetimeEnd();
      LiveInterval* input_interval = inputs[i]->GetLiveInterval()->GetSiblingAt(end - 1);
      if (input_interval->GetEnd() == end) {
        // If the input dies at the end of the predecessor, we know its register can
        // be reused.
        Location input_location = input_interval->ToLocation();
        if (input_location.IsRegisterKind()) {
          DCHECK(SameRegisterKind(input_location));
          return RegisterOrLowRegister(input_location);
        }
      }
    }
  } else {
    LocationSummary* locations = GetDefinedBy()->GetLocations();
    Location out = locations->Out();
    if (out.IsUnallocated() && out.GetPolicy() == Location::kSameAsFirstInput) {
      // Try to use the same register as the first input.
      LiveInterval* input_interval =
          GetDefinedBy()->InputAt(0)->GetLiveInterval()->GetSiblingAt(GetStart() - 1);
      if (input_interval->GetEnd() == GetStart()) {
        // If the input dies at the start of this instruction, we know its register can
        // be reused.
        Location location = input_interval->ToLocation();
        if (location.IsRegisterKind()) {
          DCHECK(SameRegisterKind(location));
          return RegisterOrLowRegister(location);
        }
      }
    }
  }
  return kNoRegister;
}

bool LiveInterval::SameRegisterKind(Location other) const {
  if (IsFloatingPoint()) {
    if (IsLowInterval() || IsHighInterval()) {
      return other.IsFpuRegisterPair();
    } else {
      return other.IsFpuRegister();
    }
  } else {
    if (IsLowInterval() || IsHighInterval()) {
      return other.IsRegisterPair();
    } else {
      return other.IsRegister();
    }
  }
}

size_t LiveInterval::NumberOfSpillSlotsNeeded() const {
  // For a SIMD operation, compute the number of needed spill slots.
  // TODO: do through vector type?
  HInstruction* definition = GetParent()->GetDefinedBy();
  if (definition != nullptr && HVecOperation::ReturnsSIMDValue(definition)) {
    if (definition->IsPhi()) {
      definition = definition->InputAt(1);  // SIMD always appears on back-edge
    }
    return definition->AsVecOperation()->GetVectorNumberOfBytes() / kVRegSize;
  }
  // Return number of needed spill slots based on type.
  return (type_ == DataType::Type::kInt64 || type_ == DataType::Type::kFloat64) ? 2 : 1;
}

Location LiveInterval::ToLocation() const {
  DCHECK(!IsHighInterval());
  if (HasRegister()) {
    if (IsFloatingPoint()) {
      if (HasHighInterval()) {
        return Location::FpuRegisterPairLocation(GetRegister(), GetHighInterval()->GetRegister());
      } else {
        return Location::FpuRegisterLocation(GetRegister());
      }
    } else {
      if (HasHighInterval()) {
        return Location::RegisterPairLocation(GetRegister(), GetHighInterval()->GetRegister());
      } else {
        return Location::RegisterLocation(GetRegister());
      }
    }
  } else {
    HInstruction* defined_by = GetParent()->GetDefinedBy();
    if (defined_by->IsConstant()) {
      return defined_by->GetLocations()->Out();
    } else if (GetParent()->HasSpillSlot()) {
      switch (NumberOfSpillSlotsNeeded()) {
        case 1: return Location::StackSlot(GetParent()->GetSpillSlot());
        case 2: return Location::DoubleStackSlot(GetParent()->GetSpillSlot());
        case 4: return Location::SIMDStackSlot(GetParent()->GetSpillSlot());
        default: LOG(FATAL) << "Unexpected number of spill slots"; UNREACHABLE();
      }
    } else {
      return Location();
    }
  }
}

Location LiveInterval::GetLocationAt(size_t position) {
  LiveInterval* sibling = GetSiblingAt(position);
  DCHECK(sibling != nullptr);
  return sibling->ToLocation();
}

LiveInterval* LiveInterval::GetSiblingAt(size_t position) {
  LiveInterval* current = this;
  while (current != nullptr && !current->IsDefinedAt(position)) {
    current = current->GetNextSibling();
  }
  return current;
}

}  // namespace art
