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

#include "register_allocator_linear_scan.h"

#include <iostream>
#include <sstream>

#include "base/bit_vector-inl.h"
#include "base/enums.h"
#include "code_generator.h"
#include "linear_order.h"
#include "register_allocation_resolver.h"
#include "ssa_liveness_analysis.h"

namespace art {

static constexpr size_t kMaxLifetimePosition = -1;
static constexpr size_t kDefaultNumberOfSpillSlots = 4;

// For simplicity, we implement register pairs as (reg, reg + 1).
// Note that this is a requirement for double registers on ARM, since we
// allocate SRegister.
static int GetHighForLowRegister(int reg) { return reg + 1; }
static bool IsLowRegister(int reg) { return (reg & 1) == 0; }
static bool IsLowOfUnalignedPairInterval(LiveInterval* low) {
  return GetHighForLowRegister(low->GetRegister()) != low->GetHighInterval()->GetRegister();
}

RegisterAllocatorLinearScan::RegisterAllocatorLinearScan(ScopedArenaAllocator* allocator,
                                                         CodeGenerator* codegen,
                                                         const SsaLivenessAnalysis& liveness)
      : RegisterAllocator(allocator, codegen, liveness),
        unhandled_core_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        unhandled_fp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        unhandled_(nullptr),
        handled_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        active_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        inactive_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_core_register_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        physical_fp_register_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        temp_intervals_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        int_spill_slots_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        long_spill_slots_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        float_spill_slots_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        double_spill_slots_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        catch_phi_spill_slots_(0),
        safepoints_(allocator->Adapter(kArenaAllocRegisterAllocator)),
        processing_core_registers_(false),
        number_of_registers_(-1),
        registers_array_(nullptr),
        blocked_core_registers_(codegen->GetBlockedCoreRegisters()),
        blocked_fp_registers_(codegen->GetBlockedFloatingPointRegisters()),
        reserved_out_slots_(0) {
  temp_intervals_.reserve(4);
  int_spill_slots_.reserve(kDefaultNumberOfSpillSlots);
  long_spill_slots_.reserve(kDefaultNumberOfSpillSlots);
  float_spill_slots_.reserve(kDefaultNumberOfSpillSlots);
  double_spill_slots_.reserve(kDefaultNumberOfSpillSlots);

  codegen->SetupBlockedRegisters();
  physical_core_register_intervals_.resize(codegen->GetNumberOfCoreRegisters(), nullptr);
  physical_fp_register_intervals_.resize(codegen->GetNumberOfFloatingPointRegisters(), nullptr);
  // Always reserve for the current method and the graph's max out registers.
  // TODO: compute it instead.
  // ArtMethod* takes 2 vregs for 64 bits.
  size_t ptr_size = static_cast<size_t>(InstructionSetPointerSize(codegen->GetInstructionSet()));
  reserved_out_slots_ = ptr_size / kVRegSize + codegen->GetGraph()->GetMaximumNumberOfOutVRegs();
}

RegisterAllocatorLinearScan::~RegisterAllocatorLinearScan() {}

static bool ShouldProcess(bool processing_core_registers, LiveInterval* interval) {
  if (interval == nullptr) return false;
  bool is_core_register = (interval->GetType() != DataType::Type::kFloat64)
      && (interval->GetType() != DataType::Type::kFloat32);
  return processing_core_registers == is_core_register;
}

void RegisterAllocatorLinearScan::AllocateRegisters() {
  AllocateRegistersInternal();
  RegisterAllocationResolver(codegen_, liveness_)
      .Resolve(ArrayRef<HInstruction* const>(safepoints_),
               reserved_out_slots_,
               int_spill_slots_.size(),
               long_spill_slots_.size(),
               float_spill_slots_.size(),
               double_spill_slots_.size(),
               catch_phi_spill_slots_,
               ArrayRef<LiveInterval* const>(temp_intervals_));

  if (kIsDebugBuild) {
    processing_core_registers_ = true;
    ValidateInternal(true);
    processing_core_registers_ = false;
    ValidateInternal(true);
    // Check that the linear order is still correct with regards to lifetime positions.
    // Since only parallel moves have been inserted during the register allocation,
    // these checks are mostly for making sure these moves have been added correctly.
    size_t current_liveness = 0;
    for (HBasicBlock* block : codegen_->GetGraph()->GetLinearOrder()) {
      for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
        HInstruction* instruction = inst_it.Current();
        DCHECK_LE(current_liveness, instruction->GetLifetimePosition());
        current_liveness = instruction->GetLifetimePosition();
      }
      for (HInstructionIterator inst_it(block->GetInstructions());
           !inst_it.Done();
           inst_it.Advance()) {
        HInstruction* instruction = inst_it.Current();
        DCHECK_LE(current_liveness, instruction->GetLifetimePosition()) << instruction->DebugName();
        current_liveness = instruction->GetLifetimePosition();
      }
    }
  }
}

void RegisterAllocatorLinearScan::BlockRegister(Location location, size_t start, size_t end) {
  int reg = location.reg();
  DCHECK(location.IsRegister() || location.IsFpuRegister());
  LiveInterval* interval = location.IsRegister()
      ? physical_core_register_intervals_[reg]
      : physical_fp_register_intervals_[reg];
  DataType::Type type = location.IsRegister()
      ? DataType::Type::kInt32
      : DataType::Type::kFloat32;
  if (interval == nullptr) {
    interval = LiveInterval::MakeFixedInterval(allocator_, reg, type);
    if (location.IsRegister()) {
      physical_core_register_intervals_[reg] = interval;
    } else {
      physical_fp_register_intervals_[reg] = interval;
    }
  }
  DCHECK(interval->GetRegister() == reg);
  interval->AddRange(start, end);
}

void RegisterAllocatorLinearScan::BlockRegisters(size_t start, size_t end, bool caller_save_only) {
  for (size_t i = 0; i < codegen_->GetNumberOfCoreRegisters(); ++i) {
    if (!caller_save_only || !codegen_->IsCoreCalleeSaveRegister(i)) {
      BlockRegister(Location::RegisterLocation(i), start, end);
    }
  }
  for (size_t i = 0; i < codegen_->GetNumberOfFloatingPointRegisters(); ++i) {
    if (!caller_save_only || !codegen_->IsFloatingPointCalleeSaveRegister(i)) {
      BlockRegister(Location::FpuRegisterLocation(i), start, end);
    }
  }
}

void RegisterAllocatorLinearScan::AllocateRegistersInternal() {
  // Iterate post-order, to ensure the list is sorted, and the last added interval
  // is the one with the lowest start position.
  for (HBasicBlock* block : codegen_->GetGraph()->GetLinearPostOrder()) {
    for (HBackwardInstructionIterator back_it(block->GetInstructions()); !back_it.Done();
         back_it.Advance()) {
      ProcessInstruction(back_it.Current());
    }
    for (HInstructionIterator inst_it(block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      ProcessInstruction(inst_it.Current());
    }

    if (block->IsCatchBlock() ||
        (block->IsLoopHeader() && block->GetLoopInformation()->IsIrreducible())) {
      // By blocking all registers at the top of each catch block or irreducible loop, we force
      // intervals belonging to the live-in set of the catch/header block to be spilled.
      // TODO(ngeoffray): Phis in this block could be allocated in register.
      size_t position = block->GetLifetimeStart();
      BlockRegisters(position, position + 1);
    }
  }

  number_of_registers_ = codegen_->GetNumberOfCoreRegisters();
  registers_array_ = allocator_->AllocArray<size_t>(number_of_registers_,
                                                    kArenaAllocRegisterAllocator);
  processing_core_registers_ = true;
  unhandled_ = &unhandled_core_intervals_;
  for (LiveInterval* fixed : physical_core_register_intervals_) {
    if (fixed != nullptr) {
      // Fixed interval is added to inactive_ instead of unhandled_.
      // It's also the only type of inactive interval whose start position
      // can be after the current interval during linear scan.
      // Fixed interval is never split and never moves to unhandled_.
      inactive_.push_back(fixed);
    }
  }
  LinearScan();

  inactive_.clear();
  active_.clear();
  handled_.clear();

  number_of_registers_ = codegen_->GetNumberOfFloatingPointRegisters();
  registers_array_ = allocator_->AllocArray<size_t>(number_of_registers_,
                                                    kArenaAllocRegisterAllocator);
  processing_core_registers_ = false;
  unhandled_ = &unhandled_fp_intervals_;
  for (LiveInterval* fixed : physical_fp_register_intervals_) {
    if (fixed != nullptr) {
      // Fixed interval is added to inactive_ instead of unhandled_.
      // It's also the only type of inactive interval whose start position
      // can be after the current interval during linear scan.
      // Fixed interval is never split and never moves to unhandled_.
      inactive_.push_back(fixed);
    }
  }
  LinearScan();
}

void RegisterAllocatorLinearScan::ProcessInstruction(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  size_t position = instruction->GetLifetimePosition();

  if (locations == nullptr) return;

  // Create synthesized intervals for temporaries.
  for (size_t i = 0; i < locations->GetTempCount(); ++i) {
    Location temp = locations->GetTemp(i);
    if (temp.IsRegister() || temp.IsFpuRegister()) {
      BlockRegister(temp, position, position + 1);
      // Ensure that an explicit temporary register is marked as being allocated.
      codegen_->AddAllocatedRegister(temp);
    } else {
      DCHECK(temp.IsUnallocated());
      switch (temp.GetPolicy()) {
        case Location::kRequiresRegister: {
          LiveInterval* interval =
              LiveInterval::MakeTempInterval(allocator_, DataType::Type::kInt32);
          temp_intervals_.push_back(interval);
          interval->AddTempUse(instruction, i);
          unhandled_core_intervals_.push_back(interval);
          break;
        }

        case Location::kRequiresFpuRegister: {
          LiveInterval* interval =
              LiveInterval::MakeTempInterval(allocator_, DataType::Type::kFloat64);
          temp_intervals_.push_back(interval);
          interval->AddTempUse(instruction, i);
          if (codegen_->NeedsTwoRegisters(DataType::Type::kFloat64)) {
            interval->AddHighInterval(/* is_temp */ true);
            LiveInterval* high = interval->GetHighInterval();
            temp_intervals_.push_back(high);
            unhandled_fp_intervals_.push_back(high);
          }
          unhandled_fp_intervals_.push_back(interval);
          break;
        }

        default:
          LOG(FATAL) << "Unexpected policy for temporary location "
                     << temp.GetPolicy();
      }
    }
  }

  bool core_register = (instruction->GetType() != DataType::Type::kFloat64)
      && (instruction->GetType() != DataType::Type::kFloat32);

  if (locations->NeedsSafepoint()) {
    if (codegen_->IsLeafMethod()) {
      // TODO: We do this here because we do not want the suspend check to artificially
      // create live registers. We should find another place, but this is currently the
      // simplest.
      DCHECK(instruction->IsSuspendCheckEntry());
      instruction->GetBlock()->RemoveInstruction(instruction);
      return;
    }
    safepoints_.push_back(instruction);
  }

  if (locations->WillCall()) {
    BlockRegisters(position, position + 1, /* caller_save_only */ true);
  }

  for (size_t i = 0; i < locations->GetInputCount(); ++i) {
    Location input = locations->InAt(i);
    if (input.IsRegister() || input.IsFpuRegister()) {
      BlockRegister(input, position, position + 1);
    } else if (input.IsPair()) {
      BlockRegister(input.ToLow(), position, position + 1);
      BlockRegister(input.ToHigh(), position, position + 1);
    }
  }

  LiveInterval* current = instruction->GetLiveInterval();
  if (current == nullptr) return;

  ScopedArenaVector<LiveInterval*>& unhandled = core_register
      ? unhandled_core_intervals_
      : unhandled_fp_intervals_;

  DCHECK(unhandled.empty() || current->StartsBeforeOrAt(unhandled.back()));

  if (codegen_->NeedsTwoRegisters(current->GetType())) {
    current->AddHighInterval();
  }

  for (size_t safepoint_index = safepoints_.size(); safepoint_index > 0; --safepoint_index) {
    HInstruction* safepoint = safepoints_[safepoint_index - 1u];
    size_t safepoint_position = safepoint->GetLifetimePosition();

    // Test that safepoints are ordered in the optimal way.
    DCHECK(safepoint_index == safepoints_.size() ||
           safepoints_[safepoint_index]->GetLifetimePosition() < safepoint_position);

    if (safepoint_position == current->GetStart()) {
      // The safepoint is for this instruction, so the location of the instruction
      // does not need to be saved.
      DCHECK_EQ(safepoint_index, safepoints_.size());
      DCHECK_EQ(safepoint, instruction);
      continue;
    } else if (current->IsDeadAt(safepoint_position)) {
      break;
    } else if (!current->Covers(safepoint_position)) {
      // Hole in the interval.
      continue;
    }
    current->AddSafepoint(safepoint);
  }
  current->ResetSearchCache();

  // Some instructions define their output in fixed register/stack slot. We need
  // to ensure we know these locations before doing register allocation. For a
  // given register, we create an interval that covers these locations. The register
  // will be unavailable at these locations when trying to allocate one for an
  // interval.
  //
  // The backwards walking ensures the ranges are ordered on increasing start positions.
  Location output = locations->Out();
  if (output.IsUnallocated() && output.GetPolicy() == Location::kSameAsFirstInput) {
    Location first = locations->InAt(0);
    if (first.IsRegister() || first.IsFpuRegister()) {
      current->SetFrom(position + 1);
      current->SetRegister(first.reg());
    } else if (first.IsPair()) {
      current->SetFrom(position + 1);
      current->SetRegister(first.low());
      LiveInterval* high = current->GetHighInterval();
      high->SetRegister(first.high());
      high->SetFrom(position + 1);
    }
  } else if (output.IsRegister() || output.IsFpuRegister()) {
    // Shift the interval's start by one to account for the blocked register.
    current->SetFrom(position + 1);
    current->SetRegister(output.reg());
    BlockRegister(output, position, position + 1);
  } else if (output.IsPair()) {
    current->SetFrom(position + 1);
    current->SetRegister(output.low());
    LiveInterval* high = current->GetHighInterval();
    high->SetRegister(output.high());
    high->SetFrom(position + 1);
    BlockRegister(output.ToLow(), position, position + 1);
    BlockRegister(output.ToHigh(), position, position + 1);
  } else if (output.IsStackSlot() || output.IsDoubleStackSlot()) {
    current->SetSpillSlot(output.GetStackIndex());
  } else {
    DCHECK(output.IsUnallocated() || output.IsConstant());
  }

  if (instruction->IsPhi() && instruction->AsPhi()->IsCatchPhi()) {
    AllocateSpillSlotForCatchPhi(instruction->AsPhi());
  }

  // If needed, add interval to the list of unhandled intervals.
  if (current->HasSpillSlot() || instruction->IsConstant()) {
    // Split just before first register use.
    size_t first_register_use = current->FirstRegisterUse();
    if (first_register_use != kNoLifetime) {
      LiveInterval* split = SplitBetween(current, current->GetStart(), first_register_use - 1);
      // Don't add directly to `unhandled`, it needs to be sorted and the start
      // of this new interval might be after intervals already in the list.
      AddSorted(&unhandled, split);
    } else {
      // Nothing to do, we won't allocate a register for this value.
    }
  } else {
    // Don't add directly to `unhandled`, temp or safepoint intervals
    // for this instruction may have been added, and those can be
    // processed first.
    AddSorted(&unhandled, current);
  }
}

class AllRangesIterator : public ValueObject {
 public:
  explicit AllRangesIterator(LiveInterval* interval)
      : current_interval_(interval),
        current_range_(interval->GetFirstRange()) {}

  bool Done() const { return current_interval_ == nullptr; }
  LiveRange* CurrentRange() const { return current_range_; }
  LiveInterval* CurrentInterval() const { return current_interval_; }

  void Advance() {
    current_range_ = current_range_->GetNext();
    if (current_range_ == nullptr) {
      current_interval_ = current_interval_->GetNextSibling();
      if (current_interval_ != nullptr) {
        current_range_ = current_interval_->GetFirstRange();
      }
    }
  }

 private:
  LiveInterval* current_interval_;
  LiveRange* current_range_;

  DISALLOW_COPY_AND_ASSIGN(AllRangesIterator);
};

bool RegisterAllocatorLinearScan::ValidateInternal(bool log_fatal_on_failure) const {
  // To simplify unit testing, we eagerly create the array of intervals, and
  // call the helper method.
  ScopedArenaAllocator allocator(allocator_->GetArenaStack());
  ScopedArenaVector<LiveInterval*> intervals(
      allocator.Adapter(kArenaAllocRegisterAllocatorValidate));
  for (size_t i = 0; i < liveness_.GetNumberOfSsaValues(); ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    if (ShouldProcess(processing_core_registers_, instruction->GetLiveInterval())) {
      intervals.push_back(instruction->GetLiveInterval());
    }
  }

  const ScopedArenaVector<LiveInterval*>* physical_register_intervals = processing_core_registers_
      ? &physical_core_register_intervals_
      : &physical_fp_register_intervals_;
  for (LiveInterval* fixed : *physical_register_intervals) {
    if (fixed != nullptr) {
      intervals.push_back(fixed);
    }
  }

  for (LiveInterval* temp : temp_intervals_) {
    if (ShouldProcess(processing_core_registers_, temp)) {
      intervals.push_back(temp);
    }
  }

  return ValidateIntervals(ArrayRef<LiveInterval* const>(intervals),
                           GetNumberOfSpillSlots(),
                           reserved_out_slots_,
                           *codegen_,
                           processing_core_registers_,
                           log_fatal_on_failure);
}

void RegisterAllocatorLinearScan::DumpInterval(std::ostream& stream, LiveInterval* interval) const {
  interval->Dump(stream);
  stream << ": ";
  if (interval->HasRegister()) {
    if (interval->IsFloatingPoint()) {
      codegen_->DumpFloatingPointRegister(stream, interval->GetRegister());
    } else {
      codegen_->DumpCoreRegister(stream, interval->GetRegister());
    }
  } else {
    stream << "spilled";
  }
  stream << std::endl;
}

void RegisterAllocatorLinearScan::DumpAllIntervals(std::ostream& stream) const {
  stream << "inactive: " << std::endl;
  for (LiveInterval* inactive_interval : inactive_) {
    DumpInterval(stream, inactive_interval);
  }
  stream << "active: " << std::endl;
  for (LiveInterval* active_interval : active_) {
    DumpInterval(stream, active_interval);
  }
  stream << "unhandled: " << std::endl;
  auto unhandled = (unhandled_ != nullptr) ?
      unhandled_ : &unhandled_core_intervals_;
  for (LiveInterval* unhandled_interval : *unhandled) {
    DumpInterval(stream, unhandled_interval);
  }
  stream << "handled: " << std::endl;
  for (LiveInterval* handled_interval : handled_) {
    DumpInterval(stream, handled_interval);
  }
}

// By the book implementation of a linear scan register allocator.
void RegisterAllocatorLinearScan::LinearScan() {
  while (!unhandled_->empty()) {
    // (1) Remove interval with the lowest start position from unhandled.
    LiveInterval* current = unhandled_->back();
    unhandled_->pop_back();

    // Make sure the interval is an expected state.
    DCHECK(!current->IsFixed() && !current->HasSpillSlot());
    // Make sure we are going in the right order.
    DCHECK(unhandled_->empty() || unhandled_->back()->GetStart() >= current->GetStart());
    // Make sure a low interval is always with a high.
    DCHECK(!current->IsLowInterval() || unhandled_->back()->IsHighInterval());
    // Make sure a high interval is always with a low.
    DCHECK(current->IsLowInterval() ||
           unhandled_->empty() ||
           !unhandled_->back()->IsHighInterval());

    size_t position = current->GetStart();

    // Remember the inactive_ size here since the ones moved to inactive_ from
    // active_ below shouldn't need to be re-checked.
    size_t inactive_intervals_to_handle = inactive_.size();

    // (2) Remove currently active intervals that are dead at this position.
    //     Move active intervals that have a lifetime hole at this position
    //     to inactive.
    auto active_kept_end = std::remove_if(
        active_.begin(),
        active_.end(),
        [this, position](LiveInterval* interval) {
          if (interval->IsDeadAt(position)) {
            handled_.push_back(interval);
            return true;
          } else if (!interval->Covers(position)) {
            inactive_.push_back(interval);
            return true;
          } else {
            return false;  // Keep this interval.
          }
        });
    active_.erase(active_kept_end, active_.end());

    // (3) Remove currently inactive intervals that are dead at this position.
    //     Move inactive intervals that cover this position to active.
    auto inactive_to_handle_end = inactive_.begin() + inactive_intervals_to_handle;
    auto inactive_kept_end = std::remove_if(
        inactive_.begin(),
        inactive_to_handle_end,
        [this, position](LiveInterval* interval) {
          DCHECK(interval->GetStart() < position || interval->IsFixed());
          if (interval->IsDeadAt(position)) {
            handled_.push_back(interval);
            return true;
          } else if (interval->Covers(position)) {
            active_.push_back(interval);
            return true;
          } else {
            return false;  // Keep this interval.
          }
        });
    inactive_.erase(inactive_kept_end, inactive_to_handle_end);

    if (current->IsHighInterval() && !current->GetLowInterval()->HasRegister()) {
      DCHECK(!current->HasRegister());
      // Allocating the low part was unsucessful. The splitted interval for the high part
      // will be handled next (it is in the `unhandled_` list).
      continue;
    }

    // (4) Try to find an available register.
    bool success = TryAllocateFreeReg(current);

    // (5) If no register could be found, we need to spill.
    if (!success) {
      success = AllocateBlockedReg(current);
    }

    // (6) If the interval had a register allocated, add it to the list of active
    //     intervals.
    if (success) {
      codegen_->AddAllocatedRegister(processing_core_registers_
          ? Location::RegisterLocation(current->GetRegister())
          : Location::FpuRegisterLocation(current->GetRegister()));
      active_.push_back(current);
      if (current->HasHighInterval() && !current->GetHighInterval()->HasRegister()) {
        current->GetHighInterval()->SetRegister(GetHighForLowRegister(current->GetRegister()));
      }
    }
  }
}

static void FreeIfNotCoverAt(LiveInterval* interval, size_t position, size_t* free_until) {
  DCHECK(!interval->IsHighInterval());
  // Note that the same instruction may occur multiple times in the input list,
  // so `free_until` may have changed already.
  // Since `position` is not the current scan position, we need to use CoversSlow.
  if (interval->IsDeadAt(position)) {
    // Set the register to be free. Note that inactive intervals might later
    // update this.
    free_until[interval->GetRegister()] = kMaxLifetimePosition;
    if (interval->HasHighInterval()) {
      DCHECK(interval->GetHighInterval()->IsDeadAt(position));
      free_until[interval->GetHighInterval()->GetRegister()] = kMaxLifetimePosition;
    }
  } else if (!interval->CoversSlow(position)) {
    // The interval becomes inactive at `defined_by`. We make its register
    // available only until the next use strictly after `defined_by`.
    free_until[interval->GetRegister()] = interval->FirstUseAfter(position);
    if (interval->HasHighInterval()) {
      DCHECK(!interval->GetHighInterval()->CoversSlow(position));
      free_until[interval->GetHighInterval()->GetRegister()] = free_until[interval->GetRegister()];
    }
  }
}

// Find a free register. If multiple are found, pick the register that
// is free the longest.
bool RegisterAllocatorLinearScan::TryAllocateFreeReg(LiveInterval* current) {
  size_t* free_until = registers_array_;

  // First set all registers to be free.
  for (size_t i = 0; i < number_of_registers_; ++i) {
    free_until[i] = kMaxLifetimePosition;
  }

  // For each active interval, set its register to not free.
  for (LiveInterval* interval : active_) {
    DCHECK(interval->HasRegister());
    free_until[interval->GetRegister()] = 0;
  }

  // An interval that starts an instruction (that is, it is not split), may
  // re-use the registers used by the inputs of that instruciton, based on the
  // location summary.
  HInstruction* defined_by = current->GetDefinedBy();
  if (defined_by != nullptr && !current->IsSplit()) {
    LocationSummary* locations = defined_by->GetLocations();
    if (!locations->OutputCanOverlapWithInputs() && locations->Out().IsUnallocated()) {
      HInputsRef inputs = defined_by->GetInputs();
      for (size_t i = 0; i < inputs.size(); ++i) {
        if (locations->InAt(i).IsValid()) {
          // Take the last interval of the input. It is the location of that interval
          // that will be used at `defined_by`.
          LiveInterval* interval = inputs[i]->GetLiveInterval()->GetLastSibling();
          // Note that interval may have not been processed yet.
          // TODO: Handle non-split intervals last in the work list.
          if (interval->HasRegister() && interval->SameRegisterKind(*current)) {
            // The input must be live until the end of `defined_by`, to comply to
            // the linear scan algorithm. So we use `defined_by`'s end lifetime
            // position to check whether the input is dead or is inactive after
            // `defined_by`.
            DCHECK(interval->CoversSlow(defined_by->GetLifetimePosition()));
            size_t position = defined_by->GetLifetimePosition() + 1;
            FreeIfNotCoverAt(interval, position, free_until);
          }
        }
      }
    }
  }

  // For each inactive interval, set its register to be free until
  // the next intersection with `current`.
  for (LiveInterval* inactive : inactive_) {
    // Temp/Slow-path-safepoint interval has no holes.
    DCHECK(!inactive->IsTemp());
    if (!current->IsSplit() && !inactive->IsFixed()) {
      // Neither current nor inactive are fixed.
      // Thanks to SSA, a non-split interval starting in a hole of an
      // inactive interval should never intersect with that inactive interval.
      // Only if it's not fixed though, because fixed intervals don't come from SSA.
      DCHECK_EQ(inactive->FirstIntersectionWith(current), kNoLifetime);
      continue;
    }

    DCHECK(inactive->HasRegister());
    if (free_until[inactive->GetRegister()] == 0) {
      // Already used by some active interval. No need to intersect.
      continue;
    }
    size_t next_intersection = inactive->FirstIntersectionWith(current);
    if (next_intersection != kNoLifetime) {
      free_until[inactive->GetRegister()] =
          std::min(free_until[inactive->GetRegister()], next_intersection);
    }
  }

  int reg = kNoRegister;
  if (current->HasRegister()) {
    // Some instructions have a fixed register output.
    reg = current->GetRegister();
    if (free_until[reg] == 0) {
      DCHECK(current->IsHighInterval());
      // AllocateBlockedReg will spill the holder of the register.
      return false;
    }
  } else {
    DCHECK(!current->IsHighInterval());
    int hint = current->FindFirstRegisterHint(free_until, liveness_);
    if ((hint != kNoRegister)
        // For simplicity, if the hint we are getting for a pair cannot be used,
        // we are just going to allocate a new pair.
        && !(current->IsLowInterval() && IsBlocked(GetHighForLowRegister(hint)))) {
      DCHECK(!IsBlocked(hint));
      reg = hint;
    } else if (current->IsLowInterval()) {
      reg = FindAvailableRegisterPair(free_until, current->GetStart());
    } else {
      reg = FindAvailableRegister(free_until, current);
    }
  }

  DCHECK_NE(reg, kNoRegister);
  // If we could not find a register, we need to spill.
  if (free_until[reg] == 0) {
    return false;
  }

  if (current->IsLowInterval()) {
    // If the high register of this interval is not available, we need to spill.
    int high_reg = current->GetHighInterval()->GetRegister();
    if (high_reg == kNoRegister) {
      high_reg = GetHighForLowRegister(reg);
    }
    if (free_until[high_reg] == 0) {
      return false;
    }
  }

  current->SetRegister(reg);
  if (!current->IsDeadAt(free_until[reg])) {
    // If the register is only available for a subset of live ranges
    // covered by `current`, split `current` before the position where
    // the register is not available anymore.
    LiveInterval* split = SplitBetween(current, current->GetStart(), free_until[reg]);
    DCHECK(split != nullptr);
    AddSorted(unhandled_, split);
  }
  return true;
}

bool RegisterAllocatorLinearScan::IsBlocked(int reg) const {
  return processing_core_registers_
      ? blocked_core_registers_[reg]
      : blocked_fp_registers_[reg];
}

int RegisterAllocatorLinearScan::FindAvailableRegisterPair(size_t* next_use, size_t starting_at) const {
  int reg = kNoRegister;
  // Pick the register pair that is used the last.
  for (size_t i = 0; i < number_of_registers_; ++i) {
    if (IsBlocked(i)) continue;
    if (!IsLowRegister(i)) continue;
    int high_register = GetHighForLowRegister(i);
    if (IsBlocked(high_register)) continue;
    int existing_high_register = GetHighForLowRegister(reg);
    if ((reg == kNoRegister) || (next_use[i] >= next_use[reg]
                        && next_use[high_register] >= next_use[existing_high_register])) {
      reg = i;
      if (next_use[i] == kMaxLifetimePosition
          && next_use[high_register] == kMaxLifetimePosition) {
        break;
      }
    } else if (next_use[reg] <= starting_at || next_use[existing_high_register] <= starting_at) {
      // If one of the current register is known to be unavailable, just unconditionally
      // try a new one.
      reg = i;
    }
  }
  return reg;
}

bool RegisterAllocatorLinearScan::IsCallerSaveRegister(int reg) const {
  return processing_core_registers_
      ? !codegen_->IsCoreCalleeSaveRegister(reg)
      : !codegen_->IsFloatingPointCalleeSaveRegister(reg);
}

int RegisterAllocatorLinearScan::FindAvailableRegister(size_t* next_use, LiveInterval* current) const {
  // We special case intervals that do not span a safepoint to try to find a caller-save
  // register if one is available. We iterate from 0 to the number of registers,
  // so if there are caller-save registers available at the end, we continue the iteration.
  bool prefers_caller_save = !current->HasWillCallSafepoint();
  int reg = kNoRegister;
  for (size_t i = 0; i < number_of_registers_; ++i) {
    if (IsBlocked(i)) {
      // Register cannot be used. Continue.
      continue;
    }

    // Best case: we found a register fully available.
    if (next_use[i] == kMaxLifetimePosition) {
      if (prefers_caller_save && !IsCallerSaveRegister(i)) {
        // We can get shorter encodings on some platforms by using
        // small register numbers. So only update the candidate if the previous
        // one was not available for the whole method.
        if (reg == kNoRegister || next_use[reg] != kMaxLifetimePosition) {
          reg = i;
        }
        // Continue the iteration in the hope of finding a caller save register.
        continue;
      } else {
        reg = i;
        // We know the register is good enough. Return it.
        break;
      }
    }

    // If we had no register before, take this one as a reference.
    if (reg == kNoRegister) {
      reg = i;
      continue;
    }

    // Pick the register that is used the last.
    if (next_use[i] > next_use[reg]) {
      reg = i;
      continue;
    }
  }
  return reg;
}

// Remove interval and its other half if any. Return iterator to the following element.
static ArenaVector<LiveInterval*>::iterator RemoveIntervalAndPotentialOtherHalf(
    ScopedArenaVector<LiveInterval*>* intervals, ScopedArenaVector<LiveInterval*>::iterator pos) {
  DCHECK(intervals->begin() <= pos && pos < intervals->end());
  LiveInterval* interval = *pos;
  if (interval->IsLowInterval()) {
    DCHECK(pos + 1 < intervals->end());
    DCHECK_EQ(*(pos + 1), interval->GetHighInterval());
    return intervals->erase(pos, pos + 2);
  } else if (interval->IsHighInterval()) {
    DCHECK(intervals->begin() < pos);
    DCHECK_EQ(*(pos - 1), interval->GetLowInterval());
    return intervals->erase(pos - 1, pos + 1);
  } else {
    return intervals->erase(pos);
  }
}

bool RegisterAllocatorLinearScan::TrySplitNonPairOrUnalignedPairIntervalAt(size_t position,
                                                                           size_t first_register_use,
                                                                           size_t* next_use) {
  for (auto it = active_.begin(), end = active_.end(); it != end; ++it) {
    LiveInterval* active = *it;
    DCHECK(active->HasRegister());
    if (active->IsFixed()) continue;
    if (active->IsHighInterval()) continue;
    if (first_register_use > next_use[active->GetRegister()]) continue;

    // Split the first interval found that is either:
    // 1) A non-pair interval.
    // 2) A pair interval whose high is not low + 1.
    // 3) A pair interval whose low is not even.
    if (!active->IsLowInterval() ||
        IsLowOfUnalignedPairInterval(active) ||
        !IsLowRegister(active->GetRegister())) {
      LiveInterval* split = Split(active, position);
      if (split != active) {
        handled_.push_back(active);
      }
      RemoveIntervalAndPotentialOtherHalf(&active_, it);
      AddSorted(unhandled_, split);
      return true;
    }
  }
  return false;
}

// Find the register that is used the last, and spill the interval
// that holds it. If the first use of `current` is after that register
// we spill `current` instead.
bool RegisterAllocatorLinearScan::AllocateBlockedReg(LiveInterval* current) {
  size_t first_register_use = current->FirstRegisterUse();
  if (current->HasRegister()) {
    DCHECK(current->IsHighInterval());
    // The low interval has allocated the register for the high interval. In
    // case the low interval had to split both intervals, we may end up in a
    // situation where the high interval does not have a register use anymore.
    // We must still proceed in order to split currently active and inactive
    // uses of the high interval's register, and put the high interval in the
    // active set.
    DCHECK(first_register_use != kNoLifetime || (current->GetNextSibling() != nullptr));
  } else if (first_register_use == kNoLifetime) {
    AllocateSpillSlotFor(current);
    return false;
  }

  // First set all registers as not being used.
  size_t* next_use = registers_array_;
  for (size_t i = 0; i < number_of_registers_; ++i) {
    next_use[i] = kMaxLifetimePosition;
  }

  // For each active interval, find the next use of its register after the
  // start of current.
  for (LiveInterval* active : active_) {
    DCHECK(active->HasRegister());
    if (active->IsFixed()) {
      next_use[active->GetRegister()] = current->GetStart();
    } else {
      size_t use = active->FirstRegisterUseAfter(current->GetStart());
      if (use != kNoLifetime) {
        next_use[active->GetRegister()] = use;
      }
    }
  }

  // For each inactive interval, find the next use of its register after the
  // start of current.
  for (LiveInterval* inactive : inactive_) {
    // Temp/Slow-path-safepoint interval has no holes.
    DCHECK(!inactive->IsTemp());
    if (!current->IsSplit() && !inactive->IsFixed()) {
      // Neither current nor inactive are fixed.
      // Thanks to SSA, a non-split interval starting in a hole of an
      // inactive interval should never intersect with that inactive interval.
      // Only if it's not fixed though, because fixed intervals don't come from SSA.
      DCHECK_EQ(inactive->FirstIntersectionWith(current), kNoLifetime);
      continue;
    }
    DCHECK(inactive->HasRegister());
    size_t next_intersection = inactive->FirstIntersectionWith(current);
    if (next_intersection != kNoLifetime) {
      if (inactive->IsFixed()) {
        next_use[inactive->GetRegister()] =
            std::min(next_intersection, next_use[inactive->GetRegister()]);
      } else {
        size_t use = inactive->FirstUseAfter(current->GetStart());
        if (use != kNoLifetime) {
          next_use[inactive->GetRegister()] = std::min(use, next_use[inactive->GetRegister()]);
        }
      }
    }
  }

  int reg = kNoRegister;
  bool should_spill = false;
  if (current->HasRegister()) {
    DCHECK(current->IsHighInterval());
    reg = current->GetRegister();
    // When allocating the low part, we made sure the high register was available.
    DCHECK_LT(first_register_use, next_use[reg]);
  } else if (current->IsLowInterval()) {
    reg = FindAvailableRegisterPair(next_use, first_register_use);
    // We should spill if both registers are not available.
    should_spill = (first_register_use >= next_use[reg])
      || (first_register_use >= next_use[GetHighForLowRegister(reg)]);
  } else {
    DCHECK(!current->IsHighInterval());
    reg = FindAvailableRegister(next_use, current);
    should_spill = (first_register_use >= next_use[reg]);
  }

  DCHECK_NE(reg, kNoRegister);
  if (should_spill) {
    DCHECK(!current->IsHighInterval());
    bool is_allocation_at_use_site = (current->GetStart() >= (first_register_use - 1));
    if (is_allocation_at_use_site) {
      if (!current->IsLowInterval()) {
        DumpInterval(std::cerr, current);
        DumpAllIntervals(std::cerr);
        // This situation has the potential to infinite loop, so we make it a non-debug CHECK.
        HInstruction* at = liveness_.GetInstructionFromPosition(first_register_use / 2);
        CHECK(false) << "There is not enough registers available for "
          << current->GetParent()->GetDefinedBy()->DebugName() << " "
          << current->GetParent()->GetDefinedBy()->GetId()
          << " at " << first_register_use - 1 << " "
          << (at == nullptr ? "" : at->DebugName());
      }

      // If we're allocating a register for `current` because the instruction at
      // that position requires it, but we think we should spill, then there are
      // non-pair intervals or unaligned pair intervals blocking the allocation.
      // We split the first interval found, and put ourselves first in the
      // `unhandled_` list.
      bool success = TrySplitNonPairOrUnalignedPairIntervalAt(current->GetStart(),
                                                              first_register_use,
                                                              next_use);
      DCHECK(success);
      LiveInterval* existing = unhandled_->back();
      DCHECK(existing->IsHighInterval());
      DCHECK_EQ(existing->GetLowInterval(), current);
      unhandled_->push_back(current);
    } else {
      // If the first use of that instruction is after the last use of the found
      // register, we split this interval just before its first register use.
      AllocateSpillSlotFor(current);
      LiveInterval* split = SplitBetween(current, current->GetStart(), first_register_use - 1);
      DCHECK(current != split);
      AddSorted(unhandled_, split);
    }
    return false;
  } else {
    // Use this register and spill the active and inactives interval that
    // have that register.
    current->SetRegister(reg);

    for (auto it = active_.begin(), end = active_.end(); it != end; ++it) {
      LiveInterval* active = *it;
      if (active->GetRegister() == reg) {
        DCHECK(!active->IsFixed());
        LiveInterval* split = Split(active, current->GetStart());
        if (split != active) {
          handled_.push_back(active);
        }
        RemoveIntervalAndPotentialOtherHalf(&active_, it);
        AddSorted(unhandled_, split);
        break;
      }
    }

    // NOTE: Retrieve end() on each iteration because we're removing elements in the loop body.
    for (auto it = inactive_.begin(); it != inactive_.end(); ) {
      LiveInterval* inactive = *it;
      bool erased = false;
      if (inactive->GetRegister() == reg) {
        if (!current->IsSplit() && !inactive->IsFixed()) {
          // Neither current nor inactive are fixed.
          // Thanks to SSA, a non-split interval starting in a hole of an
          // inactive interval should never intersect with that inactive interval.
          // Only if it's not fixed though, because fixed intervals don't come from SSA.
          DCHECK_EQ(inactive->FirstIntersectionWith(current), kNoLifetime);
        } else {
          size_t next_intersection = inactive->FirstIntersectionWith(current);
          if (next_intersection != kNoLifetime) {
            if (inactive->IsFixed()) {
              LiveInterval* split = Split(current, next_intersection);
              DCHECK_NE(split, current);
              AddSorted(unhandled_, split);
            } else {
              // Split at the start of `current`, which will lead to splitting
              // at the end of the lifetime hole of `inactive`.
              LiveInterval* split = Split(inactive, current->GetStart());
              // If it's inactive, it must start before the current interval.
              DCHECK_NE(split, inactive);
              it = RemoveIntervalAndPotentialOtherHalf(&inactive_, it);
              erased = true;
              handled_.push_back(inactive);
              AddSorted(unhandled_, split);
            }
          }
        }
      }
      // If we have erased the element, `it` already points to the next element.
      // Otherwise we need to move to the next element.
      if (!erased) {
        ++it;
      }
    }

    return true;
  }
}

void RegisterAllocatorLinearScan::AddSorted(ScopedArenaVector<LiveInterval*>* array,
                                            LiveInterval* interval) {
  DCHECK(!interval->IsFixed() && !interval->HasSpillSlot());
  size_t insert_at = 0;
  for (size_t i = array->size(); i > 0; --i) {
    LiveInterval* current = (*array)[i - 1u];
    // High intervals must be processed right after their low equivalent.
    if (current->StartsAfter(interval) && !current->IsHighInterval()) {
      insert_at = i;
      break;
    }
  }

  // Insert the high interval before the low, to ensure the low is processed before.
  auto insert_pos = array->begin() + insert_at;
  if (interval->HasHighInterval()) {
    array->insert(insert_pos, { interval->GetHighInterval(), interval });
  } else if (interval->HasLowInterval()) {
    array->insert(insert_pos, { interval, interval->GetLowInterval() });
  } else {
    array->insert(insert_pos, interval);
  }
}

void RegisterAllocatorLinearScan::AllocateSpillSlotFor(LiveInterval* interval) {
  if (interval->IsHighInterval()) {
    // The low interval already took care of allocating the spill slot.
    DCHECK(!interval->GetLowInterval()->HasRegister());
    DCHECK(interval->GetLowInterval()->GetParent()->HasSpillSlot());
    return;
  }

  LiveInterval* parent = interval->GetParent();

  // An instruction gets a spill slot for its entire lifetime. If the parent
  // of this interval already has a spill slot, there is nothing to do.
  if (parent->HasSpillSlot()) {
    return;
  }

  HInstruction* defined_by = parent->GetDefinedBy();
  DCHECK(!defined_by->IsPhi() || !defined_by->AsPhi()->IsCatchPhi());

  if (defined_by->IsParameterValue()) {
    // Parameters have their own stack slot.
    parent->SetSpillSlot(codegen_->GetStackSlotOfParameter(defined_by->AsParameterValue()));
    return;
  }

  if (defined_by->IsCurrentMethod()) {
    parent->SetSpillSlot(0);
    return;
  }

  if (defined_by->IsConstant()) {
    // Constants don't need a spill slot.
    return;
  }

  ScopedArenaVector<size_t>* spill_slots = nullptr;
  switch (interval->GetType()) {
    case DataType::Type::kFloat64:
      spill_slots = &double_spill_slots_;
      break;
    case DataType::Type::kInt64:
      spill_slots = &long_spill_slots_;
      break;
    case DataType::Type::kFloat32:
      spill_slots = &float_spill_slots_;
      break;
    case DataType::Type::kReference:
    case DataType::Type::kInt32:
    case DataType::Type::kUint16:
    case DataType::Type::kUint8:
    case DataType::Type::kInt8:
    case DataType::Type::kBool:
    case DataType::Type::kInt16:
      spill_slots = &int_spill_slots_;
      break;
    case DataType::Type::kUint32:
    case DataType::Type::kUint64:
    case DataType::Type::kVoid:
      LOG(FATAL) << "Unexpected type for interval " << interval->GetType();
  }

  // Find first available spill slots.
  size_t number_of_spill_slots_needed = parent->NumberOfSpillSlotsNeeded();
  size_t slot = 0;
  for (size_t e = spill_slots->size(); slot < e; ++slot) {
    bool found = true;
    for (size_t s = slot, u = std::min(slot + number_of_spill_slots_needed, e); s < u; s++) {
      if ((*spill_slots)[s] > parent->GetStart()) {
        found = false;  // failure
        break;
      }
    }
    if (found) {
      break;  // success
    }
  }

  // Need new spill slots?
  size_t upper = slot + number_of_spill_slots_needed;
  if (upper > spill_slots->size()) {
    spill_slots->resize(upper);
  }
  // Set slots to end.
  size_t end = interval->GetLastSibling()->GetEnd();
  for (size_t s = slot; s < upper; s++) {
    (*spill_slots)[s] = end;
  }

  // Note that the exact spill slot location will be computed when we resolve,
  // that is when we know the number of spill slots for each type.
  parent->SetSpillSlot(slot);
}

void RegisterAllocatorLinearScan::AllocateSpillSlotForCatchPhi(HPhi* phi) {
  LiveInterval* interval = phi->GetLiveInterval();

  HInstruction* previous_phi = phi->GetPrevious();
  DCHECK(previous_phi == nullptr ||
         previous_phi->AsPhi()->GetRegNumber() <= phi->GetRegNumber())
      << "Phis expected to be sorted by vreg number, so that equivalent phis are adjacent.";

  if (phi->IsVRegEquivalentOf(previous_phi)) {
    // This is an equivalent of the previous phi. We need to assign the same
    // catch phi slot.
    DCHECK(previous_phi->GetLiveInterval()->HasSpillSlot());
    interval->SetSpillSlot(previous_phi->GetLiveInterval()->GetSpillSlot());
  } else {
    // Allocate a new spill slot for this catch phi.
    // TODO: Reuse spill slots when intervals of phis from different catch
    //       blocks do not overlap.
    interval->SetSpillSlot(catch_phi_spill_slots_);
    catch_phi_spill_slots_ += interval->NumberOfSpillSlotsNeeded();
  }
}

}  // namespace art
