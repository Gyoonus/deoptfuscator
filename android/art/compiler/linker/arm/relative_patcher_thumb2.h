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

#ifndef ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_
#define ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_

#include "arch/arm/registers_arm.h"
#include "base/array_ref.h"
#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "linker/arm/relative_patcher_arm_base.h"

namespace art {

namespace arm {
class ArmVIXLAssembler;
}  // namespace arm

namespace linker {

class Thumb2RelativePatcher FINAL : public ArmBaseRelativePatcher {
 public:
  static constexpr uint32_t kBakerCcEntrypointRegister = 4u;

  static uint32_t EncodeBakerReadBarrierFieldData(uint32_t base_reg,
                                                  uint32_t holder_reg,
                                                  bool narrow) {
    CheckValidReg(base_reg);
    CheckValidReg(holder_reg);
    DCHECK(!narrow || base_reg < 8u) << base_reg;
    BakerReadBarrierWidth width =
        narrow ? BakerReadBarrierWidth::kNarrow : BakerReadBarrierWidth::kWide;
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kField) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(holder_reg) |
           BakerReadBarrierWidthField::Encode(width);
  }

  static uint32_t EncodeBakerReadBarrierArrayData(uint32_t base_reg) {
    CheckValidReg(base_reg);
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kArray) |
           BakerReadBarrierFirstRegField::Encode(base_reg) |
           BakerReadBarrierSecondRegField::Encode(kInvalidEncodedReg) |
           BakerReadBarrierWidthField::Encode(BakerReadBarrierWidth::kWide);
  }

  static uint32_t EncodeBakerReadBarrierGcRootData(uint32_t root_reg, bool narrow) {
    CheckValidReg(root_reg);
    DCHECK(!narrow || root_reg < 8u) << root_reg;
    BakerReadBarrierWidth width =
        narrow ? BakerReadBarrierWidth::kNarrow : BakerReadBarrierWidth::kWide;
    return BakerReadBarrierKindField::Encode(BakerReadBarrierKind::kGcRoot) |
           BakerReadBarrierFirstRegField::Encode(root_reg) |
           BakerReadBarrierSecondRegField::Encode(kInvalidEncodedReg) |
           BakerReadBarrierWidthField::Encode(width);
  }

  explicit Thumb2RelativePatcher(RelativePatcherTargetProvider* provider);

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
  static constexpr uint32_t kInvalidEncodedReg = /* pc is invalid */ 15u;

  enum class BakerReadBarrierKind : uint8_t {
    kField,   // Field get or array get with constant offset (i.e. constant index).
    kArray,   // Array get with index in register.
    kGcRoot,  // GC root load.
    kLast = kGcRoot
  };

  enum class BakerReadBarrierWidth : uint8_t {
    kWide,          // 32-bit LDR (and 32-bit NEG if heap poisoning is enabled).
    kNarrow,        // 16-bit LDR (and 16-bit NEG if heap poisoning is enabled).
    kLast = kNarrow
  };

  static constexpr size_t kBitsForBakerReadBarrierKind =
      MinimumBitsToStore(static_cast<size_t>(BakerReadBarrierKind::kLast));
  static constexpr size_t kBitsForRegister = 4u;
  using BakerReadBarrierKindField =
      BitField<BakerReadBarrierKind, 0, kBitsForBakerReadBarrierKind>;
  using BakerReadBarrierFirstRegField =
      BitField<uint32_t, kBitsForBakerReadBarrierKind, kBitsForRegister>;
  using BakerReadBarrierSecondRegField =
      BitField<uint32_t, kBitsForBakerReadBarrierKind + kBitsForRegister, kBitsForRegister>;
  static constexpr size_t kBitsForBakerReadBarrierWidth =
      MinimumBitsToStore(static_cast<size_t>(BakerReadBarrierWidth::kLast));
  using BakerReadBarrierWidthField = BitField<BakerReadBarrierWidth,
                                              kBitsForBakerReadBarrierKind + 2 * kBitsForRegister,
                                              kBitsForBakerReadBarrierWidth>;

  static void CheckValidReg(uint32_t reg) {
    DCHECK(reg < 12u && reg != kBakerCcEntrypointRegister) << reg;
  }

  void CompileBakerReadBarrierThunk(arm::ArmVIXLAssembler& assembler, uint32_t encoded_data);

  void SetInsn32(std::vector<uint8_t>* code, uint32_t offset, uint32_t value);
  static uint32_t GetInsn32(ArrayRef<const uint8_t> code, uint32_t offset);

  template <typename Vector>
  static uint32_t GetInsn32(Vector* code, uint32_t offset);

  static uint32_t GetInsn16(ArrayRef<const uint8_t> code, uint32_t offset);

  template <typename Vector>
  static uint32_t GetInsn16(Vector* code, uint32_t offset);

  friend class Thumb2RelativePatcherTest;

  DISALLOW_COPY_AND_ASSIGN(Thumb2RelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_COMPILER_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_
