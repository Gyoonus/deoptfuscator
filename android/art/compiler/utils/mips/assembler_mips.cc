/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "assembler_mips.h"

#include "base/bit_utils.h"
#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "memory_region.h"
#include "thread.h"

namespace art {
namespace mips {

static_assert(static_cast<size_t>(kMipsPointerSize) == kMipsWordSize,
              "Unexpected Mips pointer size.");
static_assert(kMipsPointerSize == PointerSize::k32, "Unexpected Mips pointer size.");


std::ostream& operator<<(std::ostream& os, const DRegister& rhs) {
  if (rhs >= D0 && rhs < kNumberOfDRegisters) {
    os << "d" << static_cast<int>(rhs);
  } else {
    os << "DRegister[" << static_cast<int>(rhs) << "]";
  }
  return os;
}

MipsAssembler::DelaySlot::DelaySlot()
    : instruction_(0),
      patcher_label_(nullptr) {}

InOutRegMasks& MipsAssembler::DsFsmInstr(uint32_t instruction, MipsLabel* patcher_label) {
  if (!reordering_) {
    CHECK_EQ(ds_fsm_state_, kExpectingLabel);
    CHECK_EQ(delay_slot_.instruction_, 0u);
    return delay_slot_.masks_;
  }
  switch (ds_fsm_state_) {
    case kExpectingLabel:
      break;
    case kExpectingInstruction:
      CHECK_EQ(ds_fsm_target_pc_ + sizeof(uint32_t), buffer_.Size());
      // If the last instruction is not suitable for delay slots, drop
      // the PC of the label preceding it so that no unconditional branch
      // uses this instruction to fill its delay slot.
      if (instruction == 0) {
        DsFsmDropLabel();  // Sets ds_fsm_state_ = kExpectingLabel.
      } else {
        // Otherwise wait for another instruction or label before we can
        // commit the label PC. The label PC will be dropped if instead
        // of another instruction or label there's a call from the code
        // generator to CodePosition() to record the buffer size.
        // Instructions after which the buffer size is recorded cannot
        // be moved into delay slots or anywhere else because they may
        // trigger signals and the signal handlers expect these signals
        // to be coming from the instructions immediately preceding the
        // recorded buffer locations.
        ds_fsm_state_ = kExpectingCommit;
      }
      break;
    case kExpectingCommit:
      CHECK_EQ(ds_fsm_target_pc_ + 2 * sizeof(uint32_t), buffer_.Size());
      DsFsmCommitLabel();  // Sets ds_fsm_state_ = kExpectingLabel.
      break;
  }
  delay_slot_.instruction_ = instruction;
  delay_slot_.masks_ = InOutRegMasks();
  delay_slot_.patcher_label_ = patcher_label;
  return delay_slot_.masks_;
}

void MipsAssembler::DsFsmLabel() {
  if (!reordering_) {
    CHECK_EQ(ds_fsm_state_, kExpectingLabel);
    CHECK_EQ(delay_slot_.instruction_, 0u);
    return;
  }
  switch (ds_fsm_state_) {
    case kExpectingLabel:
      ds_fsm_target_pc_ = buffer_.Size();
      ds_fsm_state_ = kExpectingInstruction;
      break;
    case kExpectingInstruction:
      // Allow consecutive labels.
      CHECK_EQ(ds_fsm_target_pc_, buffer_.Size());
      break;
    case kExpectingCommit:
      CHECK_EQ(ds_fsm_target_pc_ + sizeof(uint32_t), buffer_.Size());
      DsFsmCommitLabel();
      ds_fsm_target_pc_ = buffer_.Size();
      ds_fsm_state_ = kExpectingInstruction;
      break;
  }
  // We cannot move instructions into delay slots across labels.
  delay_slot_.instruction_ = 0;
}

void MipsAssembler::DsFsmCommitLabel() {
  if (ds_fsm_state_ == kExpectingCommit) {
    ds_fsm_target_pcs_.emplace_back(ds_fsm_target_pc_);
  }
  ds_fsm_state_ = kExpectingLabel;
}

void MipsAssembler::DsFsmDropLabel() {
  ds_fsm_state_ = kExpectingLabel;
}

bool MipsAssembler::SetReorder(bool enable) {
  bool last_state = reordering_;
  if (last_state != enable) {
    DsFsmCommitLabel();
    DsFsmInstrNop(0);
  }
  reordering_ = enable;
  return last_state;
}

size_t MipsAssembler::CodePosition() {
  // The last instruction cannot be used in a delay slot, do not commit
  // the label before it (if any) and clear the delay slot.
  DsFsmDropLabel();
  DsFsmInstrNop(0);
  size_t size = buffer_.Size();
  // In theory we can get the following sequence:
  //   label1:
  //     instr
  //   label2: # label1 gets committed when label2 is seen
  //     CodePosition() call
  // and we need to uncommit label1.
  if (ds_fsm_target_pcs_.size() != 0 && ds_fsm_target_pcs_.back() + sizeof(uint32_t) == size) {
    ds_fsm_target_pcs_.pop_back();
  }
  return size;
}

void MipsAssembler::DsFsmInstrNop(uint32_t instruction ATTRIBUTE_UNUSED) {
  DsFsmInstr(0);
}

void MipsAssembler::FinalizeCode() {
  for (auto& exception_block : exception_blocks_) {
    EmitExceptionPoll(&exception_block);
  }
  // Commit the last branch target label (if any) and disable instruction reordering.
  DsFsmCommitLabel();
  SetReorder(false);
  EmitLiterals();
  ReserveJumpTableSpace();
  PromoteBranches();
}

void MipsAssembler::FinalizeInstructions(const MemoryRegion& region) {
  size_t number_of_delayed_adjust_pcs = cfi().NumberOfDelayedAdvancePCs();
  EmitBranches();
  EmitJumpTables();
  Assembler::FinalizeInstructions(region);
  PatchCFI(number_of_delayed_adjust_pcs);
}

void MipsAssembler::PatchCFI(size_t number_of_delayed_adjust_pcs) {
  if (cfi().NumberOfDelayedAdvancePCs() == 0u) {
    DCHECK_EQ(number_of_delayed_adjust_pcs, 0u);
    return;
  }

  typedef DebugFrameOpCodeWriterForAssembler::DelayedAdvancePC DelayedAdvancePC;
  const auto data = cfi().ReleaseStreamAndPrepareForDelayedAdvancePC();
  const std::vector<uint8_t>& old_stream = data.first;
  const std::vector<DelayedAdvancePC>& advances = data.second;

  // PCs recorded before EmitBranches() need to be adjusted.
  // PCs recorded during EmitBranches() are already adjusted.
  // Both ranges are separately sorted but they may overlap.
  if (kIsDebugBuild) {
    auto cmp = [](const DelayedAdvancePC& lhs, const DelayedAdvancePC& rhs) {
      return lhs.pc < rhs.pc;
    };
    CHECK(std::is_sorted(advances.begin(), advances.begin() + number_of_delayed_adjust_pcs, cmp));
    CHECK(std::is_sorted(advances.begin() + number_of_delayed_adjust_pcs, advances.end(), cmp));
  }

  // Append initial CFI data if any.
  size_t size = advances.size();
  DCHECK_NE(size, 0u);
  cfi().AppendRawData(old_stream, 0u, advances[0].stream_pos);
  // Emit PC adjustments interleaved with the old CFI stream.
  size_t adjust_pos = 0u;
  size_t late_emit_pos = number_of_delayed_adjust_pcs;
  while (adjust_pos != number_of_delayed_adjust_pcs || late_emit_pos != size) {
    size_t adjusted_pc = (adjust_pos != number_of_delayed_adjust_pcs)
        ? GetAdjustedPosition(advances[adjust_pos].pc)
        : static_cast<size_t>(-1);
    size_t late_emit_pc = (late_emit_pos != size)
        ? advances[late_emit_pos].pc
        : static_cast<size_t>(-1);
    size_t advance_pc = std::min(adjusted_pc, late_emit_pc);
    DCHECK_NE(advance_pc, static_cast<size_t>(-1));
    size_t entry = (adjusted_pc <= late_emit_pc) ? adjust_pos : late_emit_pos;
    if (adjusted_pc <= late_emit_pc) {
      ++adjust_pos;
    } else {
      ++late_emit_pos;
    }
    cfi().AdvancePC(advance_pc);
    size_t end_pos = (entry + 1u == size) ? old_stream.size() : advances[entry + 1u].stream_pos;
    cfi().AppendRawData(old_stream, advances[entry].stream_pos, end_pos);
  }
}

void MipsAssembler::EmitBranches() {
  CHECK(!overwriting_);
  CHECK(!reordering_);
  // Now that everything has its final position in the buffer (the branches have
  // been promoted), adjust the target label PCs.
  for (size_t cnt = ds_fsm_target_pcs_.size(), i = 0; i < cnt; i++) {
    ds_fsm_target_pcs_[i] = GetAdjustedPosition(ds_fsm_target_pcs_[i]);
  }
  // Switch from appending instructions at the end of the buffer to overwriting
  // existing instructions (branch placeholders) in the buffer.
  overwriting_ = true;
  for (size_t id = 0; id < branches_.size(); id++) {
    EmitBranch(id);
  }
  overwriting_ = false;
}

void MipsAssembler::Emit(uint32_t value) {
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

uint32_t MipsAssembler::EmitR(int opcode,
                              Register rs,
                              Register rt,
                              Register rd,
                              int shamt,
                              int funct) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rd, kNoRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      static_cast<uint32_t>(rt) << kRtShift |
                      static_cast<uint32_t>(rd) << kRdShift |
                      shamt << kShamtShift |
                      funct;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitI(int opcode, Register rs, Register rt, uint16_t imm) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      static_cast<uint32_t>(rt) << kRtShift |
                      imm;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitI21(int opcode, Register rs, uint32_t imm21) {
  CHECK_NE(rs, kNoRegister);
  CHECK(IsUint<21>(imm21)) << imm21;
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      static_cast<uint32_t>(rs) << kRsShift |
                      imm21;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitI26(int opcode, uint32_t imm26) {
  CHECK(IsUint<26>(imm26)) << imm26;
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift | imm26;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitFR(int opcode,
                               int fmt,
                               FRegister ft,
                               FRegister fs,
                               FRegister fd,
                               int funct) {
  CHECK_NE(ft, kNoFRegister);
  CHECK_NE(fs, kNoFRegister);
  CHECK_NE(fd, kNoFRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      fmt << kFmtShift |
                      static_cast<uint32_t>(ft) << kFtShift |
                      static_cast<uint32_t>(fs) << kFsShift |
                      static_cast<uint32_t>(fd) << kFdShift |
                      funct;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitFI(int opcode, int fmt, FRegister ft, uint16_t imm) {
  CHECK_NE(ft, kNoFRegister);
  uint32_t encoding = static_cast<uint32_t>(opcode) << kOpcodeShift |
                      fmt << kFmtShift |
                      static_cast<uint32_t>(ft) << kFtShift |
                      imm;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitMsa3R(int operation,
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
  return encoding;
}

uint32_t MipsAssembler::EmitMsaBIT(int operation,
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
  return encoding;
}

uint32_t MipsAssembler::EmitMsaELM(int operation,
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
  return encoding;
}

uint32_t MipsAssembler::EmitMsaMI10(int s10,
                                    Register rs,
                                    VectorRegister wd,
                                    int minor_opcode,
                                    int df) {
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(wd, kNoVectorRegister);
  CHECK(IsUint<10>(s10)) << s10;
  uint32_t encoding = static_cast<uint32_t>(kMsaMajorOpcode) << kOpcodeShift |
                      s10 << kS10Shift |
                      static_cast<uint32_t>(rs) << kWsShift |
                      static_cast<uint32_t>(wd) << kWdShift |
                      minor_opcode << kS10MinorShift |
                      df;
  Emit(encoding);
  return encoding;
}

uint32_t MipsAssembler::EmitMsaI10(int operation,
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
  return encoding;
}

uint32_t MipsAssembler::EmitMsa2R(int operation,
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
  return encoding;
}

uint32_t MipsAssembler::EmitMsa2RF(int operation,
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
  return encoding;
}

void MipsAssembler::Addu(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x21)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Addiu(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label) {
  if (patcher_label != nullptr) {
    Bind(patcher_label);
  }
  DsFsmInstr(EmitI(0x9, rs, rt, imm16), patcher_label).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Addiu(Register rt, Register rs, uint16_t imm16) {
  Addiu(rt, rs, imm16, /* patcher_label */ nullptr);
}

void MipsAssembler::Subu(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x23)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::MultR2(Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x18)).GprIns(rs, rt);
}

void MipsAssembler::MultuR2(Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x19)).GprIns(rs, rt);
}

void MipsAssembler::DivR2(Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1a)).GprIns(rs, rt);
}

void MipsAssembler::DivuR2(Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, static_cast<Register>(0), 0, 0x1b)).GprIns(rs, rt);
}

void MipsAssembler::MulR2(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0x1c, rs, rt, rd, 0, 2)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::DivR2(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DivR2(rs, rt);
  Mflo(rd);
}

void MipsAssembler::ModR2(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DivR2(rs, rt);
  Mfhi(rd);
}

void MipsAssembler::DivuR2(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DivuR2(rs, rt);
  Mflo(rd);
}

void MipsAssembler::ModuR2(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DivuR2(rs, rt);
  Mfhi(rd);
}

void MipsAssembler::MulR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 2, 0x18)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::MuhR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 3, 0x18)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::MuhuR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 3, 0x19)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::DivR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 2, 0x1a)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::ModR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 3, 0x1a)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::DivuR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 2, 0x1b)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::ModuR6(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 3, 0x1b)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::And(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x24)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Andi(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0xc, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Or(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x25)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Ori(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0xd, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Xor(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x26)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Xori(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0xe, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Nor(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x27)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Movz(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x0A)).GprInOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Movn(Register rd, Register rs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x0B)).GprInOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Seleqz(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x35)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Selnez(Register rd, Register rs, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x37)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::ClzR6(Register rd, Register rs) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, static_cast<Register>(0), rd, 0x01, 0x10)).GprOuts(rd).GprIns(rs);
}

void MipsAssembler::ClzR2(Register rd, Register rs) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0x1C, rs, rd, rd, 0, 0x20)).GprOuts(rd).GprIns(rs);
}

void MipsAssembler::CloR6(Register rd, Register rs) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0, rs, static_cast<Register>(0), rd, 0x01, 0x11)).GprOuts(rd).GprIns(rs);
}

void MipsAssembler::CloR2(Register rd, Register rs) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0x1C, rs, rd, rd, 0, 0x21)).GprOuts(rd).GprIns(rs);
}

void MipsAssembler::Seb(Register rd, Register rt) {
  DsFsmInstr(EmitR(0x1f, static_cast<Register>(0), rt, rd, 0x10, 0x20)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Seh(Register rd, Register rt) {
  DsFsmInstr(EmitR(0x1f, static_cast<Register>(0), rt, rd, 0x18, 0x20)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Wsbh(Register rd, Register rt) {
  DsFsmInstr(EmitR(0x1f, static_cast<Register>(0), rt, rd, 2, 0x20)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Bitswap(Register rd, Register rt) {
  CHECK(IsR6());
  DsFsmInstr(EmitR(0x1f, static_cast<Register>(0), rt, rd, 0x0, 0x20)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Sll(Register rd, Register rt, int shamt) {
  CHECK(IsUint<5>(shamt)) << shamt;
  DsFsmInstr(EmitR(0, static_cast<Register>(0), rt, rd, shamt, 0x00)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Srl(Register rd, Register rt, int shamt) {
  CHECK(IsUint<5>(shamt)) << shamt;
  DsFsmInstr(EmitR(0, static_cast<Register>(0), rt, rd, shamt, 0x02)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Rotr(Register rd, Register rt, int shamt) {
  CHECK(IsUint<5>(shamt)) << shamt;
  DsFsmInstr(EmitR(0, static_cast<Register>(1), rt, rd, shamt, 0x02)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Sra(Register rd, Register rt, int shamt) {
  CHECK(IsUint<5>(shamt)) << shamt;
  DsFsmInstr(EmitR(0, static_cast<Register>(0), rt, rd, shamt, 0x03)).GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Sllv(Register rd, Register rt, Register rs) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x04)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Srlv(Register rd, Register rt, Register rs) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x06)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Rotrv(Register rd, Register rt, Register rs) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 1, 0x06)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Srav(Register rd, Register rt, Register rs) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x07)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Ext(Register rd, Register rt, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(0 < size && size <= 32) << size;
  CHECK(0 < pos + size && pos + size <= 32) << pos << " + " << size;
  DsFsmInstr(EmitR(0x1f, rt, rd, static_cast<Register>(size - 1), pos, 0x00))
      .GprOuts(rd).GprIns(rt);
}

void MipsAssembler::Ins(Register rd, Register rt, int pos, int size) {
  CHECK(IsUint<5>(pos)) << pos;
  CHECK(0 < size && size <= 32) << size;
  CHECK(0 < pos + size && pos + size <= 32) << pos << " + " << size;
  DsFsmInstr(EmitR(0x1f, rt, rd, static_cast<Register>(pos + size - 1), pos, 0x04))
      .GprInOuts(rd).GprIns(rt);
}

void MipsAssembler::Lsa(Register rd, Register rs, Register rt, int saPlusOne) {
  CHECK(IsR6() || HasMsa());
  CHECK(1 <= saPlusOne && saPlusOne <= 4) << saPlusOne;
  int sa = saPlusOne - 1;
  DsFsmInstr(EmitR(0x0, rs, rt, rd, sa, 0x05)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::ShiftAndAdd(Register dst,
                                Register src_idx,
                                Register src_base,
                                int shamt,
                                Register tmp) {
  CHECK(0 <= shamt && shamt <= 4) << shamt;
  CHECK_NE(src_base, tmp);
  if (shamt == TIMES_1) {
    // Catch the special case where the shift amount is zero (0).
    Addu(dst, src_base, src_idx);
  } else if (IsR6() || HasMsa()) {
    Lsa(dst, src_idx, src_base, shamt);
  } else {
    Sll(tmp, src_idx, shamt);
    Addu(dst, src_base, tmp);
  }
}

void MipsAssembler::Lb(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x20, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Lh(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x21, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Lw(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label) {
  if (patcher_label != nullptr) {
    Bind(patcher_label);
  }
  DsFsmInstr(EmitI(0x23, rs, rt, imm16), patcher_label).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Lw(Register rt, Register rs, uint16_t imm16) {
  Lw(rt, rs, imm16, /* patcher_label */ nullptr);
}

void MipsAssembler::Lwl(Register rt, Register rs, uint16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x22, rs, rt, imm16)).GprInOuts(rt).GprIns(rs);
}

void MipsAssembler::Lwr(Register rt, Register rs, uint16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x26, rs, rt, imm16)).GprInOuts(rt).GprIns(rs);
}

void MipsAssembler::Lbu(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x24, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Lhu(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x25, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Lwpc(Register rs, uint32_t imm19) {
  CHECK(IsR6());
  CHECK(IsUint<19>(imm19)) << imm19;
  DsFsmInstrNop(EmitI21(0x3B, rs, (0x01 << 19) | imm19));
}

void MipsAssembler::Lui(Register rt, uint16_t imm16) {
  DsFsmInstr(EmitI(0xf, static_cast<Register>(0), rt, imm16)).GprOuts(rt);
}

void MipsAssembler::Aui(Register rt, Register rs, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstr(EmitI(0xf, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::AddUpper(Register rt, Register rs, uint16_t imm16, Register tmp) {
  bool increment = (rs == rt);
  if (increment) {
    CHECK_NE(rs, tmp);
  }
  if (IsR6()) {
    Aui(rt, rs, imm16);
  } else if (increment) {
    Lui(tmp, imm16);
    Addu(rt, rs, tmp);
  } else {
    Lui(rt, imm16);
    Addu(rt, rs, rt);
  }
}

void MipsAssembler::Sync(uint32_t stype) {
  DsFsmInstrNop(EmitR(0, ZERO, ZERO, ZERO, stype & 0x1f, 0xf));
}

void MipsAssembler::Mfhi(Register rd) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, ZERO, ZERO, rd, 0, 0x10)).GprOuts(rd);
}

void MipsAssembler::Mflo(Register rd) {
  CHECK(!IsR6());
  DsFsmInstr(EmitR(0, ZERO, ZERO, rd, 0, 0x12)).GprOuts(rd);
}

void MipsAssembler::Sb(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x28, rs, rt, imm16)).GprIns(rt, rs);
}

void MipsAssembler::Sh(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x29, rs, rt, imm16)).GprIns(rt, rs);
}

void MipsAssembler::Sw(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label) {
  if (patcher_label != nullptr) {
    Bind(patcher_label);
  }
  DsFsmInstr(EmitI(0x2b, rs, rt, imm16), patcher_label).GprIns(rt, rs);
}

void MipsAssembler::Sw(Register rt, Register rs, uint16_t imm16) {
  Sw(rt, rs, imm16, /* patcher_label */ nullptr);
}

void MipsAssembler::Swl(Register rt, Register rs, uint16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x2a, rs, rt, imm16)).GprIns(rt, rs);
}

void MipsAssembler::Swr(Register rt, Register rs, uint16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x2e, rs, rt, imm16)).GprIns(rt, rs);
}

void MipsAssembler::LlR2(Register rt, Register base, int16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x30, base, rt, imm16)).GprOuts(rt).GprIns(base);
}

void MipsAssembler::ScR2(Register rt, Register base, int16_t imm16) {
  CHECK(!IsR6());
  DsFsmInstr(EmitI(0x38, base, rt, imm16)).GprInOuts(rt).GprIns(base);
}

void MipsAssembler::LlR6(Register rt, Register base, int16_t imm9) {
  CHECK(IsR6());
  CHECK(IsInt<9>(imm9));
  DsFsmInstr(EmitI(0x1f, base, rt, ((imm9 & 0x1ff) << 7) | 0x36)).GprOuts(rt).GprIns(base);
}

void MipsAssembler::ScR6(Register rt, Register base, int16_t imm9) {
  CHECK(IsR6());
  CHECK(IsInt<9>(imm9));
  DsFsmInstr(EmitI(0x1f, base, rt, ((imm9 & 0x1ff) << 7) | 0x26)).GprInOuts(rt).GprIns(base);
}

void MipsAssembler::Slt(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x2a)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Sltu(Register rd, Register rs, Register rt) {
  DsFsmInstr(EmitR(0, rs, rt, rd, 0, 0x2b)).GprOuts(rd).GprIns(rs, rt);
}

void MipsAssembler::Slti(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0xa, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::Sltiu(Register rt, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0xb, rs, rt, imm16)).GprOuts(rt).GprIns(rs);
}

void MipsAssembler::B(uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x4, static_cast<Register>(0), static_cast<Register>(0), imm16));
}

void MipsAssembler::Bal(uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x1, static_cast<Register>(0), static_cast<Register>(0x11), imm16));
}

void MipsAssembler::Beq(Register rs, Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x4, rs, rt, imm16));
}

void MipsAssembler::Bne(Register rs, Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x5, rs, rt, imm16));
}

void MipsAssembler::Beqz(Register rt, uint16_t imm16) {
  Beq(rt, ZERO, imm16);
}

void MipsAssembler::Bnez(Register rt, uint16_t imm16) {
  Bne(rt, ZERO, imm16);
}

void MipsAssembler::Bltz(Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x1, rt, static_cast<Register>(0), imm16));
}

void MipsAssembler::Bgez(Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x1, rt, static_cast<Register>(0x1), imm16));
}

void MipsAssembler::Blez(Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x6, rt, static_cast<Register>(0), imm16));
}

void MipsAssembler::Bgtz(Register rt, uint16_t imm16) {
  DsFsmInstrNop(EmitI(0x7, rt, static_cast<Register>(0), imm16));
}

void MipsAssembler::Bc1f(uint16_t imm16) {
  Bc1f(0, imm16);
}

void MipsAssembler::Bc1f(int cc, uint16_t imm16) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstrNop(EmitI(0x11, static_cast<Register>(0x8), static_cast<Register>(cc << 2), imm16));
}

void MipsAssembler::Bc1t(uint16_t imm16) {
  Bc1t(0, imm16);
}

void MipsAssembler::Bc1t(int cc, uint16_t imm16) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstrNop(EmitI(0x11,
                      static_cast<Register>(0x8),
                      static_cast<Register>((cc << 2) | 1),
                      imm16));
}

void MipsAssembler::J(uint32_t addr26) {
  DsFsmInstrNop(EmitI26(0x2, addr26));
}

void MipsAssembler::Jal(uint32_t addr26) {
  DsFsmInstrNop(EmitI26(0x3, addr26));
}

void MipsAssembler::Jalr(Register rd, Register rs) {
  uint32_t last_instruction = delay_slot_.instruction_;
  MipsLabel* patcher_label = delay_slot_.patcher_label_;
  bool exchange = (last_instruction != 0 &&
      (delay_slot_.masks_.gpr_outs_ & (1u << rs)) == 0 &&
      ((delay_slot_.masks_.gpr_ins_ | delay_slot_.masks_.gpr_outs_) & (1u << rd)) == 0);
  if (exchange) {
    // The last instruction cannot be used in a different delay slot,
    // do not commit the label before it (if any).
    DsFsmDropLabel();
  }
  DsFsmInstrNop(EmitR(0, rs, static_cast<Register>(0), rd, 0, 0x09));
  if (exchange) {
    // Exchange the last two instructions in the assembler buffer.
    size_t size = buffer_.Size();
    CHECK_GE(size, 2 * sizeof(uint32_t));
    size_t pos1 = size - 2 * sizeof(uint32_t);
    size_t pos2 = size - sizeof(uint32_t);
    uint32_t instr1 = buffer_.Load<uint32_t>(pos1);
    uint32_t instr2 = buffer_.Load<uint32_t>(pos2);
    CHECK_EQ(instr1, last_instruction);
    buffer_.Store<uint32_t>(pos1, instr2);
    buffer_.Store<uint32_t>(pos2, instr1);
    // Move the patcher label along with the patched instruction.
    if (patcher_label != nullptr) {
      patcher_label->AdjustBoundPosition(sizeof(uint32_t));
    }
  } else if (reordering_) {
    Nop();
  }
}

void MipsAssembler::Jalr(Register rs) {
  Jalr(RA, rs);
}

void MipsAssembler::Jr(Register rs) {
  Jalr(ZERO, rs);
}

void MipsAssembler::Nal() {
  DsFsmInstrNop(EmitI(0x1, static_cast<Register>(0), static_cast<Register>(0x10), 0));
}

void MipsAssembler::Auipc(Register rs, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitI(0x3B, rs, static_cast<Register>(0x1E), imm16));
}

void MipsAssembler::Addiupc(Register rs, uint32_t imm19) {
  CHECK(IsR6());
  CHECK(IsUint<19>(imm19)) << imm19;
  DsFsmInstrNop(EmitI21(0x3B, rs, imm19));
}

void MipsAssembler::Bc(uint32_t imm26) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitI26(0x32, imm26));
}

void MipsAssembler::Balc(uint32_t imm26) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitI26(0x3A, imm26));
}

void MipsAssembler::Jic(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitI(0x36, static_cast<Register>(0), rt, imm16));
}

void MipsAssembler::Jialc(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitI(0x3E, static_cast<Register>(0), rt, imm16));
}

void MipsAssembler::Bltc(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x17, rs, rt, imm16));
}

void MipsAssembler::Bltzc(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rt, ZERO);
  DsFsmInstrNop(EmitI(0x17, rt, rt, imm16));
}

void MipsAssembler::Bgtzc(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rt, ZERO);
  DsFsmInstrNop(EmitI(0x17, static_cast<Register>(0), rt, imm16));
}

void MipsAssembler::Bgec(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x16, rs, rt, imm16));
}

void MipsAssembler::Bgezc(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rt, ZERO);
  DsFsmInstrNop(EmitI(0x16, rt, rt, imm16));
}

void MipsAssembler::Blezc(Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rt, ZERO);
  DsFsmInstrNop(EmitI(0x16, static_cast<Register>(0), rt, imm16));
}

void MipsAssembler::Bltuc(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x7, rs, rt, imm16));
}

void MipsAssembler::Bgeuc(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x6, rs, rt, imm16));
}

void MipsAssembler::Beqc(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x8, std::min(rs, rt), std::max(rs, rt), imm16));
}

void MipsAssembler::Bnec(Register rs, Register rt, uint16_t imm16) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  CHECK_NE(rt, ZERO);
  CHECK_NE(rs, rt);
  DsFsmInstrNop(EmitI(0x18, std::min(rs, rt), std::max(rs, rt), imm16));
}

void MipsAssembler::Beqzc(Register rs, uint32_t imm21) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  DsFsmInstrNop(EmitI21(0x36, rs, imm21));
}

void MipsAssembler::Bnezc(Register rs, uint32_t imm21) {
  CHECK(IsR6());
  CHECK_NE(rs, ZERO);
  DsFsmInstrNop(EmitI21(0x3E, rs, imm21));
}

void MipsAssembler::Bc1eqz(FRegister ft, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitFI(0x11, 0x9, ft, imm16));
}

void MipsAssembler::Bc1nez(FRegister ft, uint16_t imm16) {
  CHECK(IsR6());
  DsFsmInstrNop(EmitFI(0x11, 0xD, ft, imm16));
}

void MipsAssembler::EmitBcondR2(BranchCondition cond, Register rs, Register rt, uint16_t imm16) {
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
      CHECK_EQ(rt, ZERO);
      Bc1f(static_cast<int>(rs), imm16);
      break;
    case kCondT:
      CHECK_EQ(rt, ZERO);
      Bc1t(static_cast<int>(rs), imm16);
      break;
    case kCondLT:
    case kCondGE:
    case kCondLE:
    case kCondGT:
    case kCondLTU:
    case kCondGEU:
    case kUncond:
      // We don't support synthetic R2 branches (preceded with slt[u]) at this level
      // (R2 doesn't have branches to compare 2 registers using <, <=, >=, >).
      LOG(FATAL) << "Unexpected branch condition " << cond;
      UNREACHABLE();
  }
}

void MipsAssembler::EmitBcondR6(BranchCondition cond, Register rs, Register rt, uint32_t imm16_21) {
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
      Bc1eqz(static_cast<FRegister>(rs), imm16_21);
      break;
    case kCondT:
      CHECK_EQ(rt, ZERO);
      Bc1nez(static_cast<FRegister>(rs), imm16_21);
      break;
    case kUncond:
      LOG(FATAL) << "Unexpected branch condition " << cond;
      UNREACHABLE();
  }
}

void MipsAssembler::AddS(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x0)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SubS(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x1)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::MulS(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x2)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::DivS(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x3)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::AddD(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x0)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SubD(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x1)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::MulD(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x2)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::DivD(FRegister fd, FRegister fs, FRegister ft) {
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x3)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SqrtS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x4)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::SqrtD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x4)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::AbsS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x5)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::AbsD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x5)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::MovS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x6)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::MovD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x6)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::NegS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x7)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::NegD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x7)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::CunS(FRegister fs, FRegister ft) {
  CunS(0, fs, ft);
}

void MipsAssembler::CunS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x31))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CeqS(FRegister fs, FRegister ft) {
  CeqS(0, fs, ft);
}

void MipsAssembler::CeqS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x32))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CueqS(FRegister fs, FRegister ft) {
  CueqS(0, fs, ft);
}

void MipsAssembler::CueqS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x33))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::ColtS(FRegister fs, FRegister ft) {
  ColtS(0, fs, ft);
}

void MipsAssembler::ColtS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x34))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CultS(FRegister fs, FRegister ft) {
  CultS(0, fs, ft);
}

void MipsAssembler::CultS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x35))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::ColeS(FRegister fs, FRegister ft) {
  ColeS(0, fs, ft);
}

void MipsAssembler::ColeS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x36))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CuleS(FRegister fs, FRegister ft) {
  CuleS(0, fs, ft);
}

void MipsAssembler::CuleS(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, static_cast<FRegister>(cc << 2), 0x37))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CunD(FRegister fs, FRegister ft) {
  CunD(0, fs, ft);
}

void MipsAssembler::CunD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x31))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CeqD(FRegister fs, FRegister ft) {
  CeqD(0, fs, ft);
}

void MipsAssembler::CeqD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x32))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CueqD(FRegister fs, FRegister ft) {
  CueqD(0, fs, ft);
}

void MipsAssembler::CueqD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x33))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::ColtD(FRegister fs, FRegister ft) {
  ColtD(0, fs, ft);
}

void MipsAssembler::ColtD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x34))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CultD(FRegister fs, FRegister ft) {
  CultD(0, fs, ft);
}

void MipsAssembler::CultD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x35))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::ColeD(FRegister fs, FRegister ft) {
  ColeD(0, fs, ft);
}

void MipsAssembler::ColeD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x36))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CuleD(FRegister fs, FRegister ft) {
  CuleD(0, fs, ft);
}

void MipsAssembler::CuleD(int cc, FRegister fs, FRegister ft) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, static_cast<FRegister>(cc << 2), 0x37))
      .CcOuts(cc).FprIns(fs, ft);
}

void MipsAssembler::CmpUnS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x01)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpEqS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x02)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUeqS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x03)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpLtS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x04)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUltS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x05)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpLeS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x06)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUleS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x07)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpOrS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x11)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUneS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x12)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpNeS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x14, ft, fs, fd, 0x13)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUnD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x01)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpEqD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x02)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUeqD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x03)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpLtD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x04)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUltD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x05)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpLeD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x06)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUleD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x07)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpOrD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x11)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpUneD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x12)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::CmpNeD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x15, ft, fs, fd, 0x13)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::Movf(Register rd, Register rs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitR(0, rs, static_cast<Register>(cc << 2), rd, 0, 0x01))
      .GprInOuts(rd).GprIns(rs).CcIns(cc);
}

void MipsAssembler::Movt(Register rd, Register rs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitR(0, rs, static_cast<Register>((cc << 2) | 1), rd, 0, 0x01))
      .GprInOuts(rd).GprIns(rs).CcIns(cc);
}

void MipsAssembler::MovfS(FRegister fd, FRegister fs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(cc << 2), fs, fd, 0x11))
      .FprInOuts(fd).FprIns(fs).CcIns(cc);
}

void MipsAssembler::MovfD(FRegister fd, FRegister fs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(cc << 2), fs, fd, 0x11))
      .FprInOuts(fd).FprIns(fs).CcIns(cc);
}

void MipsAssembler::MovtS(FRegister fd, FRegister fs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>((cc << 2) | 1), fs, fd, 0x11))
      .FprInOuts(fd).FprIns(fs).CcIns(cc);
}

void MipsAssembler::MovtD(FRegister fd, FRegister fs, int cc) {
  CHECK(!IsR6());
  CHECK(IsUint<3>(cc)) << cc;
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>((cc << 2) | 1), fs, fd, 0x11))
      .FprInOuts(fd).FprIns(fs).CcIns(cc);
}

void MipsAssembler::MovzS(FRegister fd, FRegister fs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(rt), fs, fd, 0x12))
      .FprInOuts(fd).FprIns(fs).GprIns(rt);
}

void MipsAssembler::MovzD(FRegister fd, FRegister fs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(rt), fs, fd, 0x12))
      .FprInOuts(fd).FprIns(fs).GprIns(rt);
}

void MipsAssembler::MovnS(FRegister fd, FRegister fs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(rt), fs, fd, 0x13))
      .FprInOuts(fd).FprIns(fs).GprIns(rt);
}

void MipsAssembler::MovnD(FRegister fd, FRegister fs, Register rt) {
  CHECK(!IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(rt), fs, fd, 0x13))
      .FprInOuts(fd).FprIns(fs).GprIns(rt);
}

void MipsAssembler::SelS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x10)).FprInOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SelD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x10)).FprInOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SeleqzS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x14)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SeleqzD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x14)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SelnezS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x17)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::SelnezD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x17)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::ClassS(FRegister fd, FRegister fs) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x1b)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::ClassD(FRegister fd, FRegister fs) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x1b)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::MinS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x1c)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::MinD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x1c)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::MaxS(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x10, ft, fs, fd, 0x1e)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::MaxD(FRegister fd, FRegister fs, FRegister ft) {
  CHECK(IsR6());
  DsFsmInstr(EmitFR(0x11, 0x11, ft, fs, fd, 0x1e)).FprOuts(fd).FprIns(fs, ft);
}

void MipsAssembler::TruncLS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x09)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::TruncLD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x09)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::TruncWS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x0D)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::TruncWD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x0D)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtsw(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x14, static_cast<FRegister>(0), fs, fd, 0x20)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtdw(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x14, static_cast<FRegister>(0), fs, fd, 0x21)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtsd(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0x20)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtds(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0x21)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtsl(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x15, static_cast<FRegister>(0), fs, fd, 0x20)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::Cvtdl(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x15, static_cast<FRegister>(0), fs, fd, 0x21)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::FloorWS(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x10, static_cast<FRegister>(0), fs, fd, 0xf)).FprOuts(fd).FprIns(fs);
}

void MipsAssembler::FloorWD(FRegister fd, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x11, static_cast<FRegister>(0), fs, fd, 0xf)).FprOuts(fd).FprIns(fs);
}

FRegister MipsAssembler::GetFpuRegLow(FRegister reg) {
  // If FPRs are 32-bit (and get paired to hold 64-bit values), accesses to
  // odd-numbered FPRs are reattributed to even-numbered FPRs. This lets us
  // use only even-numbered FPRs irrespective of whether we're doing single-
  // or double-precision arithmetic. (We don't use odd-numbered 32-bit FPRs
  // to hold single-precision values).
  return Is32BitFPU() ? static_cast<FRegister>(reg & ~1u) : reg;
}

void MipsAssembler::Mfc1(Register rt, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x00, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0))
      .GprOuts(rt).FprIns(GetFpuRegLow(fs));
}

// Note, the 32 LSBs of a 64-bit value must be loaded into an FPR before the 32 MSBs
// when loading the value as 32-bit halves.
void MipsAssembler::Mtc1(Register rt, FRegister fs) {
  uint32_t encoding =
      EmitFR(0x11, 0x04, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0);
  if (Is32BitFPU() && (fs % 2 != 0)) {
    // If mtc1 is used to simulate mthc1 by writing to the odd-numbered FPR in
    // a pair of 32-bit FPRs, the associated even-numbered FPR is an in/out.
    DsFsmInstr(encoding).FprInOuts(GetFpuRegLow(fs)).GprIns(rt);
  } else {
    // Otherwise (the FPR is 64-bit or even-numbered), the FPR is an out.
    DsFsmInstr(encoding).FprOuts(fs).GprIns(rt);
  }
}

void MipsAssembler::Mfhc1(Register rt, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x03, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0))
      .GprOuts(rt).FprIns(fs);
}

// Note, the 32 LSBs of a 64-bit value must be loaded into an FPR before the 32 MSBs
// when loading the value as 32-bit halves.
void MipsAssembler::Mthc1(Register rt, FRegister fs) {
  DsFsmInstr(EmitFR(0x11, 0x07, static_cast<FRegister>(rt), fs, static_cast<FRegister>(0), 0x0))
      .FprInOuts(fs).GprIns(rt);
}

void MipsAssembler::MoveFromFpuHigh(Register rt, FRegister fs) {
  if (Is32BitFPU()) {
    CHECK_EQ(fs % 2, 0) << fs;
    Mfc1(rt, static_cast<FRegister>(fs + 1));
  } else {
    Mfhc1(rt, fs);
  }
}

void MipsAssembler::MoveToFpuHigh(Register rt, FRegister fs) {
  if (Is32BitFPU()) {
    CHECK_EQ(fs % 2, 0) << fs;
    Mtc1(rt, static_cast<FRegister>(fs + 1));
  } else {
    Mthc1(rt, fs);
  }
}

// Note, the 32 LSBs of a 64-bit value must be loaded into an FPR before the 32 MSBs
// when loading the value as 32-bit halves.
void MipsAssembler::Lwc1(FRegister ft, Register rs, uint16_t imm16) {
  uint32_t encoding = EmitI(0x31, rs, static_cast<Register>(ft), imm16);
  if (Is32BitFPU() && (ft % 2 != 0)) {
    // If lwc1 is used to load the odd-numbered FPR in a pair of 32-bit FPRs,
    // the associated even-numbered FPR is an in/out.
    DsFsmInstr(encoding).FprInOuts(GetFpuRegLow(ft)).GprIns(rs);
  } else {
    // Otherwise (the FPR is 64-bit or even-numbered), the FPR is an out.
    DsFsmInstr(encoding).FprOuts(ft).GprIns(rs);
  }
}

void MipsAssembler::Ldc1(FRegister ft, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x35, rs, static_cast<Register>(ft), imm16)).FprOuts(ft).GprIns(rs);
}

void MipsAssembler::Swc1(FRegister ft, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x39, rs, static_cast<Register>(ft), imm16)).FprIns(GetFpuRegLow(ft)).GprIns(rs);
}

void MipsAssembler::Sdc1(FRegister ft, Register rs, uint16_t imm16) {
  DsFsmInstr(EmitI(0x3d, rs, static_cast<Register>(ft), imm16)).FprIns(ft).GprIns(rs);
}

void MipsAssembler::Break() {
  DsFsmInstrNop(EmitR(0, ZERO, ZERO, ZERO, 0, 0xD));
}

void MipsAssembler::Nop() {
  DsFsmInstrNop(EmitR(0x0, ZERO, ZERO, ZERO, 0, 0x0));
}

void MipsAssembler::NopIfNoReordering() {
  if (!reordering_) {
    Nop();
  }
}

void MipsAssembler::Move(Register rd, Register rs) {
  Or(rd, rs, ZERO);
}

void MipsAssembler::Clear(Register rd) {
  Move(rd, ZERO);
}

void MipsAssembler::Not(Register rd, Register rs) {
  Nor(rd, rs, ZERO);
}

void MipsAssembler::Push(Register rs) {
  IncreaseFrameSize(kStackAlignment);
  Sw(rs, SP, 0);
}

void MipsAssembler::Pop(Register rd) {
  Lw(rd, SP, 0);
  DecreaseFrameSize(kStackAlignment);
}

void MipsAssembler::PopAndReturn(Register rd, Register rt) {
  bool reordering = SetReorder(false);
  Lw(rd, SP, 0);
  Jr(rt);
  DecreaseFrameSize(kStackAlignment);  // Single instruction in delay slot.
  SetReorder(reordering);
}

void MipsAssembler::AndV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::OrV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::NorV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::XorV(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::AddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::AddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::AddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::AddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MulvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MulvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MulvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MulvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Div_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Mod_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x12)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Add_aB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Add_aH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Add_aW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Add_aD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ave_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Aver_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x10)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x3, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x3, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x3, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Max_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x3, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x0, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Min_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0xe)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmulW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x0, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmulD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x1, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FdivW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x2, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FdivD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x3, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmaxW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmaxD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FminW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FminD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x1b)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Ffint_sW(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2RF(0x19e, 0x0, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::Ffint_sD(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2RF(0x19e, 0x1, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::Ftint_sW(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2RF(0x19c, 0x0, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::Ftint_sD(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2RF(0x19c, 0x1, ws, wd, 0x1e)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SllB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x0, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SllH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x1, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SllW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x2, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SllD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x0, 0x3, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SraB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x0, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SraH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x1, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SraW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x2, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SraD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x3, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SrlB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x0, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SrlH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x1, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SrlW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x2, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SrlD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x3, wt, ws, wd, 0xd)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::SlliB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  DsFsmInstr(EmitMsaBIT(0x0, shamt3 | kMsaDfMByteMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SlliH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  DsFsmInstr(EmitMsaBIT(0x0, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SlliW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  DsFsmInstr(EmitMsaBIT(0x0, shamt5 | kMsaDfMWordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SlliD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  DsFsmInstr(EmitMsaBIT(0x0, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SraiB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  DsFsmInstr(EmitMsaBIT(0x1, shamt3 | kMsaDfMByteMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SraiH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  DsFsmInstr(EmitMsaBIT(0x1, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SraiW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  DsFsmInstr(EmitMsaBIT(0x1, shamt5 | kMsaDfMWordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SraiD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  DsFsmInstr(EmitMsaBIT(0x1, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SrliB(VectorRegister wd, VectorRegister ws, int shamt3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(shamt3)) << shamt3;
  DsFsmInstr(EmitMsaBIT(0x2, shamt3 | kMsaDfMByteMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SrliH(VectorRegister wd, VectorRegister ws, int shamt4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(shamt4)) << shamt4;
  DsFsmInstr(EmitMsaBIT(0x2, shamt4 | kMsaDfMHalfwordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SrliW(VectorRegister wd, VectorRegister ws, int shamt5) {
  CHECK(HasMsa());
  CHECK(IsUint<5>(shamt5)) << shamt5;
  DsFsmInstr(EmitMsaBIT(0x2, shamt5 | kMsaDfMWordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SrliD(VectorRegister wd, VectorRegister ws, int shamt6) {
  CHECK(HasMsa());
  CHECK(IsUint<6>(shamt6)) << shamt6;
  DsFsmInstr(EmitMsaBIT(0x2, shamt6 | kMsaDfMDoublewordMask, ws, wd, 0x9)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::MoveV(VectorRegister wd, VectorRegister ws) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsaBIT(0x1, 0x3e, ws, wd, 0x19)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SplatiB(VectorRegister wd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  DsFsmInstr(EmitMsaELM(0x1, n4 | kMsaDfNByteMask, ws, wd, 0x19)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SplatiH(VectorRegister wd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  DsFsmInstr(EmitMsaELM(0x1, n3 | kMsaDfNHalfwordMask, ws, wd, 0x19)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SplatiW(VectorRegister wd, VectorRegister ws, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  DsFsmInstr(EmitMsaELM(0x1, n2 | kMsaDfNWordMask, ws, wd, 0x19)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::SplatiD(VectorRegister wd, VectorRegister ws, int n1) {
  CHECK(HasMsa());
  CHECK(IsUint<1>(n1)) << n1;
  DsFsmInstr(EmitMsaELM(0x1, n1 | kMsaDfNDoublewordMask, ws, wd, 0x19)).FprOuts(wd).FprIns(ws);
}

void MipsAssembler::Copy_sB(Register rd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  DsFsmInstr(EmitMsaELM(0x2, n4 | kMsaDfNByteMask, ws, static_cast<VectorRegister>(rd), 0x19))
      .GprOuts(rd).FprIns(ws);
}

void MipsAssembler::Copy_sH(Register rd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  DsFsmInstr(EmitMsaELM(0x2, n3 | kMsaDfNHalfwordMask, ws, static_cast<VectorRegister>(rd), 0x19))
      .GprOuts(rd).FprIns(ws);
}

void MipsAssembler::Copy_sW(Register rd, VectorRegister ws, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  DsFsmInstr(EmitMsaELM(0x2, n2 | kMsaDfNWordMask, ws, static_cast<VectorRegister>(rd), 0x19))
      .GprOuts(rd).FprIns(ws);
}

void MipsAssembler::Copy_uB(Register rd, VectorRegister ws, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  DsFsmInstr(EmitMsaELM(0x3, n4 | kMsaDfNByteMask, ws, static_cast<VectorRegister>(rd), 0x19))
      .GprOuts(rd).FprIns(ws);
}

void MipsAssembler::Copy_uH(Register rd, VectorRegister ws, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  DsFsmInstr(EmitMsaELM(0x3, n3 | kMsaDfNHalfwordMask, ws, static_cast<VectorRegister>(rd), 0x19))
      .GprOuts(rd).FprIns(ws);
}

void MipsAssembler::InsertB(VectorRegister wd, Register rs, int n4) {
  CHECK(HasMsa());
  CHECK(IsUint<4>(n4)) << n4;
  DsFsmInstr(EmitMsaELM(0x4, n4 | kMsaDfNByteMask, static_cast<VectorRegister>(rs), wd, 0x19))
      .FprInOuts(wd).GprIns(rs);
}

void MipsAssembler::InsertH(VectorRegister wd, Register rs, int n3) {
  CHECK(HasMsa());
  CHECK(IsUint<3>(n3)) << n3;
  DsFsmInstr(EmitMsaELM(0x4, n3 | kMsaDfNHalfwordMask, static_cast<VectorRegister>(rs), wd, 0x19))
      .FprInOuts(wd).GprIns(rs);
}

void MipsAssembler::InsertW(VectorRegister wd, Register rs, int n2) {
  CHECK(HasMsa());
  CHECK(IsUint<2>(n2)) << n2;
  DsFsmInstr(EmitMsaELM(0x4, n2 | kMsaDfNWordMask, static_cast<VectorRegister>(rs), wd, 0x19))
      .FprInOuts(wd).GprIns(rs);
}

void MipsAssembler::FillB(VectorRegister wd, Register rs) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2R(0xc0, 0x0, static_cast<VectorRegister>(rs), wd, 0x1e))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::FillH(VectorRegister wd, Register rs) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2R(0xc0, 0x1, static_cast<VectorRegister>(rs), wd, 0x1e))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::FillW(VectorRegister wd, Register rs) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa2R(0xc0, 0x2, static_cast<VectorRegister>(rs), wd, 0x1e))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::LdiB(VectorRegister wd, int imm8) {
  CHECK(HasMsa());
  CHECK(IsInt<8>(imm8)) << imm8;
  DsFsmInstr(EmitMsaI10(0x6, 0x0, imm8 & kMsaS10Mask, wd, 0x7)).FprOuts(wd);
}

void MipsAssembler::LdiH(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  DsFsmInstr(EmitMsaI10(0x6, 0x1, imm10 & kMsaS10Mask, wd, 0x7)).FprOuts(wd);
}

void MipsAssembler::LdiW(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  DsFsmInstr(EmitMsaI10(0x6, 0x2, imm10 & kMsaS10Mask, wd, 0x7)).FprOuts(wd);
}

void MipsAssembler::LdiD(VectorRegister wd, int imm10) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(imm10)) << imm10;
  DsFsmInstr(EmitMsaI10(0x6, 0x3, imm10 & kMsaS10Mask, wd, 0x7)).FprOuts(wd);
}

void MipsAssembler::LdB(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(offset)) << offset;
  DsFsmInstr(EmitMsaMI10(offset & kMsaS10Mask, rs, wd, 0x8, 0x0)).FprOuts(wd).GprIns(rs);
}

void MipsAssembler::LdH(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<11>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsHalfwordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_2) & kMsaS10Mask, rs, wd, 0x8, 0x1))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::LdW(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<12>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsWordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_4) & kMsaS10Mask, rs, wd, 0x8, 0x2))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::LdD(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<13>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsDoublewordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_8) & kMsaS10Mask, rs, wd, 0x8, 0x3))
      .FprOuts(wd).GprIns(rs);
}

void MipsAssembler::StB(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<10>(offset)) << offset;
  DsFsmInstr(EmitMsaMI10(offset & kMsaS10Mask, rs, wd, 0x9, 0x0)).FprIns(wd).GprIns(rs);
}

void MipsAssembler::StH(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<11>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsHalfwordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_2) & kMsaS10Mask, rs, wd, 0x9, 0x1))
      .FprIns(wd).GprIns(rs);
}

void MipsAssembler::StW(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<12>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsWordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_4) & kMsaS10Mask, rs, wd, 0x9, 0x2))
      .FprIns(wd).GprIns(rs);
}

void MipsAssembler::StD(VectorRegister wd, Register rs, int offset) {
  CHECK(HasMsa());
  CHECK(IsInt<13>(offset)) << offset;
  CHECK_ALIGNED(offset, kMipsDoublewordSize);
  DsFsmInstr(EmitMsaMI10((offset >> TIMES_8) & kMsaS10Mask, rs, wd, 0x9, 0x3))
      .FprIns(wd).GprIns(rs);
}

void MipsAssembler::IlvlB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvlH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvlW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvlD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvrB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvrH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvrW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvrD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvevB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x0, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvevH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x1, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvevW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x2, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvevD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x6, 0x3, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvodB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x0, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvodH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x1, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvodW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x2, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::IlvodD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x7, 0x3, wt, ws, wd, 0x14)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MaddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x0, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MaddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x1, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MaddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x2, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MaddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x1, 0x3, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MsubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x0, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MsubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x1, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MsubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x2, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::MsubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x3, wt, ws, wd, 0x12)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x0, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x0, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Asub_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x11)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x0, wt, ws, wd, 0x1b)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x1, wt, ws, wd, 0x1b)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x2, wt, ws, wd, 0x1b)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::FmsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x2, 0x3, wt, ws, wd, 0x1b)).FprInOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x1, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x2, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x4, 0x3, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x1, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x2, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::Hadd_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt) {
  CHECK(HasMsa());
  DsFsmInstr(EmitMsa3R(0x5, 0x3, wt, ws, wd, 0x15)).FprOuts(wd).FprIns(ws, wt);
}

void MipsAssembler::ReplicateFPToVectorRegister(VectorRegister dst,
                                                FRegister src,
                                                bool is_double) {
  // Float or double in FPU register Fx can be considered as 0th element in vector register Wx.
  if (is_double) {
    SplatiD(dst, static_cast<VectorRegister>(src), 0);
  } else {
    SplatiW(dst, static_cast<VectorRegister>(src), 0);
  }
}

void MipsAssembler::LoadConst32(Register rd, int32_t value) {
  if (IsUint<16>(value)) {
    // Use OR with (unsigned) immediate to encode 16b unsigned int.
    Ori(rd, ZERO, value);
  } else if (IsInt<16>(value)) {
    // Use ADD with (signed) immediate to encode 16b signed int.
    Addiu(rd, ZERO, value);
  } else {
    Lui(rd, High16Bits(value));
    if (value & 0xFFFF)
      Ori(rd, rd, Low16Bits(value));
  }
}

void MipsAssembler::LoadConst64(Register reg_hi, Register reg_lo, int64_t value) {
  uint32_t low = Low32Bits(value);
  uint32_t high = High32Bits(value);
  LoadConst32(reg_lo, low);
  if (high != low) {
    LoadConst32(reg_hi, high);
  } else {
    Move(reg_hi, reg_lo);
  }
}

void MipsAssembler::LoadSConst32(FRegister r, int32_t value, Register temp) {
  if (value == 0) {
    temp = ZERO;
  } else {
    LoadConst32(temp, value);
  }
  Mtc1(temp, r);
}

void MipsAssembler::LoadDConst64(FRegister rd, int64_t value, Register temp) {
  uint32_t low = Low32Bits(value);
  uint32_t high = High32Bits(value);
  if (low == 0) {
    Mtc1(ZERO, rd);
  } else {
    LoadConst32(temp, low);
    Mtc1(temp, rd);
  }
  if (high == 0) {
    MoveToFpuHigh(ZERO, rd);
  } else {
    LoadConst32(temp, high);
    MoveToFpuHigh(temp, rd);
  }
}

void MipsAssembler::Addiu32(Register rt, Register rs, int32_t value, Register temp) {
  CHECK_NE(rs, temp);  // Must not overwrite the register `rs` while loading `value`.
  if (IsInt<16>(value)) {
    Addiu(rt, rs, value);
  } else if (IsR6()) {
    int16_t high = High16Bits(value);
    int16_t low = Low16Bits(value);
    high += (low < 0) ? 1 : 0;  // Account for sign extension in addiu.
    if (low != 0) {
      Aui(temp, rs, high);
      Addiu(rt, temp, low);
    } else {
      Aui(rt, rs, high);
    }
  } else {
    // Do not load the whole 32-bit `value` if it can be represented as
    // a sum of two 16-bit signed values. This can save an instruction.
    constexpr int32_t kMinValueForSimpleAdjustment = std::numeric_limits<int16_t>::min() * 2;
    constexpr int32_t kMaxValueForSimpleAdjustment = std::numeric_limits<int16_t>::max() * 2;
    if (0 <= value && value <= kMaxValueForSimpleAdjustment) {
      Addiu(temp, rs, kMaxValueForSimpleAdjustment / 2);
      Addiu(rt, temp, value - kMaxValueForSimpleAdjustment / 2);
    } else if (kMinValueForSimpleAdjustment <= value && value < 0) {
      Addiu(temp, rs, kMinValueForSimpleAdjustment / 2);
      Addiu(rt, temp, value - kMinValueForSimpleAdjustment / 2);
    } else {
      // Now that all shorter options have been exhausted, load the full 32-bit value.
      LoadConst32(temp, value);
      Addu(rt, rs, temp);
    }
  }
}

void MipsAssembler::Branch::InitShortOrLong(MipsAssembler::Branch::OffsetBits offset_size,
                                            MipsAssembler::Branch::Type short_type,
                                            MipsAssembler::Branch::Type long_type) {
  type_ = (offset_size <= branch_info_[short_type].offset_size) ? short_type : long_type;
}

void MipsAssembler::Branch::InitializeType(Type initial_type, bool is_r6) {
  OffsetBits offset_size_needed = GetOffsetSizeNeeded(location_, target_);
  if (is_r6) {
    // R6
    switch (initial_type) {
      case kLabel:
        CHECK(!IsResolved());
        type_ = kR6Label;
        break;
      case kLiteral:
        CHECK(!IsResolved());
        type_ = kR6Literal;
        break;
      case kCall:
        InitShortOrLong(offset_size_needed, kR6Call, kR6LongCall);
        break;
      case kCondBranch:
        switch (condition_) {
          case kUncond:
            InitShortOrLong(offset_size_needed, kR6UncondBranch, kR6LongUncondBranch);
            break;
          case kCondEQZ:
          case kCondNEZ:
            // Special case for beqzc/bnezc with longer offset than in other b<cond>c instructions.
            type_ = (offset_size_needed <= kOffset23) ? kR6CondBranch : kR6LongCondBranch;
            break;
          default:
            InitShortOrLong(offset_size_needed, kR6CondBranch, kR6LongCondBranch);
            break;
        }
        break;
      case kBareCall:
        type_ = kR6BareCall;
        CHECK_LE(offset_size_needed, GetOffsetSize());
        break;
      case kBareCondBranch:
        type_ = (condition_ == kUncond) ? kR6BareUncondBranch : kR6BareCondBranch;
        CHECK_LE(offset_size_needed, GetOffsetSize());
        break;
      default:
        LOG(FATAL) << "Unexpected branch type " << initial_type;
        UNREACHABLE();
    }
  } else {
    // R2
    switch (initial_type) {
      case kLabel:
        CHECK(!IsResolved());
        type_ = kLabel;
        break;
      case kLiteral:
        CHECK(!IsResolved());
        type_ = kLiteral;
        break;
      case kCall:
        InitShortOrLong(offset_size_needed, kCall, kLongCall);
        break;
      case kCondBranch:
        switch (condition_) {
          case kUncond:
            InitShortOrLong(offset_size_needed, kUncondBranch, kLongUncondBranch);
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
  }
  old_type_ = type_;
}

bool MipsAssembler::Branch::IsNop(BranchCondition condition, Register lhs, Register rhs) {
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

bool MipsAssembler::Branch::IsUncond(BranchCondition condition, Register lhs, Register rhs) {
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

MipsAssembler::Branch::Branch(bool is_r6,
                              uint32_t location,
                              uint32_t target,
                              bool is_call,
                              bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(0),
      rhs_reg_(0),
      condition_(kUncond),
      delayed_instruction_(kUnfilledDelaySlot),
      patcher_label_(nullptr) {
  InitializeType(
      (is_call ? (is_bare ? kBareCall : kCall) : (is_bare ? kBareCondBranch : kCondBranch)),
      is_r6);
}

MipsAssembler::Branch::Branch(bool is_r6,
                              uint32_t location,
                              uint32_t target,
                              MipsAssembler::BranchCondition condition,
                              Register lhs_reg,
                              Register rhs_reg,
                              bool is_bare)
    : old_location_(location),
      location_(location),
      target_(target),
      lhs_reg_(lhs_reg),
      rhs_reg_(rhs_reg),
      condition_(condition),
      delayed_instruction_(kUnfilledDelaySlot),
      patcher_label_(nullptr) {
  CHECK_NE(condition, kUncond);
  switch (condition) {
    case kCondLT:
    case kCondGE:
    case kCondLE:
    case kCondGT:
    case kCondLTU:
    case kCondGEU:
      // We don't support synthetic R2 branches (preceded with slt[u]) at this level
      // (R2 doesn't have branches to compare 2 registers using <, <=, >=, >).
      // We leave this up to the caller.
      CHECK(is_r6);
      FALLTHROUGH_INTENDED;
    case kCondEQ:
    case kCondNE:
      // Require registers other than 0 not only for R6, but also for R2 to catch errors.
      // To compare with 0, use dedicated kCond*Z conditions.
      CHECK_NE(lhs_reg, ZERO);
      CHECK_NE(rhs_reg, ZERO);
      break;
    case kCondLTZ:
    case kCondGEZ:
    case kCondLEZ:
    case kCondGTZ:
    case kCondEQZ:
    case kCondNEZ:
      // Require registers other than 0 not only for R6, but also for R2 to catch errors.
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

MipsAssembler::Branch::Branch(bool is_r6,
                              uint32_t location,
                              Register dest_reg,
                              Register base_reg,
                              Type label_or_literal_type)
    : old_location_(location),
      location_(location),
      target_(kUnresolved),
      lhs_reg_(dest_reg),
      rhs_reg_(base_reg),
      condition_(kUncond),
      delayed_instruction_(kUnfilledDelaySlot),
      patcher_label_(nullptr) {
  CHECK_NE(dest_reg, ZERO);
  if (is_r6) {
    CHECK_EQ(base_reg, ZERO);
  }
  InitializeType(label_or_literal_type, is_r6);
}

MipsAssembler::BranchCondition MipsAssembler::Branch::OppositeCondition(
    MipsAssembler::BranchCondition cond) {
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

MipsAssembler::Branch::Type MipsAssembler::Branch::GetType() const {
  return type_;
}

MipsAssembler::BranchCondition MipsAssembler::Branch::GetCondition() const {
  return condition_;
}

Register MipsAssembler::Branch::GetLeftRegister() const {
  return static_cast<Register>(lhs_reg_);
}

Register MipsAssembler::Branch::GetRightRegister() const {
  return static_cast<Register>(rhs_reg_);
}

uint32_t MipsAssembler::Branch::GetTarget() const {
  return target_;
}

uint32_t MipsAssembler::Branch::GetLocation() const {
  return location_;
}

uint32_t MipsAssembler::Branch::GetOldLocation() const {
  return old_location_;
}

uint32_t MipsAssembler::Branch::GetPrecedingInstructionLength(Type type) const {
  // Short branches with delay slots always consist of two instructions, the branch
  // and the delay slot, irrespective of whether the delay slot is filled with a
  // useful instruction or not.
  // Long composite branches may have a length longer by one instruction than
  // specified in branch_info_[].length. This happens when an instruction is taken
  // to fill the short branch delay slot, but the branch eventually becomes long
  // and formally has no delay slot to fill. This instruction is placed at the
  // beginning of the long composite branch and this needs to be accounted for in
  // the branch length and the location of the offset encoded in the branch.
  switch (type) {
    case kLongUncondBranch:
    case kLongCondBranch:
    case kLongCall:
    case kR6LongCondBranch:
      return (delayed_instruction_ != kUnfilledDelaySlot &&
          delayed_instruction_ != kUnfillableDelaySlot) ? 1 : 0;
    default:
      return 0;
  }
}

uint32_t MipsAssembler::Branch::GetPrecedingInstructionSize(Type type) const {
  return GetPrecedingInstructionLength(type) * sizeof(uint32_t);
}

uint32_t MipsAssembler::Branch::GetLength() const {
  return GetPrecedingInstructionLength(type_) + branch_info_[type_].length;
}

uint32_t MipsAssembler::Branch::GetOldLength() const {
  return GetPrecedingInstructionLength(old_type_) + branch_info_[old_type_].length;
}

uint32_t MipsAssembler::Branch::GetSize() const {
  return GetLength() * sizeof(uint32_t);
}

uint32_t MipsAssembler::Branch::GetOldSize() const {
  return GetOldLength() * sizeof(uint32_t);
}

uint32_t MipsAssembler::Branch::GetEndLocation() const {
  return GetLocation() + GetSize();
}

uint32_t MipsAssembler::Branch::GetOldEndLocation() const {
  return GetOldLocation() + GetOldSize();
}

bool MipsAssembler::Branch::IsBare() const {
  switch (type_) {
    // R2 short branches (can't be promoted to long), delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kR6BareUncondBranch:
    case kR6BareCondBranch:
    case kR6BareCall:
      return true;
    default:
      return false;
  }
}

bool MipsAssembler::Branch::IsLong() const {
  switch (type_) {
    // R2 short branches (can be promoted to long).
    case kUncondBranch:
    case kCondBranch:
    case kCall:
    // R2 short branches (can't be promoted to long), delay slots filled manually.
    case kBareUncondBranch:
    case kBareCondBranch:
    case kBareCall:
    // R2 near label.
    case kLabel:
    // R2 near literal.
    case kLiteral:
    // R6 short branches (can be promoted to long).
    case kR6UncondBranch:
    case kR6CondBranch:
    case kR6Call:
    // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
    case kR6BareUncondBranch:
    case kR6BareCondBranch:
    case kR6BareCall:
    // R6 near label.
    case kR6Label:
    // R6 near literal.
    case kR6Literal:
      return false;
    // R2 long branches.
    case kLongUncondBranch:
    case kLongCondBranch:
    case kLongCall:
    // R2 far label.
    case kFarLabel:
    // R2 far literal.
    case kFarLiteral:
    // R6 long branches.
    case kR6LongUncondBranch:
    case kR6LongCondBranch:
    case kR6LongCall:
    // R6 far label.
    case kR6FarLabel:
    // R6 far literal.
    case kR6FarLiteral:
      return true;
  }
  UNREACHABLE();
}

bool MipsAssembler::Branch::IsResolved() const {
  return target_ != kUnresolved;
}

MipsAssembler::Branch::OffsetBits MipsAssembler::Branch::GetOffsetSize() const {
  bool r6_cond_branch = (type_ == kR6CondBranch || type_ == kR6BareCondBranch);
  OffsetBits offset_size =
      (r6_cond_branch && (condition_ == kCondEQZ || condition_ == kCondNEZ))
          ? kOffset23
          : branch_info_[type_].offset_size;
  return offset_size;
}

MipsAssembler::Branch::OffsetBits MipsAssembler::Branch::GetOffsetSizeNeeded(uint32_t location,
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

void MipsAssembler::Branch::Resolve(uint32_t target) {
  target_ = target;
}

void MipsAssembler::Branch::Relocate(uint32_t expand_location, uint32_t delta) {
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

void MipsAssembler::Branch::PromoteToLong() {
  CHECK(!IsBare());  // Bare branches do not promote.
  switch (type_) {
    // R2 short branches (can be promoted to long).
    case kUncondBranch:
      type_ = kLongUncondBranch;
      break;
    case kCondBranch:
      type_ = kLongCondBranch;
      break;
    case kCall:
      type_ = kLongCall;
      break;
    // R2 near label.
    case kLabel:
      type_ = kFarLabel;
      break;
    // R2 near literal.
    case kLiteral:
      type_ = kFarLiteral;
      break;
    // R6 short branches (can be promoted to long).
    case kR6UncondBranch:
      type_ = kR6LongUncondBranch;
      break;
    case kR6CondBranch:
      type_ = kR6LongCondBranch;
      break;
    case kR6Call:
      type_ = kR6LongCall;
      break;
    // R6 near label.
    case kR6Label:
      type_ = kR6FarLabel;
      break;
    // R6 near literal.
    case kR6Literal:
      type_ = kR6FarLiteral;
      break;
    default:
      // Note: 'type_' is already long.
      break;
  }
  CHECK(IsLong());
}

uint32_t MipsAssembler::GetBranchLocationOrPcRelBase(const MipsAssembler::Branch* branch) const {
  switch (branch->GetType()) {
    case Branch::kLabel:
    case Branch::kFarLabel:
    case Branch::kLiteral:
    case Branch::kFarLiteral:
      if (branch->GetRightRegister() != ZERO) {
        return GetLabelLocation(&pc_rel_base_label_);
      }
      // For those label/literal loads which come with their own NAL instruction
      // and don't depend on `pc_rel_base_label_` we can simply use the location
      // of the "branch" (the NAL precedes the "branch" immediately). The location
      // is close enough for the user of the returned location, PromoteIfNeeded(),
      // to not miss needed promotion to a far load.
      // (GetOffsetSizeNeeded() provides a little leeway by means of kMaxBranchSize,
      // which is larger than all composite branches and label/literal loads: it's
      // OK to promote a bit earlier than strictly necessary, it makes things
      // simpler.)
      FALLTHROUGH_INTENDED;
    default:
      return branch->GetLocation();
  }
}

uint32_t MipsAssembler::Branch::PromoteIfNeeded(uint32_t location, uint32_t max_short_distance) {
  // `location` comes from GetBranchLocationOrPcRelBase() and is either the location
  // of the PC-relative branch or (for some R2 label and literal loads) the location
  // of `pc_rel_base_label_`. The PC-relative offset of the branch/load is relative
  // to this location.
  // If the branch is still unresolved or already long, nothing to do.
  if (IsLong() || !IsResolved()) {
    return 0;
  }
  // Promote the short branch to long if the offset size is too small
  // to hold the distance between location and target_.
  if (GetOffsetSizeNeeded(location, target_) > GetOffsetSize()) {
    PromoteToLong();
    uint32_t old_size = GetOldSize();
    uint32_t new_size = GetSize();
    CHECK_GT(new_size, old_size);
    return new_size - old_size;
  }
  // The following logic is for debugging/testing purposes.
  // Promote some short branches to long when it's not really required.
  if (UNLIKELY(max_short_distance != std::numeric_limits<uint32_t>::max() && !IsBare())) {
    int64_t distance = static_cast<int64_t>(target_) - location;
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

uint32_t MipsAssembler::Branch::GetOffsetLocation() const {
  return location_ + GetPrecedingInstructionSize(type_) +
      branch_info_[type_].instr_offset * sizeof(uint32_t);
}

uint32_t MipsAssembler::GetBranchOrPcRelBaseForEncoding(const MipsAssembler::Branch* branch) const {
  switch (branch->GetType()) {
    case Branch::kLabel:
    case Branch::kFarLabel:
    case Branch::kLiteral:
    case Branch::kFarLiteral:
      if (branch->GetRightRegister() == ZERO) {
        // These loads don't use `pc_rel_base_label_` and instead rely on their own
        // NAL instruction (it immediately precedes the "branch"). Therefore the
        // effective PC-relative base register is RA and it corresponds to the 2nd
        // instruction after the NAL.
        return branch->GetLocation() + sizeof(uint32_t);
      } else {
        return GetLabelLocation(&pc_rel_base_label_);
      }
    default:
      return branch->GetOffsetLocation() +
          Branch::branch_info_[branch->GetType()].pc_org * sizeof(uint32_t);
  }
}

uint32_t MipsAssembler::Branch::GetOffset(uint32_t location) const {
  // `location` comes from GetBranchOrPcRelBaseForEncoding() and is either a location
  // within/near the PC-relative branch or (for some R2 label and literal loads) the
  // location of `pc_rel_base_label_`. The PC-relative offset of the branch/load is
  // relative to this location.
  CHECK(IsResolved());
  uint32_t ofs_mask = 0xFFFFFFFF >> (32 - GetOffsetSize());
  // Calculate the byte distance between instructions and also account for
  // different PC-relative origins.
  uint32_t offset = target_ - location;
  // Prepare the offset for encoding into the instruction(s).
  offset = (offset & ofs_mask) >> branch_info_[type_].offset_shift;
  return offset;
}

MipsAssembler::Branch* MipsAssembler::GetBranch(uint32_t branch_id) {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

const MipsAssembler::Branch* MipsAssembler::GetBranch(uint32_t branch_id) const {
  CHECK_LT(branch_id, branches_.size());
  return &branches_[branch_id];
}

void MipsAssembler::BindRelativeToPrecedingBranch(MipsLabel* label,
                                                  uint32_t prev_branch_id_plus_one,
                                                  uint32_t position) {
  if (prev_branch_id_plus_one != 0) {
    const Branch* branch = GetBranch(prev_branch_id_plus_one - 1);
    position -= branch->GetEndLocation();
  }
  label->prev_branch_id_plus_one_ = prev_branch_id_plus_one;
  label->BindTo(position);
}

void MipsAssembler::Bind(MipsLabel* label) {
  CHECK(!label->IsBound());
  uint32_t bound_pc = buffer_.Size();

  // Make the delay slot FSM aware of the new label.
  DsFsmLabel();

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
  BindRelativeToPrecedingBranch(label, branches_.size(), bound_pc);
}

uint32_t MipsAssembler::GetLabelLocation(const MipsLabel* label) const {
  CHECK(label->IsBound());
  uint32_t target = label->Position();
  if (label->prev_branch_id_plus_one_ != 0) {
    // Get label location based on the branch preceding it.
    const Branch* branch = GetBranch(label->prev_branch_id_plus_one_ - 1);
    target += branch->GetEndLocation();
  }
  return target;
}

uint32_t MipsAssembler::GetAdjustedPosition(uint32_t old_position) {
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

void MipsAssembler::BindPcRelBaseLabel() {
  Bind(&pc_rel_base_label_);
}

uint32_t MipsAssembler::GetPcRelBaseLabelLocation() const {
  return GetLabelLocation(&pc_rel_base_label_);
}

void MipsAssembler::FinalizeLabeledBranch(MipsLabel* label) {
  uint32_t length = branches_.back().GetLength();
  // Commit the last branch target label (if any).
  DsFsmCommitLabel();
  if (!label->IsBound()) {
    // Branch forward (to a following label), distance is unknown.
    // The first branch forward will contain 0, serving as the terminator of
    // the list of forward-reaching branches.
    Emit(label->position_);
    // Nothing for the delay slot (yet).
    DsFsmInstrNop(0);
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

bool MipsAssembler::Branch::CanHaveDelayedInstruction(const DelaySlot& delay_slot) const {
  if (delay_slot.instruction_ == 0) {
    // NOP or no instruction for the delay slot.
    return false;
  }
  switch (type_) {
    // R2 unconditional branches.
    case kUncondBranch:
    case kLongUncondBranch:
      // There are no register interdependencies.
      return true;

    // R2 calls.
    case kCall:
    case kLongCall:
      // Instructions depending on or modifying RA should not be moved into delay slots
      // of branches modifying RA.
      return ((delay_slot.masks_.gpr_ins_ | delay_slot.masks_.gpr_outs_) & (1u << RA)) == 0;

    // R2 conditional branches.
    case kCondBranch:
    case kLongCondBranch:
      switch (condition_) {
        // Branches with one GPR source.
        case kCondLTZ:
        case kCondGEZ:
        case kCondLEZ:
        case kCondGTZ:
        case kCondEQZ:
        case kCondNEZ:
          return (delay_slot.masks_.gpr_outs_ & (1u << lhs_reg_)) == 0;

        // Branches with two GPR sources.
        case kCondEQ:
        case kCondNE:
          return (delay_slot.masks_.gpr_outs_ & ((1u << lhs_reg_) | (1u << rhs_reg_))) == 0;

        // Branches with one FPU condition code source.
        case kCondF:
        case kCondT:
          return (delay_slot.masks_.cc_outs_ & (1u << lhs_reg_)) == 0;

        default:
          // We don't support synthetic R2 branches (preceded with slt[u]) at this level
          // (R2 doesn't have branches to compare 2 registers using <, <=, >=, >).
          LOG(FATAL) << "Unexpected branch condition " << condition_;
          UNREACHABLE();
      }

    // R6 unconditional branches.
    case kR6UncondBranch:
    case kR6LongUncondBranch:
    // R6 calls.
    case kR6Call:
    case kR6LongCall:
      // There are no delay slots.
      return false;

    // R6 conditional branches.
    case kR6CondBranch:
    case kR6LongCondBranch:
      switch (condition_) {
        // Branches with one FPU register source.
        case kCondF:
        case kCondT:
          return (delay_slot.masks_.fpr_outs_ & (1u << lhs_reg_)) == 0;
        // Others have a forbidden slot instead of a delay slot.
        default:
          return false;
      }

    // Literals.
    default:
      LOG(FATAL) << "Unexpected branch type " << type_;
      UNREACHABLE();
  }
}

uint32_t MipsAssembler::Branch::GetDelayedInstruction() const {
  return delayed_instruction_;
}

MipsLabel* MipsAssembler::Branch::GetPatcherLabel() const {
  return patcher_label_;
}

void MipsAssembler::Branch::SetDelayedInstruction(uint32_t instruction, MipsLabel* patcher_label) {
  CHECK_NE(instruction, kUnfilledDelaySlot);
  CHECK_EQ(delayed_instruction_, kUnfilledDelaySlot);
  delayed_instruction_ = instruction;
  patcher_label_ = patcher_label;
}

void MipsAssembler::Branch::DecrementLocations() {
  // We first create a branch object, which gets its type and locations initialized,
  // and then we check if the branch can actually have the preceding instruction moved
  // into its delay slot. If it can, the branch locations need to be decremented.
  //
  // We could make the check before creating the branch object and avoid the location
  // adjustment, but the check is cleaner when performed on an initialized branch
  // object.
  //
  // If the branch is backwards (to a previously bound label), reducing the locations
  // cannot cause a short branch to exceed its offset range because the offset reduces.
  // And this is not at all a problem for a long branch backwards.
  //
  // If the branch is forward (not linked to any label yet), reducing the locations
  // is harmless. The branch will be promoted to long if needed when the target is known.
  CHECK_EQ(location_, old_location_);
  CHECK_GE(old_location_, sizeof(uint32_t));
  old_location_ -= sizeof(uint32_t);
  location_ = old_location_;
}

void MipsAssembler::MoveInstructionToDelaySlot(Branch& branch) {
  if (branch.IsBare()) {
    // Delay slots are filled manually in bare branches.
    return;
  }
  if (branch.CanHaveDelayedInstruction(delay_slot_)) {
    // The last instruction cannot be used in a different delay slot,
    // do not commit the label before it (if any).
    DsFsmDropLabel();
    // Remove the last emitted instruction.
    size_t size = buffer_.Size();
    CHECK_GE(size, sizeof(uint32_t));
    size -= sizeof(uint32_t);
    CHECK_EQ(buffer_.Load<uint32_t>(size), delay_slot_.instruction_);
    buffer_.Resize(size);
    // Attach it to the branch and adjust the branch locations.
    branch.DecrementLocations();
    branch.SetDelayedInstruction(delay_slot_.instruction_, delay_slot_.patcher_label_);
  } else if (!reordering_ && branch.GetType() == Branch::kUncondBranch) {
    // If reordefing is disabled, prevent absorption of the target instruction.
    branch.SetDelayedInstruction(Branch::kUnfillableDelaySlot);
  }
}

void MipsAssembler::Buncond(MipsLabel* label, bool is_r6, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(is_r6, buffer_.Size(), target, /* is_call */ false, is_bare);
  MoveInstructionToDelaySlot(branches_.back());
  FinalizeLabeledBranch(label);
}

void MipsAssembler::Bcond(MipsLabel* label,
                          bool is_r6,
                          bool is_bare,
                          BranchCondition condition,
                          Register lhs,
                          Register rhs) {
  // If lhs = rhs, this can be a NOP.
  if (Branch::IsNop(condition, lhs, rhs)) {
    return;
  }
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(is_r6, buffer_.Size(), target, condition, lhs, rhs, is_bare);
  MoveInstructionToDelaySlot(branches_.back());
  FinalizeLabeledBranch(label);
}

void MipsAssembler::Call(MipsLabel* label, bool is_r6, bool is_bare) {
  uint32_t target = label->IsBound() ? GetLabelLocation(label) : Branch::kUnresolved;
  branches_.emplace_back(is_r6, buffer_.Size(), target, /* is_call */ true, is_bare);
  MoveInstructionToDelaySlot(branches_.back());
  FinalizeLabeledBranch(label);
}

void MipsAssembler::LoadLabelAddress(Register dest_reg, Register base_reg, MipsLabel* label) {
  // Label address loads are treated as pseudo branches since they require very similar handling.
  DCHECK(!label->IsBound());
  // If `pc_rel_base_label_` isn't bound or none of registers contains its address, we
  // may generate an individual NAL instruction to simulate PC-relative addressing on R2
  // by specifying `base_reg` of `ZERO`. Check for it.
  if (base_reg == ZERO && !IsR6()) {
    Nal();
  }
  branches_.emplace_back(IsR6(), buffer_.Size(), dest_reg, base_reg, Branch::kLabel);
  FinalizeLabeledBranch(label);
}

Literal* MipsAssembler::NewLiteral(size_t size, const uint8_t* data) {
  DCHECK(size == 4u || size == 8u) << size;
  literals_.emplace_back(size, data);
  return &literals_.back();
}

void MipsAssembler::LoadLiteral(Register dest_reg, Register base_reg, Literal* literal) {
  // Literal loads are treated as pseudo branches since they require very similar handling.
  DCHECK_EQ(literal->GetSize(), 4u);
  MipsLabel* label = literal->GetLabel();
  DCHECK(!label->IsBound());
  // If `pc_rel_base_label_` isn't bound or none of registers contains its address, we
  // may generate an individual NAL instruction to simulate PC-relative addressing on R2
  // by specifying `base_reg` of `ZERO`. Check for it.
  if (base_reg == ZERO && !IsR6()) {
    Nal();
  }
  branches_.emplace_back(IsR6(), buffer_.Size(), dest_reg, base_reg, Branch::kLiteral);
  FinalizeLabeledBranch(label);
}

JumpTable* MipsAssembler::CreateJumpTable(std::vector<MipsLabel*>&& labels) {
  jump_tables_.emplace_back(std::move(labels));
  JumpTable* table = &jump_tables_.back();
  DCHECK(!table->GetLabel()->IsBound());
  return table;
}

void MipsAssembler::EmitLiterals() {
  if (!literals_.empty()) {
    // We don't support byte and half-word literals.
    // TODO: proper alignment for 64-bit literals when they're implemented.
    for (Literal& literal : literals_) {
      MipsLabel* label = literal.GetLabel();
      Bind(label);
      AssemblerBuffer::EnsureCapacity ensured(&buffer_);
      DCHECK(literal.GetSize() == 4u || literal.GetSize() == 8u);
      for (size_t i = 0, size = literal.GetSize(); i != size; ++i) {
        buffer_.Emit<uint8_t>(literal.GetData()[i]);
      }
    }
  }
}

void MipsAssembler::ReserveJumpTableSpace() {
  if (!jump_tables_.empty()) {
    for (JumpTable& table : jump_tables_) {
      MipsLabel* label = table.GetLabel();
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

void MipsAssembler::EmitJumpTables() {
  if (!jump_tables_.empty()) {
    CHECK(!overwriting_);
    // Switch from appending instructions at the end of the buffer to overwriting
    // existing instructions (here, jump tables) in the buffer.
    overwriting_ = true;

    for (JumpTable& table : jump_tables_) {
      MipsLabel* table_label = table.GetLabel();
      uint32_t start = GetLabelLocation(table_label);
      overwrite_location_ = start;

      for (MipsLabel* target : table.GetData()) {
        CHECK_EQ(buffer_.Load<uint32_t>(overwrite_location_), 0x1abe1234u);
        // The table will contain target addresses relative to the table start.
        uint32_t offset = GetLabelLocation(target) - start;
        Emit(offset);
      }
    }

    overwriting_ = false;
  }
}

void MipsAssembler::PromoteBranches() {
  // Promote short branches to long as necessary.
  bool changed;
  do {
    changed = false;
    for (auto& branch : branches_) {
      CHECK(branch.IsResolved());
      uint32_t base = GetBranchLocationOrPcRelBase(&branch);
      uint32_t delta = branch.PromoteIfNeeded(base);
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
      CHECK_GE(end, branch.GetOldEndLocation());
      uint32_t size = end - branch.GetOldEndLocation();
      buffer_.Move(branch.GetEndLocation(), branch.GetOldEndLocation(), size);
      end = branch.GetOldLocation();
    }
  }
}

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
const MipsAssembler::Branch::BranchInfo MipsAssembler::Branch::branch_info_[] = {
  // R2 short branches (can be promoted to long).
  {  2, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kUncondBranch
  {  2, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kCondBranch
  {  2, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kCall
  // R2 short branches (can't be promoted to long), delay slots filled manually.
  {  1, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kBareUncondBranch
  {  1, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kBareCondBranch
  {  1, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kBareCall
  // R2 near label.
  {  1, 0, 0, MipsAssembler::Branch::kOffset16, 0 },  // kLabel
  // R2 near literal.
  {  1, 0, 0, MipsAssembler::Branch::kOffset16, 0 },  // kLiteral
  // R2 long branches.
  {  9, 3, 1, MipsAssembler::Branch::kOffset32, 0 },  // kLongUncondBranch
  { 10, 4, 1, MipsAssembler::Branch::kOffset32, 0 },  // kLongCondBranch
  {  6, 1, 1, MipsAssembler::Branch::kOffset32, 0 },  // kLongCall
  // R2 far label.
  {  3, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kFarLabel
  // R2 far literal.
  {  3, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kFarLiteral
  // R6 short branches (can be promoted to long).
  {  1, 0, 1, MipsAssembler::Branch::kOffset28, 2 },  // kR6UncondBranch
  {  2, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kR6CondBranch
                                                      // Exception: kOffset23 for beqzc/bnezc.
  {  1, 0, 1, MipsAssembler::Branch::kOffset28, 2 },  // kR6Call
  // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
  {  1, 0, 1, MipsAssembler::Branch::kOffset28, 2 },  // kR6BareUncondBranch
  {  1, 0, 1, MipsAssembler::Branch::kOffset18, 2 },  // kR6BareCondBranch
                                                      // Exception: kOffset23 for beqzc/bnezc.
  {  1, 0, 1, MipsAssembler::Branch::kOffset28, 2 },  // kR6BareCall
  // R6 near label.
  {  1, 0, 0, MipsAssembler::Branch::kOffset21, 2 },  // kR6Label
  // R6 near literal.
  {  1, 0, 0, MipsAssembler::Branch::kOffset21, 2 },  // kR6Literal
  // R6 long branches.
  {  2, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kR6LongUncondBranch
  {  3, 1, 0, MipsAssembler::Branch::kOffset32, 0 },  // kR6LongCondBranch
  {  2, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kR6LongCall
  // R6 far label.
  {  2, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kR6FarLabel
  // R6 far literal.
  {  2, 0, 0, MipsAssembler::Branch::kOffset32, 0 },  // kR6FarLiteral
};

static inline bool IsAbsorbableInstruction(uint32_t instruction) {
  // The relative patcher patches addiu, lw and sw with an immediate operand of 0x5678.
  // We want to make sure that these instructions do not get absorbed into delay slots
  // of unconditional branches on R2. Absorption would otherwise make copies of
  // unpatched instructions.
  if ((instruction & 0xFFFF) != 0x5678) {
    return true;
  }
  switch (instruction >> kOpcodeShift) {
    case 0x09:  // Addiu.
    case 0x23:  // Lw.
    case 0x2B:  // Sw.
      return false;
    default:
      return true;
  }
}

static inline Register GetR2PcRelBaseRegister(Register reg) {
  // LoadLabelAddress() and LoadLiteral() generate individual NAL
  // instructions on R2 when the specified base register is ZERO
  // and so the effective PC-relative base register is RA, not ZERO.
  return (reg == ZERO) ? RA : reg;
}

// Note: make sure branch_info_[] and EmitBranch() are kept synchronized.
void MipsAssembler::EmitBranch(uint32_t branch_id) {
  CHECK_EQ(overwriting_, true);
  Branch* branch = GetBranch(branch_id);
  overwrite_location_ = branch->GetLocation();
  uint32_t offset = branch->GetOffset(GetBranchOrPcRelBaseForEncoding(branch));
  BranchCondition condition = branch->GetCondition();
  Register lhs = branch->GetLeftRegister();
  Register rhs = branch->GetRightRegister();
  uint32_t delayed_instruction = branch->GetDelayedInstruction();
  MipsLabel* patcher_label = branch->GetPatcherLabel();
  if (patcher_label != nullptr) {
    // Update the patcher label location to account for branch promotion and
    // delay slot filling.
    CHECK(patcher_label->IsBound());
    uint32_t bound_pc = branch->GetLocation();
    if (!branch->IsLong()) {
      // Short branches precede delay slots.
      // Long branches follow "delay slots".
      bound_pc += sizeof(uint32_t);
    }
    // Rebind the label.
    patcher_label->Reinitialize();
    BindRelativeToPrecedingBranch(patcher_label, branch_id, bound_pc);
  }
  switch (branch->GetType()) {
    // R2 short branches.
    case Branch::kUncondBranch:
      if (delayed_instruction == Branch::kUnfillableDelaySlot) {
        // The branch was created when reordering was disabled, do not absorb the target
        // instruction.
        delayed_instruction = 0;  // NOP.
      } else if (delayed_instruction == Branch::kUnfilledDelaySlot) {
        // Try to absorb the target instruction into the delay slot.
        delayed_instruction = 0;  // NOP.
        // Incrementing the signed 16-bit offset past the target instruction must not
        // cause overflow into the negative subrange, check for the max offset.
        if (offset != 0x7FFF) {
          uint32_t target = branch->GetTarget();
          if (std::binary_search(ds_fsm_target_pcs_.begin(), ds_fsm_target_pcs_.end(), target)) {
            uint32_t target_instruction = buffer_.Load<uint32_t>(target);
            if (IsAbsorbableInstruction(target_instruction)) {
              delayed_instruction = target_instruction;
              offset++;
            }
          }
        }
      }
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      B(offset);
      Emit(delayed_instruction);
      break;
    case Branch::kCondBranch:
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction == Branch::kUnfilledDelaySlot) {
        delayed_instruction = 0;  // NOP.
      }
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR2(condition, lhs, rhs, offset);
      Emit(delayed_instruction);
      break;
    case Branch::kCall:
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction == Branch::kUnfilledDelaySlot) {
        delayed_instruction = 0;  // NOP.
      }
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bal(offset);
      Emit(delayed_instruction);
      break;
    case Branch::kBareUncondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      B(offset);
      break;
    case Branch::kBareCondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR2(condition, lhs, rhs, offset);
      break;
    case Branch::kBareCall:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bal(offset);
      break;

    // R2 near label.
    case Branch::kLabel:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Addiu(lhs, GetR2PcRelBaseRegister(rhs), offset);
      break;
    // R2 near literal.
    case Branch::kLiteral:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lw(lhs, GetR2PcRelBaseRegister(rhs), offset);
      break;

    // R2 long branches.
    case Branch::kLongUncondBranch:
      // To get the value of the PC register we need to use the NAL instruction.
      // NAL clobbers the RA register. However, RA must be preserved if the
      // method is compiled without the entry/exit sequences that would take care
      // of preserving RA (typically, leaf methods don't preserve RA explicitly).
      // So, we need to preserve RA in some temporary storage ourselves. The AT
      // register can't be used for this because we need it to load a constant
      // which will be added to the value that NAL stores in RA. And we can't
      // use T9 for this in the context of the JNI compiler, which uses it
      // as a scratch register (see InterproceduralScratchRegister()).
      // If we were to add a 32-bit constant to RA using two ADDIU instructions,
      // we'd also need to use the ROTR instruction, which requires no less than
      // MIPSR2.
      // Perhaps, we could use T8 or one of R2's multiplier/divider registers
      // (LO or HI) or even a floating-point register, but that doesn't seem
      // like a nice solution. We may want this to work on both R6 and pre-R6.
      // For now simply use the stack for RA. This should be OK since for the
      // vast majority of code a short PC-relative branch is sufficient.
      // TODO: can this be improved?
      // TODO: consider generation of a shorter sequence when we know that RA
      // is explicitly preserved by the method entry/exit code.
      if (delayed_instruction != Branch::kUnfilledDelaySlot &&
          delayed_instruction != Branch::kUnfillableDelaySlot) {
        Emit(delayed_instruction);
      }
      Push(RA);
      Nal();
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lui(AT, High16Bits(offset));
      Ori(AT, AT, Low16Bits(offset));
      Addu(AT, AT, RA);
      Lw(RA, SP, 0);
      Jr(AT);
      DecreaseFrameSize(kStackAlignment);
      break;
    case Branch::kLongCondBranch:
      // The comment on case 'Branch::kLongUncondBranch' applies here as well.
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction != Branch::kUnfilledDelaySlot) {
        Emit(delayed_instruction);
      }
      // Note: the opposite condition branch encodes 8 as the distance, which is equal to the
      // number of instructions skipped:
      // (PUSH(IncreaseFrameSize(ADDIU) + SW) + NAL + LUI + ORI + ADDU + LW + JR).
      EmitBcondR2(Branch::OppositeCondition(condition), lhs, rhs, 8);
      Push(RA);
      Nal();
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lui(AT, High16Bits(offset));
      Ori(AT, AT, Low16Bits(offset));
      Addu(AT, AT, RA);
      Lw(RA, SP, 0);
      Jr(AT);
      DecreaseFrameSize(kStackAlignment);
      break;
    case Branch::kLongCall:
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction != Branch::kUnfilledDelaySlot) {
        Emit(delayed_instruction);
      }
      Nal();
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lui(AT, High16Bits(offset));
      Ori(AT, AT, Low16Bits(offset));
      Addu(AT, AT, RA);
      Jalr(AT);
      Nop();
      break;

    // R2 far label.
    case Branch::kFarLabel:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lui(AT, High16Bits(offset));
      Ori(AT, AT, Low16Bits(offset));
      Addu(lhs, AT, GetR2PcRelBaseRegister(rhs));
      break;
    // R2 far literal.
    case Branch::kFarLiteral:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in lw.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lui(AT, High16Bits(offset));
      Addu(AT, AT, GetR2PcRelBaseRegister(rhs));
      Lw(lhs, AT, Low16Bits(offset));
      break;

    // R6 short branches.
    case Branch::kR6UncondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bc(offset);
      break;
    case Branch::kR6CondBranch:
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR6(condition, lhs, rhs, offset);
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction != Branch::kUnfilledDelaySlot) {
        Emit(delayed_instruction);
      } else {
        // TODO: improve by filling the forbidden slot (IFF this is
        // a forbidden and not a delay slot).
        Nop();
      }
      break;
    case Branch::kR6Call:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;
    case Branch::kR6BareUncondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Bc(offset);
      break;
    case Branch::kR6BareCondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      EmitBcondR6(condition, lhs, rhs, offset);
      break;
    case Branch::kR6BareCall:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Balc(offset);
      break;

    // R6 near label.
    case Branch::kR6Label:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Addiupc(lhs, offset);
      break;
    // R6 near literal.
    case Branch::kR6Literal:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Lwpc(lhs, offset);
      break;

    // R6 long branches.
    case Branch::kR6LongUncondBranch:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jic(AT, Low16Bits(offset));
      break;
    case Branch::kR6LongCondBranch:
      DCHECK_NE(delayed_instruction, Branch::kUnfillableDelaySlot);
      if (delayed_instruction != Branch::kUnfilledDelaySlot) {
        Emit(delayed_instruction);
      }
      EmitBcondR6(Branch::OppositeCondition(condition), lhs, rhs, 2);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jic.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jic(AT, Low16Bits(offset));
      break;
    case Branch::kR6LongCall:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in jialc.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Jialc(AT, Low16Bits(offset));
      break;

    // R6 far label.
    case Branch::kR6FarLabel:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in addiu.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Addiu(lhs, AT, Low16Bits(offset));
      break;
    // R6 far literal.
    case Branch::kR6FarLiteral:
      DCHECK_EQ(delayed_instruction, Branch::kUnfilledDelaySlot);
      offset += (offset & 0x8000) << 1;  // Account for sign extension in lw.
      CHECK_EQ(overwrite_location_, branch->GetOffsetLocation());
      Auipc(AT, High16Bits(offset));
      Lw(lhs, AT, Low16Bits(offset));
      break;
  }
  CHECK_EQ(overwrite_location_, branch->GetEndLocation());
  CHECK_LT(branch->GetSize(), static_cast<uint32_t>(Branch::kMaxBranchSize));
  if (patcher_label != nullptr) {
    // The patched instruction should look like one.
    uint32_t patched_instruction = buffer_.Load<uint32_t>(GetLabelLocation(patcher_label));
    CHECK(!IsAbsorbableInstruction(patched_instruction));
  }
}

void MipsAssembler::B(MipsLabel* label, bool is_bare) {
  Buncond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare);
}

void MipsAssembler::Bal(MipsLabel* label, bool is_bare) {
  Call(label, /* is_r6 */ (IsR6() && !is_bare), is_bare);
}

void MipsAssembler::Beq(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondEQ, rs, rt);
}

void MipsAssembler::Bne(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondNE, rs, rt);
}

void MipsAssembler::Beqz(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondEQZ, rt);
}

void MipsAssembler::Bnez(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondNEZ, rt);
}

void MipsAssembler::Bltz(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondLTZ, rt);
}

void MipsAssembler::Bgez(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondGEZ, rt);
}

void MipsAssembler::Blez(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondLEZ, rt);
}

void MipsAssembler::Bgtz(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ (IsR6() && !is_bare), is_bare, kCondGTZ, rt);
}

bool MipsAssembler::CanExchangeWithSlt(Register rs, Register rt) const {
  // If the instruction modifies AT, `rs` or `rt`, it can't be exchanged with the slt[u]
  // instruction because either slt[u] depends on `rs` or `rt` or the following
  // conditional branch depends on AT set by slt[u].
  // Likewise, if the instruction depends on AT, it can't be exchanged with slt[u]
  // because slt[u] changes AT.
  return (delay_slot_.instruction_ != 0 &&
      (delay_slot_.masks_.gpr_outs_ & ((1u << AT) | (1u << rs) | (1u << rt))) == 0 &&
      (delay_slot_.masks_.gpr_ins_ & (1u << AT)) == 0);
}

void MipsAssembler::ExchangeWithSlt(const DelaySlot& forwarded_slot) {
  // Exchange the last two instructions in the assembler buffer.
  size_t size = buffer_.Size();
  CHECK_GE(size, 2 * sizeof(uint32_t));
  size_t pos1 = size - 2 * sizeof(uint32_t);
  size_t pos2 = size - sizeof(uint32_t);
  uint32_t instr1 = buffer_.Load<uint32_t>(pos1);
  uint32_t instr2 = buffer_.Load<uint32_t>(pos2);
  CHECK_EQ(instr1, forwarded_slot.instruction_);
  CHECK_EQ(instr2, delay_slot_.instruction_);
  buffer_.Store<uint32_t>(pos1, instr2);
  buffer_.Store<uint32_t>(pos2, instr1);
  // Set the current delay slot information to that of the last instruction
  // in the buffer.
  delay_slot_ = forwarded_slot;
}

void MipsAssembler::GenerateSltForCondBranch(bool unsigned_slt, Register rs, Register rt) {
  // If possible, exchange the slt[u] instruction with the preceding instruction,
  // so it can fill the delay slot.
  DelaySlot forwarded_slot = delay_slot_;
  bool exchange = CanExchangeWithSlt(rs, rt);
  if (exchange) {
    // The last instruction cannot be used in a different delay slot,
    // do not commit the label before it (if any).
    DsFsmDropLabel();
  }
  if (unsigned_slt) {
    Sltu(AT, rs, rt);
  } else {
    Slt(AT, rs, rt);
  }
  if (exchange) {
    ExchangeWithSlt(forwarded_slot);
  }
}

void MipsAssembler::Blt(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  if (IsR6() && !is_bare) {
    Bcond(label, IsR6(), is_bare, kCondLT, rs, rt);
  } else if (!Branch::IsNop(kCondLT, rs, rt)) {
    // Synthesize the instruction (not available on R2).
    GenerateSltForCondBranch(/* unsigned_slt */ false, rs, rt);
    Bnez(AT, label, is_bare);
  }
}

void MipsAssembler::Bge(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  if (IsR6() && !is_bare) {
    Bcond(label, IsR6(), is_bare, kCondGE, rs, rt);
  } else if (Branch::IsUncond(kCondGE, rs, rt)) {
    B(label, is_bare);
  } else {
    // Synthesize the instruction (not available on R2).
    GenerateSltForCondBranch(/* unsigned_slt */ false, rs, rt);
    Beqz(AT, label, is_bare);
  }
}

void MipsAssembler::Bltu(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  if (IsR6() && !is_bare) {
    Bcond(label, IsR6(), is_bare, kCondLTU, rs, rt);
  } else if (!Branch::IsNop(kCondLTU, rs, rt)) {
    // Synthesize the instruction (not available on R2).
    GenerateSltForCondBranch(/* unsigned_slt */ true, rs, rt);
    Bnez(AT, label, is_bare);
  }
}

void MipsAssembler::Bgeu(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  if (IsR6() && !is_bare) {
    Bcond(label, IsR6(), is_bare, kCondGEU, rs, rt);
  } else if (Branch::IsUncond(kCondGEU, rs, rt)) {
    B(label, is_bare);
  } else {
    // Synthesize the instruction (not available on R2).
    GenerateSltForCondBranch(/* unsigned_slt */ true, rs, rt);
    Beqz(AT, label, is_bare);
  }
}

void MipsAssembler::Bc1f(MipsLabel* label, bool is_bare) {
  Bc1f(0, label, is_bare);
}

void MipsAssembler::Bc1f(int cc, MipsLabel* label, bool is_bare) {
  CHECK(IsUint<3>(cc)) << cc;
  Bcond(label, /* is_r6 */ false, is_bare, kCondF, static_cast<Register>(cc), ZERO);
}

void MipsAssembler::Bc1t(MipsLabel* label, bool is_bare) {
  Bc1t(0, label, is_bare);
}

void MipsAssembler::Bc1t(int cc, MipsLabel* label, bool is_bare) {
  CHECK(IsUint<3>(cc)) << cc;
  Bcond(label, /* is_r6 */ false, is_bare, kCondT, static_cast<Register>(cc), ZERO);
}

void MipsAssembler::Bc(MipsLabel* label, bool is_bare) {
  Buncond(label, /* is_r6 */ true, is_bare);
}

void MipsAssembler::Balc(MipsLabel* label, bool is_bare) {
  Call(label, /* is_r6 */ true, is_bare);
}

void MipsAssembler::Beqc(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondEQ, rs, rt);
}

void MipsAssembler::Bnec(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondNE, rs, rt);
}

void MipsAssembler::Beqzc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondEQZ, rt);
}

void MipsAssembler::Bnezc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondNEZ, rt);
}

void MipsAssembler::Bltzc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLTZ, rt);
}

void MipsAssembler::Bgezc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGEZ, rt);
}

void MipsAssembler::Blezc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLEZ, rt);
}

void MipsAssembler::Bgtzc(Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGTZ, rt);
}

void MipsAssembler::Bltc(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLT, rs, rt);
}

void MipsAssembler::Bgec(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGE, rs, rt);
}

void MipsAssembler::Bltuc(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondLTU, rs, rt);
}

void MipsAssembler::Bgeuc(Register rs, Register rt, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondGEU, rs, rt);
}

void MipsAssembler::Bc1eqz(FRegister ft, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondF, static_cast<Register>(ft), ZERO);
}

void MipsAssembler::Bc1nez(FRegister ft, MipsLabel* label, bool is_bare) {
  Bcond(label, /* is_r6 */ true, is_bare, kCondT, static_cast<Register>(ft), ZERO);
}

void MipsAssembler::AdjustBaseAndOffset(Register& base,
                                        int32_t& offset,
                                        bool is_doubleword,
                                        bool is_float) {
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

  bool doubleword_aligned = IsAligned<kMipsDoublewordSize>(offset);
  bool two_accesses = is_doubleword && (!is_float || !doubleword_aligned);

  // IsInt<16> must be passed a signed value, hence the static cast below.
  if (IsInt<16>(offset) &&
      (!two_accesses || IsInt<16>(static_cast<int32_t>(offset + kMipsWordSize)))) {
    // Nothing to do: `offset` (and, if needed, `offset + 4`) fits into int16_t.
    return;
  }

  // Remember the "(mis)alignment" of `offset`, it will be checked at the end.
  uint32_t misalignment = offset & (kMipsDoublewordSize - 1);

  // Do not load the whole 32-bit `offset` if it can be represented as
  // a sum of two 16-bit signed offsets. This can save an instruction or two.
  // To simplify matters, only do this for a symmetric range of offsets from
  // about -64KB to about +64KB, allowing further addition of 4 when accessing
  // 64-bit variables with two 32-bit accesses.
  constexpr int32_t kMinOffsetForSimpleAdjustment = 0x7ff8;  // Max int16_t that's a multiple of 8.
  constexpr int32_t kMaxOffsetForSimpleAdjustment = 2 * kMinOffsetForSimpleAdjustment;
  if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Addiu(AT, base, kMinOffsetForSimpleAdjustment);
    offset -= kMinOffsetForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Addiu(AT, base, -kMinOffsetForSimpleAdjustment);
    offset += kMinOffsetForSimpleAdjustment;
  } else if (IsR6()) {
    // On R6 take advantage of the aui instruction, e.g.:
    //   aui   AT, base, offset_high
    //   lw    reg_lo, offset_low(AT)
    //   lw    reg_hi, (offset_low+4)(AT)
    // or when offset_low+4 overflows int16_t:
    //   aui   AT, base, offset_high
    //   addiu AT, AT, 8
    //   lw    reg_lo, (offset_low-8)(AT)
    //   lw    reg_hi, (offset_low-4)(AT)
    int16_t offset_high = High16Bits(offset);
    int16_t offset_low = Low16Bits(offset);
    offset_high += (offset_low < 0) ? 1 : 0;  // Account for offset sign extension in load/store.
    Aui(AT, base, offset_high);
    if (two_accesses && !IsInt<16>(static_cast<int32_t>(offset_low + kMipsWordSize))) {
      // Avoid overflow in the 16-bit offset of the load/store instruction when adding 4.
      Addiu(AT, AT, kMipsDoublewordSize);
      offset_low -= kMipsDoublewordSize;
    }
    offset = offset_low;
  } else {
    // Do not load the whole 32-bit `offset` if it can be represented as
    // a sum of three 16-bit signed offsets. This can save an instruction.
    // To simplify matters, only do this for a symmetric range of offsets from
    // about -96KB to about +96KB, allowing further addition of 4 when accessing
    // 64-bit variables with two 32-bit accesses.
    constexpr int32_t kMinOffsetForMediumAdjustment = 2 * kMinOffsetForSimpleAdjustment;
    constexpr int32_t kMaxOffsetForMediumAdjustment = 3 * kMinOffsetForSimpleAdjustment;
    if (0 <= offset && offset <= kMaxOffsetForMediumAdjustment) {
      Addiu(AT, base, kMinOffsetForMediumAdjustment / 2);
      Addiu(AT, AT, kMinOffsetForMediumAdjustment / 2);
      offset -= kMinOffsetForMediumAdjustment;
    } else if (-kMaxOffsetForMediumAdjustment <= offset && offset < 0) {
      Addiu(AT, base, -kMinOffsetForMediumAdjustment / 2);
      Addiu(AT, AT, -kMinOffsetForMediumAdjustment / 2);
      offset += kMinOffsetForMediumAdjustment;
    } else {
      // Now that all shorter options have been exhausted, load the full 32-bit offset.
      int32_t loaded_offset = RoundDown(offset, kMipsDoublewordSize);
      LoadConst32(AT, loaded_offset);
      Addu(AT, AT, base);
      offset -= loaded_offset;
    }
  }
  base = AT;

  CHECK(IsInt<16>(offset));
  if (two_accesses) {
    CHECK(IsInt<16>(static_cast<int32_t>(offset + kMipsWordSize)));
  }
  CHECK_EQ(misalignment, offset & (kMipsDoublewordSize - 1));
}

void MipsAssembler::AdjustBaseOffsetAndElementSizeShift(Register& base,
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
  } else if (IsAligned<kMipsDoublewordSize>(offset)) {
    element_size_shift = TIMES_8;
  } else if (IsAligned<kMipsWordSize>(offset)) {
    element_size_shift = TIMES_4;
  } else if (IsAligned<kMipsHalfwordSize>(offset)) {
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

  // First, see if `offset` can be represented as a sum of two or three signed offsets.
  // This can save an instruction or two.

  // Max int16_t that's a multiple of element size.
  const int32_t kMaxDeltaForSimpleAdjustment = 0x8000 - (1 << element_size_shift);
  // Max ld.df/st.df offset that's a multiple of element size.
  const int32_t kMaxLoadStoreOffset = 0x1ff << element_size_shift;
  const int32_t kMaxOffsetForSimpleAdjustment = kMaxDeltaForSimpleAdjustment + kMaxLoadStoreOffset;
  const int32_t kMinOffsetForMediumAdjustment = 2 * kMaxDeltaForSimpleAdjustment;
  const int32_t kMaxOffsetForMediumAdjustment = kMinOffsetForMediumAdjustment + kMaxLoadStoreOffset;

  if (IsInt<16>(offset)) {
    Addiu(AT, base, offset);
    offset = 0;
  } else if (0 <= offset && offset <= kMaxOffsetForSimpleAdjustment) {
    Addiu(AT, base, kMaxDeltaForSimpleAdjustment);
    offset -= kMaxDeltaForSimpleAdjustment;
  } else if (-kMaxOffsetForSimpleAdjustment <= offset && offset < 0) {
    Addiu(AT, base, -kMaxDeltaForSimpleAdjustment);
    offset += kMaxDeltaForSimpleAdjustment;
  } else if (!IsR6() && 0 <= offset && offset <= kMaxOffsetForMediumAdjustment) {
    Addiu(AT, base, kMaxDeltaForSimpleAdjustment);
    if (offset <= kMinOffsetForMediumAdjustment) {
      Addiu(AT, AT, offset - kMaxDeltaForSimpleAdjustment);
      offset = 0;
    } else {
      Addiu(AT, AT, kMaxDeltaForSimpleAdjustment);
      offset -= kMinOffsetForMediumAdjustment;
    }
  } else if (!IsR6() && -kMaxOffsetForMediumAdjustment <= offset && offset < 0) {
    Addiu(AT, base, -kMaxDeltaForSimpleAdjustment);
    if (-kMinOffsetForMediumAdjustment <= offset) {
      Addiu(AT, AT, offset + kMaxDeltaForSimpleAdjustment);
      offset = 0;
    } else {
      Addiu(AT, AT, -kMaxDeltaForSimpleAdjustment);
      offset += kMinOffsetForMediumAdjustment;
    }
  } else {
    // 16-bit or smaller parts of `offset`:
    // |31  hi  16|15  mid  13-10|12-9  low  0|
    //
    // Instructions that supply each part as a signed integer addend:
    // |aui       |addiu         |ld.df/st.df |
    uint32_t tmp = static_cast<uint32_t>(offset) - low;  // Exclude `low` from the rest of `offset`
                                                         // (accounts for sign of `low`).
    tmp += (tmp & (UINT32_C(1) << 15)) << 1;  // Account for sign extension in addiu.
    int16_t mid = Low16Bits(tmp);
    int16_t hi = High16Bits(tmp);
    if (IsR6()) {
      Aui(AT, base, hi);
    } else {
      Lui(AT, hi);
      Addu(AT, AT, base);
    }
    if (mid != 0) {
      Addiu(AT, AT, mid);
    }
    offset = low;
  }
  base = AT;
  CHECK_GE(JAVASTYLE_CTZ(offset), element_size_shift);
  CHECK(IsInt<10>(offset >> element_size_shift));
}

void MipsAssembler::LoadFromOffset(LoadOperandType type,
                                   Register reg,
                                   Register base,
                                   int32_t offset) {
  LoadFromOffset<>(type, reg, base, offset);
}

void MipsAssembler::LoadSFromOffset(FRegister reg, Register base, int32_t offset) {
  LoadSFromOffset<>(reg, base, offset);
}

void MipsAssembler::LoadDFromOffset(FRegister reg, Register base, int32_t offset) {
  LoadDFromOffset<>(reg, base, offset);
}

void MipsAssembler::LoadQFromOffset(FRegister reg, Register base, int32_t offset) {
  LoadQFromOffset<>(reg, base, offset);
}

void MipsAssembler::EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset,
                             size_t size) {
  MipsManagedRegister dst = m_dst.AsMips();
  if (dst.IsNoRegister()) {
    CHECK_EQ(0u, size) << dst;
  } else if (dst.IsCoreRegister()) {
    CHECK_EQ(kMipsWordSize, size) << dst;
    LoadFromOffset(kLoadWord, dst.AsCoreRegister(), src_register, src_offset);
  } else if (dst.IsRegisterPair()) {
    CHECK_EQ(kMipsDoublewordSize, size) << dst;
    LoadFromOffset(kLoadDoubleword, dst.AsRegisterPairLow(), src_register, src_offset);
  } else if (dst.IsFRegister()) {
    if (size == kMipsWordSize) {
      LoadSFromOffset(dst.AsFRegister(), src_register, src_offset);
    } else {
      CHECK_EQ(kMipsDoublewordSize, size) << dst;
      LoadDFromOffset(dst.AsFRegister(), src_register, src_offset);
    }
  } else if (dst.IsDRegister()) {
    CHECK_EQ(kMipsDoublewordSize, size) << dst;
    LoadDFromOffset(dst.AsOverlappingDRegisterLow(), src_register, src_offset);
  }
}

void MipsAssembler::StoreToOffset(StoreOperandType type,
                                  Register reg,
                                  Register base,
                                  int32_t offset) {
  StoreToOffset<>(type, reg, base, offset);
}

void MipsAssembler::StoreSToOffset(FRegister reg, Register base, int32_t offset) {
  StoreSToOffset<>(reg, base, offset);
}

void MipsAssembler::StoreDToOffset(FRegister reg, Register base, int32_t offset) {
  StoreDToOffset<>(reg, base, offset);
}

void MipsAssembler::StoreQToOffset(FRegister reg, Register base, int32_t offset) {
  StoreQToOffset<>(reg, base, offset);
}

static dwarf::Reg DWARFReg(Register reg) {
  return dwarf::Reg::MipsCore(static_cast<int>(reg));
}

constexpr size_t kFramePointerSize = 4;

void MipsAssembler::BuildFrame(size_t frame_size,
                               ManagedRegister method_reg,
                               ArrayRef<const ManagedRegister> callee_save_regs,
                               const ManagedRegisterEntrySpills& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);

  // Increase frame to required size.
  IncreaseFrameSize(frame_size);

  // Push callee saves and return address.
  int stack_offset = frame_size - kFramePointerSize;
  StoreToOffset(kStoreWord, RA, SP, stack_offset);
  cfi_.RelOffset(DWARFReg(RA), stack_offset);
  for (int i = callee_save_regs.size() - 1; i >= 0; --i) {
    stack_offset -= kFramePointerSize;
    Register reg = callee_save_regs[i].AsMips().AsCoreRegister();
    StoreToOffset(kStoreWord, reg, SP, stack_offset);
    cfi_.RelOffset(DWARFReg(reg), stack_offset);
  }

  // Write out Method*.
  StoreToOffset(kStoreWord, method_reg.AsMips().AsCoreRegister(), SP, 0);

  // Write out entry spills.
  int32_t offset = frame_size + kFramePointerSize;
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    MipsManagedRegister reg = entry_spills.at(i).AsMips();
    if (reg.IsNoRegister()) {
      ManagedRegisterSpill spill = entry_spills.at(i);
      offset += spill.getSize();
    } else if (reg.IsCoreRegister()) {
      StoreToOffset(kStoreWord, reg.AsCoreRegister(), SP, offset);
      offset += kMipsWordSize;
    } else if (reg.IsFRegister()) {
      StoreSToOffset(reg.AsFRegister(), SP, offset);
      offset += kMipsWordSize;
    } else if (reg.IsDRegister()) {
      StoreDToOffset(reg.AsOverlappingDRegisterLow(), SP, offset);
      offset += kMipsDoublewordSize;
    }
  }
}

void MipsAssembler::RemoveFrame(size_t frame_size,
                                ArrayRef<const ManagedRegister> callee_save_regs,
                                bool may_suspend ATTRIBUTE_UNUSED) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  DCHECK(!overwriting_);
  cfi_.RememberState();

  // Pop callee saves and return address.
  int stack_offset = frame_size - (callee_save_regs.size() * kFramePointerSize) - kFramePointerSize;
  for (size_t i = 0; i < callee_save_regs.size(); ++i) {
    Register reg = callee_save_regs[i].AsMips().AsCoreRegister();
    LoadFromOffset(kLoadWord, reg, SP, stack_offset);
    cfi_.Restore(DWARFReg(reg));
    stack_offset += kFramePointerSize;
  }
  LoadFromOffset(kLoadWord, RA, SP, stack_offset);
  cfi_.Restore(DWARFReg(RA));

  // Adjust the stack pointer in the delay slot if doing so doesn't break CFI.
  bool exchange = IsInt<16>(static_cast<int32_t>(frame_size));
  bool reordering = SetReorder(false);
  if (exchange) {
    // Jump to the return address.
    Jr(RA);
    // Decrease frame to required size.
    DecreaseFrameSize(frame_size);  // Single instruction in delay slot.
  } else {
    // Decrease frame to required size.
    DecreaseFrameSize(frame_size);
    // Jump to the return address.
    Jr(RA);
    Nop();  // In delay slot.
  }
  SetReorder(reordering);

  // The CFI should be restored for any code that follows the exit block.
  cfi_.RestoreState();
  cfi_.DefCFAOffset(frame_size);
}

void MipsAssembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  Addiu32(SP, SP, -adjust);
  cfi_.AdjustCFAOffset(adjust);
  if (overwriting_) {
    cfi_.OverrideDelayedPC(overwrite_location_);
  }
}

void MipsAssembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kFramePointerSize);
  Addiu32(SP, SP, adjust);
  cfi_.AdjustCFAOffset(-adjust);
  if (overwriting_) {
    cfi_.OverrideDelayedPC(overwrite_location_);
  }
}

void MipsAssembler::Store(FrameOffset dest, ManagedRegister msrc, size_t size) {
  MipsManagedRegister src = msrc.AsMips();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCoreRegister()) {
    CHECK_EQ(kMipsWordSize, size);
    StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(kMipsDoublewordSize, size);
    StoreToOffset(kStoreWord, src.AsRegisterPairLow(), SP, dest.Int32Value());
    StoreToOffset(kStoreWord, src.AsRegisterPairHigh(),
                  SP, dest.Int32Value() + kMipsWordSize);
  } else if (src.IsFRegister()) {
    if (size == kMipsWordSize) {
      StoreSToOffset(src.AsFRegister(), SP, dest.Int32Value());
    } else {
      CHECK_EQ(kMipsDoublewordSize, size);
      StoreDToOffset(src.AsFRegister(), SP, dest.Int32Value());
    }
  } else if (src.IsDRegister()) {
    CHECK_EQ(kMipsDoublewordSize, size);
    StoreDToOffset(src.AsOverlappingDRegisterLow(), SP, dest.Int32Value());
  }
}

void MipsAssembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  MipsManagedRegister src = msrc.AsMips();
  CHECK(src.IsCoreRegister());
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                          ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadConst32(scratch.AsCoreRegister(), imm);
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::StoreStackOffsetToThread(ThreadOffset32 thr_offs,
                                             FrameOffset fr_offs,
                                             ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  Addiu32(scratch.AsCoreRegister(), SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreStackPointerToThread(ThreadOffset32 thr_offs) {
  StoreToOffset(kStoreWord, SP, S1, thr_offs.Int32Value());
}

void MipsAssembler::StoreSpanning(FrameOffset dest, ManagedRegister msrc,
                                  FrameOffset in_off, ManagedRegister mscratch) {
  MipsManagedRegister src = msrc.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  StoreToOffset(kStoreWord, src.AsCoreRegister(), SP, dest.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, in_off.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + kMipsWordSize);
}

void MipsAssembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  return EmitLoad(mdest, SP, src.Int32Value(), size);
}

void MipsAssembler::LoadFromThread(ManagedRegister mdest, ThreadOffset32 src, size_t size) {
  return EmitLoad(mdest, S1, src.Int32Value(), size);
}

void MipsAssembler::LoadRef(ManagedRegister mdest, FrameOffset src) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), SP, src.Int32Value());
}

void MipsAssembler::LoadRef(ManagedRegister mdest, ManagedRegister base, MemberOffset offs,
                            bool unpoison_reference) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && base.AsMips().IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
  if (unpoison_reference) {
    MaybeUnpoisonHeapReference(dest.AsCoreRegister());
  }
}

void MipsAssembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base, Offset offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister() && base.AsMips().IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(),
                 base.AsMips().AsCoreRegister(), offs.Int32Value());
}

void MipsAssembler::LoadRawPtrFromThread(ManagedRegister mdest, ThreadOffset32 offs) {
  MipsManagedRegister dest = mdest.AsMips();
  CHECK(dest.IsCoreRegister());
  LoadFromOffset(kLoadWord, dest.AsCoreRegister(), S1, offs.Int32Value());
}

void MipsAssembler::SignExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no sign extension necessary for mips";
}

void MipsAssembler::ZeroExtend(ManagedRegister /*mreg*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL) << "no zero extension necessary for mips";
}

void MipsAssembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) {
  MipsManagedRegister dest = mdest.AsMips();
  MipsManagedRegister src = msrc.AsMips();
  if (!dest.Equals(src)) {
    if (dest.IsCoreRegister()) {
      CHECK(src.IsCoreRegister()) << src;
      Move(dest.AsCoreRegister(), src.AsCoreRegister());
    } else if (dest.IsFRegister()) {
      CHECK(src.IsFRegister()) << src;
      if (size == kMipsWordSize) {
        MovS(dest.AsFRegister(), src.AsFRegister());
      } else {
        CHECK_EQ(kMipsDoublewordSize, size);
        MovD(dest.AsFRegister(), src.AsFRegister());
      }
    } else if (dest.IsDRegister()) {
      CHECK(src.IsDRegister()) << src;
      MovD(dest.AsOverlappingDRegisterLow(), src.AsOverlappingDRegisterLow());
    } else {
      CHECK(dest.IsRegisterPair()) << dest;
      CHECK(src.IsRegisterPair()) << src;
      // Ensure that the first move doesn't clobber the input of the second.
      if (src.AsRegisterPairHigh() != dest.AsRegisterPairLow()) {
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
      } else {
        Move(dest.AsRegisterPairHigh(), src.AsRegisterPairHigh());
        Move(dest.AsRegisterPairLow(), src.AsRegisterPairLow());
      }
    }
  }
}

void MipsAssembler::CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
}

void MipsAssembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                         ThreadOffset32 thr_offs,
                                         ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, thr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                SP, fr_offs.Int32Value());
}

void MipsAssembler::CopyRawPtrToThread(ThreadOffset32 thr_offs,
                                       FrameOffset fr_offs,
                                       ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 SP, fr_offs.Int32Value());
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(),
                S1, thr_offs.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest, FrameOffset src, ManagedRegister mscratch, size_t size) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  CHECK(size == kMipsWordSize || size == kMipsDoublewordSize) << size;
  if (size == kMipsWordSize) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
  } else if (size == kMipsDoublewordSize) {
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value());
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value());
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, src.Int32Value() + kMipsWordSize);
    StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, dest.Int32Value() + kMipsWordSize);
  }
}

void MipsAssembler::Copy(FrameOffset dest, ManagedRegister src_base, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, kMipsWordSize);
  LoadFromOffset(kLoadWord, scratch, src_base.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, SP, dest.Int32Value());
}

void MipsAssembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                         ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsMips().AsCoreRegister();
  CHECK_EQ(size, kMipsWordSize);
  LoadFromOffset(kLoadWord, scratch, SP, src.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest_base.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                         FrameOffset src_base ATTRIBUTE_UNUSED,
                         Offset src_offset ATTRIBUTE_UNUSED,
                         ManagedRegister mscratch ATTRIBUTE_UNUSED,
                         size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no MIPS implementation";
}

void MipsAssembler::Copy(ManagedRegister dest, Offset dest_offset,
                         ManagedRegister src, Offset src_offset,
                         ManagedRegister mscratch, size_t size) {
  CHECK_EQ(size, kMipsWordSize);
  Register scratch = mscratch.AsMips().AsCoreRegister();
  LoadFromOffset(kLoadWord, scratch, src.AsMips().AsCoreRegister(), src_offset.Int32Value());
  StoreToOffset(kStoreWord, scratch, dest.AsMips().AsCoreRegister(), dest_offset.Int32Value());
}

void MipsAssembler::Copy(FrameOffset dest ATTRIBUTE_UNUSED,
                         Offset dest_offset ATTRIBUTE_UNUSED,
                         FrameOffset src ATTRIBUTE_UNUSED,
                         Offset src_offset ATTRIBUTE_UNUSED,
                         ManagedRegister mscratch ATTRIBUTE_UNUSED,
                         size_t size ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no MIPS implementation";
}

void MipsAssembler::MemoryBarrier(ManagedRegister) {
  // TODO: sync?
  UNIMPLEMENTED(FATAL) << "no MIPS implementation";
}

void MipsAssembler::CreateHandleScopeEntry(ManagedRegister mout_reg,
                                           FrameOffset handle_scope_offset,
                                           ManagedRegister min_reg,
                                           bool null_allowed) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(in_reg.IsNoRegister() || in_reg.IsCoreRegister()) << in_reg;
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  if (null_allowed) {
    MipsLabel null_arg;
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // E.g. out_reg = (handle == 0) ? 0 : (SP+handle_offset).
    if (in_reg.IsNoRegister()) {
      LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                     SP, handle_scope_offset.Int32Value());
      in_reg = out_reg;
    }
    if (!out_reg.Equals(in_reg)) {
      LoadConst32(out_reg.AsCoreRegister(), 0);
    }
    Beqz(in_reg.AsCoreRegister(), &null_arg);
    Addiu32(out_reg.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Addiu32(out_reg.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
  }
}

void MipsAssembler::CreateHandleScopeEntry(FrameOffset out_off,
                                           FrameOffset handle_scope_offset,
                                           ManagedRegister mscratch,
                                           bool null_allowed) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  if (null_allowed) {
    MipsLabel null_arg;
    LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
    // Null values get a handle scope entry value of 0.  Otherwise, the handle scope entry is
    // the address in the handle scope holding the reference.
    // E.g. scratch = (scratch == 0) ? 0 : (SP+handle_scope_offset).
    Beqz(scratch.AsCoreRegister(), &null_arg);
    Addiu32(scratch.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
    Bind(&null_arg);
  } else {
    Addiu32(scratch.AsCoreRegister(), SP, handle_scope_offset.Int32Value());
  }
  StoreToOffset(kStoreWord, scratch.AsCoreRegister(), SP, out_off.Int32Value());
}

// Given a handle scope entry, load the associated reference.
void MipsAssembler::LoadReferenceFromHandleScope(ManagedRegister mout_reg,
                                                 ManagedRegister min_reg) {
  MipsManagedRegister out_reg = mout_reg.AsMips();
  MipsManagedRegister in_reg = min_reg.AsMips();
  CHECK(out_reg.IsCoreRegister()) << out_reg;
  CHECK(in_reg.IsCoreRegister()) << in_reg;
  MipsLabel null_arg;
  if (!out_reg.Equals(in_reg)) {
    LoadConst32(out_reg.AsCoreRegister(), 0);
  }
  Beqz(in_reg.AsCoreRegister(), &null_arg);
  LoadFromOffset(kLoadWord, out_reg.AsCoreRegister(),
                 in_reg.AsCoreRegister(), 0);
  Bind(&null_arg);
}

void MipsAssembler::VerifyObject(ManagedRegister src ATTRIBUTE_UNUSED,
                                 bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references.
}

void MipsAssembler::VerifyObject(FrameOffset src ATTRIBUTE_UNUSED,
                                 bool could_be_null ATTRIBUTE_UNUSED) {
  // TODO: not validating references.
}

void MipsAssembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister base = mbase.AsMips();
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(base.IsCoreRegister()) << base;
  CHECK(scratch.IsCoreRegister()) << scratch;
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 base.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  NopIfNoReordering();
  // TODO: place reference map on call.
}

void MipsAssembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  MipsManagedRegister scratch = mscratch.AsMips();
  CHECK(scratch.IsCoreRegister()) << scratch;
  // Call *(*(SP + base) + offset)
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(), SP, base.Int32Value());
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 scratch.AsCoreRegister(), offset.Int32Value());
  Jalr(scratch.AsCoreRegister());
  NopIfNoReordering();
  // TODO: place reference map on call.
}

void MipsAssembler::CallFromThread(ThreadOffset32 offset ATTRIBUTE_UNUSED,
                                   ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  UNIMPLEMENTED(FATAL) << "no mips implementation";
}

void MipsAssembler::GetCurrentThread(ManagedRegister tr) {
  Move(tr.AsMips().AsCoreRegister(), S1);
}

void MipsAssembler::GetCurrentThread(FrameOffset offset,
                                     ManagedRegister mscratch ATTRIBUTE_UNUSED) {
  StoreToOffset(kStoreWord, S1, SP, offset.Int32Value());
}

void MipsAssembler::ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) {
  MipsManagedRegister scratch = mscratch.AsMips();
  exception_blocks_.emplace_back(scratch, stack_adjust);
  LoadFromOffset(kLoadWord, scratch.AsCoreRegister(),
                 S1, Thread::ExceptionOffset<kMipsPointerSize>().Int32Value());
  Bnez(scratch.AsCoreRegister(), exception_blocks_.back().Entry());
}

void MipsAssembler::EmitExceptionPoll(MipsExceptionSlowPath* exception) {
  Bind(exception->Entry());
  if (exception->stack_adjust_ != 0) {  // Fix up the frame.
    DecreaseFrameSize(exception->stack_adjust_);
  }
  // Pass exception object as argument.
  // Don't care about preserving A0 as this call won't return.
  CheckEntrypointTypes<kQuickDeliverException, void, mirror::Object*>();
  Move(A0, exception->scratch_.AsCoreRegister());
  // Set up call to Thread::Current()->pDeliverException.
  LoadFromOffset(kLoadWord, T9, S1,
    QUICK_ENTRYPOINT_OFFSET(kMipsPointerSize, pDeliverException).Int32Value());
  Jr(T9);
  NopIfNoReordering();

  // Call never returns.
  Break();
}

}  // namespace mips
}  // namespace art
