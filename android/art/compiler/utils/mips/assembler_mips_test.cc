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

class AssemblerMIPSTest : public AssemblerTest<mips::MipsAssembler,
                                               mips::MipsLabel,
                                               mips::Register,
                                               mips::FRegister,
                                               uint32_t> {
 public:
  typedef AssemblerTest<mips::MipsAssembler,
                        mips::MipsLabel,
                        mips::Register,
                        mips::FRegister,
                        uint32_t> Base;

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    return " --no-warn -32 -march=mips32r2";
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mmips:isa32r2";
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
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
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
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        instr_name + " 2f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        "2:\n" +
        instr_name + " 1b\n" +
        (is_bare ? "" : "nop\n") +
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

  void BranchFpuCondCodeHelper(void (mips::MipsAssembler::*f)(int,
                                                              mips::MipsLabel*,
                                                              bool),
                               const std::string& instr_name,
                               bool is_bare = false) {
    __ SetReorder(false);
    mips::MipsLabel label;
    (Base::GetAssembler()->*f)(0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
    }
    (Base::GetAssembler()->*f)(7, &label, is_bare);
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $fcc0, 1f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $fcc7, 1b\n" +
        (is_bare ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<mips::Register*> registers_;
  std::map<mips::Register, std::string, MIPSCpuRegisterCompare> secondary_register_names_;

  std::vector<mips::FRegister*> fp_registers_;
};


TEST_F(AssemblerMIPSTest, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

TEST_F(AssemblerMIPSTest, Addu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Addu, "addu ${reg1}, ${reg2}, ${reg3}"), "Addu");
}

TEST_F(AssemblerMIPSTest, Addiu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Addiu, -16, "addiu ${reg1}, ${reg2}, {imm}"), "Addiu");
}

TEST_F(AssemblerMIPSTest, Subu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Subu, "subu ${reg1}, ${reg2}, ${reg3}"), "Subu");
}

TEST_F(AssemblerMIPSTest, MultR2) {
  DriverStr(RepeatRR(&mips::MipsAssembler::MultR2, "mult ${reg1}, ${reg2}"), "MultR2");
}

TEST_F(AssemblerMIPSTest, MultuR2) {
  DriverStr(RepeatRR(&mips::MipsAssembler::MultuR2, "multu ${reg1}, ${reg2}"), "MultuR2");
}

TEST_F(AssemblerMIPSTest, DivR2Basic) {
  DriverStr(RepeatRR(&mips::MipsAssembler::DivR2, "div $zero, ${reg1}, ${reg2}"), "DivR2Basic");
}

TEST_F(AssemblerMIPSTest, DivuR2Basic) {
  DriverStr(RepeatRR(&mips::MipsAssembler::DivuR2, "divu $zero, ${reg1}, ${reg2}"), "DivuR2Basic");
}

TEST_F(AssemblerMIPSTest, MulR2) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::MulR2, "mul ${reg1}, ${reg2}, ${reg3}"), "MulR2");
}

TEST_F(AssemblerMIPSTest, DivR2) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivR2, "div $zero, ${reg2}, ${reg3}\nmflo ${reg1}"),
            "DivR2");
}

TEST_F(AssemblerMIPSTest, ModR2) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModR2, "div $zero, ${reg2}, ${reg3}\nmfhi ${reg1}"),
            "ModR2");
}

TEST_F(AssemblerMIPSTest, DivuR2) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::DivuR2, "divu $zero, ${reg2}, ${reg3}\nmflo ${reg1}"),
            "DivuR2");
}

TEST_F(AssemblerMIPSTest, ModuR2) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::ModuR2, "divu $zero, ${reg2}, ${reg3}\nmfhi ${reg1}"),
            "ModuR2");
}

TEST_F(AssemblerMIPSTest, And) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::And, "and ${reg1}, ${reg2}, ${reg3}"), "And");
}

TEST_F(AssemblerMIPSTest, Andi) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Andi, 16, "andi ${reg1}, ${reg2}, {imm}"), "Andi");
}

TEST_F(AssemblerMIPSTest, Or) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Or, "or ${reg1}, ${reg2}, ${reg3}"), "Or");
}

TEST_F(AssemblerMIPSTest, Ori) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Ori, 16, "ori ${reg1}, ${reg2}, {imm}"), "Ori");
}

TEST_F(AssemblerMIPSTest, Xor) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Xor, "xor ${reg1}, ${reg2}, ${reg3}"), "Xor");
}

TEST_F(AssemblerMIPSTest, Xori) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Xori, 16, "xori ${reg1}, ${reg2}, {imm}"), "Xori");
}

TEST_F(AssemblerMIPSTest, Nor) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Nor, "nor ${reg1}, ${reg2}, ${reg3}"), "Nor");
}

//////////
// MISC //
//////////

TEST_F(AssemblerMIPSTest, Movz) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Movz, "movz ${reg1}, ${reg2}, ${reg3}"), "Movz");
}

TEST_F(AssemblerMIPSTest, Movn) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Movn, "movn ${reg1}, ${reg2}, ${reg3}"), "Movn");
}

TEST_F(AssemblerMIPSTest, Seb) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Seb, "seb ${reg1}, ${reg2}"), "Seb");
}

TEST_F(AssemblerMIPSTest, Seh) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Seh, "seh ${reg1}, ${reg2}"), "Seh");
}

TEST_F(AssemblerMIPSTest, Sll) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sll, 5, "sll ${reg1}, ${reg2}, {imm}"), "Sll");
}

TEST_F(AssemblerMIPSTest, Srl) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Srl, 5, "srl ${reg1}, ${reg2}, {imm}"), "Srl");
}

TEST_F(AssemblerMIPSTest, Sra) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sra, 5, "sra ${reg1}, ${reg2}, {imm}"), "Sra");
}

TEST_F(AssemblerMIPSTest, Sllv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Sllv, "sllv ${reg1}, ${reg2}, ${reg3}"), "Sllv");
}

TEST_F(AssemblerMIPSTest, Srlv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Srlv, "srlv ${reg1}, ${reg2}, ${reg3}"), "Srlv");
}

TEST_F(AssemblerMIPSTest, Rotrv) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Rotrv, "rotrv ${reg1}, ${reg2}, ${reg3}"), "rotrv");
}

TEST_F(AssemblerMIPSTest, Srav) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Srav, "srav ${reg1}, ${reg2}, ${reg3}"), "Srav");
}

TEST_F(AssemblerMIPSTest, Ins) {
  std::vector<mips::Register*> regs = GetRegisters();
  WarnOnCombinations(regs.size() * regs.size() * 33 * 16);
  std::string expected;
  for (mips::Register* reg1 : regs) {
    for (mips::Register* reg2 : regs) {
      for (int32_t pos = 0; pos < 32; pos++) {
        for (int32_t size = 1; pos + size <= 32; size++) {
          __ Ins(*reg1, *reg2, pos, size);
          std::ostringstream instr;
          instr << "ins $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
          expected += instr.str();
        }
      }
    }
  }
  DriverStr(expected, "Ins");
}

TEST_F(AssemblerMIPSTest, Ext) {
  std::vector<mips::Register*> regs = GetRegisters();
  WarnOnCombinations(regs.size() * regs.size() * 33 * 16);
  std::string expected;
  for (mips::Register* reg1 : regs) {
    for (mips::Register* reg2 : regs) {
      for (int32_t pos = 0; pos < 32; pos++) {
        for (int32_t size = 1; pos + size <= 32; size++) {
          __ Ext(*reg1, *reg2, pos, size);
          std::ostringstream instr;
          instr << "ext $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
          expected += instr.str();
        }
      }
    }
  }
  DriverStr(expected, "Ext");
}

TEST_F(AssemblerMIPSTest, ClzR2) {
  DriverStr(RepeatRR(&mips::MipsAssembler::ClzR2, "clz ${reg1}, ${reg2}"), "clzR2");
}

TEST_F(AssemblerMIPSTest, CloR2) {
  DriverStr(RepeatRR(&mips::MipsAssembler::CloR2, "clo ${reg1}, ${reg2}"), "cloR2");
}

TEST_F(AssemblerMIPSTest, Lb) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lb, -16, "lb ${reg1}, {imm}(${reg2})"), "Lb");
}

TEST_F(AssemblerMIPSTest, Lh) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lh, -16, "lh ${reg1}, {imm}(${reg2})"), "Lh");
}

TEST_F(AssemblerMIPSTest, Lwl) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lwl, -16, "lwl ${reg1}, {imm}(${reg2})"), "Lwl");
}

TEST_F(AssemblerMIPSTest, Lw) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lw, -16, "lw ${reg1}, {imm}(${reg2})"), "Lw");
}

TEST_F(AssemblerMIPSTest, Lwr) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lwr, -16, "lwr ${reg1}, {imm}(${reg2})"), "Lwr");
}

TEST_F(AssemblerMIPSTest, Lbu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lbu, -16, "lbu ${reg1}, {imm}(${reg2})"), "Lbu");
}

TEST_F(AssemblerMIPSTest, Lhu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Lhu, -16, "lhu ${reg1}, {imm}(${reg2})"), "Lhu");
}

TEST_F(AssemblerMIPSTest, Lui) {
  DriverStr(RepeatRIb(&mips::MipsAssembler::Lui, 16, "lui ${reg}, {imm}"), "Lui");
}

TEST_F(AssemblerMIPSTest, Mfhi) {
  DriverStr(RepeatR(&mips::MipsAssembler::Mfhi, "mfhi ${reg}"), "Mfhi");
}

TEST_F(AssemblerMIPSTest, Mflo) {
  DriverStr(RepeatR(&mips::MipsAssembler::Mflo, "mflo ${reg}"), "Mflo");
}

TEST_F(AssemblerMIPSTest, Sb) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sb, -16, "sb ${reg1}, {imm}(${reg2})"), "Sb");
}

TEST_F(AssemblerMIPSTest, Sh) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sh, -16, "sh ${reg1}, {imm}(${reg2})"), "Sh");
}

TEST_F(AssemblerMIPSTest, Swl) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Swl, -16, "swl ${reg1}, {imm}(${reg2})"), "Swl");
}

TEST_F(AssemblerMIPSTest, Sw) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sw, -16, "sw ${reg1}, {imm}(${reg2})"), "Sw");
}

TEST_F(AssemblerMIPSTest, Swr) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Swr, -16, "swr ${reg1}, {imm}(${reg2})"), "Swr");
}

TEST_F(AssemblerMIPSTest, LlR2) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::LlR2, -16, "ll ${reg1}, {imm}(${reg2})"), "LlR2");
}

TEST_F(AssemblerMIPSTest, ScR2) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::ScR2, -16, "sc ${reg1}, {imm}(${reg2})"), "ScR2");
}

TEST_F(AssemblerMIPSTest, Slt) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Slt, "slt ${reg1}, ${reg2}, ${reg3}"), "Slt");
}

TEST_F(AssemblerMIPSTest, Sltu) {
  DriverStr(RepeatRRR(&mips::MipsAssembler::Sltu, "sltu ${reg1}, ${reg2}, ${reg3}"), "Sltu");
}

TEST_F(AssemblerMIPSTest, Slti) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Slti, -16, "slti ${reg1}, ${reg2}, {imm}"), "Slti");
}

TEST_F(AssemblerMIPSTest, Sltiu) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Sltiu, -16, "sltiu ${reg1}, ${reg2}, {imm}"), "Sltiu");
}

TEST_F(AssemblerMIPSTest, AddS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::AddS, "add.s ${reg1}, ${reg2}, ${reg3}"), "AddS");
}

TEST_F(AssemblerMIPSTest, AddD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::AddD, "add.d ${reg1}, ${reg2}, ${reg3}"), "AddD");
}

TEST_F(AssemblerMIPSTest, SubS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SubS, "sub.s ${reg1}, ${reg2}, ${reg3}"), "SubS");
}

TEST_F(AssemblerMIPSTest, SubD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::SubD, "sub.d ${reg1}, ${reg2}, ${reg3}"), "SubD");
}

TEST_F(AssemblerMIPSTest, MulS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MulS, "mul.s ${reg1}, ${reg2}, ${reg3}"), "MulS");
}

TEST_F(AssemblerMIPSTest, MulD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::MulD, "mul.d ${reg1}, ${reg2}, ${reg3}"), "MulD");
}

TEST_F(AssemblerMIPSTest, DivS) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::DivS, "div.s ${reg1}, ${reg2}, ${reg3}"), "DivS");
}

TEST_F(AssemblerMIPSTest, DivD) {
  DriverStr(RepeatFFF(&mips::MipsAssembler::DivD, "div.d ${reg1}, ${reg2}, ${reg3}"), "DivD");
}

TEST_F(AssemblerMIPSTest, MovS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::MovS, "mov.s ${reg1}, ${reg2}"), "MovS");
}

TEST_F(AssemblerMIPSTest, MovD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::MovD, "mov.d ${reg1}, ${reg2}"), "MovD");
}

TEST_F(AssemblerMIPSTest, NegS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::NegS, "neg.s ${reg1}, ${reg2}"), "NegS");
}

TEST_F(AssemblerMIPSTest, NegD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::NegD, "neg.d ${reg1}, ${reg2}"), "NegD");
}

TEST_F(AssemblerMIPSTest, FloorWS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::FloorWS, "floor.w.s ${reg1}, ${reg2}"), "floor.w.s");
}

TEST_F(AssemblerMIPSTest, FloorWD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::FloorWD, "floor.w.d ${reg1}, ${reg2}"), "floor.w.d");
}

TEST_F(AssemblerMIPSTest, CunS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CunS, 3, "c.un.s $fcc{imm}, ${reg1}, ${reg2}"),
            "CunS");
}

TEST_F(AssemblerMIPSTest, CeqS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CeqS, 3, "c.eq.s $fcc{imm}, ${reg1}, ${reg2}"),
            "CeqS");
}

TEST_F(AssemblerMIPSTest, CueqS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CueqS, 3, "c.ueq.s $fcc{imm}, ${reg1}, ${reg2}"),
            "CueqS");
}

TEST_F(AssemblerMIPSTest, ColtS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::ColtS, 3, "c.olt.s $fcc{imm}, ${reg1}, ${reg2}"),
            "ColtS");
}

TEST_F(AssemblerMIPSTest, CultS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CultS, 3, "c.ult.s $fcc{imm}, ${reg1}, ${reg2}"),
            "CultS");
}

TEST_F(AssemblerMIPSTest, ColeS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::ColeS, 3, "c.ole.s $fcc{imm}, ${reg1}, ${reg2}"),
            "ColeS");
}

TEST_F(AssemblerMIPSTest, CuleS) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CuleS, 3, "c.ule.s $fcc{imm}, ${reg1}, ${reg2}"),
            "CuleS");
}

TEST_F(AssemblerMIPSTest, CunD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CunD, 3, "c.un.d $fcc{imm}, ${reg1}, ${reg2}"),
            "CunD");
}

TEST_F(AssemblerMIPSTest, CeqD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CeqD, 3, "c.eq.d $fcc{imm}, ${reg1}, ${reg2}"),
            "CeqD");
}

TEST_F(AssemblerMIPSTest, CueqD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CueqD, 3, "c.ueq.d $fcc{imm}, ${reg1}, ${reg2}"),
            "CueqD");
}

TEST_F(AssemblerMIPSTest, ColtD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::ColtD, 3, "c.olt.d $fcc{imm}, ${reg1}, ${reg2}"),
            "ColtD");
}

TEST_F(AssemblerMIPSTest, CultD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CultD, 3, "c.ult.d $fcc{imm}, ${reg1}, ${reg2}"),
            "CultD");
}

TEST_F(AssemblerMIPSTest, ColeD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::ColeD, 3, "c.ole.d $fcc{imm}, ${reg1}, ${reg2}"),
            "ColeD");
}

TEST_F(AssemblerMIPSTest, CuleD) {
  DriverStr(RepeatIbFF(&mips::MipsAssembler::CuleD, 3, "c.ule.d $fcc{imm}, ${reg1}, ${reg2}"),
            "CuleD");
}

TEST_F(AssemblerMIPSTest, Movf) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Movf, 3, "movf ${reg1}, ${reg2}, $fcc{imm}"), "Movf");
}

TEST_F(AssemblerMIPSTest, Movt) {
  DriverStr(RepeatRRIb(&mips::MipsAssembler::Movt, 3, "movt ${reg1}, ${reg2}, $fcc{imm}"), "Movt");
}

TEST_F(AssemblerMIPSTest, MovfS) {
  DriverStr(RepeatFFIb(&mips::MipsAssembler::MovfS, 3, "movf.s ${reg1}, ${reg2}, $fcc{imm}"),
            "MovfS");
}

TEST_F(AssemblerMIPSTest, MovfD) {
  DriverStr(RepeatFFIb(&mips::MipsAssembler::MovfD, 3, "movf.d ${reg1}, ${reg2}, $fcc{imm}"),
            "MovfD");
}

TEST_F(AssemblerMIPSTest, MovtS) {
  DriverStr(RepeatFFIb(&mips::MipsAssembler::MovtS, 3, "movt.s ${reg1}, ${reg2}, $fcc{imm}"),
            "MovtS");
}

TEST_F(AssemblerMIPSTest, MovtD) {
  DriverStr(RepeatFFIb(&mips::MipsAssembler::MovtD, 3, "movt.d ${reg1}, ${reg2}, $fcc{imm}"),
            "MovtD");
}

TEST_F(AssemblerMIPSTest, MovzS) {
  DriverStr(RepeatFFR(&mips::MipsAssembler::MovzS, "movz.s ${reg1}, ${reg2}, ${reg3}"), "MovzS");
}

TEST_F(AssemblerMIPSTest, MovzD) {
  DriverStr(RepeatFFR(&mips::MipsAssembler::MovzD, "movz.d ${reg1}, ${reg2}, ${reg3}"), "MovzD");
}

TEST_F(AssemblerMIPSTest, MovnS) {
  DriverStr(RepeatFFR(&mips::MipsAssembler::MovnS, "movn.s ${reg1}, ${reg2}, ${reg3}"), "MovnS");
}

TEST_F(AssemblerMIPSTest, MovnD) {
  DriverStr(RepeatFFR(&mips::MipsAssembler::MovnD, "movn.d ${reg1}, ${reg2}, ${reg3}"), "MovnD");
}

TEST_F(AssemblerMIPSTest, CvtSW) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtsw, "cvt.s.w ${reg1}, ${reg2}"), "CvtSW");
}

TEST_F(AssemblerMIPSTest, CvtDW) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtdw, "cvt.d.w ${reg1}, ${reg2}"), "CvtDW");
}

TEST_F(AssemblerMIPSTest, CvtSL) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtsl, "cvt.s.l ${reg1}, ${reg2}"), "CvtSL");
}

TEST_F(AssemblerMIPSTest, CvtDL) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtdl, "cvt.d.l ${reg1}, ${reg2}"), "CvtDL");
}

TEST_F(AssemblerMIPSTest, CvtSD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtsd, "cvt.s.d ${reg1}, ${reg2}"), "CvtSD");
}

TEST_F(AssemblerMIPSTest, CvtDS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::Cvtds, "cvt.d.s ${reg1}, ${reg2}"), "CvtDS");
}

TEST_F(AssemblerMIPSTest, TruncWS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::TruncWS, "trunc.w.s ${reg1}, ${reg2}"), "TruncWS");
}

TEST_F(AssemblerMIPSTest, TruncWD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::TruncWD, "trunc.w.d ${reg1}, ${reg2}"), "TruncWD");
}

TEST_F(AssemblerMIPSTest, TruncLS) {
  DriverStr(RepeatFF(&mips::MipsAssembler::TruncLS, "trunc.l.s ${reg1}, ${reg2}"), "TruncLS");
}

TEST_F(AssemblerMIPSTest, TruncLD) {
  DriverStr(RepeatFF(&mips::MipsAssembler::TruncLD, "trunc.l.d ${reg1}, ${reg2}"), "TruncLD");
}

TEST_F(AssemblerMIPSTest, Mfc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mfc1, "mfc1 ${reg1}, ${reg2}"), "Mfc1");
}

TEST_F(AssemblerMIPSTest, Mtc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mtc1, "mtc1 ${reg1}, ${reg2}"), "Mtc1");
}

TEST_F(AssemblerMIPSTest, Mfhc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mfhc1, "mfhc1 ${reg1}, ${reg2}"), "Mfhc1");
}

TEST_F(AssemblerMIPSTest, Mthc1) {
  DriverStr(RepeatRF(&mips::MipsAssembler::Mthc1, "mthc1 ${reg1}, ${reg2}"), "Mthc1");
}

TEST_F(AssemblerMIPSTest, Lwc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Lwc1, -16, "lwc1 ${reg1}, {imm}(${reg2})"), "Lwc1");
}

TEST_F(AssemblerMIPSTest, Ldc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Ldc1, -16, "ldc1 ${reg1}, {imm}(${reg2})"), "Ldc1");
}

TEST_F(AssemblerMIPSTest, Swc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Swc1, -16, "swc1 ${reg1}, {imm}(${reg2})"), "Swc1");
}

TEST_F(AssemblerMIPSTest, Sdc1) {
  DriverStr(RepeatFRIb(&mips::MipsAssembler::Sdc1, -16, "sdc1 ${reg1}, {imm}(${reg2})"), "Sdc1");
}

TEST_F(AssemblerMIPSTest, Move) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Move, "or ${reg1}, ${reg2}, $zero"), "Move");
}

TEST_F(AssemblerMIPSTest, Clear) {
  DriverStr(RepeatR(&mips::MipsAssembler::Clear, "or ${reg}, $zero, $zero"), "Clear");
}

TEST_F(AssemblerMIPSTest, Not) {
  DriverStr(RepeatRR(&mips::MipsAssembler::Not, "nor ${reg1}, ${reg2}, $zero"), "Not");
}

TEST_F(AssemblerMIPSTest, Addiu32) {
  __ Addiu32(mips::A1, mips::A2, -0x8000);
  __ Addiu32(mips::A1, mips::A2, +0);
  __ Addiu32(mips::A1, mips::A2, +0x7FFF);
  __ Addiu32(mips::A1, mips::A2, -0x10000);
  __ Addiu32(mips::A1, mips::A2, -0x8001);
  __ Addiu32(mips::A1, mips::A2, +0x8000);
  __ Addiu32(mips::A1, mips::A2, +0xFFFE);
  __ Addiu32(mips::A1, mips::A2, -0x10001);
  __ Addiu32(mips::A1, mips::A2, +0xFFFF);
  __ Addiu32(mips::A1, mips::A2, +0x10000);
  __ Addiu32(mips::A1, mips::A2, +0x10001);
  __ Addiu32(mips::A1, mips::A2, +0x12345678);

  const char* expected =
      "addiu $a1, $a2, -0x8000\n"
      "addiu $a1, $a2, 0\n"
      "addiu $a1, $a2, 0x7FFF\n"
      "addiu $at, $a2, -0x8000\n"
      "addiu $a1, $at, -0x8000\n"
      "addiu $at, $a2, -0x8000\n"
      "addiu $a1, $at, -1\n"
      "addiu $at, $a2, 0x7FFF\n"
      "addiu $a1, $at, 1\n"
      "addiu $at, $a2, 0x7FFF\n"
      "addiu $a1, $at, 0x7FFF\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0xFFFF\n"
      "addu $a1, $a2, $at\n"
      "ori $at, $zero, 0xFFFF\n"
      "addu $a1, $a2, $at\n"
      "lui $at, 1\n"
      "addu $a1, $a2, $at\n"
      "lui $at, 1\n"
      "ori $at, $at, 1\n"
      "addu $a1, $a2, $at\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $a1, $a2, $at\n";
  DriverStr(expected, "Addiu32");
}

TEST_F(AssemblerMIPSTest, LoadFromOffset) {
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x8000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x7FF8);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x7FFB);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x7FFC);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x7FFF);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0xFFF0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x8008);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x8001);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x8000);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0xFFF0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x17FE8);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x17FE8);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x17FF0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, -0x17FE9);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x17FE9);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x17FF0);
  __ LoadFromOffset(mips::kLoadSignedByte, mips::A3, mips::A1, +0x12345678);

  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x7FF8);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x7FFB);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x7FFC);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x7FFF);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0xFFF0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x8008);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x8001);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0xFFF0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x17FE8);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x17FE8);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x17FF0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, -0x17FE9);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x17FE9);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x17FF0);
  __ LoadFromOffset(mips::kLoadUnsignedByte, mips::A3, mips::A1, +0x12345678);

  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x8000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x7FF8);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x7FFB);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x7FFC);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x7FFF);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0xFFF0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x8008);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x8001);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x8000);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0xFFF0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x17FE8);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x17FE8);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x17FF0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, -0x17FE9);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x17FE9);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x17FF0);
  __ LoadFromOffset(mips::kLoadSignedHalfword, mips::A3, mips::A1, +0x12345678);

  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x7FF8);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x7FFB);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x7FFC);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x7FFF);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0xFFF0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x8008);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x8001);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x8000);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0xFFF0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x17FE8);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x17FE8);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x17FF0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, -0x17FE9);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x17FE9);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x17FF0);
  __ LoadFromOffset(mips::kLoadUnsignedHalfword, mips::A3, mips::A1, +0x12345678);

  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x8000);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x7FF8);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x7FFB);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x7FFC);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x7FFF);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0xFFF0);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x8008);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x8001);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x8000);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0xFFF0);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x17FE8);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x17FE8);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x17FF0);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, -0x17FE9);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x17FE9);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x17FF0);
  __ LoadFromOffset(mips::kLoadWord, mips::A3, mips::A1, +0x12345678);

  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x8000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x7FF8);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x7FFB);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x7FFC);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x7FFF);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0xFFF0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x8008);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x8001);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x8000);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0xFFF0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x17FE8);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x0FFF8);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x0FFF1);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x0FFF1);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x0FFF8);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x17FE8);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x17FF0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, -0x17FE9);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x17FE9);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x17FF0);
  __ LoadFromOffset(mips::kLoadDoubleword, mips::A0, mips::A2, +0x12345678);

  const char* expected =
      "lb $a3, -0x8000($a1)\n"
      "lb $a3, 0($a1)\n"
      "lb $a3, 0x7FF8($a1)\n"
      "lb $a3, 0x7FFB($a1)\n"
      "lb $a3, 0x7FFC($a1)\n"
      "lb $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lb $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lb $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lb $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lb $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lb $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lb $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lb $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lb $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lb $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lb $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lb $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lb $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lb $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "lb $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "lb $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lb $a3, 0($at)\n"

      "lbu $a3, -0x8000($a1)\n"
      "lbu $a3, 0($a1)\n"
      "lbu $a3, 0x7FF8($a1)\n"
      "lbu $a3, 0x7FFB($a1)\n"
      "lbu $a3, 0x7FFC($a1)\n"
      "lbu $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lbu $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lbu $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lbu $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lbu $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lbu $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lbu $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lbu $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lbu $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lbu $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lbu $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lbu $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lbu $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lbu $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "lbu $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "lbu $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lbu $a3, 0($at)\n"

      "lh $a3, -0x8000($a1)\n"
      "lh $a3, 0($a1)\n"
      "lh $a3, 0x7FF8($a1)\n"
      "lh $a3, 0x7FFB($a1)\n"
      "lh $a3, 0x7FFC($a1)\n"
      "lh $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lh $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lh $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lh $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lh $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lh $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lh $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lh $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lh $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lh $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lh $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lh $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lh $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lh $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "lh $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "lh $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lh $a3, 0($at)\n"

      "lhu $a3, -0x8000($a1)\n"
      "lhu $a3, 0($a1)\n"
      "lhu $a3, 0x7FF8($a1)\n"
      "lhu $a3, 0x7FFB($a1)\n"
      "lhu $a3, 0x7FFC($a1)\n"
      "lhu $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lhu $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lhu $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lhu $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lhu $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lhu $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lhu $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lhu $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lhu $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lhu $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lhu $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lhu $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lhu $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lhu $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "lhu $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "lhu $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lhu $a3, 0($at)\n"

      "lw $a3, -0x8000($a1)\n"
      "lw $a3, 0($a1)\n"
      "lw $a3, 0x7FF8($a1)\n"
      "lw $a3, 0x7FFB($a1)\n"
      "lw $a3, 0x7FFC($a1)\n"
      "lw $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lw $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lw $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "lw $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lw $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lw $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lw $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "lw $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "lw $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "lw $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "lw $a3, 0($at)\n"

      "lw $a0, -0x8000($a2)\n"
      "lw $a1, -0x7FFC($a2)\n"
      "lw $a0, 0($a2)\n"
      "lw $a1, 4($a2)\n"
      "lw $a0, 0x7FF8($a2)\n"
      "lw $a1, 0x7FFC($a2)\n"
      "lw $a0, 0x7FFB($a2)\n"
      "lw $a1, 0x7FFF($a2)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "lw $a0, 4($at)\n"
      "lw $a1, 8($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "lw $a0, 7($at)\n"
      "lw $a1, 11($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "lw $a0, -0x7FF8($at)\n"
      "lw $a1, -0x7FF4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "lw $a0, -0x10($at)\n"
      "lw $a1, -0xC($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "lw $a0, -9($at)\n"
      "lw $a1, -5($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "lw $a0, 8($at)\n"
      "lw $a1, 12($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "lw $a0, 0x7FF8($at)\n"
      "lw $a1, 0x7FFC($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a0, -0x7FF8($at)\n"
      "lw $a1, -0x7FF4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a0, -8($at)\n"
      "lw $a1, -4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lw $a0, -1($at)\n"
      "lw $a1, 3($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a0, 1($at)\n"
      "lw $a1, 5($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a0, 8($at)\n"
      "lw $a1, 12($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lw $a0, 0x7FF8($at)\n"
      "lw $a1, 0x7FFC($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 7($at)\n"
      "lw $a1, 11($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 1($at)\n"
      "lw $a1, 5($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a2\n"
      "lw $a0, 0($at)\n"
      "lw $a1, 4($at)\n";
  DriverStr(expected, "LoadFromOffset");
}

TEST_F(AssemblerMIPSTest, LoadSFromOffset) {
  __ LoadSFromOffset(mips::F2, mips::A0, -0x8000);
  __ LoadSFromOffset(mips::F2, mips::A0, +0);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x7FF8);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x7FFB);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x7FFC);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x7FFF);
  __ LoadSFromOffset(mips::F2, mips::A0, -0xFFF0);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x8008);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x8001);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x8000);
  __ LoadSFromOffset(mips::F2, mips::A0, +0xFFF0);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x17FE8);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x0FFF8);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x0FFF1);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x0FFF1);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x0FFF8);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x17FE8);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x17FF0);
  __ LoadSFromOffset(mips::F2, mips::A0, -0x17FE9);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x17FE9);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x17FF0);
  __ LoadSFromOffset(mips::F2, mips::A0, +0x12345678);

  const char* expected =
      "lwc1 $f2, -0x8000($a0)\n"
      "lwc1 $f2, 0($a0)\n"
      "lwc1 $f2, 0x7FF8($a0)\n"
      "lwc1 $f2, 0x7FFB($a0)\n"
      "lwc1 $f2, 0x7FFC($a0)\n"
      "lwc1 $f2, 0x7FFF($a0)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "lwc1 $f2, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "lwc1 $f2, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "lwc1 $f2, -9($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f2, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f2, 0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lwc1 $f2, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lwc1 $f2, -8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lwc1 $f2, -1($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lwc1 $f2, 1($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lwc1 $f2, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lwc1 $f2, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f2, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f2, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f2, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f2, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f2, 0($at)\n";
  DriverStr(expected, "LoadSFromOffset");
}

TEST_F(AssemblerMIPSTest, LoadDFromOffset) {
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
      "lwc1 $f1, 0x7FFF($a0)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f0, 4($at)\n"
      "lwc1 $f1, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "lwc1 $f0, 7($at)\n"
      "lwc1 $f1, 11($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "ldc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "ldc1 $f0, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "lwc1 $f0, -9($at)\n"
      "lwc1 $f1, -5($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "ldc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "ldc1 $f0, 0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "ldc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "ldc1 $f0, -8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "lwc1 $f0, -1($at)\n"
      "lwc1 $f1, 3($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "lwc1 $f0, 1($at)\n"
      "lwc1 $f1, 5($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "ldc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "ldc1 $f0, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 7($at)\n"
      "lwc1 $f1, 11($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a0\n"
      "lwc1 $f0, 1($at)\n"
      "lwc1 $f1, 5($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "ldc1 $f0, 0($at)\n";
  DriverStr(expected, "LoadDFromOffset");
}

TEST_F(AssemblerMIPSTest, StoreToOffset) {
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x8000);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x7FF8);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x7FFB);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x7FFC);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x7FFF);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0xFFF0);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x8008);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x8001);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x8000);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0xFFF0);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x17FE8);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x0FFF8);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x0FFF1);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x0FFF1);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x0FFF8);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x17FE8);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x17FF0);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, -0x17FE9);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x17FE9);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x17FF0);
  __ StoreToOffset(mips::kStoreByte, mips::A3, mips::A1, +0x12345678);

  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x8000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x7FF8);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x7FFB);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x7FFC);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x7FFF);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0xFFF0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x8008);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x8001);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x8000);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0xFFF0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x17FE8);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x0FFF8);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x0FFF1);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x0FFF1);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x0FFF8);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x17FE8);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x17FF0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, -0x17FE9);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x17FE9);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x17FF0);
  __ StoreToOffset(mips::kStoreHalfword, mips::A3, mips::A1, +0x12345678);

  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x8000);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x7FF8);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x7FFB);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x7FFC);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x7FFF);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0xFFF0);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x8008);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x8001);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x8000);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0xFFF0);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x17FE8);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x0FFF8);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x0FFF1);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x0FFF1);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x0FFF8);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x17FE8);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x17FF0);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, -0x17FE9);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x17FE9);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x17FF0);
  __ StoreToOffset(mips::kStoreWord, mips::A3, mips::A1, +0x12345678);

  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x8000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x7FF8);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x7FFB);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x7FFC);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x7FFF);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0xFFF0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x8008);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x8001);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x8000);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0xFFF0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x17FE8);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x0FFF8);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x0FFF1);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x0FFF1);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x0FFF8);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x17FE8);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x17FF0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, -0x17FE9);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x17FE9);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x17FF0);
  __ StoreToOffset(mips::kStoreDoubleword, mips::A0, mips::A2, +0x12345678);

  const char* expected =
      "sb $a3, -0x8000($a1)\n"
      "sb $a3, 0($a1)\n"
      "sb $a3, 0x7FF8($a1)\n"
      "sb $a3, 0x7FFB($a1)\n"
      "sb $a3, 0x7FFC($a1)\n"
      "sb $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sb $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sb $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sb $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sb $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sb $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sb $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sb $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sb $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sb $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sb $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sb $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sb $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sb $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "sb $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "sb $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sb $a3, 0($at)\n"

      "sh $a3, -0x8000($a1)\n"
      "sh $a3, 0($a1)\n"
      "sh $a3, 0x7FF8($a1)\n"
      "sh $a3, 0x7FFB($a1)\n"
      "sh $a3, 0x7FFC($a1)\n"
      "sh $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sh $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sh $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sh $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sh $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sh $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sh $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sh $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sh $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sh $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sh $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sh $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sh $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sh $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "sh $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "sh $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sh $a3, 0($at)\n"

      "sw $a3, -0x8000($a1)\n"
      "sw $a3, 0($a1)\n"
      "sw $a3, 0x7FF8($a1)\n"
      "sw $a3, 0x7FFB($a1)\n"
      "sw $a3, 0x7FFC($a1)\n"
      "sw $a3, 0x7FFF($a1)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sw $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sw $a3, -0x10($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "sw $a3, -9($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sw $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "sw $a3, 0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a3, -0x7FF8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a3, -8($at)\n"
      "addiu $at, $a1, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a3, -1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a3, 1($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a3, 8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a3, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sw $a3, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a1\n"
      "sw $a3, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a1\n"
      "sw $a3, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a1\n"
      "sw $a3, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a1\n"
      "sw $a3, 0($at)\n"

      "sw $a0, -0x8000($a2)\n"
      "sw $a1, -0x7FFC($a2)\n"
      "sw $a0, 0($a2)\n"
      "sw $a1, 4($a2)\n"
      "sw $a0, 0x7FF8($a2)\n"
      "sw $a1, 0x7FFC($a2)\n"
      "sw $a0, 0x7FFB($a2)\n"
      "sw $a1, 0x7FFF($a2)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "sw $a0, 4($at)\n"
      "sw $a1, 8($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "sw $a0, 7($at)\n"
      "sw $a1, 11($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "sw $a0, -0x7FF8($at)\n"
      "sw $a1, -0x7FF4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "sw $a0, -0x10($at)\n"
      "sw $a1, -0xC($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "sw $a0, -9($at)\n"
      "sw $a1, -5($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "sw $a0, 8($at)\n"
      "sw $a1, 12($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "sw $a0, 0x7FF8($at)\n"
      "sw $a1, 0x7FFC($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a0, -0x7FF8($at)\n"
      "sw $a1, -0x7FF4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a0, -8($at)\n"
      "sw $a1, -4($at)\n"
      "addiu $at, $a2, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sw $a0, -1($at)\n"
      "sw $a1, 3($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a0, 1($at)\n"
      "sw $a1, 5($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a0, 8($at)\n"
      "sw $a1, 12($at)\n"
      "addiu $at, $a2, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sw $a0, 0x7FF8($at)\n"
      "sw $a1, 0x7FFC($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 7($at)\n"
      "sw $a1, 11($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 1($at)\n"
      "sw $a1, 5($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a2\n"
      "sw $a0, 0($at)\n"
      "sw $a1, 4($at)\n";
  DriverStr(expected, "StoreToOffset");
}

TEST_F(AssemblerMIPSTest, StoreSToOffset) {
  __ StoreSToOffset(mips::F2, mips::A0, -0x8000);
  __ StoreSToOffset(mips::F2, mips::A0, +0);
  __ StoreSToOffset(mips::F2, mips::A0, +0x7FF8);
  __ StoreSToOffset(mips::F2, mips::A0, +0x7FFB);
  __ StoreSToOffset(mips::F2, mips::A0, +0x7FFC);
  __ StoreSToOffset(mips::F2, mips::A0, +0x7FFF);
  __ StoreSToOffset(mips::F2, mips::A0, -0xFFF0);
  __ StoreSToOffset(mips::F2, mips::A0, -0x8008);
  __ StoreSToOffset(mips::F2, mips::A0, -0x8001);
  __ StoreSToOffset(mips::F2, mips::A0, +0x8000);
  __ StoreSToOffset(mips::F2, mips::A0, +0xFFF0);
  __ StoreSToOffset(mips::F2, mips::A0, -0x17FE8);
  __ StoreSToOffset(mips::F2, mips::A0, -0x0FFF8);
  __ StoreSToOffset(mips::F2, mips::A0, -0x0FFF1);
  __ StoreSToOffset(mips::F2, mips::A0, +0x0FFF1);
  __ StoreSToOffset(mips::F2, mips::A0, +0x0FFF8);
  __ StoreSToOffset(mips::F2, mips::A0, +0x17FE8);
  __ StoreSToOffset(mips::F2, mips::A0, -0x17FF0);
  __ StoreSToOffset(mips::F2, mips::A0, -0x17FE9);
  __ StoreSToOffset(mips::F2, mips::A0, +0x17FE9);
  __ StoreSToOffset(mips::F2, mips::A0, +0x17FF0);
  __ StoreSToOffset(mips::F2, mips::A0, +0x12345678);

  const char* expected =
      "swc1 $f2, -0x8000($a0)\n"
      "swc1 $f2, 0($a0)\n"
      "swc1 $f2, 0x7FF8($a0)\n"
      "swc1 $f2, 0x7FFB($a0)\n"
      "swc1 $f2, 0x7FFC($a0)\n"
      "swc1 $f2, 0x7FFF($a0)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "swc1 $f2, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "swc1 $f2, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "swc1 $f2, -9($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "swc1 $f2, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "swc1 $f2, 0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "swc1 $f2, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "swc1 $f2, -8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "swc1 $f2, -1($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "swc1 $f2, 1($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "swc1 $f2, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "swc1 $f2, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "swc1 $f2, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "swc1 $f2, 7($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a0\n"
      "swc1 $f2, 1($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a0\n"
      "swc1 $f2, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "swc1 $f2, 0($at)\n";
  DriverStr(expected, "StoreSToOffset");
}

TEST_F(AssemblerMIPSTest, StoreDToOffset) {
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
      "swc1 $f0, 0x7FFB($a0)\n"
      "swc1 $f1, 0x7FFF($a0)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "swc1 $f0, 4($at)\n"
      "swc1 $f1, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "swc1 $f0, 7($at)\n"
      "swc1 $f1, 11($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "sdc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "sdc1 $f0, -0x10($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "swc1 $f0, -9($at)\n"
      "swc1 $f1, -5($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "sdc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "sdc1 $f0, 0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sdc1 $f0, -0x7FF8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "sdc1 $f0, -8($at)\n"
      "addiu $at, $a0, -0x7FF8\n"
      "addiu $at, $at, -0x7FF8\n"
      "swc1 $f0, -1($at)\n"
      "swc1 $f1, 3($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "swc1 $f0, 1($at)\n"
      "swc1 $f1, 5($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sdc1 $f0, 8($at)\n"
      "addiu $at, $a0, 0x7FF8\n"
      "addiu $at, $at, 0x7FF8\n"
      "sdc1 $f0, 0x7FF8($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "lui $at, 0xFFFE\n"
      "ori $at, $at, 0x8010\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 7($at)\n"
      "swc1 $f1, 11($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FE8\n"
      "addu $at, $at, $a0\n"
      "swc1 $f0, 1($at)\n"
      "swc1 $f1, 5($at)\n"
      "lui $at, 0x1\n"
      "ori $at, $at, 0x7FF0\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "addu $at, $at, $a0\n"
      "sdc1 $f0, 0($at)\n";
  DriverStr(expected, "StoreDToOffset");
}

TEST_F(AssemblerMIPSTest, StoreConstToOffset) {
  __ StoreConstToOffset(mips::kStoreByte, 0xFF, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreHalfword, 0xFFFF, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreWord, 0x12345678, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreDoubleword, 0x123456789ABCDEF0, mips::A1, +0, mips::T8);

  __ StoreConstToOffset(mips::kStoreByte, 0, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreHalfword, 0, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreWord, 0, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreDoubleword, 0, mips::A1, +0, mips::T8);

  __ StoreConstToOffset(mips::kStoreDoubleword, 0x1234567812345678, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreDoubleword, 0x1234567800000000, mips::A1, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreDoubleword, 0x0000000012345678, mips::A1, +0, mips::T8);

  __ StoreConstToOffset(mips::kStoreWord, 0, mips::T8, +0, mips::T8);
  __ StoreConstToOffset(mips::kStoreWord, 0x12345678, mips::T8, +0, mips::T8);

  __ StoreConstToOffset(mips::kStoreWord, 0, mips::A1, -0xFFF0, mips::T8);
  __ StoreConstToOffset(mips::kStoreWord, 0x12345678, mips::A1, +0xFFF0, mips::T8);

  __ StoreConstToOffset(mips::kStoreWord, 0, mips::T8, -0xFFF0, mips::T8);
  __ StoreConstToOffset(mips::kStoreWord, 0x12345678, mips::T8, +0xFFF0, mips::T8);

  const char* expected =
      "ori $t8, $zero, 0xFF\n"
      "sb $t8, 0($a1)\n"
      "ori $t8, $zero, 0xFFFF\n"
      "sh $t8, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 0($a1)\n"
      "lui $t8, 0x9ABC\n"
      "ori $t8, $t8, 0xDEF0\n"
      "sw $t8, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 4($a1)\n"

      "sb $zero, 0($a1)\n"
      "sh $zero, 0($a1)\n"
      "sw $zero, 0($a1)\n"
      "sw $zero, 0($a1)\n"
      "sw $zero, 4($a1)\n"

      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 0($a1)\n"
      "sw $t8, 4($a1)\n"
      "sw $zero, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 4($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 0($a1)\n"
      "sw $zero, 4($a1)\n"

      "sw $zero, 0($t8)\n"
      "lui $at, 0x1234\n"
      "ori $at, $at, 0x5678\n"
      "sw $at, 0($t8)\n"

      "addiu $at, $a1, -0x7FF8\n"
      "sw $zero, -0x7FF8($at)\n"
      "addiu $at, $a1, 0x7FF8\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 0x7FF8($at)\n"

      "addiu $at, $t8, -0x7FF8\n"
      "sw $zero, -0x7FF8($at)\n"
      "addiu $at, $t8, 0x7FF8\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sw $t8, 0x7FF8($at)\n";
  DriverStr(expected, "StoreConstToOffset");
}

//////////////
// BRANCHES //
//////////////

TEST_F(AssemblerMIPSTest, B) {
  BranchHelper(&mips::MipsAssembler::B, "B");
}

TEST_F(AssemblerMIPSTest, Bal) {
  BranchHelper(&mips::MipsAssembler::Bal, "Bal");
}

TEST_F(AssemblerMIPSTest, Beq) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beq, "Beq");
}

TEST_F(AssemblerMIPSTest, Bne) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bne, "Bne");
}

TEST_F(AssemblerMIPSTest, Beqz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqz, "Beqz");
}

TEST_F(AssemblerMIPSTest, Bnez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnez, "Bnez");
}

TEST_F(AssemblerMIPSTest, Bltz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltz, "Bltz");
}

TEST_F(AssemblerMIPSTest, Bgez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgez, "Bgez");
}

TEST_F(AssemblerMIPSTest, Blez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blez, "Blez");
}

TEST_F(AssemblerMIPSTest, Bgtz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtz, "Bgtz");
}

TEST_F(AssemblerMIPSTest, Blt) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Blt, "Blt");
}

TEST_F(AssemblerMIPSTest, Bge) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bge, "Bge");
}

TEST_F(AssemblerMIPSTest, Bltu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltu, "Bltu");
}

TEST_F(AssemblerMIPSTest, Bgeu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeu, "Bgeu");
}

TEST_F(AssemblerMIPSTest, Bc1f) {
  BranchFpuCondCodeHelper(&mips::MipsAssembler::Bc1f, "Bc1f");
}

TEST_F(AssemblerMIPSTest, Bc1t) {
  BranchFpuCondCodeHelper(&mips::MipsAssembler::Bc1t, "Bc1t");
}

TEST_F(AssemblerMIPSTest, BareB) {
  BranchHelper(&mips::MipsAssembler::B, "B", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBal) {
  BranchHelper(&mips::MipsAssembler::Bal, "Bal", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBeq) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Beq, "Beq", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBne) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bne, "Bne", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBeqz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Beqz, "Beqz", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBnez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bnez, "Bnez", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBltz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bltz, "Bltz", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBgez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgez, "Bgez", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBlez) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Blez, "Blez", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBgtz) {
  BranchCondOneRegHelper(&mips::MipsAssembler::Bgtz, "Bgtz", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBlt) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Blt, "Blt", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBge) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bge, "Bge", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBltu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bltu, "Bltu", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBgeu) {
  BranchCondTwoRegsHelper(&mips::MipsAssembler::Bgeu, "Bgeu", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBc1f) {
  BranchFpuCondCodeHelper(&mips::MipsAssembler::Bc1f, "Bc1f", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, BareBc1t) {
  BranchFpuCondCodeHelper(&mips::MipsAssembler::Bc1t, "Bc1t", /* is_bare */ true);
}

TEST_F(AssemblerMIPSTest, ImpossibleReordering) {
  mips::MipsLabel label1, label2;
  __ SetReorder(true);

  __ B(&label1);  // No preceding or target instruction for the delay slot.

  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bind(&label1);
  __ B(&label1);  // The preceding label prevents moving Addu into the delay slot.
  __ B(&label1);  // No preceding or target instruction for the delay slot.

  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Beqz(mips::T0, &label1);  // T0 dependency.

  __ Or(mips::T1, mips::T2, mips::T3);
  __ Bne(mips::T2, mips::T1, &label1);  // T1 dependency.

  __ And(mips::T0, mips::T1, mips::T2);
  __ Blt(mips::T1, mips::T0, &label1);  // T0 dependency.

  __ Xor(mips::AT, mips::T0, mips::T1);
  __ Bge(mips::T1, mips::T0, &label1);  // AT dependency.

  __ Subu(mips::T0, mips::T1, mips::AT);
  __ Bltu(mips::T1, mips::T0, &label1);  // AT dependency.

  __ ColtS(1, mips::F2, mips::F4);
  __ Bc1t(1, &label1);  // cc1 dependency.

  __ Move(mips::T0, mips::RA);
  __ Bal(&label1);  // RA dependency.

  __ Lw(mips::RA, mips::T0, 0);
  __ Bal(&label1);  // RA dependency.

  __ LlR2(mips::T9, mips::T0, 0);
  __ Jalr(mips::T9);  // T9 dependency.

  __ Sw(mips::RA, mips::T0, 0);
  __ Jalr(mips::T9);  // RA dependency.

  __ Lw(mips::T1, mips::T0, 0);
  __ Jalr(mips::T1, mips::T9);  // T1 dependency.

  __ ScR2(mips::T9, mips::T0, 0);
  __ Jr(mips::T9);  // T9 dependency.

  __ Bind(&label2);

  __ Bnez(mips::T0, &label2);  // No preceding instruction for the delay slot.

  __ Bgeu(mips::T1, mips::T0, &label2);  // No preceding instruction for the delay slot.

  __ Bc1f(2, &label2);  // No preceding instruction for the delay slot.

  __ Bal(&label2);  // No preceding instruction for the delay slot.

  __ Jalr(mips::T9);  // No preceding instruction for the delay slot.

  __ Addu(mips::T0, mips::T1, mips::T2);
  __ CodePosition();  // Drops the delay slot candidate (the last instruction).
  __ Beq(mips::T1, mips::T2, &label2);  // No preceding or target instruction for the delay slot.

  std::string expected =
      ".set noreorder\n"
      "b 1f\n"
      "nop\n"

      "addu $t0, $t1, $t2\n"
      "1:\n"
      "b 1b\n"
      "nop\n"
      "b 1b\n"
      "nop\n"

      "addu $t0, $t1, $t2\n"
      "beqz $t0, 1b\n"
      "nop\n"

      "or $t1, $t2, $t3\n"
      "bne $t2, $t1, 1b\n"
      "nop\n"

      "and $t0, $t1, $t2\n"
      "slt $at, $t1, $t0\n"
      "bnez $at, 1b\n"
      "nop\n"

      "xor $at, $t0, $t1\n"
      "slt $at, $t1, $t0\n"
      "beqz $at, 1b\n"
      "nop\n"

      "subu $t0, $t1, $at\n"
      "sltu $at, $t1, $t0\n"
      "bnez $at, 1b\n"
      "nop\n"

      "c.olt.s $fcc1, $f2, $f4\n"
      "bc1t $fcc1, 1b\n"
      "nop\n"

      "or $t0, $ra, $zero\n"
      "bal 1b\n"
      "nop\n"

      "lw $ra, 0($t0)\n"
      "bal 1b\n"
      "nop\n"

      "ll $t9, 0($t0)\n"
      "jalr $t9\n"
      "nop\n"

      "sw $ra, 0($t0)\n"
      "jalr $t9\n"
      "nop\n"

      "lw $t1, 0($t0)\n"
      "jalr $t1, $t9\n"
      "nop\n"

      "sc $t9, 0($t0)\n"
      "jalr $zero, $t9\n"
      "nop\n"

      "2:\n"

      "bnez $t0, 2b\n"
      "nop\n"

      "sltu $at, $t1, $t0\n"
      "beqz $at, 2b\n"
      "nop\n"

      "bc1f $fcc2, 2b\n"
      "nop\n"

      "bal 2b\n"
      "nop\n"

      "jalr $t9\n"
      "nop\n"

      "addu $t0, $t1, $t2\n"
      "beq $t1, $t2, 2b\n"
      "nop\n";
  DriverStr(expected, "ImpossibleReordering");
}

TEST_F(AssemblerMIPSTest, Reordering) {
  mips::MipsLabel label1, label2;
  __ SetReorder(true);

  __ Bind(&label1);
  __ Bind(&label2);

  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Beqz(mips::T1, &label1);

  __ Or(mips::T1, mips::T2, mips::T3);
  __ Bne(mips::T2, mips::T3, &label1);

  __ And(mips::T0, mips::T1, mips::T2);
  __ Blt(mips::T1, mips::T2, &label1);

  __ Xor(mips::T2, mips::T0, mips::T1);
  __ Bge(mips::T1, mips::T0, &label1);

  __ Subu(mips::T2, mips::T1, mips::T0);
  __ Bltu(mips::T1, mips::T0, &label1);

  __ ColtS(0, mips::F2, mips::F4);
  __ Bc1t(1, &label1);

  __ Move(mips::T0, mips::T1);
  __ Bal(&label1);

  __ LlR2(mips::T1, mips::T0, 0);
  __ Jalr(mips::T9);

  __ ScR2(mips::T1, mips::T0, 0);
  __ Jr(mips::T9);

  std::string expected =
      ".set noreorder\n"
      "1:\n"

      "beqz $t1, 1b\n"
      "addu $t0, $t1, $t2\n"

      "bne $t2, $t3, 1b\n"
      "or $t1, $t2, $t3\n"

      "slt $at, $t1, $t2\n"
      "bnez $at, 1b\n"
      "and $t0, $t1, $t2\n"

      "slt $at, $t1, $t0\n"
      "beqz $at, 1b\n"
      "xor $t2, $t0, $t1\n"

      "sltu $at, $t1, $t0\n"
      "bnez $at, 1b\n"
      "subu $t2, $t1, $t0\n"

      "bc1t $fcc1, 1b\n"
      "c.olt.s $fcc0, $f2, $f4\n"

      "bal 1b\n"
      "or $t0, $t1, $zero\n"

      "jalr $t9\n"
      "ll $t1, 0($t0)\n"

      "jalr $zero, $t9\n"
      "sc $t1, 0($t0)\n";
  DriverStr(expected, "Reordering");
}

TEST_F(AssemblerMIPSTest, AbsorbTargetInstruction) {
  mips::MipsLabel label1, label2, label3, label4, label5, label6;
  mips::MipsLabel label7, label8, label9, label10, label11, label12, label13;
  __ SetReorder(true);

  __ B(&label1);
  __ Bind(&label1);
  __ Addu(mips::T0, mips::T1, mips::T2);

  __ Bind(&label2);
  __ Xor(mips::T0, mips::T1, mips::T2);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bind(&label3);  // Prevents reordering ADDU above with B below.
  __ B(&label2);

  __ B(&label4);
  __ Bind(&label4);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ CodePosition();  // Prevents absorbing ADDU above.

  __ B(&label5);
  __ Bind(&label5);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ Bind(&label6);
  __ CodePosition();  // Even across Bind(), CodePosition() prevents absorbing the ADDU above.

  __ Nop();
  __ B(&label7);
  __ Bind(&label7);
  __ Lw(mips::V0, mips::A0, 0x5678);  // Possibly patchable instruction, not absorbed.

  __ Nop();
  __ B(&label8);
  __ Bind(&label8);
  __ Sw(mips::V0, mips::A0, 0x5678);  // Possibly patchable instruction, not absorbed.

  __ Nop();
  __ B(&label9);
  __ Bind(&label9);
  __ Addiu(mips::V0, mips::A0, 0x5678);  // Possibly patchable instruction, not absorbed.

  __ Nop();
  __ B(&label10);
  __ Bind(&label10);
  __ Lw(mips::V0, mips::A0, 0x5680);  // Immediate isn't 0x5678, absorbed.

  __ Nop();
  __ B(&label11);
  __ Bind(&label11);
  __ Sw(mips::V0, mips::A0, 0x5680);  // Immediate isn't 0x5678, absorbed.

  __ Nop();
  __ B(&label12);
  __ Bind(&label12);
  __ Addiu(mips::V0, mips::A0, 0x5680);  // Immediate isn't 0x5678, absorbed.

  __ Nop();
  __ B(&label13);
  __ Bind(&label13);
  __ Andi(mips::V0, mips::A0, 0x5678);  // Not one of patchable instructions, absorbed.

  std::string expected =
      ".set noreorder\n"
      "b 1f\n"
      "addu $t0, $t1, $t2\n"
      "addu $t0, $t1, $t2\n"
      "1:\n"

      "xor $t0, $t1, $t2\n"
      "2:\n"
      "addu $t0, $t1, $t2\n"
      "b 2b\n"
      "xor $t0, $t1, $t2\n"

      "b 4f\n"
      "nop\n"
      "4:\n"
      "addu $t0, $t1, $t2\n"

      "b 5f\n"
      "nop\n"
      "5:\n"
      "addu $t0, $t1, $t2\n"

      "nop\n"
      "b 7f\n"
      "nop\n"
      "7:\n"
      "lw $v0, 0x5678($a0)\n"

      "nop\n"
      "b 8f\n"
      "nop\n"
      "8:\n"
      "sw $v0, 0x5678($a0)\n"

      "nop\n"
      "b 9f\n"
      "nop\n"
      "9:\n"
      "addiu $v0, $a0, 0x5678\n"

      "nop\n"
      "b 10f\n"
      "lw $v0, 0x5680($a0)\n"
      "lw $v0, 0x5680($a0)\n"
      "10:\n"

      "nop\n"
      "b 11f\n"
      "sw $v0, 0x5680($a0)\n"
      "sw $v0, 0x5680($a0)\n"
      "11:\n"

      "nop\n"
      "b 12f\n"
      "addiu $v0, $a0, 0x5680\n"
      "addiu $v0, $a0, 0x5680\n"
      "12:\n"

      "nop\n"
      "b 13f\n"
      "andi $v0, $a0, 0x5678\n"
      "andi $v0, $a0, 0x5678\n"
      "13:\n";
  DriverStr(expected, "AbsorbTargetInstruction");
}

TEST_F(AssemblerMIPSTest, SetReorder) {
  mips::MipsLabel label1, label2, label3, label4, label5, label6;

  __ SetReorder(true);
  __ Bind(&label1);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ B(&label1);
  __ B(&label5);
  __ B(&label6);

  __ SetReorder(false);
  __ Bind(&label2);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ B(&label2);
  __ B(&label5);
  __ B(&label6);

  __ SetReorder(true);
  __ Bind(&label3);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ B(&label3);
  __ B(&label5);
  __ B(&label6);

  __ SetReorder(false);
  __ Bind(&label4);
  __ Addu(mips::T0, mips::T1, mips::T2);
  __ B(&label4);
  __ B(&label5);
  __ B(&label6);

  __ SetReorder(true);
  __ Bind(&label5);
  __ Subu(mips::T0, mips::T1, mips::T2);

  __ SetReorder(false);
  __ Bind(&label6);
  __ Xor(mips::T0, mips::T1, mips::T2);

  std::string expected =
      ".set noreorder\n"
      "1:\n"
      "b 1b\n"
      "addu $t0, $t1, $t2\n"
      "b 55f\n"
      "subu $t0, $t1, $t2\n"
      "b 6f\n"
      "nop\n"

      "2:\n"
      "addu $t0, $t1, $t2\n"
      "b 2b\n"
      "nop\n"
      "b 5f\n"
      "nop\n"
      "b 6f\n"
      "nop\n"

      "3:\n"
      "b 3b\n"
      "addu $t0, $t1, $t2\n"
      "b 55f\n"
      "subu $t0, $t1, $t2\n"
      "b 6f\n"
      "nop\n"

      "4:\n"
      "addu $t0, $t1, $t2\n"
      "b 4b\n"
      "nop\n"
      "b 5f\n"
      "nop\n"
      "b 6f\n"
      "nop\n"

      "5:\n"
      "subu $t0, $t1, $t2\n"
      "55:\n"
      "6:\n"
      "xor $t0, $t1, $t2\n";
  DriverStr(expected, "SetReorder");
}

TEST_F(AssemblerMIPSTest, ReorderPatchedInstruction) {
  __ SetReorder(true);
  mips::MipsLabel label1, label2;
  mips::MipsLabel patcher_label1, patcher_label2, patcher_label3, patcher_label4, patcher_label5;
  __ Lw(mips::V0, mips::A0, 0x5678, &patcher_label1);
  __ Beq(mips::A0, mips::A1, &label1);
  constexpr uint32_t kAdduCount1 = 63;
  for (size_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label1);
  __ Sw(mips::V0, mips::A0, 0x5678, &patcher_label2);
  __ Bltz(mips::V1, &label2);
  constexpr uint32_t kAdduCount2 = 64;
  for (size_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label2);
  __ Addiu(mips::V0, mips::A0, 0x5678, &patcher_label3);
  __ B(&label1);
  __ Lw(mips::V0, mips::A0, 0x5678, &patcher_label4);
  __ Jalr(mips::T9);
  __ Sw(mips::V0, mips::A0, 0x5678, &patcher_label5);
  __ Blt(mips::V0, mips::V1, &label2);
  __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);

  std::string expected =
      ".set noreorder\n"
      "beq $a0, $a1, 1f\n"
      "lw $v0, 0x5678($a0)\n" +
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
      "1:\n"
      "bltz $v1, 2f\n"
      "sw $v0, 0x5678($a0)\n" +
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
      "2:\n"
      "b 1b\n"
      "addiu $v0, $a0, 0x5678\n"
      "jalr $t9\n"
      "lw $v0, 0x5678($a0)\n"
      "slt $at, $v0, $v1\n"
      "bnez $at, 2b\n"
      "sw $v0, 0x5678($a0)\n"
      "addu $zero, $zero, $zero\n";
  DriverStr(expected, "ReorderPatchedInstruction");
  EXPECT_EQ(__ GetLabelLocation(&patcher_label1), 1 * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label2), (kAdduCount1 + 3) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label3), (kAdduCount1 + kAdduCount2 + 5) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label4), (kAdduCount1 + kAdduCount2 + 7) * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label5), (kAdduCount1 + kAdduCount2 + 10) * 4u);
}

TEST_F(AssemblerMIPSTest, LongBranchReorder) {
  mips::MipsLabel label, patcher_label1, patcher_label2;
  __ SetReorder(true);
  __ Addiu(mips::T0, mips::T1, 0x5678, &patcher_label1);
  __ B(&label);
  constexpr uint32_t kAdduCount1 = (1u << 15) + 1;
  for (size_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 15) + 1;
  for (size_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Addiu(mips::T0, mips::T1, 0x5678, &patcher_label2);
  __ B(&label);

  // Account for 5 extra instructions: ori, addu, lw, jalr, addiu.
  uint32_t offset_forward = (kAdduCount1 + 5) * sizeof(uint32_t);
  // Account for 5 extra instructions: subu, addiu, sw, nal, lui.
  uint32_t offset_back = static_cast<uint32_t>(-(kAdduCount1 + 5) * sizeof(uint32_t));

  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "addiu $t0, $t1, 0x5678\n"
      "addiu $sp, $sp, -16\n"
      "sw $ra, 0($sp)\n"
      "bltzal $zero, .+4\n"
      "lui $at, 0x" << std::hex << High16Bits(offset_forward) << "\n"
      "ori $at, $at, 0x" << std::hex << Low16Bits(offset_forward) << "\n"
      "addu $at, $at, $ra\n"
      "lw $ra, 0($sp)\n"
      "jalr $zero, $at\n"
      "addiu $sp, $sp, 16\n" <<
      RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") <<
      RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") <<
      "addiu $t0, $t1, 0x5678\n"
      "addiu $sp, $sp, -16\n"
      "sw $ra, 0($sp)\n"
      "bltzal $zero, .+4\n"
      "lui $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "ori $at, $at, 0x" << std::hex << Low16Bits(offset_back) << "\n"
      "addu $at, $at, $ra\n"
      "lw $ra, 0($sp)\n"
      "jalr $zero, $at\n"
      "addiu $sp, $sp, 16\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBranchReorder");
  EXPECT_EQ(__ GetLabelLocation(&patcher_label1), 0 * 4u);
  EXPECT_EQ(__ GetLabelLocation(&patcher_label2), (kAdduCount1 + kAdduCount2 + 10) * 4u);
}

///////////////////////
// Loading Constants //
///////////////////////

TEST_F(AssemblerMIPSTest, LoadConst32) {
  // IsUint<16>(value)
  __ LoadConst32(mips::V0, 0);
  __ LoadConst32(mips::V0, 65535);
  // IsInt<16>(value)
  __ LoadConst32(mips::V0, -1);
  __ LoadConst32(mips::V0, -32768);
  // Everything else
  __ LoadConst32(mips::V0, 65536);
  __ LoadConst32(mips::V0, 65537);
  __ LoadConst32(mips::V0, 2147483647);
  __ LoadConst32(mips::V0, -32769);
  __ LoadConst32(mips::V0, -65536);
  __ LoadConst32(mips::V0, -65537);
  __ LoadConst32(mips::V0, -2147483647);
  __ LoadConst32(mips::V0, -2147483648);

  const char* expected =
      // IsUint<16>(value)
      "ori $v0, $zero, 0\n"         // __ LoadConst32(mips::V0, 0);
      "ori $v0, $zero, 65535\n"     // __ LoadConst32(mips::V0, 65535);
      // IsInt<16>(value)
      "addiu $v0, $zero, -1\n"      // __ LoadConst32(mips::V0, -1);
      "addiu $v0, $zero, -32768\n"  // __ LoadConst32(mips::V0, -32768);
      // Everything else
      "lui $v0, 1\n"                // __ LoadConst32(mips::V0, 65536);
      "lui $v0, 1\n"                // __ LoadConst32(mips::V0, 65537);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32767\n"            // __ LoadConst32(mips::V0, 2147483647);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips::V0, -32769);
      "ori $v0, 32767\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips::V0, -65536);
      "lui $v0, 65534\n"            // __ LoadConst32(mips::V0, -65537);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 32768\n"            // __ LoadConst32(mips::V0, -2147483647);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32768\n";           // __ LoadConst32(mips::V0, -2147483648);
  DriverStr(expected, "LoadConst32");
}

TEST_F(AssemblerMIPSTest, LoadFarthestNearLabelAddress) {
  mips::MipsLabel label;
  __ BindPcRelBaseLabel();
  __ LoadLabelAddress(mips::V0, mips::V1, &label);
  constexpr size_t kAddiuCount = 0x1FDE;
  for (size_t i = 0; i != kAddiuCount; ++i) {
    __ Addiu(mips::A0, mips::A1, 0);
  }
  __ Bind(&label);

  std::string expected =
      "1:\n"
      "addiu $v0, $v1, %lo(2f - 1b)\n" +
      RepeatInsn(kAddiuCount, "addiu $a0, $a1, %hi(2f - 1b)\n") +
      "2:\n";
  DriverStr(expected, "LoadFarthestNearLabelAddress");
}

TEST_F(AssemblerMIPSTest, LoadNearestFarLabelAddress) {
  mips::MipsLabel label;
  __ BindPcRelBaseLabel();
  __ LoadLabelAddress(mips::V0, mips::V1, &label);
  constexpr size_t kAdduCount = 0x1FDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      "1:\n"
      "lui $at, %hi(2f - 1b)\n"
      "ori $at, $at, %lo(2f - 1b)\n"
      "addu $v0, $at, $v1\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n";
  DriverStr(expected, "LoadNearestFarLabelAddress");
}

TEST_F(AssemblerMIPSTest, LoadFarthestNearLabelAddressUsingNal) {
  mips::MipsLabel label;
  __ LoadLabelAddress(mips::V0, mips::ZERO, &label);
  constexpr size_t kAddiuCount = 0x1FDE;
  for (size_t i = 0; i != kAddiuCount; ++i) {
    __ Addiu(mips::A0, mips::A1, 0);
  }
  __ Bind(&label);

  std::string expected =
      ".set noreorder\n"
      "bltzal $zero, .+4\n"
      "addiu $v0, $ra, %lo(2f - 1f)\n"
      "1:\n" +
      RepeatInsn(kAddiuCount, "addiu $a0, $a1, %hi(2f - 1b)\n") +
      "2:\n";
  DriverStr(expected, "LoadFarthestNearLabelAddressUsingNal");
}

TEST_F(AssemblerMIPSTest, LoadNearestFarLabelAddressUsingNal) {
  mips::MipsLabel label;
  __ LoadLabelAddress(mips::V0, mips::ZERO, &label);
  constexpr size_t kAdduCount = 0x1FDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      ".set noreorder\n"
      "bltzal $zero, .+4\n"
      "lui $at, %hi(2f - 1f)\n"
      "1:\n"
      "ori $at, $at, %lo(2f - 1b)\n"
      "addu $v0, $at, $ra\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n";
  DriverStr(expected, "LoadNearestFarLabelAddressUsingNal");
}

TEST_F(AssemblerMIPSTest, LoadFarthestNearLiteral) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ BindPcRelBaseLabel();
  __ LoadLiteral(mips::V0, mips::V1, literal);
  constexpr size_t kAddiuCount = 0x1FDE;
  for (size_t i = 0; i != kAddiuCount; ++i) {
    __ Addiu(mips::A0, mips::A1, 0);
  }

  std::string expected =
      "1:\n"
      "lw $v0, %lo(2f - 1b)($v1)\n" +
      RepeatInsn(kAddiuCount, "addiu $a0, $a1, %hi(2f - 1b)\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadFarthestNearLiteral");
}

TEST_F(AssemblerMIPSTest, LoadNearestFarLiteral) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ BindPcRelBaseLabel();
  __ LoadLiteral(mips::V0, mips::V1, literal);
  constexpr size_t kAdduCount = 0x1FDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }

  std::string expected =
      "1:\n"
      "lui $at, %hi(2f - 1b)\n"
      "addu $at, $at, $v1\n"
      "lw $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadNearestFarLiteral");
}

TEST_F(AssemblerMIPSTest, LoadFarthestNearLiteralUsingNal) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips::V0, mips::ZERO, literal);
  constexpr size_t kAddiuCount = 0x1FDE;
  for (size_t i = 0; i != kAddiuCount; ++i) {
    __ Addiu(mips::A0, mips::A1, 0);
  }

  std::string expected =
      ".set noreorder\n"
      "bltzal $zero, .+4\n"
      "lw $v0, %lo(2f - 1f)($ra)\n"
      "1:\n" +
      RepeatInsn(kAddiuCount, "addiu $a0, $a1, %hi(2f - 1b)\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadFarthestNearLiteralUsingNal");
}

TEST_F(AssemblerMIPSTest, LoadNearestFarLiteralUsingNal) {
  mips::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips::V0, mips::ZERO, literal);
  constexpr size_t kAdduCount = 0x1FDF;
  for (size_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips::ZERO, mips::ZERO, mips::ZERO);
  }

  std::string expected =
      ".set noreorder\n"
      "bltzal $zero, .+4\n"
      "lui $at, %hi(2f - 1f)\n"
      "1:\n"
      "addu $at, $at, $ra\n"
      "lw $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadNearestFarLiteralUsingNal");
}

#undef __

}  // namespace art
