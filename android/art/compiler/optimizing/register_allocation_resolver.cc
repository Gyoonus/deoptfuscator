/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "register_allocation_resolver.h"

#include "base/bit_vector-inl.h"
#include "code_generator.h"
#include "linear_order.h"
#include "ssa_liveness_analysis.h"

namespace art {

RegisterAllocationResolver::RegisterAllocationResolver(CodeGenerator* codegen,
                                                       const SsaLivenessAnalysis& liveness)
      : allocator_(codegen->GetGraph()->GetAllocator()),
        codegen_(codegen),
        liveness_(liveness) {}

void RegisterAllocationResolver::Resolve(ArrayRef<HInstruction* const> safepoints,
                                         size_t reserved_out_slots,
                                         size_t int_spill_slots,
                                         size_t long_spill_slots,
                                         size_t float_spill_slots,
                                         size_t double_spill_slots,
                                         size_t catch_phi_spill_slots,
                                         ArrayRef<LiveInterval* const> temp_intervals) {
  size_t spill_slots = int_spill_slots
                     + long_spill_slots
                     + float_spill_slots
                     + double_spill_slots
                     + catch_phi_spill_slots;

  // Update safepoints and calculate the size of the spills.
  UpdateSafepointLiveRegisters();
  size_t maximum_safepoint_spill_size = CalculateMaximumSafepointSpillSize(safepoints);

  // Computes frame size and spill mask.
  codegen_->InitializeCodeGeneration(spill_slots,
                                     maximum_safepoint_spill_size,
                                     reserved_out_slots,  // Includes slot(s) for the art method.
                                     codegen_->GetGraph()->GetLinearOrder());

  // Resolve outputs, including stack locations.
  // TODO: Use pointers of Location inside LiveInterval to avoid doing another iteration.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    LiveInterval* current = instruction->GetLiveInterval();
    LocationSummary* locations = instruction->GetLocations();
    Location location = locations->Out();
    if (instruction->IsParameterValue()) {
      // Now that we know the frame size, adjust the parameter's location.
      if (location.IsStackSlot()) {
        location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
        current->SetSpillSlot(location.GetStackIndex());
        locations->UpdateOut(location);
      } else if (location.IsDoubleStackSlot()) {
        location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
        current->SetSpillSlot(location.GetStackIndex());
        locations->UpdateOut(location);
      } else if (current->HasSpillSlot()) {
        current->SetSpillSlot(current->GetSpillSlot() + codegen_->GetFrameSize());
      }
    } else if (instruction->IsCurrentMethod()) {
      // The current method is always at offset 0.
      DCHECK(!current->HasSpillSlot() || (current->GetSpillSlot() == 0));
    } else if (instruction->IsPhi() && instruction->AsPhi()->IsCatchPhi()) {
      DCHECK(current->HasSpillSlot());
      size_t slot = current->GetSpillSlot()
                    + spill_slots
                    + reserved_out_slots
                    - catch_phi_spill_slots;
      current->SetSpillSlot(slot * kVRegSize);
    } else if (current->HasSpillSlot()) {
      // Adjust the stack slot, now that we know the number of them for each type.
      // The way this implementation lays out the stack is the following:
      // [parameter slots       ]
      // [art method (caller)   ]
      // [entry spill (core)    ]
      // [entry spill (float)   ]
      // [should_deoptimize flag] (this is optional)
      // [catch phi spill slots ]
      // [double spill slots    ]
      // [long spill slots      ]
      // [float spill slots     ]
      // [int/ref values        ]
      // [maximum out values    ] (number of arguments for calls)
      // [art method            ].
      size_t slot = current->GetSpillSlot();
      switch (current->GetType()) {
        case DataType::Type::kFloat64:
          slot += long_spill_slots;
          FALLTHROUGH_INTENDED;
        case DataType::Type::kUint64:
        case DataType::Type::kInt64:
          slot += float_spill_slots;
          FALLTHROUGH_INTENDED;
        case DataType::Type::kFloat32:
          slot += int_spill_slots;
          FALLTHROUGH_INTENDED;
        case DataType::Type::kReference:
        case DataType::Type::kUint32:
        case DataType::Type::kInt32:
        case DataType::Type::kUint16:
        case DataType::Type::kUint8:
        case DataType::Type::kInt8:
        case DataType::Type::kBool:
        case DataType::Type::kInt16:
          slot += reserved_out_slots;
          break;
        case DataType::Type::kVoid:
          LOG(FATAL) << "Unexpected type for interval " << current->GetType();
      }
      current->SetSpillSlot(slot * kVRegSize);
    }

    Location source = current->ToLocation();

    if (location.IsUnallocated()) {
      if (location.GetPolicy() == Location::kSameAsFirstInput) {
        if (locations->InAt(0).IsUnallocated()) {
          locations->SetInAt(0, source);
        } else {
          DCHECK(locations->InAt(0).Equals(source));
        }
      }
      locations->UpdateOut(source);
    } else {
      DCHECK(source.Equals(location));
    }
  }

  // Connect siblings and resolve inputs.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    ConnectSiblings(instruction->GetLiveInterval());
  }

  // Resolve non-linear control flow across branches. Order does not matter.
  for (HBasicBlock* block : codegen_->GetGraph()->GetLinearOrder()) {
    if (block->IsCatchBlock() ||
        (block->IsLoopHeader() && block->GetLoopInformation()->IsIrreducible())) {
      // Instructions live at the top of catch blocks or irreducible loop header
      // were forced to spill.
      if (kIsDebugBuild) {
        BitVector* live = liveness_.GetLiveInSet(*block);
        for (uint32_t idx : live->Indexes()) {
          LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
          LiveInterval* sibling = interval->GetSiblingAt(block->GetLifetimeStart());
          // `GetSiblingAt` returns the sibling that contains a position, but there could be
          // a lifetime hole in it. `CoversSlow` returns whether the interval is live at that
          // position.
          if ((sibling != nullptr) && sibling->CoversSlow(block->GetLifetimeStart())) {
            DCHECK(!sibling->HasRegister());
          }
        }
      }
    } else {
      BitVector* live = liveness_.GetLiveInSet(*block);
      for (uint32_t idx : live->Indexes()) {
        LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
        for (HBasicBlock* predecessor : block->GetPredecessors()) {
          ConnectSplitSiblings(interval, predecessor, block);
        }
      }
    }
  }

  // Resolve phi inputs. Order does not matter.
  for (HBasicBlock* block : codegen_->GetGraph()->GetLinearOrder()) {
    if (block->IsCatchBlock()) {
      // Catch phi values are set at runtime by the exception delivery mechanism.
    } else {
      for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
        HInstruction* phi = inst_it.Current();
        for (size_t i = 0, e = block->GetPredecessors().size(); i < e; ++i) {
          HBasicBlock* predecessor = block->GetPredecessors()[i];
          DCHECK_EQ(predecessor->GetNormalSuccessors().size(), 1u);
          HInstruction* input = phi->InputAt(i);
          Location source = input->GetLiveInterval()->GetLocationAt(
              predecessor->GetLifetimeEnd() - 1);
          Location destination = phi->GetLiveInterval()->ToLocation();
          InsertParallelMoveAtExitOf(predecessor, phi, source, destination);
        }
      }
    }
  }

  // Resolve temp locations.
  for (LiveInterval* temp : temp_intervals) {
    if (temp->IsHighInterval()) {
      // High intervals can be skipped, they are already handled by the low interval.
      continue;
    }
    HInstruction* at = liveness_.GetTempUser(temp);
    size_t temp_index = liveness_.GetTempIndex(temp);
    LocationSummary* locations = at->GetLocations();
    switch (temp->GetType()) {
      case DataType::Type::kInt32:
        locations->SetTempAt(temp_index, Location::RegisterLocation(temp->GetRegister()));
        break;

      case DataType::Type::kFloat64:
        if (codegen_->NeedsTwoRegisters(DataType::Type::kFloat64)) {
          Location location = Location::FpuRegisterPairLocation(
              temp->GetRegister(), temp->GetHighInterval()->GetRegister());
          locations->SetTempAt(temp_index, location);
        } else {
          locations->SetTempAt(temp_index, Location::FpuRegisterLocation(temp->GetRegister()));
        }
        break;

      default:
        LOG(FATAL) << "Unexpected type for temporary location "
                   << temp->GetType();
    }
  }
}

void RegisterAllocationResolver::UpdateSafepointLiveRegisters() {
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    for (LiveInterval* current = instruction->GetLiveInterval();
         current != nullptr;
         current = current->GetNextSibling()) {
      if (!current->HasRegister()) {
        continue;
      }
      Location source = current->ToLocation();
      for (SafepointPosition* safepoint_position = current->GetFirstSafepoint();
           safepoint_position != nullptr;
           safepoint_position = safepoint_position->GetNext()) {
        DCHECK(current->CoversSlow(safepoint_position->GetPosition()));
        LocationSummary* locations = safepoint_position->GetLocations();
        switch (source.GetKind()) {
          case Location::kRegister:
          case Location::kFpuRegister: {
            locations->AddLiveRegister(source);
            break;
          }
          case Location::kRegisterPair:
          case Location::kFpuRegisterPair: {
            locations->AddLiveRegister(source.ToLow());
            locations->AddLiveRegister(source.ToHigh());
            break;
          }
          case Location::kStackSlot:  // Fall-through
          case Location::kDoubleStackSlot:  // Fall-through
          case Location::kConstant: {
            // Nothing to do.
            break;
          }
          default: {
            LOG(FATAL) << "Unexpected location for object";
          }
        }
      }
    }
  }
}

size_t RegisterAllocationResolver::CalculateMaximumSafepointSpillSize(
    ArrayRef<HInstruction* const> safepoints) {
  size_t core_register_spill_size = codegen_->GetWordSize();
  size_t fp_register_spill_size = codegen_->GetFloatingPointSpillSlotSize();
  size_t maximum_safepoint_spill_size = 0u;
  for (HInstruction* instruction : safepoints) {
    LocationSummary* locations = instruction->GetLocations();
    if (locations->OnlyCallsOnSlowPath()) {
      size_t core_spills =
          codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ true);
      size_t fp_spills =
          codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ false);
      size_t spill_size =
          core_register_spill_size * core_spills + fp_register_spill_size * fp_spills;
      maximum_safepoint_spill_size = std::max(maximum_safepoint_spill_size, spill_size);
    } else if (locations->CallsOnMainAndSlowPath()) {
      // Nothing to spill on the slow path if the main path already clobbers caller-saves.
      DCHECK_EQ(0u, codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ true));
      DCHECK_EQ(0u, codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ false));
    }
  }
  return maximum_safepoint_spill_size;
}

void RegisterAllocationResolver::ConnectSiblings(LiveInterval* interval) {
  LiveInterval* current = interval;
  if (current->HasSpillSlot()
      && current->HasRegister()
      // Currently, we spill unconditionnally the current method in the code generators.
      && !interval->GetDefinedBy()->IsCurrentMethod()) {
    // We spill eagerly, so move must be at definition.
    Location loc;
    switch (interval->NumberOfSpillSlotsNeeded()) {
      case 1: loc = Location::StackSlot(interval->GetParent()->GetSpillSlot()); break;
      case 2: loc = Location::DoubleStackSlot(interval->GetParent()->GetSpillSlot()); break;
      case 4: loc = Location::SIMDStackSlot(interval->GetParent()->GetSpillSlot()); break;
      default: LOG(FATAL) << "Unexpected number of spill slots"; UNREACHABLE();
    }
    InsertMoveAfter(interval->GetDefinedBy(), interval->ToLocation(), loc);
  }
  UsePositionList::const_iterator use_it = current->GetUses().begin();
  const UsePositionList::const_iterator use_end = current->GetUses().end();
  EnvUsePositionList::const_iterator env_use_it = current->GetEnvironmentUses().begin();
  const EnvUsePositionList::const_iterator env_use_end = current->GetEnvironmentUses().end();

  // Walk over all siblings, updating locations of use positions, and
  // connecting them when they are adjacent.
  do {
    Location source = current->ToLocation();

    // Walk over all uses covered by this interval, and update the location
    // information.

    LiveRange* range = current->GetFirstRange();
    while (range != nullptr) {
      // Process uses in the closed interval [range->GetStart(), range->GetEnd()].
      // FindMatchingUseRange() expects a half-open interval, so pass `range->GetEnd() + 1u`.
      size_t range_begin = range->GetStart();
      size_t range_end = range->GetEnd() + 1u;
      auto matching_use_range =
          FindMatchingUseRange(use_it, use_end, range_begin, range_end);
      DCHECK(std::all_of(use_it,
                         matching_use_range.begin(),
                         [](const UsePosition& pos) { return pos.IsSynthesized(); }));
      for (const UsePosition& use : matching_use_range) {
        DCHECK(current->CoversSlow(use.GetPosition()) || (use.GetPosition() == range->GetEnd()));
        if (!use.IsSynthesized()) {
          LocationSummary* locations = use.GetUser()->GetLocations();
          Location expected_location = locations->InAt(use.GetInputIndex());
          // The expected (actual) location may be invalid in case the input is unused. Currently
          // this only happens for intrinsics.
          if (expected_location.IsValid()) {
            if (expected_location.IsUnallocated()) {
              locations->SetInAt(use.GetInputIndex(), source);
            } else if (!expected_location.IsConstant()) {
              AddInputMoveFor(
                  interval->GetDefinedBy(), use.GetUser(), source, expected_location);
            }
          } else {
            DCHECK(use.GetUser()->IsInvoke());
            DCHECK(use.GetUser()->AsInvoke()->GetIntrinsic() != Intrinsics::kNone);
          }
        }
      }
      use_it = matching_use_range.end();

      // Walk over the environment uses, and update their locations.
      auto matching_env_use_range =
          FindMatchingUseRange(env_use_it, env_use_end, range_begin, range_end);
      for (const EnvUsePosition& env_use : matching_env_use_range) {
        DCHECK(current->CoversSlow(env_use.GetPosition())
               || (env_use.GetPosition() == range->GetEnd()));
        HEnvironment* environment = env_use.GetEnvironment();
        environment->SetLocationAt(env_use.GetInputIndex(), source);
      }
      env_use_it = matching_env_use_range.end();

      range = range->GetNext();
    }

    // If the next interval starts just after this one, and has a register,
    // insert a move.
    LiveInterval* next_sibling = current->GetNextSibling();
    if (next_sibling != nullptr
        && next_sibling->HasRegister()
        && current->GetEnd() == next_sibling->GetStart()) {
      Location destination = next_sibling->ToLocation();
      InsertParallelMoveAt(current->GetEnd(), interval->GetDefinedBy(), source, destination);
    }

    for (SafepointPosition* safepoint_position = current->GetFirstSafepoint();
         safepoint_position != nullptr;
         safepoint_position = safepoint_position->GetNext()) {
      DCHECK(current->CoversSlow(safepoint_position->GetPosition()));

      if (current->GetType() == DataType::Type::kReference) {
        DCHECK(interval->GetDefinedBy()->IsActualObject())
            << interval->GetDefinedBy()->DebugName()
            << '(' << interval->GetDefinedBy()->GetId() << ')'
            << "@" << safepoint_position->GetInstruction()->DebugName()
            << '(' << safepoint_position->GetInstruction()->GetId() << ')';
        LocationSummary* locations = safepoint_position->GetLocations();
        if (current->GetParent()->HasSpillSlot()) {
          locations->SetStackBit(current->GetParent()->GetSpillSlot() / kVRegSize);
        }
        if (source.GetKind() == Location::kRegister) {
          locations->SetRegisterBit(source.reg());
        }
      }
    }
    current = next_sibling;
  } while (current != nullptr);

  // Following uses can only be synthesized uses.
  DCHECK(std::all_of(use_it, use_end, [](const UsePosition& pos) { return pos.IsSynthesized(); }));
}

static bool IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(
    HInstruction* instruction) {
  return instruction->GetBlock()->GetGraph()->HasIrreducibleLoops() &&
         (instruction->IsConstant() || instruction->IsCurrentMethod());
}

void RegisterAllocationResolver::ConnectSplitSiblings(LiveInterval* interval,
                                                      HBasicBlock* from,
                                                      HBasicBlock* to) const {
  if (interval->GetNextSibling() == nullptr) {
    // Nothing to connect. The whole range was allocated to the same location.
    return;
  }

  // Find the intervals that cover `from` and `to`.
  size_t destination_position = to->GetLifetimeStart();
  size_t source_position = from->GetLifetimeEnd() - 1;
  LiveInterval* destination = interval->GetSiblingAt(destination_position);
  LiveInterval* source = interval->GetSiblingAt(source_position);

  if (destination == source) {
    // Interval was not split.
    return;
  }

  LiveInterval* parent = interval->GetParent();
  HInstruction* defined_by = parent->GetDefinedBy();
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (destination == nullptr || !destination->CoversSlow(destination_position))) {
    // Our live_in fixed point calculation has found that the instruction is live
    // in the `to` block because it will eventually enter an irreducible loop. Our
    // live interval computation however does not compute a fixed point, and
    // therefore will not have a location for that instruction for `to`.
    // Because the instruction is a constant or the ArtMethod, we don't need to
    // do anything: it will be materialized in the irreducible loop.
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by))
        << defined_by->DebugName() << ":" << defined_by->GetId()
        << " " << from->GetBlockId() << " -> " << to->GetBlockId();
    return;
  }

  if (!destination->HasRegister()) {
    // Values are eagerly spilled. Spill slot already contains appropriate value.
    return;
  }

  Location location_source;
  // `GetSiblingAt` returns the interval whose start and end cover `position`,
  // but does not check whether the interval is inactive at that position.
  // The only situation where the interval is inactive at that position is in the
  // presence of irreducible loops for constants and ArtMethod.
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (source == nullptr || !source->CoversSlow(source_position))) {
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by));
    if (defined_by->IsConstant()) {
      location_source = defined_by->GetLocations()->Out();
    } else {
      DCHECK(defined_by->IsCurrentMethod());
      switch (parent->NumberOfSpillSlotsNeeded()) {
        case 1: location_source = Location::StackSlot(parent->GetSpillSlot()); break;
        case 2: location_source = Location::DoubleStackSlot(parent->GetSpillSlot()); break;
        case 4: location_source = Location::SIMDStackSlot(parent->GetSpillSlot()); break;
        default: LOG(FATAL) << "Unexpected number of spill slots"; UNREACHABLE();
      }
    }
  } else {
    DCHECK(source != nullptr);
    DCHECK(source->CoversSlow(source_position));
    DCHECK(destination->CoversSlow(destination_position));
    location_source = source->ToLocation();
  }

  // If `from` has only one successor, we can put the moves at the exit of it. Otherwise
  // we need to put the moves at the entry of `to`.
  if (from->GetNormalSuccessors().size() == 1) {
    InsertParallelMoveAtExitOf(from,
                               defined_by,
                               location_source,
                               destination->ToLocation());
  } else {
    DCHECK_EQ(to->GetPredecessors().size(), 1u);
    InsertParallelMoveAtEntryOf(to,
                                defined_by,
                                location_source,
                                destination->ToLocation());
  }
}

static bool IsValidDestination(Location destination) {
  return destination.IsRegister()
      || destination.IsRegisterPair()
      || destination.IsFpuRegister()
      || destination.IsFpuRegisterPair()
      || destination.IsStackSlot()
      || destination.IsDoubleStackSlot()
      || destination.IsSIMDStackSlot();
}

void RegisterAllocationResolver::AddMove(HParallelMove* move,
                                         Location source,
                                         Location destination,
                                         HInstruction* instruction,
                                         DataType::Type type) const {
  if (type == DataType::Type::kInt64
      && codegen_->ShouldSplitLongMoves()
      // The parallel move resolver knows how to deal with long constants.
      && !source.IsConstant()) {
    move->AddMove(source.ToLow(), destination.ToLow(), DataType::Type::kInt32, instruction);
    move->AddMove(source.ToHigh(), destination.ToHigh(), DataType::Type::kInt32, nullptr);
  } else {
    move->AddMove(source, destination, type, instruction);
  }
}

void RegisterAllocationResolver::AddInputMoveFor(HInstruction* input,
                                                 HInstruction* user,
                                                 Location source,
                                                 Location destination) const {
  if (source.Equals(destination)) return;

  DCHECK(!user->IsPhi());

  HInstruction* previous = user->GetPrevious();
  HParallelMove* move = nullptr;
  if (previous == nullptr
      || !previous->IsParallelMove()
      || previous->GetLifetimePosition() < user->GetLifetimePosition()) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(user->GetLifetimePosition());
    user->GetBlock()->InsertInstructionBefore(move, user);
  } else {
    move = previous->AsParallelMove();
  }
  DCHECK_EQ(move->GetLifetimePosition(), user->GetLifetimePosition());
  AddMove(move, source, destination, nullptr, input->GetType());
}

static bool IsInstructionStart(size_t position) {
  return (position & 1) == 0;
}

static bool IsInstructionEnd(size_t position) {
  return (position & 1) == 1;
}

void RegisterAllocationResolver::InsertParallelMoveAt(size_t position,
                                                      HInstruction* instruction,
                                                      Location source,
                                                      Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* at = liveness_.GetInstructionFromPosition(position / 2);
  HParallelMove* move;
  if (at == nullptr) {
    if (IsInstructionStart(position)) {
      // Block boundary, don't do anything the connection of split siblings will handle it.
      return;
    } else {
      // Move must happen before the first instruction of the block.
      at = liveness_.GetInstructionFromPosition((position + 1) / 2);
      // Note that parallel moves may have already been inserted, so we explicitly
      // ask for the first instruction of the block: `GetInstructionFromPosition` does
      // not contain the `HParallelMove` instructions.
      at = at->GetBlock()->GetFirstInstruction();

      if (at->GetLifetimePosition() < position) {
        // We may insert moves for split siblings and phi spills at the beginning of the block.
        // Since this is a different lifetime position, we need to go to the next instruction.
        DCHECK(at->IsParallelMove());
        at = at->GetNext();
      }

      if (at->GetLifetimePosition() != position) {
        DCHECK_GT(at->GetLifetimePosition(), position);
        move = new (allocator_) HParallelMove(allocator_);
        move->SetLifetimePosition(position);
        at->GetBlock()->InsertInstructionBefore(move, at);
      } else {
        DCHECK(at->IsParallelMove());
        move = at->AsParallelMove();
      }
    }
  } else if (IsInstructionEnd(position)) {
    // Move must happen after the instruction.
    DCHECK(!at->IsControlFlow());
    move = at->GetNext()->AsParallelMove();
    // This is a parallel move for connecting siblings in a same block. We need to
    // differentiate it with moves for connecting blocks, and input moves.
    if (move == nullptr || move->GetLifetimePosition() > position) {
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at->GetNext());
    }
  } else {
    // Move must happen before the instruction.
    HInstruction* previous = at->GetPrevious();
    if (previous == nullptr
        || !previous->IsParallelMove()
        || previous->GetLifetimePosition() != position) {
      // If the previous is a parallel move, then its position must be lower
      // than the given `position`: it was added just after the non-parallel
      // move instruction that precedes `instruction`.
      DCHECK(previous == nullptr
             || !previous->IsParallelMove()
             || previous->GetLifetimePosition() < position);
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at);
    } else {
      move = previous->AsParallelMove();
    }
  }
  DCHECK_EQ(move->GetLifetimePosition(), position);
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertParallelMoveAtExitOf(HBasicBlock* block,
                                                            HInstruction* instruction,
                                                            Location source,
                                                            Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  DCHECK_EQ(block->GetNormalSuccessors().size(), 1u);
  HInstruction* last = block->GetLastInstruction();
  // We insert moves at exit for phi predecessors and connecting blocks.
  // A block ending with an if or a packed switch cannot branch to a block
  // with phis because we do not allow critical edges. It can also not connect
  // a split interval between two blocks: the move has to happen in the successor.
  DCHECK(!last->IsIf() && !last->IsPackedSwitch());
  HInstruction* previous = last->GetPrevious();
  HParallelMove* move;
  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block, and output moves.
  size_t position = last->GetLifetimePosition();
  if (previous == nullptr || !previous->IsParallelMove()
      || previous->AsParallelMove()->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, last);
  } else {
    move = previous->AsParallelMove();
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                                             HInstruction* instruction,
                                                             Location source,
                                                             Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* first = block->GetFirstInstruction();
  HParallelMove* move = first->AsParallelMove();
  size_t position = block->GetLifetimeStart();
  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block, and input moves.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, first);
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertMoveAfter(HInstruction* instruction,
                                                 Location source,
                                                 Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  if (instruction->IsPhi()) {
    InsertParallelMoveAtEntryOf(instruction->GetBlock(), instruction, source, destination);
    return;
  }

  size_t position = instruction->GetLifetimePosition() + 1;
  HParallelMove* move = instruction->GetNext()->AsParallelMove();
  // This is a parallel move for moving the output of an instruction. We need
  // to differentiate with input moves, moves for connecting siblings in a
  // and moves for connecting blocks.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    instruction->GetBlock()->InsertInstructionBefore(move, instruction->GetNext());
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

}  // namespace art
