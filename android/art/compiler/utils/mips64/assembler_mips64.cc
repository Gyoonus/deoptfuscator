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

#include "assembler_mips64.h"

#include "base/bit_utils.h"
#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "memory_region.h"
#include "thread.h"

namespace art {
namespace mips64 {

static_assert(static_cast<size_t>(kMips64PointerSize) == kMips64DoublewordSize,
              "Unexpected Mips64 pointer size.");
static_assert(kMips64PointerSize == PointerSize::k64, "Unexpected Mips64 pointer size.");


void Mips64Assembler::FinalizeCode() {
  for (auto& exception_block : exception_blocks_) {
    EmitExceptionPoll(&exception_block);
  }
  ReserveJumpTableSpace();
  EmitLiterals();
  PromoteBranches();
}

void Mips64Assembler::FinalizeInstructions(const MemoryRegion& region) {
  EmitBranches();
  EmitJumpTables();
  Assembler::FinalizeInstructions(region);
  PatchCFI();
}

void Mips64Assembler::PatchCFI() {
  if (cfi().NumberOfDelayedAdvancePCs() == 0u) {
    return;
  }

  typedef DebugFrameOpCodeWriterForAssembler::DelayedAdvancePC DelayedAdvancePC;
  const auto data = cfi().ReleaseStreamAndPrepareForDelayedAdvancePC();
  const std::vector<uint8_t>& old_stream = data.first;
  const std::vector<DelayedAdvancePC>& advances = data.second;

  // Refill our data buffer with patched opcodes.
  cfi().ReserveCFIStream(old_stream.size() + advances.size() + 16);
  size_t stream_pos = 0;
  for (const DelayedAdvancePC& advance : advances) {
    DCHECK_GE(advance.stream_pos, stream_pos);
    // Copy old data up to the point where advance was issued.
    cfi().AppendRawData(old_stream, stream_pos, advance.stream_pos);
    stream_pos = advance.stream_pos;
    // Insert the advance command with its final offset.
    size_t final_pc = GetAdjustedPosition(advance.pc);
    cfi().AdvancePC(final_pc);
  }
  // Copy the final segment if any.
  cfi().AppendRawData(old_stream, stream_pos, old_stream.size());
}

void Mips64Assembler::EmitBranches() {
  CHECK(!overwriting_);
  // Switch from appending instructions at the end of the buffer to overwriting
  // existing instructions (branch placeholders) in the buffer.
  overwriting_ = true;
  for (auto& branch : branches_) {
    EmitBranch(&branch);
  }
  overwriting_ = false;
}

void Mips64Assembler::Emit(uint32_t value) {
  if (overwriting_) {
    // Branches to labels are emitted into their placeholders here.
    buffer_.Store<uint32_t>(overwrite_location_, value);
    overwrite_location_ += sizeof(uint32_t);
  } else {
    // Other instructions are simply appended at the end here.
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    buffer_.Emit<uint32_t>(value);
  }
}

void Mips64Assembler::EmitR(int opcode, GpuRegister rs, GpuRegister rt, GpuRegister rd,
                            int shamt, int funct) {
  CHECK_NE(rs, kNoGpuRegister);
  CHECK_NE(rt, kNoGpuRegister);
  CHECK_NE(rd, kNoGpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      static_cast<uint32_t>(rt) << kRtShift |
                      static_cast<uint32_t>(rd) << kRdShift |
                      shamt << kShamtShift |
                      funct;
  Emit(encoding);
}

void Mips64Assembler::EmitRsd(int opcode, GpuRegister rs, GpuRegister rd,
                              int shamt, int funct) {
  CHECK_NE(rs, kNoGpuRegister);
  CHECK_NE(rd, kNoGpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      static_cast<uint32_t>(ZERO) << kRtShift |
                      static_cast<uint32_t>(rd) << kRdShift |
                      shamt << kShamtShift |
                      funct;
  Emit(encoding);
}

void Mips64Assembler::EmitRtd(int opcode, GpuRegister rt, GpuRegister rd,
                              int shamt, int funct) {
  CHECK_NE(rt, kNoGpuRegister);
  CHECK_NE(rd, kNoGpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(ZERO) << kRsShift |
                      static_cast<uint32_t>(rt) << kRtShift |
                      static_cast<uint32_t>(rd) << kRdShift |
                      shamt << kShamtShift |
                      funct;
  Emit(encoding);
}

void Mips64Assembler::EmitI(int opcode, GpuRegister rs, GpuRegister rt, uint16_t imm) {
  CHECK_NE(rs, kNoGpuRegister);
  CHECK_NE(rt, kNoGpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      static_cast<uint32_t>(rt) << kRtShift |
                      imm;
  Emit(encoding);
}

void Mips64Assembler::EmitI21(int opcode, GpuRegister rs, uint32_t imm21) {
  CHECK_NE(rs, kNoGpuRegister);
  CHECK(IsUint<21>(imm21)) << imm21;
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      imm21;
  Emit(encoding);
}

void Mips64Assembler::EmitI26(int opcode, uint32_t imm26) {
  CHECK(IsUint<26>(imm26)) << imm26;
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift | imm26;
  Emit(encoding);
}

void Mips64Assembler::EmitFR(int opcode, int fmt, FpuRegister ft, FpuRegister fs, FpuRegister fd,
                             int funct) {
  CHECK_NE(ft, kNoFpuRegister);
  CHECK_NE(fs, kNoFpuRegister);
  CHECK_NE(fd, kNoFpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      fmt << kFmtShift |
                      static_cast<uint32_t>(ft) << kFtShift |
                      static_cast<uint32_t>(fs) << kFsShift |
                      static_cast<uint32_t>(fd) << kFdShift |
                      funct;
  Emit(encoding);
}

void Mips64Assembler::EmitFI(int opcode, int fmt, FpuRegister ft, uint16_t imm) {
  CHECK_NE(ft, kNoFpuRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      fmt << kFmtShift |
                      static_cast<uint32_t>(ft) << kFtShift |
                      imm;
  Emit(encoding);
}

void Mips64Assembler::EmitMsa3R(int operation,
                                int df,
                                VectorRegister wt,
                                VectorRegister ws,
                                VectorRegister wd,
                                int minor_opcode) {
  CHECK_NE(wt, kNoVectorRegister);
  CHECK_NE(ws, kNoVectorRegister);
  CHECK_NE(wd, kNoVectorRegister);
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsaOperationShift |
                      df << kDfShift |
                      static_cast<uint32_t>(wt) << kWtShift |
                      static_cast<uint32_t>(ws) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::EmitMsaBIT(int operation,
                                 int df_m,
                                 VectorRegister ws,
                                 VectorRegister wd,
                                 int minor_opcode) {
  CHECK_NE(ws, kNoVectorRegister);
  CHECK_NE(wd, kNoVectorRegister);
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsaOperationShift |
                      df_m << kDfMShift |
                      static_cast<uint32_t>(ws) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::EmitMsaELM(int operation,
                                 int df_n,
                                 VectorRegister ws,
                                 VectorRegister wd,
                                 int minor_opcode) {
  CHECK_NE(ws, kNoVectorRegister);
  CHECK_NE(wd, kNoVectorRegister);
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsaELMOperationShift |
                      df_n << kDfNShift |
                      static_cast<uint32_t>(ws) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::EmitMsaMI10(int s10,
                                  GpuRegister rs,
                                  VectorRegister wd,
                                  int minor_opcode,
                                  int df) {
  CHECK_NE(rs, kNoGpuRegister);
  CHECK_NE(wd, kNoVectorRegister);
  CHECK(IsUint<10>(s10)) << s10;
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      s10 << kS10Shift |
                      static_cast<uint32_t>(rs) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode << kS10MinorShift |
                      df;
  Emit(encoding);
}

void Mips64Assembler::EmitMsaI10(int operation,
                                 int df,
                                 int i10,
                                 VectorRegister wd,
                                 int minor_opcode) {
  CHECK_NE(wd, kNoVectorRegister);
  CHECK(IsUint<10>(i10)) << i10;
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsaOperationShift |
                      df << kDfShift |
                      i10 << kI10Shift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::EmitMsa2R(int operation,
                                int df,
                                VectorRegister ws,
                                VectorRegister wd,
                                int minor_opcode) {
  CHECK_NE(ws, kNoVectorRegister);
  CHECK_NE(wd, kNoVectorRegister);
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsa2ROperationShift |
                      df << kDf2RShift |
                      static_cast<uint32_t>(ws) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::EmitMsa2RF(int operation,
                                 int df,
                                 VectorRegister ws,
                                 VectorRegister wd,
                                 int minor_opcode) {
  CHECK_NE(ws, kNoVectorRegister);
  CHECK_NE(wd, kNoVectorRegister);
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      operation << kMsa2RFOperationShift |
                      df << kDf2RShift |
                      static_cast<uint32_t>(ws) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode;
  Emit(encoding);
}

void Mips64Assembler::Addu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x21);
}

void Mips64Assembler::Addiu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x9, rs, rt, imm16);
}

void Mips64Assembler::Daddu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x2d);
}

void Mips64Assembler::Daddiu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x19, rs, rt, imm16);
}

void Mips64Assembler::Subu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x23);
}

void Mips64Assembler::Dsubu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x2f);
}

void Mips64Assembler::MulR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x18);
}

void Mips64Assembler::MuhR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x18);
}

void Mips64Assembler::DivR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x1a);
}

void Mips64Assembler::ModR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x1a);
}

void Mips64Assembler::DivuR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x1b);
}

void Mips64Assembler::ModuR6(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x1b);
}

void Mips64Assembler::Dmul(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x1c);
}

void Mips64Assembler::Dmuh(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x1c);
}

void Mips64Assembler::Ddiv(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x1e);
}

void Mips64Assembler::Dmod(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x1e);
}

void Mips64Assembler::Ddivu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 2, 0x1f);
}

void Mips64Assembler::Dmodu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 3, 0x1f);
}

void Mips64Assembler::And(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x24);
}

void Mips64Assembler::Andi(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xc, rs, rt, imm16);
}

void Mips64Assembler::Or(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x25);
}

void Mips64Assembler::Ori(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xd, rs, rt, imm16);
}

void Mips64Assembler::Xor(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x26);
}

void Mips64Assembler::Xori(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xe, rs, rt, imm16);
}

void Mips64Assembler::Nor(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x27);
}

void Mips64Assembler::Bitswap(GpuRegister rd, GpuRegister rt) {
  EmitRtd(0x1f, rt, rd, 0x0, 0x20);
}

void Mips64Assembler::Dbitswap(GpuRegister rd, GpuRegister rt) {
  EmitRtd(0x1f, rt, rd, 0x0, 0x24);
}

void Mips64Assembler::Seb(GpuRegister rd, GpuRegister rt) {
  EmitR(0x1f, static_cast<GpuRegister>(0), rt, rd, 0x10, 0x20);
}

void Mips64Assembler::Seh(GpuRegister rd, GpuRegister rt) {
  EmitR(0x1f, static_cast<GpuRegister>(0), rt, rd, 0x18, 0x20);
}

void Mips64Assembler::Dsbh(GpuRegister rd, GpuRegister rt) {
  EmitRtd(0x1f, rt, rd, 0x2, 0x24);
}

void Mips64Assembler::Dshd(GpuRegister rd, GpuRegister rt) {
  EmitRtd(0x1f, rt, rd, 0x5, 0x24);
}

void Mips64Assembler::Dext(GpuRegister rt, GpuRegister rs, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(IsUint<5>(size - 1)) << size;
  EmitR(0x1f, rs, rt, static_cast<GpuRegister>(size - 1), pos, 0x3);
}

void Mips64Assembler::Ins(GpuRegister rd, GpuRegister rt, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(IsUint<5>(size - 1)) << size;
  CHECK(IsUint<5>(pos + size - 1)) << pos << " + " << size;
  EmitR(0x1f, rt, rd, static_cast<GpuRegister>(pos + size - 1), pos, 0x04);
}

void Mips64Assembler::Dinsm(GpuRegister rt, GpuRegister rs, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(2 <= size && size <= 64) << size;
  CHECK(IsUint<5>(pos + size - 33)) << pos << " + " << size;
  EmitR(0x1f, rs, rt, static_cast<GpuRegister>(pos + size - 33), pos, 0x5);
}

void Mips64Assembler::Dinsu(GpuRegister rt, GpuRegister rs, int pos, int size) {
  CHECK(IsUint<5>(pos - 32)) << pos;
  CHECK(IsUint<5>(size - 1)) << size;
  CHECK(IsUint<5>(pos + size - 33)) << pos << " + " << size;
  EmitR(0x1f, rs, rt, static_cast<GpuRegister>(pos + size - 33), pos - 32, 0x6);
}

void Mips64Assembler::Dins(GpuRegister rt, GpuRegister rs, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(IsUint<5>(size - 1)) << size;
  CHECK(IsUint<5>(pos + size - 1)) << pos << " + " << size;
  EmitR(0x1f, rs, rt, static_cast<GpuRegister>(pos + size - 1), pos, 0x7);
}

void Mips64Assembler::DblIns(GpuRegister rt, GpuRegister rs, int pos, int size) {
  if (pos >= 32) {
    Dinsu(rt, rs, pos, size);
  } else if ((static_cast<int64_t>(pos) + size - 1) >= 32) {
    Dinsm(rt, rs, pos, size);
  } else {
    Dins(rt, rs, pos, size);
  }
}

void Mips64Assembler::Lsa(GpuRegister rd, GpuRegister rs, GpuRegister rt, int saPlusOne) {
  CHECK(1 <= saPlusOne && saPlusOne <= 4) << saPlusOne;
  int sa = saPlusOne - 1;
  EmitR(0x0, rs, rt, rd, sa, 0x05);
}

void Mips64Assembler::Dlsa(GpuRegister rd, GpuRegister rs, GpuRegister rt, int saPlusOne) {
  CHECK(1 <= saPlusOne && saPlusOne <= 4) << saPlusOne;
  int sa = saPlusOne - 1;
  EmitR(0x0, rs, rt, rd, sa, 0x15);
}

void Mips64Assembler::Wsbh(GpuRegister rd, GpuRegister rt) {
  EmitRtd(0x1f, rt, rd, 2, 0x20);
}

void Mips64Assembler::Sc(GpuRegister rt, GpuRegister base, int16_t imm9) {
  CHECK(IsInt<9>(imm9));
  EmitI(0x1f, base, rt, ((imm9 & 0x1FF) << 7) | 0x26);
}

void Mips64Assembler::Scd(GpuRegister rt, GpuRegister base, int16_t imm9) {
  CHECK(IsInt<9>(imm9));
  EmitI(0x1f, base, rt, ((imm9 & 0x1FF) << 7) | 0x27);
}

void Mips64Assembler::Ll(GpuRegister rt, GpuRegister base, int16_t imm9) {
  CHECK(IsInt<9>(imm9));
  EmitI(0x1f, base, rt, ((imm9 & 0x1FF) << 7) | 0x36);
}

void Mips64Assembler::Lld(GpuRegister rt, GpuRegister base, int16_t imm9) {
  CHECK(IsInt<9>(imm9));
  EmitI(0x1f, base, rt, ((imm9 & 0x1FF) << 7) | 0x37);
}

void Mips64Assembler::Sll(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x00);
}

void Mips64Assembler::Srl(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x02);
}

void Mips64Assembler::Rotr(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(1), rt, rd, shamt, 0x02);
}

void Mips64Assembler::Sra(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x03);
}

void Mips64Assembler::Sllv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x04);
}

void Mips64Assembler::Rotrv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 1, 0x06);
}

void Mips64Assembler::Srlv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x06);
}

void Mips64Assembler::Srav(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x07);
}

void Mips64Assembler::Dsll(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x38);
}

void Mips64Assembler::Dsrl(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x3a);
}

void Mips64Assembler::Drotr(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(1), rt, rd, shamt, 0x3a);
}

void Mips64Assembler::Dsra(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x3b);
}

void Mips64Assembler::Dsll32(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x3c);
}

void Mips64Assembler::Dsrl32(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x3e);
}

void Mips64Assembler::Drotr32(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(1), rt, rd, shamt, 0x3e);
}

void Mips64Assembler::Dsra32(GpuRegister rd, GpuRegister rt, int shamt) {
  EmitR(0, static_cast<GpuRegister>(0), rt, rd, shamt, 0x3f);
}

void Mips64Assembler::Dsllv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x14);
}

void Mips64Assembler::Dsrlv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x16);
}

void Mips64Assembler::Drotrv(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 1, 0x16);
}

void Mips64Assembler::Dsrav(GpuRegister rd, GpuRegister rt, GpuRegister rs) {
  EmitR(0, rs, rt, rd, 0, 0x17);
}

void Mips64Assembler::Lb(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x20, rs, rt, imm16);
}

void Mips64Assembler::Lh(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x21, rs, rt, imm16);
}

void Mips64Assembler::Lw(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x23, rs, rt, imm16);
}

void Mips64Assembler::Ld(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x37, rs, rt, imm16);
}

void Mips64Assembler::Lbu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x24, rs, rt, imm16);
}

void Mips64Assembler::Lhu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x25, rs, rt, imm16);
}

void Mips64Assembler::Lwu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x27, rs, rt, imm16);
}

void Mips64Assembler::Lwpc(GpuRegister rs, uint32_t imm19) {
  CHECK(IsUint<19>(imm19)) << imm19;
  EmitI21(0x3B, rs, (0x01 << 19) | imm19);
}

void Mips64Assembler::Lwupc(GpuRegister rs, uint32_t imm19) {
  CHECK(IsUint<19>(imm19)) << imm19;
  EmitI21(0x3B, rs, (0x02 << 19) | imm19);
}

void Mips64Assembler::Ldpc(GpuRegister rs, uint32_t imm18) {
  CHECK(IsUint<18>(imm18)) << imm18;
  EmitI21(0x3B, rs, (0x06 << 18) | imm18);
}

void Mips64Assembler::Lui(GpuRegister rt, uint16_t imm16) {
  EmitI(0xf, static_cast<GpuRegister>(0), rt, imm16);
}

void Mips64Assembler::Aui(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xf, rs, rt, imm16);
}

void Mips64Assembler::Daui(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  EmitI(0x1d, rs, rt, imm16);
}

void Mips64Assembler::Dahi(GpuRegister rs, uint16_t imm16) {
  EmitI(1, rs, static_cast<GpuRegister>(6), imm16);
}

void Mips64Assembler::Dati(GpuRegister rs, uint16_t imm16) {
  EmitI(1, rs, static_cast<GpuRegister>(0x1e), imm16);
}

void Mips64Assembler::Sync(uint32_t stype) {
  EmitR(0, static_cast<GpuRegister>(0), static_cast<GpuRegister>(0),
           static_cast<GpuRegister>(0), stype & 0x1f, 0xf);
}

void Mips64Assembler::Sb(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x28, rs, rt, imm16);
}

void Mips64Assembler::Sh(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x29, rs, rt, imm16);
}

void Mips64Assembler::Sw(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x2b, rs, rt, imm16);
}

void Mips64Assembler::Sd(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0x3f, rs, rt, imm16);
}

void Mips64Assembler::Slt(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x2a);
}

void Mips64Assembler::Sltu(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x2b);
}

void Mips64Assembler::Slti(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xa, rs, rt, imm16);
}

void Mips64Assembler::Sltiu(GpuRegister rt, GpuRegister rs, uint16_t imm16) {
  EmitI(0xb, rs, rt, imm16);
}

void Mips64Assembler::Seleqz(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x35);
}

void Mips64Assembler::Selnez(GpuRegister rd, GpuRegister rs, GpuRegister rt) {
  EmitR(0, rs, rt, rd, 0, 0x37);
}

void Mips64Assembler::Clz(GpuRegister rd, GpuRegister rs) {
  EmitRsd(0, rs, rd, 0x01, 0x10);
}

void Mips64Assembler::Clo(GpuRegister rd, GpuRegister rs) {
  EmitRsd(0, rs, rd, 0x01, 0x11);
}

void Mips64Assembler::Dclz(GpuRegister rd, GpuRegister rs) {
  EmitRsd(0, rs, rd, 0x01, 0x12);
}

void Mips64Assembler::Dclo(GpuRegister rd, GpuRegister rs) {
  EmitRsd(0, rs, rd, 0x01, 0x13);
}

void Mips64Assembler::Jalr(GpuRegister rd, GpuRegister rs) {
  EmitR(0, rs, static_cast<GpuRegister>(0), rd, 0, 0x09);
}

void Mips64Assembler::Jalr(GpuRegister rs) {
  Jalr(RA, rs);
}

void Mips64Assembler::Jr(GpuRegister rs) {
  Jalr(ZERO, rs);
}

void Mips64Assembler::Auipc(GpuRegister rs, uint16_t imm16) {
  EmitI(0x3B, rs, static_cast<GpuRegister>(0x1E), imm16);
}

void Mips64Assembler::Addiupc(GpuRegister rs, uint32_t imm19) {
  CHECK(IsUint<19>(imm19)) << imm19;
  EmitI21(0x3B, rs, imm19);
}

void Mips64Assembler::Bc(uint32_t imm26) {
  EmitI26(0x32, imm26);
}

void Mips64Assembler::Balc(uint32_t imm26) {
  EmitI26(0x3A, imm26);
}

void Mips64Assembler::Jic(GpuRegister rt, uint16_t imm16) {
  EmitI(0x36, static_cast<GpuRegister>(0), rt, imm16);
}

void Mips64Assembler::Jialc(GpuRegister rt, uint16_t imm16) {
  EmitI(0x3E, static_cast<GpuRegister>(0), rt, imm16);
}

void Mips64Assembler::Bltc(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x17, rs, rt, imm16);
}

void Mips64Assembler::Bltzc(GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rt, ZERO);
  EmitI(0x17, rt, rt, imm16);
}

void Mips64Assembler::Bgtzc(GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rt, ZERO);
  EmitI(0x17, static_cast<GpuRegister>(0), rt, imm16);
}

void Mips64Assembler::Bgec(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x16, rs, rt, imm16);
}

void Mips64Assembler::Bgezc(GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rt, ZERO);
  EmitI(0x16, rt, rt, imm16);
}

void Mips64Assembler::Blezc(GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rt, ZERO);
  EmitI(0x16, static_cast<GpuRegister>(0), rt, imm16);
}

void Mips64Assembler::Bltuc(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x7, rs, rt, imm16);
}

void Mips64Assembler::Bgeuc(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x6, rs, rt, imm16);
}

void Mips64Assembler::Beqc(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x8, std::min(rs, rt), std::max(rs, rt), imm16);
}

void Mips64Assembler::Bnec(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  EmitI(0x18, std::min(rs, rt), std::max(rs, rt), imm16);
}

void Mips64Assembler::Beqzc(GpuRegister rs, uint32_t imm21) {
  CHECK_NE(rs, ZERO);
  EmitI21(0x36, rs, imm21);
}

void Mips64Assembler::Bnezc(GpuRegister rs, uint32_t imm21) {
  CHECK_NE(rs, ZERO);
  EmitI21(0x3E, rs, imm21);
}

void Mips64Assembler::Bc1eqz(FpuRegister ft, uint16_t imm16) {
  EmitFI(0x11, 0x9, ft, imm16);
}

void Mips64Assembler::Bc1nez(FpuRegister ft, uint16_t imm16) {
  EmitFI(0x11, 0xD, ft, imm16);
}

void Mips64Assembler::Beq(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  EmitI(0x4, rs, rt, imm16);
}

void Mips64Assembler::Bne(GpuRegister rs, GpuRegister rt, uint16_t imm16) {
  EmitI(0x5, rs, rt, imm16);
}

void Mips64Assembler::Beqz(GpuRegister rt, uint16_t imm16) {
  Beq(rt, ZERO, imm16);
}

void Mips64Assembler::Bnez(GpuRegister rt, uint16_t imm16) {
  Bne(rt, ZERO, imm16);
}

void Mips64Assembler::Bltz(GpuRegister rt, uint16_t imm16) {
  EmitI(0x1, rt, static_cast<GpuRegister>(0), imm16);
}

void Mips64Assembler::Bgez(GpuRegister rt, uint16_t imm16) {
  EmitI(0x1, rt, static_cast<GpuRegister>(0x1), imm16);
}

void Mips64Assembler::Blez(GpuRegister rt, uint16_t imm16) {
  EmitI(0x6, rt, static_cast<GpuRegister>(0), imm16);
}

void Mips64Assembler::Bgtz(GpuRegister rt, uint16_t imm16) {
  EmitI(0x7, rt, static_cast<GpuRegister>(0), imm16);
}

void Mips64Assembler::EmitBcondR6(BranchCondition cond,
                                  GpuRegister rs,
                                  GpuRegister rt,
                                  uint32_t imm16_21) {
  switch (cond) {
    case kCondLT:
      Bltc(rs, rt, imm16_21);
      break;
    case kCondGE:
      Bgec(rs, rt, imm16_21);
      break;
    case kCondLE:
      Bgec(rt, rs, imm16_21);
      break;
    case kCondGT:
      Bltc(rt, rs, imm16_21);
      break;
    case kCondLTZ:
      CHECK_EQ(rt, ZERO);
      Bltzc(rs, imm16_21);
      break;
    case kCondGEZ:
      CHECK_EQ(rt, ZERO);
      Bgezc(rs, imm16_21);
      break;
    case kCondLEZ:
      CHECK_EQ(rt, ZERO);
      Blezc(rs, imm16_21);
      break;
    case kCondGTZ:
      CHECK_EQ(rt, ZERO);
      Bgtzc(rs, imm16_21);
      break;
    case kCondEQ:
      Beqc(rs, rt, imm16_21);
      break;
    case kCondNE:
      Bnec(rs, rt, imm16_21);
      break;
    case kCondEQZ:
      CHECK_EQ(rt, ZERO);
      Beqzc(rs, imm16_21);
      break;
    case kCondNEZ:
      CHECK_EQ(rt, ZERO);
      Bnezc(rs, imm16_21);
      break;
    case kCondLTU:
      Bltuc(rs, rt, imm16_21);
      break;
    case kCondGEU:
      Bgeuc(rs, rt, imm16_21);
      break;
    case kCondF:
      CHECK_EQ(rt, ZERO);
      Bc1eqz(static_cast<FpuRegister>(rs), imm16_21);
      break;
    case kCondT:
      CHECK_EQ(rt, ZERO);
      Bc1nez(static_cast<FpuRegister>(rs), imm16_21);
      break;
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
      UNREACHABLE();
  }
}

void Mips64Assembler::EmitBcondR2(BranchCondition cond,
                                  GpuRegister rs,
                                  GpuRegister rt,
                                  uint16_t imm16) {
  switch (cond) {
    case kCondLTZ:
      CHECK_EQ(rt, ZERO);
      Bltz(rs, imm16);
      break;
    case kCondGEZ:
      CHECK_EQ(rt, ZERO);
      Bgez(rs, imm16);
      break;
    case kCondLEZ:
      CHECK_EQ(rt, ZERO);
      Blez(rs, imm16);
      break;
    case kCondGTZ:
      CHECK_EQ(rt, ZERO);
      Bgtz(rs, imm16);
      break;
    case kCondEQ:
      Beq(rs, rt, imm16);
      break;
    case kCondNE:
      Bne(rs, rt, imm16);
      break;
    case kCondEQZ:
      CHECK_EQ(rt, ZERO);
      Beqz(rs, imm16);
      break;
    case kCondNEZ:
      CHECK_EQ(rt, ZERO);
      Bnez(rs, imm16);
      break;
    case kCondF:
    case kCondT:
    case kCondLT:
    case kCondGE:
    case kCondLE:
    case kCondGT:
    case kCondLTU:
    case kCondGEU:
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
      UNREACHABLE();
  }
}

void Mips64Assembler::AddS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x0);
}

void Mips64Assembler::SubS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x1);
}

void Mips64Assembler::MulS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x2);
}

void Mips64Assembler::DivS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x3);
}

void Mips64Assembler::AddD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x0);
}

void Mips64Assembler::SubD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x1);
}

void Mips64Assembler::MulD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x2);
}

void Mips64Assembler::DivD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x3);
}

void Mips64Assembler::SqrtS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x4);
}

void Mips64Assembler::SqrtD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x4);
}

void Mips64Assembler::AbsS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x5);
}

void Mips64Assembler::AbsD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x5);
}

void Mips64Assembler::MovS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x6);
}

void Mips64Assembler::MovD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x6);
}

void Mips64Assembler::NegS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x7);
}

void Mips64Assembler::NegD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x7);
}

void Mips64Assembler::RoundLS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x8);
}

void Mips64Assembler::RoundLD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x8);
}

void Mips64Assembler::RoundWS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xc);
}

void Mips64Assembler::RoundWD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xc);
}

void Mips64Assembler::TruncLS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x9);
}

void Mips64Assembler::TruncLD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x9);
}

void Mips64Assembler::TruncWS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xd);
}

void Mips64Assembler::TruncWD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xd);
}

void Mips64Assembler::CeilLS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xa);
}

void Mips64Assembler::CeilLD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xa);
}

void Mips64Assembler::CeilWS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xe);
}

void Mips64Assembler::CeilWD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xe);
}

void Mips64Assembler::FloorLS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xb);
}

void Mips64Assembler::FloorLD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xb);
}

void Mips64Assembler::FloorWS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0xf);
}

void Mips64Assembler::FloorWD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0xf);
}

void Mips64Assembler::SelS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x10);
}

void Mips64Assembler::SelD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x10);
}

void Mips64Assembler::SeleqzS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x14);
}

void Mips64Assembler::SeleqzD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x14);
}

void Mips64Assembler::SelnezS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x17);
}

void Mips64Assembler::SelnezD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x17);
}

void Mips64Assembler::RintS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x1a);
}

void Mips64Assembler::RintD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x1a);
}

void Mips64Assembler::ClassS(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x1b);
}

void Mips64Assembler::ClassD(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x1b);
}

void Mips64Assembler::MinS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x1c);
}

void Mips64Assembler::MinD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x1c);
}

void Mips64Assembler::MaxS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x10, ft, fs, fd, 0x1e);
}

void Mips64Assembler::MaxD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x11, ft, fs, fd, 0x1e);
}

void Mips64Assembler::CmpUnS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x01);
}

void Mips64Assembler::CmpEqS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x02);
}

void Mips64Assembler::CmpUeqS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x03);
}

void Mips64Assembler::CmpLtS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x04);
}

void Mips64Assembler::CmpUltS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x05);
}

void Mips64Assembler::CmpLeS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x06);
}

void Mips64Assembler::CmpUleS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x07);
}

void Mips64Assembler::CmpOrS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x11);
}

void Mips64Assembler::CmpUneS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x12);
}

void Mips64Assembler::CmpNeS(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x14, ft, fs, fd, 0x13);
}

void Mips64Assembler::CmpUnD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x01);
}

void Mips64Assembler::CmpEqD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x02);
}

void Mips64Assembler::CmpUeqD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x03);
}

void Mips64Assembler::CmpLtD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x04);
}

void Mips64Assembler::CmpUltD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x05);
}

void Mips64Assembler::CmpLeD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x06);
}

void Mips64Assembler::CmpUleD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x07);
}

void Mips64Assembler::CmpOrD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x11);
}

void Mips64Assembler::CmpUneD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x12);
}

void Mips64Assembler::CmpNeD(FpuRegister fd, FpuRegister fs, FpuRegister ft) {
  EmitFR(0x11, 0x15, ft, fs, fd, 0x13);
}

void Mips64Assembler::Cvtsw(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x14, static_cast<FpuRegister>(0), fs, fd, 0x20);
}

void Mips64Assembler::Cvtdw(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x14, static_cast<FpuRegister>(0), fs, fd, 0x21);
}

void Mips64Assembler::Cvtsd(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x11, static_cast<FpuRegister>(0), fs, fd, 0x20);
}

void Mips64Assembler::Cvtds(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x10, static_cast<FpuRegister>(0), fs, fd, 0x21);
}

void Mips64Assembler::Cvtsl(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x15, static_cast<FpuRegister>(0), fs, fd, 0x20);
}

void Mips64Assembler::Cvtdl(FpuRegister fd, FpuRegister fs) {
  EmitFR(0x11, 0x15, static_cast<FpuRegister>(0), fs, fd, 0x21);
}

void Mips64Assembler::Mfc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x00, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Mfhc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x03, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Mtc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x04, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Mthc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x07, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Dmfc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x01, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Dmtc1(GpuRegister rt, FpuRegister fs) {
  EmitFR(0x11, 0x05, static_cast<FpuRegister>(rt), fs, static_cast<FpuRegister>(0), 0x0);
}

void Mips64Assembler::Lwc1(FpuRegister ft, GpuRegister rs, uint16_t imm16) {
  EmitI(0x31, rs, static_cast<GpuRegister>(ft), imm16);
}

void Mips64Assembler::Ldc1(FpuRegister ft, GpuRegister rs, uint16_t imm16) {
  EmitI(0x35, rs, static_cast<GpuRegister>(ft), imm16);
}

void Mips64Assembler::Swc1(FpuRegister ft, GpuRegister rs, uint16_t imm16) {
  EmitI(0x39, rs, static_cast<GpuRegister>(ft), imm16);
}

void Mips64Assembler::Sdc1(FpuRegister ft, GpuRegister rs, uint16_t imm16) {
  EmitI(0x3d, rs, static_cast<GpuRegister>(ft), imm16);
}

void Mips64Assembler::Break() {
  EmitR(0, static_cast<GpuRegister>(0), static_cast<GpuRegister>(0),
        static_cast<GpuRegister>(0), 0, 0xD);
}

void Mips64Assembler::Nop() {
  EmitR(0x0, static_cast<GpuRegister>(0), static_cast<GpuRegister>(0),
        static_cast<GpuRegister>(0), 0, 0x0);
}

void Mips64Assembler::Move(GpuRegister rd, GpuRegister rs) {
  Or(rd, rs, ZERO);
}

void Mips64Assembler::Clear(GpuRegister rd) {
  Move(rd, ZERO);
}

void Mips64Assembler::Not(GpuRegister rd, GpuRegister rs) {
  Nor(rd, rs, ZERO);
}

void Mips64Assembler::AndV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x1e);
}

void Mips64Assembler::OrV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x1e);
}

void Mips64Assembler::NorV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x1e);
}

void Mips64Assembler::XorV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x1e);
}

void Mips64Assembler::AddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::AddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::AddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::AddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::SubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::SubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::SubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::SubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::Asub_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x11);
}

void Mips64Assembler::Asub_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x11);
}

void Mips64Assembler::MulvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::MulvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::MulvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::MulvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::Div_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::Mod_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::Add_aB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x10);
}

void Mips64Assembler::Add_aH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x10);
}

void Mips64Assembler::Add_aW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x10);
}

void Mips64Assembler::Add_aD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x10);
}

void Mips64Assembler::Ave_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x10);
}

void Mips64Assembler::Aver_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x10);
}

void Mips64Assembler::Max_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x3, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x3, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x3, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::Max_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x3, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x0, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0xe);
}

void Mips64Assembler::Min_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0xe);
}

void Mips64Assembler::FaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmulW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x0, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmulD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x1, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FdivW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x2, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FdivD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x3, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmaxW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmaxD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FminW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FminD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x1b);
}

void Mips64Assembler::Ffint_sW(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  EmitMsa2RF(0x19e, 0x0, ws, wd, 0x1e);
}

void Mips64Assembler::Ffint_sD(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  EmitMsa2RF(0x19e, 0x1, ws, wd, 0x1e);
}

void Mips64Assembler::Ftint_sW(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  EmitMsa2RF(0x19c, 0x0, ws, wd, 0x1e);
}

void Mips64Assembler::Ftint_sD(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  EmitMsa2RF(0x19c, 0x1, ws, wd, 0x1e);
}

void Mips64Assembler::SllB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x0, wt, ws, wd, 0xd);
}

void Mips64Assembler::SllH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x1, wt, ws, wd, 0xd);
}

void Mips64Assembler::SllW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x2, wt, ws, wd, 0xd);
}

void Mips64Assembler::SllD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x0, 0x3, wt, ws, wd, 0xd);
}

void Mips64Assembler::SraB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x0, wt, ws, wd, 0xd);
}

void Mips64Assembler::SraH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x1, wt, ws, wd, 0xd);
}

void Mips64Assembler::SraW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x2, wt, ws, wd, 0xd);
}

void Mips64Assembler::SraD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x3, wt, ws, wd, 0xd);
}

void Mips64Assembler::SrlB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x0, wt, ws, wd, 0xd);
}

void Mips64Assembler::SrlH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x1, wt, ws, wd, 0xd);
}

void Mips64Assembler::SrlW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x2, wt, ws, wd, 0xd);
}

void Mips64Assembler::SrlD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x3, wt, ws, wd, 0xd);
}

void Mips64Assembler::SlliB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  EmitMsaBIT(0x0, shamt3 | kMsaDfMByteMask, ws, wd, 0x9);
}

void Mips64Assembler::SlliH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  EmitMsaBIT(0x0, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9);
}

void Mips64Assembler::SlliW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitMsaBIT(0x0, shamt5 | kMsaDfMWordMask, ws, wd, 0x9);
}

void Mips64Assembler::SlliD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitMsaBIT(0x0, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9);
}

void Mips64Assembler::SraiB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  EmitMsaBIT(0x1, shamt3 | kMsaDfMByteMask, ws, wd, 0x9);
}

void Mips64Assembler::SraiH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  EmitMsaBIT(0x1, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9);
}

void Mips64Assembler::SraiW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitMsaBIT(0x1, shamt5 | kMsaDfMWordMask, ws, wd, 0x9);
}

void Mips64Assembler::SraiD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitMsaBIT(0x1, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9);
}

void Mips64Assembler::SrliB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  EmitMsaBIT(0x2, shamt3 | kMsaDfMByteMask, ws, wd, 0x9);
}

void Mips64Assembler::SrliH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  EmitMsaBIT(0x2, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9);
}

void Mips64Assembler::SrliW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  EmitMsaBIT(0x2, shamt5 | kMsaDfMWordMask, ws, wd, 0x9);
}

void Mips64Assembler::SrliD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  EmitMsaBIT(0x2, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9);
}

void Mips64Assembler::MoveV(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  EmitMsaBIT(0x1, 0x3e, ws, wd, 0x19);
}

void Mips64Assembler::SplatiB(VectorRegister wd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  EmitMsaELM(0x1, n4 | kMsaDfNByteMask, ws, wd, 0x19);
}

void Mips64Assembler::SplatiH(VectorRegister wd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  EmitMsaELM(0x1, n3 | kMsaDfNHalfwordMask, ws, wd, 0x19);
}

void Mips64Assembler::SplatiW(VectorRegister wd, VectorRegister ws, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  EmitMsaELM(0x1, n2 | kMsaDfNWordMask, ws, wd, 0x19);
}

void Mips64Assembler::SplatiD(VectorRegister wd, VectorRegister ws, int n1) {
  CHECK(HasMsa());
  CHECK(IsUint<1>(n1)) << n1;
  EmitMsaELM(0x1, n1 | kMsaDfNDoublewordMask, ws, wd, 0x19);
}

void Mips64Assembler::Copy_sB(GpuRegister rd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  EmitMsaELM(0x2, n4 | kMsaDfNByteMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_sH(GpuRegister rd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  EmitMsaELM(0x2, n3 | kMsaDfNHalfwordMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_sW(GpuRegister rd, VectorRegister ws, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  EmitMsaELM(0x2, n2 | kMsaDfNWordMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_sD(GpuRegister rd, VectorRegister ws, int n1) {
  CHECK(HasMsa());
  CHECK(IsUint<1>(n1)) << n1;
  EmitMsaELM(0x2, n1 | kMsaDfNDoublewordMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_uB(GpuRegister rd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  EmitMsaELM(0x3, n4 | kMsaDfNByteMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_uH(GpuRegister rd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  EmitMsaELM(0x3, n3 | kMsaDfNHalfwordMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::Copy_uW(GpuRegister rd, VectorRegister ws, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  EmitMsaELM(0x3, n2 | kMsaDfNWordMask, ws, static_cast<VectorRegister>(rd), 0x19);
}

void Mips64Assembler::InsertB(VectorRegister wd, GpuRegister rs, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  EmitMsaELM(0x4, n4 | kMsaDfNByteMask, static_cast<VectorRegister>(rs), wd, 0x19);
}

void Mips64Assembler::InsertH(VectorRegister wd, GpuRegister rs, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  EmitMsaELM(0x4, n3 | kMsaDfNHalfwordMask, static_cast<VectorRegister>(rs), wd, 0x19);
}

void Mips64Assembler::InsertW(VectorRegister wd, GpuRegister rs, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  EmitMsaELM(0x4, n2 | kMsaDfNWordMask, static_cast<VectorRegister>(rs), wd, 0x19);
}

void Mips64Assembler::InsertD(VectorRegister wd, GpuRegister rs, int n1) {
  CHECK(HasMsa());
  CHECK(IsUint<1>(n1)) << n1;
  EmitMsaELM(0x4, n1 | kMsaDfNDoublewordMask, static_cast<VectorRegister>(rs), wd, 0x19);
}

void Mips64Assembler::FillB(VectorRegister wd, GpuRegister rs) {
  CHECK(HasMsa());
  EmitMsa2R(0xc0, 0x0, static_cast<VectorRegister>(rs), wd, 0x1e);
}

void Mips64Assembler::FillH(VectorRegister wd, GpuRegister rs) {
  CHECK(HasMsa());
  EmitMsa2R(0xc0, 0x1, static_cast<VectorRegister>(rs), wd, 0x1e);
}

void Mips64Assembler::FillW(VectorRegister wd, GpuRegister rs) {
  CHECK(HasMsa());
  EmitMsa2R(0xc0, 0x2, static_cast<VectorRegister>(rs), wd, 0x1e);
}

void Mips64Assembler::FillD(VectorRegister wd, GpuRegister rs) {
  CHECK(HasMsa());
  EmitMsa2R(0xc0, 0x3, static_cast<VectorRegister>(rs), wd, 0x1e);
}

void Mips64Assembler::LdiB(VectorRegister wd, int imm8) {
  CHECK(HasMsa());
  CHECK(IsInt<8>(imm8)) << imm8;
  EmitMsaI10(0x6, 0x0, imm8 & kMsaS10Mask, wd, 0x7);
}

void Mips64Assembler::LdiH(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  EmitMsaI10(0x6, 0x1, imm10 & kMsaS10Mask, wd, 0x7);
}

void Mips64Assembler::LdiW(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  EmitMsaI10(0x6, 0x2, imm10 & kMsaS10Mask, wd, 0x7);
}

void Mips64Assembler::LdiD(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  EmitMsaI10(0x6, 0x3, imm10 & kMsaS10Mask, wd, 0x7);
}

void Mips64Assembler::LdB(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(offset)) << offset;
  EmitMsaMI10(offset & kMsaS10Mask, rs, wd, 0x8, 0x0);
}

void Mips64Assembler::LdH(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<11>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64HalfwordSize);
  EmitMsaMI10((offset >> TIMES_2) & kMsaS10Mask, rs, wd, 0x8, 0x1);
}

void Mips64Assembler::LdW(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<12>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64WordSize);
  EmitMsaMI10((offset >> TIMES_4) & kMsaS10Mask, rs, wd, 0x8, 0x2);
}

void Mips64Assembler::LdD(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<13>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64DoublewordSize);
  EmitMsaMI10((offset >> TIMES_8) & kMsaS10Mask, rs, wd, 0x8, 0x3);
}

void Mips64Assembler::StB(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(offset)) << offset;
  EmitMsaMI10(offset & kMsaS10Mask, rs, wd, 0x9, 0x0);
}

void Mips64Assembler::StH(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<11>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64HalfwordSize);
  EmitMsaMI10((offset >> TIMES_2) & kMsaS10Mask, rs, wd, 0x9, 0x1);
}

void Mips64Assembler::StW(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<12>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64WordSize);
  EmitMsaMI10((offset >> TIMES_4) & kMsaS10Mask, rs, wd, 0x9, 0x2);
}

void Mips64Assembler::StD(VectorRegister wd, GpuRegister rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<13>(offset)) << offset;
  CHECK_ALIGNED(offset, kMips64DoublewordSize);
  EmitMsaMI10((offset >> TIMES_8) & kMsaS10Mask, rs, wd, 0x9, 0x3);
}

void Mips64Assembler::IlvlB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvlH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvlW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvlD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvrB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvrH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvrW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvrD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvevB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvevH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvevW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvevD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvodB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvodH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvodW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x14);
}

void Mips64Assembler::IlvodD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x14);
}

void Mips64Assembler::MaddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::MaddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::MaddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::MaddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x1, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::MsubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x0, wt, ws, wd, 0x12);
}

void Mips64Assembler::MsubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x1, wt, ws, wd, 0x12);
}

void Mips64Assembler::MsubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x2, wt, ws, wd, 0x12);
}

void Mips64Assembler::MsubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x3, wt, ws, wd, 0x12);
}

void Mips64Assembler::FmaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x0, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x1, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x2, wt, ws, wd, 0x1b);
}

void Mips64Assembler::FmsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x2, 0x3, wt, ws, wd, 0x1b);
}

void Mips64Assembler::Hadd_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x15);
}

void Mips64Assembler::Hadd_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x15);
}

void Mips64Assembler::Hadd_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x15);
}

void Mips64Assembler::Hadd_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x15);
}

void Mips64Assembler::Hadd_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x15);
}

void Mips64Assembler::Hadd_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x15);
}

void Mips64Assembler::ReplicateFPToVectorRegister(VectorRegister dst,
                                                  FpuRegister src,
                                                  bool is_double) {
  // Float or double in FPU register Fx can be considered as 0th element in vector register Wx.
  if (is_double) {
    SplatiD(dst, static_cast<VectorRegister>(src), 0);
  } else {
    SplatiW(dst, static_cast<VectorRegister>(src), 0);
  }
}

void Mips64Assembler::LoadConst32(GpuRegister rd, int32_t value) {
  TemplateLoadConst32(this, rd, value);
}

// This function is only used for testing purposes.
void Mips64Assembler::RecordLoadConst64Path(int value ATTRIBUTE_UNUSED) {
}

void Mips64Assembler::LoadConst64(GpuRegister rd, int64_t value) {
  TemplateLoadConst64(this, rd, value);
}

void Mips64Assembler::Addiu32(GpuRegister rt, GpuRegister rs, int32_t value) {
  if (IsInt<16>(value)) {
    Addiu(rt, rs, value);
  } else {
    int16_t high = High16Bits(value);
    int16_t low = Low16Bits(value);
    high += (low < 0) ? 1 : 0;  // Account for sign extension in addiu.
    Aui(rt, rs, high);
    if (low != 0) {
      Addiu(rt, rt, low);
    }
  }
}

// TODO: don't use rtmp, use daui, dahi, dati.
void Mips64Assembler::Daddiu64(GpuRegister rt, GpuRegister rs, int64_t value, GpuRegister rtmp) {
  CHECK_NE(rs, rtmp);
  if (IsInt<16>(value)) {
    Daddiu(rt, rs, value);
  } else {
    LoadConst64(rtmp, value);
    Daddu(rt, rs, rtmp);
  }
}

void Mips64Assembler::Branch::InitShortOrLong(Mips64Assembler::Branch::OffsetBits offset_size,
                                              Mips64Assembler::Branch::Type short_type,
                                              Mips64Assembler::Branch::Type long_type) {
  type_ = (offset_size <= branch_info_[short_type].offset_size) ? short_type : long_type;
}

void Mips64Assembler::Branch::InitializeType(Type initial_type, bool is_r6) {
  OffsetBits offset_size_needed = GetOffsetSizeNeeded(location_, target_);
  if (is_r6) {
    // R6
    switch (initial_type) {
      case kLabel:
      case kLiteral:
      case kLiteralUnsigned:
      case kLiteralLong:
        CHECK(!IsResolved());
        type_ = initial_type;
        break;
      case kCall:
        InitShortOrLong(offset_size_needed, kCall, kLongCall);
        break;
      case kCondBranch:
        switch (condition_) {
          case kUncond:
            InitShortOrLong(offset_size_needed, kUncondBranch, kLongUncondBranch);
            break;
          case kCondEQZ:
          case kCondNEZ:
            // Special case for beqzc/bnezc with longer offset than in other b<cond>c instructions.
            type_ = (offset_size_needed <= kOffset23) ? kCondBranch : kLongCondBranch;
            break;
          default:
            InitShortOrLong(offset_size_needed, kCondBranch, kLongCondBranch);
            break;
        }
        break;
      case kBareCall:
        type_ = kBareCall;
        CHECK_LE(offset_size_needed, GetOffsetSize());
        break;
      case kBareCondBranch:
        type_ = (condition_ == kUncond) ? kBareUncondBranch : kBareCondBranch;
        CHECK_LE(offset_size_needed, GetOffsetSize());
        break;
      default:
        LOG(FATAL) << "Unexpected branch type " << initial_type;
        UNREACHABLE();
    }
  } else {
    // R2
    CHECK_EQ(initial_type, kBareCondBranch);
    switch (condition_) {
      case kCondLTZ:
      case kCondGEZ:
      case kCondLEZ:
      case kCondGTZ:
      case kCondEQ:
      case kCondNE:
      case kCondEQZ:
      case kCondNEZ:
        break;
      default:
        LOG(FATAL) << "Unexpected R2 branch condition " << condition_;
        UNREACHABLE();
    }
    type_ = kR2BareCondBranch;
    CHECK_LE(offset_size_needed, GetOffsetSize());
  }
  old_type_ = type_;
}

bool Mips64Assembler::Branch::IsNop(BranchCondition condition, GpuRegister lhs, GpuRegister rhs) {
  switch (condition) {
    case kCondLT:
    case kCondGT:
    case kCondNE:
    case kCondLTU:
      return lhs == rhs;
    default:
      return false;
  }
}

bool Mips64Assembler::Branch::IsUncond(BranchCondition condition,
                                       GpuRegister lhs,
                                       GpuRegister rhs) {
  switch (condition) {
    case kUncond:
      return true;
    case kCondGE:
    case kCondLE:
    case kCondEQ:
    case kCondGEU:
      return lhs == rhs;
    default:
      return false;
  }
}

Mips64Assembler::Branch::Branch(uint32_t location, uint32_t target, bool is_call, bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(ZERO),
      rhs_reg_(ZERO),
      condition_(kUncond) {
  InitializeType(
      (is_call ? (is_bare ? kBareCall : kCall) : (is_bare ? kBareCondBranch : kCondBranch)),
      /* is_r6 */ true);
}

Mips64Assembler::Branch::Branch(bool is_r6,
                                uint32_t location,
                                uint32_t target,
                                Mips64Assembler::BranchCondition condition,
                                GpuRegister lhs_reg,
                                GpuRegister rhs_reg,
                                bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(lhs_reg),
      rhs_reg_(rhs_reg),
      condition_(condition) {
  CHECK_NE(condition, kUncond);
  switch (condition) {
    case kCondEQ:
    case kCondNE:
    case kCondLT:
    case kCondGE:
    case kCondLE:
    case kCondGT:
    case kCondLTU:
    case kCondGEU:
      CHECK_NE(lhs_reg, ZERO);
      CHECK_NE(rhs_reg, ZERO);
      break;
    case kCondLTZ:
    case kCondGEZ:
    case kCondLEZ:
    case kCondGTZ:
    case kCondEQZ:
    case kCondNEZ:
      CHECK_NE(lhs_reg, ZERO);
      CHECK_EQ(rhs_reg, ZERO);
      break;
    case kCondF:
    case kCondT:
      CHECK_EQ(rhs_reg, ZERO);
      break;
    case kUncond:
      UNREACHABLE();
  }
  CHECK(!IsNop(condition, lhs_reg, rhs_reg));
  if (IsUncond(condition, lhs_reg, rhs_reg)) {
    // Branch condition is always true, make the branch unconditional.
    condition_ = kUncond;
  }
  InitializeType((is_bare ? kBareCondBranch : kCondBranch), is_r6);
}

Mips64Assembler::Branch::Branch(uint32_t location, GpuRegister dest_reg, Type label_or_literal_type)
    : old_location_(location),
      location_(location),
      target_(kUnresolved),
      lhs_reg_(dest_reg),
      rhs_reg_(ZERO),
      condition_(kUncond) {
  CHECK_NE(dest_reg, ZERO);
  InitializeType(label_or_literal_type, /* is_r6 */ true);
}

Mips64Assembler::BranchCondition Mips64Assembler::Branch::OppositeCondition(
    Mips64Assembler::BranchCondition cond) {
  switch (cond) {
    case kCondLT:
      return kCondGE;
    case kCondGE:
      return kCondLT;
    case kCondLE:
      return kCondGT;
    case kCondGT:
      return kCondLE;
    case kCondLTZ:
      return kCondGEZ;
    case kCondGEZ:
      return kCondLTZ;
    case kCondLEZ:
      return kCondGTZ;
    case kCondGTZ:
      return kCondLEZ;
    case kCondEQ:
      return kCondNE;
    case kCondNE:
      return kCondEQ;
    case kCondEQZ:
      return kCondNEZ;
    case kCondNEZ:
      return kCondEQZ;
    case kCondLTU:
      return kCondGEU;
    case kCondGEU:
      return kCondLTU;
    case kCondF:
      return kCondT;
    case kCondT:
      return kCondF;
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
  }
  UNREACHABLE();
}

Mips64Assembler::Branch::Type Mips64Assembler::Branch::GetType() const {
  return type_;
}

Mips64Assembler::BranchCondition Mips64Assembler::Branch::GetCondition() const {
  return condition_;
}

GpuRegister Mips64Assembler::Branch::GetLeftRegister() const {
  return lhs_reg_;
}

GpuRegister Mips64Assembler::Branch::GetRightRegister() const {
  return rhs_reg_;
}

uint32_t Mips64Assembler::Branch::GetTarget() const {
  return target_;
}

uint32_t Mips64Assembler::Branch::GetLocation() const {
  return location_;
}

uint32_t Mips64Assembler::Branch::GetOldLocation() const {
  return old_location_;
}

uint32_t Mips64Assembler::Branch::GetLength() const {
  return branch_info_[type_].length;
}

uint32_t Mips64Assembler::Branch::GetOldLength() const {
  return branch_info_[old_type_].length;
}

uint32_t Mips64Assembler::Branch::GetSize() const {
  return GetLength() * sizeof(uint32_t);
}

uint32_t Mips64Assembler::Branch::GetOldSize() const {
  return GetOldLength() * sizeof(uint32_t);
}

uint32_t Mips64Assembler::Branch::GetEndLocation() const {
  return GetLocation() + GetSize();
}

uint32_t Mips64Assembler::Branch::GetOldEndLocation() const {
  return GetOldLocation() + GetOldSize();
}

bool Mips64Assembler::Branch::IsBare() const {
  switch (type_) {
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
    // R2 short branches (can't be promoted to long), delay slots filled manually.
    case kR2BareCondBranch:
      return true;
    default:
      return false;
  }
}

bool Mips64Assembler::Branch::IsLong() const {
  switch (type_) {
    // R6 short branches (can be promoted to long).
    case kUncondBranch:
    case kCondBranch:
    case kCall:
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
    // R2 short branches (can't be promoted to long), delay slots filled manually.
    case kR2BareCondBranch:
    // Near label.
    case kLabel:
    // Near literals.
    case kLiteral:
    case kLiteralUnsigned:
    case kLiteralLong:
      return false;
    // Long branches.
    case kLongUncondBranch:
    case kLongCondBranch:
    case kLongCall:
    // Far label.
    case kFarLabel:
    // Far literals.
    case kFarLiteral:
    case kFarLiteralUnsigned:
    case kFarLiteralLong:
      return true;
  }
  UNREACHABLE();
}

bool Mips64Assembler::Branch::IsResolved() const {
  return target_ != kUnresolved;
}

Mips64Assembler::Branch::OffsetBits Mips64Assembler::Branch::GetOffsetSize() const {
  bool r6_cond_branch = (type_ == kCondBranch || type_ == kBareCondBranch);
  OffsetBits offset_size =
      (r6_cond_branch && (condition_ == kCondEQZ || condition_ == kCondNEZ))
          ? kOffset23
          : branch_info_[type_].offset_size;
  return offset_size;
}

Mips64Assembler::Branch::OffsetBits Mips64Assembler::Branch::GetOffsetSizeNeeded(uint32_t location,
                                                                                 uint32_t target) {
  // For unresolved targets assume the shortest encoding
  // (later it will be made longer if needed).
  if (target == kUnresolved)
    return kOffset16;
  int64_t distance = static_cast<int64_t>(target) - location;
  // To simplify calculations in composite branches consisting of multiple instructions
  // bump up the distance by a value larger than the max byte size of a composite branch.
  distance += (distance >= 0) ? kMaxBranchSize : -kMaxBranchSize;
  if (IsInt<kOffset16>(distance))
    return kOffset16;
  else if (IsInt<kOffset18>(distance))
    return kOffset18;
  else if (IsInt<kOffset21>(distance))
    return kOffset21;
  else if (IsInt<kOffset23>(distance))
    return kOffset23;
  else if (IsInt<kOffset28>(distance))
    return kOffset28;
  return kOffset32;
}

void Mips64Assembler::Branch::Resolve(uint32_t target) {
  target_ = target;
}

void Mips64Assembler::Branch::Relocate(uint32_t expand_location, uint32_t delta) {
  if (location_ > expand_location) {
    location_ += delta;
  }
  if (!IsResolved()) {
    return;  // Don't know the target yet.
  }
  if (target_ > expand_location) {
    target_ += delta;
  }
}

void Mips64Assembler::Branch::PromoteToLong() {
  CHECK(!IsBare());  // Bare branches do not promote.
  switch (type_) {
    // R6 short branches (can be promoted to long).
    case kUncondBranch:
      type_ = kLongUncondBranch;
      break;
    case kCondBranch:
      type_ = kLongCondBranch;
      break;
    case kCall:
      type_ = kLongCall;
      break;
    // Near label.
    case kLabel:
      type_ = kFarLabel;
      break;
    // Near literals.
    case kLiteral:
      type_ = kFarLiteral;
      break;
    case kLiteralUnsigned:
      type_ = kFarLiteralUnsigned;
      break;
    case kLiteralLong:
      type_ = kFarLiteralLong;
      break;
    default:
      // Note: 'type_' is already long.
      break;
  }
  CHECK(IsLong());
}

uint32_t Mips64Assembler::Branch::PromoteIfNeeded(uint32_t max_short_distance) {
  // If the branch is still unresolved or already long, nothing to do.
  if (IsLong() || !IsResolved()) {
    return 0;
  }
  // Promote the short branch to long if the offset size is too small
  // to hold the distance between location_ and target_.
  if (GetOffsetSizeNeeded(location_, target_) > GetOffsetSize()) {
    PromoteToLong();
    uint32_t old_size = GetOldSize();
    uint32_t new_size = GetSize();
    CHECK_GT(new_size, old_size);
    return new_size - old_size;
  }
  // The following logic is for debugging/testing purposes.
  // Promote some short branches to long when it's not really required.
  if (UNLIKELY(max_short_distance != std::numeric_limits<uint32_t>::max() && !IsBare())) {
    int64_t distance = static_cast<int64_t>(target_) - location_;
    distance = (distance >= 0) ? distance : -distance;
    if (distance >= max_short_distance) {
      PromoteToLong();
      uint32_t old_size = GetOldSize();
      uint32_t new_size = GetSize();
      CHECK_GT(new_size, old_size);
      return new_size - old_size;
    }
  }
  return 0;
}

uint32_t Mips64Assembler::Branch::GetOffsetLocation() const {
  return location_ + branch_info_[type_].instr_offset * sizeof(uint32_t);
}

uint32_t Mips64Assembler::Branch::GetOffset() const {
  CHECK(IsResolved());
  uint32_t ofs_mask = 0xFFFFFFFF >> (32 - GetOffsetSize());
  // Calculate the byte distance between instructions and also account for
  // different PC-relative origins.
  uint32_t offset_location = GetOffsetLocation();
  if (type_ == kLiteralLong) {
    // Special case for the ldpc instruction, whose address (PC) is rounded down to
    // a multiple of 8 before adding the offset.
    // Note, branch promotion has already taken care of aligning `target_` to an
    // address that's a multiple of 8.
    offset_location = RoundDown(offset_location, sizeof(uint64_t));
  }
  uint32_t offset = target_ - offset_location - branch_info_[type_].pc_org * sizeof(uint32_t);
  // Prepare the offset for encoding into the instruction(s).
  offset = (offset & ofs_mask) >> branch_info_[type_].offset_shift;
  return offset;
}

Mips64Assembler::Branch* Mips64Assembler::GetBranch(uint32_t branch_id) {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

const Mips64Assembler::Branch* Mips64Assembler::GetBranch(uint32_t branch_id) const {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

void Mips64Assembler::Bind(Mips64Label* label) {
  CHECK(!label->IsBound());
  uint32_t bound_pc = buffer_.Size();

  // Walk the list of branches referring to and preceding this label.
  // Store the previously unknown target addresses in them.
  while (label->IsLinked()) {
    uint32_t branch_id = label->Position();
    Branch* branch = GetBranch(branch_id);
    branch->Resolve(bound_pc);

    uint32_t branch_location = branch->GetLocation();
    // Extract the location of the previous branch in the list (walking the list backwards;
    // the previous branch ID was stored in the space reserved for this branch).
    uint32_t prev = buffer_.Load<uint32_t>(branch_location);

    // On to the previous branch in the list...
    label->position_ = prev;
  }

  // Now make the label object contain its own location (relative to the end of the preceding
  // branch, if any; it will be used by the branches referring to and following this label).
  label->prev_branch_id_plus_one_ = branches_.size();
  if (label->prev_branch_id_plus_one_) {
    uint32_t branch_id = label->prev_branch_id_plus_one_ - 1;
    const Branch* branch = GetBranch(branch_id);
    bound_pc -= branch->GetEndLocation();
  }
  label->BindTo(bound_pc);
}

uint32_t Mips64Assembler::GetLabelLocation(const Mips64Label* label) const {
  CHECK(label->IsBound());
  uint32_t target = label->Position();
  if (label->prev_branch_id_plus_one_) {
    // Get label location based on the branch preceding it.
    uint32_t branch_id = label->prev_branch_id_plus_one_ - 1;
    const Branch* branch = GetBranch(branch_id);
    target += branch->GetEndLocation();
  }
  return target;
}

uint32_t Mips64Assembler::GetAdjustedPosition(uint32_t old_position) {
  // We can reconstruct the adjustment by going through all the branches from the beginning
  // up to the old_position. Since we expect AdjustedPosition() to be called in a loop
  // with increasing old_position, we can use the data from last AdjustedPosition() to
  // continue where we left off and the whole loop should be O(m+n) where m is the number
  // of positions to adjust and n is the number of branches.
  if (old_position < last_old_position_) {
    last_position_adjustment_ = 0;
    last_old_position_ = 0;
    last_branch_id_ = 0;
  }
  while (last_branch_id_ != branches_.size()) {
    const Branch* branch = GetBranch(last_branch_id_);
    if (branch->GetLocation() >= old_position + last_position_adjustment_) {
      break;
    }
    last_position_adjustment_ += branch->GetSize() - branch->GetOldSize();
    ++last_branch_id_;
  }
  last_old_position_ = old_position;
  return old_position + last_position_adjustment_;
}

void Mips64Assembler::FinalizeLabeledBranch(Mips64Label* label) {
  uint32_t length = branches_.back().GetLength();
  if (!label->IsBound()) {
    // Branch forward (to a following label), distance is unknown.
    // The first branch forward will contain 0, serving as the terminator of
    // the list of forward-reaching branches.
    Emit(label->position_);
    length--;
    // Now make the label object point to this branch
    // (this forms a linked list of branches preceding this label).
    uint32_t branch_id = branches_.size() - 1;
    label->LinkTo(branch_id);
  }
  // Reserve space for the branch.
  while (length--) {
    Nop();
  }
}

void Mips64Assembler::Buncond(Mips64Label* label, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call */ false, is_bare);
  FinalizeLabeledBranch(label);
}

void Mips64Assembler::Bcond(Mips64Label* label,
                            bool is_r6,
                            bool is_bare,
                            BranchCondition condition,
                            GpuRegister lhs,
                            GpuRegister rhs) {
  // If lhs = rhs, this can be a NOP.
  if (Branch::IsNop(condition, lhs, rhs)) {
    return;
  }
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(is_r6, buffer_.Size(), target, condition, lhs, rhs, is_bare);
  FinalizeLabeledBranch(label);
}

void Mips64Assembler::Call(Mips64Label* label, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(buffer_.Size(), target, /* is_call */ true, is_bare);
  FinalizeLabeledBranch(label);
}

void Mips64Assembler::LoadLabelAddress(GpuRegister dest_reg, Mips64Label* label) {
  // Label address loads are treated as pseudo branches since they require very similar handling.
  DCHECK(!label->IsBound());
  branches_.emplace_back(buffer_.Size(), dest_reg, Branch::kLabel);
  FinalizeLabeledBranch(label);
}

Literal* Mips64Assembler::NewLiteral(size_t size, const uint8_t* data) {
  // We don't support byte and half-word literals.
  if (size == 4u) {
    literals_.emplace_back(size, data);
    return &literals_.back();
  } else {
    DCHECK_EQ(size, 8u);
    long_literals_.emplace_back(size, data);
    return &long_literals_.back();
  }
}

void Mips64Assembler::LoadLiteral(GpuRegister dest_reg,
                                  LoadOperandType load_type,
                                  Literal* literal) {
  // Literal loads are treated as pseudo branches since they require very similar handling.
  Branch::Type literal_type;
  switch (load_type) {
    case kLoadWord:
      DCHECK_EQ(literal->GetSize(), 4u);
      literal_type = Branch::kLiteral;
      break;
    case kLoadUnsignedWord:
      DCHECK_EQ(literal->GetSize(), 4u);
      literal_type = Branch::kLiteralUnsigned;
      break;
    case kLoadDoubleword:
      DCHECK_EQ(literal->GetSize(), 8u);
      literal_type = Branch::kLiteralLong;
      break;
    default:
      LOG(FATAL) << "Unexpected literal load type " << load_type;
      UNREACHABLE();
  }
  Mips64Label* label = literal->GetLabel();
  DCHECK(!label->IsBound());
  branches_.emplace_back(buffer_.Size(), dest_reg, literal_type);
  FinalizeLabeledBranch(label);
}

JumpTable* Mips64Assembler::CreateJumpTable(std::vector<Mips64Label*>&& labels) {
  jump_tables_.emplace_back(std::move(labels));
  JumpTable* table = &jump_tables_.back();
  DCHECK(!table->GetLabel()->IsBound());
  return table;
}

void Mips64Assembler::ReserveJumpTableSpace() {
  if (!jump_tables_.empty()) {
    for (JumpTable& table : jump_tables_) {
      Mips64Label* label = table.GetLabel();
      Bind(label);

      // Bulk ensure capacity, as this may be large.
      size_t orig_size = buffer_.Size();
      size_t required_capacity = orig_size + table.GetSize();
      if (required_capacity > buffer_.Capacity()) {
        buffer_.ExtendCapacity(required_capacity);
      }
#ifndef NDEBUG
      buffer_.has_ensured_capacity_ = true;
#endif

      // Fill the space with dummy data as the data is not final
      // until the branches have been promoted. And we shouldn't
      // be moving uninitialized data during branch promotion.
      for (size_t cnt = table.GetData().size(), i = 0; i < cnt; i++) {
        buffer_.Emit<uint32_t>(0x1abe1234u);
      }

#ifndef NDEBUG
      buffer_.has_ensured_capacity_ = false;
#endif
    }
  }
}

void Mips64Assembler::EmitJumpTables() {
  if (!jump_tables_.empty()) {
    CHECK(!overwriting_);
    // Switch from appending instructions at the end of the buffer to overwriting
    // existing instructions (here, jump tables) in the buffer.
    overwriting_ = true;

    for (JumpTable& table : jump_tables_) {
      Mips64Label* table_label = table.GetLabel();
      uint32_t start = GetLabelLocation(table_label);
      overwrite_location_ = start;

      for (Mips64Label* target : table.GetData()) {
        CHECK_EQ(buffer_.Load<uint32_t>(overwrite_location_), 0x1abe1234u);
        // The table will contain target addresses relative to the table start.
        uint32_t offset = GetLabelLocation(target) - start;
        Emit(offset);
      }
    }

    overwriting_ = false;
  }
}

void Mips64Assembler::EmitLiterals() {
  if (!literals_.empty()) {
    for (Literal& literal : literals_) {
      Mips64Label* label = literal.GetLabel();
      Bind(label);
      AssemblerBuffer::EnsureCapacity ensured(&buffer_);
      DCHECK_EQ(literal.GetSize(), 4u);
      for (size_t i = 0, size = literal.GetSize(); i != size; ++i) {
        buffer_.Emit<uint8_t>(literal.GetData()[i]);
      }
    }
  }
  if (!long_literals_.empty()) {
    // Reserve 4 bytes for potential alignment. If after the branch promotion the 64-bit
    // literals don't end up 8-byte-aligned, they will be moved down 4 bytes.
    Emit(0);  // NOP.
    for (Literal& literal : long_literals_) {
      Mips64Label* label = literal.GetLabel();
      Bind(label);
      AssemblerBuffer::EnsureCapacity ensured(&buffer_);
      DCHECK_EQ(literal.GetSize(), 8u);
      for (size_t i = 0, size = literal.GetSize(); i != size; ++i) {
        buffer_.Emit<uint8_t>(literal.GetData()[i]);
      }
    }
  }
}

void Mips64Assembler::PromoteBranches() {
  // Promote short branches to long as necessary.
  bool changed;
  do {
    changed = false;
    for (auto& branch : branches_) {
      CHECK(branch.IsResolved());
      uint32_t delta = branch.PromoteIfNeeded();
      // If this branch has been promoted and needs to expand in size,
      // relocate all branches by the expansion size.
      if (delta) {
        changed = true;
        uint32_t expand_location = branch.GetLocation();
        for (auto& branch2 : branches_) {
          branch2.Relocate(expand_location, delta);
        }
      }
    }
  } while (changed);

  // Account for branch expansion by resizing the code buffer
  // and moving the code in it to its final location.
  size_t branch_count = branches_.size();
  if (branch_count > 0) {
    // Resize.
    Branch& last_branch = branches_[branch_count - 1];
    uint32_t size_delta = last_branch.GetEndLocation() - last_branch.GetOldEndLocation();
    uint32_t old_size = buffer_.Size();
    buffer_.Resize(old_size + size_delta);
    // Move the code residing between branch placeholders.
    uint32_t end = old_size;
    for (size_t i = branch_count; i > 0; ) {
      Branch& branch = branches_[--i];
      uint32_t size = end - branch.GetOldEndLocation();
      buffer_.Move(branch.GetEndLocation(), branch.GetOldEndLocation(), size);
      end = branch.GetOldLocation();
    }
  }

  // Align 64-bit literals by moving them down by 4 bytes if needed.
  // This will reduce the PC-relative distance, which should be safe for both near and far literals.
  if (!long_literals_.empty()) {
    uint32_t first_literal_location = GetLabelLocation(long_literals_.front().GetLabel());
    size_t lit_size = long_literals_.size() * sizeof(uint64_t);
    size_t buf_size = buffer_.Size();
    // 64-bit literals must be at the very end of the buffer.
    CHECK_EQ(first_literal_location + lit_size, buf_size);
    if (!IsAligned<sizeof(uint64_t)>(first_literal_location)) {
      buffer_.Move(first_literal_location - sizeof(uint32_t), first_literal_location, lit_size);
      // The 4 reserved bytes proved useless, reduce the buffer size.
      buffer_.Resize(buf_size - sizeof(uint32_t));
      // Reduce target addresses in literal and address loads by 4 bytes in order for correct
      // offsets from PC to be generated.
      for (auto& branch : branches_) {
        uint32_t target = branch.GetTarget();
        if (target >= first_literal_location) {
          branch.Resolve(target - sizeof(uint32_t));
        }
      }
      // If after this we ever call GetLabelLocation() to get the location of a 64-bit literal,
      // we need to adjust the location of the literal's label as well.
      for (Literal& literal : long_literals_) {
        // Bound label's position is negative, hence incrementing it instead of decrementing.
        literal.GetLabel()->position_ += sizeof(uint32_t);
      }
    }
  }
}

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
const Mips64Assembler::Branch::BranchInfo Mips64Assembler::Branch::branch_info_[] = {
  // R6 short branches (can be promoted to long).
  {  1, 0, 1, Mips64Assembler::Branch::kOffset28, 2 },  // kUncondBranch
  {  2, 0, 1, Mips64Assembler::Branch::kOffset18, 2 },  // kCondBranch
                                                        // Exception: kOffset23 for beqzc/bnezc
  {  1, 0, 1, Mips64Assembler::Branch::kOffset28, 2 },  // kCall
  // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
  {  1, 0, 1, Mips64Assembler::Branch::kOffset28, 2 },  // kBareUncondBranch
  {  1, 0, 1, Mips64Assembler::Branch::kOffset18, 2 },  // kBareCondBranch
                                                        // Exception: kOffset23 for beqzc/bnezc
  {  1, 0, 1, Mips64Assembler::Branch::kOffset28, 2 },  // kBareCall
  // R2 short branches (can't be promoted to long), delay slots filled manually.
  {  1, 0, 1, Mips64Assembler::Branch::kOffset18, 2 },  // kR2BareCondBranch
  // Near label.
  {  1, 0, 0, Mips64Assembler::Branch::kOffset21, 2 },  // kLabel
  // Near literals.
  {  1, 0, 0, Mips64Assembler::Branch::kOffset21, 2 },  // kLiteral
  {  1, 0, 0, Mips64Assembler::Branch::kOffset21, 2 },  // kLiteralUnsigned
  {  1, 0, 0, Mips64Assembler::Branch::kOffset21, 3 },  // kLiteralLong
  // Long branches.
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kLongUncondBranch
  {  3, 1, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kLongCondBranch
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kLongCall
  // Far label.
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kFarLabel
  // Far literals.
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kFarLiteral
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kFarLiteralUnsigned
  {  2, 0, 0, Mips64Assembler::Branch::kOffset32, 0 },  // kFarLiteralLong
};

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
void Mips64Assembler::EmitBranch(Mips64Assembler::Branch* branch) {
  CHECK(overwriting_);
  overwrite_location_ = branch->GetLocation();
  uint32_t offset = branch->GetOffset();
  BranchCondition condition = branch->GetCondition();
  GpuRegister lhs = branch->GetLeftRegister();
  GpuRegister rhs = branch->GetRightRegister();
  switch (branch->GetType()) {
    // Short branches.
    case Branch::kUncondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bc(offset);
      break;
    case Branch::kCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR6(condition, lhs, rhs, offset);
      Nop();  // TODO: improve by filling the forbidden/delay slot.
      break;
    case Branch::kCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;
    case Branch::kBareUncondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bc(offset);
      break;
    case Branch::kBareCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR6(condition, lhs, rhs, offset);
      break;
    case Branch::kBareCall:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;
    case Branch::kR2BareCondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR2(condition, lhs, rhs, offset);
      break;

    // Near label.
    case Branch::kLabel:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Addiupc(lhs, offset);
      break;
    // Near literals.
    case Branch::kLiteral:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lwpc(lhs, offset);
      break;
    case Branch::kLiteralUnsigned:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lwupc(lhs, offset);
      break;
    case Branch::kLiteralLong:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Ldpc(lhs, offset);
      break;

    // Long branches.
    case Branch::kLongUncondBranch:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jic(AT, Low16Bits(offset));
      break;
    case Branch::kLongCondBranch:
      EmitBcondR6(Branch::OppositeCondition(condition), lhs, rhs, 2);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jic(AT, Low16Bits(offset));
      break;
    case Branch::kLongCall:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jialc.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jialc(AT, Low16Bits(offset));
      break;

    // Far label.
    case Branch::kFarLabel:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in daddiu.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Daddiu(lhs, AT, Low16Bits(offset));
      break;
    // Far literals.
    case Branch::kFarLiteral:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in lw.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Lw(lhs, AT, Low16Bits(offset));
      break;
    case Branch::kFarLiteralUnsigned:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in lwu.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Lwu(lhs, AT, Low16Bits(offset));
      break;
    case Branch::kFarLiteralLong:
      offset += (offset & 0x8000) << 1;  // Account for sign extension in ld.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Ld(lhs, AT, Low16Bits(offset));
      break;
  }
  CHECK_EQ(overwrite_location_, branch->GetEndLocation());
  CHECK_LT(branch->GetSize(), static_cast<uint32_t>(Branch::kMaxBranchSize));
}

void Mips64Assembler::Bc(Mips64Label* label, bool is_bare) {
  Buncond(label, is_bare);
}

void Mips64Assembler::Balc(Mips64Label* label, bool is_bare) {
  Call(label, is_bare);
}

void Mips64Assembler::Bltc(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLT, rs, rt);
}

void Mips64Assembler::Bltzc(GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLTZ, rt);
}

void Mips64Assembler::Bgtzc(GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGTZ, rt);
}

void Mips64Assembler::Bgec(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGE, rs, rt);
}

void Mips64Assembler::Bgezc(GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGEZ, rt);
}

void Mips64Assembler::Blezc(GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLEZ, rt);
}

void Mips64Assembler::Bltuc(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLTU, rs, rt);
}

void Mips64Assembler::Bgeuc(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGEU, rs, rt);
}

void Mips64Assembler::Beqc(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondEQ, rs, rt);
}

void Mips64Assembler::Bnec(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondNE, rs, rt);
}

void Mips64Assembler::Beqzc(GpuRegister rs, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondEQZ, rs);
}

void Mips64Assembler::Bnezc(GpuRegister rs, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondNEZ, rs);
}

void Mips64Assembler::Bc1eqz(FpuRegister ft, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondF, static_cast<GpuRegister>(ft), ZERO);
}

void Mips64Assembler::Bc1nez(FpuRegister ft, Mips64Label* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondT, static_cast<GpuRegister>(ft), ZERO);
}

void Mips64Assembler::Bltz(GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondLTZ, rt);
}

void Mips64Assembler::Bgtz(GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondGTZ, rt);
}

void Mips64Assembler::Bgez(GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondGEZ, rt);
}

void Mips64Assembler::Blez(GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondLEZ, rt);
}

void Mips64Assembler::Beq(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondEQ, rs, rt);
}

void Mips64Assembler::Bne(GpuRegister rs, GpuRegister rt, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondNE, rs, rt);
}

void Mips64Assembler::Beqz(GpuRegister rs, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondEQZ, rs);
}

void Mips64Assembler::Bnez(GpuRegister rs, Mips64Label* label, bool is_bare) {
  CHECK(is_bare);
  Bcond(label, /* is_r6 */ false, is_bare, kCondNEZ, rs);
}

void Mips64Assembler::AdjustBaseAndOffset(GpuRegister& base,
                                          int32_t& offset,
                                          bool is_doubleword) {
  // This method is used to adjust the base register and offset pair
  // for a load/store when the offset doesn't fit into int16_t.
  // It is assumed that `base + offset` is sufficiently aligned for memory
  // operands that are machine word in size or smaller. For doubleword-sized
  // operands it's assumed that `base` is a multiple of 8, while `offset`
  // may be a multiple of 4 (e.g. 4-byte-aligned long and double arguments
  // and spilled variables on the stack accessed relative to the stack
  // pointer register).
  // We preserve the "alignment" of `offset` by adjusting it by a multiple of 8.
  CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.

  bool doubleword_aligned = IsAligned<kMips64DoublewordSize>(offset);
  bool two_accesses = is_doubleword && !doubleword_aligned;

  // IsInt<16> must be passed a signed value, hence the static cast below.
  if (IsInt<16>(offset) &&
      (!two_accesses || IsInt<16>(static_cast<int32_t>(offset + kMips64WordSize)))) {
    // Nothing to do: `offset` (and, if needed, `offset + 4`) fits into int16_t.
    return;
  }

  // Remember the "(mis)alignment" of `offset`, it will be checked at the end.
  uint32_t misalignment = offset & (kMips64DoublewordSize - 1);

  // First, see if `offset` can be represented as a sum of two 16-bit signed
  // offsets. This can save an instruction.
  // To simplify matters, only do this for a symmetric range of offsets from
  // about -64KB to about +64KB, allowing further addition of 4 when accessing
  // 64-bit variables with two 32-bit accesses.
  constexpr int32_t kMinOffsetForSimpleAdjustment = 0x7ff8;  // Max int16_t that's a multiple of 8.
  constexpr int32_t kMaxOffsetForSimpleAdjustment = 2 * kMinOffsetForSimpleAdjustment;

  if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Daddiu(AT, base, kMinOffsetForSimpleAdjustment);
    offset -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Daddiu(AT, base, -kMinOffsetForSimpleAdjustment);
    offset += kMinOffsetForSimpleAdjustment;
  } else {
    // In more complex cases take advantage of the daui instruction, e.g.:
    //    daui   AT, base, offset_high
    //   [dahi   AT, 1]                       // When `offset` is close to +2GB.
    //    lw     reg_lo, offset_low(AT)
    //   [lw     reg_hi, (offset_low+4)(AT)]  // If misaligned 64-bit load.
    // or when offset_low+4 overflows int16_t:
    //    daui   AT, base, offset_high
    //    daddiu AT, AT, 8
    //    lw     reg_lo, (offset_low-8)(AT)
    //    lw     reg_hi, (offset_low-4)(AT)
    int16_t offset_low = Low16Bits(offset);
    int32_t offset_low32 = offset_low;
    int16_t offset_high = High16Bits(offset);
    bool increment_hi16 = offset_low < 0;
    bool overflow_hi16 = false;

    if (increment_hi16) {
      offset_high++;
      overflow_hi16 = (offset_high == -32768);
    }
    Daui(AT, base, offset_high);

    if (overflow_hi16) {
      Dahi(AT, 1);
    }

    if (two_accesses && !IsInt<16>(static_cast<int32_t>(offset_low32 + kMips64WordSize))) {
      // Avoid overflow in the 16-bit offset of the load/store instruction when adding 4.
      Daddiu(AT, AT, kMips64DoublewordSize);
      offset_low32 -= kMips64DoublewordSize;
    }

    offset = offset_low32;
  }
  base = AT;

  CHECK(IsInt<16>(offset));
  if (two_accesses) {
    CHECK(IsInt<16>(static_cast<int32_t>(offset + kMips64WordSize)));
  }
  CHECK_EQ(misalignment, offset & (kMips64DoublewordSize - 1));
}

void Mips64Assembler::AdjustBaseOffsetAndElementSizeShift(GpuRegister& base,
                                                          int32_t& offset,
                                                          int& element_size_shift) {
  // This method is used to adjust the base register, offset and element_size_shift
  // for a vector load/store when the offset doesn't fit into allowed number of bits.
  // MSA ld.df and st.df instructions take signed offsets as arguments, but maximum
  // offset is dependant on the size of the data format df (10-bit offsets for ld.b,
  // 11-bit for ld.h, 12-bit for ld.w and 13-bit for ld.d).
  // If element_size_shift is non-negative at entry, it won't be changed, but offset
  // will be checked for appropriate alignment. If negative at entry, it will be
  // adjusted based on offset for maximum fit.
  // It's assumed that `base` is a multiple of 8.

  CHECK_NE(base, AT);  // Must not overwrite the register `base` while loading `offset`.

  if (element_size_shift >= 0) {
    CHECK_LE(element_size_shift, TIMES_8);
    CHECK_GE(JAVASTYLE_CTZ(offset), element_size_shift);
  } else if (IsAligned<kMips64DoublewordSize>(offset)) {
    element_size_shift = TIMES_8;
  } else if (IsAligned<kMips64WordSize>(offset)) {
    element_size_shift = TIMES_4;
  } else if (IsAligned<kMips64HalfwordSize>(offset)) {
    element_size_shift = TIMES_2;
  } else {
    element_size_shift = TIMES_1;
  }

  const int low_len = 10 + element_size_shift;  // How many low bits of `offset` ld.df/st.df
                                                // will take.
  int16_t low = offset & ((1 << low_len) - 1);  // Isolate these bits.
  low -= (low & (1 << (low_len - 1))) << 1;     // Sign-extend these bits.
  if (low == offset) {
    return;  // `offset` fits into ld.df/st.df.
  }

  // First, see if `offset` can be represented as a sum of two signed offsets.
  // This can save an instruction.

  // Max int16_t that's a multiple of element size.
  const int32_t kMaxDeltaForSimpleAdjustment = 0x8000 - (1 << element_size_shift);
  // Max ld.df/st.df offset that's a multiple of element size.
  const int32_t kMaxLoadStoreOffset = 0x1ff << element_size_shift;
  const int32_t kMaxOffsetForSimpleAdjustment = kMaxDeltaForSimpleAdjustment + kMaxLoadStoreOffset;

  if (IsInt<16>(offset)) {
    Daddiu(AT, base, offset);
    offset = 0;
  } else if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Daddiu(AT, base, kMaxDeltaForSimpleAdjustment);
    offset -= kMaxDeltaForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Daddiu(AT, base, -kMaxDeltaForSimpleAdjustment);
    offset += kMaxDeltaForSimpleAdjustment;
  } else {
    // Let's treat `offset` as 64-bit to simplify handling of sign
    // extensions in the instructions that supply its smaller signed parts.
    //
    // 16-bit or smaller parts of `offset`:
    // |63  top  48|47  hi  32|31  upper  16|15  mid  13-10|12-9  low  0|
    //
    // Instructions that supply each part as a signed integer addend:
    // |dati       |dahi      |daui         |daddiu        |ld.df/st.df |
    //
    // `top` is always 0, so dati isn't used.
    // `hi` is 1 when `offset` is close to +2GB and 0 otherwise.
    uint64_t tmp = static_cast<uint64_t>(offset) - low;  // Exclude `low` from the rest of `offset`
                                                         // (accounts for sign of `low`).
    tmp += (tmp & (UINT64_C(1) << 15)) << 1;  // Account for sign extension in daddiu.
    tmp += (tmp & (UINT64_C(1) << 31)) << 1;  // Account for sign extension in daui.
    int16_t mid = Low16Bits(tmp);
    int16_t upper = High16Bits(tmp);
    int16_t hi = Low16Bits(High32Bits(tmp));
    Daui(AT, base, upper);
    if (hi != 0) {
      CHECK_EQ(hi, 1);
      Dahi(AT, hi);
    }
    if (mid != 0) {
      Daddiu(AT, AT, mid);
    }
    offset = low;
  }
  base = AT;
  CHECK_GE(JAVASTYLE_CTZ(offset), element_size_shift);
  CHECK(IsInt<10>(offset >> element_size_shift));
}

void Mips64Assembler::LoadFromOffset(LoadOperandType type,
                                     GpuRegister reg,
                                     GpuRegister base,
                                     int32_t offset) {
  LoadFromOffset<>(type, reg, base, offset);
}

void Mips64Assembler::LoadFpuFromOffset(LoadOperandType type,
                                        FpuRegister reg,
                                        GpuRegister base,
                                        int32_t offset) {
  LoadFpuFromOffset<>(type, reg, base, offset);
}

void Mips64Assembler::EmitLoad(ManagedRegister m_dst, GpuRegister src_register, int32_t src_offset,
                               size_t size) {
  Mips64ManagedRegister dst = m_dst.AsMips64();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsGpuRegister()) {
    if (size == 4) {
      LoadFromOffset(kLoadWord, dst.AsGpuRegister(), src_register, src_offset);
    } else if (size == 8) {
      CHECK_EQ(8u, size) << dst;
      LoadFromOffset(kLoadDoubleword, dst.AsGpuRegister(), src_register, src_offset);
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Load() of size 4 and 8";
    }
  } else if (dst.IsFpuRegister()) {
    if (size == 4) {
      CHECK_EQ(4u, size) << dst;
      LoadFpuFromOffset(kLoadWord, dst.AsFpuRegister(), src_register, src_offset);
    } else if (size == 8) {
      CHECK_EQ(8u, size) << dst;
      LoadFpuFromOffset(kLoadDoubleword, dst.AsFpuRegister(), src_register, src_offset);
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Load() of size 4 and 8";
    }
  }
}

void Mips64Assembler::StoreToOffset(StoreOperandType type,
                                    GpuRegister reg,
                                    GpuRegister base,
                                    int32_t offset) {
  StoreToOffset<>(type, reg, base, offset);
}

void Mips64Assembler::StoreFpuToOffset(StoreOperandType type,
                                       FpuRegister reg,
                                       GpuRegister base,
                                       int32_t offset) {
  StoreFpuToOffset<>(type, reg, base, offset);
}

static dwarf::Reg DWARFReg(GpuRegister reg) {
  return dwarf::Reg::Mips64Core(static_cast<int>(reg));
}

constexpr size_t kFramePointerSize = 8;

void Mips64Assembler::BuildFrame(size_t frame_size,
                                 ManagedRegister method_reg,
                                 ArrayRef<const ManagedRegister> callee_save_regs,
                                 const ManagedRegisterEntrySpills& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);

  // Increase frame to required size.
  IncreaseFrameSize(frame_size);

  // Push callee saves and return address
  int stack_offset = frame_size - kFramePointerSize;
  StoreToOffset(kStoreDoubleword, RA, SP, stack_offset);
  cfi_.RelOffset(DWARFReg(RA), stack_offset);
  for (int i = callee_save_regs.size() - 1; i >= 0; --i) {
    stack_offset -= kFramePointerSize;
    GpuRegister reg = callee_save_regs[i].AsMips64().AsGpuRegister();
    StoreToOffset(kStoreDoubleword, reg, SP, stack_offset);
    cfi_.RelOffset(DWARFReg(reg), stack_offset);
  }

  // Write out Method*.
  StoreToOffset(kStoreDoubleword, method_reg.AsMips64().AsGpuRegister(), SP, 0);

  // Write out entry spills.
  int32_t offset = frame_size + kFramePointerSize;
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    Mips64ManagedRegister reg = entry_spills[i].AsMips64();
    ManagedRegisterSpill spill = entry_spills.at(i);
    int32_t size = spill.getSize();
    if (reg.IsNoRegister()) {
      // only increment stack offset.
      offset += size;
    } else if (reg.IsFpuRegister()) {
      StoreFpuToOffset((size == 4) ? kStoreWord : kStoreDoubleword,
          reg.AsFpuRegister(), SP, offset);
      offset += size;
    } else if (reg.IsGpuRegister()) {
      StoreToOffset((size == 4) ? kStoreWord : kStoreDoubleword,
          reg.AsGpuRegister(), SP, offset);
      offset += size;
    }
  }
}

void Mips64Assembler::RemoveFrame(size_t frame_size,
                                  ArrayRef<const ManagedRegister> callee_save_regs,
                                  bool may_suspend ATTRIBUTE_UNUSED) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);
  cfi_.RememberState();

  // Pop callee saves and return address
  int stack_offset = frame_size - (callee_save_regs.size() * kFramePointerSize) - kFramePointerSize;
  for (size_t i = 0; i < callee_save_regs.size(); ++i) {
    GpuRegister reg = callee_save_regs[i].AsMips64().AsGpuRegister();
    LoadFromOffset(kLoadDoubleword, reg, SP, stack_offset);
    cfi_.Restore(DWARFReg(reg));
    stack_offset += kFramePointerSize;
  }
  LoadFromOffset(kLoadDoubleword, RA, SP, stack_offset);
  cfi_.Restore(DWARFReg(RA));

  // Decrease frame to required size.
  DecreaseFrameSize(frame_size);

  // Then jump to the return address.
  Jr(RA);
  Nop();

  // The CFI should be restored for any code that follows the exit block.
  cfi_.RestoreState();
  cfi_.DefCFAOffset(frame_size);
}

void Mips64Assembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Daddiu64(SP, SP, static_cast<int32_t>(-adjust));
  cfi_.AdjustCFAOffset(adjust);
}

void Mips64Assembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  DCHECK(!overwriting_);
  Daddiu64(SP, SP, static_cast<int32_t>(adjust));
  cfi_.AdjustCFAOffset(-adjust);
}

void Mips64Assembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  Mips64ManagedRegister src = msrc.AsMips64();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsGpuRegister()) {
    CHECK(size == 4 || size == 8) << size;
    if (size == 8) {
      StoreToOffset(kStoreDoubleword, src.AsGpuRegister(), SP, dest.Int32Value());
    } else if (size == 4) {
      StoreToOffset(kStoreWord, src.AsGpuRegister(), SP, dest.Int32Value());
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Store() of size 4 and 8";
    }
  } else if (src.IsFpuRegister()) {
    CHECK(size == 4 || size == 8) << size;
    if (size == 8) {
      StoreFpuToOffset(kStoreDoubleword, src.AsFpuRegister(), SP, dest.Int32Value());
    } else if (size == 4) {
      StoreFpuToOffset(kStoreWord, src.AsFpuRegister(), SP, dest.Int32Value());
    } else {
      UNIMPLEMENTED(FATAL) << "We only support Store() of size 4 and 8";
    }
  }
}

void Mips64Assembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  Mips64ManagedRegister src = msrc.AsMips64();
  CHECK(src.IsGpuRegister());
  StoreToOffset(kStoreWord, src.AsGpuRegister(), SP, dest.Int32Value());
}

void Mips64Assembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  Mips64ManagedRegister src = msrc.AsMips64();
  CHECK(src.IsGpuRegister());
  StoreToOffset(kStoreDoubleword, src.AsGpuRegister(), SP, dest.Int32Value());
}

void Mips64Assembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                            ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  LoadConst32(scratch.AsGpuRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsGpuRegister(), SP, dest.Int32Value());
}

void Mips64Assembler::StoreStackOffsetToThread(ThreadOffset64 thr_offs,
                                               FrameOffset fr_offs,
                                               ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  Daddiu64(scratch.AsGpuRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), S1, thr_offs.Int32Value());
}

void Mips64Assembler::StoreStackPointerToThread(ThreadOffset64 thr_offs) {
  StoreToOffset(kStoreDoubleword, SP, S1, thr_offs.Int32Value());
}

void Mips64Assembler::StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                                    FrameOffset in_off, ManagedRegister mscratch) {
  Mips64ManagedRegister src = msrc.AsMips64();
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  StoreToOffset(kStoreDoubleword, src.AsGpuRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), SP, dest.Int32Value() + 8);
}

void Mips64Assembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  return EmitLoad(mdest, SP, src.Int32Value(), size);
}

void Mips64Assembler::LoadFromThread(ManagedRegister mdest, ThreadOffset64 src, size_t size) {
  return EmitLoad(mdest, S1, src.Int32Value(), size);
}

void Mips64Assembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  Mips64ManagedRegister dest = mdest.AsMips64();
  CHECK(dest.IsGpuRegister());
  LoadFromOffset(kLoadUnsignedWord, dest.AsGpuRegister(), SP, src.Int32Value());
}

void Mips64Assembler::LoadRef(ManagedRegister mdest, ManagedRegister base, MemberOffset offs,
                              bool unpoison_reference) {
  Mips64ManagedRegister dest = mdest.AsMips64();
  CHECK(dest.IsGpuRegister() && base.AsMips64().IsGpuRegister());
  LoadFromOffset(kLoadUnsignedWord, dest.AsGpuRegister(),
                 base.AsMips64().AsGpuRegister(), offs.Int32Value());
  if (unpoison_reference) {
    MaybeUnpoisonHeapReference(dest.AsGpuRegister());
  }
}

void Mips64Assembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                                 Offset offs) {
  Mips64ManagedRegister dest = mdest.AsMips64();
  CHECK(dest.IsGpuRegister() && base.AsMips64().IsGpuRegister());
  LoadFromOffset(kLoadDoubleword, dest.AsGpuRegister(),
                 base.AsMips64().AsGpuRegister(), offs.Int32Value());
}

void Mips64Assembler::LoadRawPtrFromThread(ManagedRegister mdest, ThreadOffset64 offs) {
  Mips64ManagedRegister dest = mdest.AsMips64();
  CHECK(dest.IsGpuRegister());
  LoadFromOffset(kLoadDoubleword, dest.AsGpuRegister(), S1, offs.Int32Value());
}

void Mips64Assembler::SignExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                 size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No sign extension necessary for MIPS64";
}

void Mips64Assembler::ZeroExtend(ManagedRegister mreg ATTRIBUTE_UNUSED,
                                 size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No zero extension necessary for MIPS64";
}

void Mips64Assembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) {
  Mips64ManagedRegister dest = mdest.AsMips64();
  Mips64ManagedRegister src = msrc.AsMips64();
  if (!dest.Equals(src)) {
    if (dest.IsGpuRegister()) {
      CHECK(src.IsGpuRegister()) << src;
      Move(dest.AsGpuRegister(), src.AsGpuRegister());
    } else if (dest.IsFpuRegister()) {
      CHECK(src.IsFpuRegister()) << src;
      if (size == 4) {
        MovS(dest.AsFpuRegister(), src.AsFpuRegister());
      } else if (size == 8) {
        MovD(dest.AsFpuRegister(), src.AsFpuRegister());
      } else {
        UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
      }
    }
  }
}

void Mips64Assembler::CopyRef(FrameOffset dest, FrameOffset src,
                              ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsGpuRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsGpuRegister(), SP, dest.Int32Value());
}

void Mips64Assembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                           ThreadOffset64 thr_offs,
                                           ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(), S1, thr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), SP, fr_offs.Int32Value());
}

void Mips64Assembler::CopyRawPtrToThread(ThreadOffset64 thr_offs,
                                         FrameOffset fr_offs,
                                         ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(),
                 SP, fr_offs.Int32Value());
  StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(),
                S1, thr_offs.Int32Value());
}

void Mips64Assembler::Copy(FrameOffset dest, FrameOffset src,
                           ManagedRegister mscratch, size_t size) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch.AsGpuRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Mips64Assembler::Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                           ManagedRegister mscratch, size_t size) {
  GpuRegister scratch = mscratch.AsMips64().AsGpuRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, src_base.AsMips64().AsGpuRegister(),
                   src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, SP, dest.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch, src_base.AsMips64().AsGpuRegister(),
                   src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, SP, dest.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Mips64Assembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                           ManagedRegister mscratch, size_t size) {
  GpuRegister scratch = mscratch.AsMips64().AsGpuRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, dest_base.AsMips64().AsGpuRegister(),
                  dest_offset.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch, SP, src.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, dest_base.AsMips64().AsGpuRegister(),
                  dest_offset.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Mips64Assembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                           FrameOffset src_base ATTRIBUTE_UNUSED,
                           Offset src_offset ATTRIBUTE_UNUSED,
                           ManagedRegister mscratch ATTRIBUTE_UNUSED,
                           size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No MIPS64 implementation";
}

void Mips64Assembler::Copy(ManagedRegister dest, Offset dest_offset,
                           ManagedRegister src, Offset src_offset,
                           ManagedRegister mscratch, size_t size) {
  GpuRegister scratch = mscratch.AsMips64().AsGpuRegister();
  CHECK(size == 4 || size == 8) << size;
  if (size == 4) {
    LoadFromOffset(kLoadWord, scratch, src.AsMips64().AsGpuRegister(), src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, dest.AsMips64().AsGpuRegister(), dest_offset.Int32Value());
  } else if (size == 8) {
    LoadFromOffset(kLoadDoubleword, scratch, src.AsMips64().AsGpuRegister(),
                   src_offset.Int32Value());
    StoreToOffset(kStoreDoubleword, scratch, dest.AsMips64().AsGpuRegister(),
                  dest_offset.Int32Value());
  } else {
    UNIMPLEMENTED(FATAL) << "We only support Copy() of size 4 and 8";
  }
}

void Mips64Assembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                           Offset dest_offset ATTRIBUTE_UNUSED,
                           FrameOffset src ATTRIBUTE_UNUSED,
                           Offset src_offset ATTRIBUTE_UNUSED,
                           ManagedRegister mscratch ATTRIBUTE_UNUSED,
                           size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No MIPS64 implementation";
}

void Mips64Assembler::MemoryBarrier(ManagedRegister mreg ATTRIBUTE_UNUSED) {
  // TODO: sync?
  UNIMPLEMENTED(FATAL) << "No MIPS64 implementation";
}

void Mips64Assembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                             FrameOffset handle_scope_offset,
                                             ManagedRegister min_reg,
                                             bool null_allowed) {
  Mips64ManagedRegister out_reg = mout_reg.AsMips64();
  Mips64ManagedRegister in_reg = min_reg.AsMips64();
  CHECK(in_reg.IsNoRegister() || in_reg.IsGpuRegister()) << in_reg;
  CHECK(out_reg.IsGpuRegister()) << out_reg;
  if (null_allowed) {
    Mips64Label null_arg;
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset)
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(kLoadUnsignedWord, out_reg.AsGpuRegister(),
                     SP, handle_scope_offset.Int32Value());
      in_reg = out_reg;
    }
    if (!out_reg.Equals(in_reg)) {
      LoadConst32(out_reg.AsGpuRegister(), 0);
    }
    Beqzc(in_reg.AsGpuRegister(), &null_arg);
    Daddiu64(out_reg.AsGpuRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Daddiu64(out_reg.AsGpuRegister(), SP, handle_scope_offset.Int32Value());
  }
}

void Mips64Assembler::CreateHandleScopeEntry(FrameOffset out_off,
                                             FrameOffset handle_scope_offset,
                                             ManagedRegister mscratch,
                                             bool null_allowed) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  if (null_allowed) {
    Mips64Label null_arg;
    LoadFromOffset(kLoadUnsignedWord, scratch.AsGpuRegister(), SP,
                   handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // e.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset)
    Beqzc(scratch.AsGpuRegister(), &null_arg);
    Daddiu64(scratch.AsGpuRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Daddiu64(scratch.AsGpuRegister(), SP, handle_scope_offset.Int32Value());
  }
  StoreToOffset(kStoreDoubleword, scratch.AsGpuRegister(), SP, out_off.Int32Value());
}

// Given a handle scope entry, load the associated reference.
void Mips64Assembler::LoadReferenceFromHandleScope(ManagedRegister mout_reg,
                                                   ManagedRegister min_reg) {
  Mips64ManagedRegister out_reg = mout_reg.AsMips64();
  Mips64ManagedRegister in_reg = min_reg.AsMips64();
  CHECK(out_reg.IsGpuRegister()) << out_reg;
  CHECK(in_reg.IsGpuRegister()) << in_reg;
  Mips64Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadConst32(out_reg.AsGpuRegister(), 0);
  }
  Beqzc(in_reg.AsGpuRegister(), &null_arg);
  LoadFromOffset(kLoadDoubleword, out_reg.AsGpuRegister(),
                 in_reg.AsGpuRegister(), 0);
  Bind(&null_arg);
}

void Mips64Assembler::VerifyObject(ManagedRegister src ATTRIBUTE_UNUSED,
                                   bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references
}

void Mips64Assembler::VerifyObject(FrameOffset src ATTRIBUTE_UNUSED,
                                   bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references
}

void Mips64Assembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister mscratch) {
  Mips64ManagedRegister base = mbase.AsMips64();
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(base.IsGpuRegister()) << base;
  CHECK(scratch.IsGpuRegister()) << scratch;
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(),
                 base.AsGpuRegister(), offset.Int32Value());
  Jalr(scratch.AsGpuRegister());
  Nop();
  // TODO: place reference map on call
}

void Mips64Assembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  CHECK(scratch.IsGpuRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(),
                 SP, base.Int32Value());
  LoadFromOffset(kLoadDoubleword, scratch.AsGpuRegister(),
                 scratch.AsGpuRegister(), offset.Int32Value());
  Jalr(scratch.AsGpuRegister());
  Nop();
  // TODO: place reference map on call
}

void Mips64Assembler::CallFromThread(ThreadOffset64 offset ATTRIBUTE_UNUSED,
                                     ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "No MIPS64 implementation";
}

void Mips64Assembler::GetCurrentThread(ManagedRegister tr) {
  Move(tr.AsMips64().AsGpuRegister(), S1);
}

void Mips64Assembler::GetCurrentThread(FrameOffset offset,
                                       ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  StoreToOffset(kStoreDoubleword, S1, SP, offset.Int32Value());
}

void Mips64Assembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  Mips64ManagedRegister scratch = mscratch.AsMips64();
  exception_blocks_.emplace_back(scratch, stack_adjust);
  LoadFromOffset(kLoadDoubleword,
                 scratch.AsGpuRegister(),
                 S1,
                 Thread::ExceptionOffset<kMips64PointerSize>().Int32Value());
  Bnezc(scratch.AsGpuRegister(), exception_blocks_.back().Entry());
}

void Mips64Assembler::EmitExceptionPoll(Mips64ExceptionSlowPath* exception) {
  Bind(exception->Entry());
  if (exception->stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSize(exception->stack_adjust_);
  }
  // Pass exception object as argument.
  // Don't care about preserving A0 as this call won't return.
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
  Move(A0, exception->scratch_.AsGpuRegister());
  // Set up call to Thread::Current()->pDeliverException
  LoadFromOffset(kLoadDoubleword,
                 T9,
                 S1,
                 QUICK_ENTRYPOINT_OFFSET(kMips64PointerSize, pDeliverException).Int32Value());
  Jr(T9);
  Nop();

  // Call never returns
  Break();
}

}  // namespace mips64
}  // namespace art
