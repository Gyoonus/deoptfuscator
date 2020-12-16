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

#include "bounds_check_elimination.h"

#include <limits>

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "induction_var_range.h"
#include "nodes.h"
#include "side_effects_analysis.h"

namespace art {

class MonotonicValueRange;

/**
 * A value bound is represented as a pair of value and constant,
 * e.g. array.length - 1.
 */
class ValueBound : public ValueObject {
 public:
  ValueBound(HInstruction* instruction, int32_t constant) {
    if (instruction != nullptr && instruction->IsIntConstant()) {
      // Normalize ValueBound with constant instruction.
      int32_t instr_const = instruction->AsIntConstant()->GetValue();
      if (!WouldAddOverflowOrUnderflow(instr_const, constant)) {
        instruction_ = nullptr;
        constant_ = instr_const + constant;
        return;
      }
    }
    instruction_ = instruction;
    constant_ = constant;
  }

  // Return whether (left + right) overflows or underflows.
  static bool WouldAddOverflowOrUnderflow(int32_t left, int32_t right) {
    if (right == 0) {
      return false;
    }
    if ((right > 0) && (left <= (std::numeric_limits<int32_t>::max() - right))) {
      // No overflow.
      return false;
    }
    if ((right < 0) && (left >= (std::numeric_limits<int32_t>::min() - right))) {
      // No underflow.
      return false;
    }
    return true;
  }

  // Return true if instruction can be expressed as "left_instruction + right_constant".
  static bool IsAddOrSubAConstant(HInstruction* instruction,
                                  /* out */ HInstruction** left_instruction,
                                  /* out */ int32_t* right_constant) {
    HInstruction* left_so_far = nullptr;
    int32_t right_so_far = 0;
    while (instruction->IsAdd() || instruction->IsSub()) {
      HBinaryOperation* bin_op = instruction->AsBinaryOperation();
      HInstruction* left = bin_op->GetLeft();
      HInstruction* right = bin_op->GetRight();
      if (right->IsIntConstant()) {
        int32_t v = right->AsIntConstant()->GetValue();
        int32_t c = instruction->IsAdd() ? v : -v;
        if (!WouldAddOverflowOrUnderflow(right_so_far, c)) {
          instruction = left;
          left_so_far = left;
          right_so_far += c;
          continue;
        }
      }
      break;
    }
    // Return result: either false and "null+0" or true and "instr+constant".
    *left_instruction = left_so_far;
    *right_constant = right_so_far;
    return left_so_far != nullptr;
  }

  // Expresses any instruction as a value bound.
  static ValueBound AsValueBound(HInstruction* instruction) {
    if (instruction->IsIntConstant()) {
      return ValueBound(nullptr, instruction->AsIntConstant()->GetValue());
    }
    HInstruction *left;
    int32_t right;
    if (IsAddOrSubAConstant(instruction, &left, &right)) {
      return ValueBound(left, right);
    }
    return ValueBound(instruction, 0);
  }

  // Try to detect useful value bound format from an instruction, e.g.
  // a constant or array length related value.
  static ValueBound DetectValueBoundFromValue(HInstruction* instruction, /* out */ bool* found) {
    DCHECK(instruction != nullptr);
    if (instruction->IsIntConstant()) {
      *found = true;
      return ValueBound(nullptr, instruction->AsIntConstant()->GetValue());
    }

    if (instruction->IsArrayLength()) {
      *found = true;
      return ValueBound(instruction, 0);
    }
    // Try to detect (array.length + c) format.
    HInstruction *left;
    int32_t right;
    if (IsAddOrSubAConstant(instruction, &left, &right)) {
      if (left->IsArrayLength()) {
        *found = true;
        return ValueBound(left, right);
      }
    }

    // No useful bound detected.
    *found = false;
    return ValueBound::Max();
  }

  HInstruction* GetInstruction() const { return instruction_; }
  int32_t GetConstant() const { return constant_; }

  bool IsRelatedToArrayLength() const {
    // Some bounds are created with HNewArray* as the instruction instead
    // of HArrayLength*. They are treated the same.
    return (instruction_ != nullptr) &&
           (instruction_->IsArrayLength() || instruction_->IsNewArray());
  }

  bool IsConstant() const {
    return instruction_ == nullptr;
  }

  static ValueBound Min() { return ValueBound(nullptr, std::numeric_limits<int32_t>::min()); }
  static ValueBound Max() { return ValueBound(nullptr, std::numeric_limits<int32_t>::max()); }

  bool Equals(ValueBound bound) const {
    return instruction_ == bound.instruction_ && constant_ == bound.constant_;
  }

  static bool Equal(HInstruction* instruction1, HInstruction* instruction2) {
    if (instruction1 == instruction2) {
      return true;
    }
    if (instruction1 == nullptr || instruction2 == nullptr) {
      return false;
    }
    instruction1 = HuntForDeclaration(instruction1);
    instruction2 = HuntForDeclaration(instruction2);
    return instruction1 == instruction2;
  }

  // Returns if it's certain this->bound >= `bound`.
  bool GreaterThanOrEqualTo(ValueBound bound) const {
    if (Equal(instruction_, bound.instruction_)) {
      return constant_ >= bound.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Returns if it's certain this->bound <= `bound`.
  bool LessThanOrEqualTo(ValueBound bound) const {
    if (Equal(instruction_, bound.instruction_)) {
      return constant_ <= bound.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Returns if it's certain this->bound > `bound`.
  bool GreaterThan(ValueBound bound) const {
    if (Equal(instruction_, bound.instruction_)) {
      return constant_ > bound.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Returns if it's certain this->bound < `bound`.
  bool LessThan(ValueBound bound) const {
    if (Equal(instruction_, bound.instruction_)) {
      return constant_ < bound.constant_;
    }
    // Not comparable. Just return false.
    return false;
  }

  // Try to narrow lower bound. Returns the greatest of the two if possible.
  // Pick one if they are not comparable.
  static ValueBound NarrowLowerBound(ValueBound bound1, ValueBound bound2) {
    if (bound1.GreaterThanOrEqualTo(bound2)) {
      return bound1;
    }
    if (bound2.GreaterThanOrEqualTo(bound1)) {
      return bound2;
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor constant as lower bound.
    return bound1.IsConstant() ? bound1 : bound2;
  }

  // Try to narrow upper bound. Returns the lowest of the two if possible.
  // Pick one if they are not comparable.
  static ValueBound NarrowUpperBound(ValueBound bound1, ValueBound bound2) {
    if (bound1.LessThanOrEqualTo(bound2)) {
      return bound1;
    }
    if (bound2.LessThanOrEqualTo(bound1)) {
      return bound2;
    }

    // Not comparable. Just pick one. We may lose some info, but that's ok.
    // Favor array length as upper bound.
    return bound1.IsRelatedToArrayLength() ? bound1 : bound2;
  }

  // Add a constant to a ValueBound.
  // `overflow` or `underflow` will return whether the resulting bound may
  // overflow or underflow an int.
  ValueBound Add(int32_t c, /* out */ bool* overflow, /* out */ bool* underflow) const {
    *overflow = *underflow = false;
    if (c == 0) {
      return *this;
    }

    int32_t new_constant;
    if (c > 0) {
      if (constant_ > (std::numeric_limits<int32_t>::max() - c)) {
        *overflow = true;
        return Max();
      }

      new_constant = constant_ + c;
      // (array.length + non-positive-constant) won't overflow an int.
      if (IsConstant() || (IsRelatedToArrayLength() && new_constant <= 0)) {
        return ValueBound(instruction_, new_constant);
      }
      // Be conservative.
      *overflow = true;
      return Max();
    } else {
      if (constant_ < (std::numeric_limits<int32_t>::min() - c)) {
        *underflow = true;
        return Min();
      }

      new_constant = constant_ + c;
      // Regardless of the value new_constant, (array.length+new_constant) will
      // never underflow since array.length is no less than 0.
      if (IsConstant() || IsRelatedToArrayLength()) {
        return ValueBound(instruction_, new_constant);
      }
      // Be conservative.
      *underflow = true;
      return Min();
    }
  }

 private:
  HInstruction* instruction_;
  int32_t constant_;
};

/**
 * Represent a range of lower bound and upper bound, both being inclusive.
 * Currently a ValueRange may be generated as a result of the following:
 * comparisons related to array bounds, array bounds check, add/sub on top
 * of an existing value range, NewArray or a loop phi corresponding to an
 * incrementing/decrementing array index (MonotonicValueRange).
 */
class ValueRange : public ArenaObject<kArenaAllocBoundsCheckElimination> {
 public:
  ValueRange(ScopedArenaAllocator* allocator, ValueBound lower, ValueBound upper)
      : allocator_(allocator), lower_(lower), upper_(upper) {}

  virtual ~ValueRange() {}

  virtual MonotonicValueRange* AsMonotonicValueRange() { return nullptr; }
  bool IsMonotonicValueRange() {
    return AsMonotonicValueRange() != nullptr;
  }

  ScopedArenaAllocator* GetAllocator() const { return allocator_; }
  ValueBound GetLower() const { return lower_; }
  ValueBound GetUpper() const { return upper_; }

  bool IsConstantValueRange() const { return lower_.IsConstant() && upper_.IsConstant(); }

  // If it's certain that this value range fits in other_range.
  virtual bool FitsIn(ValueRange* other_range) const {
    if (other_range == nullptr) {
      return true;
    }
    DCHECK(!other_range->IsMonotonicValueRange());
    return lower_.GreaterThanOrEqualTo(other_range->lower_) &&
           upper_.LessThanOrEqualTo(other_range->upper_);
  }

  // Returns the intersection of this and range.
  // If it's not possible to do intersection because some
  // bounds are not comparable, it's ok to pick either bound.
  virtual ValueRange* Narrow(ValueRange* range) {
    if (range == nullptr) {
      return this;
    }

    if (range->IsMonotonicValueRange()) {
      return this;
    }

    return new (allocator_) ValueRange(
        allocator_,
        ValueBound::NarrowLowerBound(lower_, range->lower_),
        ValueBound::NarrowUpperBound(upper_, range->upper_));
  }

  // Shift a range by a constant.
  ValueRange* Add(int32_t constant) const {
    bool overflow, underflow;
    ValueBound lower = lower_.Add(constant, &overflow, &underflow);
    if (underflow) {
      // Lower bound underflow will wrap around to positive values
      // and invalidate the upper bound.
      return nullptr;
    }
    ValueBound upper = upper_.Add(constant, &overflow, &underflow);
    if (overflow) {
      // Upper bound overflow will wrap around to negative values
      // and invalidate the lower bound.
      return nullptr;
    }
    return new (allocator_) ValueRange(allocator_, lower, upper);
  }

 private:
  ScopedArenaAllocator* const allocator_;
  const ValueBound lower_;  // inclusive
  const ValueBound upper_;  // inclusive

  DISALLOW_COPY_AND_ASSIGN(ValueRange);
};

/**
 * A monotonically incrementing/decrementing value range, e.g.
 * the variable i in "for (int i=0; i<array.length; i++)".
 * Special care needs to be taken to account for overflow/underflow
 * of such value ranges.
 */
class MonotonicValueRange : public ValueRange {
 public:
  MonotonicValueRange(ScopedArenaAllocator* allocator,
                      HPhi* induction_variable,
                      HInstruction* initial,
                      int32_t increment,
                      ValueBound bound)
      // To be conservative, give it full range [Min(), Max()] in case it's
      // used as a regular value range, due to possible overflow/underflow.
      : ValueRange(allocator, ValueBound::Min(), ValueBound::Max()),
        induction_variable_(induction_variable),
        initial_(initial),
        increment_(increment),
        bound_(bound) {}

  virtual ~MonotonicValueRange() {}

  int32_t GetIncrement() const { return increment_; }
  ValueBound GetBound() const { return bound_; }
  HBasicBlock* GetLoopHeader() const {
    DCHECK(induction_variable_->GetBlock()->IsLoopHeader());
    return induction_variable_->GetBlock();
  }

  MonotonicValueRange* AsMonotonicValueRange() OVERRIDE { return this; }

  // If it's certain that this value range fits in other_range.
  bool FitsIn(ValueRange* other_range) const OVERRIDE {
    if (other_range == nullptr) {
      return true;
    }
    DCHECK(!other_range->IsMonotonicValueRange());
    return false;
  }

  // Try to narrow this MonotonicValueRange given another range.
  // Ideally it will return a normal ValueRange. But due to
  // possible overflow/underflow, that may not be possible.
  ValueRange* Narrow(ValueRange* range) OVERRIDE {
    if (range == nullptr) {
      return this;
    }
    DCHECK(!range->IsMonotonicValueRange());

    if (increment_ > 0) {
      // Monotonically increasing.
      ValueBound lower = ValueBound::NarrowLowerBound(bound_, range->GetLower());
      if (!lower.IsConstant() || lower.GetConstant() == std::numeric_limits<int32_t>::min()) {
        // Lower bound isn't useful. Leave it to deoptimization.
        return this;
      }

      // We currently conservatively assume max array length is Max().
      // If we can make assumptions about the max array length, e.g. due to the max heap size,
      // divided by the element size (such as 4 bytes for each integer array), we can
      // lower this number and rule out some possible overflows.
      int32_t max_array_len = std::numeric_limits<int32_t>::max();

      // max possible integer value of range's upper value.
      int32_t upper = std::numeric_limits<int32_t>::max();
      // Try to lower upper.
      ValueBound upper_bound = range->GetUpper();
      if (upper_bound.IsConstant()) {
        upper = upper_bound.GetConstant();
      } else if (upper_bound.IsRelatedToArrayLength() && upper_bound.GetConstant() <= 0) {
        // Normal case. e.g. <= array.length - 1.
        upper = max_array_len + upper_bound.GetConstant();
      }

      // If we can prove for the last number in sequence of initial_,
      // initial_ + increment_, initial_ + 2 x increment_, ...
      // that's <= upper, (last_num_in_sequence + increment_) doesn't trigger overflow,
      // then this MonoticValueRange is narrowed to a normal value range.

      // Be conservative first, assume last number in the sequence hits upper.
      int32_t last_num_in_sequence = upper;
      if (initial_->IsIntConstant()) {
        int32_t initial_constant = initial_->AsIntConstant()->GetValue();
        if (upper <= initial_constant) {
          last_num_in_sequence = upper;
        } else {
          // Cast to int64_t for the substraction part to avoid int32_t overflow.
          last_num_in_sequence = initial_constant +
              ((int64_t)upper - (int64_t)initial_constant) / increment_ * increment_;
        }
      }
      if (last_num_in_sequence <= (std::numeric_limits<int32_t>::max() - increment_)) {
        // No overflow. The sequence will be stopped by the upper bound test as expected.
        return new (GetAllocator()) ValueRange(GetAllocator(), lower, range->GetUpper());
      }

      // There might be overflow. Give up narrowing.
      return this;
    } else {
      DCHECK_NE(increment_, 0);
      // Monotonically decreasing.
      ValueBound upper = ValueBound::NarrowUpperBound(bound_, range->GetUpper());
      if ((!upper.IsConstant() || upper.GetConstant() == std::numeric_limits<int32_t>::max()) &&
          !upper.IsRelatedToArrayLength()) {
        // Upper bound isn't useful. Leave it to deoptimization.
        return this;
      }

      // Need to take care of underflow. Try to prove underflow won't happen
      // for common cases.
      if (range->GetLower().IsConstant()) {
        int32_t constant = range->GetLower().GetConstant();
        if (constant >= (std::numeric_limits<int32_t>::min() - increment_)) {
          return new (GetAllocator()) ValueRange(GetAllocator(), range->GetLower(), upper);
        }
      }

      // For non-constant lower bound, just assume might be underflow. Give up narrowing.
      return this;
    }
  }

 private:
  HPhi* const induction_variable_;  // Induction variable for this monotonic value range.
  HInstruction* const initial_;     // Initial value.
  const int32_t increment_;         // Increment for each loop iteration.
  const ValueBound bound_;          // Additional value bound info for initial_.

  DISALLOW_COPY_AND_ASSIGN(MonotonicValueRange);
};

class BCEVisitor : public HGraphVisitor {
 public:
  // The least number of bounds checks that should be eliminated by triggering
  // the deoptimization technique.
  static constexpr size_t kThresholdForAddingDeoptimize = 2;

  // Very large lengths are considered an anomaly. This is a threshold beyond which we don't
  // bother to apply the deoptimization technique since it's likely, or sometimes certain,
  // an AIOOBE will be thrown.
  static constexpr uint32_t kMaxLengthForAddingDeoptimize =
      std::numeric_limits<int32_t>::max() - 1024 * 1024;

  // Added blocks for loop body entry test.
  bool IsAddedBlock(HBasicBlock* block) const {
    return block->GetBlockId() >= initial_block_size_;
  }

  BCEVisitor(HGraph* graph,
             const SideEffectsAnalysis& side_effects,
             HInductionVarAnalysis* induction_analysis)
      : HGraphVisitor(graph),
        allocator_(graph->GetArenaStack()),
        maps_(graph->GetBlocks().size(),
              ScopedArenaSafeMap<int, ValueRange*>(
                  std::less<int>(),
                  allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
              allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
        first_index_bounds_check_map_(std::less<int>(),
                                      allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
        early_exit_loop_(std::less<uint32_t>(),
                         allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
        taken_test_loop_(std::less<uint32_t>(),
                         allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
        finite_loop_(allocator_.Adapter(kArenaAllocBoundsCheckElimination)),
        has_dom_based_dynamic_bce_(false),
        initial_block_size_(graph->GetBlocks().size()),
        side_effects_(side_effects),
        induction_range_(induction_analysis),
        next_(nullptr) {}

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    DCHECK(!IsAddedBlock(block));
    first_index_bounds_check_map_.clear();
    // Visit phis and instructions using a safe iterator. The iteration protects
    // against deleting the current instruction during iteration. However, it
    // must advance next_ if that instruction is deleted during iteration.
    for (HInstruction* instruction = block->GetFirstPhi(); instruction != nullptr;) {
      DCHECK(instruction->IsInBlock());
      next_ = instruction->GetNext();
      instruction->Accept(this);
      instruction = next_;
    }
    for (HInstruction* instruction = block->GetFirstInstruction(); instruction != nullptr;) {
      DCHECK(instruction->IsInBlock());
      next_ = instruction->GetNext();
      instruction->Accept(this);
      instruction = next_;
    }
    // We should never deoptimize from an osr method, otherwise we might wrongly optimize
    // code dominated by the deoptimization.
    if (!GetGraph()->IsCompilingOsr()) {
      AddComparesWithDeoptimization(block);
    }
  }

  void Finish() {
    // Preserve SSA structure which may have been broken by adding one or more
    // new taken-test structures (see TransformLoopForDeoptimizationIfNeeded()).
    InsertPhiNodes();

    // Clear the loop data structures.
    early_exit_loop_.clear();
    taken_test_loop_.clear();
    finite_loop_.clear();
  }

 private:
  // Return the map of proven value ranges at the beginning of a basic block.
  ScopedArenaSafeMap<int, ValueRange*>* GetValueRangeMap(HBasicBlock* basic_block) {
    if (IsAddedBlock(basic_block)) {
      // Added blocks don't keep value ranges.
      return nullptr;
    }
    return &maps_[basic_block->GetBlockId()];
  }

  // Traverse up the dominator tree to look for value range info.
  ValueRange* LookupValueRange(HInstruction* instruction, HBasicBlock* basic_block) {
    while (basic_block != nullptr) {
      ScopedArenaSafeMap<int, ValueRange*>* map = GetValueRangeMap(basic_block);
      if (map != nullptr) {
        if (map->find(instruction->GetId()) != map->end()) {
          return map->Get(instruction->GetId());
        }
      } else {
        DCHECK(IsAddedBlock(basic_block));
      }
      basic_block = basic_block->GetDominator();
    }
    // Didn't find any.
    return nullptr;
  }

  // Helper method to assign a new range to an instruction in given basic block.
  void AssignRange(HBasicBlock* basic_block, HInstruction* instruction, ValueRange* range) {
    DCHECK(!range->IsMonotonicValueRange() || instruction->IsLoopHeaderPhi());
    GetValueRangeMap(basic_block)->Overwrite(instruction->GetId(), range);
  }

  // Narrow the value range of `instruction` at the end of `basic_block` with `range`,
  // and push the narrowed value range to `successor`.
  void ApplyRangeFromComparison(HInstruction* instruction, HBasicBlock* basic_block,
                                HBasicBlock* successor, ValueRange* range) {
    ValueRange* existing_range = LookupValueRange(instruction, basic_block);
    if (existing_range == nullptr) {
      if (range != nullptr) {
        AssignRange(successor, instruction, range);
      }
      return;
    }
    if (existing_range->IsMonotonicValueRange()) {
      DCHECK(instruction->IsLoopHeaderPhi());
      // Make sure the comparison is in the loop header so each increment is
      // checked with a comparison.
      if (instruction->GetBlock() != basic_block) {
        return;
      }
    }
    AssignRange(successor, instruction, existing_range->Narrow(range));
  }

  // Special case that we may simultaneously narrow two MonotonicValueRange's to
  // regular value ranges.
  void HandleIfBetweenTwoMonotonicValueRanges(HIf* instruction,
                                              HInstruction* left,
                                              HInstruction* right,
                                              IfCondition cond,
                                              MonotonicValueRange* left_range,
                                              MonotonicValueRange* right_range) {
    DCHECK(left->IsLoopHeaderPhi());
    DCHECK(right->IsLoopHeaderPhi());
    if (instruction->GetBlock() != left->GetBlock()) {
      // Comparison needs to be in loop header to make sure it's done after each
      // increment/decrement.
      return;
    }

    // Handle common cases which also don't have overflow/underflow concerns.
    if (left_range->GetIncrement() == 1 &&
        left_range->GetBound().IsConstant() &&
        right_range->GetIncrement() == -1 &&
        right_range->GetBound().IsRelatedToArrayLength() &&
        right_range->GetBound().GetConstant() < 0) {
      HBasicBlock* successor = nullptr;
      int32_t left_compensation = 0;
      int32_t right_compensation = 0;
      if (cond == kCondLT) {
        left_compensation = -1;
        right_compensation = 1;
        successor = instruction->IfTrueSuccessor();
      } else if (cond == kCondLE) {
        successor = instruction->IfTrueSuccessor();
      } else if (cond == kCondGT) {
        successor = instruction->IfFalseSuccessor();
      } else if (cond == kCondGE) {
        left_compensation = -1;
        right_compensation = 1;
        successor = instruction->IfFalseSuccessor();
      } else {
        // We don't handle '=='/'!=' test in case left and right can cross and
        // miss each other.
        return;
      }

      if (successor != nullptr) {
        bool overflow;
        bool underflow;
        ValueRange* new_left_range = new (&allocator_) ValueRange(
            &allocator_,
            left_range->GetBound(),
            right_range->GetBound().Add(left_compensation, &overflow, &underflow));
        if (!overflow && !underflow) {
          ApplyRangeFromComparison(left, instruction->GetBlock(), successor,
                                   new_left_range);
        }

        ValueRange* new_right_range = new (&allocator_) ValueRange(
            &allocator_,
            left_range->GetBound().Add(right_compensation, &overflow, &underflow),
            right_range->GetBound());
        if (!overflow && !underflow) {
          ApplyRangeFromComparison(right, instruction->GetBlock(), successor,
                                   new_right_range);
        }
      }
    }
  }

  // Handle "if (left cmp_cond right)".
  void HandleIf(HIf* instruction, HInstruction* left, HInstruction* right, IfCondition cond) {
    HBasicBlock* block = instruction->GetBlock();

    HBasicBlock* true_successor = instruction->IfTrueSuccessor();
    // There should be no critical edge at this point.
    DCHECK_EQ(true_successor->GetPredecessors().size(), 1u);

    HBasicBlock* false_successor = instruction->IfFalseSuccessor();
    // There should be no critical edge at this point.
    DCHECK_EQ(false_successor->GetPredecessors().size(), 1u);

    ValueRange* left_range = LookupValueRange(left, block);
    MonotonicValueRange* left_monotonic_range = nullptr;
    if (left_range != nullptr) {
      left_monotonic_range = left_range->AsMonotonicValueRange();
      if (left_monotonic_range != nullptr) {
        HBasicBlock* loop_head = left_monotonic_range->GetLoopHeader();
        if (instruction->GetBlock() != loop_head) {
          // For monotonic value range, don't handle `instruction`
          // if it's not defined in the loop header.
          return;
        }
      }
    }

    bool found;
    ValueBound bound = ValueBound::DetectValueBoundFromValue(right, &found);
    // Each comparison can establish a lower bound and an upper bound
    // for the left hand side.
    ValueBound lower = bound;
    ValueBound upper = bound;
    if (!found) {
      // No constant or array.length+c format bound found.
      // For i<j, we can still use j's upper bound as i's upper bound. Same for lower.
      ValueRange* right_range = LookupValueRange(right, block);
      if (right_range != nullptr) {
        if (right_range->IsMonotonicValueRange()) {
          if (left_range != nullptr && left_range->IsMonotonicValueRange()) {
            HandleIfBetweenTwoMonotonicValueRanges(instruction, left, right, cond,
                                                   left_range->AsMonotonicValueRange(),
                                                   right_range->AsMonotonicValueRange());
            return;
          }
        }
        lower = right_range->GetLower();
        upper = right_range->GetUpper();
      } else {
        lower = ValueBound::Min();
        upper = ValueBound::Max();
      }
    }

    bool overflow, underflow;
    if (cond == kCondLT || cond == kCondLE) {
      if (!upper.Equals(ValueBound::Max())) {
        int32_t compensation = (cond == kCondLT) ? -1 : 0;  // upper bound is inclusive
        ValueBound new_upper = upper.Add(compensation, &overflow, &underflow);
        if (overflow || underflow) {
          return;
        }
        ValueRange* new_range = new (&allocator_) ValueRange(
            &allocator_, ValueBound::Min(), new_upper);
        ApplyRangeFromComparison(left, block, true_successor, new_range);
      }

      // array.length as a lower bound isn't considered useful.
      if (!lower.Equals(ValueBound::Min()) && !lower.IsRelatedToArrayLength()) {
        int32_t compensation = (cond == kCondLE) ? 1 : 0;  // lower bound is inclusive
        ValueBound new_lower = lower.Add(compensation, &overflow, &underflow);
        if (overflow || underflow) {
          return;
        }
        ValueRange* new_range = new (&allocator_) ValueRange(
            &allocator_, new_lower, ValueBound::Max());
        ApplyRangeFromComparison(left, block, false_successor, new_range);
      }
    } else if (cond == kCondGT || cond == kCondGE) {
      // array.length as a lower bound isn't considered useful.
      if (!lower.Equals(ValueBound::Min()) && !lower.IsRelatedToArrayLength()) {
        int32_t compensation = (cond == kCondGT) ? 1 : 0;  // lower bound is inclusive
        ValueBound new_lower = lower.Add(compensation, &overflow, &underflow);
        if (overflow || underflow) {
          return;
        }
        ValueRange* new_range = new (&allocator_) ValueRange(
            &allocator_, new_lower, ValueBound::Max());
        ApplyRangeFromComparison(left, block, true_successor, new_range);
      }

      if (!upper.Equals(ValueBound::Max())) {
        int32_t compensation = (cond == kCondGE) ? -1 : 0;  // upper bound is inclusive
        ValueBound new_upper = upper.Add(compensation, &overflow, &underflow);
        if (overflow || underflow) {
          return;
        }
        ValueRange* new_range = new (&allocator_) ValueRange(
            &allocator_, ValueBound::Min(), new_upper);
        ApplyRangeFromComparison(left, block, false_successor, new_range);
      }
    } else if (cond == kCondNE || cond == kCondEQ) {
      if (left->IsArrayLength()) {
        if (lower.IsConstant() && upper.IsConstant()) {
          // Special case:
          //   length == [c,d] yields [c, d] along true
          //   length != [c,d] yields [c, d] along false
          if (!lower.Equals(ValueBound::Min()) || !upper.Equals(ValueBound::Max())) {
            ValueRange* new_range = new (&allocator_) ValueRange(&allocator_, lower, upper);
            ApplyRangeFromComparison(
                left, block, cond == kCondEQ ? true_successor : false_successor, new_range);
          }
          // In addition:
          //   length == 0 yields [1, max] along false
          //   length != 0 yields [1, max] along true
          if (lower.GetConstant() == 0 && upper.GetConstant() == 0) {
            ValueRange* new_range = new (&allocator_) ValueRange(
                &allocator_, ValueBound(nullptr, 1), ValueBound::Max());
            ApplyRangeFromComparison(
                left, block, cond == kCondEQ ? false_successor : true_successor, new_range);
          }
        }
      } else if (lower.IsRelatedToArrayLength() && lower.Equals(upper)) {
        // Special aliasing case, with x not array length itself:
        //   x == [length,length] yields x == length along true
        //   x != [length,length] yields x == length along false
        ValueRange* new_range = new (&allocator_) ValueRange(&allocator_, lower, upper);
        ApplyRangeFromComparison(
            left, block, cond == kCondEQ ? true_successor : false_successor, new_range);
      }
    }
  }

  void VisitBoundsCheck(HBoundsCheck* bounds_check) OVERRIDE {
    HBasicBlock* block = bounds_check->GetBlock();
    HInstruction* index = bounds_check->InputAt(0);
    HInstruction* array_length = bounds_check->InputAt(1);
    DCHECK(array_length->IsIntConstant() ||
           array_length->IsArrayLength() ||
           array_length->IsPhi());
    bool try_dynamic_bce = true;
    // Analyze index range.
    if (!index->IsIntConstant()) {
      // Non-constant index.
      ValueBound lower = ValueBound(nullptr, 0);        // constant 0
      ValueBound upper = ValueBound(array_length, -1);  // array_length - 1
      ValueRange array_range(&allocator_, lower, upper);
      // Try index range obtained by dominator-based analysis.
      ValueRange* index_range = LookupValueRange(index, block);
      if (index_range != nullptr) {
        if (index_range->FitsIn(&array_range)) {
          ReplaceInstruction(bounds_check, index);
          return;
        } else if (index_range->IsConstantValueRange()) {
          // If the non-constant index turns out to have a constant range,
          // make one more attempt to get a constant in the array range.
          ValueRange* existing_range = LookupValueRange(array_length, block);
          if (existing_range != nullptr &&
              existing_range->IsConstantValueRange()) {
            ValueRange constant_array_range(&allocator_, lower, existing_range->GetLower());
            if (index_range->FitsIn(&constant_array_range)) {
              ReplaceInstruction(bounds_check, index);
              return;
            }
          }
        }
      }
      // Try index range obtained by induction variable analysis.
      // Disables dynamic bce if OOB is certain.
      if (InductionRangeFitsIn(&array_range, bounds_check, &try_dynamic_bce)) {
        ReplaceInstruction(bounds_check, index);
        return;
      }
    } else {
      // Constant index.
      int32_t constant = index->AsIntConstant()->GetValue();
      if (constant < 0) {
        // Will always throw exception.
        return;
      } else if (array_length->IsIntConstant()) {
        if (constant < array_length->AsIntConstant()->GetValue()) {
          ReplaceInstruction(bounds_check, index);
        }
        return;
      }
      // Analyze array length range.
      DCHECK(array_length->IsArrayLength());
      ValueRange* existing_range = LookupValueRange(array_length, block);
      if (existing_range != nullptr) {
        ValueBound lower = existing_range->GetLower();
        DCHECK(lower.IsConstant());
        if (constant < lower.GetConstant()) {
          ReplaceInstruction(bounds_check, index);
          return;
        } else {
          // Existing range isn't strong enough to eliminate the bounds check.
          // Fall through to update the array_length range with info from this
          // bounds check.
        }
      }
      // Once we have an array access like 'array[5] = 1', we record array.length >= 6.
      // We currently don't do it for non-constant index since a valid array[i] can't prove
      // a valid array[i-1] yet due to the lower bound side.
      if (constant == std::numeric_limits<int32_t>::max()) {
        // Max() as an index will definitely throw AIOOBE.
        return;
      } else {
        ValueBound lower = ValueBound(nullptr, constant + 1);
        ValueBound upper = ValueBound::Max();
        ValueRange* range = new (&allocator_) ValueRange(&allocator_, lower, upper);
        AssignRange(block, array_length, range);
      }
    }

    // If static analysis fails, and OOB is not certain, try dynamic elimination.
    if (try_dynamic_bce) {
      // Try loop-based dynamic elimination.
      HLoopInformation* loop = bounds_check->GetBlock()->GetLoopInformation();
      bool needs_finite_test = false;
      bool needs_taken_test = false;
      if (DynamicBCESeemsProfitable(loop, bounds_check->GetBlock()) &&
          induction_range_.CanGenerateRange(
              bounds_check, index, &needs_finite_test, &needs_taken_test) &&
          CanHandleInfiniteLoop(loop, index, needs_finite_test) &&
          // Do this test last, since it may generate code.
          CanHandleLength(loop, array_length, needs_taken_test)) {
        TransformLoopForDeoptimizationIfNeeded(loop, needs_taken_test);
        TransformLoopForDynamicBCE(loop, bounds_check);
        return;
      }
      // Otherwise, prepare dominator-based dynamic elimination.
      if (first_index_bounds_check_map_.find(array_length->GetId()) ==
          first_index_bounds_check_map_.end()) {
        // Remember the first bounds check against each array_length. That bounds check
        // instruction has an associated HEnvironment where we may add an HDeoptimize
        // to eliminate subsequent bounds checks against the same array_length.
        first_index_bounds_check_map_.Put(array_length->GetId(), bounds_check);
      }
    }
  }

  static bool HasSameInputAtBackEdges(HPhi* phi) {
    DCHECK(phi->IsLoopHeaderPhi());
    HConstInputsRef inputs = phi->GetInputs();
    // Start with input 1. Input 0 is from the incoming block.
    const HInstruction* input1 = inputs[1];
    DCHECK(phi->GetBlock()->GetLoopInformation()->IsBackEdge(
        *phi->GetBlock()->GetPredecessors()[1]));
    for (size_t i = 2; i < inputs.size(); ++i) {
      DCHECK(phi->GetBlock()->GetLoopInformation()->IsBackEdge(
          *phi->GetBlock()->GetPredecessors()[i]));
      if (input1 != inputs[i]) {
        return false;
      }
    }
    return true;
  }

  void VisitPhi(HPhi* phi) OVERRIDE {
    if (phi->IsLoopHeaderPhi()
        && (phi->GetType() == DataType::Type::kInt32)
        && HasSameInputAtBackEdges(phi)) {
      HInstruction* instruction = phi->InputAt(1);
      HInstruction *left;
      int32_t increment;
      if (ValueBound::IsAddOrSubAConstant(instruction, &left, &increment)) {
        if (left == phi) {
          HInstruction* initial_value = phi->InputAt(0);
          ValueRange* range = nullptr;
          if (increment == 0) {
            // Add constant 0. It's really a fixed value.
            range = new (&allocator_) ValueRange(
                &allocator_,
                ValueBound(initial_value, 0),
                ValueBound(initial_value, 0));
          } else {
            // Monotonically increasing/decreasing.
            bool found;
            ValueBound bound = ValueBound::DetectValueBoundFromValue(
                initial_value, &found);
            if (!found) {
              // No constant or array.length+c bound found.
              // For i=j, we can still use j's upper bound as i's upper bound.
              // Same for lower.
              ValueRange* initial_range = LookupValueRange(initial_value, phi->GetBlock());
              if (initial_range != nullptr) {
                bound = increment > 0 ? initial_range->GetLower() :
                                        initial_range->GetUpper();
              } else {
                bound = increment > 0 ? ValueBound::Min() : ValueBound::Max();
              }
            }
            range = new (&allocator_) MonotonicValueRange(
                &allocator_,
                phi,
                initial_value,
                increment,
                bound);
          }
          AssignRange(phi->GetBlock(), phi, range);
        }
      }
    }
  }

  void VisitIf(HIf* instruction) OVERRIDE {
    if (instruction->InputAt(0)->IsCondition()) {
      HCondition* cond = instruction->InputAt(0)->AsCondition();
      HandleIf(instruction, cond->GetLeft(), cond->GetRight(), cond->GetCondition());
    }
  }

  void VisitAdd(HAdd* add) OVERRIDE {
    HInstruction* right = add->GetRight();
    if (right->IsIntConstant()) {
      ValueRange* left_range = LookupValueRange(add->GetLeft(), add->GetBlock());
      if (left_range == nullptr) {
        return;
      }
      ValueRange* range = left_range->Add(right->AsIntConstant()->GetValue());
      if (range != nullptr) {
        AssignRange(add->GetBlock(), add, range);
      }
    }
  }

  void VisitSub(HSub* sub) OVERRIDE {
    HInstruction* left = sub->GetLeft();
    HInstruction* right = sub->GetRight();
    if (right->IsIntConstant()) {
      ValueRange* left_range = LookupValueRange(left, sub->GetBlock());
      if (left_range == nullptr) {
        return;
      }
      ValueRange* range = left_range->Add(-right->AsIntConstant()->GetValue());
      if (range != nullptr) {
        AssignRange(sub->GetBlock(), sub, range);
        return;
      }
    }

    // Here we are interested in the typical triangular case of nested loops,
    // such as the inner loop 'for (int j=0; j<array.length-i; j++)' where i
    // is the index for outer loop. In this case, we know j is bounded by array.length-1.

    // Try to handle (array.length - i) or (array.length + c - i) format.
    HInstruction* left_of_left;  // left input of left.
    int32_t right_const = 0;
    if (ValueBound::IsAddOrSubAConstant(left, &left_of_left, &right_const)) {
      left = left_of_left;
    }
    // The value of left input of the sub equals (left + right_const).

    if (left->IsArrayLength()) {
      HInstruction* array_length = left->AsArrayLength();
      ValueRange* right_range = LookupValueRange(right, sub->GetBlock());
      if (right_range != nullptr) {
        ValueBound lower = right_range->GetLower();
        ValueBound upper = right_range->GetUpper();
        if (lower.IsConstant() && upper.IsRelatedToArrayLength()) {
          HInstruction* upper_inst = upper.GetInstruction();
          // Make sure it's the same array.
          if (ValueBound::Equal(array_length, upper_inst)) {
            int32_t c0 = right_const;
            int32_t c1 = lower.GetConstant();
            int32_t c2 = upper.GetConstant();
            // (array.length + c0 - v) where v is in [c1, array.length + c2]
            // gets [c0 - c2, array.length + c0 - c1] as its value range.
            if (!ValueBound::WouldAddOverflowOrUnderflow(c0, -c2) &&
                !ValueBound::WouldAddOverflowOrUnderflow(c0, -c1)) {
              if ((c0 - c1) <= 0) {
                // array.length + (c0 - c1) won't overflow/underflow.
                ValueRange* range = new (&allocator_) ValueRange(
                    &allocator_,
                    ValueBound(nullptr, right_const - upper.GetConstant()),
                    ValueBound(array_length, right_const - lower.GetConstant()));
                AssignRange(sub->GetBlock(), sub, range);
              }
            }
          }
        }
      }
    }
  }

  void FindAndHandlePartialArrayLength(HBinaryOperation* instruction) {
    DCHECK(instruction->IsDiv() || instruction->IsShr() || instruction->IsUShr());
    HInstruction* right = instruction->GetRight();
    int32_t right_const;
    if (right->IsIntConstant()) {
      right_const = right->AsIntConstant()->GetValue();
      // Detect division by two or more.
      if ((instruction->IsDiv() && right_const <= 1) ||
          (instruction->IsShr() && right_const < 1) ||
          (instruction->IsUShr() && right_const < 1)) {
        return;
      }
    } else {
      return;
    }

    // Try to handle array.length/2 or (array.length-1)/2 format.
    HInstruction* left = instruction->GetLeft();
    HInstruction* left_of_left;  // left input of left.
    int32_t c = 0;
    if (ValueBound::IsAddOrSubAConstant(left, &left_of_left, &c)) {
      left = left_of_left;
    }
    // The value of left input of instruction equals (left + c).

    // (array_length + 1) or smaller divided by two or more
    // always generate a value in [Min(), array_length].
    // This is true even if array_length is Max().
    if (left->IsArrayLength() && c <= 1) {
      if (instruction->IsUShr() && c < 0) {
        // Make sure for unsigned shift, left side is not negative.
        // e.g. if array_length is 2, ((array_length - 3) >>> 2) is way bigger
        // than array_length.
        return;
      }
      ValueRange* range = new (&allocator_) ValueRange(
          &allocator_,
          ValueBound(nullptr, std::numeric_limits<int32_t>::min()),
          ValueBound(left, 0));
      AssignRange(instruction->GetBlock(), instruction, range);
    }
  }

  void VisitDiv(HDiv* div) OVERRIDE {
    FindAndHandlePartialArrayLength(div);
  }

  void VisitShr(HShr* shr) OVERRIDE {
    FindAndHandlePartialArrayLength(shr);
  }

  void VisitUShr(HUShr* ushr) OVERRIDE {
    FindAndHandlePartialArrayLength(ushr);
  }

  void VisitAnd(HAnd* instruction) OVERRIDE {
    if (instruction->GetRight()->IsIntConstant()) {
      int32_t constant = instruction->GetRight()->AsIntConstant()->GetValue();
      if (constant > 0) {
        // constant serves as a mask so any number masked with it
        // gets a [0, constant] value range.
        ValueRange* range = new (&allocator_) ValueRange(
            &allocator_,
            ValueBound(nullptr, 0),
            ValueBound(nullptr, constant));
        AssignRange(instruction->GetBlock(), instruction, range);
      }
    }
  }

  void VisitRem(HRem* instruction) OVERRIDE {
    HInstruction* left = instruction->GetLeft();
    HInstruction* right = instruction->GetRight();

    // Handle 'i % CONST' format expression in array index, e.g:
    //   array[i % 20];
    if (right->IsIntConstant()) {
      int32_t right_const = std::abs(right->AsIntConstant()->GetValue());
      if (right_const == 0) {
        return;
      }
      // The sign of divisor CONST doesn't affect the sign final value range.
      // For example:
      // if (i > 0) {
      //   array[i % 10];  // index value range [0, 9]
      //   array[i % -10]; // index value range [0, 9]
      // }
      ValueRange* right_range = new (&allocator_) ValueRange(
          &allocator_,
          ValueBound(nullptr, 1 - right_const),
          ValueBound(nullptr, right_const - 1));

      ValueRange* left_range = LookupValueRange(left, instruction->GetBlock());
      if (left_range != nullptr) {
        right_range = right_range->Narrow(left_range);
      }
      AssignRange(instruction->GetBlock(), instruction, right_range);
      return;
    }

    // Handle following pattern:
    // i0 NullCheck
    // i1 ArrayLength[i0]
    // i2 DivByZeroCheck [i1]  <-- right
    // i3 Rem [i5, i2]         <-- we are here.
    // i4 BoundsCheck [i3,i1]
    if (right->IsDivZeroCheck()) {
      // if array_length can pass div-by-zero check,
      // array_length must be > 0.
      right = right->AsDivZeroCheck()->InputAt(0);
    }

    // Handle 'i % array.length' format expression in array index, e.g:
    //   array[(i+7) % array.length];
    if (right->IsArrayLength()) {
      ValueBound lower = ValueBound::Min();  // ideally, lower should be '1-array_length'.
      ValueBound upper = ValueBound(right, -1);  // array_length - 1
      ValueRange* right_range = new (&allocator_) ValueRange(
          &allocator_,
          lower,
          upper);
      ValueRange* left_range = LookupValueRange(left, instruction->GetBlock());
      if (left_range != nullptr) {
        right_range = right_range->Narrow(left_range);
      }
      AssignRange(instruction->GetBlock(), instruction, right_range);
      return;
    }
  }

  void VisitNewArray(HNewArray* new_array) OVERRIDE {
    HInstruction* len = new_array->GetLength();
    if (!len->IsIntConstant()) {
      HInstruction *left;
      int32_t right_const;
      if (ValueBound::IsAddOrSubAConstant(len, &left, &right_const)) {
        // (left + right_const) is used as size to new the array.
        // We record "-right_const <= left <= new_array - right_const";
        ValueBound lower = ValueBound(nullptr, -right_const);
        // We use new_array for the bound instead of new_array.length,
        // which isn't available as an instruction yet. new_array will
        // be treated the same as new_array.length when it's used in a ValueBound.
        ValueBound upper = ValueBound(new_array, -right_const);
        ValueRange* range = new (&allocator_) ValueRange(&allocator_, lower, upper);
        ValueRange* existing_range = LookupValueRange(left, new_array->GetBlock());
        if (existing_range != nullptr) {
          range = existing_range->Narrow(range);
        }
        AssignRange(new_array->GetBlock(), left, range);
      }
    }
  }

  /**
    * After null/bounds checks are eliminated, some invariant array references
    * may be exposed underneath which can be hoisted out of the loop to the
    * preheader or, in combination with dynamic bce, the deoptimization block.
    *
    * for (int i = 0; i < n; i++) {
    *                                <-------+
    *   for (int j = 0; j < n; j++)          |
    *     a[i][j] = 0;               --a[i]--+
    * }
    *
    * Note: this optimization is no longer applied after dominator-based dynamic deoptimization
    * has occurred (see AddCompareWithDeoptimization()), since in those cases it would be
    * unsafe to hoist array references across their deoptimization instruction inside a loop.
    */
  void VisitArrayGet(HArrayGet* array_get) OVERRIDE {
    if (!has_dom_based_dynamic_bce_ && array_get->IsInLoop()) {
      HLoopInformation* loop = array_get->GetBlock()->GetLoopInformation();
      if (loop->IsDefinedOutOfTheLoop(array_get->InputAt(0)) &&
          loop->IsDefinedOutOfTheLoop(array_get->InputAt(1))) {
        SideEffects loop_effects = side_effects_.GetLoopEffects(loop->GetHeader());
        if (!array_get->GetSideEffects().MayDependOn(loop_effects)) {
          // We can hoist ArrayGet only if its execution is guaranteed on every iteration.
          // In other words only if array_get_bb dominates all back branches.
          if (loop->DominatesAllBackEdges(array_get->GetBlock())) {
            HoistToPreHeaderOrDeoptBlock(loop, array_get);
          }
        }
      }
    }
  }

  /** Performs dominator-based dynamic elimination on suitable set of bounds checks. */
  void AddCompareWithDeoptimization(HBasicBlock* block,
                                    HInstruction* array_length,
                                    HInstruction* base,
                                    int32_t min_c, int32_t max_c) {
    HBoundsCheck* bounds_check =
        first_index_bounds_check_map_.Get(array_length->GetId())->AsBoundsCheck();
    // Construct deoptimization on single or double bounds on range [base-min_c,base+max_c],
    // for example either for a[0]..a[3] just 3 or for a[base-1]..a[base+3] both base-1
    // and base+3, since we made the assumption any in between value may occur too.
    // In code, using unsigned comparisons:
    // (1) constants only
    //       if (max_c >= a.length) deoptimize;
    // (2) general case
    //       if (base-min_c >  base+max_c) deoptimize;
    //       if (base+max_c >= a.length  ) deoptimize;
    static_assert(kMaxLengthForAddingDeoptimize < std::numeric_limits<int32_t>::max(),
                  "Incorrect max length may be subject to arithmetic wrap-around");
    HInstruction* upper = GetGraph()->GetIntConstant(max_c);
    if (base == nullptr) {
      DCHECK_GE(min_c, 0);
    } else {
      HInstruction* lower = new (GetGraph()->GetAllocator())
          HAdd(DataType::Type::kInt32, base, GetGraph()->GetIntConstant(min_c));
      upper = new (GetGraph()->GetAllocator()) HAdd(DataType::Type::kInt32, base, upper);
      block->InsertInstructionBefore(lower, bounds_check);
      block->InsertInstructionBefore(upper, bounds_check);
      InsertDeoptInBlock(bounds_check, new (GetGraph()->GetAllocator()) HAbove(lower, upper));
    }
    InsertDeoptInBlock(
        bounds_check, new (GetGraph()->GetAllocator()) HAboveOrEqual(upper, array_length));
    // Flag that this kind of deoptimization has occurred.
    has_dom_based_dynamic_bce_ = true;
  }

  /** Attempts dominator-based dynamic elimination on remaining candidates. */
  void AddComparesWithDeoptimization(HBasicBlock* block) {
    for (const auto& entry : first_index_bounds_check_map_) {
      HBoundsCheck* bounds_check = entry.second;
      HInstruction* index = bounds_check->InputAt(0);
      HInstruction* array_length = bounds_check->InputAt(1);
      if (!array_length->IsArrayLength()) {
        continue;  // disregard phis and constants
      }
      // Collect all bounds checks that are still there and that are related as "a[base + constant]"
      // for a base instruction (possibly absent) and various constants. Note that no attempt
      // is made to partition the set into matching subsets (viz. a[0], a[1] and a[base+1] and
      // a[base+2] are considered as one set).
      // TODO: would such a partitioning be worthwhile?
      ValueBound value = ValueBound::AsValueBound(index);
      HInstruction* base = value.GetInstruction();
      int32_t min_c = base == nullptr ? 0 : value.GetConstant();
      int32_t max_c = value.GetConstant();
      ScopedArenaVector<HBoundsCheck*> candidates(
          allocator_.Adapter(kArenaAllocBoundsCheckElimination));
      ScopedArenaVector<HBoundsCheck*> standby(
          allocator_.Adapter(kArenaAllocBoundsCheckElimination));
      for (const HUseListNode<HInstruction*>& use : array_length->GetUses()) {
        // Another bounds check in same or dominated block?
        HInstruction* user = use.GetUser();
        HBasicBlock* other_block = user->GetBlock();
        if (user->IsBoundsCheck() && block->Dominates(other_block)) {
          HBoundsCheck* other_bounds_check = user->AsBoundsCheck();
          HInstruction* other_index = other_bounds_check->InputAt(0);
          HInstruction* other_array_length = other_bounds_check->InputAt(1);
          ValueBound other_value = ValueBound::AsValueBound(other_index);
          if (array_length == other_array_length && base == other_value.GetInstruction()) {
            // Reject certain OOB if BoundsCheck(l, l) occurs on considered subset.
            if (array_length == other_index) {
              candidates.clear();
              standby.clear();
              break;
            }
            // Since a subsequent dominated block could be under a conditional, only accept
            // the other bounds check if it is in same block or both blocks dominate the exit.
            // TODO: we could improve this by testing proper post-dominance, or even if this
            //       constant is seen along *all* conditional paths that follow.
            HBasicBlock* exit = GetGraph()->GetExitBlock();
            if (block == user->GetBlock() ||
                (block->Dominates(exit) && other_block->Dominates(exit))) {
              int32_t other_c = other_value.GetConstant();
              min_c = std::min(min_c, other_c);
              max_c = std::max(max_c, other_c);
              candidates.push_back(other_bounds_check);
            } else {
              // Add this candidate later only if it falls into the range.
              standby.push_back(other_bounds_check);
            }
          }
        }
      }
      // Add standby candidates that fall in selected range.
      for (HBoundsCheck* other_bounds_check : standby) {
        HInstruction* other_index = other_bounds_check->InputAt(0);
        int32_t other_c = ValueBound::AsValueBound(other_index).GetConstant();
        if (min_c <= other_c && other_c <= max_c) {
          candidates.push_back(other_bounds_check);
        }
      }
      // Perform dominator-based deoptimization if it seems profitable, where we eliminate
      // bounds checks and replace these with deopt checks that guard against any possible
      // OOB. Note that we reject cases where the distance min_c:max_c range gets close to
      // the maximum possible array length, since those cases are likely to always deopt
      // (such situations do not necessarily go OOB, though, since the array could be really
      // large, or the programmer could rely on arithmetic wrap-around from max to min).
      size_t threshold = kThresholdForAddingDeoptimize + (base == nullptr ? 0 : 1);  // extra test?
      uint32_t distance = static_cast<uint32_t>(max_c) - static_cast<uint32_t>(min_c);
      if (candidates.size() >= threshold &&
          (base != nullptr || min_c >= 0) &&  // reject certain OOB
           distance <= kMaxLengthForAddingDeoptimize) {  // reject likely/certain deopt
        AddCompareWithDeoptimization(block, array_length, base, min_c, max_c);
        for (HBoundsCheck* other_bounds_check : candidates) {
          // Only replace if still in the graph. This avoids visiting the same
          // bounds check twice if it occurred multiple times in the use list.
          if (other_bounds_check->IsInBlock()) {
            ReplaceInstruction(other_bounds_check, other_bounds_check->InputAt(0));
          }
        }
      }
    }
  }

  /**
   * Returns true if static range analysis based on induction variables can determine the bounds
   * check on the given array range is always satisfied with the computed index range. The output
   * parameter try_dynamic_bce is set to false if OOB is certain.
   */
  bool InductionRangeFitsIn(ValueRange* array_range,
                            HBoundsCheck* context,
                            bool* try_dynamic_bce) {
    InductionVarRange::Value v1;
    InductionVarRange::Value v2;
    bool needs_finite_test = false;
    HInstruction* index = context->InputAt(0);
    HInstruction* hint = HuntForDeclaration(context->InputAt(1));
    if (induction_range_.GetInductionRange(context, index, hint, &v1, &v2, &needs_finite_test)) {
      if (v1.is_known && (v1.a_constant == 0 || v1.a_constant == 1) &&
          v2.is_known && (v2.a_constant == 0 || v2.a_constant == 1)) {
        DCHECK(v1.a_constant == 1 || v1.instruction == nullptr);
        DCHECK(v2.a_constant == 1 || v2.instruction == nullptr);
        ValueRange index_range(&allocator_,
                               ValueBound(v1.instruction, v1.b_constant),
                               ValueBound(v2.instruction, v2.b_constant));
        // If analysis reveals a certain OOB, disable dynamic BCE. Otherwise,
        // use analysis for static bce only if loop is finite.
        if (index_range.GetLower().LessThan(array_range->GetLower()) ||
            index_range.GetUpper().GreaterThan(array_range->GetUpper())) {
          *try_dynamic_bce = false;
        } else if (!needs_finite_test && index_range.FitsIn(array_range)) {
          return true;
        }
      }
    }
    return false;
  }

  /**
   * Performs loop-based dynamic elimination on a bounds check. In order to minimize the
   * number of eventually generated tests, related bounds checks with tests that can be
   * combined with tests for the given bounds check are collected first.
   */
  void TransformLoopForDynamicBCE(HLoopInformation* loop, HBoundsCheck* bounds_check) {
    HInstruction* index = bounds_check->InputAt(0);
    HInstruction* array_length = bounds_check->InputAt(1);
    DCHECK(loop->IsDefinedOutOfTheLoop(array_length));  // pre-checked
    DCHECK(loop->DominatesAllBackEdges(bounds_check->GetBlock()));
    // Collect all bounds checks in the same loop that are related as "a[base + constant]"
    // for a base instruction (possibly absent) and various constants.
    ValueBound value = ValueBound::AsValueBound(index);
    HInstruction* base = value.GetInstruction();
    int32_t min_c = base == nullptr ? 0 : value.GetConstant();
    int32_t max_c = value.GetConstant();
    ScopedArenaVector<HBoundsCheck*> candidates(
        allocator_.Adapter(kArenaAllocBoundsCheckElimination));
    ScopedArenaVector<HBoundsCheck*> standby(
        allocator_.Adapter(kArenaAllocBoundsCheckElimination));
    for (const HUseListNode<HInstruction*>& use : array_length->GetUses()) {
      HInstruction* user = use.GetUser();
      if (user->IsBoundsCheck() && loop == user->GetBlock()->GetLoopInformation()) {
        HBoundsCheck* other_bounds_check = user->AsBoundsCheck();
        HInstruction* other_index = other_bounds_check->InputAt(0);
        HInstruction* other_array_length = other_bounds_check->InputAt(1);
        ValueBound other_value = ValueBound::AsValueBound(other_index);
        int32_t other_c = other_value.GetConstant();
        if (array_length == other_array_length && base == other_value.GetInstruction()) {
          // Ensure every candidate could be picked for code generation.
          bool b1 = false, b2 = false;
          if (!induction_range_.CanGenerateRange(other_bounds_check, other_index, &b1, &b2)) {
            continue;
          }
          // Does the current basic block dominate all back edges? If not,
          // add this candidate later only if it falls into the range.
          if (!loop->DominatesAllBackEdges(user->GetBlock())) {
            standby.push_back(other_bounds_check);
            continue;
          }
          min_c = std::min(min_c, other_c);
          max_c = std::max(max_c, other_c);
          candidates.push_back(other_bounds_check);
        }
      }
    }
    // Add standby candidates that fall in selected range.
    for (HBoundsCheck* other_bounds_check : standby) {
      HInstruction* other_index = other_bounds_check->InputAt(0);
      int32_t other_c = ValueBound::AsValueBound(other_index).GetConstant();
      if (min_c <= other_c && other_c <= max_c) {
        candidates.push_back(other_bounds_check);
      }
    }
    // Perform loop-based deoptimization if it seems profitable, where we eliminate bounds
    // checks and replace these with deopt checks that guard against any possible OOB.
    DCHECK_LT(0u, candidates.size());
    uint32_t distance = static_cast<uint32_t>(max_c) - static_cast<uint32_t>(min_c);
    if ((base != nullptr || min_c >= 0) &&  // reject certain OOB
        distance <= kMaxLengthForAddingDeoptimize) {  // reject likely/certain deopt
      HBasicBlock* block = GetPreHeader(loop, bounds_check);
      HInstruction* min_lower = nullptr;
      HInstruction* min_upper = nullptr;
      HInstruction* max_lower = nullptr;
      HInstruction* max_upper = nullptr;
      // Iterate over all bounds checks.
      for (HBoundsCheck* other_bounds_check : candidates) {
        // Only handle if still in the graph. This avoids visiting the same
        // bounds check twice if it occurred multiple times in the use list.
        if (other_bounds_check->IsInBlock()) {
          HInstruction* other_index = other_bounds_check->InputAt(0);
          int32_t other_c = ValueBound::AsValueBound(other_index).GetConstant();
          // Generate code for either the maximum or minimum. Range analysis already was queried
          // whether code generation on the original and, thus, related bounds check was possible.
          // It handles either loop invariants (lower is not set) or unit strides.
          if (other_c == max_c) {
            induction_range_.GenerateRange(
                other_bounds_check, other_index, GetGraph(), block, &max_lower, &max_upper);
          } else if (other_c == min_c && base != nullptr) {
            induction_range_.GenerateRange(
                other_bounds_check, other_index, GetGraph(), block, &min_lower, &min_upper);
          }
          ReplaceInstruction(other_bounds_check, other_index);
        }
      }
      // In code, using unsigned comparisons:
      // (1) constants only
      //       if (max_upper >= a.length ) deoptimize;
      // (2) two symbolic invariants
      //       if (min_upper >  max_upper) deoptimize;   unless min_c == max_c
      //       if (max_upper >= a.length ) deoptimize;
      // (3) general case, unit strides (where lower would exceed upper for arithmetic wrap-around)
      //       if (min_lower >  max_lower) deoptimize;   unless min_c == max_c
      //       if (max_lower >  max_upper) deoptimize;
      //       if (max_upper >= a.length ) deoptimize;
      if (base == nullptr) {
        // Constants only.
        DCHECK_GE(min_c, 0);
        DCHECK(min_lower == nullptr && min_upper == nullptr &&
               max_lower == nullptr && max_upper != nullptr);
      } else if (max_lower == nullptr) {
        // Two symbolic invariants.
        if (min_c != max_c) {
          DCHECK(min_lower == nullptr && min_upper != nullptr &&
                 max_lower == nullptr && max_upper != nullptr);
          InsertDeoptInLoop(
              loop, block, new (GetGraph()->GetAllocator()) HAbove(min_upper, max_upper));
        } else {
          DCHECK(min_lower == nullptr && min_upper == nullptr &&
                 max_lower == nullptr && max_upper != nullptr);
        }
      } else {
        // General case, unit strides.
        if (min_c != max_c) {
          DCHECK(min_lower != nullptr && min_upper != nullptr &&
                 max_lower != nullptr && max_upper != nullptr);
          InsertDeoptInLoop(
              loop, block, new (GetGraph()->GetAllocator()) HAbove(min_lower, max_lower));
        } else {
          DCHECK(min_lower == nullptr && min_upper == nullptr &&
                 max_lower != nullptr && max_upper != nullptr);
        }
        InsertDeoptInLoop(
            loop, block, new (GetGraph()->GetAllocator()) HAbove(max_lower, max_upper));
      }
      InsertDeoptInLoop(
          loop, block, new (GetGraph()->GetAllocator()) HAboveOrEqual(max_upper, array_length));
    } else {
      // TODO: if rejected, avoid doing this again for subsequent instructions in this set?
    }
  }

  /**
   * Returns true if heuristics indicate that dynamic bce may be profitable.
   */
  bool DynamicBCESeemsProfitable(HLoopInformation* loop, HBasicBlock* block) {
    if (loop != nullptr) {
      // The loop preheader of an irreducible loop does not dominate all the blocks in
      // the loop. We would need to find the common dominator of all blocks in the loop.
      if (loop->IsIrreducible()) {
        return false;
      }
      // We should never deoptimize from an osr method, otherwise we might wrongly optimize
      // code dominated by the deoptimization.
      if (GetGraph()->IsCompilingOsr()) {
        return false;
      }
      // A try boundary preheader is hard to handle.
      // TODO: remove this restriction.
      if (loop->GetPreHeader()->GetLastInstruction()->IsTryBoundary()) {
        return false;
      }
      // Does loop have early-exits? If so, the full range may not be covered by the loop
      // at runtime and testing the range may apply deoptimization unnecessarily.
      if (IsEarlyExitLoop(loop)) {
        return false;
      }
      // Does the current basic block dominate all back edges? If not,
      // don't apply dynamic bce to something that may not be executed.
      return loop->DominatesAllBackEdges(block);
    }
    return false;
  }

  /**
   * Returns true if the loop has early exits, which implies it may not cover
   * the full range computed by range analysis based on induction variables.
   */
  bool IsEarlyExitLoop(HLoopInformation* loop) {
    const uint32_t loop_id = loop->GetHeader()->GetBlockId();
    // If loop has been analyzed earlier for early-exit, don't repeat the analysis.
    auto it = early_exit_loop_.find(loop_id);
    if (it != early_exit_loop_.end()) {
      return it->second;
    }
    // First time early-exit analysis for this loop. Since analysis requires scanning
    // the full loop-body, results of the analysis is stored for subsequent queries.
    HBlocksInLoopReversePostOrderIterator it_loop(*loop);
    for (it_loop.Advance(); !it_loop.Done(); it_loop.Advance()) {
      for (HBasicBlock* successor : it_loop.Current()->GetSuccessors()) {
        if (!loop->Contains(*successor)) {
          early_exit_loop_.Put(loop_id, true);
          return true;
        }
      }
    }
    early_exit_loop_.Put(loop_id, false);
    return false;
  }

  /**
   * Returns true if the array length is already loop invariant, or can be made so
   * by handling the null check under the hood of the array length operation.
   */
  bool CanHandleLength(HLoopInformation* loop, HInstruction* length, bool needs_taken_test) {
    if (loop->IsDefinedOutOfTheLoop(length)) {
      return true;
    } else if (length->IsArrayLength() && length->GetBlock()->GetLoopInformation() == loop) {
      if (CanHandleNullCheck(loop, length->InputAt(0), needs_taken_test)) {
        HoistToPreHeaderOrDeoptBlock(loop, length);
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if the null check is already loop invariant, or can be made so
   * by generating a deoptimization test.
   */
  bool CanHandleNullCheck(HLoopInformation* loop, HInstruction* check, bool needs_taken_test) {
    if (loop->IsDefinedOutOfTheLoop(check)) {
      return true;
    } else if (check->IsNullCheck() && check->GetBlock()->GetLoopInformation() == loop) {
      HInstruction* array = check->InputAt(0);
      if (loop->IsDefinedOutOfTheLoop(array)) {
        // Generate: if (array == null) deoptimize;
        TransformLoopForDeoptimizationIfNeeded(loop, needs_taken_test);
        HBasicBlock* block = GetPreHeader(loop, check);
        HInstruction* cond =
            new (GetGraph()->GetAllocator()) HEqual(array, GetGraph()->GetNullConstant());
        InsertDeoptInLoop(loop, block, cond, /* is_null_check */ true);
        ReplaceInstruction(check, array);
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if compiler can apply dynamic bce to loops that may be infinite
   * (e.g. for (int i = 0; i <= U; i++) with U = MAX_INT), which would invalidate
   * the range analysis evaluation code by "overshooting" the computed range.
   * Since deoptimization would be a bad choice, and there is no other version
   * of the loop to use, dynamic bce in such cases is only allowed if other tests
   * ensure the loop is finite.
   */
  bool CanHandleInfiniteLoop(HLoopInformation* loop, HInstruction* index, bool needs_infinite_test) {
    if (needs_infinite_test) {
      // If we already forced the loop to be finite, allow directly.
      const uint32_t loop_id = loop->GetHeader()->GetBlockId();
      if (finite_loop_.find(loop_id) != finite_loop_.end()) {
        return true;
      }
      // Otherwise, allow dynamic bce if the index (which is necessarily an induction at
      // this point) is the direct loop index (viz. a[i]), since then the runtime tests
      // ensure upper bound cannot cause an infinite loop.
      HInstruction* control = loop->GetHeader()->GetLastInstruction();
      if (control->IsIf()) {
        HInstruction* if_expr = control->AsIf()->InputAt(0);
        if (if_expr->IsCondition()) {
          HCondition* condition = if_expr->AsCondition();
          if (index == condition->InputAt(0) ||
              index == condition->InputAt(1)) {
            finite_loop_.insert(loop_id);
            return true;
          }
        }
      }
      return false;
    }
    return true;
  }

  /**
   * Returns appropriate preheader for the loop, depending on whether the
   * instruction appears in the loop header or proper loop-body.
   */
  HBasicBlock* GetPreHeader(HLoopInformation* loop, HInstruction* instruction) {
    // Use preheader unless there is an earlier generated deoptimization block since
    // hoisted expressions may depend on and/or used by the deoptimization tests.
    HBasicBlock* header = loop->GetHeader();
    const uint32_t loop_id = header->GetBlockId();
    auto it = taken_test_loop_.find(loop_id);
    if (it != taken_test_loop_.end()) {
      HBasicBlock* block = it->second;
      // If always taken, keep it that way by returning the original preheader,
      // which can be found by following the predecessor of the true-block twice.
      if (instruction->GetBlock() == header) {
        return block->GetSinglePredecessor()->GetSinglePredecessor();
      }
      return block;
    }
    return loop->GetPreHeader();
  }

  /** Inserts a deoptimization test in a loop preheader. */
  void InsertDeoptInLoop(HLoopInformation* loop,
                         HBasicBlock* block,
                         HInstruction* condition,
                         bool is_null_check = false) {
    HInstruction* suspend = loop->GetSuspendCheck();
    block->InsertInstructionBefore(condition, block->GetLastInstruction());
    DeoptimizationKind kind =
        is_null_check ? DeoptimizationKind::kLoopNullBCE : DeoptimizationKind::kLoopBoundsBCE;
    HDeoptimize* deoptimize = new (GetGraph()->GetAllocator()) HDeoptimize(
        GetGraph()->GetAllocator(), condition, kind, suspend->GetDexPc());
    block->InsertInstructionBefore(deoptimize, block->GetLastInstruction());
    if (suspend->HasEnvironment()) {
      deoptimize->CopyEnvironmentFromWithLoopPhiAdjustment(
          suspend->GetEnvironment(), loop->GetHeader());
    }
  }

  /** Inserts a deoptimization test right before a bounds check. */
  void InsertDeoptInBlock(HBoundsCheck* bounds_check, HInstruction* condition) {
    HBasicBlock* block = bounds_check->GetBlock();
    block->InsertInstructionBefore(condition, bounds_check);
    HDeoptimize* deoptimize = new (GetGraph()->GetAllocator()) HDeoptimize(
        GetGraph()->GetAllocator(),
        condition,
        DeoptimizationKind::kBlockBCE,
        bounds_check->GetDexPc());
    block->InsertInstructionBefore(deoptimize, bounds_check);
    deoptimize->CopyEnvironmentFrom(bounds_check->GetEnvironment());
  }

  /** Hoists instruction out of the loop to preheader or deoptimization block. */
  void HoistToPreHeaderOrDeoptBlock(HLoopInformation* loop, HInstruction* instruction) {
    HBasicBlock* block = GetPreHeader(loop, instruction);
    DCHECK(!instruction->HasEnvironment());
    instruction->MoveBefore(block->GetLastInstruction());
  }

  /**
   * Adds a new taken-test structure to a loop if needed and not already done.
   * The taken-test protects range analysis evaluation code to avoid any
   * deoptimization caused by incorrect trip-count evaluation in non-taken loops.
   *
   *          old_preheader
   *               |
   *            if_block          <- taken-test protects deoptimization block
   *            /      \
   *     true_block  false_block  <- deoptimizations/invariants are placed in true_block
   *            \       /
   *          new_preheader       <- may require phi nodes to preserve SSA structure
   *                |
   *             header
   *
   * For example, this loop:
   *
   *   for (int i = lower; i < upper; i++) {
   *     array[i] = 0;
   *   }
   *
   * will be transformed to:
   *
   *   if (lower < upper) {
   *     if (array == null) deoptimize;
   *     array_length = array.length;
   *     if (lower > upper)         deoptimize;  // unsigned
   *     if (upper >= array_length) deoptimize;  // unsigned
   *   } else {
   *     array_length = 0;
   *   }
   *   for (int i = lower; i < upper; i++) {
   *     // Loop without null check and bounds check, and any array.length replaced with array_length.
   *     array[i] = 0;
   *   }
   */
  void TransformLoopForDeoptimizationIfNeeded(HLoopInformation* loop, bool needs_taken_test) {
    // Not needed (can use preheader) or already done (can reuse)?
    const uint32_t loop_id = loop->GetHeader()->GetBlockId();
    if (!needs_taken_test || taken_test_loop_.find(loop_id) != taken_test_loop_.end()) {
      return;
    }

    // Generate top test structure.
    HBasicBlock* header = loop->GetHeader();
    GetGraph()->TransformLoopHeaderForBCE(header);
    HBasicBlock* new_preheader = loop->GetPreHeader();
    HBasicBlock* if_block = new_preheader->GetDominator();
    HBasicBlock* true_block = if_block->GetSuccessors()[0];  // True successor.
    HBasicBlock* false_block = if_block->GetSuccessors()[1];  // False successor.

    // Goto instructions.
    true_block->AddInstruction(new (GetGraph()->GetAllocator()) HGoto());
    false_block->AddInstruction(new (GetGraph()->GetAllocator()) HGoto());
    new_preheader->AddInstruction(new (GetGraph()->GetAllocator()) HGoto());

    // Insert the taken-test to see if the loop body is entered. If the
    // loop isn't entered at all, it jumps around the deoptimization block.
    if_block->AddInstruction(new (GetGraph()->GetAllocator()) HGoto());  // placeholder
    HInstruction* condition = induction_range_.GenerateTakenTest(
        header->GetLastInstruction(), GetGraph(), if_block);
    DCHECK(condition != nullptr);
    if_block->RemoveInstruction(if_block->GetLastInstruction());
    if_block->AddInstruction(new (GetGraph()->GetAllocator()) HIf(condition));

    taken_test_loop_.Put(loop_id, true_block);
  }

  /**
   * Inserts phi nodes that preserve SSA structure in generated top test structures.
   * All uses of instructions in the deoptimization block that reach the loop need
   * a phi node in the new loop preheader to fix the dominance relation.
   *
   * Example:
   *           if_block
   *            /      \
   *         x_0 = ..  false_block
   *            \       /
   *           x_1 = phi(x_0, null)   <- synthetic phi
   *               |
   *          new_preheader
   */
  void InsertPhiNodes() {
    // Scan all new deoptimization blocks.
    for (const auto& entry : taken_test_loop_) {
      HBasicBlock* true_block = entry.second;
      HBasicBlock* new_preheader = true_block->GetSingleSuccessor();
      // Scan all instructions in a new deoptimization block.
      for (HInstructionIterator it(true_block->GetInstructions()); !it.Done(); it.Advance()) {
        HInstruction* instruction = it.Current();
        DataType::Type type = instruction->GetType();
        HPhi* phi = nullptr;
        // Scan all uses of an instruction and replace each later use with a phi node.
        const HUseList<HInstruction*>& uses = instruction->GetUses();
        for (auto it2 = uses.begin(), end2 = uses.end(); it2 != end2; /* ++it2 below */) {
          HInstruction* user = it2->GetUser();
          size_t index = it2->GetIndex();
          // Increment `it2` now because `*it2` may disappear thanks to user->ReplaceInput().
          ++it2;
          if (user->GetBlock() != true_block) {
            if (phi == nullptr) {
              phi = NewPhi(new_preheader, instruction, type);
            }
            user->ReplaceInput(phi, index);  // Removes the use node from the list.
            induction_range_.Replace(user, instruction, phi);  // update induction
          }
        }
        // Scan all environment uses of an instruction and replace each later use with a phi node.
        const HUseList<HEnvironment*>& env_uses = instruction->GetEnvUses();
        for (auto it2 = env_uses.begin(), end2 = env_uses.end(); it2 != end2; /* ++it2 below */) {
          HEnvironment* user = it2->GetUser();
          size_t index = it2->GetIndex();
          // Increment `it2` now because `*it2` may disappear thanks to user->RemoveAsUserOfInput().
          ++it2;
          if (user->GetHolder()->GetBlock() != true_block) {
            if (phi == nullptr) {
              phi = NewPhi(new_preheader, instruction, type);
            }
            user->RemoveAsUserOfInput(index);
            user->SetRawEnvAt(index, phi);
            phi->AddEnvUseAt(user, index);
          }
        }
      }
    }
  }

  /**
   * Construct a phi(instruction, 0) in the new preheader to fix the dominance relation.
   * These are synthetic phi nodes without a virtual register.
   */
  HPhi* NewPhi(HBasicBlock* new_preheader,
               HInstruction* instruction,
               DataType::Type type) {
    HGraph* graph = GetGraph();
    HInstruction* zero;
    switch (type) {
      case DataType::Type::kReference: zero = graph->GetNullConstant(); break;
      case DataType::Type::kFloat32: zero = graph->GetFloatConstant(0); break;
      case DataType::Type::kFloat64: zero = graph->GetDoubleConstant(0); break;
      default: zero = graph->GetConstant(type, 0); break;
    }
    HPhi* phi = new (graph->GetAllocator())
        HPhi(graph->GetAllocator(), kNoRegNumber, /*number_of_inputs*/ 2, HPhi::ToPhiType(type));
    phi->SetRawInputAt(0, instruction);
    phi->SetRawInputAt(1, zero);
    if (type == DataType::Type::kReference) {
      phi->SetReferenceTypeInfo(instruction->GetReferenceTypeInfo());
    }
    new_preheader->AddPhi(phi);
    return phi;
  }

  /** Helper method to replace an instruction with another instruction. */
  void ReplaceInstruction(HInstruction* instruction, HInstruction* replacement) {
    // Safe iteration.
    if (instruction == next_) {
      next_ = next_->GetNext();
    }
    // Replace and remove.
    instruction->ReplaceWith(replacement);
    instruction->GetBlock()->RemoveInstruction(instruction);
  }

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator_;

  // A set of maps, one per basic block, from instruction to range.
  ScopedArenaVector<ScopedArenaSafeMap<int, ValueRange*>> maps_;

  // Map an HArrayLength instruction's id to the first HBoundsCheck instruction
  // in a block that checks an index against that HArrayLength.
  ScopedArenaSafeMap<int, HBoundsCheck*> first_index_bounds_check_map_;

  // Early-exit loop bookkeeping.
  ScopedArenaSafeMap<uint32_t, bool> early_exit_loop_;

  // Taken-test loop bookkeeping.
  ScopedArenaSafeMap<uint32_t, HBasicBlock*> taken_test_loop_;

  // Finite loop bookkeeping.
  ScopedArenaSet<uint32_t> finite_loop_;

  // Flag that denotes whether dominator-based dynamic elimination has occurred.
  bool has_dom_based_dynamic_bce_;

  // Initial number of blocks.
  uint32_t initial_block_size_;

  // Side effects.
  const SideEffectsAnalysis& side_effects_;

  // Range analysis based on induction variables.
  InductionVarRange induction_range_;

  // Safe iteration.
  HInstruction* next_;

  DISALLOW_COPY_AND_ASSIGN(BCEVisitor);
};

void BoundsCheckElimination::Run() {
  if (!graph_->HasBoundsChecks()) {
    return;
  }

  // Reverse post order guarantees a node's dominators are visited first.
  // We want to visit in the dominator-based order since if a value is known to
  // be bounded by a range at one instruction, it must be true that all uses of
  // that value dominated by that instruction fits in that range. Range of that
  // value can be narrowed further down in the dominator tree.
  BCEVisitor visitor(graph_, side_effects_, induction_analysis_);
  for (size_t i = 0, size = graph_->GetReversePostOrder().size(); i != size; ++i) {
    HBasicBlock* current = graph_->GetReversePostOrder()[i];
    if (visitor.IsAddedBlock(current)) {
      // Skip added blocks. Their effects are already taken care of.
      continue;
    }
    visitor.VisitBasicBlock(current);
    // Skip forward to the current block in case new basic blocks were inserted
    // (which always appear earlier in reverse post order) to avoid visiting the
    // same basic block twice.
    size_t new_size = graph_->GetReversePostOrder().size();
    DCHECK_GE(new_size, size);
    i += new_size - size;
    DCHECK_EQ(current, graph_->GetReversePostOrder()[i]);
    size = new_size;
  }

  // Perform cleanup.
  visitor.Finish();
}

}  // namespace art
