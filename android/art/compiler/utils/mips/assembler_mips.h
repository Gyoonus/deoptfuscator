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

#ifndef ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
#define ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_

#include <deque>
#include <utility>
#include <vector>

#include "arch/mips/instruction_set_features_mips.h"
#include "base/arena_containers.h"
#include "base/enums.h"
#include "base/macros.h"
#include "base/stl_util_identity.h"
#include "constants_mips.h"
#include "globals.h"
#include "heap_poisoning.h"
#include "managed_register_mips.h"
#include "offsets.h"
#include "utils/assembler.h"
#include "utils/jni_macro_assembler.h"
#include "utils/label.h"

namespace art {
namespace mips {

static constexpr size_t kMipsHalfwordSize = 2;
static constexpr size_t kMipsWordSize = 4;
static constexpr size_t kMipsDoublewordSize = 8;

enum LoadOperandType {
  kLoadSignedByte,
  kLoadUnsignedByte,
  kLoadSignedHalfword,
  kLoadUnsignedHalfword,
  kLoadWord,
  kLoadDoubleword,
  kLoadQuadword
};

enum StoreOperandType {
  kStoreByte,
  kStoreHalfword,
  kStoreWord,
  kStoreDoubleword,
  kStoreQuadword
};

// Used to test the values returned by ClassS/ClassD.
enum FPClassMaskType {
  kSignalingNaN      = 0x001,
  kQuietNaN          = 0x002,
  kNegativeInfinity  = 0x004,
  kNegativeNormal    = 0x008,
  kNegativeSubnormal = 0x010,
  kNegativeZero      = 0x020,
  kPositiveInfinity  = 0x040,
  kPositiveNormal    = 0x080,
  kPositiveSubnormal = 0x100,
  kPositiveZero      = 0x200,
};

// Instruction description in terms of input and output registers.
// Used for instruction reordering.
struct InOutRegMasks {
  InOutRegMasks()
      : gpr_outs_(0), gpr_ins_(0), fpr_outs_(0), fpr_ins_(0), cc_outs_(0), cc_ins_(0) {}

  inline InOutRegMasks& GprOuts(Register reg) {
    gpr_outs_ |= (1u << reg);
    gpr_outs_ &= ~1u;  // Ignore register ZERO.
    return *this;
  }
  template<typename T, typename... Ts>
  inline InOutRegMasks& GprOuts(T one, Ts... more) { GprOuts(one); GprOuts(more...); return *this; }

  inline InOutRegMasks& GprIns(Register reg) {
    gpr_ins_ |= (1u << reg);
    gpr_ins_ &= ~1u;  // Ignore register ZERO.
    return *this;
  }
  template<typename T, typename... Ts>
  inline InOutRegMasks& GprIns(T one, Ts... more) { GprIns(one); GprIns(more...); return *this; }

  inline InOutRegMasks& GprInOuts(Register reg) { GprIns(reg); GprOuts(reg); return *this; }
  template<typename T, typename... Ts>
  inline InOutRegMasks& GprInOuts(T one, Ts... more) {
    GprInOuts(one);
    GprInOuts(more...);
    return *this;
  }

  inline InOutRegMasks& FprOuts(FRegister reg) { fpr_outs_ |= (1u << reg); return *this; }
  inline InOutRegMasks& FprOuts(VectorRegister reg) { return FprOuts(static_cast<FRegister>(reg)); }
  template<typename T, typename... Ts>
  inline InOutRegMasks& FprOuts(T one, Ts... more) { FprOuts(one); FprOuts(more...); return *this; }

  inline InOutRegMasks& FprIns(FRegister reg) { fpr_ins_ |= (1u << reg); return *this; }
  inline InOutRegMasks& FprIns(VectorRegister reg) { return FprIns(static_cast<FRegister>(reg)); }
  template<typename T, typename... Ts>
  inline InOutRegMasks& FprIns(T one, Ts... more) { FprIns(one); FprIns(more...); return *this; }

  inline InOutRegMasks& FprInOuts(FRegister reg) { FprIns(reg); FprOuts(reg); return *this; }
  inline InOutRegMasks& FprInOuts(VectorRegister reg) {
    return FprInOuts(static_cast<FRegister>(reg));
  }
  template<typename T, typename... Ts>
  inline InOutRegMasks& FprInOuts(T one, Ts... more) {
    FprInOuts(one);
    FprInOuts(more...);
    return *this;
  }

  inline InOutRegMasks& CcOuts(int cc) { cc_outs_ |= (1u << cc); return *this; }
  template<typename T, typename... Ts>
  inline InOutRegMasks& CcOuts(T one, Ts... more) { CcOuts(one); CcOuts(more...); return *this; }

  inline InOutRegMasks& CcIns(int cc) { cc_ins_ |= (1u << cc); return *this; }
  template<typename T, typename... Ts>
  inline InOutRegMasks& CcIns(T one, Ts... more) { CcIns(one); CcIns(more...); return *this; }

  // Mask of output GPRs for the instruction.
  uint32_t gpr_outs_;
  // Mask of input GPRs for the instruction.
  uint32_t gpr_ins_;
  // Mask of output FPRs for the instruction.
  uint32_t fpr_outs_;
  // Mask of input FPRs for the instruction.
  uint32_t fpr_ins_;
  // Mask of output FPU condition code flags for the instruction.
  uint32_t cc_outs_;
  // Mask of input FPU condition code flags for the instruction.
  uint32_t cc_ins_;

  // TODO: add LO and HI.
};

class MipsLabel : public Label {
 public:
  MipsLabel() : prev_branch_id_plus_one_(0) {}

  MipsLabel(MipsLabel&& src)
      : Label(std::move(src)), prev_branch_id_plus_one_(src.prev_branch_id_plus_one_) {}

  void AdjustBoundPosition(int delta) {
    CHECK(IsBound());
    // Bound label's position is negative, hence decrementing it.
    position_ -= delta;
  }

 private:
  uint32_t prev_branch_id_plus_one_;  // To get distance from preceding branch, if any.

  friend class MipsAssembler;
  DISALLOW_COPY_AND_ASSIGN(MipsLabel);
};

// Assembler literal is a value embedded in code, retrieved using a PC-relative load.
class Literal {
 public:
  static constexpr size_t kMaxSize = 8;

  Literal(uint32_t size, const uint8_t* data)
      : label_(), size_(size) {
    DCHECK_LE(size, Literal::kMaxSize);
    memcpy(data_, data, size);
  }

  template <typename T>
  T GetValue() const {
    DCHECK_EQ(size_, sizeof(T));
    T value;
    memcpy(&value, data_, sizeof(T));
    return value;
  }

  uint32_t GetSize() const {
    return size_;
  }

  const uint8_t* GetData() const {
    return data_;
  }

  MipsLabel* GetLabel() {
    return &label_;
  }

  const MipsLabel* GetLabel() const {
    return &label_;
  }

 private:
  MipsLabel label_;
  const uint32_t size_;
  uint8_t data_[kMaxSize];

  DISALLOW_COPY_AND_ASSIGN(Literal);
};

// Jump table: table of labels emitted after the literals. Similar to literals.
class JumpTable {
 public:
  explicit JumpTable(std::vector<MipsLabel*>&& labels)
      : label_(), labels_(std::move(labels)) {
  }

  uint32_t GetSize() const {
    return static_cast<uint32_t>(labels_.size()) * sizeof(uint32_t);
  }

  const std::vector<MipsLabel*>& GetData() const {
    return labels_;
  }

  MipsLabel* GetLabel() {
    return &label_;
  }

  const MipsLabel* GetLabel() const {
    return &label_;
  }

 private:
  MipsLabel label_;
  std::vector<MipsLabel*> labels_;

  DISALLOW_COPY_AND_ASSIGN(JumpTable);
};

// Slowpath entered when Thread::Current()->_exception is non-null.
class MipsExceptionSlowPath {
 public:
  explicit MipsExceptionSlowPath(MipsManagedRegister scratch, size_t stack_adjust)
      : scratch_(scratch), stack_adjust_(stack_adjust) {}

  MipsExceptionSlowPath(MipsExceptionSlowPath&& src)
      : scratch_(src.scratch_),
        stack_adjust_(src.stack_adjust_),
        exception_entry_(std::move(src.exception_entry_)) {}

 private:
  MipsLabel* Entry() { return &exception_entry_; }
  const MipsManagedRegister scratch_;
  const size_t stack_adjust_;
  MipsLabel exception_entry_;

  friend class MipsAssembler;
  DISALLOW_COPY_AND_ASSIGN(MipsExceptionSlowPath);
};

class MipsAssembler FINAL : public Assembler, public JNIMacroAssembler<PointerSize::k32> {
 public:
  using JNIBase = JNIMacroAssembler<PointerSize::k32>;

  explicit MipsAssembler(ArenaAllocator* allocator,
                         const MipsInstructionSetFeatures* instruction_set_features = nullptr)
      : Assembler(allocator),
        overwriting_(false),
        overwrite_location_(0),
        reordering_(true),
        ds_fsm_state_(kExpectingLabel),
        ds_fsm_target_pc_(0),
        literals_(allocator->Adapter(kArenaAllocAssembler)),
        jump_tables_(allocator->Adapter(kArenaAllocAssembler)),
        last_position_adjustment_(0),
        last_old_position_(0),
        last_branch_id_(0),
        has_msa_(instruction_set_features != nullptr ? instruction_set_features->HasMsa() : false),
        isa_features_(instruction_set_features) {
    cfi().DelayEmittingAdvancePCs();
  }

  size_t CodeSize() const OVERRIDE { return Assembler::CodeSize(); }
  size_t CodePosition() OVERRIDE;
  DebugFrameOpCodeWriterForAssembler& cfi() { return Assembler::cfi(); }

  virtual ~MipsAssembler() {
    for (auto& branch : branches_) {
      CHECK(branch.IsResolved());
    }
  }

  // Emit Machine Instructions.
  void Addu(Register rd, Register rs, Register rt);
  void Addiu(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label);
  void Addiu(Register rt, Register rs, uint16_t imm16);
  void Subu(Register rd, Register rs, Register rt);

  void MultR2(Register rs, Register rt);  // R2
  void MultuR2(Register rs, Register rt);  // R2
  void DivR2(Register rs, Register rt);  // R2
  void DivuR2(Register rs, Register rt);  // R2
  void MulR2(Register rd, Register rs, Register rt);  // R2
  void DivR2(Register rd, Register rs, Register rt);  // R2
  void ModR2(Register rd, Register rs, Register rt);  // R2
  void DivuR2(Register rd, Register rs, Register rt);  // R2
  void ModuR2(Register rd, Register rs, Register rt);  // R2
  void MulR6(Register rd, Register rs, Register rt);  // R6
  void MuhR6(Register rd, Register rs, Register rt);  // R6
  void MuhuR6(Register rd, Register rs, Register rt);  // R6
  void DivR6(Register rd, Register rs, Register rt);  // R6
  void ModR6(Register rd, Register rs, Register rt);  // R6
  void DivuR6(Register rd, Register rs, Register rt);  // R6
  void ModuR6(Register rd, Register rs, Register rt);  // R6

  void And(Register rd, Register rs, Register rt);
  void Andi(Register rt, Register rs, uint16_t imm16);
  void Or(Register rd, Register rs, Register rt);
  void Ori(Register rt, Register rs, uint16_t imm16);
  void Xor(Register rd, Register rs, Register rt);
  void Xori(Register rt, Register rs, uint16_t imm16);
  void Nor(Register rd, Register rs, Register rt);

  void Movz(Register rd, Register rs, Register rt);  // R2
  void Movn(Register rd, Register rs, Register rt);  // R2
  void Seleqz(Register rd, Register rs, Register rt);  // R6
  void Selnez(Register rd, Register rs, Register rt);  // R6
  void ClzR6(Register rd, Register rs);
  void ClzR2(Register rd, Register rs);
  void CloR6(Register rd, Register rs);
  void CloR2(Register rd, Register rs);

  void Seb(Register rd, Register rt);  // R2+
  void Seh(Register rd, Register rt);  // R2+
  void Wsbh(Register rd, Register rt);  // R2+
  void Bitswap(Register rd, Register rt);  // R6

  void Sll(Register rd, Register rt, int shamt);
  void Srl(Register rd, Register rt, int shamt);
  void Rotr(Register rd, Register rt, int shamt);  // R2+
  void Sra(Register rd, Register rt, int shamt);
  void Sllv(Register rd, Register rt, Register rs);
  void Srlv(Register rd, Register rt, Register rs);
  void Rotrv(Register rd, Register rt, Register rs);  // R2+
  void Srav(Register rd, Register rt, Register rs);
  void Ext(Register rd, Register rt, int pos, int size);  // R2+
  void Ins(Register rd, Register rt, int pos, int size);  // R2+
  void Lsa(Register rd, Register rs, Register rt, int saPlusOne);  // R6
  void ShiftAndAdd(Register dst, Register src_idx, Register src_base, int shamt, Register tmp = AT);

  void Lb(Register rt, Register rs, uint16_t imm16);
  void Lh(Register rt, Register rs, uint16_t imm16);
  void Lw(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label);
  void Lw(Register rt, Register rs, uint16_t imm16);
  void Lwl(Register rt, Register rs, uint16_t imm16);
  void Lwr(Register rt, Register rs, uint16_t imm16);
  void Lbu(Register rt, Register rs, uint16_t imm16);
  void Lhu(Register rt, Register rs, uint16_t imm16);
  void Lwpc(Register rs, uint32_t imm19);  // R6
  void Lui(Register rt, uint16_t imm16);
  void Aui(Register rt, Register rs, uint16_t imm16);  // R6
  void AddUpper(Register rt, Register rs, uint16_t imm16, Register tmp = AT);
  void Sync(uint32_t stype);
  void Mfhi(Register rd);  // R2
  void Mflo(Register rd);  // R2

  void Sb(Register rt, Register rs, uint16_t imm16);
  void Sh(Register rt, Register rs, uint16_t imm16);
  void Sw(Register rt, Register rs, uint16_t imm16, MipsLabel* patcher_label);
  void Sw(Register rt, Register rs, uint16_t imm16);
  void Swl(Register rt, Register rs, uint16_t imm16);
  void Swr(Register rt, Register rs, uint16_t imm16);

  void LlR2(Register rt, Register base, int16_t imm16 = 0);
  void ScR2(Register rt, Register base, int16_t imm16 = 0);
  void LlR6(Register rt, Register base, int16_t imm9 = 0);
  void ScR6(Register rt, Register base, int16_t imm9 = 0);

  void Slt(Register rd, Register rs, Register rt);
  void Sltu(Register rd, Register rs, Register rt);
  void Slti(Register rt, Register rs, uint16_t imm16);
  void Sltiu(Register rt, Register rs, uint16_t imm16);

  // Branches and jumps to immediate offsets/addresses do not take care of their
  // delay/forbidden slots and generally should not be used directly. This applies
  // to the following R2 and R6 branch/jump instructions with imm16, imm21, addr26
  // offsets/addresses.
  // Use branches/jumps to labels instead.
  void B(uint16_t imm16);
  void Bal(uint16_t imm16);
  void Beq(Register rs, Register rt, uint16_t imm16);
  void Bne(Register rs, Register rt, uint16_t imm16);
  void Beqz(Register rt, uint16_t imm16);
  void Bnez(Register rt, uint16_t imm16);
  void Bltz(Register rt, uint16_t imm16);
  void Bgez(Register rt, uint16_t imm16);
  void Blez(Register rt, uint16_t imm16);
  void Bgtz(Register rt, uint16_t imm16);
  void Bc1f(uint16_t imm16);  // R2
  void Bc1f(int cc, uint16_t imm16);  // R2
  void Bc1t(uint16_t imm16);  // R2
  void Bc1t(int cc, uint16_t imm16);  // R2
  void J(uint32_t addr26);
  void Jal(uint32_t addr26);
  // Jalr() and Jr() fill their delay slots when reordering is enabled.
  // When reordering is disabled, the delay slots must be filled manually.
  // You may use NopIfNoReordering() to fill them when reordering is disabled.
  void Jalr(Register rd, Register rs);
  void Jalr(Register rs);
  void Jr(Register rs);
  // Nal() does not fill its delay slot. It must be filled manually.
  void Nal();
  void Auipc(Register rs, uint16_t imm16);  // R6
  void Addiupc(Register rs, uint32_t imm19);  // R6
  void Bc(uint32_t imm26);  // R6
  void Balc(uint32_t imm26);  // R6
  void Jic(Register rt, uint16_t imm16);  // R6
  void Jialc(Register rt, uint16_t imm16);  // R6
  void Bltc(Register rs, Register rt, uint16_t imm16);  // R6
  void Bltzc(Register rt, uint16_t imm16);  // R6
  void Bgtzc(Register rt, uint16_t imm16);  // R6
  void Bgec(Register rs, Register rt, uint16_t imm16);  // R6
  void Bgezc(Register rt, uint16_t imm16);  // R6
  void Blezc(Register rt, uint16_t imm16);  // R6
  void Bltuc(Register rs, Register rt, uint16_t imm16);  // R6
  void Bgeuc(Register rs, Register rt, uint16_t imm16);  // R6
  void Beqc(Register rs, Register rt, uint16_t imm16);  // R6
  void Bnec(Register rs, Register rt, uint16_t imm16);  // R6
  void Beqzc(Register rs, uint32_t imm21);  // R6
  void Bnezc(Register rs, uint32_t imm21);  // R6
  void Bc1eqz(FRegister ft, uint16_t imm16);  // R6
  void Bc1nez(FRegister ft, uint16_t imm16);  // R6

  void AddS(FRegister fd, FRegister fs, FRegister ft);
  void SubS(FRegister fd, FRegister fs, FRegister ft);
  void MulS(FRegister fd, FRegister fs, FRegister ft);
  void DivS(FRegister fd, FRegister fs, FRegister ft);
  void AddD(FRegister fd, FRegister fs, FRegister ft);
  void SubD(FRegister fd, FRegister fs, FRegister ft);
  void MulD(FRegister fd, FRegister fs, FRegister ft);
  void DivD(FRegister fd, FRegister fs, FRegister ft);
  void SqrtS(FRegister fd, FRegister fs);
  void SqrtD(FRegister fd, FRegister fs);
  void AbsS(FRegister fd, FRegister fs);
  void AbsD(FRegister fd, FRegister fs);
  void MovS(FRegister fd, FRegister fs);
  void MovD(FRegister fd, FRegister fs);
  void NegS(FRegister fd, FRegister fs);
  void NegD(FRegister fd, FRegister fs);

  void CunS(FRegister fs, FRegister ft);  // R2
  void CunS(int cc, FRegister fs, FRegister ft);  // R2
  void CeqS(FRegister fs, FRegister ft);  // R2
  void CeqS(int cc, FRegister fs, FRegister ft);  // R2
  void CueqS(FRegister fs, FRegister ft);  // R2
  void CueqS(int cc, FRegister fs, FRegister ft);  // R2
  void ColtS(FRegister fs, FRegister ft);  // R2
  void ColtS(int cc, FRegister fs, FRegister ft);  // R2
  void CultS(FRegister fs, FRegister ft);  // R2
  void CultS(int cc, FRegister fs, FRegister ft);  // R2
  void ColeS(FRegister fs, FRegister ft);  // R2
  void ColeS(int cc, FRegister fs, FRegister ft);  // R2
  void CuleS(FRegister fs, FRegister ft);  // R2
  void CuleS(int cc, FRegister fs, FRegister ft);  // R2
  void CunD(FRegister fs, FRegister ft);  // R2
  void CunD(int cc, FRegister fs, FRegister ft);  // R2
  void CeqD(FRegister fs, FRegister ft);  // R2
  void CeqD(int cc, FRegister fs, FRegister ft);  // R2
  void CueqD(FRegister fs, FRegister ft);  // R2
  void CueqD(int cc, FRegister fs, FRegister ft);  // R2
  void ColtD(FRegister fs, FRegister ft);  // R2
  void ColtD(int cc, FRegister fs, FRegister ft);  // R2
  void CultD(FRegister fs, FRegister ft);  // R2
  void CultD(int cc, FRegister fs, FRegister ft);  // R2
  void ColeD(FRegister fs, FRegister ft);  // R2
  void ColeD(int cc, FRegister fs, FRegister ft);  // R2
  void CuleD(FRegister fs, FRegister ft);  // R2
  void CuleD(int cc, FRegister fs, FRegister ft);  // R2
  void CmpUnS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpEqS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUeqS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpLtS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUltS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpLeS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUleS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpOrS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUneS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpNeS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUnD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpEqD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUeqD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpLtD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUltD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpLeD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUleD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpOrD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpUneD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void CmpNeD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void Movf(Register rd, Register rs, int cc = 0);  // R2
  void Movt(Register rd, Register rs, int cc = 0);  // R2
  void MovfS(FRegister fd, FRegister fs, int cc = 0);  // R2
  void MovfD(FRegister fd, FRegister fs, int cc = 0);  // R2
  void MovtS(FRegister fd, FRegister fs, int cc = 0);  // R2
  void MovtD(FRegister fd, FRegister fs, int cc = 0);  // R2
  void MovzS(FRegister fd, FRegister fs, Register rt);  // R2
  void MovzD(FRegister fd, FRegister fs, Register rt);  // R2
  void MovnS(FRegister fd, FRegister fs, Register rt);  // R2
  void MovnD(FRegister fd, FRegister fs, Register rt);  // R2
  void SelS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void SelD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void SeleqzS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void SeleqzD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void SelnezS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void SelnezD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void ClassS(FRegister fd, FRegister fs);  // R6
  void ClassD(FRegister fd, FRegister fs);  // R6
  void MinS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void MinD(FRegister fd, FRegister fs, FRegister ft);  // R6
  void MaxS(FRegister fd, FRegister fs, FRegister ft);  // R6
  void MaxD(FRegister fd, FRegister fs, FRegister ft);  // R6

  void TruncLS(FRegister fd, FRegister fs);  // R2+, FR=1
  void TruncLD(FRegister fd, FRegister fs);  // R2+, FR=1
  void TruncWS(FRegister fd, FRegister fs);
  void TruncWD(FRegister fd, FRegister fs);
  void Cvtsw(FRegister fd, FRegister fs);
  void Cvtdw(FRegister fd, FRegister fs);
  void Cvtsd(FRegister fd, FRegister fs);
  void Cvtds(FRegister fd, FRegister fs);
  void Cvtsl(FRegister fd, FRegister fs);  // R2+, FR=1
  void Cvtdl(FRegister fd, FRegister fs);  // R2+, FR=1
  void FloorWS(FRegister fd, FRegister fs);
  void FloorWD(FRegister fd, FRegister fs);

  // Note, the 32 LSBs of a 64-bit value must be loaded into an FPR before the 32 MSBs
  // when loading the value as 32-bit halves. This applies to all 32-bit FPR loads:
  // Mtc1(), Mthc1(), MoveToFpuHigh(), Lwc1(). Even if you need two Mtc1()'s or two
  // Lwc1()'s to load a pair of 32-bit FPRs and these loads do not interfere with one
  // another (unlike Mtc1() and Mthc1() with 64-bit FPRs), maintain the order:
  // low then high.
  //
  // Also, prefer MoveFromFpuHigh()/MoveToFpuHigh() over Mfhc1()/Mthc1() and Mfc1()/Mtc1().
  // This will save you some if statements.
  FRegister GetFpuRegLow(FRegister reg);
  void Mfc1(Register rt, FRegister fs);
  void Mtc1(Register rt, FRegister fs);
  void Mfhc1(Register rt, FRegister fs);
  void Mthc1(Register rt, FRegister fs);
  void MoveFromFpuHigh(Register rt, FRegister fs);
  void MoveToFpuHigh(Register rt, FRegister fs);
  void Lwc1(FRegister ft, Register rs, uint16_t imm16);
  void Ldc1(FRegister ft, Register rs, uint16_t imm16);
  void Swc1(FRegister ft, Register rs, uint16_t imm16);
  void Sdc1(FRegister ft, Register rs, uint16_t imm16);

  void Break();
  void Nop();
  void NopIfNoReordering();
  void Move(Register rd, Register rs);
  void Clear(Register rd);
  void Not(Register rd, Register rs);

  // MSA instructions.
  void AndV(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void OrV(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void NorV(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void XorV(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  void AddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void AddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void AddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void AddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Asub_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MulvB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MulvH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MulvW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MulvD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Div_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Mod_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Add_aB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Add_aH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Add_aW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Add_aD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Ave_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Aver_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Max_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_sB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_uB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Min_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  void FaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmulW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmulD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FdivW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FdivD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmaxW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmaxD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FminW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FminD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  void Ffint_sW(VectorRegister wd, VectorRegister ws);
  void Ffint_sD(VectorRegister wd, VectorRegister ws);
  void Ftint_sW(VectorRegister wd, VectorRegister ws);
  void Ftint_sD(VectorRegister wd, VectorRegister ws);

  void SllB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SllH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SllW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SllD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SraB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SraH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SraW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SraD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SrlB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SrlH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SrlW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void SrlD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  // Immediate shift instructions, where shamtN denotes shift amount (must be between 0 and 2^N-1).
  void SlliB(VectorRegister wd, VectorRegister ws, int shamt3);
  void SlliH(VectorRegister wd, VectorRegister ws, int shamt4);
  void SlliW(VectorRegister wd, VectorRegister ws, int shamt5);
  void SlliD(VectorRegister wd, VectorRegister ws, int shamt6);
  void SraiB(VectorRegister wd, VectorRegister ws, int shamt3);
  void SraiH(VectorRegister wd, VectorRegister ws, int shamt4);
  void SraiW(VectorRegister wd, VectorRegister ws, int shamt5);
  void SraiD(VectorRegister wd, VectorRegister ws, int shamt6);
  void SrliB(VectorRegister wd, VectorRegister ws, int shamt3);
  void SrliH(VectorRegister wd, VectorRegister ws, int shamt4);
  void SrliW(VectorRegister wd, VectorRegister ws, int shamt5);
  void SrliD(VectorRegister wd, VectorRegister ws, int shamt6);

  void MoveV(VectorRegister wd, VectorRegister ws);
  void SplatiB(VectorRegister wd, VectorRegister ws, int n4);
  void SplatiH(VectorRegister wd, VectorRegister ws, int n3);
  void SplatiW(VectorRegister wd, VectorRegister ws, int n2);
  void SplatiD(VectorRegister wd, VectorRegister ws, int n1);
  void Copy_sB(Register rd, VectorRegister ws, int n4);
  void Copy_sH(Register rd, VectorRegister ws, int n3);
  void Copy_sW(Register rd, VectorRegister ws, int n2);
  void Copy_uB(Register rd, VectorRegister ws, int n4);
  void Copy_uH(Register rd, VectorRegister ws, int n3);
  void InsertB(VectorRegister wd, Register rs, int n4);
  void InsertH(VectorRegister wd, Register rs, int n3);
  void InsertW(VectorRegister wd, Register rs, int n2);
  void FillB(VectorRegister wd, Register rs);
  void FillH(VectorRegister wd, Register rs);
  void FillW(VectorRegister wd, Register rs);

  void LdiB(VectorRegister wd, int imm8);
  void LdiH(VectorRegister wd, int imm10);
  void LdiW(VectorRegister wd, int imm10);
  void LdiD(VectorRegister wd, int imm10);
  void LdB(VectorRegister wd, Register rs, int offset);
  void LdH(VectorRegister wd, Register rs, int offset);
  void LdW(VectorRegister wd, Register rs, int offset);
  void LdD(VectorRegister wd, Register rs, int offset);
  void StB(VectorRegister wd, Register rs, int offset);
  void StH(VectorRegister wd, Register rs, int offset);
  void StW(VectorRegister wd, Register rs, int offset);
  void StD(VectorRegister wd, Register rs, int offset);

  void IlvlB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvlH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvlW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvlD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvrB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvrH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvrW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvrD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvevB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvevH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvevW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvevD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvodB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvodH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvodW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void IlvodD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  void MaddvB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MaddvH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MaddvW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MaddvD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MsubvB(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MsubvH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MsubvW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void MsubvD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmaddW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmaddD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmsubW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void FmsubD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  void Hadd_sH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Hadd_sW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Hadd_sD(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Hadd_uH(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Hadd_uW(VectorRegister wd, VectorRegister ws, VectorRegister wt);
  void Hadd_uD(VectorRegister wd, VectorRegister ws, VectorRegister wt);

  // Helper for replicating floating point value in all destination elements.
  void ReplicateFPToVectorRegister(VectorRegister dst, FRegister src, bool is_double);

  // Higher level composite instructions.
  void LoadConst32(Register rd, int32_t value);
  void LoadConst64(Register reg_hi, Register reg_lo, int64_t value);
  void LoadDConst64(FRegister rd, int64_t value, Register temp);
  void LoadSConst32(FRegister r, int32_t value, Register temp);
  void Addiu32(Register rt, Register rs, int32_t value, Register rtmp = AT);

  void Bind(MipsLabel* label);
  // When `is_bare` is false, the branches will promote to long (if the range
  // of the individual branch instruction is insufficient) and the delay/
  // forbidden slots will be taken care of.
  // Use `is_bare = false` when the branch target may be out of reach of the
  // individual branch instruction. IOW, this is for general purpose use.
  //
  // When `is_bare` is true, just the branch instructions will be generated
  // leaving delay/forbidden slot filling up to the caller and the branches
  // won't promote to long if the range is insufficient (you'll get a
  // compilation error when the range is exceeded).
  // Use `is_bare = true` when the branch target is known to be within reach
  // of the individual branch instruction. This is intended for small local
  // optimizations around delay/forbidden slots.
  // Also prefer using `is_bare = true` if the code near the branch is to be
  // patched or analyzed at run time (e.g. introspection) to
  // - show the intent and
  // - fail during compilation rather than during patching/execution if the
  //   bare branch range is insufficent but the code size and layout are
  //   expected to remain unchanged
  //
  // R2 branches with delay slots that are also available on R6.
  // On R6 when `is_bare` is false these convert to equivalent R6 compact
  // branches (to reduce code size). On R2 or when `is_bare` is true they
  // remain R2 branches with delay slots.
  void B(MipsLabel* label, bool is_bare = false);
  void Bal(MipsLabel* label, bool is_bare = false);
  void Beq(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  void Bne(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  void Beqz(Register rt, MipsLabel* label, bool is_bare = false);
  void Bnez(Register rt, MipsLabel* label, bool is_bare = false);
  void Bltz(Register rt, MipsLabel* label, bool is_bare = false);
  void Bgez(Register rt, MipsLabel* label, bool is_bare = false);
  void Blez(Register rt, MipsLabel* label, bool is_bare = false);
  void Bgtz(Register rt, MipsLabel* label, bool is_bare = false);
  void Blt(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  void Bge(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  void Bltu(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  void Bgeu(Register rs, Register rt, MipsLabel* label, bool is_bare = false);
  // R2-only branches with delay slots.
  void Bc1f(MipsLabel* label, bool is_bare = false);  // R2
  void Bc1f(int cc, MipsLabel* label, bool is_bare = false);  // R2
  void Bc1t(MipsLabel* label, bool is_bare = false);  // R2
  void Bc1t(int cc, MipsLabel* label, bool is_bare = false);  // R2
  // R6-only compact branches without delay/forbidden slots.
  void Bc(MipsLabel* label, bool is_bare = false);  // R6
  void Balc(MipsLabel* label, bool is_bare = false);  // R6
  // R6-only compact branches with forbidden slots.
  void Beqc(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bnec(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Beqzc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bnezc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bltzc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bgezc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Blezc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bgtzc(Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bltc(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bgec(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bltuc(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  void Bgeuc(Register rs, Register rt, MipsLabel* label, bool is_bare = false);  // R6
  // R6-only branches with delay slots.
  void Bc1eqz(FRegister ft, MipsLabel* label, bool is_bare = false);  // R6
  void Bc1nez(FRegister ft, MipsLabel* label, bool is_bare = false);  // R6

  void EmitLoad(ManagedRegister m_dst, Register src_register, int32_t src_offset, size_t size);
  void AdjustBaseAndOffset(Register& base,
                           int32_t& offset,
                           bool is_doubleword,
                           bool is_float = false);
  void AdjustBaseOffsetAndElementSizeShift(Register& base,
                                           int32_t& offset,
                                           int& element_size_shift);

 private:
  // This will be used as an argument for loads/stores
  // when there is no need for implicit null checks.
  struct NoImplicitNullChecker {
    void operator()() const {}
  };

 public:
  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreConstToOffset(StoreOperandType type,
                          int64_t value,
                          Register base,
                          int32_t offset,
                          Register temp,
                          ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // We permit `base` and `temp` to coincide (however, we check that neither is AT),
    // in which case the `base` register may be overwritten in the process.
    CHECK_NE(temp, AT);  // Must not use AT as temp, so as not to overwrite the adjusted base.
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ (type == kStoreDoubleword));
    uint32_t low = Low32Bits(value);
    uint32_t high = High32Bits(value);
    Register reg;
    // If the adjustment left `base` unchanged and equal to `temp`, we can't use `temp`
    // to load and hold the value but we can use AT instead as AT hasn't been used yet.
    // Otherwise, `temp` can be used for the value. And if `temp` is the same as the
    // original `base` (that is, `base` prior to the adjustment), the original `base`
    // register will be overwritten.
    if (base == temp) {
      temp = AT;
    }
    if (low == 0) {
      reg = ZERO;
    } else {
      reg = temp;
      LoadConst32(reg, low);
    }
    switch (type) {
      case kStoreByte:
        Sb(reg, base, offset);
        break;
      case kStoreHalfword:
        Sh(reg, base, offset);
        break;
      case kStoreWord:
        Sw(reg, base, offset);
        break;
      case kStoreDoubleword:
        Sw(reg, base, offset);
        null_checker();
        if (high == 0) {
          reg = ZERO;
        } else {
          reg = temp;
          if (high != low) {
            LoadConst32(reg, high);
          }
        }
        Sw(reg, base, offset + kMipsWordSize);
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    if (type != kStoreDoubleword) {
      null_checker();
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadFromOffset(LoadOperandType type,
                      Register reg,
                      Register base,
                      int32_t offset,
                      ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ (type == kLoadDoubleword));
    switch (type) {
      case kLoadSignedByte:
        Lb(reg, base, offset);
        break;
      case kLoadUnsignedByte:
        Lbu(reg, base, offset);
        break;
      case kLoadSignedHalfword:
        Lh(reg, base, offset);
        break;
      case kLoadUnsignedHalfword:
        Lhu(reg, base, offset);
        break;
      case kLoadWord:
        Lw(reg, base, offset);
        break;
      case kLoadDoubleword:
        if (reg == base) {
          // This will clobber the base when loading the lower register. Since we have to load the
          // higher register as well, this will fail. Solution: reverse the order.
          Lw(static_cast<Register>(reg + 1), base, offset + kMipsWordSize);
          null_checker();
          Lw(reg, base, offset);
        } else {
          Lw(reg, base, offset);
          null_checker();
          Lw(static_cast<Register>(reg + 1), base, offset + kMipsWordSize);
        }
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    if (type != kLoadDoubleword) {
      null_checker();
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadSFromOffset(FRegister reg,
                       Register base,
                       int32_t offset,
                       ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ false, /* is_float */ true);
    Lwc1(reg, base, offset);
    null_checker();
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadDFromOffset(FRegister reg,
                       Register base,
                       int32_t offset,
                       ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ true, /* is_float */ true);
    if (IsAligned<kMipsDoublewordSize>(offset)) {
      Ldc1(reg, base, offset);
      null_checker();
    } else {
      if (Is32BitFPU()) {
        Lwc1(reg, base, offset);
        null_checker();
        Lwc1(static_cast<FRegister>(reg + 1), base, offset + kMipsWordSize);
      } else {
        // 64-bit FPU.
        Lwc1(reg, base, offset);
        null_checker();
        Lw(T8, base, offset + kMipsWordSize);
        Mthc1(T8, reg);
      }
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void LoadQFromOffset(FRegister reg,
                       Register base,
                       int32_t offset,
                       ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    int element_size_shift = -1;
    AdjustBaseOffsetAndElementSizeShift(base, offset, element_size_shift);
    switch (element_size_shift) {
      case TIMES_1: LdB(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_2: LdH(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_4: LdW(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_8: LdD(static_cast<VectorRegister>(reg), base, offset); break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    null_checker();
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreToOffset(StoreOperandType type,
                     Register reg,
                     Register base,
                     int32_t offset,
                     ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    // Must not use AT as `reg`, so as not to overwrite the value being stored
    // with the adjusted `base`.
    CHECK_NE(reg, AT);
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ (type == kStoreDoubleword));
    switch (type) {
      case kStoreByte:
        Sb(reg, base, offset);
        break;
      case kStoreHalfword:
        Sh(reg, base, offset);
        break;
      case kStoreWord:
        Sw(reg, base, offset);
        break;
      case kStoreDoubleword:
        CHECK_NE(reg, base);
        CHECK_NE(static_cast<Register>(reg + 1), base);
        Sw(reg, base, offset);
        null_checker();
        Sw(static_cast<Register>(reg + 1), base, offset + kMipsWordSize);
        break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    if (type != kStoreDoubleword) {
      null_checker();
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreSToOffset(FRegister reg,
                      Register base,
                      int32_t offset,
                      ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ false, /* is_float */ true);
    Swc1(reg, base, offset);
    null_checker();
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreDToOffset(FRegister reg,
                      Register base,
                      int32_t offset,
                      ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    AdjustBaseAndOffset(base, offset, /* is_doubleword */ true, /* is_float */ true);
    if (IsAligned<kMipsDoublewordSize>(offset)) {
      Sdc1(reg, base, offset);
      null_checker();
    } else {
      if (Is32BitFPU()) {
        Swc1(reg, base, offset);
        null_checker();
        Swc1(static_cast<FRegister>(reg + 1), base, offset + kMipsWordSize);
      } else {
        // 64-bit FPU.
        Mfhc1(T8, reg);
        Swc1(reg, base, offset);
        null_checker();
        Sw(T8, base, offset + kMipsWordSize);
      }
    }
  }

  template <typename ImplicitNullChecker = NoImplicitNullChecker>
  void StoreQToOffset(FRegister reg,
                      Register base,
                      int32_t offset,
                      ImplicitNullChecker null_checker = NoImplicitNullChecker()) {
    int element_size_shift = -1;
    AdjustBaseOffsetAndElementSizeShift(base, offset, element_size_shift);
    switch (element_size_shift) {
      case TIMES_1: StB(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_2: StH(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_4: StW(static_cast<VectorRegister>(reg), base, offset); break;
      case TIMES_8: StD(static_cast<VectorRegister>(reg), base, offset); break;
      default:
        LOG(FATAL) << "UNREACHABLE";
    }
    null_checker();
  }

  void LoadFromOffset(LoadOperandType type, Register reg, Register base, int32_t offset);
  void LoadSFromOffset(FRegister reg, Register base, int32_t offset);
  void LoadDFromOffset(FRegister reg, Register base, int32_t offset);
  void LoadQFromOffset(FRegister reg, Register base, int32_t offset);
  void StoreToOffset(StoreOperandType type, Register reg, Register base, int32_t offset);
  void StoreSToOffset(FRegister reg, Register base, int32_t offset);
  void StoreDToOffset(FRegister reg, Register base, int32_t offset);
  void StoreQToOffset(FRegister reg, Register base, int32_t offset);

  // Emit data (e.g. encoded instruction or immediate) to the instruction stream.
  void Emit(uint32_t value);

  // Push/pop composite routines.
  void Push(Register rs);
  void Pop(Register rd);
  void PopAndReturn(Register rd, Register rt);

  //
  // Heap poisoning.
  //

  // Poison a heap reference contained in `src` and store it in `dst`.
  void PoisonHeapReference(Register dst, Register src) {
    // dst = -src.
    Subu(dst, ZERO, src);
  }
  // Poison a heap reference contained in `reg`.
  void PoisonHeapReference(Register reg) {
    // reg = -reg.
    PoisonHeapReference(reg, reg);
  }
  // Unpoison a heap reference contained in `reg`.
  void UnpoisonHeapReference(Register reg) {
    // reg = -reg.
    Subu(reg, ZERO, reg);
  }
  // Poison a heap reference contained in `reg` if heap poisoning is enabled.
  void MaybePoisonHeapReference(Register reg) {
    if (kPoisonHeapReferences) {
      PoisonHeapReference(reg);
    }
  }
  // Unpoison a heap reference contained in `reg` if heap poisoning is enabled.
  void MaybeUnpoisonHeapReference(Register reg) {
    if (kPoisonHeapReferences) {
      UnpoisonHeapReference(reg);
    }
  }

  void Bind(Label* label) OVERRIDE {
    Bind(down_cast<MipsLabel*>(label));
  }
  void Jump(Label* label ATTRIBUTE_UNUSED) OVERRIDE {
    UNIMPLEMENTED(FATAL) << "Do not use Jump for MIPS";
  }

  // Don't warn about a different virtual Bind/Jump in the base class.
  using JNIBase::Bind;
  using JNIBase::Jump;

  // Create a new label that can be used with Jump/Bind calls.
  std::unique_ptr<JNIMacroLabel> CreateLabel() OVERRIDE {
    LOG(FATAL) << "Not implemented on MIPS32";
    UNREACHABLE();
  }
  // Emit an unconditional jump to the label.
  void Jump(JNIMacroLabel* label ATTRIBUTE_UNUSED) OVERRIDE {
    LOG(FATAL) << "Not implemented on MIPS32";
    UNREACHABLE();
  }
  // Emit a conditional jump to the label by applying a unary condition test to the register.
  void Jump(JNIMacroLabel* label ATTRIBUTE_UNUSED,
            JNIMacroUnaryCondition cond ATTRIBUTE_UNUSED,
            ManagedRegister test ATTRIBUTE_UNUSED) OVERRIDE {
    LOG(FATAL) << "Not implemented on MIPS32";
    UNREACHABLE();
  }

  // Code at this offset will serve as the target for the Jump call.
  void Bind(JNIMacroLabel* label ATTRIBUTE_UNUSED) OVERRIDE {
    LOG(FATAL) << "Not implemented on MIPS32";
    UNREACHABLE();
  }

  // Create a new literal with a given value.
  // NOTE: Force the template parameter to be explicitly specified.
  template <typename T>
  Literal* NewLiteral(typename Identity<T>::type value) {
    static_assert(std::is_integral<T>::value, "T must be an integral type.");
    return NewLiteral(sizeof(value), reinterpret_cast<const uint8_t*>(&value));
  }

  // Load label address using PC-relative addressing.
  // To be used with data labels in the literal / jump table area only and not
  // with regular code labels.
  //
  // For R6 base_reg must be ZERO.
  //
  // On R2 there are two possible uses w.r.t. base_reg:
  //
  // - base_reg = ZERO:
  //   The NAL instruction will be generated as part of the load and it will
  //   clobber the RA register.
  //
  // - base_reg != ZERO:
  //   The RA-clobbering NAL instruction won't be generated as part of the load.
  //   The label pc_rel_base_label_ must be bound (with BindPcRelBaseLabel())
  //   and base_reg must hold the address of the label. Example:
  //     __ Nal();
  //     __ Move(S3, RA);
  //     __ BindPcRelBaseLabel();  // S3 holds the address of pc_rel_base_label_.
  //     __ LoadLabelAddress(A0, S3, label1);
  //     __ LoadLabelAddress(A1, S3, label2);
  //     __ LoadLiteral(V0, S3, literal1);
  //     __ LoadLiteral(V1, S3, literal2);
  void LoadLabelAddress(Register dest_reg, Register base_reg, MipsLabel* label);

  // Create a new literal with the given data.
  Literal* NewLiteral(size_t size, const uint8_t* data);

  // Load literal using PC-relative addressing.
  // See the above comments for LoadLabelAddress() on the value of base_reg.
  void LoadLiteral(Register dest_reg, Register base_reg, Literal* literal);

  // Create a jump table for the given labels that will be emitted when finalizing.
  // When the table is emitted, offsets will be relative to the location of the table.
  // The table location is determined by the location of its label (the label precedes
  // the table data) and should be loaded using LoadLabelAddress().
  JumpTable* CreateJumpTable(std::vector<MipsLabel*>&& labels);

  //
  // Overridden common assembler high-level functionality.
  //

  // Emit code that will create an activation on the stack.
  void BuildFrame(size_t frame_size,
                  ManagedRegister method_reg,
                  ArrayRef<const ManagedRegister> callee_save_regs,
                  const ManagedRegisterEntrySpills& entry_spills) OVERRIDE;

  // Emit code that will remove an activation from the stack.
  void RemoveFrame(size_t frame_size,
                   ArrayRef<const ManagedRegister> callee_save_regs,
                   bool may_suspend) OVERRIDE;

  void IncreaseFrameSize(size_t adjust) OVERRIDE;
  void DecreaseFrameSize(size_t adjust) OVERRIDE;

  // Store routines.
  void Store(FrameOffset offs, ManagedRegister msrc, size_t size) OVERRIDE;
  void StoreRef(FrameOffset dest, ManagedRegister msrc) OVERRIDE;
  void StoreRawPtr(FrameOffset dest, ManagedRegister msrc) OVERRIDE;

  void StoreImmediateToFrame(FrameOffset dest, uint32_t imm, ManagedRegister mscratch) OVERRIDE;

  void StoreStackOffsetToThread(ThreadOffset32 thr_offs,
                                FrameOffset fr_offs,
                                ManagedRegister mscratch) OVERRIDE;

  void StoreStackPointerToThread(ThreadOffset32 thr_offs) OVERRIDE;

  void StoreSpanning(FrameOffset dest,
                     ManagedRegister msrc,
                     FrameOffset in_off,
                     ManagedRegister mscratch) OVERRIDE;

  // Load routines.
  void Load(ManagedRegister mdest, FrameOffset src, size_t size) OVERRIDE;

  void LoadFromThread(ManagedRegister mdest, ThreadOffset32 src, size_t size) OVERRIDE;

  void LoadRef(ManagedRegister dest, FrameOffset src) OVERRIDE;

  void LoadRef(ManagedRegister mdest,
               ManagedRegister base,
               MemberOffset offs,
               bool unpoison_reference) OVERRIDE;

  void LoadRawPtr(ManagedRegister mdest, ManagedRegister base, Offset offs) OVERRIDE;

  void LoadRawPtrFromThread(ManagedRegister mdest, ThreadOffset32 offs) OVERRIDE;

  // Copying routines.
  void Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) OVERRIDE;

  void CopyRawPtrFromThread(FrameOffset fr_offs,
                            ThreadOffset32 thr_offs,
                            ManagedRegister mscratch) OVERRIDE;

  void CopyRawPtrToThread(ThreadOffset32 thr_offs,
                          FrameOffset fr_offs,
                          ManagedRegister mscratch) OVERRIDE;

  void CopyRef(FrameOffset dest, FrameOffset src, ManagedRegister mscratch) OVERRIDE;

  void Copy(FrameOffset dest, FrameOffset src, ManagedRegister mscratch, size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            ManagedRegister src_base,
            Offset src_offset,
            ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest_base,
            Offset dest_offset,
            FrameOffset src,
            ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            FrameOffset src_base,
            Offset src_offset,
            ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(ManagedRegister dest,
            Offset dest_offset,
            ManagedRegister src,
            Offset src_offset,
            ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void Copy(FrameOffset dest,
            Offset dest_offset,
            FrameOffset src,
            Offset src_offset,
            ManagedRegister mscratch,
            size_t size) OVERRIDE;

  void MemoryBarrier(ManagedRegister) OVERRIDE;

  // Sign extension.
  void SignExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Zero extension.
  void ZeroExtend(ManagedRegister mreg, size_t size) OVERRIDE;

  // Exploit fast access in managed code to Thread::Current().
  void GetCurrentThread(ManagedRegister tr) OVERRIDE;
  void GetCurrentThread(FrameOffset dest_offset, ManagedRegister mscratch) OVERRIDE;

  // Set up out_reg to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed. in_reg holds a possibly stale reference
  // that can be used to avoid loading the handle scope entry to see if the value is
  // null.
  void CreateHandleScopeEntry(ManagedRegister out_reg,
                              FrameOffset handlescope_offset,
                              ManagedRegister in_reg,
                              bool null_allowed) OVERRIDE;

  // Set up out_off to hold a Object** into the handle scope, or to be null if the
  // value is null and null_allowed.
  void CreateHandleScopeEntry(FrameOffset out_off,
                              FrameOffset handlescope_offset,
                              ManagedRegister mscratch,
                              bool null_allowed) OVERRIDE;

  // src holds a handle scope entry (Object**) load this into dst.
  void LoadReferenceFromHandleScope(ManagedRegister dst, ManagedRegister src) OVERRIDE;

  // Heap::VerifyObject on src. In some cases (such as a reference to this) we
  // know that src may not be null.
  void VerifyObject(ManagedRegister src, bool could_be_null) OVERRIDE;
  void VerifyObject(FrameOffset src, bool could_be_null) OVERRIDE;

  // Call to address held at [base+offset].
  void Call(ManagedRegister base, Offset offset, ManagedRegister mscratch) OVERRIDE;
  void Call(FrameOffset base, Offset offset, ManagedRegister mscratch) OVERRIDE;
  void CallFromThread(ThreadOffset32 offset, ManagedRegister mscratch) OVERRIDE;

  // Generate code to check if Thread::Current()->exception_ is non-null
  // and branch to a ExceptionSlowPath if it is.
  void ExceptionPoll(ManagedRegister mscratch, size_t stack_adjust) OVERRIDE;

  // Emit slow paths queued during assembly and promote short branches to long if needed.
  void FinalizeCode() OVERRIDE;

  // Emit branches and finalize all instructions.
  void FinalizeInstructions(const MemoryRegion& region);

  // Returns the (always-)current location of a label (can be used in class CodeGeneratorMIPS,
  // must be used instead of MipsLabel::GetPosition()).
  uint32_t GetLabelLocation(const MipsLabel* label) const;

  // Get the final position of a label after local fixup based on the old position
  // recorded before FinalizeCode().
  uint32_t GetAdjustedPosition(uint32_t old_position);

  // R2 doesn't have PC-relative addressing, which we need to access literals. We simulate it by
  // reading the PC value into a general-purpose register with the NAL instruction and then loading
  // literals through this base register. The code generator calls this method (at most once per
  // method being compiled) to bind a label to the location for which the PC value is acquired.
  // The assembler then computes literal offsets relative to this label.
  void BindPcRelBaseLabel();

  // Returns the location of the label bound with BindPcRelBaseLabel().
  uint32_t GetPcRelBaseLabelLocation() const;

  // Note that PC-relative literal loads are handled as pseudo branches because they need very
  // similar relocation and may similarly expand in size to accomodate for larger offsets relative
  // to PC.
  enum BranchCondition {
    kCondLT,
    kCondGE,
    kCondLE,
    kCondGT,
    kCondLTZ,
    kCondGEZ,
    kCondLEZ,
    kCondGTZ,
    kCondEQ,
    kCondNE,
    kCondEQZ,
    kCondNEZ,
    kCondLTU,
    kCondGEU,
    kCondF,    // Floating-point predicate false.
    kCondT,    // Floating-point predicate true.
    kUncond,
  };
  friend std::ostream& operator<<(std::ostream& os, const BranchCondition& rhs);

  // Enables or disables instruction reordering (IOW, automatic filling of delay slots)
  // similarly to ".set reorder" / ".set noreorder" in traditional MIPS assembly.
  // Returns the last state, which may be useful for temporary enabling/disabling of
  // reordering.
  bool SetReorder(bool enable);

 private:
  // Description of the last instruction in terms of input and output registers.
  // Used to make the decision of moving the instruction into a delay slot.
  struct DelaySlot {
    DelaySlot();

    // Encoded instruction that may be used to fill the delay slot or 0
    // (0 conveniently represents NOP).
    uint32_t instruction_;

    // Input/output register masks.
    InOutRegMasks masks_;

    // Label for patchable instructions to allow moving them into delay slots.
    MipsLabel* patcher_label_;
  };

  // Delay slot finite state machine's (DS FSM's) state. The FSM state is updated
  // upon every new instruction and label generated. The FSM detects instructions
  // suitable for delay slots and immediately preceded with labels. These are target
  // instructions for branches. If an unconditional R2 branch does not get its delay
  // slot filled with the immediately preceding instruction, it may instead get the
  // slot filled with the target instruction (the branch will need its offset
  // incremented past the target instruction). We call this "absorption". The FSM
  // records PCs of the target instructions suitable for this optimization.
  enum DsFsmState {
    kExpectingLabel,
    kExpectingInstruction,
    kExpectingCommit
  };
  friend std::ostream& operator<<(std::ostream& os, const DsFsmState& rhs);

  class Branch {
   public:
    enum Type {
      // R2 short branches (can be promoted to long).
      kUncondBranch,
      kCondBranch,
      kCall,
      // R2 short branches (can't be promoted to long), delay slots filled manually.
      kBareUncondBranch,
      kBareCondBranch,
      kBareCall,
      // R2 near label.
      kLabel,
      // R2 near literal.
      kLiteral,
      // R2 long branches.
      kLongUncondBranch,
      kLongCondBranch,
      kLongCall,
      // R2 far label.
      kFarLabel,
      // R2 far literal.
      kFarLiteral,
      // R6 short branches (can be promoted to long).
      kR6UncondBranch,
      kR6CondBranch,
      kR6Call,
      // R6 short branches (can't be promoted to long), forbidden/delay slots filled manually.
      kR6BareUncondBranch,
      kR6BareCondBranch,
      kR6BareCall,
      // R6 near label.
      kR6Label,
      // R6 near literal.
      kR6Literal,
      // R6 long branches.
      kR6LongUncondBranch,
      kR6LongCondBranch,
      kR6LongCall,
      // R6 far label.
      kR6FarLabel,
      // R6 far literal.
      kR6FarLiteral,
    };
    // Bit sizes of offsets defined as enums to minimize chance of typos.
    enum OffsetBits {
      kOffset16 = 16,
      kOffset18 = 18,
      kOffset21 = 21,
      kOffset23 = 23,
      kOffset28 = 28,
      kOffset32 = 32,
    };

    static constexpr uint32_t kUnresolved = 0xffffffff;  // Unresolved target_
    static constexpr int32_t kMaxBranchLength = 32;
    static constexpr int32_t kMaxBranchSize = kMaxBranchLength * sizeof(uint32_t);
    // The following two instruction encodings can never legally occur in branch delay
    // slots and are used as markers.
    //
    // kUnfilledDelaySlot means that the branch may use either the preceding or the target
    // instruction to fill its delay slot (the latter is only possible with unconditional
    // R2 branches and is termed here as "absorption").
    static constexpr uint32_t kUnfilledDelaySlot = 0x10000000;  // beq zero, zero, 0.
    // kUnfillableDelaySlot means that the branch cannot use an instruction (other than NOP)
    // to fill its delay slot. This is only used for unconditional R2 branches to prevent
    // absorption of the target instruction when reordering is disabled.
    static constexpr uint32_t kUnfillableDelaySlot = 0x13FF0000;  // beq ra, ra, 0.

    struct BranchInfo {
      // Branch length as a number of 4-byte-long instructions.
      uint32_t length;
      // Ordinal number (0-based) of the first (or the only) instruction that contains the branch's
      // PC-relative offset (or its most significant 16-bit half, which goes first).
      uint32_t instr_offset;
      // Different MIPS instructions with PC-relative offsets apply said offsets to slightly
      // different origins, e.g. to PC or PC+4. Encode the origin distance (as a number of 4-byte
      // instructions) from the instruction containing the offset.
      uint32_t pc_org;
      // How large (in bits) a PC-relative offset can be for a given type of branch (kR6CondBranch
      // and kR6BareCondBranch are an exception: use kOffset23 for beqzc/bnezc).
      OffsetBits offset_size;
      // Some MIPS instructions with PC-relative offsets shift the offset by 2. Encode the shift
      // count.
      int offset_shift;
    };
    static const BranchInfo branch_info_[/* Type */];

    // Unconditional branch or call.
    Branch(bool is_r6, uint32_t location, uint32_t target, bool is_call, bool is_bare);
    // Conditional branch.
    Branch(bool is_r6,
           uint32_t location,
           uint32_t target,
           BranchCondition condition,
           Register lhs_reg,
           Register rhs_reg,
           bool is_bare);
    // Label address (in literal area) or literal.
    Branch(bool is_r6,
           uint32_t location,
           Register dest_reg,
           Register base_reg,
           Type label_or_literal_type);

    // Some conditional branches with lhs = rhs are effectively NOPs, while some
    // others are effectively unconditional. MIPSR6 conditional branches require lhs != rhs.
    // So, we need a way to identify such branches in order to emit no instructions for them
    // or change them to unconditional.
    static bool IsNop(BranchCondition condition, Register lhs, Register rhs);
    static bool IsUncond(BranchCondition condition, Register lhs, Register rhs);

    static BranchCondition OppositeCondition(BranchCondition cond);

    Type GetType() const;
    BranchCondition GetCondition() const;
    Register GetLeftRegister() const;
    Register GetRightRegister() const;
    uint32_t GetTarget() const;
    uint32_t GetLocation() const;
    uint32_t GetOldLocation() const;
    uint32_t GetPrecedingInstructionLength(Type type) const;
    uint32_t GetPrecedingInstructionSize(Type type) const;
    uint32_t GetLength() const;
    uint32_t GetOldLength() const;
    uint32_t GetSize() const;
    uint32_t GetOldSize() const;
    uint32_t GetEndLocation() const;
    uint32_t GetOldEndLocation() const;
    bool IsBare() const;
    bool IsLong() const;
    bool IsResolved() const;

    // Various helpers for branch delay slot management.
    bool CanHaveDelayedInstruction(const DelaySlot& delay_slot) const;
    void SetDelayedInstruction(uint32_t instruction, MipsLabel* patcher_label = nullptr);
    uint32_t GetDelayedInstruction() const;
    MipsLabel* GetPatcherLabel() const;
    void DecrementLocations();

    // Returns the bit size of the signed offset that the branch instruction can handle.
    OffsetBits GetOffsetSize() const;

    // Calculates the distance between two byte locations in the assembler buffer and
    // returns the number of bits needed to represent the distance as a signed integer.
    //
    // Branch instructions have signed offsets of 16, 19 (addiupc), 21 (beqzc/bnezc),
    // and 26 (bc) bits, which are additionally shifted left 2 positions at run time.
    //
    // Composite branches (made of several instructions) with longer reach have 32-bit
    // offsets encoded as 2 16-bit "halves" in two instructions (high half goes first).
    // The composite branches cover the range of PC + +/-2GB on MIPS32 CPUs. However,
    // the range is not end-to-end on MIPS64 (unless addresses are forced to zero- or
    // sign-extend from 32 to 64 bits by the appropriate CPU configuration).
    // Consider the following implementation of a long unconditional branch, for
    // example:
    //
    //   auipc at, offset_31_16  // at = pc + sign_extend(offset_31_16) << 16
    //   jic   at, offset_15_0   // pc = at + sign_extend(offset_15_0)
    //
    // Both of the above instructions take 16-bit signed offsets as immediate operands.
    // When bit 15 of offset_15_0 is 1, it effectively causes subtraction of 0x10000
    // due to sign extension. This must be compensated for by incrementing offset_31_16
    // by 1. offset_31_16 can only be incremented by 1 if it's not 0x7FFF. If it is
    // 0x7FFF, adding 1 will overflow the positive offset into the negative range.
    // Therefore, the long branch range is something like from PC - 0x80000000 to
    // PC + 0x7FFF7FFF, IOW, shorter by 32KB on one side.
    //
    // The returned values are therefore: 18, 21, 23, 28 and 32. There's also a special
    // case with the addiu instruction and a 16 bit offset.
    static OffsetBits GetOffsetSizeNeeded(uint32_t location, uint32_t target);

    // Resolve a branch when the target is known.
    void Resolve(uint32_t target);

    // Relocate a branch by a given delta if needed due to expansion of this or another
    // branch at a given location by this delta (just changes location_ and target_).
    void Relocate(uint32_t expand_location, uint32_t delta);

    // If the branch is short, changes its type to long.
    void PromoteToLong();

    // If necessary, updates the type by promoting a short branch to a long branch
    // based on the branch location and target. Returns the amount (in bytes) by
    // which the branch size has increased.
    // max_short_distance caps the maximum distance between location_ and target_
    // that is allowed for short branches. This is for debugging/testing purposes.
    // max_short_distance = 0 forces all short branches to become long.
    // Use the implicit default argument when not debugging/testing.
    uint32_t PromoteIfNeeded(uint32_t location,
                             uint32_t max_short_distance = std::numeric_limits<uint32_t>::max());

    // Returns the location of the instruction(s) containing the offset.
    uint32_t GetOffsetLocation() const;

    // Calculates and returns the offset ready for encoding in the branch instruction(s).
    uint32_t GetOffset(uint32_t location) const;

   private:
    // Completes branch construction by determining and recording its type.
    void InitializeType(Type initial_type, bool is_r6);
    // Helper for the above.
    void InitShortOrLong(OffsetBits ofs_size, Type short_type, Type long_type);

    uint32_t old_location_;         // Offset into assembler buffer in bytes.
    uint32_t location_;             // Offset into assembler buffer in bytes.
    uint32_t target_;               // Offset into assembler buffer in bytes.

    uint32_t lhs_reg_;              // Left-hand side register in conditional branches or
                                    // FPU condition code. Destination register in literals.
    uint32_t rhs_reg_;              // Right-hand side register in conditional branches.
                                    // Base register in literals (ZERO on R6).
    BranchCondition condition_;     // Condition for conditional branches.

    Type type_;                     // Current type of the branch.
    Type old_type_;                 // Initial type of the branch.

    uint32_t delayed_instruction_;  // Encoded instruction for the delay slot or
                                    // kUnfilledDelaySlot if none but fillable or
                                    // kUnfillableDelaySlot if none and unfillable
                                    // (the latter is only used for unconditional R2
                                    // branches).

    MipsLabel* patcher_label_;      // Patcher label for the instruction in the delay slot.
  };
  friend std::ostream& operator<<(std::ostream& os, const Branch::Type& rhs);
  friend std::ostream& operator<<(std::ostream& os, const Branch::OffsetBits& rhs);

  uint32_t EmitR(int opcode, Register rs, Register rt, Register rd, int shamt, int funct);
  uint32_t EmitI(int opcode, Register rs, Register rt, uint16_t imm);
  uint32_t EmitI21(int opcode, Register rs, uint32_t imm21);
  uint32_t EmitI26(int opcode, uint32_t imm26);
  uint32_t EmitFR(int opcode, int fmt, FRegister ft, FRegister fs, FRegister fd, int funct);
  uint32_t EmitFI(int opcode, int fmt, FRegister rt, uint16_t imm);
  void EmitBcondR2(BranchCondition cond, Register rs, Register rt, uint16_t imm16);
  void EmitBcondR6(BranchCondition cond, Register rs, Register rt, uint32_t imm16_21);
  uint32_t EmitMsa3R(int operation,
                     int df,
                     VectorRegister wt,
                     VectorRegister ws,
                     VectorRegister wd,
                     int minor_opcode);
  uint32_t EmitMsaBIT(int operation,
                      int df_m,
                      VectorRegister ws,
                      VectorRegister wd,
                      int minor_opcode);
  uint32_t EmitMsaELM(int operation,
                      int df_n,
                      VectorRegister ws,
                      VectorRegister wd,
                      int minor_opcode);
  uint32_t EmitMsaMI10(int s10, Register rs, VectorRegister wd, int minor_opcode, int df);
  uint32_t EmitMsaI10(int operation, int df, int i10, VectorRegister wd, int minor_opcode);
  uint32_t EmitMsa2R(int operation, int df, VectorRegister ws, VectorRegister wd, int minor_opcode);
  uint32_t EmitMsa2RF(int operation,
                      int df,
                      VectorRegister ws,
                      VectorRegister wd,
                      int minor_opcode);

  void Buncond(MipsLabel* label, bool is_r6, bool is_bare);
  void Bcond(MipsLabel* label,
             bool is_r6,
             bool is_bare,
             BranchCondition condition,
             Register lhs,
             Register rhs = ZERO);
  void Call(MipsLabel* label, bool is_r6, bool is_bare);
  void FinalizeLabeledBranch(MipsLabel* label);

  // Various helpers for branch delay slot management.
  InOutRegMasks& DsFsmInstr(uint32_t instruction, MipsLabel* patcher_label = nullptr);
  void DsFsmInstrNop(uint32_t instruction);
  void DsFsmLabel();
  void DsFsmCommitLabel();
  void DsFsmDropLabel();
  void MoveInstructionToDelaySlot(Branch& branch);
  bool CanExchangeWithSlt(Register rs, Register rt) const;
  void ExchangeWithSlt(const DelaySlot& forwarded_slot);
  void GenerateSltForCondBranch(bool unsigned_slt, Register rs, Register rt);

  Branch* GetBranch(uint32_t branch_id);
  const Branch* GetBranch(uint32_t branch_id) const;
  uint32_t GetBranchLocationOrPcRelBase(const MipsAssembler::Branch* branch) const;
  uint32_t GetBranchOrPcRelBaseForEncoding(const MipsAssembler::Branch* branch) const;
  void BindRelativeToPrecedingBranch(MipsLabel* label,
                                     uint32_t prev_branch_id_plus_one,
                                     uint32_t position);

  void EmitLiterals();
  void ReserveJumpTableSpace();
  void EmitJumpTables();
  void PromoteBranches();
  void EmitBranch(uint32_t branch_id);
  void EmitBranches();
  void PatchCFI(size_t number_of_delayed_adjust_pcs);

  // Emits exception block.
  void EmitExceptionPoll(MipsExceptionSlowPath* exception);

  bool HasMsa() const {
    return has_msa_;
  }

  bool IsR6() const {
    if (isa_features_ != nullptr) {
      return isa_features_->IsR6();
    } else {
      return false;
    }
  }

  bool Is32BitFPU() const {
    if (isa_features_ != nullptr) {
      return isa_features_->Is32BitFloatingPoint();
    } else {
      return true;
    }
  }

  // List of exception blocks to generate at the end of the code cache.
  std::vector<MipsExceptionSlowPath> exception_blocks_;

  std::vector<Branch> branches_;

  // Whether appending instructions at the end of the buffer or overwriting the existing ones.
  bool overwriting_;
  // The current overwrite location.
  uint32_t overwrite_location_;

  // Whether instruction reordering (IOW, automatic filling of delay slots) is enabled.
  bool reordering_;
  // Information about the last instruction that may be used to fill a branch delay slot.
  DelaySlot delay_slot_;
  // Delay slot FSM state.
  DsFsmState ds_fsm_state_;
  // PC of the current labeled target instruction.
  uint32_t ds_fsm_target_pc_;
  // PCs of labeled target instructions.
  std::vector<uint32_t> ds_fsm_target_pcs_;

  // Use std::deque<> for literal labels to allow insertions at the end
  // without invalidating pointers and references to existing elements.
  ArenaDeque<Literal> literals_;

  // Jump table list.
  ArenaDeque<JumpTable> jump_tables_;

  // There's no PC-relative addressing on MIPS32R2. So, in order to access literals relative to PC
  // we get PC using the NAL instruction. This label marks the position within the assembler buffer
  // that PC (from NAL) points to.
  MipsLabel pc_rel_base_label_;

  // Data for GetAdjustedPosition(), see the description there.
  uint32_t last_position_adjustment_;
  uint32_t last_old_position_;
  uint32_t last_branch_id_;

  const bool has_msa_;

  const MipsInstructionSetFeatures* isa_features_;

  DISALLOW_COPY_AND_ASSIGN(MipsAssembler);
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_UTILS_MIPS_ASSEMBLER_MIPS_H_
