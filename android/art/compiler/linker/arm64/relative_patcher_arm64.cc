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

#include "linker/arm64/relative_patcher_arm64.h"

#include "arch/arm64/asm_support_arm64.h"
#include "arch/arm64/instruction_set_features_arm64.h"
#include "art_method.h"
#include "base/bit_utils.h"
#include "compiled_method-inl.h"
#include "driver/compiler_driver.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "heap_poisoning.h"
#include "linker/linker_patch.h"
#include "linker/output_stream.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object.h"
#include "oat.h"
#include "oat_quick_method_header.h"
#include "read_barrier.h"
#include "utils/arm64/assembler_arm64.h"

namespace art {
namespace linker {

namespace {

// Maximum positive and negative displacement for method call measured from the patch location.
// (Signed 28 bit displacement with the last two bits 0 has range [-2^27, 2^27-4] measured from
// the ARM64 PC pointing to the BL.)
constexpr uint32_t kMaxMethodCallPositiveDisplacement = (1u << 27) - 4u;
constexpr uint32_t kMaxMethodCallNegativeDisplacement = (1u << 27);

// Maximum positive and negative displacement for a conditional branch measured from the patch
// location. (Signed 21 bit displacement with the last two bits 0 has range [-2^20, 2^20-4]
// measured from the ARM64 PC pointing to the B.cond.)
constexpr uint32_t kMaxBcondPositiveDisplacement = (1u << 20) - 4u;
constexpr uint32_t kMaxBcondNegativeDisplacement = (1u << 20);

// The ADRP thunk for erratum 843419 is 2 instructions, i.e. 8 bytes.
constexpr uint32_t kAdrpThunkSize = 8u;

inline bool IsAdrpPatch(const LinkerPatch& patch) {
  switch (patch.GetType()) {
    case LinkerPatch::Type::kCall:
    case LinkerPatch::Type::kCallRelative:
    case LinkerPatch::Type::kBakerReadBarrierBranch:
      return false;
    case LinkerPatch::Type::kMethodRelative:
    case LinkerPatch::Type::kMethodBssEntry:
    case LinkerPatch::Type::kTypeRelative:
    case LinkerPatch::Type::kTypeClassTable:
    case LinkerPatch::Type::kTypeBssEntry:
    case LinkerPatch::Type::kStringRelative:
    case LinkerPatch::Type::kStringInternTable:
    case LinkerPatch::Type::kStringBssEntry:
      return patch.LiteralOffset() == patch.PcInsnOffset();
  }
}

inline uint32_t MaxExtraSpace(size_t num_adrp, size_t code_size) {
  if (num_adrp == 0u) {
    return 0u;
  }
  uint32_t alignment_bytes =
      CompiledMethod::AlignCode(code_size, InstructionSet::kArm64) - code_size;
  return kAdrpThunkSize * num_adrp + alignment_bytes;
}

}  // anonymous namespace

Arm64RelativePatcher::Arm64RelativePatcher(RelativePatcherTargetProvider* provider,
                                           const Arm64InstructionSetFeatures* features)
    : ArmBaseRelativePatcher(provider, InstructionSet::kArm64),
      fix_cortex_a53_843419_(features->NeedFixCortexA53_843419()),
      reserved_adrp_thunks_(0u),
      processed_adrp_thunks_(0u) {
  if (fix_cortex_a53_843419_) {
    adrp_thunk_locations_.reserve(16u);
    current_method_thunks_.reserve(16u * kAdrpThunkSize);
  }
}

uint32_t Arm64RelativePatcher::ReserveSpace(uint32_t offset,
                                            const CompiledMethod* compiled_method,
                                            MethodReference method_ref) {
  if (!fix_cortex_a53_843419_) {
    DCHECK(adrp_thunk_locations_.empty());
    return ReserveSpaceInternal(offset, compiled_method, method_ref, 0u);
  }

  // Add thunks for previous method if any.
  if (reserved_adrp_thunks_ != adrp_thunk_locations_.size()) {
    size_t num_adrp_thunks = adrp_thunk_locations_.size() - reserved_adrp_thunks_;
    offset = CompiledMethod::AlignCode(offset, InstructionSet::kArm64) +
             kAdrpThunkSize * num_adrp_thunks;
    reserved_adrp_thunks_ = adrp_thunk_locations_.size();
  }

  // Count the number of ADRP insns as the upper bound on the number of thunks needed
  // and use it to reserve space for other linker patches.
  size_t num_adrp = 0u;
  DCHECK(compiled_method != nullptr);
  for (const LinkerPatch& patch : compiled_method->GetPatches()) {
    if (IsAdrpPatch(patch)) {
      ++num_adrp;
    }
  }
  ArrayRef<const uint8_t> code = compiled_method->GetQuickCode();
  uint32_t max_extra_space = MaxExtraSpace(num_adrp, code.size());
  offset = ReserveSpaceInternal(offset, compiled_method, method_ref, max_extra_space);
  if (num_adrp == 0u) {
    return offset;
  }

  // Now that we have the actual offset where the code will be placed, locate the ADRP insns
  // that actually require the thunk.
  uint32_t quick_code_offset = compiled_method->AlignCode(offset + sizeof(OatQuickMethodHeader));
  uint32_t thunk_offset = compiled_method->AlignCode(quick_code_offset + code.size());
  DCHECK(compiled_method != nullptr);
  for (const LinkerPatch& patch : compiled_method->GetPatches()) {
    if (IsAdrpPatch(patch)) {
      uint32_t patch_offset = quick_code_offset + patch.LiteralOffset();
      if (NeedsErratum843419Thunk(code, patch.LiteralOffset(), patch_offset)) {
        adrp_thunk_locations_.emplace_back(patch_offset, thunk_offset);
        thunk_offset += kAdrpThunkSize;
      }
    }
  }
  return offset;
}

uint32_t Arm64RelativePatcher::ReserveSpaceEnd(uint32_t offset) {
  if (!fix_cortex_a53_843419_) {
    DCHECK(adrp_thunk_locations_.empty());
  } else {
    // Add thunks for the last method if any.
    if (reserved_adrp_thunks_ != adrp_thunk_locations_.size()) {
      size_t num_adrp_thunks = adrp_thunk_locations_.size() - reserved_adrp_thunks_;
      offset = CompiledMethod::AlignCode(offset, InstructionSet::kArm64) +
               kAdrpThunkSize * num_adrp_thunks;
      reserved_adrp_thunks_ = adrp_thunk_locations_.size();
    }
  }
  return ArmBaseRelativePatcher::ReserveSpaceEnd(offset);
}

uint32_t Arm64RelativePatcher::WriteThunks(OutputStream* out, uint32_t offset) {
  if (fix_cortex_a53_843419_) {
    if (!current_method_thunks_.empty()) {
      uint32_t aligned_offset = CompiledMethod::AlignCode(offset, InstructionSet::kArm64);
      if (kIsDebugBuild) {
        CHECK_ALIGNED(current_method_thunks_.size(), kAdrpThunkSize);
        size_t num_thunks = current_method_thunks_.size() / kAdrpThunkSize;
        CHECK_LE(num_thunks, processed_adrp_thunks_);
        for (size_t i = 0u; i != num_thunks; ++i) {
          const auto& entry = adrp_thunk_locations_[processed_adrp_thunks_ - num_thunks + i];
          CHECK_EQ(entry.second, aligned_offset + i * kAdrpThunkSize);
        }
      }
      uint32_t aligned_code_delta = aligned_offset - offset;
      if (aligned_code_delta != 0u && !WriteCodeAlignment(out, aligned_code_delta)) {
        return 0u;
      }
      if (!WriteMiscThunk(out, ArrayRef<const uint8_t>(current_method_thunks_))) {
        return 0u;
      }
      offset = aligned_offset + current_method_thunks_.size();
      current_method_thunks_.clear();
    }
  }
  return ArmBaseRelativePatcher::WriteThunks(out, offset);
}

void Arm64RelativePatcher::PatchCall(std::vector<uint8_t>* code,
                                     uint32_t literal_offset,
                                     uint32_t patch_offset, uint32_t
                                     target_offset) {
  DCHECK_LE(literal_offset + 4u, code->size());
  DCHECK_EQ(literal_offset & 3u, 0u);
  DCHECK_EQ(patch_offset & 3u, 0u);
  DCHECK_EQ(target_offset & 3u, 0u);
  uint32_t displacement = CalculateMethodCallDisplacement(patch_offset, target_offset & ~1u);
  DCHECK_EQ(displacement & 3u, 0u);
  DCHECK((displacement >> 27) == 0u || (displacement >> 27) == 31u);  // 28-bit signed.
  uint32_t insn = (displacement & 0x0fffffffu) >> 2;
  insn |= 0x94000000;  // BL

  // Check that we're just overwriting an existing BL.
  DCHECK_EQ(GetInsn(code, literal_offset) & 0xfc000000u, 0x94000000u);
  // Write the new BL.
  SetInsn(code, literal_offset, insn);
}

void Arm64RelativePatcher::PatchPcRelativeReference(std::vector<uint8_t>* code,
                                                    const LinkerPatch& patch,
                                                    uint32_t patch_offset,
                                                    uint32_t target_offset) {
  DCHECK_EQ(patch_offset & 3u, 0u);
  DCHECK_EQ(target_offset & 3u, 0u);
  uint32_t literal_offset = patch.LiteralOffset();
  uint32_t insn = GetInsn(code, literal_offset);
  uint32_t pc_insn_offset = patch.PcInsnOffset();
  uint32_t disp = target_offset - ((patch_offset - literal_offset + pc_insn_offset) & ~0xfffu);
  bool wide = (insn & 0x40000000) != 0;
  uint32_t shift = wide ? 3u : 2u;
  if (literal_offset == pc_insn_offset) {
    // Check it's an ADRP with imm == 0 (unset).
    DCHECK_EQ((insn & 0xffffffe0u), 0x90000000u)
        << literal_offset << ", " << pc_insn_offset << ", 0x" << std::hex << insn;
    if (fix_cortex_a53_843419_ && processed_adrp_thunks_ != adrp_thunk_locations_.size() &&
        adrp_thunk_locations_[processed_adrp_thunks_].first == patch_offset) {
      DCHECK(NeedsErratum843419Thunk(ArrayRef<const uint8_t>(*code),
                                     literal_offset, patch_offset));
      uint32_t thunk_offset = adrp_thunk_locations_[processed_adrp_thunks_].second;
      uint32_t adrp_disp = target_offset - (thunk_offset & ~0xfffu);
      uint32_t adrp = PatchAdrp(insn, adrp_disp);

      uint32_t out_disp = thunk_offset - patch_offset;
      DCHECK_EQ(out_disp & 3u, 0u);
      DCHECK((out_disp >> 27) == 0u || (out_disp >> 27) == 31u);  // 28-bit signed.
      insn = (out_disp & 0x0fffffffu) >> shift;
      insn |= 0x14000000;  // B <thunk>

      uint32_t back_disp = -out_disp;
      DCHECK_EQ(back_disp & 3u, 0u);
      DCHECK((back_disp >> 27) == 0u || (back_disp >> 27) == 31u);  // 28-bit signed.
      uint32_t b_back = (back_disp & 0x0fffffffu) >> 2;
      b_back |= 0x14000000;  // B <back>
      size_t thunks_code_offset = current_method_thunks_.size();
      current_method_thunks_.resize(thunks_code_offset + kAdrpThunkSize);
      SetInsn(&current_method_thunks_, thunks_code_offset, adrp);
      SetInsn(&current_method_thunks_, thunks_code_offset + 4u, b_back);
      static_assert(kAdrpThunkSize == 2 * 4u, "thunk has 2 instructions");

      processed_adrp_thunks_ += 1u;
    } else {
      insn = PatchAdrp(insn, disp);
    }
    // Write the new ADRP (or B to the erratum 843419 thunk).
    SetInsn(code, literal_offset, insn);
  } else {
    if ((insn & 0xfffffc00) == 0x91000000) {
      // ADD immediate, 64-bit with imm12 == 0 (unset).
      if (!kEmitCompilerReadBarrier) {
        DCHECK(patch.GetType() == LinkerPatch::Type::kMethodRelative ||
               patch.GetType() == LinkerPatch::Type::kTypeRelative ||
               patch.GetType() == LinkerPatch::Type::kStringRelative) << patch.GetType();
      } else {
        // With the read barrier (non-Baker) enabled, it could be kStringBssEntry or kTypeBssEntry.
        DCHECK(patch.GetType() == LinkerPatch::Type::kMethodRelative ||
               patch.GetType() == LinkerPatch::Type::kTypeRelative ||
               patch.GetType() == LinkerPatch::Type::kStringRelative ||
               patch.GetType() == LinkerPatch::Type::kTypeBssEntry ||
               patch.GetType() == LinkerPatch::Type::kStringBssEntry) << patch.GetType();
      }
      shift = 0u;  // No shift for ADD.
    } else {
      // LDR/STR 32-bit or 64-bit with imm12 == 0 (unset).
      DCHECK(patch.GetType() == LinkerPatch::Type::kMethodBssEntry ||
             patch.GetType() == LinkerPatch::Type::kTypeClassTable ||
             patch.GetType() == LinkerPatch::Type::kTypeBssEntry ||
             patch.GetType() == LinkerPatch::Type::kStringInternTable ||
             patch.GetType() == LinkerPatch::Type::kStringBssEntry) << patch.GetType();
      DCHECK_EQ(insn & 0xbfbffc00, 0xb9000000) << std::hex << insn;
    }
    if (kIsDebugBuild) {
      uint32_t adrp = GetInsn(code, pc_insn_offset);
      if ((adrp & 0x9f000000u) != 0x90000000u) {
        CHECK(fix_cortex_a53_843419_);
        CHECK_EQ(adrp & 0xfc000000u, 0x14000000u);  // B <thunk>
        CHECK_ALIGNED(current_method_thunks_.size(), kAdrpThunkSize);
        size_t num_thunks = current_method_thunks_.size() / kAdrpThunkSize;
        CHECK_LE(num_thunks, processed_adrp_thunks_);
        uint32_t b_offset = patch_offset - literal_offset + pc_insn_offset;
        for (size_t i = processed_adrp_thunks_ - num_thunks; ; ++i) {
          CHECK_NE(i, processed_adrp_thunks_);
          if (adrp_thunk_locations_[i].first == b_offset) {
            size_t idx = num_thunks - (processed_adrp_thunks_ - i);
            adrp = GetInsn(&current_method_thunks_, idx * kAdrpThunkSize);
            break;
          }
        }
      }
      CHECK_EQ(adrp & 0x9f00001fu,                    // Check that pc_insn_offset points
               0x90000000 | ((insn >> 5) & 0x1fu));   // to ADRP with matching register.
    }
    uint32_t imm12 = (disp & 0xfffu) >> shift;
    insn = (insn & ~(0xfffu << 10)) | (imm12 << 10);
    SetInsn(code, literal_offset, insn);
  }
}

void Arm64RelativePatcher::PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                                       const LinkerPatch& patch,
                                                       uint32_t patch_offset) {
  DCHECK_ALIGNED(patch_offset, 4u);
  uint32_t literal_offset = patch.LiteralOffset();
  DCHECK_ALIGNED(literal_offset, 4u);
  DCHECK_LT(literal_offset, code->size());
  uint32_t insn = GetInsn(code, literal_offset);
  DCHECK_EQ(insn & 0xffffffe0u, 0xb5000000);  // CBNZ Xt, +0 (unpatched)
  ThunkKey key = GetBakerThunkKey(patch);
  if (kIsDebugBuild) {
    const uint32_t encoded_data = key.GetCustomValue1();
    BakerReadBarrierKind kind = BakerReadBarrierKindField::Decode(encoded_data);
    // Check that the next instruction matches the expected LDR.
    switch (kind) {
      case BakerReadBarrierKind::kField: {
        DCHECK_GE(code->size() - literal_offset, 8u);
        uint32_t next_insn = GetInsn(code, literal_offset + 4u);
        // LDR (immediate) with correct base_reg.
        CheckValidReg(next_insn & 0x1fu);  // Check destination register.
        const uint32_t base_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
        CHECK_EQ(next_insn & 0xffc003e0u, 0xb9400000u | (base_reg << 5));
        break;
      }
      case BakerReadBarrierKind::kArray: {
        DCHECK_GE(code->size() - literal_offset, 8u);
        uint32_t next_insn = GetInsn(code, literal_offset + 4u);
        // LDR (register) with the correct base_reg, size=10 (32-bit), option=011 (extend = LSL),
        // and S=1 (shift amount = 2 for 32-bit version), i.e. LDR Wt, [Xn, Xm, LSL #2].
        CheckValidReg(next_insn & 0x1fu);  // Check destination register.
        const uint32_t base_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
        CHECK_EQ(next_insn & 0xffe0ffe0u, 0xb8607800u | (base_reg << 5));
        CheckValidReg((next_insn >> 16) & 0x1f);  // Check index register
        break;
      }
      case BakerReadBarrierKind::kGcRoot: {
        DCHECK_GE(literal_offset, 4u);
        uint32_t prev_insn = GetInsn(code, literal_offset - 4u);
        // LDR (immediate) with correct root_reg.
        const uint32_t root_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
        CHECK_EQ(prev_insn & 0xffc0001fu, 0xb9400000u | root_reg);
        break;
      }
      default:
        LOG(FATAL) << "Unexpected kind: " << static_cast<uint32_t>(kind);
        UNREACHABLE();
    }
  }
  uint32_t target_offset = GetThunkTargetOffset(key, patch_offset);
  DCHECK_ALIGNED(target_offset, 4u);
  uint32_t disp = target_offset - patch_offset;
  DCHECK((disp >> 20) == 0u || (disp >> 20) == 4095u);  // 21-bit signed.
  insn |= (disp << (5 - 2)) & 0x00ffffe0u;              // Shift bits 2-20 to 5-23.
  SetInsn(code, literal_offset, insn);
}

#define __ assembler.GetVIXLAssembler()->

static void EmitGrayCheckAndFastPath(arm64::Arm64Assembler& assembler,
                                     vixl::aarch64::Register base_reg,
                                     vixl::aarch64::MemOperand& lock_word,
                                     vixl::aarch64::Label* slow_path) {
  using namespace vixl::aarch64;  // NOLINT(build/namespaces)
  // Load the lock word containing the rb_state.
  __ Ldr(ip0.W(), lock_word);
  // Given the numeric representation, it's enough to check the low bit of the rb_state.
  static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
  static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
  __ Tbnz(ip0.W(), LockWord::kReadBarrierStateShift, slow_path);
  static_assert(
      BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET == BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET,
      "Field and array LDR offsets must be the same to reuse the same code.");
  // Adjust the return address back to the LDR (1 instruction; 2 for heap poisoning).
  static_assert(BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET == (kPoisonHeapReferences ? -8 : -4),
                "Field LDR must be 1 instruction (4B) before the return address label; "
                " 2 instructions (8B) for heap poisoning.");
  __ Add(lr, lr, BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET);
  // Introduce a dependency on the lock_word including rb_state,
  // to prevent load-load reordering, and without using
  // a memory barrier (which would be more expensive).
  __ Add(base_reg, base_reg, Operand(ip0, LSR, 32));
  __ Br(lr);          // And return back to the function.
  // Note: The fake dependency is unnecessary for the slow path.
}

// Load the read barrier introspection entrypoint in register `entrypoint`.
static void LoadReadBarrierMarkIntrospectionEntrypoint(arm64::Arm64Assembler& assembler,
                                                       vixl::aarch64::Register entrypoint) {
  using vixl::aarch64::MemOperand;
  using vixl::aarch64::ip0;
  // Thread Register.
  const vixl::aarch64::Register tr = vixl::aarch64::x19;

  // entrypoint = Thread::Current()->pReadBarrierMarkReg16, i.e. pReadBarrierMarkIntrospection.
  DCHECK_EQ(ip0.GetCode(), 16u);
  const int32_t entry_point_offset =
      Thread::ReadBarrierMarkEntryPointsOffset<kArm64PointerSize>(ip0.GetCode());
  __ Ldr(entrypoint, MemOperand(tr, entry_point_offset));
}

void Arm64RelativePatcher::CompileBakerReadBarrierThunk(arm64::Arm64Assembler& assembler,
                                                        uint32_t encoded_data) {
  using namespace vixl::aarch64;  // NOLINT(build/namespaces)
  BakerReadBarrierKind kind = BakerReadBarrierKindField::Decode(encoded_data);
  switch (kind) {
    case BakerReadBarrierKind::kField: {
      // Check if the holder is gray and, if not, add fake dependency to the base register
      // and return to the LDR instruction to load the reference. Otherwise, use introspection
      // to load the reference and call the entrypoint (in IP1) that performs further checks
      // on the reference and marks it if needed.
      auto base_reg =
          Register::GetXRegFromCode(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(base_reg.GetCode());
      auto holder_reg =
          Register::GetXRegFromCode(BakerReadBarrierSecondRegField::Decode(encoded_data));
      CheckValidReg(holder_reg.GetCode());
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip0, ip1);
      // If base_reg differs from holder_reg, the offset was too large and we must have
      // emitted an explicit null check before the load. Otherwise, we need to null-check
      // the holder as we do not necessarily do that check before going to the thunk.
      vixl::aarch64::Label throw_npe;
      if (holder_reg.Is(base_reg)) {
        __ Cbz(holder_reg.W(), &throw_npe);
      }
      vixl::aarch64::Label slow_path;
      MemOperand lock_word(holder_reg, mirror::Object::MonitorOffset().Int32Value());
      EmitGrayCheckAndFastPath(assembler, base_reg, lock_word, &slow_path);
      __ Bind(&slow_path);
      MemOperand ldr_address(lr, BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET);
      __ Ldr(ip0.W(), ldr_address);         // Load the LDR (immediate) unsigned offset.
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ip1);
      __ Ubfx(ip0.W(), ip0.W(), 10, 12);    // Extract the offset.
      __ Ldr(ip0.W(), MemOperand(base_reg, ip0, LSL, 2));   // Load the reference.
      // Do not unpoison. With heap poisoning enabled, the entrypoint expects a poisoned reference.
      __ Br(ip1);                           // Jump to the entrypoint.
      if (holder_reg.Is(base_reg)) {
        // Add null check slow path. The stack map is at the address pointed to by LR.
        __ Bind(&throw_npe);
        int32_t offset = GetThreadOffset<kArm64PointerSize>(kQuickThrowNullPointer).Int32Value();
        __ Ldr(ip0, MemOperand(/* Thread* */ vixl::aarch64::x19, offset));
        __ Br(ip0);
      }
      break;
    }
    case BakerReadBarrierKind::kArray: {
      auto base_reg =
          Register::GetXRegFromCode(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(base_reg.GetCode());
      DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip0, ip1);
      vixl::aarch64::Label slow_path;
      int32_t data_offset =
          mirror::Array::DataOffset(Primitive::ComponentSize(Primitive::kPrimNot)).Int32Value();
      MemOperand lock_word(base_reg, mirror::Object::MonitorOffset().Int32Value() - data_offset);
      DCHECK_LT(lock_word.GetOffset(), 0);
      EmitGrayCheckAndFastPath(assembler, base_reg, lock_word, &slow_path);
      __ Bind(&slow_path);
      MemOperand ldr_address(lr, BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET);
      __ Ldr(ip0.W(), ldr_address);         // Load the LDR (register) unsigned offset.
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ip1);
      __ Ubfx(ip0, ip0, 16, 6);             // Extract the index register, plus 32 (bit 21 is set).
      __ Bfi(ip1, ip0, 3, 6);               // Insert ip0 to the entrypoint address to create
                                            // a switch case target based on the index register.
      __ Mov(ip0, base_reg);                // Move the base register to ip0.
      __ Br(ip1);                           // Jump to the entrypoint's array switch case.
      break;
    }
    case BakerReadBarrierKind::kGcRoot: {
      // Check if the reference needs to be marked and if so (i.e. not null, not marked yet
      // and it does not have a forwarding address), call the correct introspection entrypoint;
      // otherwise return the reference (or the extracted forwarding address).
      // There is no gray bit check for GC roots.
      auto root_reg =
          Register::GetWRegFromCode(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(root_reg.GetCode());
      DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip0, ip1);
      vixl::aarch64::Label return_label, not_marked, forwarding_address;
      __ Cbz(root_reg, &return_label);
      MemOperand lock_word(root_reg.X(), mirror::Object::MonitorOffset().Int32Value());
      __ Ldr(ip0.W(), lock_word);
      __ Tbz(ip0.W(), LockWord::kMarkBitStateShift, &not_marked);
      __ Bind(&return_label);
      __ Br(lr);
      __ Bind(&not_marked);
      __ Tst(ip0.W(), Operand(ip0.W(), LSL, 1));
      __ B(&forwarding_address, mi);
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ip1);
      // Adjust the art_quick_read_barrier_mark_introspection address in IP1 to
      // art_quick_read_barrier_mark_introspection_gc_roots.
      __ Add(ip1, ip1, Operand(BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET));
      __ Mov(ip0.W(), root_reg);
      __ Br(ip1);
      __ Bind(&forwarding_address);
      __ Lsl(root_reg, ip0.W(), LockWord::kForwardingAddressShift);
      __ Br(lr);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected kind: " << static_cast<uint32_t>(kind);
      UNREACHABLE();
  }
}

std::vector<uint8_t> Arm64RelativePatcher::CompileThunk(const ThunkKey& key) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  arm64::Arm64Assembler assembler(&allocator);

  switch (key.GetType()) {
    case ThunkType::kMethodCall: {
      // The thunk just uses the entry point in the ArtMethod. This works even for calls
      // to the generic JNI and interpreter trampolines.
      Offset offset(ArtMethod::EntryPointFromQuickCompiledCodeOffset(
          kArm64PointerSize).Int32Value());
      assembler.JumpTo(ManagedRegister(arm64::X0), offset, ManagedRegister(arm64::IP0));
      break;
    }
    case ThunkType::kBakerReadBarrier: {
      CompileBakerReadBarrierThunk(assembler, key.GetCustomValue1());
      break;
    }
  }

  // Ensure we emit the literal pool.
  assembler.FinalizeCode();
  std::vector<uint8_t> thunk_code(assembler.CodeSize());
  MemoryRegion code(thunk_code.data(), thunk_code.size());
  assembler.FinalizeInstructions(code);
  return thunk_code;
}

std::string Arm64RelativePatcher::GetThunkDebugName(const ThunkKey& key) {
  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      return "MethodCallThunk";

    case ThunkType::kBakerReadBarrier: {
      uint32_t encoded_data = key.GetCustomValue1();
      BakerReadBarrierKind kind = BakerReadBarrierKindField::Decode(encoded_data);
      std::ostringstream oss;
      oss << "BakerReadBarrierThunk";
      switch (kind) {
        case BakerReadBarrierKind::kField:
          oss << "Field_r" << BakerReadBarrierFirstRegField::Decode(encoded_data)
              << "_r" << BakerReadBarrierSecondRegField::Decode(encoded_data);
          break;
        case BakerReadBarrierKind::kArray:
          oss << "Array_r" << BakerReadBarrierFirstRegField::Decode(encoded_data);
          DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
          break;
        case BakerReadBarrierKind::kGcRoot:
          oss << "GcRoot_r" << BakerReadBarrierFirstRegField::Decode(encoded_data);
          DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
          break;
      }
      return oss.str();
    }
  }
}

#undef __

uint32_t Arm64RelativePatcher::MaxPositiveDisplacement(const ThunkKey& key) {
  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      return kMaxMethodCallPositiveDisplacement;
    case ThunkType::kBakerReadBarrier:
      return kMaxBcondPositiveDisplacement;
  }
}

uint32_t Arm64RelativePatcher::MaxNegativeDisplacement(const ThunkKey& key) {
  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      return kMaxMethodCallNegativeDisplacement;
    case ThunkType::kBakerReadBarrier:
      return kMaxBcondNegativeDisplacement;
  }
}

uint32_t Arm64RelativePatcher::PatchAdrp(uint32_t adrp, uint32_t disp) {
  return (adrp & 0x9f00001fu) |  // Clear offset bits, keep ADRP with destination reg.
      // Bottom 12 bits are ignored, the next 2 lowest bits are encoded in bits 29-30.
      ((disp & 0x00003000u) << (29 - 12)) |
      // The next 16 bits are encoded in bits 5-22.
      ((disp & 0xffffc000u) >> (12 + 2 - 5)) |
      // Since the target_offset is based on the beginning of the oat file and the
      // image space precedes the oat file, the target_offset into image space will
      // be negative yet passed as uint32_t. Therefore we limit the displacement
      // to +-2GiB (rather than the maximim +-4GiB) and determine the sign bit from
      // the highest bit of the displacement. This is encoded in bit 23.
      ((disp & 0x80000000u) >> (31 - 23));
}

bool Arm64RelativePatcher::NeedsErratum843419Thunk(ArrayRef<const uint8_t> code,
                                                   uint32_t literal_offset,
                                                   uint32_t patch_offset) {
  DCHECK_EQ(patch_offset & 0x3u, 0u);
  if ((patch_offset & 0xff8) == 0xff8) {  // ...ff8 or ...ffc
    uint32_t adrp = GetInsn(code, literal_offset);
    DCHECK_EQ(adrp & 0x9f000000, 0x90000000);
    uint32_t next_offset = patch_offset + 4u;
    uint32_t next_insn = GetInsn(code, literal_offset + 4u);

    // Below we avoid patching sequences where the adrp is followed by a load which can easily
    // be proved to be aligned.

    // First check if the next insn is the LDR using the result of the ADRP.
    // LDR <Wt>, [<Xn>, #pimm], where <Xn> == ADRP destination reg.
    if ((next_insn & 0xffc00000) == 0xb9400000 &&
        (((next_insn >> 5) ^ adrp) & 0x1f) == 0) {
      return false;
    }

    // And since LinkerPatch::Type::k{Method,Type,String}Relative is using the result
    // of the ADRP for an ADD immediate, check for that as well. We generalize a bit
    // to include ADD/ADDS/SUB/SUBS immediate that either uses the ADRP destination
    // or stores the result to a different register.
    if ((next_insn & 0x1f000000) == 0x11000000 &&
        ((((next_insn >> 5) ^ adrp) & 0x1f) == 0 || ((next_insn ^ adrp) & 0x1f) != 0)) {
      return false;
    }

    // LDR <Wt>, <label> is always aligned and thus it doesn't cause boundary crossing.
    if ((next_insn & 0xff000000) == 0x18000000) {
      return false;
    }

    // LDR <Xt>, <label> is aligned iff the pc + displacement is a multiple of 8.
    if ((next_insn & 0xff000000) == 0x58000000) {
      bool is_aligned_load = (((next_offset >> 2) ^ (next_insn >> 5)) & 1) == 0;
      return !is_aligned_load;
    }

    // LDR <Wt>, [SP, #<pimm>] and LDR <Xt>, [SP, #<pimm>] are always aligned loads, as SP is
    // guaranteed to be 128-bits aligned and <pimm> is multiple of the load size.
    if ((next_insn & 0xbfc003e0) == 0xb94003e0) {
      return false;
    }
    return true;
  }
  return false;
}

void Arm64RelativePatcher::SetInsn(std::vector<uint8_t>* code, uint32_t offset, uint32_t value) {
  DCHECK_LE(offset + 4u, code->size());
  DCHECK_EQ(offset & 3u, 0u);
  uint8_t* addr = &(*code)[offset];
  addr[0] = (value >> 0) & 0xff;
  addr[1] = (value >> 8) & 0xff;
  addr[2] = (value >> 16) & 0xff;
  addr[3] = (value >> 24) & 0xff;
}

uint32_t Arm64RelativePatcher::GetInsn(ArrayRef<const uint8_t> code, uint32_t offset) {
  DCHECK_LE(offset + 4u, code.size());
  DCHECK_EQ(offset & 3u, 0u);
  const uint8_t* addr = &code[offset];
  return
      (static_cast<uint32_t>(addr[0]) << 0) +
      (static_cast<uint32_t>(addr[1]) << 8) +
      (static_cast<uint32_t>(addr[2]) << 16)+
      (static_cast<uint32_t>(addr[3]) << 24);
}

template <typename Alloc>
uint32_t Arm64RelativePatcher::GetInsn(std::vector<uint8_t, Alloc>* code, uint32_t offset) {
  return GetInsn(ArrayRef<const uint8_t>(*code), offset);
}

}  // namespace linker
}  // namespace art
