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

#include "jni_macro_assembler_arm_vixl.h"

#include <iostream>
#include <type_traits>

#include "entrypoints/quick/quick_entrypoints.h"
#include "thread.h"

using namespace vixl::aarch32;  // NOLINT(build/namespaces)
namespace vixl32 = vixl::aarch32;

using vixl::ExactAssemblyScope;
using vixl::CodeBufferCheckScope;

namespace art {
namespace arm {

#ifdef ___
#error "ARM Assembler macro already defined."
#else
#define ___   asm_.GetVIXLAssembler()->
#endif

void ArmVIXLJNIMacroAssembler::FinalizeCode() {
  for (const std::unique_ptr<
      ArmVIXLJNIMacroAssembler::ArmException>& exception : exception_blocks_) {
    EmitExceptionPoll(exception.get());
  }
  asm_.FinalizeCode();
}

static dwarf::Reg DWARFReg(vixl32::Register reg) {
  return dwarf::Reg::ArmCore(static_cast<int>(reg.GetCode()));
}

static dwarf::Reg DWARFReg(vixl32::SRegister reg) {
  return dwarf::Reg::ArmFp(static_cast<int>(reg.GetCode()));
}

static constexpr size_t kFramePointerSize = static_cast<size_t>(kArmPointerSize);

void ArmVIXLJNIMacroAssembler::BuildFrame(size_t frame_size,
                                          ManagedRegister method_reg,
                                          ArrayRef<const ManagedRegister> callee_save_regs,
                                          const ManagedRegisterEntrySpills& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  CHECK(r0.Is(method_reg.AsArm().AsVIXLRegister()));

  // Push callee saves and link register.
  RegList core_spill_mask = 1 << LR;
  uint32_t fp_spill_mask = 0;
  for (const ManagedRegister& reg : callee_save_regs) {
    if (reg.AsArm().IsCoreRegister()) {
      core_spill_mask |= 1 << reg.AsArm().AsCoreRegister();
    } else {
      fp_spill_mask |= 1 << reg.AsArm().AsSRegister();
    }
  }
  ___ Push(RegisterList(core_spill_mask));
  cfi().AdjustCFAOffset(POPCOUNT(core_spill_mask) * kFramePointerSize);
  cfi().RelOffsetForMany(DWARFReg(r0), 0, core_spill_mask, kFramePointerSize);
  if (fp_spill_mask != 0) {
    uint32_t first = CTZ(fp_spill_mask);

    // Check that list is contiguous.
    DCHECK_EQ(fp_spill_mask >> CTZ(fp_spill_mask), ~0u >> (32 - POPCOUNT(fp_spill_mask)));

    ___ Vpush(SRegisterList(vixl32::SRegister(first), POPCOUNT(fp_spill_mask)));
    cfi().AdjustCFAOffset(POPCOUNT(fp_spill_mask) * kFramePointerSize);
    cfi().RelOffsetForMany(DWARFReg(s0), 0, fp_spill_mask, kFramePointerSize);
  }

  // Increase frame to required size.
  int pushed_values = POPCOUNT(core_spill_mask) + POPCOUNT(fp_spill_mask);
  // Must at least have space for Method*.
  CHECK_GT(frame_size, pushed_values * kFramePointerSize);
  IncreaseFrameSize(frame_size - pushed_values * kFramePointerSize);  // handles CFI as well.

  // Write out Method*.
  asm_.StoreToOffset(kStoreWord, r0, sp, 0);

  // Write out entry spills.
  int32_t offset = frame_size + kFramePointerSize;
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    ArmManagedRegister reg = entry_spills.at(i).AsArm();
    if (reg.IsNoRegister()) {
      // only increment stack offset.
      ManagedRegisterSpill spill = entry_spills.at(i);
      offset += spill.getSize();
    } else if (reg.IsCoreRegister()) {
      asm_.StoreToOffset(kStoreWord, reg.AsVIXLRegister(), sp, offset);
      offset += 4;
    } else if (reg.IsSRegister()) {
      asm_.StoreSToOffset(reg.AsVIXLSRegister(), sp, offset);
      offset += 4;
    } else if (reg.IsDRegister()) {
      asm_.StoreDToOffset(reg.AsVIXLDRegister(), sp, offset);
      offset += 8;
    }
  }
}

void ArmVIXLJNIMacroAssembler::RemoveFrame(size_t frame_size,
                                           ArrayRef<const ManagedRegister> callee_save_regs,
                                           bool may_suspend) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  cfi().RememberState();

  // Compute callee saves to pop and LR.
  RegList core_spill_mask = 1 << LR;
  uint32_t fp_spill_mask = 0;
  for (const ManagedRegister& reg : callee_save_regs) {
    if (reg.AsArm().IsCoreRegister()) {
      core_spill_mask |= 1 << reg.AsArm().AsCoreRegister();
    } else {
      fp_spill_mask |= 1 << reg.AsArm().AsSRegister();
    }
  }

  // Decrease frame to start of callee saves.
  int pop_values = POPCOUNT(core_spill_mask) + POPCOUNT(fp_spill_mask);
  CHECK_GT(frame_size, pop_values * kFramePointerSize);
  DecreaseFrameSize(frame_size - (pop_values * kFramePointerSize));  // handles CFI as well.

  // Pop FP callee saves.
  if (fp_spill_mask != 0) {
    uint32_t first = CTZ(fp_spill_mask);
    // Check that list is contiguous.
     DCHECK_EQ(fp_spill_mask >> CTZ(fp_spill_mask), ~0u >> (32 - POPCOUNT(fp_spill_mask)));

    ___ Vpop(SRegisterList(vixl32::SRegister(first), POPCOUNT(fp_spill_mask)));
    cfi().AdjustCFAOffset(-kFramePointerSize * POPCOUNT(fp_spill_mask));
    cfi().RestoreMany(DWARFReg(s0), fp_spill_mask);
  }

  // Pop core callee saves and LR.
  ___ Pop(RegisterList(core_spill_mask));

  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    if (may_suspend) {
      // The method may be suspended; refresh the Marking Register.
      ___ Ldr(mr, MemOperand(tr, Thread::IsGcMarkingOffset<kArmPointerSize>().Int32Value()));
    } else {
      // The method shall not be suspended; no need to refresh the Marking Register.

      // Check that the Marking Register is a callee-save register,
      // and thus has been preserved by native code following the
      // AAPCS calling convention.
      DCHECK_NE(core_spill_mask & (1 << MR), 0)
          << "core_spill_mask should contain Marking Register R" << MR;

      // The following condition is a compile-time one, so it does not have a run-time cost.
      if (kIsDebugBuild) {
        // The following condition is a run-time one; it is executed after the
        // previous compile-time test, to avoid penalizing non-debug builds.
        if (emit_run_time_checks_in_debug_mode_) {
          // Emit a run-time check verifying that the Marking Register is up-to-date.
          UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
          vixl32::Register temp = temps.Acquire();
          // Ensure we are not clobbering a callee-save register that was restored before.
          DCHECK_EQ(core_spill_mask & (1 << temp.GetCode()), 0)
              << "core_spill_mask hould not contain scratch register R" << temp.GetCode();
          asm_.GenerateMarkingRegisterCheck(temp);
        }
      }
    }
  }

  // Return to LR.
  ___ Bx(vixl32::lr);

  // The CFI should be restored for any code that follows the exit block.
  cfi().RestoreState();
  cfi().DefCFAOffset(frame_size);
}


void ArmVIXLJNIMacroAssembler::IncreaseFrameSize(size_t adjust) {
  asm_.AddConstant(sp, -adjust);
  cfi().AdjustCFAOffset(adjust);
}

void ArmVIXLJNIMacroAssembler::DecreaseFrameSize(size_t adjust) {
  asm_.AddConstant(sp, adjust);
  cfi().AdjustCFAOffset(-adjust);
}

void ArmVIXLJNIMacroAssembler::Store(FrameOffset dest, ManagedRegister m_src, size_t size) {
  ArmManagedRegister src = m_src.AsArm();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCoreRegister()) {
    CHECK_EQ(4u, size);
    UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
    temps.Exclude(src.AsVIXLRegister());
    asm_.StoreToOffset(kStoreWord, src.AsVIXLRegister(), sp, dest.Int32Value());
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    asm_.StoreToOffset(kStoreWord, src.AsVIXLRegisterPairLow(),  sp, dest.Int32Value());
    asm_.StoreToOffset(kStoreWord, src.AsVIXLRegisterPairHigh(), sp, dest.Int32Value() + 4);
  } else if (src.IsSRegister()) {
    CHECK_EQ(4u, size);
    asm_.StoreSToOffset(src.AsVIXLSRegister(), sp, dest.Int32Value());
  } else {
    CHECK_EQ(8u, size);
    CHECK(src.IsDRegister()) << src;
    asm_.StoreDToOffset(src.AsVIXLDRegister(), sp, dest.Int32Value());
  }
}

void ArmVIXLJNIMacroAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  ArmManagedRegister src = msrc.AsArm();
  CHECK(src.IsCoreRegister()) << src;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(src.AsVIXLRegister());
  asm_.StoreToOffset(kStoreWord, src.AsVIXLRegister(), sp, dest.Int32Value());
}

void ArmVIXLJNIMacroAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  ArmManagedRegister src = msrc.AsArm();
  CHECK(src.IsCoreRegister()) << src;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(src.AsVIXLRegister());
  asm_.StoreToOffset(kStoreWord, src.AsVIXLRegister(), sp, dest.Int32Value());
}

void ArmVIXLJNIMacroAssembler::StoreSpanning(FrameOffset dest,
                                             ManagedRegister msrc,
                                             FrameOffset in_off,
                                             ManagedRegister mscratch) {
  ArmManagedRegister src = msrc.AsArm();
  ArmManagedRegister scratch = mscratch.AsArm();
  asm_.StoreToOffset(kStoreWord, src.AsVIXLRegister(), sp, dest.Int32Value());
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord, scratch.AsVIXLRegister(), sp, in_off.Int32Value());
  asm_.StoreToOffset(kStoreWord, scratch.AsVIXLRegister(), sp, dest.Int32Value() + 4);
}

void ArmVIXLJNIMacroAssembler::CopyRef(FrameOffset dest,
                                       FrameOffset src,
                                       ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord, scratch.AsVIXLRegister(), sp, src.Int32Value());
  asm_.StoreToOffset(kStoreWord, scratch.AsVIXLRegister(), sp, dest.Int32Value());
}

void ArmVIXLJNIMacroAssembler::LoadRef(ManagedRegister dest,
                                       ManagedRegister base,
                                       MemberOffset offs,
                                       bool unpoison_reference) {
  ArmManagedRegister dst = dest.AsArm();
  CHECK(dst.IsCoreRegister() && dst.IsCoreRegister()) << dst;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(dst.AsVIXLRegister(), base.AsArm().AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord,
                      dst.AsVIXLRegister(),
                      base.AsArm().AsVIXLRegister(),
                      offs.Int32Value());

  if (unpoison_reference) {
    asm_.MaybeUnpoisonHeapReference(dst.AsVIXLRegister());
  }
}

void ArmVIXLJNIMacroAssembler::LoadRef(ManagedRegister dest ATTRIBUTE_UNUSED,
                                       FrameOffset src ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::LoadRawPtr(ManagedRegister dest ATTRIBUTE_UNUSED,
                                          ManagedRegister base ATTRIBUTE_UNUSED,
                                          Offset offs ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::StoreImmediateToFrame(FrameOffset dest,
                                                     uint32_t imm,
                                                     ManagedRegister scratch) {
  ArmManagedRegister mscratch = scratch.AsArm();
  CHECK(mscratch.IsCoreRegister()) << mscratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(mscratch.AsVIXLRegister());
  asm_.LoadImmediate(mscratch.AsVIXLRegister(), imm);
  asm_.StoreToOffset(kStoreWord, mscratch.AsVIXLRegister(), sp, dest.Int32Value());
}

void ArmVIXLJNIMacroAssembler::Load(ManagedRegister m_dst, FrameOffset src, size_t size) {
  return Load(m_dst.AsArm(), sp, src.Int32Value(), size);
}

void ArmVIXLJNIMacroAssembler::LoadFromThread(ManagedRegister m_dst,
                                              ThreadOffset32 src,
                                              size_t size) {
  return Load(m_dst.AsArm(), tr, src.Int32Value(), size);
}

void ArmVIXLJNIMacroAssembler::LoadRawPtrFromThread(ManagedRegister m_dst, ThreadOffset32 offs) {
  ArmManagedRegister dst = m_dst.AsArm();
  CHECK(dst.IsCoreRegister()) << dst;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(dst.AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord, dst.AsVIXLRegister(), tr, offs.Int32Value());
}

void ArmVIXLJNIMacroAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                                    ThreadOffset32 thr_offs,
                                                    ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord, scratch.AsVIXLRegister(), tr, thr_offs.Int32Value());
  asm_.StoreToOffset(kStoreWord, scratch.AsVIXLRegister(), sp, fr_offs.Int32Value());
}

void ArmVIXLJNIMacroAssembler::CopyRawPtrToThread(ThreadOffset32 thr_offs ATTRIBUTE_UNUSED,
                                                  FrameOffset fr_offs ATTRIBUTE_UNUSED,
                                                  ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::StoreStackOffsetToThread(ThreadOffset32 thr_offs,
                                                        FrameOffset fr_offs,
                                                        ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  asm_.AddConstant(scratch.AsVIXLRegister(), sp, fr_offs.Int32Value());
  asm_.StoreToOffset(kStoreWord, scratch.AsVIXLRegister(), tr, thr_offs.Int32Value());
}

void ArmVIXLJNIMacroAssembler::StoreStackPointerToThread(ThreadOffset32 thr_offs) {
  asm_.StoreToOffset(kStoreWord, sp, tr, thr_offs.Int32Value());
}

void ArmVIXLJNIMacroAssembler::SignExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                          size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no sign extension necessary for arm";
}

void ArmVIXLJNIMacroAssembler::ZeroExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                          size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no zero extension necessary for arm";
}

void ArmVIXLJNIMacroAssembler::Move(ManagedRegister m_dst,
                                    ManagedRegister m_src,
                                    size_t size  ATTRIBUTE_UNUSED) {
  ArmManagedRegister dst = m_dst.AsArm();
  ArmManagedRegister src = m_src.AsArm();
  if (!dst.Equals(src)) {
    if (dst.IsCoreRegister()) {
      CHECK(src.IsCoreRegister()) << src;
      UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
      temps.Exclude(dst.AsVIXLRegister());
      ___ Mov(dst.AsVIXLRegister(), src.AsVIXLRegister());
    } else if (dst.IsDRegister()) {
      if (src.IsDRegister()) {
        ___ Vmov(F64, dst.AsVIXLDRegister(), src.AsVIXLDRegister());
      } else {
        // VMOV Dn, Rlo, Rhi (Dn = {Rlo, Rhi})
        CHECK(src.IsRegisterPair()) << src;
        ___ Vmov(dst.AsVIXLDRegister(), src.AsVIXLRegisterPairLow(), src.AsVIXLRegisterPairHigh());
      }
    } else if (dst.IsSRegister()) {
      if (src.IsSRegister()) {
        ___ Vmov(F32, dst.AsVIXLSRegister(), src.AsVIXLSRegister());
      } else {
        // VMOV Sn, Rn  (Sn = Rn)
        CHECK(src.IsCoreRegister()) << src;
        ___ Vmov(dst.AsVIXLSRegister(), src.AsVIXLRegister());
      }
    } else {
      CHECK(dst.IsRegisterPair()) << dst;
      CHECK(src.IsRegisterPair()) << src;
      // Ensure that the first move doesn't clobber the input of the second.
      if (src.AsRegisterPairHigh() != dst.AsRegisterPairLow()) {
        ___ Mov(dst.AsVIXLRegisterPairLow(),  src.AsVIXLRegisterPairLow());
        ___ Mov(dst.AsVIXLRegisterPairHigh(), src.AsVIXLRegisterPairHigh());
      } else {
        ___ Mov(dst.AsVIXLRegisterPairHigh(), src.AsVIXLRegisterPairHigh());
        ___ Mov(dst.AsVIXLRegisterPairLow(),  src.AsVIXLRegisterPairLow());
      }
    }
  }
}

void ArmVIXLJNIMacroAssembler::Copy(FrameOffset dest,
                                    FrameOffset src,
                                    ManagedRegister scratch,
                                    size_t size) {
  ArmManagedRegister temp = scratch.AsArm();
  CHECK(temp.IsCoreRegister()) << temp;
  CHECK(size == 4 || size == 8) << size;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(temp.AsVIXLRegister());
  if (size == 4) {
    asm_.LoadFromOffset(kLoadWord, temp.AsVIXLRegister(), sp, src.Int32Value());
    asm_.StoreToOffset(kStoreWord, temp.AsVIXLRegister(), sp, dest.Int32Value());
  } else if (size == 8) {
    asm_.LoadFromOffset(kLoadWord, temp.AsVIXLRegister(), sp, src.Int32Value());
    asm_.StoreToOffset(kStoreWord, temp.AsVIXLRegister(), sp, dest.Int32Value());
    asm_.LoadFromOffset(kLoadWord, temp.AsVIXLRegister(), sp, src.Int32Value() + 4);
    asm_.StoreToOffset(kStoreWord, temp.AsVIXLRegister(), sp, dest.Int32Value() + 4);
  }
}

void ArmVIXLJNIMacroAssembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                                    ManagedRegister src_base ATTRIBUTE_UNUSED,
                                    Offset src_offset ATTRIBUTE_UNUSED,
                                    ManagedRegister mscratch ATTRIBUTE_UNUSED,
                                    size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::Copy(ManagedRegister dest_base ATTRIBUTE_UNUSED,
                                    Offset dest_offset ATTRIBUTE_UNUSED,
                                    FrameOffset src ATTRIBUTE_UNUSED,
                                    ManagedRegister mscratch ATTRIBUTE_UNUSED,
                                    size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::Copy(FrameOffset dst ATTRIBUTE_UNUSED,
                                    FrameOffset src_base ATTRIBUTE_UNUSED,
                                    Offset src_offset ATTRIBUTE_UNUSED,
                                    ManagedRegister mscratch ATTRIBUTE_UNUSED,
                                    size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::Copy(ManagedRegister dest ATTRIBUTE_UNUSED,
                                    Offset dest_offset ATTRIBUTE_UNUSED,
                                    ManagedRegister src ATTRIBUTE_UNUSED,
                                    Offset src_offset ATTRIBUTE_UNUSED,
                                    ManagedRegister mscratch ATTRIBUTE_UNUSED,
                                    size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::Copy(FrameOffset dst ATTRIBUTE_UNUSED,
                                    Offset dest_offset ATTRIBUTE_UNUSED,
                                    FrameOffset src ATTRIBUTE_UNUSED,
                                    Offset src_offset ATTRIBUTE_UNUSED,
                                    ManagedRegister scratch ATTRIBUTE_UNUSED,
                                    size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                                      FrameOffset handle_scope_offset,
                                                      ManagedRegister min_reg,
                                                      bool null_allowed) {
  ArmManagedRegister out_reg = mout_reg.AsArm();
  ArmManagedRegister in_reg = min_reg.AsArm();
  CHECK(in_reg.IsNoRegister() || in_reg.IsCoreRegister()) << in_reg;
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(out_reg.AsVIXLRegister());
  if (null_allowed) {
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      asm_.LoadFromOffset(kLoadWord,
                          out_reg.AsVIXLRegister(),
                          sp,
                          handle_scope_offset.Int32Value());
      in_reg = out_reg;
    }

    temps.Exclude(in_reg.AsVIXLRegister());
    ___ Cmp(in_reg.AsVIXLRegister(), 0);

    if (asm_.ShifterOperandCanHold(ADD, handle_scope_offset.Int32Value())) {
      if (!out_reg.Equals(in_reg)) {
        ExactAssemblyScope guard(asm_.GetVIXLAssembler(),
                                 3 * vixl32::kMaxInstructionSizeInBytes,
                                 CodeBufferCheckScope::kMaximumSize);
        ___ it(eq, 0xc);
        ___ mov(eq, out_reg.AsVIXLRegister(), 0);
        asm_.AddConstantInIt(out_reg.AsVIXLRegister(), sp, handle_scope_offset.Int32Value(), ne);
      } else {
        ExactAssemblyScope guard(asm_.GetVIXLAssembler(),
                                 2 * vixl32::kMaxInstructionSizeInBytes,
                                 CodeBufferCheckScope::kMaximumSize);
        ___ it(ne, 0x8);
        asm_.AddConstantInIt(out_reg.AsVIXLRegister(), sp, handle_scope_offset.Int32Value(), ne);
      }
    } else {
      // TODO: Implement this (old arm assembler would have crashed here).
      UNIMPLEMENTED(FATAL);
    }
  } else {
    asm_.AddConstant(out_reg.AsVIXLRegister(), sp, handle_scope_offset.Int32Value());
  }
}

void ArmVIXLJNIMacroAssembler::CreateHandleScopeEntry(FrameOffset out_off,
                                                      FrameOffset handle_scope_offset,
                                                      ManagedRegister mscratch,
                                                      bool null_allowed) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  if (null_allowed) {
    asm_.LoadFromOffset(kLoadWord, scratch.AsVIXLRegister(), sp, handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset)
    ___ Cmp(scratch.AsVIXLRegister(), 0);

    if (asm_.ShifterOperandCanHold(ADD, handle_scope_offset.Int32Value())) {
      ExactAssemblyScope guard(asm_.GetVIXLAssembler(),
                               2 * vixl32::kMaxInstructionSizeInBytes,
                               CodeBufferCheckScope::kMaximumSize);
      ___ it(ne, 0x8);
      asm_.AddConstantInIt(scratch.AsVIXLRegister(), sp, handle_scope_offset.Int32Value(), ne);
    } else {
      // TODO: Implement this (old arm assembler would have crashed here).
      UNIMPLEMENTED(FATAL);
    }
  } else {
    asm_.AddConstant(scratch.AsVIXLRegister(), sp, handle_scope_offset.Int32Value());
  }
  asm_.StoreToOffset(kStoreWord, scratch.AsVIXLRegister(), sp, out_off.Int32Value());
}

void ArmVIXLJNIMacroAssembler::LoadReferenceFromHandleScope(
    ManagedRegister mout_reg ATTRIBUTE_UNUSED,
    ManagedRegister min_reg ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::VerifyObject(ManagedRegister src ATTRIBUTE_UNUSED,
                                            bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references.
}

void ArmVIXLJNIMacroAssembler::VerifyObject(FrameOffset src ATTRIBUTE_UNUSED,
                                            bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references.
}

void ArmVIXLJNIMacroAssembler::Call(ManagedRegister mbase,
                                    Offset offset,
                                    ManagedRegister mscratch) {
  ArmManagedRegister base = mbase.AsArm();
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(base.IsCoreRegister()) << base;
  CHECK(scratch.IsCoreRegister()) << scratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  asm_.LoadFromOffset(kLoadWord,
                      scratch.AsVIXLRegister(),
                      base.AsVIXLRegister(),
                      offset.Int32Value());
  ___ Blx(scratch.AsVIXLRegister());
  // TODO: place reference map on call.
}

void ArmVIXLJNIMacroAssembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  ArmManagedRegister scratch = mscratch.AsArm();
  CHECK(scratch.IsCoreRegister()) << scratch;
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  // Call *(*(SP + base) + offset)
  asm_.LoadFromOffset(kLoadWord, scratch.AsVIXLRegister(), sp, base.Int32Value());
  asm_.LoadFromOffset(kLoadWord,
                      scratch.AsVIXLRegister(),
                      scratch.AsVIXLRegister(),
                      offset.Int32Value());
  ___ Blx(scratch.AsVIXLRegister());
  // TODO: place reference map on call
}

void ArmVIXLJNIMacroAssembler::CallFromThread(ThreadOffset32 offset ATTRIBUTE_UNUSED,
                                              ManagedRegister scratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::GetCurrentThread(ManagedRegister mtr) {
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(mtr.AsArm().AsVIXLRegister());
  ___ Mov(mtr.AsArm().AsVIXLRegister(), tr);
}

void ArmVIXLJNIMacroAssembler::GetCurrentThread(FrameOffset dest_offset,
                                                ManagedRegister scratch ATTRIBUTE_UNUSED) {
  asm_.StoreToOffset(kStoreWord, tr, sp, dest_offset.Int32Value());
}

void ArmVIXLJNIMacroAssembler::ExceptionPoll(ManagedRegister m_scratch, size_t stack_adjust) {
  CHECK_ALIGNED(stack_adjust, kStackAlignment);
  ArmManagedRegister scratch = m_scratch.AsArm();
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(scratch.AsVIXLRegister());
  exception_blocks_.emplace_back(
      new ArmVIXLJNIMacroAssembler::ArmException(scratch, stack_adjust));
  asm_.LoadFromOffset(kLoadWord,
                      scratch.AsVIXLRegister(),
                      tr,
                      Thread::ExceptionOffset<kArmPointerSize>().Int32Value());

  ___ Cmp(scratch.AsVIXLRegister(), 0);
  vixl32::Label* label = exception_blocks_.back()->Entry();
  ___ BPreferNear(ne, label);
  // TODO: think about using CBNZ here.
}

std::unique_ptr<JNIMacroLabel> ArmVIXLJNIMacroAssembler::CreateLabel() {
  return std::unique_ptr<JNIMacroLabel>(new ArmVIXLJNIMacroLabel());
}

void ArmVIXLJNIMacroAssembler::Jump(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  ___ B(ArmVIXLJNIMacroLabel::Cast(label)->AsArm());
}

void ArmVIXLJNIMacroAssembler::Jump(JNIMacroLabel* label,
                                    JNIMacroUnaryCondition condition,
                                    ManagedRegister test) {
  CHECK(label != nullptr);

  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(test.AsArm().AsVIXLRegister());
  switch (condition) {
    case JNIMacroUnaryCondition::kZero:
      ___ CompareAndBranchIfZero(test.AsArm().AsVIXLRegister(),
                                 ArmVIXLJNIMacroLabel::Cast(label)->AsArm());
      break;
    case JNIMacroUnaryCondition::kNotZero:
      ___ CompareAndBranchIfNonZero(test.AsArm().AsVIXLRegister(),
                                    ArmVIXLJNIMacroLabel::Cast(label)->AsArm());
      break;
    default:
      LOG(FATAL) << "Not implemented unary condition: " << static_cast<int>(condition);
      UNREACHABLE();
  }
}

void ArmVIXLJNIMacroAssembler::Bind(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  ___ Bind(ArmVIXLJNIMacroLabel::Cast(label)->AsArm());
}

void ArmVIXLJNIMacroAssembler::EmitExceptionPoll(
    ArmVIXLJNIMacroAssembler::ArmException* exception) {
  ___ Bind(exception->Entry());
  if (exception->stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSize(exception->stack_adjust_);
  }

  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(exception->scratch_.AsVIXLRegister());
  // Pass exception object as argument.
  // Don't care about preserving r0 as this won't return.
  ___ Mov(r0, exception->scratch_.AsVIXLRegister());
  temps.Include(exception->scratch_.AsVIXLRegister());
  // TODO: check that exception->scratch_ is dead by this point.
  vixl32::Register temp = temps.Acquire();
  ___ Ldr(temp,
          MemOperand(tr,
              QUICK_ENTRYPOINT_OFFSET(kArmPointerSize, pDeliverException).Int32Value()));
  ___ Blx(temp);
}

void ArmVIXLJNIMacroAssembler::MemoryBarrier(ManagedRegister scratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL);
}

void ArmVIXLJNIMacroAssembler::Load(ArmManagedRegister
                                    dest,
                                    vixl32::Register base,
                                    int32_t offset,
                                    size_t size) {
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size) << dest;
  } else if (dest.IsCoreRegister()) {
    CHECK(!dest.AsVIXLRegister().Is(sp)) << dest;

    UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
    temps.Exclude(dest.AsVIXLRegister());

    if (size == 1u) {
      ___ Ldrb(dest.AsVIXLRegister(), MemOperand(base, offset));
    } else {
      CHECK_EQ(4u, size) << dest;
      ___ Ldr(dest.AsVIXLRegister(), MemOperand(base, offset));
    }
  } else if (dest.IsRegisterPair()) {
    CHECK_EQ(8u, size) << dest;
    ___ Ldr(dest.AsVIXLRegisterPairLow(),  MemOperand(base, offset));
    ___ Ldr(dest.AsVIXLRegisterPairHigh(), MemOperand(base, offset + 4));
  } else if (dest.IsSRegister()) {
    ___ Vldr(dest.AsVIXLSRegister(), MemOperand(base, offset));
  } else {
    CHECK(dest.IsDRegister()) << dest;
    ___ Vldr(dest.AsVIXLDRegister(), MemOperand(base, offset));
  }
}

}  // namespace arm
}  // namespace art
