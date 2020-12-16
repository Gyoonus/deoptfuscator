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

#ifndef ART_COMPILER_OPTIMIZING_NODES_MIPS_H_
#define ART_COMPILER_OPTIMIZING_NODES_MIPS_H_

namespace art {

// Compute the address of the method for MIPS Constant area support.
class HMipsComputeBaseMethodAddress : public HExpression<0> {
 public:
  // Treat the value as an int32_t, but it is really a 32 bit native pointer.
  HMipsComputeBaseMethodAddress()
      : HExpression(kMipsComputeBaseMethodAddress,
                    DataType::Type::kInt32,
                    SideEffects::None(),
                    kNoDexPc) {
  }

  bool CanBeMoved() const OVERRIDE { return true; }

  DECLARE_INSTRUCTION(MipsComputeBaseMethodAddress);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(MipsComputeBaseMethodAddress);
};

// Mips version of HPackedSwitch that holds a pointer to the base method address.
class HMipsPackedSwitch FINAL : public HTemplateInstruction<2> {
 public:
  HMipsPackedSwitch(int32_t start_value,
                    int32_t num_entries,
                    HInstruction* input,
                    HMipsComputeBaseMethodAddress* method_base,
                    uint32_t dex_pc)
    : HTemplateInstruction(kMipsPackedSwitch, SideEffects::None(), dex_pc),
      start_value_(start_value),
      num_entries_(num_entries) {
    SetRawInputAt(0, input);
    SetRawInputAt(1, method_base);
  }

  bool IsControlFlow() const OVERRIDE { return true; }

  int32_t GetStartValue() const { return start_value_; }

  int32_t GetNumEntries() const { return num_entries_; }

  HBasicBlock* GetDefaultBlock() const {
    // Last entry is the default block.
    return GetBlock()->GetSuccessors()[num_entries_];
  }

  DECLARE_INSTRUCTION(MipsPackedSwitch);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(MipsPackedSwitch);

 private:
  const int32_t start_value_;
  const int32_t num_entries_;
};

// This instruction computes part of the array access offset (index offset).
//
// For array accesses the element address has the following structure:
// Address = CONST_OFFSET + base_addr + index << ELEM_SHIFT. The address part
// (index << ELEM_SHIFT) can be shared across array accesses with
// the same data type and index. For example, in the following loop 5 accesses can share address
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
// effects.
class HIntermediateArrayAddressIndex FINAL : public HExpression<2> {
 public:
  HIntermediateArrayAddressIndex(HInstruction* index, HInstruction* shift, uint32_t dex_pc)
      : HExpression(kIntermediateArrayAddressIndex,
                    DataType::Type::kInt32,
                    SideEffects::None(),
                    dex_pc) {
    SetRawInputAt(0, index);
    SetRawInputAt(1, shift);
  }

  bool CanBeMoved() const OVERRIDE { return true; }
  bool InstructionDataEquals(const HInstruction* other ATTRIBUTE_UNUSED) const OVERRIDE {
    return true;
  }
  bool IsActualObject() const OVERRIDE { return false; }

  HInstruction* GetIndex() const { return InputAt(0); }
  HInstruction* GetShift() const { return InputAt(1); }

  DECLARE_INSTRUCTION(IntermediateArrayAddressIndex);

 protected:
  DEFAULT_COPY_CONSTRUCTOR(IntermediateArrayAddressIndex);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_NODES_MIPS_H_
