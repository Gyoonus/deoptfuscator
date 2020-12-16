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

#ifndef ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_

#include <iostream>

#include "base/iteration_range.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "nodes.h"
#include "utils/intrusive_forward_list.h"

namespace art {

class CodeGenerator;
class SsaLivenessAnalysis;

static constexpr int kNoRegister = -1;

class BlockInfo : public ArenaObject<kArenaAllocSsaLiveness> {
 public:
  BlockInfo(ScopedArenaAllocator* allocator, const HBasicBlock& block, size_t number_of_ssa_values)
      : block_(block),
        live_in_(allocator, number_of_ssa_values, false, kArenaAllocSsaLiveness),
        live_out_(allocator, number_of_ssa_values, false, kArenaAllocSsaLiveness),
        kill_(allocator, number_of_ssa_values, false, kArenaAllocSsaLiveness) {
    UNUSED(block_);
    live_in_.ClearAllBits();
    live_out_.ClearAllBits();
    kill_.ClearAllBits();
  }

 private:
  const HBasicBlock& block_;
  ArenaBitVector live_in_;
  ArenaBitVector live_out_;
  ArenaBitVector kill_;

  friend class SsaLivenessAnalysis;

  DISALLOW_COPY_AND_ASSIGN(BlockInfo);
};

/**
 * A live range contains the start and end of a range where an instruction or a temporary
 * is live.
 */
class LiveRange FINAL : public ArenaObject<kArenaAllocSsaLiveness> {
 public:
  LiveRange(size_t start, size_t end, LiveRange* next) : start_(start), end_(end), next_(next) {
    DCHECK_LT(start, end);
    DCHECK(next_ == nullptr || next_->GetStart() > GetEnd());
  }

  size_t GetStart() const { return start_; }
  size_t GetEnd() const { return end_; }
  LiveRange* GetNext() const { return next_; }

  bool IntersectsWith(const LiveRange& other) const {
    return (start_ >= other.start_ && start_ < other.end_)
        || (other.start_ >= start_ && other.start_ < end_);
  }

  bool IsBefore(const LiveRange& other) const {
    return end_ <= other.start_;
  }

  void Dump(std::ostream& stream) const {
    stream << "[" << start_ << "," << end_ << ")";
  }

  LiveRange* Dup(ScopedArenaAllocator* allocator) const {
    return new (allocator) LiveRange(
        start_, end_, next_ == nullptr ? nullptr : next_->Dup(allocator));
  }

  LiveRange* GetLastRange() {
    return next_ == nullptr ? this : next_->GetLastRange();
  }

 private:
  size_t start_;
  size_t end_;
  LiveRange* next_;

  friend class LiveInterval;

  DISALLOW_COPY_AND_ASSIGN(LiveRange);
};

/**
 * A use position represents a live interval use at a given position.
 */
class UsePosition : public ArenaObject<kArenaAllocSsaLiveness>,
                    public IntrusiveForwardListNode<UsePosition> {
 public:
  UsePosition(HInstruction* user, size_t input_index, size_t position)
      : user_(user),
        input_index_(input_index),
        position_(position) {
  }

  explicit UsePosition(size_t position)
      : user_(nullptr),
        input_index_(kNoInput),
        position_(dchecked_integral_cast<uint32_t>(position)) {
  }

  size_t GetPosition() const { return position_; }

  HInstruction* GetUser() const { return user_; }

  bool IsSynthesized() const { return user_ == nullptr; }

  size_t GetInputIndex() const { return input_index_; }

  void Dump(std::ostream& stream) const {
    stream << position_;
  }

  HLoopInformation* GetLoopInformation() const {
    return user_->GetBlock()->GetLoopInformation();
  }

  UsePosition* Clone(ScopedArenaAllocator* allocator) const {
    return new (allocator) UsePosition(user_, input_index_, position_);
  }

  bool RequiresRegister() const {
    if (IsSynthesized()) return false;
    Location location = GetUser()->GetLocations()->InAt(GetInputIndex());
    return location.IsUnallocated() && location.RequiresRegisterKind();
  }

 private:
  static constexpr uint32_t kNoInput = static_cast<uint32_t>(-1);

  HInstruction* const user_;
  const size_t input_index_;
  const size_t position_;

  DISALLOW_COPY_AND_ASSIGN(UsePosition);
};
using UsePositionList = IntrusiveForwardList<UsePosition>;

/**
 * An environment use position represents a live interval for environment use at a given position.
 */
class EnvUsePosition : public ArenaObject<kArenaAllocSsaLiveness>,
                       public IntrusiveForwardListNode<EnvUsePosition> {
 public:
  EnvUsePosition(HEnvironment* environment,
                 size_t input_index,
                 size_t position)
      : environment_(environment),
        input_index_(input_index),
        position_(position) {
    DCHECK(environment != nullptr);
  }

  size_t GetPosition() const { return position_; }

  HEnvironment* GetEnvironment() const { return environment_; }
  size_t GetInputIndex() const { return input_index_; }

  void Dump(std::ostream& stream) const {
    stream << position_;
  }

  EnvUsePosition* Clone(ScopedArenaAllocator* allocator) const {
    return new (allocator) EnvUsePosition(environment_, input_index_, position_);
  }

 private:
  HEnvironment* const environment_;
  const size_t input_index_;
  const size_t position_;

  DISALLOW_COPY_AND_ASSIGN(EnvUsePosition);
};
using EnvUsePositionList = IntrusiveForwardList<EnvUsePosition>;

template <typename Iterator>
inline Iterator FindUseAtOrAfterPosition(Iterator first, Iterator last, size_t position) {
  using value_type = const typename Iterator::value_type;
  static_assert(std::is_same<value_type, const UsePosition>::value ||
                    std::is_same<value_type, const EnvUsePosition>::value,
                "Expecting value type UsePosition or EnvUsePosition.");
  Iterator ret = std::find_if(
      first, last, [position](const value_type& use) { return use.GetPosition() >= position; });
  // Check that the processed range is sorted. Do not check the rest of the range to avoid
  // increasing the complexity of callers from O(n) to O(n^2).
  DCHECK(std::is_sorted(
      first,
      ret,
      [](const value_type& lhs, const value_type& rhs) {
          return lhs.GetPosition() < rhs.GetPosition();
      }));
  return ret;
}

template <typename Iterator>
inline IterationRange<Iterator> FindMatchingUseRange(Iterator first,
                                                     Iterator last,
                                                     size_t position_begin,
                                                     size_t position_end) {
  Iterator begin = FindUseAtOrAfterPosition(first, last, position_begin);
  Iterator end = FindUseAtOrAfterPosition(begin, last, position_end);
  return MakeIterationRange(begin, end);
}

class SafepointPosition : public ArenaObject<kArenaAllocSsaLiveness> {
 public:
  explicit SafepointPosition(HInstruction* instruction)
      : instruction_(instruction),
        next_(nullptr) {}

  void SetNext(SafepointPosition* next) {
    next_ = next;
  }

  size_t GetPosition() const {
    return instruction_->GetLifetimePosition();
  }

  SafepointPosition* GetNext() const {
    return next_;
  }

  LocationSummary* GetLocations() const {
    return instruction_->GetLocations();
  }

  HInstruction* GetInstruction() const {
    return instruction_;
  }

 private:
  HInstruction* const instruction_;
  SafepointPosition* next_;

  DISALLOW_COPY_AND_ASSIGN(SafepointPosition);
};

/**
 * An interval is a list of disjoint live ranges where an instruction is live.
 * Each instruction that has uses gets an interval.
 */
class LiveInterval : public ArenaObject<kArenaAllocSsaLiveness> {
 public:
  static LiveInterval* MakeInterval(ScopedArenaAllocator* allocator,
                                    DataType::Type type,
                                    HInstruction* instruction = nullptr) {
    return new (allocator) LiveInterval(allocator, type, instruction);
  }

  static LiveInterval* MakeFixedInterval(ScopedArenaAllocator* allocator,
                                         int reg,
                                         DataType::Type type) {
    return new (allocator) LiveInterval(allocator, type, nullptr, true, reg, false);
  }

  static LiveInterval* MakeTempInterval(ScopedArenaAllocator* allocator, DataType::Type type) {
    return new (allocator) LiveInterval(allocator, type, nullptr, false, kNoRegister, true);
  }

  bool IsFixed() const { return is_fixed_; }
  bool IsTemp() const { return is_temp_; }
  // This interval is the result of a split.
  bool IsSplit() const { return parent_ != this; }

  void AddTempUse(HInstruction* instruction, size_t temp_index) {
    DCHECK(IsTemp());
    DCHECK(GetUses().empty()) << "A temporary can only have one user";
    DCHECK(GetEnvironmentUses().empty()) << "A temporary cannot have environment user";
    size_t position = instruction->GetLifetimePosition();
    UsePosition* new_use = new (allocator_) UsePosition(instruction, temp_index, position);
    uses_.push_front(*new_use);
    AddRange(position, position + 1);
  }

  // Record use of an input. The use will be recorded as an environment use if
  // `environment` is not null and as register use otherwise. If `actual_user`
  // is specified, the use will be recorded at `actual_user`'s lifetime position.
  void AddUse(HInstruction* instruction,
              HEnvironment* environment,
              size_t input_index,
              HInstruction* actual_user = nullptr,
              bool keep_alive = false) {
    bool is_environment = (environment != nullptr);
    LocationSummary* locations = instruction->GetLocations();
    if (actual_user == nullptr) {
      actual_user = instruction;
    }

    // Set the use within the instruction.
    size_t position = actual_user->GetLifetimePosition() + 1;
    if (!is_environment) {
      if (locations->IsFixedInput(input_index) || locations->OutputUsesSameAs(input_index)) {
        // For fixed inputs and output same as input, the register allocator
        // requires to have inputs die at the instruction, so that input moves use the
        // location of the input just before that instruction (and not potential moves due
        // to splitting).
        DCHECK_EQ(instruction, actual_user);
        position = actual_user->GetLifetimePosition();
      } else if (!locations->InAt(input_index).IsValid()) {
        return;
      }
    }

    if (!is_environment && instruction->IsInLoop()) {
      AddBackEdgeUses(*instruction->GetBlock());
    }

    if ((!uses_.empty()) &&
        (uses_.front().GetUser() == actual_user) &&
        (uses_.front().GetPosition() < position)) {
      // The user uses the instruction multiple times, and one use dies before the other.
      // We update the use list so that the latter is first.
      DCHECK(!is_environment);
      DCHECK(uses_.front().GetPosition() + 1 == position);
      UsePositionList::iterator next_pos = uses_.begin();
      UsePositionList::iterator insert_pos;
      do {
        insert_pos = next_pos;
        ++next_pos;
      } while (next_pos != uses_.end() && next_pos->GetPosition() < position);
      UsePosition* new_use = new (allocator_) UsePosition(instruction, input_index, position);
      uses_.insert_after(insert_pos, *new_use);
      if (first_range_->GetEnd() == uses_.front().GetPosition()) {
        first_range_->end_ = position;
      }
      return;
    }

    if (is_environment) {
      DCHECK(env_uses_.empty() || position <= env_uses_.front().GetPosition());
      EnvUsePosition* new_env_use =
          new (allocator_) EnvUsePosition(environment, input_index, position);
      env_uses_.push_front(*new_env_use);
    } else {
      DCHECK(uses_.empty() || position <= uses_.front().GetPosition());
      UsePosition* new_use = new (allocator_) UsePosition(instruction, input_index, position);
      uses_.push_front(*new_use);
    }

    if (is_environment && !keep_alive) {
      // If this environment use does not keep the instruction live, it does not
      // affect the live range of that instruction.
      return;
    }

    size_t start_block_position = instruction->GetBlock()->GetLifetimeStart();
    if (first_range_ == nullptr) {
      // First time we see a use of that interval.
      first_range_ = last_range_ = range_search_start_ =
          new (allocator_) LiveRange(start_block_position, position, nullptr);
    } else if (first_range_->GetStart() == start_block_position) {
      // There is a use later in the same block or in a following block.
      // Note that in such a case, `AddRange` for the whole blocks has been called
      // before arriving in this method, and this is the reason the start of
      // `first_range_` is before the given `position`.
      DCHECK_LE(position, first_range_->GetEnd());
    } else {
      DCHECK(first_range_->GetStart() > position);
      // There is a hole in the interval. Create a new range.
      // Note that the start of `first_range_` can be equal to `end`: two blocks
      // having adjacent lifetime positions are not necessarily
      // predecessor/successor. When two blocks are predecessor/successor, the
      // liveness algorithm has called `AddRange` before arriving in this method,
      // and the check line 205 would succeed.
      first_range_ = range_search_start_ =
          new (allocator_) LiveRange(start_block_position, position, first_range_);
    }
  }

  void AddPhiUse(HInstruction* instruction, size_t input_index, HBasicBlock* block) {
    DCHECK(instruction->IsPhi());
    if (block->IsInLoop()) {
      AddBackEdgeUses(*block);
    }
    UsePosition* new_use =
        new (allocator_) UsePosition(instruction, input_index, block->GetLifetimeEnd());
    uses_.push_front(*new_use);
  }

  ALWAYS_INLINE void AddRange(size_t start, size_t end) {
    if (first_range_ == nullptr) {
      first_range_ = last_range_ = range_search_start_ =
          new (allocator_) LiveRange(start, end, first_range_);
    } else if (first_range_->GetStart() == end) {
      // There is a use in the following block.
      first_range_->start_ = start;
    } else if (first_range_->GetStart() == start && first_range_->GetEnd() == end) {
      DCHECK(is_fixed_);
    } else {
      DCHECK_GT(first_range_->GetStart(), end);
      // There is a hole in the interval. Create a new range.
      first_range_ = range_search_start_ = new (allocator_) LiveRange(start, end, first_range_);
    }
  }

  void AddLoopRange(size_t start, size_t end) {
    DCHECK(first_range_ != nullptr);
    DCHECK_LE(start, first_range_->GetStart());
    // Find the range that covers the positions after the loop.
    LiveRange* after_loop = first_range_;
    LiveRange* last_in_loop = nullptr;
    while (after_loop != nullptr && after_loop->GetEnd() < end) {
      DCHECK_LE(start, after_loop->GetStart());
      last_in_loop = after_loop;
      after_loop = after_loop->GetNext();
    }
    if (after_loop == nullptr) {
      // Uses are only in the loop.
      first_range_ = last_range_ = range_search_start_ =
          new (allocator_) LiveRange(start, end, nullptr);
    } else if (after_loop->GetStart() <= end) {
      first_range_ = range_search_start_ = after_loop;
      // There are uses after the loop.
      first_range_->start_ = start;
    } else {
      // The use after the loop is after a lifetime hole.
      DCHECK(last_in_loop != nullptr);
      first_range_ = range_search_start_ = last_in_loop;
      first_range_->start_ = start;
      first_range_->end_ = end;
    }
  }

  bool HasSpillSlot() const { return spill_slot_ != kNoSpillSlot; }
  void SetSpillSlot(int slot) {
    DCHECK(!is_fixed_);
    DCHECK(!is_temp_);
    spill_slot_ = slot;
  }
  int GetSpillSlot() const { return spill_slot_; }

  void SetFrom(size_t from) {
    if (first_range_ != nullptr) {
      first_range_->start_ = from;
    } else {
      // Instruction without uses.
      DCHECK(uses_.empty());
      DCHECK(from == defined_by_->GetLifetimePosition());
      first_range_ = last_range_ = range_search_start_ =
          new (allocator_) LiveRange(from, from + 2, nullptr);
    }
  }

  LiveInterval* GetParent() const { return parent_; }

  // Returns whether this interval is the parent interval, that is, the interval
  // that starts where the HInstruction is defined.
  bool IsParent() const { return parent_ == this; }

  LiveRange* GetFirstRange() const { return first_range_; }
  LiveRange* GetLastRange() const { return last_range_; }

  int GetRegister() const { return register_; }
  void SetRegister(int reg) { register_ = reg; }
  void ClearRegister() { register_ = kNoRegister; }
  bool HasRegister() const { return register_ != kNoRegister; }

  bool IsDeadAt(size_t position) const {
    return GetEnd() <= position;
  }

  bool IsDefinedAt(size_t position) const {
    return GetStart() <= position && !IsDeadAt(position);
  }

  // Returns true if the interval contains a LiveRange covering `position`.
  // The range at or immediately after the current position of linear scan
  // is cached for better performance. If `position` can be smaller than
  // that, CoversSlow should be used instead.
  bool Covers(size_t position) {
    LiveRange* candidate = FindRangeAtOrAfter(position, range_search_start_);
    range_search_start_ = candidate;
    return (candidate != nullptr && candidate->GetStart() <= position);
  }

  // Same as Covers but always tests all ranges.
  bool CoversSlow(size_t position) const {
    LiveRange* candidate = FindRangeAtOrAfter(position, first_range_);
    return candidate != nullptr && candidate->GetStart() <= position;
  }

  // Returns the first intersection of this interval with `current`, which
  // must be the interval currently being allocated by linear scan.
  size_t FirstIntersectionWith(LiveInterval* current) const {
    // Find the first range after the start of `current`. We use the search
    // cache to improve performance.
    DCHECK(GetStart() <= current->GetStart() || IsFixed());
    LiveRange* other_range = current->first_range_;
    LiveRange* my_range = FindRangeAtOrAfter(other_range->GetStart(), range_search_start_);
    if (my_range == nullptr) {
      return kNoLifetime;
    }

    // Advance both intervals and find the first matching range start in
    // this interval.
    do {
      if (my_range->IsBefore(*other_range)) {
        my_range = my_range->GetNext();
        if (my_range == nullptr) {
          return kNoLifetime;
        }
      } else if (other_range->IsBefore(*my_range)) {
        other_range = other_range->GetNext();
        if (other_range == nullptr) {
          return kNoLifetime;
        }
      } else {
        DCHECK(my_range->IntersectsWith(*other_range));
        return std::max(my_range->GetStart(), other_range->GetStart());
      }
    } while (true);
  }

  size_t GetStart() const {
    return first_range_->GetStart();
  }

  size_t GetEnd() const {
    return last_range_->GetEnd();
  }

  size_t GetLength() const {
    return GetEnd() - GetStart();
  }

  size_t FirstRegisterUseAfter(size_t position) const {
    if (is_temp_) {
      return position == GetStart() ? position : kNoLifetime;
    }

    if (IsDefiningPosition(position) && DefinitionRequiresRegister()) {
      return position;
    }

    size_t end = GetEnd();
    for (const UsePosition& use : GetUses()) {
      size_t use_position = use.GetPosition();
      if (use_position > end) {
        break;
      }
      if (use_position > position) {
        if (use.RequiresRegister()) {
          return use_position;
        }
      }
    }
    return kNoLifetime;
  }

  // Returns the location of the first register use for this live interval,
  // including a register definition if applicable.
  size_t FirstRegisterUse() const {
    return FirstRegisterUseAfter(GetStart());
  }

  // Whether the interval requires a register rather than a stack location.
  // If needed for performance, this could be cached.
  bool RequiresRegister() const {
    return !HasRegister() && FirstRegisterUse() != kNoLifetime;
  }

  size_t FirstUseAfter(size_t position) const {
    if (is_temp_) {
      return position == GetStart() ? position : kNoLifetime;
    }

    if (IsDefiningPosition(position)) {
      DCHECK(defined_by_->GetLocations()->Out().IsValid());
      return position;
    }

    size_t end = GetEnd();
    for (const UsePosition& use : GetUses()) {
      size_t use_position = use.GetPosition();
      if (use_position > end) {
        break;
      }
      if (use_position > position) {
        return use_position;
      }
    }
    return kNoLifetime;
  }

  const UsePositionList& GetUses() const {
    return parent_->uses_;
  }

  const EnvUsePositionList& GetEnvironmentUses() const {
    return parent_->env_uses_;
  }

  DataType::Type GetType() const {
    return type_;
  }

  HInstruction* GetDefinedBy() const {
    return defined_by_;
  }

  bool HasWillCallSafepoint() const {
    for (SafepointPosition* safepoint = first_safepoint_;
         safepoint != nullptr;
         safepoint = safepoint->GetNext()) {
      if (safepoint->GetLocations()->WillCall()) return true;
    }
    return false;
  }

  SafepointPosition* FindSafepointJustBefore(size_t position) const {
    for (SafepointPosition* safepoint = first_safepoint_, *previous = nullptr;
         safepoint != nullptr;
         previous = safepoint, safepoint = safepoint->GetNext()) {
      if (safepoint->GetPosition() >= position) return previous;
    }
    return last_safepoint_;
  }

  /**
   * Split this interval at `position`. This interval is changed to:
   * [start ... position).
   *
   * The new interval covers:
   * [position ... end)
   */
  LiveInterval* SplitAt(size_t position) {
    DCHECK(!is_temp_);
    DCHECK(!is_fixed_);
    DCHECK_GT(position, GetStart());

    if (GetEnd() <= position) {
      // This range dies before `position`, no need to split.
      return nullptr;
    }

    LiveInterval* new_interval = new (allocator_) LiveInterval(allocator_, type_);
    SafepointPosition* new_last_safepoint = FindSafepointJustBefore(position);
    if (new_last_safepoint == nullptr) {
      new_interval->first_safepoint_ = first_safepoint_;
      new_interval->last_safepoint_ = last_safepoint_;
      first_safepoint_ = last_safepoint_ = nullptr;
    } else if (last_safepoint_ != new_last_safepoint) {
      new_interval->last_safepoint_ = last_safepoint_;
      new_interval->first_safepoint_ = new_last_safepoint->GetNext();
      DCHECK(new_interval->first_safepoint_ != nullptr);
      last_safepoint_ = new_last_safepoint;
      last_safepoint_->SetNext(nullptr);
    }

    new_interval->next_sibling_ = next_sibling_;
    next_sibling_ = new_interval;
    new_interval->parent_ = parent_;

    LiveRange* current = first_range_;
    LiveRange* previous = nullptr;
    // Iterate over the ranges, and either find a range that covers this position, or
    // two ranges in between this position (that is, the position is in a lifetime hole).
    do {
      if (position >= current->GetEnd()) {
        // Move to next range.
        previous = current;
        current = current->next_;
      } else if (position <= current->GetStart()) {
        // If the previous range did not cover this position, we know position is in
        // a lifetime hole. We can just break the first_range_ and last_range_ links
        // and return the new interval.
        DCHECK(previous != nullptr);
        DCHECK(current != first_range_);
        new_interval->last_range_ = last_range_;
        last_range_ = previous;
        previous->next_ = nullptr;
        new_interval->first_range_ = current;
        if (range_search_start_ != nullptr && range_search_start_->GetEnd() >= current->GetEnd()) {
          // Search start point is inside `new_interval`. Change it to null
          // (i.e. the end of the interval) in the original interval.
          range_search_start_ = nullptr;
        }
        new_interval->range_search_start_ = new_interval->first_range_;
        return new_interval;
      } else {
        // This range covers position. We create a new last_range_ for this interval
        // that covers last_range_->Start() and position. We also shorten the current
        // range and make it the first range of the new interval.
        DCHECK(position < current->GetEnd() && position > current->GetStart());
        new_interval->last_range_ = last_range_;
        last_range_ = new (allocator_) LiveRange(current->start_, position, nullptr);
        if (previous != nullptr) {
          previous->next_ = last_range_;
        } else {
          first_range_ = last_range_;
        }
        new_interval->first_range_ = current;
        current->start_ = position;
        if (range_search_start_ != nullptr && range_search_start_->GetEnd() >= current->GetEnd()) {
          // Search start point is inside `new_interval`. Change it to `last_range`
          // in the original interval. This is conservative but always correct.
          range_search_start_ = last_range_;
        }
        new_interval->range_search_start_ = new_interval->first_range_;
        return new_interval;
      }
    } while (current != nullptr);

    LOG(FATAL) << "Unreachable";
    return nullptr;
  }

  bool StartsBeforeOrAt(LiveInterval* other) const {
    return GetStart() <= other->GetStart();
  }

  bool StartsAfter(LiveInterval* other) const {
    return GetStart() > other->GetStart();
  }

  void Dump(std::ostream& stream) const {
    stream << "ranges: { ";
    LiveRange* current = first_range_;
    while (current != nullptr) {
      current->Dump(stream);
      stream << " ";
      current = current->GetNext();
    }
    stream << "}, uses: { ";
    for (const UsePosition& use : GetUses()) {
      use.Dump(stream);
      stream << " ";
    }
    stream << "}, { ";
    for (const EnvUsePosition& env_use : GetEnvironmentUses()) {
      env_use.Dump(stream);
      stream << " ";
    }
    stream << "}";
    stream << " is_fixed: " << is_fixed_ << ", is_split: " << IsSplit();
    stream << " is_low: " << IsLowInterval();
    stream << " is_high: " << IsHighInterval();
  }

  // Same as Dump, but adds context such as the instruction defining this interval, and
  // the register currently assigned to this interval.
  void DumpWithContext(std::ostream& stream, const CodeGenerator& codegen) const;

  LiveInterval* GetNextSibling() const { return next_sibling_; }
  LiveInterval* GetLastSibling() {
    LiveInterval* result = this;
    while (result->next_sibling_ != nullptr) {
      result = result->next_sibling_;
    }
    return result;
  }

  // Returns the first register hint that is at least free before
  // the value contained in `free_until`. If none is found, returns
  // `kNoRegister`.
  int FindFirstRegisterHint(size_t* free_until, const SsaLivenessAnalysis& liveness) const;

  // If there is enough at the definition site to find a register (for example
  // it uses the same input as the first input), returns the register as a hint.
  // Returns kNoRegister otherwise.
  int FindHintAtDefinition() const;

  // Returns the number of required spilling slots (measured as a multiple of the
  // Dex virtual register size `kVRegSize`).
  size_t NumberOfSpillSlotsNeeded() const;

  bool IsFloatingPoint() const {
    return type_ == DataType::Type::kFloat32 || type_ == DataType::Type::kFloat64;
  }

  // Converts the location of the interval to a `Location` object.
  Location ToLocation() const;

  // Returns the location of the interval following its siblings at `position`.
  Location GetLocationAt(size_t position);

  // Finds the sibling that is defined at `position`.
  LiveInterval* GetSiblingAt(size_t position);

  // Returns whether `other` and `this` share the same kind of register.
  bool SameRegisterKind(Location other) const;
  bool SameRegisterKind(const LiveInterval& other) const {
    return IsFloatingPoint() == other.IsFloatingPoint();
  }

  bool HasHighInterval() const {
    return IsLowInterval();
  }

  bool HasLowInterval() const {
    return IsHighInterval();
  }

  LiveInterval* GetLowInterval() const {
    DCHECK(HasLowInterval());
    return high_or_low_interval_;
  }

  LiveInterval* GetHighInterval() const {
    DCHECK(HasHighInterval());
    return high_or_low_interval_;
  }

  bool IsHighInterval() const {
    return GetParent()->is_high_interval_;
  }

  bool IsLowInterval() const {
    return !IsHighInterval() && (GetParent()->high_or_low_interval_ != nullptr);
  }

  void SetLowInterval(LiveInterval* low) {
    DCHECK(IsHighInterval());
    high_or_low_interval_ = low;
  }

  void SetHighInterval(LiveInterval* high) {
    DCHECK(IsLowInterval());
    high_or_low_interval_ = high;
  }

  void AddHighInterval(bool is_temp = false) {
    DCHECK(IsParent());
    DCHECK(!HasHighInterval());
    DCHECK(!HasLowInterval());
    high_or_low_interval_ = new (allocator_) LiveInterval(
        allocator_, type_, defined_by_, false, kNoRegister, is_temp, true);
    high_or_low_interval_->high_or_low_interval_ = this;
    if (first_range_ != nullptr) {
      high_or_low_interval_->first_range_ = first_range_->Dup(allocator_);
      high_or_low_interval_->last_range_ = high_or_low_interval_->first_range_->GetLastRange();
      high_or_low_interval_->range_search_start_ = high_or_low_interval_->first_range_;
    }
    auto pos = high_or_low_interval_->uses_.before_begin();
    for (const UsePosition& use : uses_) {
      UsePosition* new_use = use.Clone(allocator_);
      pos = high_or_low_interval_->uses_.insert_after(pos, *new_use);
    }

    auto env_pos = high_or_low_interval_->env_uses_.before_begin();
    for (const EnvUsePosition& env_use : env_uses_) {
      EnvUsePosition* new_env_use = env_use.Clone(allocator_);
      env_pos = high_or_low_interval_->env_uses_.insert_after(env_pos, *new_env_use);
    }
  }

  // Returns whether an interval, when it is non-split, is using
  // the same register of one of its input.
  bool IsUsingInputRegister() const {
    CHECK(kIsDebugBuild) << "Function should be used only for DCHECKs";
    if (defined_by_ != nullptr && !IsSplit()) {
      for (const HInstruction* input : defined_by_->GetInputs()) {
        LiveInterval* interval = input->GetLiveInterval();

        // Find the interval that covers `defined_by`_. Calls to this function
        // are made outside the linear scan, hence we need to use CoversSlow.
        while (interval != nullptr && !interval->CoversSlow(defined_by_->GetLifetimePosition())) {
          interval = interval->GetNextSibling();
        }

        // Check if both intervals have the same register of the same kind.
        if (interval != nullptr
            && interval->SameRegisterKind(*this)
            && interval->GetRegister() == GetRegister()) {
          return true;
        }
      }
    }
    return false;
  }

  // Returns whether an interval, when it is non-split, can safely use
  // the same register of one of its input. Note that this method requires
  // IsUsingInputRegister() to be true.
  bool CanUseInputRegister() const {
    CHECK(kIsDebugBuild) << "Function should be used only for DCHECKs";
    DCHECK(IsUsingInputRegister());
    if (defined_by_ != nullptr && !IsSplit()) {
      LocationSummary* locations = defined_by_->GetLocations();
      if (locations->OutputCanOverlapWithInputs()) {
        return false;
      }
      for (const HInstruction* input : defined_by_->GetInputs()) {
        LiveInterval* interval = input->GetLiveInterval();

        // Find the interval that covers `defined_by`_. Calls to this function
        // are made outside the linear scan, hence we need to use CoversSlow.
        while (interval != nullptr && !interval->CoversSlow(defined_by_->GetLifetimePosition())) {
          interval = interval->GetNextSibling();
        }

        if (interval != nullptr
            && interval->SameRegisterKind(*this)
            && interval->GetRegister() == GetRegister()) {
          // We found the input that has the same register. Check if it is live after
          // `defined_by`_.
          return !interval->CoversSlow(defined_by_->GetLifetimePosition() + 1);
        }
      }
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

  void AddSafepoint(HInstruction* instruction) {
    SafepointPosition* safepoint = new (allocator_) SafepointPosition(instruction);
    if (first_safepoint_ == nullptr) {
      first_safepoint_ = last_safepoint_ = safepoint;
    } else {
      DCHECK_LT(last_safepoint_->GetPosition(), safepoint->GetPosition());
      last_safepoint_->SetNext(safepoint);
      last_safepoint_ = safepoint;
    }
  }

  SafepointPosition* GetFirstSafepoint() const {
    return first_safepoint_;
  }

  // Resets the starting point for range-searching queries to the first range.
  // Intervals must be reset prior to starting a new linear scan over them.
  void ResetSearchCache() {
    range_search_start_ = first_range_;
  }

  bool DefinitionRequiresRegister() const {
    DCHECK(IsParent());
    LocationSummary* locations = defined_by_->GetLocations();
    Location location = locations->Out();
    // This interval is the first interval of the instruction. If the output
    // of the instruction requires a register, we return the position of that instruction
    // as the first register use.
    if (location.IsUnallocated()) {
      if ((location.GetPolicy() == Location::kRequiresRegister)
           || (location.GetPolicy() == Location::kSameAsFirstInput
               && (locations->InAt(0).IsRegister()
                   || locations->InAt(0).IsRegisterPair()
                   || locations->InAt(0).GetPolicy() == Location::kRequiresRegister))) {
        return true;
      } else if ((location.GetPolicy() == Location::kRequiresFpuRegister)
                 || (location.GetPolicy() == Location::kSameAsFirstInput
                     && (locations->InAt(0).IsFpuRegister()
                         || locations->InAt(0).IsFpuRegisterPair()
                         || locations->InAt(0).GetPolicy() == Location::kRequiresFpuRegister))) {
        return true;
      }
    } else if (location.IsRegister() || location.IsRegisterPair()) {
      return true;
    }
    return false;
  }

 private:
  LiveInterval(ScopedArenaAllocator* allocator,
               DataType::Type type,
               HInstruction* defined_by = nullptr,
               bool is_fixed = false,
               int reg = kNoRegister,
               bool is_temp = false,
               bool is_high_interval = false)
      : allocator_(allocator),
        first_range_(nullptr),
        last_range_(nullptr),
        range_search_start_(nullptr),
        first_safepoint_(nullptr),
        last_safepoint_(nullptr),
        uses_(),
        env_uses_(),
        type_(type),
        next_sibling_(nullptr),
        parent_(this),
        register_(reg),
        spill_slot_(kNoSpillSlot),
        is_fixed_(is_fixed),
        is_temp_(is_temp),
        is_high_interval_(is_high_interval),
        high_or_low_interval_(nullptr),
        defined_by_(defined_by) {}

  // Searches for a LiveRange that either covers the given position or is the
  // first next LiveRange. Returns null if no such LiveRange exists. Ranges
  // known to end before `position` can be skipped with `search_start`.
  LiveRange* FindRangeAtOrAfter(size_t position, LiveRange* search_start) const {
    if (kIsDebugBuild) {
      if (search_start != first_range_) {
        // If we are not searching the entire list of ranges, make sure we do
        // not skip the range we are searching for.
        if (search_start == nullptr) {
          DCHECK(IsDeadAt(position));
        } else if (search_start->GetStart() > position) {
          DCHECK_EQ(search_start, FindRangeAtOrAfter(position, first_range_));
        }
      }
    }

    LiveRange* range;
    for (range = search_start;
         range != nullptr && range->GetEnd() <= position;
         range = range->GetNext()) {
      continue;
    }
    return range;
  }

  bool IsDefiningPosition(size_t position) const {
    return IsParent() && (position == GetStart());
  }

  bool HasSynthesizeUseAt(size_t position) const {
    for (const UsePosition& use : GetUses()) {
      size_t use_position = use.GetPosition();
      if ((use_position == position) && use.IsSynthesized()) {
        return true;
      }
      if (use_position > position) break;
    }
    return false;
  }

  void AddBackEdgeUses(const HBasicBlock& block_at_use) {
    DCHECK(block_at_use.IsInLoop());
    if (block_at_use.GetGraph()->HasIrreducibleLoops()) {
      // Linear order may not be well formed when irreducible loops are present,
      // i.e. loop blocks may not be adjacent and a back edge may not be last,
      // which violates assumptions made in this method.
      return;
    }

    // Add synthesized uses at the back edge of loops to help the register allocator.
    // Note that this method is called in decreasing liveness order, to faciliate adding
    // uses at the head of the `uses_` list. Because below
    // we iterate from inner-most to outer-most, which is in increasing liveness order,
    // we need to add subsequent entries after the last inserted entry.
    const UsePositionList::iterator old_begin = uses_.begin();
    UsePositionList::iterator insert_pos = uses_.before_begin();
    for (HLoopInformationOutwardIterator it(block_at_use);
         !it.Done();
         it.Advance()) {
      HLoopInformation* current = it.Current();
      if (GetDefinedBy()->GetLifetimePosition() >= current->GetHeader()->GetLifetimeStart()) {
        // This interval is defined in the loop. We can stop going outward.
        break;
      }

      // We're only adding a synthesized use at the last back edge. Adding synthesized uses on
      // all back edges is not necessary: anything used in the loop will have its use at the
      // last back edge. If we want branches in a loop to have better register allocation than
      // another branch, then it is the linear order we should change.
      size_t back_edge_use_position = current->GetLifetimeEnd();
      if ((old_begin != uses_.end()) && (old_begin->GetPosition() <= back_edge_use_position)) {
        // There was a use already seen in this loop. Therefore the previous call to `AddUse`
        // already inserted the backedge use. We can stop going outward.
        DCHECK(HasSynthesizeUseAt(back_edge_use_position));
        break;
      }

      DCHECK(insert_pos != uses_.before_begin()
             ? back_edge_use_position > insert_pos->GetPosition()
             : current == block_at_use.GetLoopInformation())
          << std::distance(uses_.before_begin(), insert_pos);

      UsePosition* new_use = new (allocator_) UsePosition(back_edge_use_position);
      insert_pos = uses_.insert_after(insert_pos, *new_use);
    }
  }

  ScopedArenaAllocator* const allocator_;

  // Ranges of this interval. We need a quick access to the last range to test
  // for liveness (see `IsDeadAt`).
  LiveRange* first_range_;
  LiveRange* last_range_;

  // The first range at or after the current position of a linear scan. It is
  // used to optimize range-searching queries.
  LiveRange* range_search_start_;

  // Safepoints where this interval is live.
  SafepointPosition* first_safepoint_;
  SafepointPosition* last_safepoint_;

  // Uses of this interval. Only the parent interval keeps these lists.
  UsePositionList uses_;
  EnvUsePositionList env_uses_;

  // The instruction type this interval corresponds to.
  const DataType::Type type_;

  // Live interval that is the result of a split.
  LiveInterval* next_sibling_;

  // The first interval from which split intervals come from.
  LiveInterval* parent_;

  // The register allocated to this interval.
  int register_;

  // The spill slot allocated to this interval.
  int spill_slot_;

  // Whether the interval is for a fixed register.
  const bool is_fixed_;

  // Whether the interval is for a temporary.
  const bool is_temp_;

  // Whether this interval is a synthesized interval for register pair.
  const bool is_high_interval_;

  // If this interval needs a register pair, the high or low equivalent.
  // `is_high_interval_` tells whether this holds the low or the high.
  LiveInterval* high_or_low_interval_;

  // The instruction represented by this interval.
  HInstruction* const defined_by_;

  static constexpr int kNoRegister = -1;
  static constexpr int kNoSpillSlot = -1;

  ART_FRIEND_TEST(RegisterAllocatorTest, SpillInactive);

  DISALLOW_COPY_AND_ASSIGN(LiveInterval);
};

/**
 * Analysis that computes the liveness of instructions:
 *
 * (a) Non-environment uses of an instruction always make
 *     the instruction live.
 * (b) Environment uses of an instruction whose type is
 *     object (that is, non-primitive), make the instruction live.
 *     This is due to having to keep alive objects that have
 *     finalizers deleting native objects.
 * (c) When the graph has the debuggable property, environment uses
 *     of an instruction that has a primitive type make the instruction live.
 *     If the graph does not have the debuggable property, the environment
 *     use has no effect, and may get a 'none' value after register allocation.
 *
 * (b) and (c) are implemented through SsaLivenessAnalysis::ShouldBeLiveForEnvironment.
 */
class SsaLivenessAnalysis : public ValueObject {
 public:
  SsaLivenessAnalysis(HGraph* graph, CodeGenerator* codegen, ScopedArenaAllocator* allocator)
      : graph_(graph),
        codegen_(codegen),
        allocator_(allocator),
        block_infos_(graph->GetBlocks().size(),
                     nullptr,
                     allocator_->Adapter(kArenaAllocSsaLiveness)),
        instructions_from_ssa_index_(allocator_->Adapter(kArenaAllocSsaLiveness)),
        instructions_from_lifetime_position_(allocator_->Adapter(kArenaAllocSsaLiveness)),
        number_of_ssa_values_(0) {
  }

  void Analyze();

  BitVector* GetLiveInSet(const HBasicBlock& block) const {
    return &block_infos_[block.GetBlockId()]->live_in_;
  }

  BitVector* GetLiveOutSet(const HBasicBlock& block) const {
    return &block_infos_[block.GetBlockId()]->live_out_;
  }

  BitVector* GetKillSet(const HBasicBlock& block) const {
    return &block_infos_[block.GetBlockId()]->kill_;
  }

  HInstruction* GetInstructionFromSsaIndex(size_t index) const {
    return instructions_from_ssa_index_[index];
  }

  HInstruction* GetInstructionFromPosition(size_t index) const {
    return instructions_from_lifetime_position_[index];
  }

  HBasicBlock* GetBlockFromPosition(size_t index) const {
    HInstruction* instruction = GetInstructionFromPosition(index);
    if (instruction == nullptr) {
      // If we are at a block boundary, get the block following.
      instruction = GetInstructionFromPosition(index + 1);
    }
    return instruction->GetBlock();
  }

  bool IsAtBlockBoundary(size_t index) const {
    return GetInstructionFromPosition(index) == nullptr;
  }

  HInstruction* GetTempUser(LiveInterval* temp) const {
    // A temporary shares the same lifetime start as the instruction that requires it.
    DCHECK(temp->IsTemp());
    HInstruction* user = GetInstructionFromPosition(temp->GetStart() / 2);
    DCHECK_EQ(user, temp->GetUses().front().GetUser());
    return user;
  }

  size_t GetTempIndex(LiveInterval* temp) const {
    // We use the input index to store the index of the temporary in the user's temporary list.
    DCHECK(temp->IsTemp());
    return temp->GetUses().front().GetInputIndex();
  }

  size_t GetMaxLifetimePosition() const {
    return instructions_from_lifetime_position_.size() * 2 - 1;
  }

  size_t GetNumberOfSsaValues() const {
    return number_of_ssa_values_;
  }

  static constexpr const char* kLivenessPassName = "liveness";

 private:
  // Give an SSA number to each instruction that defines a value used by another instruction,
  // and setup the lifetime information of each instruction and block.
  void NumberInstructions();

  // Compute live ranges of instructions, as well as live_in, live_out and kill sets.
  void ComputeLiveness();

  // Compute the live ranges of instructions, as well as the initial live_in, live_out and
  // kill sets, that do not take into account backward branches.
  void ComputeLiveRanges();

  // After computing the initial sets, this method does a fixed point
  // calculation over the live_in and live_out set to take into account
  // backwards branches.
  void ComputeLiveInAndLiveOutSets();

  // Update the live_in set of the block and returns whether it has changed.
  bool UpdateLiveIn(const HBasicBlock& block);

  // Update the live_out set of the block and returns whether it has changed.
  bool UpdateLiveOut(const HBasicBlock& block);

  // Returns whether `instruction` in an HEnvironment held by `env_holder`
  // should be kept live by the HEnvironment.
  static bool ShouldBeLiveForEnvironment(HInstruction* env_holder, HInstruction* instruction) {
    if (instruction == nullptr) return false;
    // A value that's not live in compiled code may still be needed in interpreter,
    // due to code motion, etc.
    if (env_holder->IsDeoptimize()) return true;
    // A value live at a throwing instruction in a try block may be copied by
    // the exception handler to its location at the top of the catch block.
    if (env_holder->CanThrowIntoCatchBlock()) return true;
    if (instruction->GetBlock()->GetGraph()->IsDebuggable()) return true;
    return instruction->GetType() == DataType::Type::kReference;
  }

  void CheckNoLiveInIrreducibleLoop(const HBasicBlock& block) const {
    if (!block.IsLoopHeader() || !block.GetLoopInformation()->IsIrreducible()) {
      return;
    }
    BitVector* live_in = GetLiveInSet(block);
    // To satisfy our liveness algorithm, we need to ensure loop headers of
    // irreducible loops do not have any live-in instructions, except constants
    // and the current method, which can be trivially re-materialized.
    for (uint32_t idx : live_in->Indexes()) {
      HInstruction* instruction = GetInstructionFromSsaIndex(idx);
      DCHECK(instruction->GetBlock()->IsEntryBlock()) << instruction->DebugName();
      DCHECK(!instruction->IsParameterValue());
      DCHECK(instruction->IsCurrentMethod() || instruction->IsConstant())
          << instruction->DebugName();
    }
  }

  HGraph* const graph_;
  CodeGenerator* const codegen_;

  // Use a local ScopedArenaAllocator for allocating memory.
  // This allocator must remain alive while doing register allocation.
  ScopedArenaAllocator* const allocator_;

  ScopedArenaVector<BlockInfo*> block_infos_;

  // Temporary array used when computing live_in, live_out, and kill sets.
  ScopedArenaVector<HInstruction*> instructions_from_ssa_index_;

  // Temporary array used when inserting moves in the graph.
  ScopedArenaVector<HInstruction*> instructions_from_lifetime_position_;
  size_t number_of_ssa_values_;

  ART_FRIEND_TEST(RegisterAllocatorTest, SpillInactive);
  ART_FRIEND_TEST(RegisterAllocatorTest, FreeUntil);

  DISALLOW_COPY_AND_ASSIGN(SsaLivenessAnalysis);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SSA_LIVENESS_ANALYSIS_H_
