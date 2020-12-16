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

#include "assembler_arm64.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "heap_poisoning.h"
#include "offsets.h"
#include "thread.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

namespace art {
namespace arm64 {

#ifdef ___
#error "ARM64 Assembler macro already defined."
#else
#define ___   vixl_masm_.
#endif

void Arm64Assembler::FinalizeCode() {
  ___ FinalizeCode();
}

size_t Arm64Assembler::CodeSize() const {
  return vixl_masm_.GetSizeOfCodeGenerated();
}

const uint8_t* Arm64Assembler::CodeBufferBaseAddress() const {
  return vixl_masm_.GetBuffer().GetStartAddress<const uint8_t*>();
}

void Arm64Assembler::FinalizeInstructions(const MemoryRegion& region) {
  // Copy the instructions from the buffer.
  MemoryRegion from(vixl_masm_.GetBuffer()->GetStartAddress<void*>(), CodeSize());
  region.CopyFrom(0, from);
}

void Arm64Assembler::LoadRawPtr(ManagedRegister m_dst, ManagedRegister m_base, Offset offs) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  Arm64ManagedRegister base = m_base.AsArm64();
  CHECK(dst.IsXRegister() && base.IsXRegister());
  // Remove dst and base form the temp list - higher level API uses IP1, IP0.
  UseScratchRegisterScope temps(&vixl_masm_);
  temps.Exclude(reg_x(dst.AsXRegister()), reg_x(base.AsXRegister()));
  ___ Ldr(reg_x(dst.AsXRegister()), MEM_OP(reg_x(base.AsXRegister()), offs.Int32Value()));
}

void Arm64Assembler::JumpTo(ManagedRegister m_base, Offset offs, ManagedRegister m_scratch) {
  Arm64ManagedRegister base = m_base.AsArm64();
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(base.IsXRegister()) << base;
  CHECK(scratch.IsXRegister()) << scratch;
  // Remove base and scratch form the temp list - higher level API uses IP1, IP0.
  UseScratchRegisterScope temps(&vixl_masm_);
  temps.Exclude(reg_x(base.AsXRegister()), reg_x(scratch.AsXRegister()));
  ___ Ldr(reg_x(scratch.AsXRegister()), MEM_OP(reg_x(base.AsXRegister()), offs.Int32Value()));
  ___ Br(reg_x(scratch.AsXRegister()));
}

static inline dwarf::Reg DWARFReg(CPURegister reg) {
  if (reg.IsFPRegister()) {
    return dwarf::Reg::Arm64Fp(reg.GetCode());
  } else {
    DCHECK_LT(reg.GetCode(), 31u);  // X0 - X30.
    return dwarf::Reg::Arm64Core(reg.GetCode());
  }
}

void Arm64Assembler::SpillRegisters(CPURegList registers, int offset) {
  int size = registers.GetRegisterSizeInBytes();
  const Register sp = vixl_masm_.StackPointer();
  // Since we are operating on register pairs, we would like to align on
  // double the standard size; on the other hand, we don't want to insert
  // an extra store, which will happen if the number of registers is even.
  if (!IsAlignedParam(offset, 2 * size) && registers.GetCount() % 2 != 0) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    ___ Str(dst0, MemOperand(sp, offset));
    cfi_.RelOffset(DWARFReg(dst0), offset);
    offset += size;
  }
  while (registers.GetCount() >= 2) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    ___ Stp(dst0, dst1, MemOperand(sp, offset));
    cfi_.RelOffset(DWARFReg(dst0), offset);
    cfi_.RelOffset(DWARFReg(dst1), offset + size);
    offset += 2 * size;
  }
  if (!registers.IsEmpty()) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    ___ Str(dst0, MemOperand(sp, offset));
    cfi_.RelOffset(DWARFReg(dst0), offset);
  }
  DCHECK(registers.IsEmpty());
}

void Arm64Assembler::UnspillRegisters(CPURegList registers, int offset) {
  int size = registers.GetRegisterSizeInBytes();
  const Register sp = vixl_masm_.StackPointer();
  // Be consistent with the logic for spilling registers.
  if (!IsAlignedParam(offset, 2 * size) && registers.GetCount() % 2 != 0) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    ___ Ldr(dst0, MemOperand(sp, offset));
    cfi_.Restore(DWARFReg(dst0));
    offset += size;
  }
  while (registers.GetCount() >= 2) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    ___ Ldp(dst0, dst1, MemOperand(sp, offset));
    cfi_.Restore(DWARFReg(dst0));
    cfi_.Restore(DWARFReg(dst1));
    offset += 2 * size;
  }
  if (!registers.IsEmpty()) {
    const CPURegister& dst0 = registers.PopLowestIndex();
    ___ Ldr(dst0, MemOperand(sp, offset));
    cfi_.Restore(DWARFReg(dst0));
  }
  DCHECK(registers.IsEmpty());
}

void Arm64Assembler::PoisonHeapReference(Register reg) {
  DCHECK(reg.IsW());
  // reg = -reg.
  ___ Neg(reg, Operand(reg));
}

void Arm64Assembler::UnpoisonHeapReference(Register reg) {
  DCHECK(reg.IsW());
  // reg = -reg.
  ___ Neg(reg, Operand(reg));
}

void Arm64Assembler::MaybePoisonHeapReference(Register reg) {
  if (kPoisonHeapReferences) {
    PoisonHeapReference(reg);
  }
}

void Arm64Assembler::MaybeUnpoisonHeapReference(Register reg) {
  if (kPoisonHeapReferences) {
    UnpoisonHeapReference(reg);
  }
}

void Arm64Assembler::GenerateMarkingRegisterCheck(Register temp, int code) {
  // The Marking Register is only used in the Baker read barrier configuration.
  DCHECK(kEmitCompilerReadBarrier);
  DCHECK(kUseBakerReadBarrier);

  vixl::aarch64::Register mr = reg_x(MR);  // Marking Register.
  vixl::aarch64::Register tr = reg_x(TR);  // Thread Register.
  vixl::aarch64::Label mr_is_ok;

  // temp = self.tls32_.is.gc_marking
  ___ Ldr(temp, MemOperand(tr, Thread::IsGcMarkingOffset<kArm64PointerSize>().Int32Value()));
  // Check that mr == self.tls32_.is.gc_marking.
  ___ Cmp(mr.W(), temp);
  ___ B(eq, &mr_is_ok);
  ___ Brk(code);
  ___ Bind(&mr_is_ok);
}

#undef ___

}  // namespace arm64
}  // namespace art
