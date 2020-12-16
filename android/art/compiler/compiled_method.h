/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_COMPILED_METHOD_H_
#define ART_COMPILER_COMPILED_METHOD_H_

#include <memory>
#include <string>
#include <vector>

#include "arch/instruction_set.h"
#include "base/bit_field.h"
#include "base/bit_utils.h"

namespace art {

template <typename T> class ArrayRef;
class CompilerDriver;
class CompiledMethodStorage;
template<typename T> class LengthPrefixedArray;

namespace linker {
class LinkerPatch;
}  // namespace linker

class CompiledCode {
 public:
  // For Quick to supply an code blob
  CompiledCode(CompilerDriver* compiler_driver,
               InstructionSet instruction_set,
               const ArrayRef<const uint8_t>& quick_code);

  virtual ~CompiledCode();

  InstructionSet GetInstructionSet() const {
    return GetPackedField<InstructionSetField>();
  }

  ArrayRef<const uint8_t> GetQuickCode() const;

  bool operator==(const CompiledCode& rhs) const;

  // To align an offset from a page-aligned value to make it suitable
  // for code storage. For example on ARM, to ensure that PC relative
  // valu computations work out as expected.
  size_t AlignCode(size_t offset) const;
  static size_t AlignCode(size_t offset, InstructionSet instruction_set);

  // returns the difference between the code address and a usable PC.
  // mainly to cope with kThumb2 where the lower bit must be set.
  size_t CodeDelta() const;
  static size_t CodeDelta(InstructionSet instruction_set);

  // Returns a pointer suitable for invoking the code at the argument
  // code_pointer address.  Mainly to cope with kThumb2 where the
  // lower bit must be set to indicate Thumb mode.
  static const void* CodePointer(const void* code_pointer, InstructionSet instruction_set);

 protected:
  static constexpr size_t kInstructionSetFieldSize =
      MinimumBitsToStore(static_cast<size_t>(InstructionSet::kLast));
  static constexpr size_t kNumberOfCompiledCodePackedBits = kInstructionSetFieldSize;
  static constexpr size_t kMaxNumberOfPackedBits = sizeof(uint32_t) * kBitsPerByte;

  template <typename T>
  static ArrayRef<const T> GetArray(const LengthPrefixedArray<T>* array);

  CompilerDriver* GetCompilerDriver() {
    return compiler_driver_;
  }

  template <typename BitFieldType>
  typename BitFieldType::value_type GetPackedField() const {
    return BitFieldType::Decode(packed_fields_);
  }

  template <typename BitFieldType>
  void SetPackedField(typename BitFieldType::value_type value) {
    DCHECK(IsUint<BitFieldType::size>(static_cast<uintptr_t>(value)));
    packed_fields_ = BitFieldType::Update(value, packed_fields_);
  }

 private:
  using InstructionSetField = BitField<InstructionSet, 0u, kInstructionSetFieldSize>;

  CompilerDriver* const compiler_driver_;

  // Used to store the compiled code.
  const LengthPrefixedArray<uint8_t>* const quick_code_;

  uint32_t packed_fields_;
};

class CompiledMethod FINAL : public CompiledCode {
 public:
  // Constructs a CompiledMethod.
  // Note: Consider using the static allocation methods below that will allocate the CompiledMethod
  //       in the swap space.
  CompiledMethod(CompilerDriver* driver,
                 InstructionSet instruction_set,
                 const ArrayRef<const uint8_t>& quick_code,
                 const size_t frame_size_in_bytes,
                 const uint32_t core_spill_mask,
                 const uint32_t fp_spill_mask,
                 const ArrayRef<const uint8_t>& method_info,
                 const ArrayRef<const uint8_t>& vmap_table,
                 const ArrayRef<const uint8_t>& cfi_info,
                 const ArrayRef<const linker::LinkerPatch>& patches);

  virtual ~CompiledMethod();

  static CompiledMethod* SwapAllocCompiledMethod(
      CompilerDriver* driver,
      InstructionSet instruction_set,
      const ArrayRef<const uint8_t>& quick_code,
      const size_t frame_size_in_bytes,
      const uint32_t core_spill_mask,
      const uint32_t fp_spill_mask,
      const ArrayRef<const uint8_t>& method_info,
      const ArrayRef<const uint8_t>& vmap_table,
      const ArrayRef<const uint8_t>& cfi_info,
      const ArrayRef<const linker::LinkerPatch>& patches);

  static void ReleaseSwapAllocatedCompiledMethod(CompilerDriver* driver, CompiledMethod* m);

  bool IsIntrinsic() const {
    return GetPackedField<IsIntrinsicField>();
  }

  // Marks the compiled method as being generated using an intrinsic codegen.
  // Such methods have no relationships to their code items.
  // This affects debug information generated at link time.
  void MarkAsIntrinsic() {
    DCHECK(!IsIntrinsic());
    SetPackedField<IsIntrinsicField>(/* value */ true);
  }

  size_t GetFrameSizeInBytes() const {
    return frame_size_in_bytes_;
  }

  uint32_t GetCoreSpillMask() const {
    return core_spill_mask_;
  }

  uint32_t GetFpSpillMask() const {
    return fp_spill_mask_;
  }

  ArrayRef<const uint8_t> GetMethodInfo() const;

  ArrayRef<const uint8_t> GetVmapTable() const;

  ArrayRef<const uint8_t> GetCFIInfo() const;

  ArrayRef<const linker::LinkerPatch> GetPatches() const;

 private:
  static constexpr size_t kIsIntrinsicLsb = kNumberOfCompiledCodePackedBits;
  static constexpr size_t kIsIntrinsicSize = 1u;
  static constexpr size_t kNumberOfCompiledMethodPackedBits = kIsIntrinsicLsb + kIsIntrinsicSize;
  static_assert(kNumberOfCompiledMethodPackedBits <= CompiledCode::kMaxNumberOfPackedBits,
                "Too many packed fields.");

  using IsIntrinsicField = BitField<bool, kIsIntrinsicLsb, kIsIntrinsicSize>;

  // For quick code, the size of the activation used by the code.
  const size_t frame_size_in_bytes_;
  // For quick code, a bit mask describing spilled GPR callee-save registers.
  const uint32_t core_spill_mask_;
  // For quick code, a bit mask describing spilled FPR callee-save registers.
  const uint32_t fp_spill_mask_;
  // For quick code, method specific information that is not very dedupe friendly (method indices).
  const LengthPrefixedArray<uint8_t>* const method_info_;
  // For quick code, holds code infos which contain stack maps, inline information, and etc.
  const LengthPrefixedArray<uint8_t>* const vmap_table_;
  // For quick code, a FDE entry for the debug_frame section.
  const LengthPrefixedArray<uint8_t>* const cfi_info_;
  // For quick code, linker patches needed by the method.
  const LengthPrefixedArray<linker::LinkerPatch>* const patches_;
};

}  // namespace art

#endif  // ART_COMPILER_COMPILED_METHOD_H_
