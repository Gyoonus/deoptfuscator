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

#include <iostream>
#include <type_traits>

#include "assembler_arm_vixl.h"
#include "base/bit_utils.h"
#include "base/bit_utils_iterator.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "heap_poisoning.h"
#include "thread.h"

using namespace vixl::aarch32;  // NOLINT(build/namespaces)

using vixl::ExactAssemblyScope;
using vixl::CodeBufferCheckScope;

namespace art {
namespace arm {

#ifdef ___
#error "ARM Assembler macro already defined."
#else
#define ___   vixl_masm_.
#endif

// Thread register definition.
extern const vixl32::Register tr(TR);
// Marking register definition.
extern const vixl32::Register mr(MR);

void ArmVIXLAssembler::FinalizeCode() {
  vixl_masm_.FinalizeCode();
}

size_t ArmVIXLAssembler::CodeSize() const {
  return vixl_masm_.GetSizeOfCodeGenerated();
}

const uint8_t* ArmVIXLAssembler::CodeBufferBaseAddress() const {
  return vixl_masm_.GetBuffer().GetStartAddress<const uint8_t*>();
}

void ArmVIXLAssembler::FinalizeInstructions(const MemoryRegion& region) {
  // Copy the instructions from the buffer.
  MemoryRegion from(vixl_masm_.GetBuffer()->GetStartAddress<void*>(), CodeSize());
  region.CopyFrom(0, from);
}

void ArmVIXLAssembler::PoisonHeapReference(vixl::aarch32::Register reg) {
  // reg = -reg.
  ___ Rsb(reg, reg, 0);
}

void ArmVIXLAssembler::UnpoisonHeapReference(vixl::aarch32::Register reg) {
  // reg = -reg.
  ___ Rsb(reg, reg, 0);
}

void ArmVIXLAssembler::MaybePoisonHeapReference(vixl32::Register reg) {
  if (kPoisonHeapReferences) {
    PoisonHeapReference(reg);
  }
}

void ArmVIXLAssembler::MaybeUnpoisonHeapReference(vixl32::Register reg) {
  if (kPoisonHeapReferences) {
    UnpoisonHeapReference(reg);
  }
}

void ArmVIXLAssembler::GenerateMarkingRegisterCheck(vixl32::Register temp, int code) {
  // The Marking Register is only used in the Baker read barrier configuration.
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  vixl32::Label mr_is_ok;

  // temp = self.tls32_.is.gc_marking
  ___ Ldr(temp, MemOperand(tr, Thread::IsGcMarkingOffset<kArmPointerSize>().Int32Value()));
  // Check that mr == self.tls32_.is.gc_marking.
  ___ Cmp(mr, temp);
  ___ B(eq, &mr_is_ok, /* far_target */ false);
  ___ Bkpt(code);
  ___ Bind(&mr_is_ok);
}

void ArmVIXLAssembler::LoadImmediate(vixl32::Register rd, int32_t value) {
  // TODO(VIXL): Implement this optimization in VIXL.
  if (!ShifterOperandCanAlwaysHold(value) && ShifterOperandCanAlwaysHold(~value)) {
    ___ Mvn(rd, ~value);
  } else {
    ___ Mov(rd, value);
  }
}

bool ArmVIXLAssembler::ShifterOperandCanAlwaysHold(uint32_t immediate) {
  return vixl_masm_.IsModifiedImmediate(immediate);
}

bool ArmVIXLAssembler::ShifterOperandCanHold(Opcode opcode,
                                             uint32_t immediate,
                                             vixl::aarch32::FlagsUpdate update_flags) {
  switch (opcode) {
    case ADD:
    case SUB:
      // Less than (or equal to) 12 bits can be done if we don't need to set condition codes.
      if (IsUint<12>(immediate) && update_flags != vixl::aarch32::SetFlags) {
        return true;
      }
      return ShifterOperandCanAlwaysHold(immediate);

    case MOV:
      // TODO: Support less than or equal to 12bits.
      return ShifterOperandCanAlwaysHold(immediate);

    case MVN:
    default:
      return ShifterOperandCanAlwaysHold(immediate);
  }
}

bool ArmVIXLAssembler::CanSplitLoadStoreOffset(int32_t allowed_offset_bits,
                                               int32_t offset,
                                               /*out*/ int32_t* add_to_base,
                                               /*out*/ int32_t* offset_for_load_store) {
  int32_t other_bits = offset & ~allowed_offset_bits;
  if (ShifterOperandCanAlwaysHold(other_bits) || ShifterOperandCanAlwaysHold(-other_bits)) {
    *add_to_base = offset & ~allowed_offset_bits;
    *offset_for_load_store = offset & allowed_offset_bits;
    return true;
  }
  return false;
}

int32_t ArmVIXLAssembler::AdjustLoadStoreOffset(int32_t allowed_offset_bits,
                                                vixl32::Register temp,
                                                vixl32::Register base,
                                                int32_t offset) {
  DCHECK_NE(offset & ~allowed_offset_bits, 0);
  int32_t add_to_base, offset_for_load;
  if (CanSplitLoadStoreOffset(allowed_offset_bits, offset, &add_to_base, &offset_for_load)) {
    ___ Add(temp, base, add_to_base);
    return offset_for_load;
  } else {
    ___ Mov(temp, offset);
    ___ Add(temp, temp, base);
    return 0;
  }
}

// TODO(VIXL): Implement this in VIXL.
int32_t ArmVIXLAssembler::GetAllowedLoadOffsetBits(LoadOperandType type) {
  switch (type) {
    case kLoadSignedByte:
    case kLoadSignedHalfword:
    case kLoadUnsignedHalfword:
    case kLoadUnsignedByte:
    case kLoadWord:
      // We can encode imm12 offset.
      return 0xfff;
    case kLoadSWord:
    case kLoadDWord:
    case kLoadWordPair:
      // We can encode imm8:'00' offset.
      return 0xff << 2;
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

// TODO(VIXL): Implement this in VIXL.
int32_t ArmVIXLAssembler::GetAllowedStoreOffsetBits(StoreOperandType type) {
  switch (type) {
    case kStoreHalfword:
    case kStoreByte:
    case kStoreWord:
      // We can encode imm12 offset.
      return 0xfff;
    case kStoreSWord:
    case kStoreDWord:
    case kStoreWordPair:
      // We can encode imm8:'00' offset.
      return 0xff << 2;
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

// TODO(VIXL): Implement this in VIXL.
static bool CanHoldLoadOffsetThumb(LoadOperandType type, int offset) {
  switch (type) {
    case kLoadSignedByte:
    case kLoadSignedHalfword:
    case kLoadUnsignedHalfword:
    case kLoadUnsignedByte:
    case kLoadWord:
      return IsAbsoluteUint<12>(offset);
    case kLoadSWord:
    case kLoadDWord:
      return IsAbsoluteUint<10>(offset) && IsAligned<4>(offset);  // VFP addressing mode.
    case kLoadWordPair:
      return IsAbsoluteUint<10>(offset) && IsAligned<4>(offset);
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

// TODO(VIXL): Implement this in VIXL.
static bool CanHoldStoreOffsetThumb(StoreOperandType type, int offset) {
  switch (type) {
    case kStoreHalfword:
    case kStoreByte:
    case kStoreWord:
      return IsAbsoluteUint<12>(offset);
    case kStoreSWord:
    case kStoreDWord:
      return IsAbsoluteUint<10>(offset) && IsAligned<4>(offset);  // VFP addressing mode.
    case kStoreWordPair:
      return IsAbsoluteUint<10>(offset) && IsAligned<4>(offset);
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffsetThumb.
// TODO(VIXL): Implement AdjustLoadStoreOffset logic in VIXL.
void ArmVIXLAssembler::StoreToOffset(StoreOperandType type,
                                     vixl32::Register reg,
                                     vixl32::Register base,
                                     int32_t offset) {
  vixl32::Register tmp_reg;
  UseScratchRegisterScope temps(&vixl_masm_);

  if (!CanHoldStoreOffsetThumb(type, offset)) {
    CHECK_NE(base.GetCode(), kIpCode);
    if ((reg.GetCode() != kIpCode) &&
        (!vixl_masm_.GetScratchRegisterList()->IsEmpty()) &&
        ((type != kStoreWordPair) || (reg.GetCode() + 1 != kIpCode))) {
      tmp_reg = temps.Acquire();
    } else {
      // Be careful not to use ip twice (for `reg` (or `reg` + 1 in
      // the case of a word-pair store) and `base`) to build the
      // Address object used by the store instruction(s) below.
      // Instead, save R5 on the stack (or R6 if R5 is already used by
      // `base`), use it as secondary temporary register, and restore
      // it after the store instruction has been emitted.
      tmp_reg = (base.GetCode() != 5) ? r5 : r6;
      ___ Push(tmp_reg);
      if (base.GetCode() == kSpCode) {
        offset += kRegisterSize;
      }
    }
    // TODO: Implement indexed store (not available for STRD), inline AdjustLoadStoreOffset()
    // and in the "unsplittable" path get rid of the "add" by using the store indexed instead.
    offset = AdjustLoadStoreOffset(GetAllowedStoreOffsetBits(type), tmp_reg, base, offset);
    base = tmp_reg;
  }
  DCHECK(CanHoldStoreOffsetThumb(type, offset));
  switch (type) {
    case kStoreByte:
      ___ Strb(reg, MemOperand(base, offset));
      break;
    case kStoreHalfword:
      ___ Strh(reg, MemOperand(base, offset));
      break;
    case kStoreWord:
      ___ Str(reg, MemOperand(base, offset));
      break;
    case kStoreWordPair:
      ___ Strd(reg, vixl32::Register(reg.GetCode() + 1), MemOperand(base, offset));
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
  if ((tmp_reg.IsValid()) && (tmp_reg.GetCode() != kIpCode)) {
    CHECK(tmp_reg.Is(r5) || tmp_reg.Is(r6)) << tmp_reg;
    ___ Pop(tmp_reg);
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffsetThumb.
// TODO(VIXL): Implement AdjustLoadStoreOffset logic in VIXL.
void ArmVIXLAssembler::LoadFromOffset(LoadOperandType type,
                                      vixl32::Register dest,
                                      vixl32::Register base,
                                      int32_t offset) {
  if (!CanHoldLoadOffsetThumb(type, offset)) {
    CHECK(!base.Is(ip));
    // Inlined AdjustLoadStoreOffset() allows us to pull a few more tricks.
    int32_t allowed_offset_bits = GetAllowedLoadOffsetBits(type);
    DCHECK_NE(offset & ~allowed_offset_bits, 0);
    int32_t add_to_base, offset_for_load;
    if (CanSplitLoadStoreOffset(allowed_offset_bits, offset, &add_to_base, &offset_for_load)) {
      // Use reg for the adjusted base. If it's low reg, we may end up using 16-bit load.
      AddConstant(dest, base, add_to_base);
      base = dest;
      offset = offset_for_load;
    } else {
      UseScratchRegisterScope temps(&vixl_masm_);
      vixl32::Register temp = (dest.Is(base)) ? temps.Acquire() : dest;
      LoadImmediate(temp, offset);
      // TODO: Implement indexed load (not available for LDRD) and use it here to avoid the ADD.
      // Use reg for the adjusted base. If it's low reg, we may end up using 16-bit load.
      ___ Add(dest, dest, (dest.Is(base)) ? temp : base);
      base = dest;
      offset = 0;
    }
  }

  DCHECK(CanHoldLoadOffsetThumb(type, offset));
  switch (type) {
    case kLoadSignedByte:
      ___ Ldrsb(dest, MemOperand(base, offset));
      break;
    case kLoadUnsignedByte:
      ___ Ldrb(dest, MemOperand(base, offset));
      break;
    case kLoadSignedHalfword:
      ___ Ldrsh(dest, MemOperand(base, offset));
      break;
    case kLoadUnsignedHalfword:
      ___ Ldrh(dest, MemOperand(base, offset));
      break;
    case kLoadWord:
      CHECK(!dest.IsSP());
      ___ Ldr(dest, MemOperand(base, offset));
      break;
    case kLoadWordPair:
      ___ Ldrd(dest, vixl32::Register(dest.GetCode() + 1), MemOperand(base, offset));
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
}

void ArmVIXLAssembler::StoreSToOffset(vixl32::SRegister source,
                                      vixl32::Register base,
                                      int32_t offset) {
  ___ Vstr(source, MemOperand(base, offset));
}

void ArmVIXLAssembler::StoreDToOffset(vixl32::DRegister source,
                                      vixl32::Register base,
                                      int32_t offset) {
  ___ Vstr(source, MemOperand(base, offset));
}

void ArmVIXLAssembler::LoadSFromOffset(vixl32::SRegister reg,
                                       vixl32::Register base,
                                       int32_t offset) {
  ___ Vldr(reg, MemOperand(base, offset));
}

void ArmVIXLAssembler::LoadDFromOffset(vixl32::DRegister reg,
                                       vixl32::Register base,
                                       int32_t offset) {
  ___ Vldr(reg, MemOperand(base, offset));
}

// Prefer Str to Add/Stm in ArmVIXLAssembler::StoreRegisterList and
// ArmVIXLAssembler::LoadRegisterList where this generates less code (size).
static constexpr int kRegListThreshold = 4;

void ArmVIXLAssembler::StoreRegisterList(RegList regs, size_t stack_offset) {
  int number_of_regs = POPCOUNT(static_cast<uint32_t>(regs));
  if (number_of_regs != 0) {
    if (number_of_regs > kRegListThreshold) {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register base = sp;
      if (stack_offset != 0) {
        base = temps.Acquire();
        DCHECK_EQ(regs & (1u << base.GetCode()), 0u);
        ___ Add(base, sp, Operand::From(stack_offset));
      }
      ___ Stm(base, NO_WRITE_BACK, RegisterList(regs));
    } else {
      for (uint32_t i : LowToHighBits(static_cast<uint32_t>(regs))) {
        ___ Str(vixl32::Register(i), MemOperand(sp, stack_offset));
        stack_offset += kRegSizeInBytes;
      }
    }
  }
}

void ArmVIXLAssembler::LoadRegisterList(RegList regs, size_t stack_offset) {
  int number_of_regs = POPCOUNT(static_cast<uint32_t>(regs));
  if (number_of_regs != 0) {
    if (number_of_regs > kRegListThreshold) {
      UseScratchRegisterScope temps(GetVIXLAssembler());
      vixl32::Register base = sp;
      if (stack_offset != 0) {
        base = temps.Acquire();
        ___ Add(base, sp, Operand::From(stack_offset));
      }
      ___ Ldm(base, NO_WRITE_BACK, RegisterList(regs));
    } else {
      for (uint32_t i : LowToHighBits(static_cast<uint32_t>(regs))) {
        ___ Ldr(vixl32::Register(i), MemOperand(sp, stack_offset));
        stack_offset += kRegSizeInBytes;
      }
    }
  }
}

void ArmVIXLAssembler::AddConstant(vixl32::Register rd, int32_t value) {
  AddConstant(rd, rd, value);
}

// TODO(VIXL): think about using adds which updates flags where possible.
void ArmVIXLAssembler::AddConstant(vixl32::Register rd,
                                   vixl32::Register rn,
                                   int32_t value) {
  DCHECK(vixl_masm_.OutsideITBlock());
  // TODO(VIXL): implement this optimization in VIXL.
  if (value == 0) {
    if (!rd.Is(rn)) {
      ___ Mov(rd, rn);
    }
    return;
  }
  ___ Add(rd, rn, value);
}

// Inside IT block we must use assembler, macroassembler instructions are not permitted.
void ArmVIXLAssembler::AddConstantInIt(vixl32::Register rd,
                                       vixl32::Register rn,
                                       int32_t value,
                                       vixl32::Condition cond) {
  DCHECK(vixl_masm_.InITBlock());
  if (value == 0) {
    ___ mov(cond, rd, rn);
  } else {
    ___ add(cond, rd, rn, value);
  }
}

void ArmVIXLMacroAssembler::CompareAndBranchIfZero(vixl32::Register rn,
                                                   vixl32::Label* label,
                                                   bool is_far_target) {
  if (!is_far_target && rn.IsLow() && !label->IsBound()) {
    // In T32, Cbz/Cbnz instructions have following limitations:
    // - There are only 7 bits (i:imm5:0) to encode branch target address (cannot be far target).
    // - Only low registers (i.e R0 .. R7) can be encoded.
    // - Only forward branches (unbound labels) are supported.
    Cbz(rn, label);
    return;
  }
  Cmp(rn, 0);
  B(eq, label, is_far_target);
}

void ArmVIXLMacroAssembler::CompareAndBranchIfNonZero(vixl32::Register rn,
                                                      vixl32::Label* label,
                                                      bool is_far_target) {
  if (!is_far_target && rn.IsLow() && !label->IsBound()) {
    Cbnz(rn, label);
    return;
  }
  Cmp(rn, 0);
  B(ne, label, is_far_target);
}

void ArmVIXLMacroAssembler::B(vixl32::Label* label) {
  if (!label->IsBound()) {
    // Try to use a 16-bit encoding of the B instruction.
    DCHECK(OutsideITBlock());
    BPreferNear(label);
    return;
  }
  MacroAssembler::B(label);
}

void ArmVIXLMacroAssembler::B(vixl32::Condition cond, vixl32::Label* label, bool is_far_target) {
  if (!label->IsBound() && !is_far_target) {
    // Try to use a 16-bit encoding of the B instruction.
    DCHECK(OutsideITBlock());
    BPreferNear(cond, label);
    return;
  }
  MacroAssembler::B(cond, label);
}

}  // namespace arm
}  // namespace art
