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

#ifndef ART_COMPILER_LINKER_ARM64_RELATIVE_PATCHER_ARM64_H_
#define ART_COMPILER_LINKER_ARM64_RELATIVE_PATCHER_ARM64_H_

#include "base/array_ref.h"
#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "linker/arm/relative_patcher_arm_base.h"

namespace art {

namespace arm64 {
class Arm64Assembler;
}  // namespace arm64

namespace linker {

class Arm64RelativePatcher FINAL : public ArmBaseRelativePatcher {
 public:
  static uint32_t EncodeBakerReadBarrierFieldData(uint32_t base_reg, uint32_t holder_reg) {
    CheckValidReg(base_reg);
    CheckValidReg(holder_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kField) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(holder_reg);
  }

  static uint32_t EncodeBakerReadBarrierArrayData(uint32_t base_reg) {
    CheckValidReg(base_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kArray) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(kInvalidEncodedReg);
  }

  static uint32_t EncodeBakerReadBarrierGcRootData(uint32_t root_reg) {
    CheckValidReg(root_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kGcRoot) |
           BakerReadBarrierFirstRegField::Encode(root_reg) |
           BakerReadBarrierSecondRegField::Encode(kInvalidEncodedReg);
  }

  Arm64RelativePatcher(RelativePatcherTargetProvider* provider,
                       const Arm64InstructionSetFeatures* features);

  uint32_t ReserveSpace(uint32_t offset,
                        const CompiledMethod* compiled_method,
                        MethodReference method_ref) OVERRIDE;
  uint32_t ReserveSpaceEnd(uint32_t offset) OVERRIDE;
  uint32_t WriteThunks(OutputStream* out, uint32_t offset) OVERRIDE;
  void PatchCall(std::vector<uint8_t>* code,
                 uint32_t literal_offset,
                 uint32_t patch_offset,
                 uint32_t target_offset) OVERRIDE;
  void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                const LinkerPatch& patch,
                                uint32_t patch_offset,
                                uint32_t target_offset) OVERRIDE;
  void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                   const LinkerPatch& patch,
                                   uint32_t patch_offset) OVERRIDE;

 protected:
  std::vector<uint8_t> CompileThunk(const ThunkKey& key) OVERRIDE;
  std::string GetThunkDebugName(const ThunkKey& key) OVERRIDE;
  uint32_t MaxPositiveDisplacement(const ThunkKey& key) OVERRIDE;
  uint32_t MaxNegativeDisplacement(const ThunkKey& key) OVERRIDE;

 private:
  static constexpr uint32_t kInvalidEncodedReg = /* sp/zr is invalid */ 31u;

  enum class BakerReadBarrierKind : uint8_t {
    kField,   // Field get or array get with constant offset (i.e. constant index).
    kArray,   // Array get with index in register.
    kGcRoot,  // GC root load.
    kLast = kGcRoot
  };

  static constexpr size_t kBitsForBakerReadBarrierKind =
      MinimumBitsToStore(static_cast<size_t>(BakerReadBarrierKind::kLast));
  static constexpr size_t kBitsForRegister = 5u;
  using BakerReadBarrierKindField =
      BitField<BakerReadBarrierKind, 0, kBitsForBakerReadBarrierKind>;
  using BakerReadBarrierFirstRegField =
      BitField<uint32_t, kBitsForBakerReadBarrierKind, kBitsForRegister>;
  using BakerReadBarrierSecondRegField =
      BitField<uint32_t, kBitsForBakerReadBarrierKind + kBitsForRegister, kBitsForRegister>;

  static void CheckValidReg(uint32_t reg) {
    DCHECK(reg < 30u && reg != 16u && reg != 17u) << reg;
  }

  void CompileBakerReadBarrierThunk(arm64::Arm64Assembler& assembler, uint32_t encoded_data);

  static uint32_t PatchAdrp(uint32_t adrp, uint32_t disp);

  static bool NeedsErratum843419Thunk(ArrayRef<const uint8_t> code, uint32_t literal_offset,
                                      uint32_t patch_offset);
  void SetInsn(std::vector<uint8_t>* code, uint32_t offset, uint32_t value);
  static uint32_t GetInsn(ArrayRef<const uint8_t> code, uint32_t offset);

  template <typename Alloc>
  static uint32_t GetInsn(std::vector<uint8_t, Alloc>* code, uint32_t offset);

  const bool fix_cortex_a53_843419_;
  // Map original patch_offset to thunk offset.
  std::vector<std::pair<uint32_t, uint32_t>> adrp_thunk_locations_;
  size_t reserved_adrp_thunks_;
  size_t processed_adrp_thunks_;
  std::vector<uint8_t> current_method_thunks_;

  friend class Arm64RelativePatcherTest;

  DISALLOW_COPY_AND_ASSIGN(Arm64RelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_ARM64_RELATIVE_PATCHER_ARM64_H_
