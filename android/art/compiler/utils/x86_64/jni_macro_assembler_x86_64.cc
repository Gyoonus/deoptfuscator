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

#include "jni_macro_assembler_x86_64.h"

#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "memory_region.h"
#include "thread.h"

namespace art {
namespace x86_64 {

static dwarf::Reg DWARFReg(Register reg) {
  return dwarf::Reg::X86_64Core(static_cast<int>(reg));
}
static dwarf::Reg DWARFReg(FloatRegister reg) {
  return dwarf::Reg::X86_64Fp(static_cast<int>(reg));
}

constexpr size_t kFramePointerSize = 8;

#define __ asm_.

void X86_64JNIMacroAssembler::BuildFrame(size_t frame_size,
                                         ManagedRegister method_reg,
                                         ArrayRef<const ManagedRegister> spill_regs,
                                         const ManagedRegisterEntrySpills& entry_spills) {
  DCHECK_EQ(CodeSize(), 0U);  // Nothing emitted yet.
  cfi().SetCurrentCFAOffset(8);  // Return address on stack.
  CHECK_ALIGNED(frame_size, kStackAlignment);
  int gpr_count = 0;
  for (int i = spill_regs.size() - 1; i >= 0; --i) {
    x86_64::X86_64ManagedRegister spill = spill_regs[i].AsX86_64();
    if (spill.IsCpuRegister()) {
      __ pushq(spill.AsCpuRegister());
      gpr_count++;
      cfi().AdjustCFAOffset(kFramePointerSize);
      cfi().RelOffset(DWARFReg(spill.AsCpuRegister().AsRegister()), 0);
    }
  }
  // return address then method on stack.
  int64_t rest_of_frame = static_cast<int64_t>(frame_size)
                          - (gpr_count * kFramePointerSize)
                          - kFramePointerSize /*return address*/;
  __ subq(CpuRegister(RSP), Immediate(rest_of_frame));
  cfi().AdjustCFAOffset(rest_of_frame);

  // spill xmms
  int64_t offset = rest_of_frame;
  for (int i = spill_regs.size() - 1; i >= 0; --i) {
    x86_64::X86_64ManagedRegister spill = spill_regs[i].AsX86_64();
    if (spill.IsXmmRegister()) {
      offset -= sizeof(double);
      __ movsd(Address(CpuRegister(RSP), offset), spill.AsXmmRegister());
      cfi().RelOffset(DWARFReg(spill.AsXmmRegister().AsFloatRegister()), offset);
    }
  }

  static_assert(static_cast<size_t>(kX86_64PointerSize) == kFramePointerSize,
                "Unexpected frame pointer size.");

  __ movq(Address(CpuRegister(RSP), 0), method_reg.AsX86_64().AsCpuRegister());

  for (size_t i = 0; i < entry_spills.size(); ++i) {
    ManagedRegisterSpill spill = entry_spills.at(i);
    if (spill.AsX86_64().IsCpuRegister()) {
      if (spill.getSize() == 8) {
        __ movq(Address(CpuRegister(RSP), frame_size + spill.getSpillOffset()),
                spill.AsX86_64().AsCpuRegister());
      } else {
        CHECK_EQ(spill.getSize(), 4);
        __ movl(Address(CpuRegister(RSP), frame_size + spill.getSpillOffset()),
                spill.AsX86_64().AsCpuRegister());
      }
    } else {
      if (spill.getSize() == 8) {
        __ movsd(Address(CpuRegister(RSP), frame_size + spill.getSpillOffset()),
                 spill.AsX86_64().AsXmmRegister());
      } else {
        CHECK_EQ(spill.getSize(), 4);
        __ movss(Address(CpuRegister(RSP), frame_size + spill.getSpillOffset()),
                 spill.AsX86_64().AsXmmRegister());
      }
    }
  }
}

void X86_64JNIMacroAssembler::RemoveFrame(size_t frame_size,
                                          ArrayRef<const ManagedRegister> spill_regs,
                                          bool may_suspend ATTRIBUTE_UNUSED) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  cfi().RememberState();
  int gpr_count = 0;
  // unspill xmms
  int64_t offset = static_cast<int64_t>(frame_size)
      - (spill_regs.size() * kFramePointerSize)
      - 2 * kFramePointerSize;
  for (size_t i = 0; i < spill_regs.size(); ++i) {
    x86_64::X86_64ManagedRegister spill = spill_regs[i].AsX86_64();
    if (spill.IsXmmRegister()) {
      offset += sizeof(double);
      __ movsd(spill.AsXmmRegister(), Address(CpuRegister(RSP), offset));
      cfi().Restore(DWARFReg(spill.AsXmmRegister().AsFloatRegister()));
    } else {
      gpr_count++;
    }
  }
  int adjust = static_cast<int>(frame_size) - (gpr_count * kFramePointerSize) - kFramePointerSize;
  __ addq(CpuRegister(RSP), Immediate(adjust));
  cfi().AdjustCFAOffset(-adjust);
  for (size_t i = 0; i < spill_regs.size(); ++i) {
    x86_64::X86_64ManagedRegister spill = spill_regs[i].AsX86_64();
    if (spill.IsCpuRegister()) {
      __ popq(spill.AsCpuRegister());
      cfi().AdjustCFAOffset(-static_cast<int>(kFramePointerSize));
      cfi().Restore(DWARFReg(spill.AsCpuRegister().AsRegister()));
    }
  }
  __ ret();
  // The CFI should be restored for any code that follows the exit block.
  cfi().RestoreState();
  cfi().DefCFAOffset(frame_size);
}

void X86_64JNIMacroAssembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  __ addq(CpuRegister(RSP), Immediate(-static_cast<int64_t>(adjust)));
  cfi().AdjustCFAOffset(adjust);
}

static void DecreaseFrameSizeImpl(size_t adjust, X86_64Assembler* assembler) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  assembler->addq(CpuRegister(RSP), Immediate(adjust));
  assembler->cfi().AdjustCFAOffset(-adjust);
}

void X86_64JNIMacroAssembler::DecreaseFrameSize(size_t adjust) {
  DecreaseFrameSizeImpl(adjust, &asm_);
}

void X86_64JNIMacroAssembler::Store(FrameOffset offs, ManagedRegister msrc, size_t size) {
  X86_64ManagedRegister src = msrc.AsX86_64();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCpuRegister()) {
    if (size == 4) {
      CHECK_EQ(4u, size);
      __ movl(Address(CpuRegister(RSP), offs), src.AsCpuRegister());
    } else {
      CHECK_EQ(8u, size);
      __ movq(Address(CpuRegister(RSP), offs), src.AsCpuRegister());
    }
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(0u, size);
    __ movq(Address(CpuRegister(RSP), offs), src.AsRegisterPairLow());
    __ movq(Address(CpuRegister(RSP), FrameOffset(offs.Int32Value()+4)),
            src.AsRegisterPairHigh());
  } else if (src.IsX87Register()) {
    if (size == 4) {
      __ fstps(Address(CpuRegister(RSP), offs));
    } else {
      __ fstpl(Address(CpuRegister(RSP), offs));
    }
  } else {
    CHECK(src.IsXmmRegister());
    if (size == 4) {
      __ movss(Address(CpuRegister(RSP), offs), src.AsXmmRegister());
    } else {
      __ movsd(Address(CpuRegister(RSP), offs), src.AsXmmRegister());
    }
  }
}

void X86_64JNIMacroAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  X86_64ManagedRegister src = msrc.AsX86_64();
  CHECK(src.IsCpuRegister());
  __ movl(Address(CpuRegister(RSP), dest), src.AsCpuRegister());
}

void X86_64JNIMacroAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  X86_64ManagedRegister src = msrc.AsX86_64();
  CHECK(src.IsCpuRegister());
  __ movq(Address(CpuRegister(RSP), dest), src.AsCpuRegister());
}

void X86_64JNIMacroAssembler::StoreImmediateToFrame(FrameOffset dest,
                                                    uint32_t imm,
                                                    ManagedRegister) {
  __ movl(Address(CpuRegister(RSP), dest), Immediate(imm));  // TODO(64) movq?
}

void X86_64JNIMacroAssembler::StoreStackOffsetToThread(ThreadOffset64 thr_offs,
                                                       FrameOffset fr_offs,
                                                       ManagedRegister mscratch) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  CHECK(scratch.IsCpuRegister());
  __ leaq(scratch.AsCpuRegister(), Address(CpuRegister(RSP), fr_offs));
  __ gs()->movq(Address::Absolute(thr_offs, true), scratch.AsCpuRegister());
}

void X86_64JNIMacroAssembler::StoreStackPointerToThread(ThreadOffset64 thr_offs) {
  __ gs()->movq(Address::Absolute(thr_offs, true), CpuRegister(RSP));
}

void X86_64JNIMacroAssembler::StoreSpanning(FrameOffset /*dst*/,
                                            ManagedRegister /*src*/,
                                            FrameOffset /*in_off*/,
                                            ManagedRegister /*scratch*/) {
  UNIMPLEMENTED(FATAL);  // this case only currently exists for ARM
}

void X86_64JNIMacroAssembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  X86_64ManagedRegister dest = mdest.AsX86_64();
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (dest.IsCpuRegister()) {
    if (size == 4) {
      CHECK_EQ(4u, size);
      __ movl(dest.AsCpuRegister(), Address(CpuRegister(RSP), src));
    } else {
      CHECK_EQ(8u, size);
      __ movq(dest.AsCpuRegister(), Address(CpuRegister(RSP), src));
    }
  } else if (dest.IsRegisterPair()) {
    CHECK_EQ(0u, size);
    __ movq(dest.AsRegisterPairLow(), Address(CpuRegister(RSP), src));
    __ movq(dest.AsRegisterPairHigh(), Address(CpuRegister(RSP), FrameOffset(src.Int32Value()+4)));
  } else if (dest.IsX87Register()) {
    if (size == 4) {
      __ flds(Address(CpuRegister(RSP), src));
    } else {
      __ fldl(Address(CpuRegister(RSP), src));
    }
  } else {
    CHECK(dest.IsXmmRegister());
    if (size == 4) {
      __ movss(dest.AsXmmRegister(), Address(CpuRegister(RSP), src));
    } else {
      __ movsd(dest.AsXmmRegister(), Address(CpuRegister(RSP), src));
    }
  }
}

void X86_64JNIMacroAssembler::LoadFromThread(ManagedRegister mdest,
                                             ThreadOffset64 src, size_t size) {
  X86_64ManagedRegister dest = mdest.AsX86_64();
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (dest.IsCpuRegister()) {
    if (size == 1u) {
      __ gs()->movzxb(dest.AsCpuRegister(), Address::Absolute(src, true));
    } else {
      CHECK_EQ(4u, size);
      __ gs()->movl(dest.AsCpuRegister(), Address::Absolute(src, true));
    }
  } else if (dest.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    __ gs()->movq(dest.AsRegisterPairLow(), Address::Absolute(src, true));
  } else if (dest.IsX87Register()) {
    if (size == 4) {
      __ gs()->flds(Address::Absolute(src, true));
    } else {
      __ gs()->fldl(Address::Absolute(src, true));
    }
  } else {
    CHECK(dest.IsXmmRegister());
    if (size == 4) {
      __ gs()->movss(dest.AsXmmRegister(), Address::Absolute(src, true));
    } else {
      __ gs()->movsd(dest.AsXmmRegister(), Address::Absolute(src, true));
    }
  }
}

void X86_64JNIMacroAssembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  X86_64ManagedRegister dest = mdest.AsX86_64();
  CHECK(dest.IsCpuRegister());
  __ movq(dest.AsCpuRegister(), Address(CpuRegister(RSP), src));
}

void X86_64JNIMacroAssembler::LoadRef(ManagedRegister mdest,
                                      ManagedRegister mbase,
                                      MemberOffset offs,
                                      bool unpoison_reference) {
  X86_64ManagedRegister base = mbase.AsX86_64();
  X86_64ManagedRegister dest = mdest.AsX86_64();
  CHECK(base.IsCpuRegister());
  CHECK(dest.IsCpuRegister());
  __ movl(dest.AsCpuRegister(), Address(base.AsCpuRegister(), offs));
  if (unpoison_reference) {
    __ MaybeUnpoisonHeapReference(dest.AsCpuRegister());
  }
}

void X86_64JNIMacroAssembler::LoadRawPtr(ManagedRegister mdest,
                                         ManagedRegister mbase,
                                         Offset offs) {
  X86_64ManagedRegister base = mbase.AsX86_64();
  X86_64ManagedRegister dest = mdest.AsX86_64();
  CHECK(base.IsCpuRegister());
  CHECK(dest.IsCpuRegister());
  __ movq(dest.AsCpuRegister(), Address(base.AsCpuRegister(), offs));
}

void X86_64JNIMacroAssembler::LoadRawPtrFromThread(ManagedRegister mdest, ThreadOffset64 offs) {
  X86_64ManagedRegister dest = mdest.AsX86_64();
  CHECK(dest.IsCpuRegister());
  __ gs()->movq(dest.AsCpuRegister(), Address::Absolute(offs, true));
}

void X86_64JNIMacroAssembler::SignExtend(ManagedRegister mreg, size_t size) {
  X86_64ManagedRegister reg = mreg.AsX86_64();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsCpuRegister()) << reg;
  if (size == 1) {
    __ movsxb(reg.AsCpuRegister(), reg.AsCpuRegister());
  } else {
    __ movsxw(reg.AsCpuRegister(), reg.AsCpuRegister());
  }
}

void X86_64JNIMacroAssembler::ZeroExtend(ManagedRegister mreg, size_t size) {
  X86_64ManagedRegister reg = mreg.AsX86_64();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsCpuRegister()) << reg;
  if (size == 1) {
    __ movzxb(reg.AsCpuRegister(), reg.AsCpuRegister());
  } else {
    __ movzxw(reg.AsCpuRegister(), reg.AsCpuRegister());
  }
}

void X86_64JNIMacroAssembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) {
  X86_64ManagedRegister dest = mdest.AsX86_64();
  X86_64ManagedRegister src = msrc.AsX86_64();
  if (!dest.Equals(src)) {
    if (dest.IsCpuRegister() && src.IsCpuRegister()) {
      __ movq(dest.AsCpuRegister(), src.AsCpuRegister());
    } else if (src.IsX87Register() && dest.IsXmmRegister()) {
      // Pass via stack and pop X87 register
      __ subl(CpuRegister(RSP), Immediate(16));
      if (size == 4) {
        CHECK_EQ(src.AsX87Register(), ST0);
        __ fstps(Address(CpuRegister(RSP), 0));
        __ movss(dest.AsXmmRegister(), Address(CpuRegister(RSP), 0));
      } else {
        CHECK_EQ(src.AsX87Register(), ST0);
        __ fstpl(Address(CpuRegister(RSP), 0));
        __ movsd(dest.AsXmmRegister(), Address(CpuRegister(RSP), 0));
      }
      __ addq(CpuRegister(RSP), Immediate(16));
    } else {
      // TODO: x87, SSE
      UNIMPLEMENTED(FATAL) << ": Move " << dest << ", " << src;
    }
  }
}

void X86_64JNIMacroAssembler::CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister mscratch) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  CHECK(scratch.IsCpuRegister());
  __ movl(scratch.AsCpuRegister(), Address(CpuRegister(RSP), src));
  __ movl(Address(CpuRegister(RSP), dest), scratch.AsCpuRegister());
}

void X86_64JNIMacroAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                                   ThreadOffset64 thr_offs,
                                                   ManagedRegister mscratch) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  CHECK(scratch.IsCpuRegister());
  __ gs()->movq(scratch.AsCpuRegister(), Address::Absolute(thr_offs, true));
  Store(fr_offs, scratch, 8);
}

void X86_64JNIMacroAssembler::CopyRawPtrToThread(ThreadOffset64 thr_offs,
                                                 FrameOffset fr_offs,
                                                 ManagedRegister mscratch) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  CHECK(scratch.IsCpuRegister());
  Load(scratch, fr_offs, 8);
  __ gs()->movq(Address::Absolute(thr_offs, true), scratch.AsCpuRegister());
}

void X86_64JNIMacroAssembler::Copy(FrameOffset dest,
                                   FrameOffset src,
                                   ManagedRegister mscratch,
                                   size_t size) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  if (scratch.IsCpuRegister() && size == 8) {
    Load(scratch, src, 4);
    Store(dest, scratch, 4);
    Load(scratch, FrameOffset(src.Int32Value() + 4), 4);
    Store(FrameOffset(dest.Int32Value() + 4), scratch, 4);
  } else {
    Load(scratch, src, size);
    Store(dest, scratch, size);
  }
}

void X86_64JNIMacroAssembler::Copy(FrameOffset /*dst*/,
                                   ManagedRegister /*src_base*/,
                                   Offset /*src_offset*/,
                                   ManagedRegister /*scratch*/,
                                   size_t /*size*/) {
  UNIMPLEMENTED(FATAL);
}

void X86_64JNIMacroAssembler::Copy(ManagedRegister dest_base,
                                   Offset dest_offset,
                                   FrameOffset src,
                                   ManagedRegister scratch,
                                   size_t size) {
  CHECK(scratch.IsNoRegister());
  CHECK_EQ(size, 4u);
  __ pushq(Address(CpuRegister(RSP), src));
  __ popq(Address(dest_base.AsX86_64().AsCpuRegister(), dest_offset));
}

void X86_64JNIMacroAssembler::Copy(FrameOffset dest,
                                   FrameOffset src_base,
                                   Offset src_offset,
                                   ManagedRegister mscratch,
                                   size_t size) {
  CpuRegister scratch = mscratch.AsX86_64().AsCpuRegister();
  CHECK_EQ(size, 4u);
  __ movq(scratch, Address(CpuRegister(RSP), src_base));
  __ movq(scratch, Address(scratch, src_offset));
  __ movq(Address(CpuRegister(RSP), dest), scratch);
}

void X86_64JNIMacroAssembler::Copy(ManagedRegister dest,
                                   Offset dest_offset,
                                   ManagedRegister src,
                                   Offset src_offset,
                                   ManagedRegister scratch,
                                   size_t size) {
  CHECK_EQ(size, 4u);
  CHECK(scratch.IsNoRegister());
  __ pushq(Address(src.AsX86_64().AsCpuRegister(), src_offset));
  __ popq(Address(dest.AsX86_64().AsCpuRegister(), dest_offset));
}

void X86_64JNIMacroAssembler::Copy(FrameOffset dest,
                                   Offset dest_offset,
                                   FrameOffset src,
                                   Offset src_offset,
                                   ManagedRegister mscratch,
                                   size_t size) {
  CpuRegister scratch = mscratch.AsX86_64().AsCpuRegister();
  CHECK_EQ(size, 4u);
  CHECK_EQ(dest.Int32Value(), src.Int32Value());
  __ movq(scratch, Address(CpuRegister(RSP), src));
  __ pushq(Address(scratch, src_offset));
  __ popq(Address(scratch, dest_offset));
}

void X86_64JNIMacroAssembler::MemoryBarrier(ManagedRegister) {
  __ mfence();
}

void X86_64JNIMacroAssembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                                     FrameOffset handle_scope_offset,
                                                     ManagedRegister min_reg,
                                                     bool null_allowed) {
  X86_64ManagedRegister out_reg = mout_reg.AsX86_64();
  X86_64ManagedRegister in_reg = min_reg.AsX86_64();
  if (in_reg.IsNoRegister()) {  // TODO(64): && null_allowed
    // Use out_reg as indicator of null.
    in_reg = out_reg;
    // TODO: movzwl
    __ movl(in_reg.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
  }
  CHECK(in_reg.IsCpuRegister());
  CHECK(out_reg.IsCpuRegister());
  VerifyObject(in_reg, null_allowed);
  if (null_allowed) {
    Label null_arg;
    if (!out_reg.Equals(in_reg)) {
      __ xorl(out_reg.AsCpuRegister(), out_reg.AsCpuRegister());
    }
    __ testl(in_reg.AsCpuRegister(), in_reg.AsCpuRegister());
    __ j(kZero, &null_arg);
    __ leaq(out_reg.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
    __ Bind(&null_arg);
  } else {
    __ leaq(out_reg.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
  }
}

void X86_64JNIMacroAssembler::CreateHandleScopeEntry(FrameOffset out_off,
                                                     FrameOffset handle_scope_offset,
                                                     ManagedRegister mscratch,
                                                     bool null_allowed) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  CHECK(scratch.IsCpuRegister());
  if (null_allowed) {
    Label null_arg;
    __ movl(scratch.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
    __ testl(scratch.AsCpuRegister(), scratch.AsCpuRegister());
    __ j(kZero, &null_arg);
    __ leaq(scratch.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
    __ Bind(&null_arg);
  } else {
    __ leaq(scratch.AsCpuRegister(), Address(CpuRegister(RSP), handle_scope_offset));
  }
  Store(out_off, scratch, 8);
}

// Given a handle scope entry, load the associated reference.
void X86_64JNIMacroAssembler::LoadReferenceFromHandleScope(ManagedRegister mout_reg,
                                                           ManagedRegister min_reg) {
  X86_64ManagedRegister out_reg = mout_reg.AsX86_64();
  X86_64ManagedRegister in_reg = min_reg.AsX86_64();
  CHECK(out_reg.IsCpuRegister());
  CHECK(in_reg.IsCpuRegister());
  Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    __ xorl(out_reg.AsCpuRegister(), out_reg.AsCpuRegister());
  }
  __ testl(in_reg.AsCpuRegister(), in_reg.AsCpuRegister());
  __ j(kZero, &null_arg);
  __ movq(out_reg.AsCpuRegister(), Address(in_reg.AsCpuRegister(), 0));
  __ Bind(&null_arg);
}

void X86_64JNIMacroAssembler::VerifyObject(ManagedRegister /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void X86_64JNIMacroAssembler::VerifyObject(FrameOffset /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void X86_64JNIMacroAssembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister) {
  X86_64ManagedRegister base = mbase.AsX86_64();
  CHECK(base.IsCpuRegister());
  __ call(Address(base.AsCpuRegister(), offset.Int32Value()));
  // TODO: place reference map on call
}

void X86_64JNIMacroAssembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  CpuRegister scratch = mscratch.AsX86_64().AsCpuRegister();
  __ movq(scratch, Address(CpuRegister(RSP), base));
  __ call(Address(scratch, offset));
}

void X86_64JNIMacroAssembler::CallFromThread(ThreadOffset64 offset, ManagedRegister /*mscratch*/) {
  __ gs()->call(Address::Absolute(offset, true));
}

void X86_64JNIMacroAssembler::GetCurrentThread(ManagedRegister tr) {
  __ gs()->movq(tr.AsX86_64().AsCpuRegister(),
                Address::Absolute(Thread::SelfOffset<kX86_64PointerSize>(), true));
}

void X86_64JNIMacroAssembler::GetCurrentThread(FrameOffset offset, ManagedRegister mscratch) {
  X86_64ManagedRegister scratch = mscratch.AsX86_64();
  __ gs()->movq(scratch.AsCpuRegister(),
                Address::Absolute(Thread::SelfOffset<kX86_64PointerSize>(), true));
  __ movq(Address(CpuRegister(RSP), offset), scratch.AsCpuRegister());
}

// Slowpath entered when Thread::Current()->_exception is non-null
class X86_64ExceptionSlowPath FINAL : public SlowPath {
 public:
  explicit X86_64ExceptionSlowPath(size_t stack_adjust) : stack_adjust_(stack_adjust) {}
  virtual void Emit(Assembler *sp_asm) OVERRIDE;
 private:
  const size_t stack_adjust_;
};

void X86_64JNIMacroAssembler::ExceptionPoll(ManagedRegister /*scratch*/, size_t stack_adjust) {
  X86_64ExceptionSlowPath* slow = new (__ GetAllocator()) X86_64ExceptionSlowPath(stack_adjust);
  __ GetBuffer()->EnqueueSlowPath(slow);
  __ gs()->cmpl(Address::Absolute(Thread::ExceptionOffset<kX86_64PointerSize>(), true),
                Immediate(0));
  __ j(kNotEqual, slow->Entry());
}

std::unique_ptr<JNIMacroLabel> X86_64JNIMacroAssembler::CreateLabel() {
  return std::unique_ptr<JNIMacroLabel>(new X86_64JNIMacroLabel());
}

void X86_64JNIMacroAssembler::Jump(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  __ jmp(X86_64JNIMacroLabel::Cast(label)->AsX86_64());
}

void X86_64JNIMacroAssembler::Jump(JNIMacroLabel* label,
                                   JNIMacroUnaryCondition condition,
                                   ManagedRegister test) {
  CHECK(label != nullptr);

  art::x86_64::Condition x86_64_cond;
  switch (condition) {
    case JNIMacroUnaryCondition::kZero:
      x86_64_cond = art::x86_64::kZero;
      break;
    case JNIMacroUnaryCondition::kNotZero:
      x86_64_cond = art::x86_64::kNotZero;
      break;
    default:
      LOG(FATAL) << "Not implemented condition: " << static_cast<int>(condition);
      UNREACHABLE();
  }

  // TEST reg, reg
  // Jcc <Offset>
  __ testq(test.AsX86_64().AsCpuRegister(), test.AsX86_64().AsCpuRegister());
  __ j(x86_64_cond, X86_64JNIMacroLabel::Cast(label)->AsX86_64());
}

void X86_64JNIMacroAssembler::Bind(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  __ Bind(X86_64JNIMacroLabel::Cast(label)->AsX86_64());
}

#undef __

void X86_64ExceptionSlowPath::Emit(Assembler *sasm) {
  X86_64Assembler* sp_asm = down_cast<X86_64Assembler*>(sasm);
#define __ sp_asm->
  __ Bind(&entry_);
  // Note: the return value is dead
  if (stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSizeImpl(stack_adjust_, sp_asm);
  }
  // Pass exception as argument in RDI
  __ gs()->movq(CpuRegister(RDI),
                Address::Absolute(Thread::ExceptionOffset<kX86_64PointerSize>(), true));
  __ gs()->call(
      Address::Absolute(QUICK_ENTRYPOINT_OFFSET(kX86_64PointerSize, pDeliverException), true));
  // this call should never return
  __ int3();
#undef __
}

}  // namespace x86_64
}  // namespace art
