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

#include "jni_macro_assembler_arm64.h"

#include "entrypoints/quick/quick_entrypoints.h"
#include "managed_register_arm64.h"
#include "offsets.h"
#include "thread.h"

using namespace vixl::aarch64;  // NOLINT(build/namespaces)

namespace art {
namespace arm64 {

#ifdef ___
#error "ARM64 Assembler macro already defined."
#else
#define ___   asm_.GetVIXLAssembler()->
#endif

#define reg_x(X) Arm64Assembler::reg_x(X)
#define reg_w(W) Arm64Assembler::reg_w(W)
#define reg_d(D) Arm64Assembler::reg_d(D)
#define reg_s(S) Arm64Assembler::reg_s(S)

Arm64JNIMacroAssembler::~Arm64JNIMacroAssembler() {
}

void Arm64JNIMacroAssembler::FinalizeCode() {
  for (const std::unique_ptr<Arm64Exception>& exception : exception_blocks_) {
    EmitExceptionPoll(exception.get());
  }
  ___ FinalizeCode();
}

void Arm64JNIMacroAssembler::GetCurrentThread(ManagedRegister tr) {
  ___ Mov(reg_x(tr.AsArm64().AsXRegister()), reg_x(TR));
}

void Arm64JNIMacroAssembler::GetCurrentThread(FrameOffset offset, ManagedRegister /* scratch */) {
  StoreToOffset(TR, SP, offset.Int32Value());
}

// See Arm64 PCS Section 5.2.2.1.
void Arm64JNIMacroAssembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  AddConstant(SP, -adjust);
  cfi().AdjustCFAOffset(adjust);
}

// See Arm64 PCS Section 5.2.2.1.
void Arm64JNIMacroAssembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  AddConstant(SP, adjust);
  cfi().AdjustCFAOffset(-adjust);
}

void Arm64JNIMacroAssembler::AddConstant(XRegister rd, int32_t value, Condition cond) {
  AddConstant(rd, rd, value, cond);
}

void Arm64JNIMacroAssembler::AddConstant(XRegister rd,
                                         XRegister rn,
                                         int32_t value,
                                         Condition cond) {
  if ((cond == al) || (cond == nv)) {
    // VIXL macro-assembler handles all variants.
    ___ Add(reg_x(rd), reg_x(rn), value);
  } else {
    // temp = rd + value
    // rd = cond ? temp : rn
    UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
    temps.Exclude(reg_x(rd), reg_x(rn));
    Register temp = temps.AcquireX();
    ___ Add(temp, reg_x(rn), value);
    ___ Csel(reg_x(rd), temp, reg_x(rd), cond);
  }
}

void Arm64JNIMacroAssembler::StoreWToOffset(StoreOperandType type,
                                            WRegister source,
                                            XRegister base,
                                            int32_t offset) {
  switch (type) {
    case kStoreByte:
      ___ Strb(reg_w(source), MEM_OP(reg_x(base), offset));
      break;
    case kStoreHalfword:
      ___ Strh(reg_w(source), MEM_OP(reg_x(base), offset));
      break;
    case kStoreWord:
      ___ Str(reg_w(source), MEM_OP(reg_x(base), offset));
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

void Arm64JNIMacroAssembler::StoreToOffset(XRegister source, XRegister base, int32_t offset) {
  CHECK_NE(source, SP);
  ___ Str(reg_x(source), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::StoreSToOffset(SRegister source, XRegister base, int32_t offset) {
  ___ Str(reg_s(source), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::StoreDToOffset(DRegister source, XRegister base, int32_t offset) {
  ___ Str(reg_d(source), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::Store(FrameOffset offs, ManagedRegister m_src, size_t size) {
  Arm64ManagedRegister src = m_src.AsArm64();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsWRegister()) {
    CHECK_EQ(4u, size);
    StoreWToOffset(kStoreWord, src.AsWRegister(), SP, offs.Int32Value());
  } else if (src.IsXRegister()) {
    CHECK_EQ(8u, size);
    StoreToOffset(src.AsXRegister(), SP, offs.Int32Value());
  } else if (src.IsSRegister()) {
    StoreSToOffset(src.AsSRegister(), SP, offs.Int32Value());
  } else {
    CHECK(src.IsDRegister()) << src;
    StoreDToOffset(src.AsDRegister(), SP, offs.Int32Value());
  }
}

void Arm64JNIMacroAssembler::StoreRef(FrameOffset offs, ManagedRegister m_src) {
  Arm64ManagedRegister src = m_src.AsArm64();
  CHECK(src.IsXRegister()) << src;
  StoreWToOffset(kStoreWord, src.AsOverlappingWRegister(), SP,
                 offs.Int32Value());
}

void Arm64JNIMacroAssembler::StoreRawPtr(FrameOffset offs, ManagedRegister m_src) {
  Arm64ManagedRegister src = m_src.AsArm64();
  CHECK(src.IsXRegister()) << src;
  StoreToOffset(src.AsXRegister(), SP, offs.Int32Value());
}

void Arm64JNIMacroAssembler::StoreImmediateToFrame(FrameOffset offs,
                                                   uint32_t imm,
                                                   ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadImmediate(scratch.AsXRegister(), imm);
  StoreWToOffset(kStoreWord, scratch.AsOverlappingWRegister(), SP,
                 offs.Int32Value());
}

void Arm64JNIMacroAssembler::StoreStackOffsetToThread(ThreadOffset64 tr_offs,
                                                      FrameOffset fr_offs,
                                                      ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  AddConstant(scratch.AsXRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(scratch.AsXRegister(), TR, tr_offs.Int32Value());
}

void Arm64JNIMacroAssembler::StoreStackPointerToThread(ThreadOffset64 tr_offs) {
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  Register temp = temps.AcquireX();
  ___ Mov(temp, reg_x(SP));
  ___ Str(temp, MEM_OP(reg_x(TR), tr_offs.Int32Value()));
}

void Arm64JNIMacroAssembler::StoreSpanning(FrameOffset dest_off,
                                           ManagedRegister m_source,
                                           FrameOffset in_off,
                                           ManagedRegister m_scratch) {
  Arm64ManagedRegister source = m_source.AsArm64();
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  StoreToOffset(source.AsXRegister(), SP, dest_off.Int32Value());
  LoadFromOffset(scratch.AsXRegister(), SP, in_off.Int32Value());
  StoreToOffset(scratch.AsXRegister(), SP, dest_off.Int32Value() + 8);
}

// Load routines.
void Arm64JNIMacroAssembler::LoadImmediate(XRegister dest, int32_t value, Condition cond) {
  if ((cond == al) || (cond == nv)) {
    ___ Mov(reg_x(dest), value);
  } else {
    // temp = value
    // rd = cond ? temp : rd
    if (value != 0) {
      UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
      temps.Exclude(reg_x(dest));
      Register temp = temps.AcquireX();
      ___ Mov(temp, value);
      ___ Csel(reg_x(dest), temp, reg_x(dest), cond);
    } else {
      ___ Csel(reg_x(dest), reg_x(XZR), reg_x(dest), cond);
    }
  }
}

void Arm64JNIMacroAssembler::LoadWFromOffset(LoadOperandType type,
                                             WRegister dest,
                                             XRegister base,
                                             int32_t offset) {
  switch (type) {
    case kLoadSignedByte:
      ___ Ldrsb(reg_w(dest), MEM_OP(reg_x(base), offset));
      break;
    case kLoadSignedHalfword:
      ___ Ldrsh(reg_w(dest), MEM_OP(reg_x(base), offset));
      break;
    case kLoadUnsignedByte:
      ___ Ldrb(reg_w(dest), MEM_OP(reg_x(base), offset));
      break;
    case kLoadUnsignedHalfword:
      ___ Ldrh(reg_w(dest), MEM_OP(reg_x(base), offset));
      break;
    case kLoadWord:
      ___ Ldr(reg_w(dest), MEM_OP(reg_x(base), offset));
      break;
    default:
        LOG(FATAL) << "UNREACHABLE";
  }
}

// Note: We can extend this member by adding load type info - see
// sign extended A64 load variants.
void Arm64JNIMacroAssembler::LoadFromOffset(XRegister dest, XRegister base, int32_t offset) {
  CHECK_NE(dest, SP);
  ___ Ldr(reg_x(dest), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::LoadSFromOffset(SRegister dest, XRegister base, int32_t offset) {
  ___ Ldr(reg_s(dest), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::LoadDFromOffset(DRegister dest, XRegister base, int32_t offset) {
  ___ Ldr(reg_d(dest), MEM_OP(reg_x(base), offset));
}

void Arm64JNIMacroAssembler::Load(Arm64ManagedRegister dest,
                                  XRegister base,
                                  int32_t offset,
                                  size_t size) {
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size) << dest;
  } else if (dest.IsWRegister()) {
    CHECK_EQ(4u, size) << dest;
    ___ Ldr(reg_w(dest.AsWRegister()), MEM_OP(reg_x(base), offset));
  } else if (dest.IsXRegister()) {
    CHECK_NE(dest.AsXRegister(), SP) << dest;

    if (size == 1u) {
      ___ Ldrb(reg_w(dest.AsOverlappingWRegister()), MEM_OP(reg_x(base), offset));
    } else if (size == 4u) {
      ___ Ldr(reg_w(dest.AsOverlappingWRegister()), MEM_OP(reg_x(base), offset));
    }  else {
      CHECK_EQ(8u, size) << dest;
      ___ Ldr(reg_x(dest.AsXRegister()), MEM_OP(reg_x(base), offset));
    }
  } else if (dest.IsSRegister()) {
    ___ Ldr(reg_s(dest.AsSRegister()), MEM_OP(reg_x(base), offset));
  } else {
    CHECK(dest.IsDRegister()) << dest;
    ___ Ldr(reg_d(dest.AsDRegister()), MEM_OP(reg_x(base), offset));
  }
}

void Arm64JNIMacroAssembler::Load(ManagedRegister m_dst, FrameOffset src, size_t size) {
  return Load(m_dst.AsArm64(), SP, src.Int32Value(), size);
}

void Arm64JNIMacroAssembler::LoadFromThread(ManagedRegister m_dst,
                                            ThreadOffset64 src,
                                            size_t size) {
  return Load(m_dst.AsArm64(), TR, src.Int32Value(), size);
}

void Arm64JNIMacroAssembler::LoadRef(ManagedRegister m_dst, FrameOffset offs) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  CHECK(dst.IsXRegister()) << dst;
  LoadWFromOffset(kLoadWord, dst.AsOverlappingWRegister(), SP, offs.Int32Value());
}

void Arm64JNIMacroAssembler::LoadRef(ManagedRegister m_dst,
                                     ManagedRegister m_base,
                                     MemberOffset offs,
                                     bool unpoison_reference) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  Arm64ManagedRegister base = m_base.AsArm64();
  CHECK(dst.IsXRegister() && base.IsXRegister());
  LoadWFromOffset(kLoadWord, dst.AsOverlappingWRegister(), base.AsXRegister(),
                  offs.Int32Value());
  if (unpoison_reference) {
    WRegister ref_reg = dst.AsOverlappingWRegister();
    asm_.MaybeUnpoisonHeapReference(reg_w(ref_reg));
  }
}

void Arm64JNIMacroAssembler::LoadRawPtr(ManagedRegister m_dst,
                                        ManagedRegister m_base,
                                        Offset offs) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  Arm64ManagedRegister base = m_base.AsArm64();
  CHECK(dst.IsXRegister() && base.IsXRegister());
  // Remove dst and base form the temp list - higher level API uses IP1, IP0.
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(reg_x(dst.AsXRegister()), reg_x(base.AsXRegister()));
  ___ Ldr(reg_x(dst.AsXRegister()), MEM_OP(reg_x(base.AsXRegister()), offs.Int32Value()));
}

void Arm64JNIMacroAssembler::LoadRawPtrFromThread(ManagedRegister m_dst, ThreadOffset64 offs) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  CHECK(dst.IsXRegister()) << dst;
  LoadFromOffset(dst.AsXRegister(), TR, offs.Int32Value());
}

// Copying routines.
void Arm64JNIMacroAssembler::Move(ManagedRegister m_dst, ManagedRegister m_src, size_t size) {
  Arm64ManagedRegister dst = m_dst.AsArm64();
  Arm64ManagedRegister src = m_src.AsArm64();
  if (!dst.Equals(src)) {
    if (dst.IsXRegister()) {
      if (size == 4) {
        CHECK(src.IsWRegister());
        ___ Mov(reg_w(dst.AsOverlappingWRegister()), reg_w(src.AsWRegister()));
      } else {
        if (src.IsXRegister()) {
          ___ Mov(reg_x(dst.AsXRegister()), reg_x(src.AsXRegister()));
        } else {
          ___ Mov(reg_x(dst.AsXRegister()), reg_x(src.AsOverlappingXRegister()));
        }
      }
    } else if (dst.IsWRegister()) {
      CHECK(src.IsWRegister()) << src;
      ___ Mov(reg_w(dst.AsWRegister()), reg_w(src.AsWRegister()));
    } else if (dst.IsSRegister()) {
      CHECK(src.IsSRegister()) << src;
      ___ Fmov(reg_s(dst.AsSRegister()), reg_s(src.AsSRegister()));
    } else {
      CHECK(dst.IsDRegister()) << dst;
      CHECK(src.IsDRegister()) << src;
      ___ Fmov(reg_d(dst.AsDRegister()), reg_d(src.AsDRegister()));
    }
  }
}

void Arm64JNIMacroAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                                  ThreadOffset64 tr_offs,
                                                  ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(scratch.AsXRegister(), TR, tr_offs.Int32Value());
  StoreToOffset(scratch.AsXRegister(), SP, fr_offs.Int32Value());
}

void Arm64JNIMacroAssembler::CopyRawPtrToThread(ThreadOffset64 tr_offs,
                                                FrameOffset fr_offs,
                                                ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(scratch.AsXRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(scratch.AsXRegister(), TR, tr_offs.Int32Value());
}

void Arm64JNIMacroAssembler::CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  LoadWFromOffset(kLoadWord, scratch.AsOverlappingWRegister(),
                  SP, src.Int32Value());
  StoreWToOffset(kStoreWord, scratch.AsOverlappingWRegister(),
                 SP, dest.Int32Value());
}

void Arm64JNIMacroAssembler::Copy(FrameOffset dest,
                                  FrameOffset src,
                                  ManagedRegister m_scratch,
                                  size_t size) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadWFromOffset(kLoadWord, scratch.AsOverlappingWRegister(), SP, src.Int32Value());
    StoreWToOffset(kStoreWord, scratch.AsOverlappingWRegister(), SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(scratch.AsXRegister(), SP, src.Int32Value());
    StoreToOffset(scratch.AsXRegister(), SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Arm64JNIMacroAssembler::Copy(FrameOffset dest,
                                  ManagedRegister src_base,
                                  Offset src_offset,
                                  ManagedRegister m_scratch,
                                  size_t size) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  Arm64ManagedRegister base = src_base.AsArm64();
  CHECK(base.IsXRegister()) << base;
  CHECK(scratch.IsXRegister() || scratch.IsWRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadWFromOffset(kLoadWord, scratch.AsWRegister(), base.AsXRegister(),
                   src_offset.Int32Value());
    StoreWToOffset(kStoreWord, scratch.AsWRegister(), SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(scratch.AsXRegister(), base.AsXRegister(), src_offset.Int32Value());
    StoreToOffset(scratch.AsXRegister(), SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Arm64JNIMacroAssembler::Copy(ManagedRegister m_dest_base,
                                  Offset dest_offs,
                                  FrameOffset src,
                                  ManagedRegister m_scratch,
                                  size_t size) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  Arm64ManagedRegister base = m_dest_base.AsArm64();
  CHECK(base.IsXRegister()) << base;
  CHECK(scratch.IsXRegister() || scratch.IsWRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadWFromOffset(kLoadWord, scratch.AsWRegister(), SP, src.Int32Value());
    StoreWToOffset(kStoreWord, scratch.AsWRegister(), base.AsXRegister(),
                   dest_offs.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(scratch.AsXRegister(), SP, src.Int32Value());
    StoreToOffset(scratch.AsXRegister(), base.AsXRegister(), dest_offs.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Arm64JNIMacroAssembler::Copy(FrameOffset /*dst*/,
                                  FrameOffset /*src_base*/,
                                  Offset /*src_offset*/,
                                  ManagedRegister /*mscratch*/,
                                  size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "Unimplemented Copy() variant";
}

void Arm64JNIMacroAssembler::Copy(ManagedRegister m_dest,
                                  Offset dest_offset,
                                  ManagedRegister m_src,
                                  Offset src_offset,
                                  ManagedRegister m_scratch,
                                  size_t size) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  Arm64ManagedRegister src = m_src.AsArm64();
  Arm64ManagedRegister dest = m_dest.AsArm64();
  CHECK(dest.IsXRegister()) << dest;
  CHECK(src.IsXRegister()) << src;
  CHECK(scratch.IsXRegister() || scratch.IsWRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    if (scratch.IsWRegister()) {
      LoadWFromOffset(kLoadWord, scratch.AsWRegister(), src.AsXRegister(),
                    src_offset.Int32Value());
      StoreWToOffset(kStoreWord, scratch.AsWRegister(), dest.AsXRegister(),
                   dest_offset.Int32Value());
    } else {
      LoadWFromOffset(kLoadWord, scratch.AsOverlappingWRegister(), src.AsXRegister(),
                    src_offset.Int32Value());
      StoreWToOffset(kStoreWord, scratch.AsOverlappingWRegister(), dest.AsXRegister(),
                   dest_offset.Int32Value());
    }
  } else if (size == 8) {
    LoadFromOffset(scratch.AsXRegister(), src.AsXRegister(), src_offset.Int32Value());
    StoreToOffset(scratch.AsXRegister(), dest.AsXRegister(), dest_offset.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Arm64JNIMacroAssembler::Copy(FrameOffset /*dst*/,
                                  Offset /*dest_offset*/,
                                  FrameOffset /*src*/,
                                  Offset /*src_offset*/,
                                  ManagedRegister /*scratch*/,
                                  size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "Unimplemented Copy() variant";
}

void Arm64JNIMacroAssembler::MemoryBarrier(ManagedRegister m_scratch ATTRIBUTE_UNUSED) {
  // TODO: Should we check that m_scratch is IP? - see arm.
  ___ Dmb(InnerShareable, BarrierAll);
}

void Arm64JNIMacroAssembler::SignExtend(ManagedRegister mreg, size_t size) {
  Arm64ManagedRegister reg = mreg.AsArm64();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsWRegister()) << reg;
  if (size == 1) {
    ___ Sxtb(reg_w(reg.AsWRegister()), reg_w(reg.AsWRegister()));
  } else {
    ___ Sxth(reg_w(reg.AsWRegister()), reg_w(reg.AsWRegister()));
  }
}

void Arm64JNIMacroAssembler::ZeroExtend(ManagedRegister mreg, size_t size) {
  Arm64ManagedRegister reg = mreg.AsArm64();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsWRegister()) << reg;
  if (size == 1) {
    ___ Uxtb(reg_w(reg.AsWRegister()), reg_w(reg.AsWRegister()));
  } else {
    ___ Uxth(reg_w(reg.AsWRegister()), reg_w(reg.AsWRegister()));
  }
}

void Arm64JNIMacroAssembler::VerifyObject(ManagedRegister /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references.
}

void Arm64JNIMacroAssembler::VerifyObject(FrameOffset /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references.
}

void Arm64JNIMacroAssembler::Call(ManagedRegister m_base, Offset offs, ManagedRegister m_scratch) {
  Arm64ManagedRegister base = m_base.AsArm64();
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(base.IsXRegister()) << base;
  CHECK(scratch.IsXRegister()) << scratch;
  LoadFromOffset(scratch.AsXRegister(), base.AsXRegister(), offs.Int32Value());
  ___ Blr(reg_x(scratch.AsXRegister()));
}

void Arm64JNIMacroAssembler::Call(FrameOffset base, Offset offs, ManagedRegister m_scratch) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(scratch.AsXRegister(), SP, base.Int32Value());
  LoadFromOffset(scratch.AsXRegister(), scratch.AsXRegister(), offs.Int32Value());
  ___ Blr(reg_x(scratch.AsXRegister()));
}

void Arm64JNIMacroAssembler::CallFromThread(ThreadOffset64 offset ATTRIBUTE_UNUSED,
                                            ManagedRegister scratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "Unimplemented Call() variant";
}

void Arm64JNIMacroAssembler::CreateHandleScopeEntry(ManagedRegister m_out_reg,
                                                    FrameOffset handle_scope_offs,
                                                    ManagedRegister m_in_reg,
                                                    bool null_allowed) {
  Arm64ManagedRegister out_reg = m_out_reg.AsArm64();
  Arm64ManagedRegister in_reg = m_in_reg.AsArm64();
  // For now we only hold stale handle scope entries in x registers.
  CHECK(in_reg.IsNoRegister() || in_reg.IsXRegister()) << in_reg;
  CHECK(out_reg.IsXRegister()) << out_reg;
  if (null_allowed) {
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadWFromOffset(kLoadWord, out_reg.AsOverlappingWRegister(), SP,
                      handle_scope_offs.Int32Value());
      in_reg = out_reg;
    }
    ___ Cmp(reg_w(in_reg.AsOverlappingWRegister()), 0);
    if (!out_reg.Equals(in_reg)) {
      LoadImmediate(out_reg.AsXRegister(), 0, eq);
    }
    AddConstant(out_reg.AsXRegister(), SP, handle_scope_offs.Int32Value(), ne);
  } else {
    AddConstant(out_reg.AsXRegister(), SP, handle_scope_offs.Int32Value(), al);
  }
}

void Arm64JNIMacroAssembler::CreateHandleScopeEntry(FrameOffset out_off,
                                                    FrameOffset handle_scope_offset,
                                                    ManagedRegister m_scratch,
                                                    bool null_allowed) {
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  CHECK(scratch.IsXRegister()) << scratch;
  if (null_allowed) {
    LoadWFromOffset(kLoadWord, scratch.AsOverlappingWRegister(), SP,
                    handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset)
    ___ Cmp(reg_w(scratch.AsOverlappingWRegister()), 0);
    // Move this logic in add constants with flags.
    AddConstant(scratch.AsXRegister(), SP, handle_scope_offset.Int32Value(), ne);
  } else {
    AddConstant(scratch.AsXRegister(), SP, handle_scope_offset.Int32Value(), al);
  }
  StoreToOffset(scratch.AsXRegister(), SP, out_off.Int32Value());
}

void Arm64JNIMacroAssembler::LoadReferenceFromHandleScope(ManagedRegister m_out_reg,
                                                          ManagedRegister m_in_reg) {
  Arm64ManagedRegister out_reg = m_out_reg.AsArm64();
  Arm64ManagedRegister in_reg = m_in_reg.AsArm64();
  CHECK(out_reg.IsXRegister()) << out_reg;
  CHECK(in_reg.IsXRegister()) << in_reg;
  vixl::aarch64::Label exit;
  if (!out_reg.Equals(in_reg)) {
    // FIXME: Who sets the flags here?
    LoadImmediate(out_reg.AsXRegister(), 0, eq);
  }
  ___ Cbz(reg_x(in_reg.AsXRegister()), &exit);
  LoadFromOffset(out_reg.AsXRegister(), in_reg.AsXRegister(), 0);
  ___ Bind(&exit);
}

void Arm64JNIMacroAssembler::ExceptionPoll(ManagedRegister m_scratch, size_t stack_adjust) {
  CHECK_ALIGNED(stack_adjust, kStackAlignment);
  Arm64ManagedRegister scratch = m_scratch.AsArm64();
  exception_blocks_.emplace_back(new Arm64Exception(scratch, stack_adjust));
  LoadFromOffset(scratch.AsXRegister(),
                 TR,
                 Thread::ExceptionOffset<kArm64PointerSize>().Int32Value());
  ___ Cbnz(reg_x(scratch.AsXRegister()), exception_blocks_.back()->Entry());
}

std::unique_ptr<JNIMacroLabel> Arm64JNIMacroAssembler::CreateLabel() {
  return std::unique_ptr<JNIMacroLabel>(new Arm64JNIMacroLabel());
}

void Arm64JNIMacroAssembler::Jump(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  ___ B(Arm64JNIMacroLabel::Cast(label)->AsArm64());
}

void Arm64JNIMacroAssembler::Jump(JNIMacroLabel* label,
                                  JNIMacroUnaryCondition condition,
                                  ManagedRegister test) {
  CHECK(label != nullptr);

  switch (condition) {
    case JNIMacroUnaryCondition::kZero:
      ___ Cbz(reg_x(test.AsArm64().AsXRegister()), Arm64JNIMacroLabel::Cast(label)->AsArm64());
      break;
    case JNIMacroUnaryCondition::kNotZero:
      ___ Cbnz(reg_x(test.AsArm64().AsXRegister()), Arm64JNIMacroLabel::Cast(label)->AsArm64());
      break;
    default:
      LOG(FATAL) << "Not implemented unary condition: " << static_cast<int>(condition);
      UNREACHABLE();
  }
}

void Arm64JNIMacroAssembler::Bind(JNIMacroLabel* label) {
  CHECK(label != nullptr);
  ___ Bind(Arm64JNIMacroLabel::Cast(label)->AsArm64());
}

void Arm64JNIMacroAssembler::EmitExceptionPoll(Arm64Exception* exception) {
  UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
  temps.Exclude(reg_x(exception->scratch_.AsXRegister()));
  Register temp = temps.AcquireX();

  // Bind exception poll entry.
  ___ Bind(exception->Entry());
  if (exception->stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSize(exception->stack_adjust_);
  }
  // Pass exception object as argument.
  // Don't care about preserving X0 as this won't return.
  ___ Mov(reg_x(X0), reg_x(exception->scratch_.AsXRegister()));
  ___ Ldr(temp,
          MEM_OP(reg_x(TR),
                 QUICK_ENTRYPOINT_OFFSET(kArm64PointerSize, pDeliverException).Int32Value()));

  ___ Blr(temp);
  // Call should never return.
  ___ Brk();
}

void Arm64JNIMacroAssembler::BuildFrame(size_t frame_size,
                                        ManagedRegister method_reg,
                                        ArrayRef<const ManagedRegister> callee_save_regs,
                                        const ManagedRegisterEntrySpills& entry_spills) {
  // Setup VIXL CPURegList for callee-saves.
  CPURegList core_reg_list(CPURegister::kRegister, kXRegSize, 0);
  CPURegList fp_reg_list(CPURegister::kFPRegister, kDRegSize, 0);
  for (auto r : callee_save_regs) {
    Arm64ManagedRegister reg = r.AsArm64();
    if (reg.IsXRegister()) {
      core_reg_list.Combine(reg_x(reg.AsXRegister()).GetCode());
    } else {
      DCHECK(reg.IsDRegister());
      fp_reg_list.Combine(reg_d(reg.AsDRegister()).GetCode());
    }
  }
  size_t core_reg_size = core_reg_list.GetTotalSizeInBytes();
  size_t fp_reg_size = fp_reg_list.GetTotalSizeInBytes();

  // Increase frame to required size.
  DCHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK_GE(frame_size, core_reg_size + fp_reg_size + static_cast<size_t>(kArm64PointerSize));
  IncreaseFrameSize(frame_size);

  // Save callee-saves.
  asm_.SpillRegisters(core_reg_list, frame_size - core_reg_size);
  asm_.SpillRegisters(fp_reg_list, frame_size - core_reg_size - fp_reg_size);

  DCHECK(core_reg_list.IncludesAliasOf(reg_x(TR)));

  // Write ArtMethod*
  DCHECK(X0 == method_reg.AsArm64().AsXRegister());
  StoreToOffset(X0, SP, 0);

  // Write out entry spills
  int32_t offset = frame_size + static_cast<size_t>(kArm64PointerSize);
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    Arm64ManagedRegister reg = entry_spills.at(i).AsArm64();
    if (reg.IsNoRegister()) {
      // only increment stack offset.
      ManagedRegisterSpill spill = entry_spills.at(i);
      offset += spill.getSize();
    } else if (reg.IsXRegister()) {
      StoreToOffset(reg.AsXRegister(), SP, offset);
      offset += 8;
    } else if (reg.IsWRegister()) {
      StoreWToOffset(kStoreWord, reg.AsWRegister(), SP, offset);
      offset += 4;
    } else if (reg.IsDRegister()) {
      StoreDToOffset(reg.AsDRegister(), SP, offset);
      offset += 8;
    } else if (reg.IsSRegister()) {
      StoreSToOffset(reg.AsSRegister(), SP, offset);
      offset += 4;
    }
  }
}

void Arm64JNIMacroAssembler::RemoveFrame(size_t frame_size,
                                         ArrayRef<const ManagedRegister> callee_save_regs,
                                         bool may_suspend) {
  // Setup VIXL CPURegList for callee-saves.
  CPURegList core_reg_list(CPURegister::kRegister, kXRegSize, 0);
  CPURegList fp_reg_list(CPURegister::kFPRegister, kDRegSize, 0);
  for (auto r : callee_save_regs) {
    Arm64ManagedRegister reg = r.AsArm64();
    if (reg.IsXRegister()) {
      core_reg_list.Combine(reg_x(reg.AsXRegister()).GetCode());
    } else {
      DCHECK(reg.IsDRegister());
      fp_reg_list.Combine(reg_d(reg.AsDRegister()).GetCode());
    }
  }
  size_t core_reg_size = core_reg_list.GetTotalSizeInBytes();
  size_t fp_reg_size = fp_reg_list.GetTotalSizeInBytes();

  // For now we only check that the size of the frame is large enough to hold spills and method
  // reference.
  DCHECK_GE(frame_size, core_reg_size + fp_reg_size + static_cast<size_t>(kArm64PointerSize));
  DCHECK_ALIGNED(frame_size, kStackAlignment);

  DCHECK(core_reg_list.IncludesAliasOf(reg_x(TR)));

  cfi().RememberState();

  // Restore callee-saves.
  asm_.UnspillRegisters(core_reg_list, frame_size - core_reg_size);
  asm_.UnspillRegisters(fp_reg_list, frame_size - core_reg_size - fp_reg_size);

  if (kEmitCompilerReadBarrier && kUseBakerReadBarrier) {
    vixl::aarch64::Register mr = reg_x(MR);  // Marking Register.
    vixl::aarch64::Register tr = reg_x(TR);  // Thread Register.

    if (may_suspend) {
      // The method may be suspended; refresh the Marking Register.
      ___ Ldr(mr.W(), MemOperand(tr, Thread::IsGcMarkingOffset<kArm64PointerSize>().Int32Value()));
    } else {
      // The method shall not be suspended; no need to refresh the Marking Register.

      // Check that the Marking Register is a callee-save register,
      // and thus has been preserved by native code following the
      // AAPCS64 calling convention.
      DCHECK(core_reg_list.IncludesAliasOf(mr))
          << "core_reg_list should contain Marking Register X" << mr.GetCode();

      // The following condition is a compile-time one, so it does not have a run-time cost.
      if (kIsDebugBuild) {
        // The following condition is a run-time one; it is executed after the
        // previous compile-time test, to avoid penalizing non-debug builds.
        if (emit_run_time_checks_in_debug_mode_) {
          // Emit a run-time check verifying that the Marking Register is up-to-date.
          UseScratchRegisterScope temps(asm_.GetVIXLAssembler());
          Register temp = temps.AcquireW();
          // Ensure we are not clobbering a callee-save register that was restored before.
          DCHECK(!core_reg_list.IncludesAliasOf(temp.X()))
              << "core_reg_list should not contain scratch register X" << temp.GetCode();
          asm_.GenerateMarkingRegisterCheck(temp);
        }
      }
    }
  }

  // Decrease frame size to start of callee saved regs.
  DecreaseFrameSize(frame_size);

  // Return to LR.
  ___ Ret();

  // The CFI should be restored for any code that follows the exit block.
  cfi().RestoreState();
  cfi().DefCFAOffset(frame_size);
}

#undef ___

}  // namespace arm64
}  // namespace art
