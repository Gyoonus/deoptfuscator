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

#include "assembler_mips.h"

#include <map>

#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define __ GetAssembler()->

namespace art {

struct MIPSCpuRegisterCompare {
  bool operator()(const mips::Register& a, const mips::Register& b) const {
    return a < b;
  }
};

class AssemblerMIPS32r6Test : public AssemblerTest<mips::MipsAssembler,
                                                   mips::MipsLabel,
                                                   mips::Register,
                                                   mips::FRegister,
                                                   uint32_t,
                                                   mips::VectorRegister> {
 public:
  typedef AssemblerTest<mips::MipsAssembler,
                        mips::MipsLabel,
                        mips::Register,
                        mips::FRegister,
                        uint32_t,
                        mips::VectorRegister> Base;

  AssemblerMIPS32r6Test() :
    instruction_set_features_(MipsInstructionSetFeatures::FromVariant("mips32r6", nullptr)) {
  }

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips";
  }

  std::string GetAssemblerCmdName() OVERRIDE {
    // We assemble and link for MIPS32R6. See GetAssemblerParameters() for details.
    return "gcc";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    // We assemble and link for MIPS32R6. The reason is that object files produced for MIPS32R6
    // (and MIPS64R6) with the GNU assembler don't have correct final offsets in PC-relative
    // branches in the .text section and so they require a relocation pass (there's a relocation
    // section, .rela.text, that has the needed info to fix up the branches).
    // We use "-modd-spreg" so we can use odd-numbered single precision FPU registers.
    // We put the code at address 0x1000000 (instead of 0) to avoid overlapping with the
    // .MIPS.abiflags section (there doesn't seem to be a way to suppress its generation easily).
    return " -march=mips32r6 -mmsa -modd-spreg -Wa,--no-warn"
        " -Wl,-Ttext=0x1000000 -Wl,-e0x1000000 -nostdlib";
  }

  void Pad(std::vector<uint8_t>& data) OVERRIDE {
    // The GNU linker unconditionally pads the code segment with NOPs to a size that is a multiple
    // of 16 and there doesn't appear to be a way to suppress this padding. Our assembler doesn't
    // pad, so, in order for two assembler outputs to match, we need to match the padding as well.
    // NOP is encoded as four zero bytes on MIPS.
    size_t pad_size = RoundUp(data.size(), 16u) - data.size();
    data.insert(data.end(), pad_size, 0);
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mmips:isa32r6";
  }

  mips::MipsAssembler* CreateAssembler(ArenaAllocator* allocator) OVERRIDE {
    return new (allocator) mips::MipsAssembler(allocator, instruction_set_features_.get());
  }

  void SetUpHelpers() OVERRIDE {
    if (registers_.size() == 0) {
      registers_.push_back(new mips::Register(mips::ZERO));
      registers_.push_back(new mips::Register(mips::AT));
      registers_.push_back(new mips::Register(mips::V0));
      registers_.push_back(new mips::Register(mips::V1));
      registers_.push_back(new mips::Register(mips::A0));
      registers_.push_back(new mips::Register(mips::A1));
      registers_.push_back(new mips::Register(mips::A2));
      registers_.push_back(new mips::Register(mips::A3));
      registers_.push_back(new mips::Register(mips::T0));
      registers_.push_back(new mips::Register(mips::T1));
      registers_.push_back(new mips::Register(mips::T2));
      registers_.push_back(new mips::Register(mips::T3));
      registers_.push_back(new mips::Register(mips::T4));
      registers_.push_back(new mips::Register(mips::T5));
      registers_.push_back(new mips::Register(mips::T6));
      registers_.push_back(new mips::Register(mips::T7));
      registers_.push_back(new mips::Register(mips::S0));
      registers_.push_back(new mips::Register(mips::S1));
      registers_.push_back(new mips::Register(mips::S2));
      registers_.push_back(new mips::Register(mips::S3));
      registers_.push_back(new mips::Register(mips::S4));
      registers_.push_back(new mips::Register(mips::S5));
      registers_.push_back(new mips::Register(mips::S6));
      registers_.push_back(new mips::Register(mips::S7));
      registers_.push_back(new mips::Register(mips::T8));
      registers_.push_back(new mips::Register(mips::T9));
      registers_.push_back(new mips::Register(mips::K0));
      registers_.push_back(new mips::Register(mips::K1));
      registers_.push_back(new mips::Register(mips::GP));
      registers_.push_back(new mips::Register(mips::SP));
      registers_.push_back(new mips::Register(mips::FP));
      registers_.push_back(new mips::Register(mips::RA));

      secondary_register_names_.emplace(mips::Register(mips::ZERO), "zero");
      secondary_register_names_.emplace(mips::Register(mips::AT), "at");
      secondary_register_names_.emplace(mips::Register(mips::V0), "v0");
      secondary_register_names_.emplace(mips::Register(mips::V1), "v1");
      secondary_register_names_.emplace(mips::Register(mips::A0), "a0");
      secondary_register_names_.emplace(mips::Register(mips::A1), "a1");
      secondary_register_names_.emplace(mips::Register(mips::A2), "a2");
      secondary_register_names_.emplace(mips::Register(mips::A3), "a3");
      secondary_register_names_.emplace(mips::Register(mips::T0), "t0");
      secondary_register_names_.emplace(mips::Register(mips::T1), "t1");
      secondary_register_names_.emplace(mips::Register(mips::T2), "t2");
      secondary_register_names_.emplace(mips::Register(mips::T3), "t3");
      secondary_register_names_.emplace(mips::Register(mips::T4), "t4");
      secondary_register_names_.emplace(mips::Register(mips::T5), "t5");
      secondary_register_names_.emplace(mips::Register(mips::T6), "t6");
      secondary_register_names_.emplace(mips::Register(mips::T7), "t7");
      secondary_register_names_.emplace(mips::Register(mips::S0), "s0");
      secondary_register_names_.emplace(mips::Register(mips::S1), "s1");
      secondary_register_names_.emplace(mips::Register(mips::S2), "s2");
      secondary_register_names_.emplace(mips::Register(mips::S3), "s3");
      secondary_register_names_.emplace(mips::Register(mips::S4), "s4");
      secondary_register_names_.emplace(mips::Register(mips::S5), "s5");
      secondary_register_names_.emplace(mips::Register(mips::S6), "s6");
      secondary_register_names_.emplace(mips::Register(mips::S7), "s7");
      secondary_register_names_.emplace(mips::Register(mips::T8), "t8");
      secondary_register_names_.emplace(mips::Register(mips::T9), "t9");
      secondary_register_names_.emplace(mips::Register(mips::K0), "k0");
      secondary_register_names_.emplace(mips::Register(mips::K1), "k1");
      secondary_register_names_.emplace(mips::Register(mips::GP), "gp");
      secondary_register_names_.emplace(mips::Register(mips::SP), "sp");
      secondary_register_names_.emplace(mips::Register(mips::FP), "fp");
      secondary_register_names_.emplace(mips::Register(mips::RA), "ra");

      fp_registers_.push_back(new mips::FRegister(mips::F0));
      fp_registers_.push_back(new mips::FRegister(mips::F1));
      fp_registers_.push_back(new mips::FRegister(mips::F2));
      fp_registers_.push_back(new mips::FRegister(mips::F3));
      fp_registers_.push_back(new mips::FRegister(mips::F4));
      fp_registers_.push_back(new mips::FRegister(mips::F5));
      fp_registers_.push_back(new mips::FRegister(mips::F6));
      fp_registers_.push_back(new mips::FRegister(mips::F7));
      fp_registers_.push_back(new mips::FRegister(mips::F8));
      fp_registers_.push_back(new mips::FRegister(mips::F9));
      fp_registers_.push_back(new mips::FRegister(mips::F10));
      fp_registers_.push_back(new mips::FRegister(mips::F11));
      fp_registers_.push_back(new mips::FRegister(mips::F12));
      fp_registers_.push_back(new mips::FRegister(mips::F13));
      fp_registers_.push_back(new mips::FRegister(mips::F14));
      fp_registers_.push_back(new mips::FRegister(mips::F15));
      fp_registers_.push_back(new mips::FRegister(mips::F16));
      fp_registers_.push_back(new mips::FRegister(mips::F17));
      fp_registers_.push_back(new mips::FRegister(mips::F18));
      fp_registers_.push_back(new mips::FRegister(mips::F19));
      fp_registers_.push_back(new mips::FRegister(mips::F20));
      fp_registers_.push_back(new mips::FRegister(mips::F21));
      fp_registers_.push_back(new mips::FRegister(mips::F22));
      fp_registers_.push_back(new mips::FRegister(mips::F23));
      fp_registers_.push_back(new mips::FRegister(mips::F24));
      fp_registers_.push_back(new mips::FRegister(mips::F25));
      fp_registers_.push_back(new mips::FRegister(mips::F26));
      fp_registers_.push_back(new mips::FRegister(mips::F27));
      fp_registers_.push_back(new mips::FRegister(mips::F28));
      fp_registers_.push_back(new mips::FRegister(mips::F29));
      fp_registers_.push_back(new mips::FRegister(mips::F30));
      fp_registers_.push_back(new mips::FRegister(mips::F31));

      vec_registers_.push_back(new mips::VectorRegister(mips::W0));
      vec_registers_.push_back(new mips::VectorRegister(mips::W1));
      vec_registers_.push_back(new mips::VectorRegister(mips::W2));
      vec_registers_.push_back(new mips::VectorRegister(mips::W3));
      vec_registers_.push_back(new mips::VectorRegister(mips::W4));
      vec_registers_.push_back(new mips::VectorRegister(mips::W5));
      vec_registers_.push_back(new mips::VectorRegister(mips::W6));
      vec_registers_.push_back(new mips::VectorRegister(mips::W7));
      vec_registers_.push_back(new mips::VectorRegister(mips::W8));
      vec_registers_.push_back(new mips::VectorRegister(mips::W9));
      vec_registers_.push_back(new mips::VectorRegister(mips::W10));
      vec_registers_.push_back(new mips::VectorRegister(mips::W11));
      vec_registers_.push_back(new mips::VectorRegister(mips::W12));
      vec_registers_.push_back(new mips::VectorRegister(mips::W13));
      vec_registers_.push_back(new mips::VectorRegister(mips::W14));
      vec_registers_.push_back(new mips::VectorRegister(mips::W15));
      vec_registers_.push_back(new mips::VectorRegister(mips::W16));
      vec_registers_.push_back(new mips::VectorRegister(mips::W17));
      vec_registers_.push_back(new mips::VectorRegister(mips::W18));
      vec_registers_.push_back(new mips::VectorRegister(mips::W19));
      vec_registers_.push_back(new mips::VectorRegister(mips::W20));
      vec_registers_.push_back(new mips::VectorRegister(mips::W21));
      vec_registers_.push_back(new mips::VectorRegister(mips::W22));
      vec_registers_.push_back(new mips::VectorRegister(mips::W23));
      vec_registers_.push_back(new mips::VectorRegister(mips::W24));
      vec_registers_.push_back(new mips::VectorRegister(mips::W25));
      vec_registers_.push_back(new mips::VectorRegister(mips::W26));
      vec_registers_.push_back(new mips::VectorRegister(mips::W27));
      vec_registers_.push_back(new mips::VectorRegister(mips::W28));
      vec_registers_.push_back(new mips::VectorRegister(mips::W29));
      vec_registers_.push_back(new mips::VectorRegister(mips::W30));
      vec_registers_.push_back(new mips::VectorRegister(mips::W31));
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
    STLDeleteElements(&vec_registers_);
  }

  std::vector<mips::MipsLabel> GetAddresses() {
    UNIMPLEMENTED(FATAL) << "Feature not implemented yet";
    UNREACHABLE();
  }

  std::vector<mips::Register*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<mips::FRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  std::vector<mips::VectorRegister*> GetVectorRegisters() OVERRIDE {
    return vec_registers_;
  }

  uint32_t CreateImmediate(int64_t imm_value) OVERRIDE {
    return imm_value;
  }

  std::string GetSecondaryRegisterName(const mips::Register& reg) OVERRIDE {
    CHECK(secondary_register_names_.find(reg) != secondary_register_names_.end());
    return secondary_register_names_[reg];
  }

  std::string RepeatInsn(size_t count, const std::string& insn) {
    std::string result;
    for (; count != 0u; --count) {
      result += insn;
    }
    return result;
  }

  void BranchHelper(void (mips::MipsAssembler::*f)(mips::MipsLabel*,
                                                   bool),
                    const std::string& instr_name,
                    bool has_slot,
                    bool is_bare = false) {
    __ SetReorder(false);
    mips::MipsLabel label1, label2;
    (Base::GetAssembler()->*f)(&label1, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label1);
    (Base::GetAssembler()->*f)(&label2, is_bare);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label2);
    (Base::GetAssembler()->*f)(&label1, is_bare);
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " 1f\n" +
        ((is_bare || !has_slot) ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        instr_name + " 2f\n" +
        ((is_bare || !has_slot) ? "" : "nop\n") +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        "2:\n" +
        instr_name + " 1b\n" +
        ((is_bare || !has_slot) ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondOneRegHelper(void (mips::MipsAssembler::*f)(mips::Register,
                                                             mips::MipsLabel*,
                                                             bool),
                              const std::string& instr_name,
                              bool is_bare = false) {
    __ SetReorder(false);
    mips::MipsLabel label;
    (Base::GetAssembler()->*f)(mips::A0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    (Base::GetAssembler()->*f)(mips::A1, &label, is_bare);
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $a0, 1f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $a1, 1b\n" +
        (is_bare ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondTwoRegsHelper(void (mips::MipsAssembler::*f)(mips::Register,
                                                              mips::Register,
                                                              mips::MipsLabel*,
                                                              bool),
                               const std::string& instr_name,
                               bool is_bare = false) {
    __ SetReorder(false);
    mips::MipsLabel label;
    (Base::GetAssembler()->*f)(mips::A0, mips::A1, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    (Base::GetAssembler()->*f)(mips::A2, mips::A3, &label, is_bare);
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $a0, $a1, 1f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $a2, $a3, 1b\n" +
        (is_bare ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchFpuCondHelper(void (mips::MipsAssembler::*f)(mips::FRegister,
                                                          mips::MipsLabel*,
                                                          bool),
                           const std::string& instr_name,
                           bool is_bare = false) {
    __ SetReorder(false);
    mips::MipsLabel label;
    (Base::GetAssembler()->*f)(mips::F0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    (Base::GetAssembler()->*f)(mips::F30, &label, is_bare);
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $f0, 1f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $f30, 1b\n" +
        (is_bare ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<mips::Register*> registers_;
  std::map<mips::Register, std::string, MIPSCpuRegisterCompare> secondary_register_names_;

  std::vector<mips::FRegister*> fp_registers_;
  std::vector<mips::VectorRegister*> vec_registers_;
  std::unique_ptr<const MipsInstructionSetFeatures> instruction_set_features_;
};


TEST_F(AssemblerMIPS32r6Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

TEST_F(AssemblerMIPS32r6Test, MulR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MulR6, "mul ${reg1}, ${reg2}, ${reg3}"), "MulR6");
}

TEST_F(AssemblerMIPS32r6Test, MuhR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MuhR6, "muh ${reg1}, ${reg2}, ${reg3}"), "MuhR6");
}

TEST_F(AssemblerMIPS32r6Test, MuhuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MuhuR6, "muhu ${reg1}, ${reg2}, ${reg3}"), "MuhuR6");
}

TEST_F(AssemblerMIPS32r6Test, DivR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivR6, "div ${reg1}, ${reg2}, ${reg3}"), "DivR6");
}

TEST_F(AssemblerMIPS32r6Test, ModR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModR6, "mod ${reg1}, ${reg2}, ${reg3}"), "ModR6");
}

TEST_F(AssemblerMIPS32r6Test, DivuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivuR6, "divu ${reg1}, ${reg2}, ${reg3}"), "DivuR6");
}

TEST_F(AssemblerMIPS32r6Test, ModuR6) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModuR6, "modu ${reg1}, ${reg2}, ${reg3}"), "ModuR6");
}

//////////
// MISC //
//////////

TEST_F(AssemblerMIPS32r6Test, Aui) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Aui, 16, "aui ${reg1}, ${reg2}, {imm}"), "Aui");
}

TEST_F(AssemblerMIPS32r6Test, Auipc) {
  DriverStr(RepeatRIb(&mips::MipsAssembler::Auipc, 16, "auipc ${reg}, {imm}"), "Auipc");
}

TEST_F(AssemblerMIPS32r6Test, Lwpc) {
  // Lwpc() takes an unsigned 19-bit immediate, while the GNU assembler needs a signed offset,
  // hence the sign extension from bit 18 with `imm - ((imm & 0x40000) << 1)`.
  // The GNU assembler also wants the offset to be a multiple of 4, which it will shift right
  // by 2 positions when encoding, hence `<< 2` to compensate for that shift.
  // We capture the value of the immediate with `.set imm, {imm}` because the value is needed
  // twice for the sign extension, but `{imm}` is substituted only once.
  const char* code = ".set imm, {imm}\nlw ${reg}, ((imm - ((imm & 0x40000) << 1)) << 2)($pc)";
  DriverStr(RepeatRIb(&mips::MipsAssembler::Lwpc, 19, code), "Lwpc");
}

TEST_F(AssemblerMIPS32r6Test, Addiupc) {
  // The comment from the Lwpc() test applies to this Addiupc() test as well.
  const char* code = ".set imm, {imm}\naddiupc ${reg}, (imm - ((imm & 0x40000) << 1)) << 2";
  DriverStr(RepeatRIb(&mips::MipsAssembler::Addiupc, 19, code), "Addiupc");
}

TEST_F(AssemblerMIPS32r6Test, Bitswap) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Bitswap, "bitswap ${reg1}, ${reg2}"), "bitswap");
}

TEST_F(AssemblerMIPS32r6Test, Lsa) {
  DriverStr(RepeatRRRIb(&mips::MipsAssembler::Lsa,
                        2,
                        "lsa ${reg1}, ${reg2}, ${reg3}, {imm}",
                        1),
            "lsa");
}

TEST_F(AssemblerMIPS32r6Test, Seleqz) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Seleqz, "seleqz ${reg1}, ${reg2}, ${reg3}"), "seleqz");
}

TEST_F(AssemblerMIPS32r6Test, Selnez) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Selnez, "selnez ${reg1}, ${reg2}, ${reg3}"), "selnez");
}

TEST_F(AssemblerMIPS32r6Test, ClzR6) {
  DriverStr(RepeatRR(&mips::MipsAssembler::ClzR6, "clz ${reg1}, ${reg2}"), "clzR6");
}

TEST_F(AssemblerMIPS32r6Test, CloR6) {
  DriverStr(RepeatRR(&mips::MipsAssembler::CloR6, "clo ${reg1}, ${reg2}"), "cloR6");
}

////////////////////
// FLOATING POINT //
////////////////////

TEST_F(AssemblerMIPS32r6Test, SelS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelS, "sel.s ${reg1}, ${reg2}, ${reg3}"), "sel.s");
}

TEST_F(AssemblerMIPS32r6Test, SelD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelD, "sel.d ${reg1}, ${reg2}, ${reg3}"), "sel.d");
}

TEST_F(AssemblerMIPS32r6Test, SeleqzS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SeleqzS, "seleqz.s ${reg1}, ${reg2}, ${reg3}"),
            "seleqz.s");
}

TEST_F(AssemblerMIPS32r6Test, SeleqzD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SeleqzD, "seleqz.d ${reg1}, ${reg2}, ${reg3}"),
            "seleqz.d");
}

TEST_F(AssemblerMIPS32r6Test, SelnezS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelnezS, "selnez.s ${reg1}, ${reg2}, ${reg3}"),
            "selnez.s");
}

TEST_F(AssemblerMIPS32r6Test, SelnezD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SelnezD, "selnez.d ${reg1}, ${reg2}, ${reg3}"),
            "selnez.d");
}

TEST_F(AssemblerMIPS32r6Test, ClassS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::ClassS, "class.s ${reg1}, ${reg2}"), "class.s");
}

TEST_F(AssemblerMIPS32r6Test, ClassD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::ClassD, "class.d ${reg1}, ${reg2}"), "class.d");
}

TEST_F(AssemblerMIPS32r6Test, MinS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MinS, "min.s ${reg1}, ${reg2}, ${reg3}"), "min.s");
}

TEST_F(AssemblerMIPS32r6Test, MinD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MinD, "min.d ${reg1}, ${reg2}, ${reg3}"), "min.d");
}

TEST_F(AssemblerMIPS32r6Test, MaxS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MaxS, "max.s ${reg1}, ${reg2}, ${reg3}"), "max.s");
}

TEST_F(AssemblerMIPS32r6Test, MaxD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MaxD, "max.d ${reg1}, ${reg2}, ${reg3}"), "max.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUnS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUnS, "cmp.un.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpEqS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpEqS, "cmp.eq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUeqS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUeqS, "cmp.ueq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpLtS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLtS, "cmp.lt.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUltS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUltS, "cmp.ult.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpLeS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLeS, "cmp.le.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUleS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUleS, "cmp.ule.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpOrS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpOrS, "cmp.or.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUneS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUneS, "cmp.une.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpNeS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpNeS, "cmp.ne.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.s");
}

TEST_F(AssemblerMIPS32r6Test, CmpUnD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUnD, "cmp.un.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpEqD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpEqD, "cmp.eq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUeqD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUeqD, "cmp.ueq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpLtD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLtD, "cmp.lt.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUltD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUltD, "cmp.ult.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpLeD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpLeD, "cmp.le.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUleD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUleD, "cmp.ule.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpOrD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpOrD, "cmp.or.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpUneD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpUneD, "cmp.une.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.d");
}

TEST_F(AssemblerMIPS32r6Test, CmpNeD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::CmpNeD, "cmp.ne.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.d");
}

TEST_F(AssemblerMIPS32r6Test, LoadDFromOffset) {
  __ LoadDFromOffset(mips::F0, mips::A0, -0x8000);
  __ LoadDFromOffset(mips::F0, mips::A0, +0);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x7FF8);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x7FFB);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x7FFC);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x7FFF);
  __ LoadDFromOffset(mips::F0, mips::A0, -0xFFF0);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x8008);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x8001);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x8000);
  __ LoadDFromOffset(mips::F0, mips::A0, +0xFFF0);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x17FE8);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x0FFF8);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x0FFF1);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x0FFF1);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x0FFF8);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x17FE8);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x17FF0);
  __ LoadDFromOffset(mips::F0, mips::A0, -0x17FE9);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x17FE9);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x17FF0);
  __ LoadDFromOffset(mips::F0, mips::A0, +0x12345678);

  const char* expected =
      "ldc1 $f0, -0x8000($a0)\n"
      "ldc1 $f0, 0($a0)\n"
      "ldc1 $f0, 0x7FF8($a0)\n"
      "lwc1 $f0, 0x7FFB($a0)\n"
      "lw $t8, 0x7FFF($a0)\n"
      "mthc1 $t8, $f0\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f0, 4($at)\n"
      "lw $t8, 8($at)\n"
      "mthc1 $t8, $f0\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f0, 7($at)\n"
      "lw $t8, 11($at)\n"
      "mthc1 $t8, $f0\n"
      "addiu $at, $a0, -0x7FF8\n"
      "ldc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "ldc1 $f0, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "lwc1 $f0, -9($at)\n"
      "lw $t8, -5($at)\n"
      "mthc1 $t8, $f0\n"
      "addiu $at, $a0, 0x7FF8\n"
      "ldc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "ldc1 $f0, 0x7FF8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "ldc1 $f0, -0x7FE8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "ldc1 $f0, 0x8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "lwc1 $f0, 0xF($at)\n"
      "lw $t8, 0x13($at)\n"
      "mthc1 $t8, $f0\n"
      "aui $at, $a0, 0x1\n"
      "lwc1 $f0, -0xF($at)\n"
      "lw $t8, -0xB($at)\n"
      "mthc1 $t8, $f0\n"
      "aui $at, $a0, 0x1\n"
      "ldc1 $f0, -0x8($at)\n"
      "aui $at, $a0, 0x1\n"
      "ldc1 $f0, 0x7FE8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "ldc1 $f0, -0x7FF0($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "lwc1 $f0, -0x7FE9($at)\n"
      "lw $t8, -0x7FE5($at)\n"
      "mthc1 $t8, $f0\n"
      "aui $at, $a0, 0x1\n"
      "lwc1 $f0, 0x7FE9($at)\n"
      "lw $t8, 0x7FED($at)\n"
      "mthc1 $t8, $f0\n"
      "aui $at, $a0, 0x1\n"
      "ldc1 $f0, 0x7FF0($at)\n"
      "aui $at, $a0, 0x1234\n"
      "ldc1 $f0, 0x5678($at)\n";
  DriverStr(expected, "LoadDFromOffset");
}

TEST_F(AssemblerMIPS32r6Test, LoadQFromOffset) {
  __ LoadQFromOffset(mips::F0, mips::A0, 0);
  __ LoadQFromOffset(mips::F0, mips::A0, 1);
  __ LoadQFromOffset(mips::F0, mips::A0, 2);
  __ LoadQFromOffset(mips::F0, mips::A0, 4);
  __ LoadQFromOffset(mips::F0, mips::A0, 8);
  __ LoadQFromOffset(mips::F0, mips::A0, 511);
  __ LoadQFromOffset(mips::F0, mips::A0, 512);
  __ LoadQFromOffset(mips::F0, mips::A0, 513);
  __ LoadQFromOffset(mips::F0, mips::A0, 514);
  __ LoadQFromOffset(mips::F0, mips::A0, 516);
  __ LoadQFromOffset(mips::F0, mips::A0, 1022);
  __ LoadQFromOffset(mips::F0, mips::A0, 1024);
  __ LoadQFromOffset(mips::F0, mips::A0, 1025);
  __ LoadQFromOffset(mips::F0, mips::A0, 1026);
  __ LoadQFromOffset(mips::F0, mips::A0, 1028);
  __ LoadQFromOffset(mips::F0, mips::A0, 2044);
  __ LoadQFromOffset(mips::F0, mips::A0, 2048);
  __ LoadQFromOffset(mips::F0, mips::A0, 2049);
  __ LoadQFromOffset(mips::F0, mips::A0, 2050);
  __ LoadQFromOffset(mips::F0, mips::A0, 2052);
  __ LoadQFromOffset(mips::F0, mips::A0, 4088);
  __ LoadQFromOffset(mips::F0, mips::A0, 4096);
  __ LoadQFromOffset(mips::F0, mips::A0, 4097);
  __ LoadQFromOffset(mips::F0, mips::A0, 4098);
  __ LoadQFromOffset(mips::F0, mips::A0, 4100);
  __ LoadQFromOffset(mips::F0, mips::A0, 4104);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x7FFC);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x8000);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x10000);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x12345678);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x12350078);
  __ LoadQFromOffset(mips::F0, mips::A0, -256);
  __ LoadQFromOffset(mips::F0, mips::A0, -511);
  __ LoadQFromOffset(mips::F0, mips::A0, -513);
  __ LoadQFromOffset(mips::F0, mips::A0, -1022);
  __ LoadQFromOffset(mips::F0, mips::A0, -1026);
  __ LoadQFromOffset(mips::F0, mips::A0, -2044);
  __ LoadQFromOffset(mips::F0, mips::A0, -2052);
  __ LoadQFromOffset(mips::F0, mips::A0, -4096);
  __ LoadQFromOffset(mips::F0, mips::A0, -4104);
  __ LoadQFromOffset(mips::F0, mips::A0, -32768);
  __ LoadQFromOffset(mips::F0, mips::A0, 0xABCDEF00);
  __ LoadQFromOffset(mips::F0, mips::A0, 0x7FFFABCD);

  const char* expected =
      "ld.d $w0, 0($a0)\n"
      "ld.b $w0, 1($a0)\n"
      "ld.h $w0, 2($a0)\n"
      "ld.w $w0, 4($a0)\n"
      "ld.d $w0, 8($a0)\n"
      "ld.b $w0, 511($a0)\n"
      "ld.d $w0, 512($a0)\n"
      "addiu $at, $a0, 513\n"
      "ld.b $w0, 0($at)\n"
      "ld.h $w0, 514($a0)\n"
      "ld.w $w0, 516($a0)\n"
      "ld.h $w0, 1022($a0)\n"
      "ld.d $w0, 1024($a0)\n"
      "addiu $at, $a0, 1025\n"
      "ld.b $w0, 0($at)\n"
      "addiu $at, $a0, 1026\n"
      "ld.h $w0, 0($at)\n"
      "ld.w $w0, 1028($a0)\n"
      "ld.w $w0, 2044($a0)\n"
      "ld.d $w0, 2048($a0)\n"
      "addiu $at, $a0, 2049\n"
      "ld.b $w0, 0($at)\n"
      "addiu $at, $a0, 2050\n"
      "ld.h $w0, 0($at)\n"
      "addiu $at, $a0, 2052\n"
      "ld.w $w0, 0($at)\n"
      "ld.d $w0, 4088($a0)\n"
      "addiu $at, $a0, 4096\n"
      "ld.d $w0, 0($at)\n"
      "addiu $at, $a0, 4097\n"
      "ld.b $w0, 0($at)\n"
      "addiu $at, $a0, 4098\n"
      "ld.h $w0, 0($at)\n"
      "addiu $at, $a0, 4100\n"
      "ld.w $w0, 0($at)\n"
      "addiu $at, $a0, 4104\n"
      "ld.d $w0, 0($at)\n"
      "addiu $at, $a0, 0x7FFC\n"
      "ld.w $w0, 0($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "ld.d $w0, 8($at)\n"
      "aui $at, $a0, 0x1\n"
      "ld.d $w0, 0($at)\n"
      "aui $at, $a0, 0x1234\n"
      "addiu $at, $at, 0x6000\n"
      "ld.d $w0, -2440($at) # 0xF678\n"
      "aui $at, $a0, 0x1235\n"
      "ld.d $w0, 0x78($at)\n"
      "ld.d $w0, -256($a0)\n"
      "ld.b $w0, -511($a0)\n"
      "addiu $at, $a0, -513\n"
      "ld.b $w0, 0($at)\n"
      "ld.h $w0, -1022($a0)\n"
      "addiu $at, $a0, -1026\n"
      "ld.h $w0, 0($at)\n"
      "ld.w $w0, -2044($a0)\n"
      "addiu $at, $a0, -2052\n"
      "ld.w $w0, 0($at)\n"
      "ld.d $w0, -4096($a0)\n"
      "addiu $at, $a0, -4104\n"
      "ld.d $w0, 0($at)\n"
      "addiu $at, $a0, -32768\n"
      "ld.d $w0, 0($at)\n"
      "aui $at, $a0, 0xABCE\n"
      "addiu $at, $at, -8192 # 0xE000\n"
      "ld.d $w0, 0xF00($at)\n"
      "aui $at, $a0, 0x8000\n"
      "addiu $at, $at, -21504 # 0xAC00\n"
      "ld.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "LoadQFromOffset");
}

TEST_F(AssemblerMIPS32r6Test, StoreDToOffset) {
  __ StoreDToOffset(mips::F0, mips::A0, -0x8000);
  __ StoreDToOffset(mips::F0, mips::A0, +0);
  __ StoreDToOffset(mips::F0, mips::A0, +0x7FF8);
  __ StoreDToOffset(mips::F0, mips::A0, +0x7FFB);
  __ StoreDToOffset(mips::F0, mips::A0, +0x7FFC);
  __ StoreDToOffset(mips::F0, mips::A0, +0x7FFF);
  __ StoreDToOffset(mips::F0, mips::A0, -0xFFF0);
  __ StoreDToOffset(mips::F0, mips::A0, -0x8008);
  __ StoreDToOffset(mips::F0, mips::A0, -0x8001);
  __ StoreDToOffset(mips::F0, mips::A0, +0x8000);
  __ StoreDToOffset(mips::F0, mips::A0, +0xFFF0);
  __ StoreDToOffset(mips::F0, mips::A0, -0x17FE8);
  __ StoreDToOffset(mips::F0, mips::A0, -0x0FFF8);
  __ StoreDToOffset(mips::F0, mips::A0, -0x0FFF1);
  __ StoreDToOffset(mips::F0, mips::A0, +0x0FFF1);
  __ StoreDToOffset(mips::F0, mips::A0, +0x0FFF8);
  __ StoreDToOffset(mips::F0, mips::A0, +0x17FE8);
  __ StoreDToOffset(mips::F0, mips::A0, -0x17FF0);
  __ StoreDToOffset(mips::F0, mips::A0, -0x17FE9);
  __ StoreDToOffset(mips::F0, mips::A0, +0x17FE9);
  __ StoreDToOffset(mips::F0, mips::A0, +0x17FF0);
  __ StoreDToOffset(mips::F0, mips::A0, +0x12345678);

  const char* expected =
      "sdc1 $f0, -0x8000($a0)\n"
      "sdc1 $f0, 0($a0)\n"
      "sdc1 $f0, 0x7FF8($a0)\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 0x7FFB($a0)\n"
      "sw $t8, 0x7FFF($a0)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 4($at)\n"
      "sw $t8, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 7($at)\n"
      "sw $t8, 11($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "sdc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "sdc1 $f0, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, -9($at)\n"
      "sw $t8, -5($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "sdc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "sdc1 $f0, 0x7FF8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "sdc1 $f0, -0x7FE8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "sdc1 $f0, 0x8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 0xF($at)\n"
      "sw $t8, 0x13($at)\n"
      "aui $at, $a0, 0x1\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, -0xF($at)\n"
      "sw $t8, -0xB($at)\n"
      "aui $at, $a0, 0x1\n"
      "sdc1 $f0, -0x8($at)\n"
      "aui $at, $a0, 0x1\n"
      "sdc1 $f0, 0x7FE8($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "sdc1 $f0, -0x7FF0($at)\n"
      "aui $at, $a0, 0xFFFF\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, -0x7FE9($at)\n"
      "sw $t8, -0x7FE5($at)\n"
      "aui $at, $a0, 0x1\n"
      "mfhc1 $t8, $f0\n"
      "swc1 $f0, 0x7FE9($at)\n"
      "sw $t8, 0x7FED($at)\n"
      "aui $at, $a0, 0x1\n"
      "sdc1 $f0, 0x7FF0($at)\n"
      "aui $at, $a0, 0x1234\n"
      "sdc1 $f0, 0x5678($at)\n";
  DriverStr(expected, "StoreDToOffset");
}

TEST_F(AssemblerMIPS32r6Test, StoreQToOffset) {
  __ StoreQToOffset(mips::F0, mips::A0, 0);
  __ StoreQToOffset(mips::F0, mips::A0, 1);
  __ StoreQToOffset(mips::F0, mips::A0, 2);
  __ StoreQToOffset(mips::F0, mips::A0, 4);
  __ StoreQToOffset(mips::F0, mips::A0, 8);
  __ StoreQToOffset(mips::F0, mips::A0, 511);
  __ StoreQToOffset(mips::F0, mips::A0, 512);
  __ StoreQToOffset(mips::F0, mips::A0, 513);
  __ StoreQToOffset(mips::F0, mips::A0, 514);
  __ StoreQToOffset(mips::F0, mips::A0, 516);
  __ StoreQToOffset(mips::F0, mips::A0, 1022);
  __ StoreQToOffset(mips::F0, mips::A0, 1024);
  __ StoreQToOffset(mips::F0, mips::A0, 1025);
  __ StoreQToOffset(mips::F0, mips::A0, 1026);
  __ StoreQToOffset(mips::F0, mips::A0, 1028);
  __ StoreQToOffset(mips::F0, mips::A0, 2044);
  __ StoreQToOffset(mips::F0, mips::A0, 2048);
  __ StoreQToOffset(mips::F0, mips::A0, 2049);
  __ StoreQToOffset(mips::F0, mips::A0, 2050);
  __ StoreQToOffset(mips::F0, mips::A0, 2052);
  __ StoreQToOffset(mips::F0, mips::A0, 4088);
  __ StoreQToOffset(mips::F0, mips::A0, 4096);
  __ StoreQToOffset(mips::F0, mips::A0, 4097);
  __ StoreQToOffset(mips::F0, mips::A0, 4098);
  __ StoreQToOffset(mips::F0, mips::A0, 4100);
  __ StoreQToOffset(mips::F0, mips::A0, 4104);
  __ StoreQToOffset(mips::F0, mips::A0, 0x7FFC);
  __ StoreQToOffset(mips::F0, mips::A0, 0x8000);
  __ StoreQToOffset(mips::F0, mips::A0, 0x10000);
  __ StoreQToOffset(mips::F0, mips::A0, 0x12345678);
  __ StoreQToOffset(mips::F0, mips::A0, 0x12350078);
  __ StoreQToOffset(mips::F0, mips::A0, -256);
  __ StoreQToOffset(mips::F0, mips::A0, -511);
  __ StoreQToOffset(mips::F0, mips::A0, -513);
  __ StoreQToOffset(mips::F0, mips::A0, -1022);
  __ StoreQToOffset(mips::F0, mips::A0, -1026);
  __ StoreQToOffset(mips::F0, mips::A0, -2044);
  __ StoreQToOffset(mips::F0, mips::A0, -2052);
  __ StoreQToOffset(mips::F0, mips::A0, -4096);
  __ StoreQToOffset(mips::F0, mips::A0, -4104);
  __ StoreQToOffset(mips::F0, mips::A0, -32768);
  __ StoreQToOffset(mips::F0, mips::A0, 0xABCDEF00);
  __ StoreQToOffset(mips::F0, mips::A0, 0x7FFFABCD);

  const char* expected =
      "st.d $w0, 0($a0)\n"
      "st.b $w0, 1($a0)\n"
      "st.h $w0, 2($a0)\n"
      "st.w $w0, 4($a0)\n"
      "st.d $w0, 8($a0)\n"
      "st.b $w0, 511($a0)\n"
      "st.d $w0, 512($a0)\n"
      "addiu $at, $a0, 513\n"
      "st.b $w0, 0($at)\n"
      "st.h $w0, 514($a0)\n"
      "st.w $w0, 516($a0)\n"
      "st.h $w0, 1022($a0)\n"
      "st.d $w0, 1024($a0)\n"
      "addiu $at, $a0, 1025\n"
      "st.b $w0, 0($at)\n"
      "addiu $at, $a0, 1026\n"
      "st.h $w0, 0($at)\n"
      "st.w $w0, 1028($a0)\n"
      "st.w $w0, 2044($a0)\n"
      "st.d $w0, 2048($a0)\n"
      "addiu $at, $a0, 2049\n"
      "st.b $w0, 0($at)\n"
      "addiu $at, $a0, 2050\n"
      "st.h $w0, 0($at)\n"
      "addiu $at, $a0, 2052\n"
      "st.w $w0, 0($at)\n"
      "st.d $w0, 4088($a0)\n"
      "addiu $at, $a0, 4096\n"
      "st.d $w0, 0($at)\n"
      "addiu $at, $a0, 4097\n"
      "st.b $w0, 0($at)\n"
      "addiu $at, $a0, 4098\n"
      "st.h $w0, 0($at)\n"
      "addiu $at, $a0, 4100\n"
      "st.w $w0, 0($at)\n"
      "addiu $at, $a0, 4104\n"
      "st.d $w0, 0($at)\n"
      "addiu $at, $a0, 0x7FFC\n"
      "st.w $w0, 0($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "st.d $w0, 8($at)\n"
      "aui $at, $a0, 0x1\n"
      "st.d $w0, 0($at)\n"
      "aui $at, $a0, 0x1234\n"
      "addiu $at, $at, 0x6000\n"
      "st.d $w0, -2440($at) # 0xF678\n"
      "aui $at, $a0, 0x1235\n"
      "st.d $w0, 0x78($at)\n"
      "st.d $w0, -256($a0)\n"
      "st.b $w0, -511($a0)\n"
      "addiu $at, $a0, -513\n"
      "st.b $w0, 0($at)\n"
      "st.h $w0, -1022($a0)\n"
      "addiu $at, $a0, -1026\n"
      "st.h $w0, 0($at)\n"
      "st.w $w0, -2044($a0)\n"
      "addiu $at, $a0, -2052\n"
      "st.w $w0, 0($at)\n"
      "st.d $w0, -4096($a0)\n"
      "addiu $at, $a0, -4104\n"
      "st.d $w0, 0($at)\n"
      "addiu $at, $a0, -32768\n"
      "st.d $w0, 0($at)\n"
      "aui $at, $a0, 0xABCE\n"
      "addiu $at, $at, -8192 # 0xE000\n"
      "st.d $w0, 0xF00($at)\n"
      "aui $at, $a0, 0x8000\n"
      "addiu $at, $at, -21504 # 0xAC00\n"
      "st.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "StoreQToOffset");
}

//////////////
// BRANCHES //
//////////////

TEST_F(AssemblerMIPS32r6Test, Bc) {
  BranchHelper(&mips::MipsAssembler::Bc, "Bc", /* has_slot */ false);
}

TEST_F(AssemblerMIPS32r6Test, Balc) {
  BranchHelper(&mips::MipsAssembler::Balc, "Balc", /* has_slot */ false);
}

TEST_F(AssemblerMIPS32r6Test, Beqc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beqc, "Beqc");
}

TEST_F(AssemblerMIPS32r6Test, Bnec) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bnec, "Bnec");
}

TEST_F(AssemblerMIPS32r6Test, Beqzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqzc, "Beqzc");
}

TEST_F(AssemblerMIPS32r6Test, Bnezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnezc, "Bnezc");
}

TEST_F(AssemblerMIPS32r6Test, Bltzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltzc, "Bltzc");
}

TEST_F(AssemblerMIPS32r6Test, Bgezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgezc, "Bgezc");
}

TEST_F(AssemblerMIPS32r6Test, Blezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blezc, "Blezc");
}

TEST_F(AssemblerMIPS32r6Test, Bgtzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtzc, "Bgtzc");
}

TEST_F(AssemblerMIPS32r6Test, Bltc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltc, "Bltc");
}

TEST_F(AssemblerMIPS32r6Test, Bgec) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgec, "Bgec");
}

TEST_F(AssemblerMIPS32r6Test, Bltuc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltuc, "Bltuc");
}

TEST_F(AssemblerMIPS32r6Test, Bgeuc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeuc, "Bgeuc");
}

TEST_F(AssemblerMIPS32r6Test, Bc1eqz) {
  BranchFpuCondHelper(&mips::MipsAssembler::Bc1eqz, "Bc1eqz");
}

TEST_F(AssemblerMIPS32r6Test, Bc1nez) {
  BranchFpuCondHelper(&mips::MipsAssembler::Bc1nez, "Bc1nez");
}

TEST_F(AssemblerMIPS32r6Test, B) {
  BranchHelper(&mips::MipsAssembler::B, "Bc", /* has_slot */ false);
}

TEST_F(AssemblerMIPS32r6Test, Bal) {
  BranchHelper(&mips::MipsAssembler::Bal, "Balc", /* has_slot */ false);
}

TEST_F(AssemblerMIPS32r6Test, Beq) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beq, "Beqc");
}

TEST_F(AssemblerMIPS32r6Test, Bne) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bne, "Bnec");
}

TEST_F(AssemblerMIPS32r6Test, Beqz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqz, "Beqzc");
}

TEST_F(AssemblerMIPS32r6Test, Bnez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnez, "Bnezc");
}

TEST_F(AssemblerMIPS32r6Test, Bltz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltz, "Bltzc");
}

TEST_F(AssemblerMIPS32r6Test, Bgez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgez, "Bgezc");
}

TEST_F(AssemblerMIPS32r6Test, Blez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blez, "Blezc");
}

TEST_F(AssemblerMIPS32r6Test, Bgtz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtz, "Bgtzc");
}

TEST_F(AssemblerMIPS32r6Test, Blt) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Blt, "Bltc");
}

TEST_F(AssemblerMIPS32r6Test, Bge) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bge, "Bgec");
}

TEST_F(AssemblerMIPS32r6Test, Bltu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltu, "Bltuc");
}

TEST_F(AssemblerMIPS32r6Test, Bgeu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeu, "Bgeuc");
}

TEST_F(AssemblerMIPS32r6Test, BareBc) {
  BranchHelper(&mips::MipsAssembler::Bc, "Bc", /* has_slot */ false, /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBalc) {
  BranchHelper(&mips::MipsAssembler::Balc, "Balc", /* has_slot */ false, /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBeqc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beqc, "Beqc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBnec) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bnec, "Bnec", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBeqzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqzc, "Beqzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBnezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnezc, "Bnezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBltzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltzc, "Bltzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgezc, "Bgezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBlezc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blezc, "Blezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgtzc) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtzc, "Bgtzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBltc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltc, "Bltc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgec) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgec, "Bgec", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBltuc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltuc, "Bltuc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgeuc) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeuc, "Bgeuc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBc1eqz) {
  BranchFpuCondHelper(&mips::MipsAssembler::Bc1eqz, "Bc1eqz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBc1nez) {
  BranchFpuCondHelper(&mips::MipsAssembler::Bc1nez, "Bc1nez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareB) {
  BranchHelper(&mips::MipsAssembler::B, "B", /* has_slot */ true, /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBal) {
  BranchHelper(&mips::MipsAssembler::Bal, "Bal", /* has_slot */ true, /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBeq) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beq, "Beq", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBne) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bne, "Bne", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBeqz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqz, "Beqz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBnez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnez, "Bnez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBltz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltz, "Bltz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgez, "Bgez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBlez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blez, "Blez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgtz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtz, "Bgtz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBlt) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Blt, "Blt", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBge) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bge, "Bge", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBltu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltu, "Bltu", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, BareBgeu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeu, "Bgeu", /* is_bare */ true);
}

TEST_F(AssemblerMIPS32r6Test, LongBeqc) {
  mips::MipsLabel label;
  __ Beqc(mips::A0, mips::A1, &label);
  constexpr uint32_t kAdduCount1 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Beqc(mips::A2, mips::A3, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jic.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_back = -(kAdduCount2 + 1);  // 1: account for bnec.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jic.

  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "bnec $a0, $a1, 1f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward) << "\n"
      "1:\n" <<
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") <<
      "2:\n" <<
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") <<
      "bnec $a2, $a3, 3f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBeqc");
}

TEST_F(AssemblerMIPS32r6Test, LongBeqzc) {
  constexpr uint32_t kNopCount1 = (1u << 20) + 1;
  constexpr uint32_t kNopCount2 = (1u << 20) + 1;
  constexpr uint32_t kRequiredCapacity = (kNopCount1 + kNopCount2 + 6u) * 4u;
  ASSERT_LT(__ GetBuffer()->Capacity(), kRequiredCapacity);
  __ GetBuffer()->ExtendCapacity(kRequiredCapacity);
  mips::MipsLabel label;
  __ Beqzc(mips::A0, &label);
  for (uint32_t i = 0; i != kNopCount1; ++i) {
    __ Nop();
  }
  __ Bind(&label);
  for (uint32_t i = 0; i != kNopCount2; ++i) {
    __ Nop();
  }
  __ Beqzc(mips::A2, &label);

  uint32_t offset_forward = 2 + kNopCount1;  // 2: account for auipc and jic.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_back = -(kNopCount2 + 1);  // 1: account for bnezc.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jic.

  // Note, we're using the ".fill" directive to tell the assembler to generate many NOPs
  // instead of generating them ourselves in the source code. This saves test time.
  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "bnezc $a0, 1f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward) << "\n"
      "1:\n" <<
      ".fill 0x" << std::hex << kNopCount1 << " , 4, 0\n"
      "2:\n" <<
      ".fill 0x" << std::hex << kNopCount2 << " , 4, 0\n"
      "bnezc $a2, 3f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBeqzc");
}

TEST_F(AssemblerMIPS32r6Test, LongBc) {
  constexpr uint32_t kNopCount1 = (1u << 25) + 1;
  constexpr uint32_t kNopCount2 = (1u << 25) + 1;
  constexpr uint32_t kRequiredCapacity = (kNopCount1 + kNopCount2 + 6u) * 4u;
  ASSERT_LT(__ GetBuffer()->Capacity(), kRequiredCapacity);
  __ GetBuffer()->ExtendCapacity(kRequiredCapacity);
  mips::MipsLabel label1, label2;
  __ Bc(&label1);
  for (uint32_t i = 0; i != kNopCount1; ++i) {
    __ Nop();
  }
  __ Bind(&label1);
  __ Bc(&label2);
  for (uint32_t i = 0; i != kNopCount2; ++i) {
    __ Nop();
  }
  __ Bind(&label2);
  __ Bc(&label1);

  uint32_t offset_forward1 = 2 + kNopCount1;  // 2: account for auipc and jic.
  offset_forward1 <<= 2;
  offset_forward1 += (offset_forward1 & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_forward2 = 2 + kNopCount2;  // 2: account for auipc and jic.
  offset_forward2 <<= 2;
  offset_forward2 += (offset_forward2 & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_back = -(2 + kNopCount2);  // 2: account for auipc and jic.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jic.

  // Note, we're using the ".fill" directive to tell the assembler to generate many NOPs
  // instead of generating them ourselves in the source code. This saves a few minutes
  // of test time.
  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward1) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward1) << "\n"
      ".fill 0x" << std::hex << kNopCount1 << " , 4, 0\n"
      "1:\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward2) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward2) << "\n"
      ".fill 0x" << std::hex << kNopCount2 << " , 4, 0\n"
      "2:\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_back) << "\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBc");
}

TEST_F(AssemblerMIPS32r6Test, ImpossibleReordering) {
  mips::MipsLabel label;
  __ SetReorder(true);
  __ Bind(&label);

  __ CmpLtD(mips::F0, mips::F2, mips::F4);
  __ Bc1nez(mips::F0, &label);  // F0 dependency.

  __ MulD(mips::F10, mips::F2, mips::F4);
  __ Bc1eqz(mips::F10, &label);  // F10 dependency.

  std::string expected =
      ".set noreorder\n"
      "1:\n"

      "cmp.lt.d $f0, $f2, $f4\n"
      "bc1nez $f0, 1b\n"
      "nop\n"

      "mul.d $f10, $f2, $f4\n"
      "bc1eqz $f10, 1b\n"
      "nop\n";
  DriverStr(expected, "ImpossibleReordering");
}

TEST_F(AssemblerMIPS32r6Test, Reordering) {
  mips::MipsLabel label;
  __ SetReorder(true);
  __ Bind(&label);

  __ CmpLtD(mips::F0, mips::F2, mips::F4);
  __ Bc1nez(mips::F2, &label);

  __ MulD(mips::F0, mips::F2, mips::F4);
  __ Bc1eqz(mips::F4, &label);

  std::string expected =
      ".set noreorder\n"
      "1:\n"

      "bc1nez $f2, 1b\n"
      "cmp.lt.d $f0, $f2, $f4\n"

      "bc1eqz $f4, 1b\n"
      "mul.d $f0, $f2, $f4\n";
  DriverStr(expected, "Reordering");
}

TEST_F(AssemblerMIPS32r6Test, SetReorder) {
  mips::MipsLabel label1, label2, label3, label4;

  __ SetReorder(true);
  __ Bind(&label1);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bc1nez(mips::F0, &label1);

  __ SetReorder(false);
  __ Bind(&label2);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bc1nez(mips::F0, &label2);

  __ SetReorder(true);
  __ Bind(&label3);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bc1eqz(mips::F0, &label3);

  __ SetReorder(false);
  __ Bind(&label4);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bc1eqz(mips::F0, &label4);

  std::string expected =
      ".set noreorder\n"
      "1:\n"
      "bc1nez $f0, 1b\n"
      "addu $t0, $t1, $t2\n"

      "2:\n"
      "addu $t0, $t1, $t2\n"
      "bc1nez $f0, 2b\n"
      "nop\n"

      "3:\n"
      "bc1eqz $f0, 3b\n"
      "addu $t0, $t1, $t2\n"

      "4:\n"
      "addu $t0, $t1, $t2\n"
      "bc1eqz $f0, 4b\n"
      "nop\n";
  DriverStr(expected, "SetReorder");
}

TEST_F(AssemblerMIPS32r6Test, ReorderPatchedInstruction) {
  __ SetReorder(true);
  mips::MipsLabel label1, label2;
  mips::MipsLabel patcher_label1, patcher_label2, patcher_label3, patcher_label4, patcher_label5;
  __ Lw(mips::V0, mips::A0, 0x5678, &patcher_label1);
  __ Bc1eqz(mips::F0, &label1);
  constexpr uint32_t kAdduCount1 = 63;
  for (size_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label1);
  __ Sw(mips::V0, mips::A0, 0x5678, &patcher_label2);
  __ Bc1nez(mips::F2, &label2);
  constexpr uint32_t kAdduCount2 = 64;
  for (size_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label2);
  __ Addiu(mips::V0, mips::A0, 0x5678, &patcher_label3);
  __ Bc1eqz(mips::F4, &label1);
  __ Lw(mips::V0, mips::A0, 0x5678, &patcher_label4);
  __ Jalr(mips::T9);
  __ Sw(mips::V0, mips::A0, 0x5678, &patcher_label5);
  __ Bltc(mips::V0, mips::V1, &label2);
  __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

  std::string expected =
      ".set noreorder\n"
      "bc1eqz $f0, 1f\n"
      "lw $v0, 0x5678($a0)\n" +
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
      "1:\n"
      "bc1nez $f2, 2f\n"
      "sw $v0, 0x5678($a0)\n" +
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
      "2:\n"
      "bc1eqz $f4, 1b\n"
      "addiu $v0, $a0, 0x5678\n"
      "jalr $t9\n"
      "lw $v0, 0x5678($a0)\n"
      "sw $v0, 0x5678($a0)\n"
      "bltc $v0, $v1, 2b\n"
      "nop\n"
      "addu $zero, $zero, $zero\n";
  DriverStr(expected, "ReorderPatchedInstruction");
  EXPECT_EQ(__ GetLabelLocation(&patcher_label1), 1 * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label2), (kAdduCount1 + 3) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label3), (kAdduCount1 + kAdduCount2 + 5) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label4), (kAdduCount1 + kAdduCount2 + 7) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label5), (kAdduCount1 + kAdduCount2 + 8) * 4u);
}

TEST_F(AssemblerMIPS32r6Test, LongBranchReorder) {
  mips::MipsLabel label, patcher_label1, patcher_label2;
  __ SetReorder(true);
  __ Addiu(mips::T0, mips::T1, 0x5678, &patcher_label1);
  __ Bc1nez(mips::F0, &label);
  constexpr uint32_t kAdduCount1 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Addiu(mips::T0, mips::T1, 0x5678, &patcher_label2);
  __ Bc1eqz(mips::F0, &label);

  uint32_t offset_forward = 2 + kAdduCount1;  // 2: account for auipc and jic.
  offset_forward <<= 2;
  offset_forward += (offset_forward & 0x8000) << 1;  // Account for sign extension in jic.

  uint32_t offset_back = -(kAdduCount2 + 2);  // 2: account for subu and bc1nez.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jic.

  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "addiu $t0, $t1, 0x5678\n"
      "bc1eqz $f0, 1f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_forward) << "\n"
      "1:\n" <<
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") <<
      "2:\n" <<
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") <<
      "addiu $t0, $t1, 0x5678\n"
      "bc1nez $f0, 3f\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jic $at, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "3:\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBranchReorder");
  EXPECT_EQ(__ GetLabelLocation(&patcher_label1), 0 * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label2), (kAdduCount1 + kAdduCount2 + 4) * 4u);
}

///////////////////////
// Loading Constants //
///////////////////////

TEST_F(AssemblerMIPS32r6Test, LoadFarthestNearLabelAddress) {
  mips::MipsLabel label;
  __ LoadLabelAddress(mips::V0, mips::ZERO, &label);
  constexpr size_t kAdduCount = 0x3FFDE;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      "lapc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n";
  DriverStr(expected, "LoadFarthestNearLabelAddress");
}

TEST_F(AssemblerMIPS32r6Test, LoadNearestFarLabelAddress) {
  mips::MipsLabel label;
  __ LoadLabelAddress(mips::V0, mips::ZERO, &label);
  constexpr size_t kAdduCount = 0x3FFDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "addiu $v0, $at, %lo(2f - 1b)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n";
  DriverStr(expected, "LoadNearestFarLabelAddress");
}

TEST_F(AssemblerMIPS32r6Test, LoadFarthestNearLiteral) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips::V0, mips::ZERO, literal);
  constexpr size_t kAdduCount = 0x3FFDE;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }

  std::string expected =
      "lwpc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadFarthestNearLiteral");
}

TEST_F(AssemblerMIPS32r6Test, LoadNearestFarLiteral) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips::V0, mips::ZERO, literal);
  constexpr size_t kAdduCount = 0x3FFDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "lw $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadNearestFarLiteral");
}

// MSA instructions.

TEST_F(AssemblerMIPS32r6Test, AndV) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::AndV, "and.v ${reg1}, ${reg2}, ${reg3}"), "and.v");
}

TEST_F(AssemblerMIPS32r6Test, OrV) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::OrV, "or.v ${reg1}, ${reg2}, ${reg3}"), "or.v");
}

TEST_F(AssemblerMIPS32r6Test, NorV) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::NorV, "nor.v ${reg1}, ${reg2}, ${reg3}"), "nor.v");
}

TEST_F(AssemblerMIPS32r6Test, XorV) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::XorV, "xor.v ${reg1}, ${reg2}, ${reg3}"), "xor.v");
}

TEST_F(AssemblerMIPS32r6Test, AddvB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::AddvB, "addv.b ${reg1}, ${reg2}, ${reg3}"), "addv.b");
}

TEST_F(AssemblerMIPS32r6Test, AddvH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::AddvH, "addv.h ${reg1}, ${reg2}, ${reg3}"), "addv.h");
}

TEST_F(AssemblerMIPS32r6Test, AddvW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::AddvW, "addv.w ${reg1}, ${reg2}, ${reg3}"), "addv.w");
}

TEST_F(AssemblerMIPS32r6Test, AddvD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::AddvD, "addv.d ${reg1}, ${reg2}, ${reg3}"), "addv.d");
}

TEST_F(AssemblerMIPS32r6Test, SubvB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SubvB, "subv.b ${reg1}, ${reg2}, ${reg3}"), "subv.b");
}

TEST_F(AssemblerMIPS32r6Test, SubvH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SubvH, "subv.h ${reg1}, ${reg2}, ${reg3}"), "subv.h");
}

TEST_F(AssemblerMIPS32r6Test, SubvW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SubvW, "subv.w ${reg1}, ${reg2}, ${reg3}"), "subv.w");
}

TEST_F(AssemblerMIPS32r6Test, SubvD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SubvD, "subv.d ${reg1}, ${reg2}, ${reg3}"), "subv.d");
}

TEST_F(AssemblerMIPS32r6Test, Asub_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_sB, "asub_s.b ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Asub_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_sH, "asub_s.h ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Asub_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_sW, "asub_s.w ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Asub_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_sD, "asub_s.d ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Asub_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_uB, "asub_u.b ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Asub_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_uH, "asub_u.h ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Asub_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_uW, "asub_u.w ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Asub_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Asub_uD, "asub_u.d ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.d");
}

TEST_F(AssemblerMIPS32r6Test, MulvB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MulvB, "mulv.b ${reg1}, ${reg2}, ${reg3}"), "mulv.b");
}

TEST_F(AssemblerMIPS32r6Test, MulvH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MulvH, "mulv.h ${reg1}, ${reg2}, ${reg3}"), "mulv.h");
}

TEST_F(AssemblerMIPS32r6Test, MulvW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MulvW, "mulv.w ${reg1}, ${reg2}, ${reg3}"), "mulv.w");
}

TEST_F(AssemblerMIPS32r6Test, MulvD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MulvD, "mulv.d ${reg1}, ${reg2}, ${reg3}"), "mulv.d");
}

TEST_F(AssemblerMIPS32r6Test, Div_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_sB, "div_s.b ${reg1}, ${reg2}, ${reg3}"),
            "div_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Div_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_sH, "div_s.h ${reg1}, ${reg2}, ${reg3}"),
            "div_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Div_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_sW, "div_s.w ${reg1}, ${reg2}, ${reg3}"),
            "div_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Div_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_sD, "div_s.d ${reg1}, ${reg2}, ${reg3}"),
            "div_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Div_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_uB, "div_u.b ${reg1}, ${reg2}, ${reg3}"),
            "div_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Div_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_uH, "div_u.h ${reg1}, ${reg2}, ${reg3}"),
            "div_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Div_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_uW, "div_u.w ${reg1}, ${reg2}, ${reg3}"),
            "div_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Div_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Div_uD, "div_u.d ${reg1}, ${reg2}, ${reg3}"),
            "div_u.d");
}

TEST_F(AssemblerMIPS32r6Test, Mod_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_sB, "mod_s.b ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Mod_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_sH, "mod_s.h ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Mod_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_sW, "mod_s.w ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Mod_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_sD, "mod_s.d ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Mod_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_uB, "mod_u.b ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Mod_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_uH, "mod_u.h ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Mod_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_uW, "mod_u.w ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Mod_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Mod_uD, "mod_u.d ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.d");
}

TEST_F(AssemblerMIPS32r6Test, Add_aB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Add_aB, "add_a.b ${reg1}, ${reg2}, ${reg3}"),
            "add_a.b");
}

TEST_F(AssemblerMIPS32r6Test, Add_aH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Add_aH, "add_a.h ${reg1}, ${reg2}, ${reg3}"),
            "add_a.h");
}

TEST_F(AssemblerMIPS32r6Test, Add_aW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Add_aW, "add_a.w ${reg1}, ${reg2}, ${reg3}"),
            "add_a.w");
}

TEST_F(AssemblerMIPS32r6Test, Add_aD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Add_aD, "add_a.d ${reg1}, ${reg2}, ${reg3}"),
            "add_a.d");
}

TEST_F(AssemblerMIPS32r6Test, Ave_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_sB, "ave_s.b ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Ave_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_sH, "ave_s.h ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Ave_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_sW, "ave_s.w ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Ave_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_sD, "ave_s.d ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Ave_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_uB, "ave_u.b ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Ave_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_uH, "ave_u.h ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Ave_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_uW, "ave_u.w ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Ave_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Ave_uD, "ave_u.d ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.d");
}

TEST_F(AssemblerMIPS32r6Test, Aver_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_sB, "aver_s.b ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Aver_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_sH, "aver_s.h ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Aver_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_sW, "aver_s.w ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Aver_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_sD, "aver_s.d ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Aver_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_uB, "aver_u.b ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Aver_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_uH, "aver_u.h ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Aver_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_uW, "aver_u.w ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Aver_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Aver_uD, "aver_u.d ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.d");
}

TEST_F(AssemblerMIPS32r6Test, Max_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_sB, "max_s.b ${reg1}, ${reg2}, ${reg3}"),
            "max_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Max_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_sH, "max_s.h ${reg1}, ${reg2}, ${reg3}"),
            "max_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Max_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_sW, "max_s.w ${reg1}, ${reg2}, ${reg3}"),
            "max_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Max_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_sD, "max_s.d ${reg1}, ${reg2}, ${reg3}"),
            "max_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Max_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_uB, "max_u.b ${reg1}, ${reg2}, ${reg3}"),
            "max_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Max_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_uH, "max_u.h ${reg1}, ${reg2}, ${reg3}"),
            "max_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Max_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_uW, "max_u.w ${reg1}, ${reg2}, ${reg3}"),
            "max_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Max_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Max_uD, "max_u.d ${reg1}, ${reg2}, ${reg3}"),
            "max_u.d");
}

TEST_F(AssemblerMIPS32r6Test, Min_sB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_sB, "min_s.b ${reg1}, ${reg2}, ${reg3}"),
            "min_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Min_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_sH, "min_s.h ${reg1}, ${reg2}, ${reg3}"),
            "min_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Min_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_sW, "min_s.w ${reg1}, ${reg2}, ${reg3}"),
            "min_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Min_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_sD, "min_s.d ${reg1}, ${reg2}, ${reg3}"),
            "min_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Min_uB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_uB, "min_u.b ${reg1}, ${reg2}, ${reg3}"),
            "min_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Min_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_uH, "min_u.h ${reg1}, ${reg2}, ${reg3}"),
            "min_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Min_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_uW, "min_u.w ${reg1}, ${reg2}, ${reg3}"),
            "min_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Min_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Min_uD, "min_u.d ${reg1}, ${reg2}, ${reg3}"),
            "min_u.d");
}

TEST_F(AssemblerMIPS32r6Test, FaddW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FaddW, "fadd.w ${reg1}, ${reg2}, ${reg3}"), "fadd.w");
}

TEST_F(AssemblerMIPS32r6Test, FaddD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FaddD, "fadd.d ${reg1}, ${reg2}, ${reg3}"), "fadd.d");
}

TEST_F(AssemblerMIPS32r6Test, FsubW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FsubW, "fsub.w ${reg1}, ${reg2}, ${reg3}"), "fsub.w");
}

TEST_F(AssemblerMIPS32r6Test, FsubD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FsubD, "fsub.d ${reg1}, ${reg2}, ${reg3}"), "fsub.d");
}

TEST_F(AssemblerMIPS32r6Test, FmulW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmulW, "fmul.w ${reg1}, ${reg2}, ${reg3}"), "fmul.w");
}

TEST_F(AssemblerMIPS32r6Test, FmulD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmulD, "fmul.d ${reg1}, ${reg2}, ${reg3}"), "fmul.d");
}

TEST_F(AssemblerMIPS32r6Test, FdivW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FdivW, "fdiv.w ${reg1}, ${reg2}, ${reg3}"), "fdiv.w");
}

TEST_F(AssemblerMIPS32r6Test, FdivD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FdivD, "fdiv.d ${reg1}, ${reg2}, ${reg3}"), "fdiv.d");
}

TEST_F(AssemblerMIPS32r6Test, FmaxW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmaxW, "fmax.w ${reg1}, ${reg2}, ${reg3}"), "fmax.w");
}

TEST_F(AssemblerMIPS32r6Test, FmaxD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmaxD, "fmax.d ${reg1}, ${reg2}, ${reg3}"), "fmax.d");
}

TEST_F(AssemblerMIPS32r6Test, FminW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FminW, "fmin.w ${reg1}, ${reg2}, ${reg3}"), "fmin.w");
}

TEST_F(AssemblerMIPS32r6Test, FminD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FminD, "fmin.d ${reg1}, ${reg2}, ${reg3}"), "fmin.d");
}

TEST_F(AssemblerMIPS32r6Test, Ffint_sW) {
  DriverStr(RepeatVV(&mips::MipsAssembler::Ffint_sW, "ffint_s.w ${reg1}, ${reg2}"), "ffint_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Ffint_sD) {
  DriverStr(RepeatVV(&mips::MipsAssembler::Ffint_sD, "ffint_s.d ${reg1}, ${reg2}"), "ffint_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Ftint_sW) {
  DriverStr(RepeatVV(&mips::MipsAssembler::Ftint_sW, "ftint_s.w ${reg1}, ${reg2}"), "ftint_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Ftint_sD) {
  DriverStr(RepeatVV(&mips::MipsAssembler::Ftint_sD, "ftint_s.d ${reg1}, ${reg2}"), "ftint_s.d");
}

TEST_F(AssemblerMIPS32r6Test, SllB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SllB, "sll.b ${reg1}, ${reg2}, ${reg3}"), "sll.b");
}

TEST_F(AssemblerMIPS32r6Test, SllH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SllH, "sll.h ${reg1}, ${reg2}, ${reg3}"), "sll.h");
}

TEST_F(AssemblerMIPS32r6Test, SllW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SllW, "sll.w ${reg1}, ${reg2}, ${reg3}"), "sll.w");
}

TEST_F(AssemblerMIPS32r6Test, SllD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SllD, "sll.d ${reg1}, ${reg2}, ${reg3}"), "sll.d");
}

TEST_F(AssemblerMIPS32r6Test, SraB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SraB, "sra.b ${reg1}, ${reg2}, ${reg3}"), "sra.b");
}

TEST_F(AssemblerMIPS32r6Test, SraH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SraH, "sra.h ${reg1}, ${reg2}, ${reg3}"), "sra.h");
}

TEST_F(AssemblerMIPS32r6Test, SraW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SraW, "sra.w ${reg1}, ${reg2}, ${reg3}"), "sra.w");
}

TEST_F(AssemblerMIPS32r6Test, SraD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SraD, "sra.d ${reg1}, ${reg2}, ${reg3}"), "sra.d");
}

TEST_F(AssemblerMIPS32r6Test, SrlB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SrlB, "srl.b ${reg1}, ${reg2}, ${reg3}"), "srl.b");
}

TEST_F(AssemblerMIPS32r6Test, SrlH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SrlH, "srl.h ${reg1}, ${reg2}, ${reg3}"), "srl.h");
}

TEST_F(AssemblerMIPS32r6Test, SrlW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SrlW, "srl.w ${reg1}, ${reg2}, ${reg3}"), "srl.w");
}

TEST_F(AssemblerMIPS32r6Test, SrlD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::SrlD, "srl.d ${reg1}, ${reg2}, ${reg3}"), "srl.d");
}

TEST_F(AssemblerMIPS32r6Test, SlliB) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SlliB, 3, "slli.b ${reg1}, ${reg2}, {imm}"), "slli.b");
}

TEST_F(AssemblerMIPS32r6Test, SlliH) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SlliH, 4, "slli.h ${reg1}, ${reg2}, {imm}"), "slli.h");
}

TEST_F(AssemblerMIPS32r6Test, SlliW) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SlliW, 5, "slli.w ${reg1}, ${reg2}, {imm}"), "slli.w");
}

TEST_F(AssemblerMIPS32r6Test, SlliD) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SlliD, 6, "slli.d ${reg1}, ${reg2}, {imm}"), "slli.d");
}

TEST_F(AssemblerMIPS32r6Test, MoveV) {
  DriverStr(RepeatVV(&mips::MipsAssembler::MoveV, "move.v ${reg1}, ${reg2}"), "move.v");
}

TEST_F(AssemblerMIPS32r6Test, SplatiB) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SplatiB, 4, "splati.b ${reg1}, ${reg2}[{imm}]"),
            "splati.b");
}

TEST_F(AssemblerMIPS32r6Test, SplatiH) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SplatiH, 3, "splati.h ${reg1}, ${reg2}[{imm}]"),
            "splati.h");
}

TEST_F(AssemblerMIPS32r6Test, SplatiW) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SplatiW, 2, "splati.w ${reg1}, ${reg2}[{imm}]"),
            "splati.w");
}

TEST_F(AssemblerMIPS32r6Test, SplatiD) {
  DriverStr(RepeatVVIb(&mips::MipsAssembler::SplatiD, 1, "splati.d ${reg1}, ${reg2}[{imm}]"),
            "splati.d");
}

TEST_F(AssemblerMIPS32r6Test, Copy_sB) {
  DriverStr(RepeatRVIb(&mips::MipsAssembler::Copy_sB, 4, "copy_s.b ${reg1}, ${reg2}[{imm}]"),
            "copy_s.b");
}

TEST_F(AssemblerMIPS32r6Test, Copy_sH) {
  DriverStr(RepeatRVIb(&mips::MipsAssembler::Copy_sH, 3, "copy_s.h ${reg1}, ${reg2}[{imm}]"),
            "copy_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Copy_sW) {
  DriverStr(RepeatRVIb(&mips::MipsAssembler::Copy_sW, 2, "copy_s.w ${reg1}, ${reg2}[{imm}]"),
            "copy_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Copy_uB) {
  DriverStr(RepeatRVIb(&mips::MipsAssembler::Copy_uB, 4, "copy_u.b ${reg1}, ${reg2}[{imm}]"),
            "copy_u.b");
}

TEST_F(AssemblerMIPS32r6Test, Copy_uH) {
  DriverStr(RepeatRVIb(&mips::MipsAssembler::Copy_uH, 3, "copy_u.h ${reg1}, ${reg2}[{imm}]"),
            "copy_u.h");
}

TEST_F(AssemblerMIPS32r6Test, InsertB) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::InsertB, 4, "insert.b ${reg1}[{imm}], ${reg2}"),
            "insert.b");
}

TEST_F(AssemblerMIPS32r6Test, InsertH) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::InsertH, 3, "insert.h ${reg1}[{imm}], ${reg2}"),
            "insert.h");
}

TEST_F(AssemblerMIPS32r6Test, InsertW) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::InsertW, 2, "insert.w ${reg1}[{imm}], ${reg2}"),
            "insert.w");
}

TEST_F(AssemblerMIPS32r6Test, FillB) {
  DriverStr(RepeatVR(&mips::MipsAssembler::FillB, "fill.b ${reg1}, ${reg2}"), "fill.b");
}

TEST_F(AssemblerMIPS32r6Test, FillH) {
  DriverStr(RepeatVR(&mips::MipsAssembler::FillH, "fill.h ${reg1}, ${reg2}"), "fill.h");
}

TEST_F(AssemblerMIPS32r6Test, FillW) {
  DriverStr(RepeatVR(&mips::MipsAssembler::FillW, "fill.w ${reg1}, ${reg2}"), "fill.w");
}

TEST_F(AssemblerMIPS32r6Test, LdiB) {
  DriverStr(RepeatVIb(&mips::MipsAssembler::LdiB, -8, "ldi.b ${reg}, {imm}"), "ldi.b");
}

TEST_F(AssemblerMIPS32r6Test, LdiH) {
  DriverStr(RepeatVIb(&mips::MipsAssembler::LdiH, -10, "ldi.h ${reg}, {imm}"), "ldi.h");
}

TEST_F(AssemblerMIPS32r6Test, LdiW) {
  DriverStr(RepeatVIb(&mips::MipsAssembler::LdiW, -10, "ldi.w ${reg}, {imm}"), "ldi.w");
}

TEST_F(AssemblerMIPS32r6Test, LdiD) {
  DriverStr(RepeatVIb(&mips::MipsAssembler::LdiD, -10, "ldi.d ${reg}, {imm}"), "ldi.d");
}

TEST_F(AssemblerMIPS32r6Test, LdB) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::LdB, -10, "ld.b ${reg1}, {imm}(${reg2})"), "ld.b");
}

TEST_F(AssemblerMIPS32r6Test, LdH) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::LdH, -10, "ld.h ${reg1}, {imm}(${reg2})", 0, 2),
            "ld.h");
}

TEST_F(AssemblerMIPS32r6Test, LdW) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::LdW, -10, "ld.w ${reg1}, {imm}(${reg2})", 0, 4),
            "ld.w");
}

TEST_F(AssemblerMIPS32r6Test, LdD) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::LdD, -10, "ld.d ${reg1}, {imm}(${reg2})", 0, 8),
            "ld.d");
}

TEST_F(AssemblerMIPS32r6Test, StB) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::StB, -10, "st.b ${reg1}, {imm}(${reg2})"), "st.b");
}

TEST_F(AssemblerMIPS32r6Test, StH) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::StH, -10, "st.h ${reg1}, {imm}(${reg2})", 0, 2),
            "st.h");
}

TEST_F(AssemblerMIPS32r6Test, StW) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::StW, -10, "st.w ${reg1}, {imm}(${reg2})", 0, 4),
            "st.w");
}

TEST_F(AssemblerMIPS32r6Test, StD) {
  DriverStr(RepeatVRIb(&mips::MipsAssembler::StD, -10, "st.d ${reg1}, {imm}(${reg2})", 0, 8),
            "st.d");
}

TEST_F(AssemblerMIPS32r6Test, IlvlB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvlB, "ilvl.b ${reg1}, ${reg2}, ${reg3}"), "ilvl.b");
}

TEST_F(AssemblerMIPS32r6Test, IlvlH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvlH, "ilvl.h ${reg1}, ${reg2}, ${reg3}"), "ilvl.h");
}

TEST_F(AssemblerMIPS32r6Test, IlvlW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvlW, "ilvl.w ${reg1}, ${reg2}, ${reg3}"), "ilvl.w");
}

TEST_F(AssemblerMIPS32r6Test, IlvlD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvlD, "ilvl.d ${reg1}, ${reg2}, ${reg3}"), "ilvl.d");
}

TEST_F(AssemblerMIPS32r6Test, IlvrB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvrB, "ilvr.b ${reg1}, ${reg2}, ${reg3}"), "ilvr.b");
}

TEST_F(AssemblerMIPS32r6Test, IlvrH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvrH, "ilvr.h ${reg1}, ${reg2}, ${reg3}"), "ilvr.h");
}

TEST_F(AssemblerMIPS32r6Test, IlvrW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvrW, "ilvr.w ${reg1}, ${reg2}, ${reg3}"), "ilvr.w");
}

TEST_F(AssemblerMIPS32r6Test, IlvrD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvrD, "ilvr.d ${reg1}, ${reg2}, ${reg3}"), "ilvr.d");
}

TEST_F(AssemblerMIPS32r6Test, IlvevB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvevB, "ilvev.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.b");
}

TEST_F(AssemblerMIPS32r6Test, IlvevH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvevH, "ilvev.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.h");
}

TEST_F(AssemblerMIPS32r6Test, IlvevW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvevW, "ilvev.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.w");
}

TEST_F(AssemblerMIPS32r6Test, IlvevD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvevD, "ilvev.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.d");
}

TEST_F(AssemblerMIPS32r6Test, IlvodB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvodB, "ilvod.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.b");
}

TEST_F(AssemblerMIPS32r6Test, IlvodH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvodH, "ilvod.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.h");
}

TEST_F(AssemblerMIPS32r6Test, IlvodW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvodW, "ilvod.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.w");
}

TEST_F(AssemblerMIPS32r6Test, IlvodD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::IlvodD, "ilvod.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.d");
}

TEST_F(AssemblerMIPS32r6Test, MaddvB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MaddvB, "maddv.b ${reg1}, ${reg2}, ${reg3}"),
            "maddv.b");
}

TEST_F(AssemblerMIPS32r6Test, MaddvH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MaddvH, "maddv.h ${reg1}, ${reg2}, ${reg3}"),
            "maddv.h");
}

TEST_F(AssemblerMIPS32r6Test, MaddvW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MaddvW, "maddv.w ${reg1}, ${reg2}, ${reg3}"),
            "maddv.w");
}

TEST_F(AssemblerMIPS32r6Test, MaddvD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MaddvD, "maddv.d ${reg1}, ${reg2}, ${reg3}"),
            "maddv.d");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_sH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_sH, "hadd_s.h ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.h");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_sW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_sW, "hadd_s.w ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.w");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_sD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_sD, "hadd_s.d ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.d");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_uH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_uH, "hadd_u.h ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.h");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_uW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_uW, "hadd_u.w ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.w");
}

TEST_F(AssemblerMIPS32r6Test, Hadd_uD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::Hadd_uD, "hadd_u.d ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.d");
}

TEST_F(AssemblerMIPS32r6Test, MsubvB) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MsubvB, "msubv.b ${reg1}, ${reg2}, ${reg3}"),
            "msubv.b");
}

TEST_F(AssemblerMIPS32r6Test, MsubvH) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MsubvH, "msubv.h ${reg1}, ${reg2}, ${reg3}"),
            "msubv.h");
}

TEST_F(AssemblerMIPS32r6Test, MsubvW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MsubvW, "msubv.w ${reg1}, ${reg2}, ${reg3}"),
            "msubv.w");
}

TEST_F(AssemblerMIPS32r6Test, MsubvD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::MsubvD, "msubv.d ${reg1}, ${reg2}, ${reg3}"),
            "msubv.d");
}

TEST_F(AssemblerMIPS32r6Test, FmaddW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmaddW, "fmadd.w ${reg1}, ${reg2}, ${reg3}"),
            "fmadd.w");
}

TEST_F(AssemblerMIPS32r6Test, FmaddD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmaddD, "fmadd.d ${reg1}, ${reg2}, ${reg3}"),
            "fmadd.d");
}

TEST_F(AssemblerMIPS32r6Test, FmsubW) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmsubW, "fmsub.w ${reg1}, ${reg2}, ${reg3}"),
            "fmsub.w");
}

TEST_F(AssemblerMIPS32r6Test, FmsubD) {
  DriverStr(RepeatVVV(&mips::MipsAssembler::FmsubD, "fmsub.d ${reg1}, ${reg2}, ${reg3}"),
            "fmsub.d");
}

#undef __

}  // namespace art
