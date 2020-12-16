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

#include "linker/arm/relative_patcher_thumb2.h"

#include <sstream>

#include "arch/arm/asm_support_arm.h"
#include "art_method.h"
#include "base/bit_utils.h"
#include "compiled_method.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "linker/linker_patch.h"
#include "lock_word.h"
#include "mirror/array-inl.h"
#include "mirror/object.h"
#include "read_barrier.h"
#include "utils/arm/assembler_arm_vixl.h"

namespace art {
namespace linker {

// PC displacement from patch location; Thumb2 PC is always at instruction address + 4.
static constexpr int32_t kPcDisplacement = 4;

// Maximum positive and negative displacement for method call measured from the patch location.
// (Signed 25 bit displacement with the last bit 0 has range [-2^24, 2^24-2] measured from
// the Thumb2 PC pointing right after the BL, i.e. 4 bytes later than the patch location.)
constexpr uint32_t kMaxMethodCallPositiveDisplacement = (1u << 24) - 2 + kPcDisplacement;
constexpr uint32_t kMaxMethodCallNegativeDisplacement = (1u << 24) - kPcDisplacement;

// Maximum positive and negative displacement for a conditional branch measured from the patch
// location. (Signed 21 bit displacement with the last bit 0 has range [-2^20, 2^20-2] measured
// from the Thumb2 PC pointing right after the B.cond, i.e. 4 bytes later than the patch location.)
constexpr uint32_t kMaxBcondPositiveDisplacement = (1u << 20) - 2u + kPcDisplacement;
constexpr uint32_t kMaxBcondNegativeDisplacement = (1u << 20) - kPcDisplacement;

Thumb2RelativePatcher::Thumb2RelativePatcher(RelativePatcherTargetProvider* provider)
    : ArmBaseRelativePatcher(provider, InstructionSet::kThumb2) {
}

void Thumb2RelativePatcher::PatchCall(std::vector<uint8_t>* code,
                                      uint32_t literal_offset,
                                      uint32_t patch_offset,
                                      uint32_t target_offset) {
  DCHECK_LE(literal_offset + 4u, code->size());
  DCHECK_EQ(literal_offset & 1u, 0u);
  DCHECK_EQ(patch_offset & 1u, 0u);
  DCHECK_EQ(target_offset & 1u, 1u);  // Thumb2 mode bit.
  uint32_t displacement = CalculateMethodCallDisplacement(patch_offset, target_offset & ~1u);
  displacement -= kPcDisplacement;  // The base PC is at the end of the 4-byte patch.
  DCHECK_EQ(displacement & 1u, 0u);
  DCHECK((displacement >> 24) == 0u || (displacement >> 24) == 255u);  // 25-bit signed.
  uint32_t signbit = (displacement >> 31) & 0x1;
  uint32_t i1 = (displacement >> 23) & 0x1;
  uint32_t i2 = (displacement >> 22) & 0x1;
  uint32_t imm10 = (displacement >> 12) & 0x03ff;
  uint32_t imm11 = (displacement >> 1) & 0x07ff;
  uint32_t j1 = i1 ^ (signbit ^ 1);
  uint32_t j2 = i2 ^ (signbit ^ 1);
  uint32_t value = (signbit << 26) | (j1 << 13) | (j2 << 11) | (imm10 << 16) | imm11;
  value |= 0xf000d000;  // BL

  // Check that we're just overwriting an existing BL.
  DCHECK_EQ(GetInsn32(code, literal_offset) & 0xf800d000, 0xf000d000);
  // Write the new BL.
  SetInsn32(code, literal_offset, value);
}

void Thumb2RelativePatcher::PatchPcRelativeReference(std::vector<uint8_t>* code,
                                                     const LinkerPatch& patch,
                                                     uint32_t patch_offset,
                                                     uint32_t target_offset) {
  uint32_t literal_offset = patch.LiteralOffset();
  uint32_t pc_literal_offset = patch.PcInsnOffset();
  uint32_t pc_base = patch_offset + (pc_literal_offset - literal_offset) + 4u /* PC adjustment */;
  uint32_t diff = target_offset - pc_base;

  uint32_t insn = GetInsn32(code, literal_offset);
  DCHECK_EQ(insn & 0xff7ff0ffu, 0xf2400000u);  // MOVW/MOVT, unpatched (imm16 == 0).
  uint32_t diff16 = ((insn & 0x00800000u) != 0u) ? (diff >> 16) : (diff & 0xffffu);
  uint32_t imm4 = (diff16 >> 12) & 0xfu;
  uint32_t imm = (diff16 >> 11) & 0x1u;
  uint32_t imm3 = (diff16 >> 8) & 0x7u;
  uint32_t imm8 = diff16 & 0xffu;
  insn = (insn & 0xfbf08f00u) | (imm << 26) | (imm4 << 16) | (imm3 << 12) | imm8;
  SetInsn32(code, literal_offset, insn);
}

void Thumb2RelativePatcher::PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                                        const LinkerPatch& patch,
                                                        uint32_t patch_offset) {
  DCHECK_ALIGNED(patch_offset, 2u);
  uint32_t literal_offset = patch.LiteralOffset();
  DCHECK_ALIGNED(literal_offset, 2u);
  DCHECK_LT(literal_offset, code->size());
  uint32_t insn = GetInsn32(code, literal_offset);
  DCHECK_EQ(insn, 0xf0408000);  // BNE +0 (unpatched)
  ThunkKey key = GetBakerThunkKey(patch);
  if (kIsDebugBuild) {
    const uint32_t encoded_data = key.GetCustomValue1();
    BakerReadBarrierKind kind = BakerReadBarrierKindField::Decode(encoded_data);
    // Check that the next instruction matches the expected LDR.
    switch (kind) {
      case BakerReadBarrierKind::kField: {
        BakerReadBarrierWidth width = BakerReadBarrierWidthField::Decode(encoded_data);
        if (width == BakerReadBarrierWidth::kWide) {
          DCHECK_GE(code->size() - literal_offset, 8u);
          uint32_t next_insn = GetInsn32(code, literal_offset + 4u);
          // LDR (immediate), encoding T3, with correct base_reg.
          CheckValidReg((next_insn >> 12) & 0xfu);  // Check destination register.
          const uint32_t base_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
          CHECK_EQ(next_insn & 0xffff0000u, 0xf8d00000u | (base_reg << 16));
        } else {
          DCHECK_GE(code->size() - literal_offset, 6u);
          uint32_t next_insn = GetInsn16(code, literal_offset + 4u);
          // LDR (immediate), encoding T1, with correct base_reg.
          CheckValidReg(next_insn & 0x7u);  // Check destination register.
          const uint32_t base_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
          CHECK_EQ(next_insn & 0xf838u, 0x6800u | (base_reg << 3));
        }
        break;
      }
      case BakerReadBarrierKind::kArray: {
        DCHECK_GE(code->size() - literal_offset, 8u);
        uint32_t next_insn = GetInsn32(code, literal_offset + 4u);
        // LDR (register) with correct base_reg, S=1 and option=011 (LDR Wt, [Xn, Xm, LSL #2]).
        CheckValidReg((next_insn >> 12) & 0xfu);  // Check destination register.
        const uint32_t base_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
        CHECK_EQ(next_insn & 0xffff0ff0u, 0xf8500020u | (base_reg << 16));
        CheckValidReg(next_insn & 0xf);  // Check index register
        break;
      }
      case BakerReadBarrierKind::kGcRoot: {
        BakerReadBarrierWidth width = BakerReadBarrierWidthField::Decode(encoded_data);
        if (width == BakerReadBarrierWidth::kWide) {
          DCHECK_GE(literal_offset, 4u);
          uint32_t prev_insn = GetInsn32(code, literal_offset - 4u);
          // LDR (immediate), encoding T3, with correct root_reg.
          const uint32_t root_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
          CHECK_EQ(prev_insn & 0xfff0f000u, 0xf8d00000u | (root_reg << 12));
        } else {
          DCHECK_GE(literal_offset, 2u);
          uint32_t prev_insn = GetInsn16(code, literal_offset - 2u);
          // LDR (immediate), encoding T1, with correct root_reg.
          const uint32_t root_reg = BakerReadBarrierFirstRegField::Decode(encoded_data);
          CHECK_EQ(prev_insn & 0xf807u, 0x6800u | root_reg);
        }
        break;
      }
      default:
        LOG(FATAL) << "Unexpected type: " << static_cast<uint32_t>(key.GetType());
        UNREACHABLE();
    }
  }
  uint32_t target_offset = GetThunkTargetOffset(key, patch_offset);
  DCHECK_ALIGNED(target_offset, 4u);
  uint32_t disp = target_offset - (patch_offset + kPcDisplacement);
  DCHECK((disp >> 20) == 0u || (disp >> 20) == 0xfffu);   // 21-bit signed.
  insn |= ((disp << (26 - 20)) & 0x04000000u) |           // Shift bit 20 to 26, "S".
          ((disp >> (19 - 11)) & 0x00000800u) |           // Shift bit 19 to 13, "J1".
          ((disp >> (18 - 13)) & 0x00002000u) |           // Shift bit 18 to 11, "J2".
          ((disp << (16 - 12)) & 0x003f0000u) |           // Shift bits 12-17 to 16-25, "imm6".
          ((disp >> (1 - 0)) & 0x000007ffu);              // Shift bits 1-12 to 0-11, "imm11".
  SetInsn32(code, literal_offset, insn);
}

#define __ assembler.GetVIXLAssembler()->

static void EmitGrayCheckAndFastPath(arm::ArmVIXLAssembler& assembler,
                                     vixl::aarch32::Register base_reg,
                                     vixl::aarch32::MemOperand& lock_word,
                                     vixl::aarch32::Label* slow_path,
                                     int32_t raw_ldr_offset) {
  using namespace vixl::aarch32;  // NOLINT(build/namespaces)
  // Load the lock word containing the rb_state.
  __ Ldr(ip, lock_word);
  // Given the numeric representation, it's enough to check the low bit of the rb_state.
  static_assert(ReadBarrier::WhiteState() == 0, "Expecting white to have value 0");
  static_assert(ReadBarrier::GrayState() == 1, "Expecting gray to have value 1");
  __ Tst(ip, Operand(LockWord::kReadBarrierStateMaskShifted));
  __ B(ne, slow_path, /* is_far_target */ false);
  __ Add(lr, lr, raw_ldr_offset);
  // Introduce a dependency on the lock_word including rb_state,
  // to prevent load-load reordering, and without using
  // a memory barrier (which would be more expensive).
  __ Add(base_reg, base_reg, Operand(ip, LSR, 32));
  __ Bx(lr);          // And return back to the function.
  // Note: The fake dependency is unnecessary for the slow path.
}

// Load the read barrier introspection entrypoint in register `entrypoint`
static void LoadReadBarrierMarkIntrospectionEntrypoint(arm::ArmVIXLAssembler& assembler,
                                                       vixl::aarch32::Register entrypoint) {
  using vixl::aarch32::MemOperand;
  using vixl::aarch32::ip;
  // Thread Register.
  const vixl::aarch32::Register tr = vixl::aarch32::r9;

  // The register where the read barrier introspection entrypoint is loaded
  // is fixed: `Thumb2RelativePatcher::kBakerCcEntrypointRegister` (R4).
  DCHECK_EQ(entrypoint.GetCode(), Thumb2RelativePatcher::kBakerCcEntrypointRegister);
  // entrypoint = Thread::Current()->pReadBarrierMarkReg12, i.e. pReadBarrierMarkIntrospection.
  DCHECK_EQ(ip.GetCode(), 12u);
  const int32_t entry_point_offset =
      Thread::ReadBarrierMarkEntryPointsOffset<kArmPointerSize>(ip.GetCode());
  __ Ldr(entrypoint, MemOperand(tr, entry_point_offset));
}

void Thumb2RelativePatcher::CompileBakerReadBarrierThunk(arm::ArmVIXLAssembler& assembler,
                                                         uint32_t encoded_data) {
  using namespace vixl::aarch32;  // NOLINT(build/namespaces)
  BakerReadBarrierKind kind = BakerReadBarrierKindField::Decode(encoded_data);
  switch (kind) {
    case BakerReadBarrierKind::kField: {
      // Check if the holder is gray and, if not, add fake dependency to the base register
      // and return to the LDR instruction to load the reference. Otherwise, use introspection
      // to load the reference and call the entrypoint (in kBakerCcEntrypointRegister)
      // that performs further checks on the reference and marks it if needed.
      Register base_reg(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(base_reg.GetCode());
      Register holder_reg(BakerReadBarrierSecondRegField::Decode(encoded_data));
      CheckValidReg(holder_reg.GetCode());
      BakerReadBarrierWidth width = BakerReadBarrierWidthField::Decode(encoded_data);
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip);
      // If base_reg differs from holder_reg, the offset was too large and we must have
      // emitted an explicit null check before the load. Otherwise, we need to null-check
      // the holder as we do not necessarily do that check before going to the thunk.
      vixl::aarch32::Label throw_npe;
      if (holder_reg.Is(base_reg)) {
        __ CompareAndBranchIfZero(holder_reg, &throw_npe, /* is_far_target */ false);
      }
      vixl::aarch32::Label slow_path;
      MemOperand lock_word(holder_reg, mirror::Object::MonitorOffset().Int32Value());
      const int32_t raw_ldr_offset = (width == BakerReadBarrierWidth::kWide)
          ? BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET
          : BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET;
      EmitGrayCheckAndFastPath(assembler, base_reg, lock_word, &slow_path, raw_ldr_offset);
      __ Bind(&slow_path);
      const int32_t ldr_offset = /* Thumb state adjustment (LR contains Thumb state). */ -1 +
                                 raw_ldr_offset;
      Register ep_reg(kBakerCcEntrypointRegister);
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ep_reg);
      if (width == BakerReadBarrierWidth::kWide) {
        MemOperand ldr_half_address(lr, ldr_offset + 2);
        __ Ldrh(ip, ldr_half_address);        // Load the LDR immediate half-word with "Rt | imm12".
        __ Ubfx(ip, ip, 0, 12);               // Extract the offset imm12.
        __ Ldr(ip, MemOperand(base_reg, ip));   // Load the reference.
      } else {
        MemOperand ldr_address(lr, ldr_offset);
        __ Ldrh(ip, ldr_address);             // Load the LDR immediate, encoding T1.
        __ Add(ep_reg,                        // Adjust the entrypoint address to the entrypoint
               ep_reg,                        // for narrow LDR.
               Operand(BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_ENTRYPOINT_OFFSET));
        __ Ubfx(ip, ip, 6, 5);                // Extract the imm5, i.e. offset / 4.
        __ Ldr(ip, MemOperand(base_reg, ip, LSL, 2));   // Load the reference.
      }
      // Do not unpoison. With heap poisoning enabled, the entrypoint expects a poisoned reference.
      __ Bx(ep_reg);                          // Jump to the entrypoint.
      if (holder_reg.Is(base_reg)) {
        // Add null check slow path. The stack map is at the address pointed to by LR.
        __ Bind(&throw_npe);
        int32_t offset = GetThreadOffset<kArmPointerSize>(kQuickThrowNullPointer).Int32Value();
        __ Ldr(ip, MemOperand(/* Thread* */ vixl::aarch32::r9, offset));
        __ Bx(ip);
      }
      break;
    }
    case BakerReadBarrierKind::kArray: {
      Register base_reg(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(base_reg.GetCode());
      DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
      DCHECK(BakerReadBarrierWidthField::Decode(encoded_data) == BakerReadBarrierWidth::kWide);
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip);
      vixl::aarch32::Label slow_path;
      int32_t data_offset =
          mirror::Array::DataOffset(Primitive::ComponentSize(Primitive::kPrimNot)).Int32Value();
      MemOperand lock_word(base_reg, mirror::Object::MonitorOffset().Int32Value() - data_offset);
      DCHECK_LT(lock_word.GetOffsetImmediate(), 0);
      const int32_t raw_ldr_offset = BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET;
      EmitGrayCheckAndFastPath(assembler, base_reg, lock_word, &slow_path, raw_ldr_offset);
      __ Bind(&slow_path);
      const int32_t ldr_offset = /* Thumb state adjustment (LR contains Thumb state). */ -1 +
                                 raw_ldr_offset;
      MemOperand ldr_address(lr, ldr_offset + 2);
      __ Ldrb(ip, ldr_address);               // Load the LDR (register) byte with "00 | imm2 | Rm",
                                              // i.e. Rm+32 because the scale in imm2 is 2.
      Register ep_reg(kBakerCcEntrypointRegister);
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ep_reg);
      __ Bfi(ep_reg, ip, 3, 6);               // Insert ip to the entrypoint address to create
                                              // a switch case target based on the index register.
      __ Mov(ip, base_reg);                   // Move the base register to ip0.
      __ Bx(ep_reg);                          // Jump to the entrypoint's array switch case.
      break;
    }
    case BakerReadBarrierKind::kGcRoot: {
      // Check if the reference needs to be marked and if so (i.e. not null, not marked yet
      // and it does not have a forwarding address), call the correct introspection entrypoint;
      // otherwise return the reference (or the extracted forwarding address).
      // There is no gray bit check for GC roots.
      Register root_reg(BakerReadBarrierFirstRegField::Decode(encoded_data));
      CheckValidReg(root_reg.GetCode());
      DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
      BakerReadBarrierWidth width = BakerReadBarrierWidthField::Decode(encoded_data);
      UseScratchRegisterScope temps(assembler.GetVIXLAssembler());
      temps.Exclude(ip);
      vixl::aarch32::Label return_label, not_marked, forwarding_address;
      __ CompareAndBranchIfZero(root_reg, &return_label, /* is_far_target */ false);
      MemOperand lock_word(root_reg, mirror::Object::MonitorOffset().Int32Value());
      __ Ldr(ip, lock_word);
      __ Tst(ip, LockWord::kMarkBitStateMaskShifted);
      __ B(eq, &not_marked);
      __ Bind(&return_label);
      __ Bx(lr);
      __ Bind(&not_marked);
      static_assert(LockWord::kStateShift == 30 && LockWord::kStateForwardingAddress == 3,
                    "To use 'CMP ip, #modified-immediate; BHS', we need the lock word state in "
                    " the highest bits and the 'forwarding address' state to have all bits set");
      __ Cmp(ip, Operand(0xc0000000));
      __ B(hs, &forwarding_address);
      Register ep_reg(kBakerCcEntrypointRegister);
      LoadReadBarrierMarkIntrospectionEntrypoint(assembler, ep_reg);
      // Adjust the art_quick_read_barrier_mark_introspection address in kBakerCcEntrypointRegister
      // to art_quick_read_barrier_mark_introspection_gc_roots.
      int32_t entrypoint_offset = (width == BakerReadBarrierWidth::kWide)
          ? BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_ENTRYPOINT_OFFSET
          : BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_ENTRYPOINT_OFFSET;
      __ Add(ep_reg, ep_reg, Operand(entrypoint_offset));
      __ Mov(ip, root_reg);
      __ Bx(ep_reg);
      __ Bind(&forwarding_address);
      __ Lsl(root_reg, ip, LockWord::kForwardingAddressShift);
      __ Bx(lr);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected kind: " << static_cast<uint32_t>(kind);
      UNREACHABLE();
  }
}

std::vector<uint8_t> Thumb2RelativePatcher::CompileThunk(const ThunkKey& key) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  arm::ArmVIXLAssembler assembler(&allocator);

  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      // The thunk just uses the entry point in the ArtMethod. This works even for calls
      // to the generic JNI and interpreter trampolines.
      assembler.LoadFromOffset(
          arm::kLoadWord,
          vixl::aarch32::pc,
          vixl::aarch32::r0,
          ArtMethod::EntryPointFromQuickCompiledCodeOffset(kArmPointerSize).Int32Value());
      __ Bkpt(0);
      break;
    case ThunkType::kBakerReadBarrier:
      CompileBakerReadBarrierThunk(assembler, key.GetCustomValue1());
      break;
  }

  assembler.FinalizeCode();
  std::vector<uint8_t> thunk_code(assembler.CodeSize());
  MemoryRegion code(thunk_code.data(), thunk_code.size());
  assembler.FinalizeInstructions(code);
  return thunk_code;
}

std::string Thumb2RelativePatcher::GetThunkDebugName(const ThunkKey& key) {
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
          oss << "Field";
          if (BakerReadBarrierWidthField::Decode(encoded_data) == BakerReadBarrierWidth::kWide) {
            oss << "Wide";
          }
          oss << "_r" << BakerReadBarrierFirstRegField::Decode(encoded_data)
              << "_r" << BakerReadBarrierSecondRegField::Decode(encoded_data);
          break;
        case BakerReadBarrierKind::kArray:
          oss << "Array_r" << BakerReadBarrierFirstRegField::Decode(encoded_data);
          DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
          DCHECK(BakerReadBarrierWidthField::Decode(encoded_data) == BakerReadBarrierWidth::kWide);
          break;
        case BakerReadBarrierKind::kGcRoot:
          oss << "GcRoot";
          if (BakerReadBarrierWidthField::Decode(encoded_data) == BakerReadBarrierWidth::kWide) {
            oss << "Wide";
          }
          oss << "_r" << BakerReadBarrierFirstRegField::Decode(encoded_data);
          DCHECK_EQ(kInvalidEncodedReg, BakerReadBarrierSecondRegField::Decode(encoded_data));
          break;
      }
      return oss.str();
    }
  }
}

#undef __

uint32_t Thumb2RelativePatcher::MaxPositiveDisplacement(const ThunkKey& key) {
  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      return kMaxMethodCallPositiveDisplacement;
    case ThunkType::kBakerReadBarrier:
      return kMaxBcondPositiveDisplacement;
  }
}

uint32_t Thumb2RelativePatcher::MaxNegativeDisplacement(const ThunkKey& key) {
  switch (key.GetType()) {
    case ThunkType::kMethodCall:
      return kMaxMethodCallNegativeDisplacement;
    case ThunkType::kBakerReadBarrier:
      return kMaxBcondNegativeDisplacement;
  }
}

void Thumb2RelativePatcher::SetInsn32(std::vector<uint8_t>* code, uint32_t offset, uint32_t value) {
  DCHECK_LE(offset + 4u, code->size());
  DCHECK_ALIGNED(offset, 2u);
  uint8_t* addr = &(*code)[offset];
  addr[0] = (value >> 16) & 0xff;
  addr[1] = (value >> 24) & 0xff;
  addr[2] = (value >> 0) & 0xff;
  addr[3] = (value >> 8) & 0xff;
}

uint32_t Thumb2RelativePatcher::GetInsn32(ArrayRef<const uint8_t> code, uint32_t offset) {
  DCHECK_LE(offset + 4u, code.size());
  DCHECK_ALIGNED(offset, 2u);
  const uint8_t* addr = &code[offset];
  return
      (static_cast<uint32_t>(addr[0]) << 16) +
      (static_cast<uint32_t>(addr[1]) << 24) +
      (static_cast<uint32_t>(addr[2]) << 0)+
      (static_cast<uint32_t>(addr[3]) << 8);
}

template <typename Vector>
uint32_t Thumb2RelativePatcher::GetInsn32(Vector* code, uint32_t offset) {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");
  return GetInsn32(ArrayRef<const uint8_t>(*code), offset);
}

uint32_t Thumb2RelativePatcher::GetInsn16(ArrayRef<const uint8_t> code, uint32_t offset) {
  DCHECK_LE(offset + 2u, code.size());
  DCHECK_ALIGNED(offset, 2u);
  const uint8_t* addr = &code[offset];
  return (static_cast<uint32_t>(addr[0]) << 0) + (static_cast<uint32_t>(addr[1]) << 8);
}

template <typename Vector>
uint32_t Thumb2RelativePatcher::GetInsn16(Vector* code, uint32_t offset) {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");
  return GetInsn16(ArrayRef<const uint8_t>(*code), offset);
}

}  // namespace linker
}  // namespace art
