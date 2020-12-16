/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_NODES_SHARED_H_
#define ART_COMPILER_OPTIMIZING_NODES_SHARED_H_

// This `#include` should never be used by compilation, as this file (`nodes_shared.h`) is included
// in `nodes.h`. However it helps editing tools (e.g. YouCompleteMe) by giving them better context
// (defining `HInstruction` and co).
#include "nodes.h"

namespace art {

class HMultiplyAccumulate FINAL : public HExpression<3> {
 public:
  HMultiplyAccumulate(DataType::Type type,
                      InstructionKind op,
                      HInstruction* accumulator,
                      HInstruction* mul_left,
                      HInstruction* mul_right,
                      uint32_t dex_pc = kNoDexPc)
      : HExpression(kMultiplyAccumulate, type, SideEffects::None(), dex_pc),
        op_kind_(op) {
    SetRawInputAt(kInputAccumulatorIndex, accumulator);
    SetRawInputAt(kInputMulLeftIndex, mul_left);
    SetRawInputAt(kInputMulRightIndex, mul_right);
  }

  bool IsClonable() const OVERRIDE { return true; }

  static constexpr int kInputAccumulatorIndex = 0;
  static constexpr int kInputMulLeftIndex = 1;
  static constexpr int kInputMulRightIndex = 2;

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other) const OVERRIDE {
    return op_kind_ == other->AsMultiplyAccumulate()->op_kind_;
  }

  InstructionKind GetOpKind() const { return op_kind_; }

  DECLARE_INSTRUCTION(MultiplyAccumulate);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(MultiplyAccumulate);

 private:
  // Indicates if this is a MADD or MSUB.
  const InstructionKind op_kind_;
};

class HBitwiseNegatedRight FINAL : public HBinaryOperation {
 public:
  HBitwiseNegatedRight(DataType::Type result_type,
                       InstructionKind op,
                       HInstruction* left,
                       HInstruction* right,
                       uint32_t dex_pc = kNoDexPc)
    : HBinaryOperation(kBitwiseNegatedRight,
                       result_type,
                       left,
                       right,
                       SideEffects::None(),
                       dex_pc),
      op_kind_(op) {
    DCHECK(op == HInstruction::kAnd || op == HInstruction::kOr || op == HInstruction::kXor) << op;
  }

  template <typename T, typename U>
  auto Compute(T x, U y) const -> decltype(x & ~y) {
    static_assert(std::is_same<decltype(x & ~y), decltype(x | ~y)>::value &&
                  std::is_same<decltype(x & ~y), decltype(x ^ ~y)>::value,
                  "Inconsistent negated bitwise types");
    switch (op_kind_) {
      case HInstruction::kAnd:
        return x & ~y;
      case HInstruction::kOr:
        return x | ~y;
      case HInstruction::kXor:
        return x ^ ~y;
      default:
        LOG(FATAL) << "Unreachable";
        UNREACHABLE();
    }
  }

  HConstant* Evaluate(HIntConstant* x, HIntConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetIntConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HLongConstant* x, HLongConstant* y) const OVERRIDE {
    return GetBlock()->GetGraph()->GetLongConstant(
        Compute(x->GetValue(), y->GetValue()), GetDexPc());
  }
  HConstant* Evaluate(HFloatConstant* x ATTRIBUTE_UNUSED,
                      HFloatConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for float values";
    UNREACHABLE();
  }
  HConstant* Evaluate(HDoubleConstant* x ATTRIBUTE_UNUSED,
                      HDoubleConstant* y ATTRIBUTE_UNUSED) const OVERRIDE {
    LOG(FATAL) << DebugName() << " is not defined for double values";
    UNREACHABLE();
  }

  InstructionKind GetOpKind() const { return op_kind_; }

  DECLARE_INSTRUCTION(BitwiseNegatedRight);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(BitwiseNegatedRight);

 private:
  // Specifies the bitwise operation, which will be then negated.
  const InstructionKind op_kind_;
};

// This instruction computes part of the array access offset (data and index offset).
//
// For array accesses the element address has the following structure:
// Address = CONST_OFFSET + base_addr + index << ELEM_SHIFT. Taking into account LDR/STR addressing
// modes address part (CONST_OFFSET + index << ELEM_SHIFT) can be shared across array access with
// the same data type and index. For example, for the following loop 5 accesses can share address
// computation:
//
// void foo(int[] a, int[] b, int[] c) {
//   for (i...) {
//     a[i] = a[i] + 5;
//     b[i] = b[i] + c[i];
//   }
// }
//
// Note: as the instruction doesn't involve base array address into computations it has no side
// effects (in comparison of HIntermediateAddress).
class HIntermediateAddressIndex FINAL : public HExpression<3> {
 public:
  HIntermediateAddressIndex(
      HInstruction* index, HInstruction* offset, HInstruction* shift, uint32_t dex_pc)
      : HExpression(kIntermediateAddressIndex,
                    DataType::Type::kInt32,
                    SideEffects::None(),
                    dex_pc) {
    SetRawInputAt(0, index);
    SetRawInputAt(1, offset);
    SetRawInputAt(2, shift);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }
  bool IsActualObject() const OVERRIDE { return false; }

  HInstruction* GetIndex() const { return InputAt(0); }
  HInstruction* GetOffset() const { return InputAt(1); }
  HInstruction* GetShift() const { return InputAt(2); }

  DECLARE_INSTRUCTION(IntermediateAddressIndex);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(IntermediateAddressIndex);
};

class HDataProcWithShifterOp FINAL : public HExpression<2> {
 public:
  enum OpKind {
    kLSL,   // Logical shift left.
    kLSR,   // Logical shift right.
    kASR,   // Arithmetic shift right.
    kUXTB,  // Unsigned extend byte.
    kUXTH,  // Unsigned extend half-word.
    kUXTW,  // Unsigned extend word.
    kSXTB,  // Signed extend byte.
    kSXTH,  // Signed extend half-word.
    kSXTW,  // Signed extend word.

    // Aliases.
    kFirstShiftOp = kLSL,
    kLastShiftOp = kASR,
    kFirstExtensionOp = kUXTB,
    kLastExtensionOp = kSXTW
  };
  HDataProcWithShifterOp(HInstruction* instr,
                         HInstruction* left,
                         HInstruction* right,
                         OpKind op,
                         // The shift argument is unused if the operation
                         // is an extension.
                         int shift = 0,
                         uint32_t dex_pc = kNoDexPc)
      : HExpression(kDataProcWithShifterOp, instr->GetType(), SideEffects::None(), dex_pc),
        instr_kind_(instr->GetKind()), op_kind_(op),
        shift_amount_(shift & (instr->GetType() == DataType::Type::kInt32
            ? kMaxIntShiftDistance
            : kMaxLongShiftDistance)) {
    DCHECK(!instr->HasSideEffects());
    SetRawInputAt(0, left);
    SetRawInputAt(1, right);
  }

  bool IsClonable() const OVERRIDE { return true; }
  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other_instr) const OVERRIDE {
    const HDataProcWithShifterOp* other = other_instr->AsDataProcWithShifterOp();
    return instr_kind_ == other->instr_kind_ &&
        op_kind_ == other->op_kind_ &&
        shift_amount_ == other->shift_amount_;
  }

  static bool IsShiftOp(OpKind op_kind) {
    return kFirstShiftOp <= op_kind && op_kind <= kLastShiftOp;
  }

  static bool IsExtensionOp(OpKind op_kind) {
    return kFirstExtensionOp <= op_kind && op_kind <= kLastExtensionOp;
  }

  // Find the operation kind and shift amount from a bitfield move instruction.
  static void GetOpInfoFromInstruction(HInstruction* bitfield_op,
                                       /*out*/OpKind* op_kind,
                                       /*out*/int* shift_amount);

  InstructionKind GetInstrKind() const { return instr_kind_; }
  OpKind GetOpKind() const { return op_kind_; }
  int GetShiftAmount() const { return shift_amount_; }

  DECLARE_INSTRUCTION(DataProcWithShifterOp);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(DataProcWithShifterOp);

 private:
  InstructionKind instr_kind_;
  OpKind op_kind_;
  int shift_amount_;

  friend std::ostream& operator<<(std::ostream& os, OpKind op);
};

std::ostream& operator<<(std::ostream& os, const HDataProcWithShifterOp::OpKind op);

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_SHARED_H_
