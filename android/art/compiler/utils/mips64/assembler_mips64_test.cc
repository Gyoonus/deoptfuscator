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

#include "assembler_mips64.h"

#include <inttypes.h>
#include <map>
#include <random>

#include "base/bit_utils.h"
#include "base/stl_util.h"
#include "utils/assembler_test.h"

#define __ GetAssembler()->

namespace art {

struct MIPS64CpuRegisterCompare {
  bool operator()(const mips64::GpuRegister& a, const mips64::GpuRegister& b) const {
    return a < b;
  }
};

class AssemblerMIPS64Test : public AssemblerTest<mips64::Mips64Assembler,
                                                 mips64::Mips64Label,
                                                 mips64::GpuRegister,
                                                 mips64::FpuRegister,
                                                 uint32_t,
                                                 mips64::VectorRegister> {
 public:
  typedef AssemblerTest<mips64::Mips64Assembler,
                        mips64::Mips64Label,
                        mips64::GpuRegister,
                        mips64::FpuRegister,
                        uint32_t,
                        mips64::VectorRegister> Base;

  AssemblerMIPS64Test()
      : instruction_set_features_(Mips64InstructionSetFeatures::FromVariant("default", nullptr)) {}

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips64";
  }

  std::string GetAssemblerCmdName() OVERRIDE {
    // We assemble and link for MIPS64R6. See GetAssemblerParameters() for details.
    return "gcc";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    // We assemble and link for MIPS64R6. The reason is that object files produced for MIPS64R6
    // (and MIPS32R6) with the GNU assembler don't have correct final offsets in PC-relative
    // branches in the .text section and so they require a relocation pass (there's a relocation
    // section, .rela.text, that has the needed info to fix up the branches).
    return " -march=mips64r6 -mmsa -Wa,--no-warn -Wl,-Ttext=0 -Wl,-e0 -nostdlib";
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
    return " -D -bbinary -mmips:isa64r6";
  }

  mips64::Mips64Assembler* CreateAssembler(ArenaAllocator* allocator) OVERRIDE {
    return new (allocator) mips64::Mips64Assembler(allocator, instruction_set_features_.get());
  }

  void SetUpHelpers() OVERRIDE {
    if (registers_.size() == 0) {
      registers_.push_back(new mips64::GpuRegister(mips64::ZERO));
      registers_.push_back(new mips64::GpuRegister(mips64::AT));
      registers_.push_back(new mips64::GpuRegister(mips64::V0));
      registers_.push_back(new mips64::GpuRegister(mips64::V1));
      registers_.push_back(new mips64::GpuRegister(mips64::A0));
      registers_.push_back(new mips64::GpuRegister(mips64::A1));
      registers_.push_back(new mips64::GpuRegister(mips64::A2));
      registers_.push_back(new mips64::GpuRegister(mips64::A3));
      registers_.push_back(new mips64::GpuRegister(mips64::A4));
      registers_.push_back(new mips64::GpuRegister(mips64::A5));
      registers_.push_back(new mips64::GpuRegister(mips64::A6));
      registers_.push_back(new mips64::GpuRegister(mips64::A7));
      registers_.push_back(new mips64::GpuRegister(mips64::T0));
      registers_.push_back(new mips64::GpuRegister(mips64::T1));
      registers_.push_back(new mips64::GpuRegister(mips64::T2));
      registers_.push_back(new mips64::GpuRegister(mips64::T3));
      registers_.push_back(new mips64::GpuRegister(mips64::S0));
      registers_.push_back(new mips64::GpuRegister(mips64::S1));
      registers_.push_back(new mips64::GpuRegister(mips64::S2));
      registers_.push_back(new mips64::GpuRegister(mips64::S3));
      registers_.push_back(new mips64::GpuRegister(mips64::S4));
      registers_.push_back(new mips64::GpuRegister(mips64::S5));
      registers_.push_back(new mips64::GpuRegister(mips64::S6));
      registers_.push_back(new mips64::GpuRegister(mips64::S7));
      registers_.push_back(new mips64::GpuRegister(mips64::T8));
      registers_.push_back(new mips64::GpuRegister(mips64::T9));
      registers_.push_back(new mips64::GpuRegister(mips64::K0));
      registers_.push_back(new mips64::GpuRegister(mips64::K1));
      registers_.push_back(new mips64::GpuRegister(mips64::GP));
      registers_.push_back(new mips64::GpuRegister(mips64::SP));
      registers_.push_back(new mips64::GpuRegister(mips64::S8));
      registers_.push_back(new mips64::GpuRegister(mips64::RA));

      secondary_register_names_.emplace(mips64::GpuRegister(mips64::ZERO), "zero");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::AT), "at");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::V0), "v0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::V1), "v1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A0), "a0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A1), "a1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A2), "a2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A3), "a3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A4), "a4");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A5), "a5");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A6), "a6");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::A7), "a7");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T0), "t0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T1), "t1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T2), "t2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T3), "t3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S0), "s0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S1), "s1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S2), "s2");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S3), "s3");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S4), "s4");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S5), "s5");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S6), "s6");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S7), "s7");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T8), "t8");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::T9), "t9");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::K0), "k0");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::K1), "k1");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::GP), "gp");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::SP), "sp");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::S8), "s8");
      secondary_register_names_.emplace(mips64::GpuRegister(mips64::RA), "ra");

      fp_registers_.push_back(new mips64::FpuRegister(mips64::F0));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F1));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F2));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F3));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F4));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F5));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F6));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F7));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F8));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F9));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F10));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F11));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F12));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F13));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F14));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F15));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F16));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F17));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F18));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F19));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F20));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F21));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F22));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F23));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F24));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F25));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F26));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F27));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F28));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F29));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F30));
      fp_registers_.push_back(new mips64::FpuRegister(mips64::F31));

      vec_registers_.push_back(new mips64::VectorRegister(mips64::W0));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W1));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W2));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W3));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W4));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W5));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W6));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W7));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W8));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W9));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W10));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W11));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W12));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W13));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W14));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W15));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W16));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W17));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W18));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W19));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W20));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W21));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W22));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W23));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W24));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W25));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W26));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W27));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W28));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W29));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W30));
      vec_registers_.push_back(new mips64::VectorRegister(mips64::W31));
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
    STLDeleteElements(&vec_registers_);
  }

  std::vector<mips64::Mips64Label> GetAddresses() {
    UNIMPLEMENTED(FATAL) << "Feature not implemented yet";
    UNREACHABLE();
  }

  std::vector<mips64::GpuRegister*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<mips64::FpuRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  std::vector<mips64::VectorRegister*> GetVectorRegisters() OVERRIDE {
    return vec_registers_;
  }

  uint32_t CreateImmediate(int64_t imm_value) OVERRIDE {
    return imm_value;
  }

  std::string GetSecondaryRegisterName(const mips64::GpuRegister& reg) OVERRIDE {
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

  void BranchHelper(void (mips64::Mips64Assembler::*f)(mips64::Mips64Label*,
                                                       bool),
                    const std::string& instr_name,
                    bool is_bare = false) {
    mips64::Mips64Label label1, label2;
    (Base::GetAssembler()->*f)(&label1, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label1);
    (Base::GetAssembler()->*f)(&label2, is_bare);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label2);
    (Base::GetAssembler()->*f)(&label1, is_bare);
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " 1f\n" +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        instr_name + " 2f\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        "2:\n" +
        instr_name + " 1b\n" +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

  void BranchCondOneRegHelper(void (mips64::Mips64Assembler::*f)(mips64::GpuRegister,
                                                                 mips64::Mips64Label*,
                                                                 bool),
                              const std::string& instr_name,
                              bool is_bare = false) {
    mips64::Mips64Label label;
    (Base::GetAssembler()->*f)(mips64::A0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    (Base::GetAssembler()->*f)(mips64::A1, &label, is_bare);
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);

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

  void BranchCondTwoRegsHelper(void (mips64::Mips64Assembler::*f)(mips64::GpuRegister,
                                                                  mips64::GpuRegister,
                                                                  mips64::Mips64Label*,
                                                                  bool),
                               const std::string& instr_name,
                               bool is_bare = false) {
    mips64::Mips64Label label;
    (Base::GetAssembler()->*f)(mips64::A0, mips64::A1, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    (Base::GetAssembler()->*f)(mips64::A2, mips64::A3, &label, is_bare);
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);

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

  void BranchFpuCondHelper(void (mips64::Mips64Assembler::*f)(mips64::FpuRegister,
                                                              mips64::Mips64Label*,
                                                              bool),
                           const std::string& instr_name,
                           bool is_bare = false) {
    mips64::Mips64Label label;
    (Base::GetAssembler()->*f)(mips64::F0, &label, is_bare);
    constexpr size_t kAdduCount1 = 63;
    for (size_t i = 0; i != kAdduCount1; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    __ Bind(&label);
    constexpr size_t kAdduCount2 = 64;
    for (size_t i = 0; i != kAdduCount2; ++i) {
      __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
    }
    (Base::GetAssembler()->*f)(mips64::F31, &label, is_bare);
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);

    std::string expected =
        ".set noreorder\n" +
        instr_name + " $f0, 1f\n" +
        (is_bare ? "" : "nop\n") +
        RepeatInsn(kAdduCount1, "addu $zero, $zero, $zero\n") +
        "1:\n" +
        RepeatInsn(kAdduCount2, "addu $zero, $zero, $zero\n") +
        instr_name + " $f31, 1b\n" +
        (is_bare ? "" : "nop\n") +
        "addu $zero, $zero, $zero\n";
    DriverStr(expected, instr_name);
  }

 private:
  std::vector<mips64::GpuRegister*> registers_;
  std::map<mips64::GpuRegister, std::string, MIPS64CpuRegisterCompare> secondary_register_names_;

  std::vector<mips64::FpuRegister*> fp_registers_;
  std::vector<mips64::VectorRegister*> vec_registers_;

  std::unique_ptr<const Mips64InstructionSetFeatures> instruction_set_features_;
};

TEST_F(AssemblerMIPS64Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

///////////////////
// FP Operations //
///////////////////

TEST_F(AssemblerMIPS64Test, AddS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::AddS, "add.s ${reg1}, ${reg2}, ${reg3}"), "add.s");
}

TEST_F(AssemblerMIPS64Test, AddD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::AddD, "add.d ${reg1}, ${reg2}, ${reg3}"), "add.d");
}

TEST_F(AssemblerMIPS64Test, SubS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SubS, "sub.s ${reg1}, ${reg2}, ${reg3}"), "sub.s");
}

TEST_F(AssemblerMIPS64Test, SubD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SubD, "sub.d ${reg1}, ${reg2}, ${reg3}"), "sub.d");
}

TEST_F(AssemblerMIPS64Test, MulS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MulS, "mul.s ${reg1}, ${reg2}, ${reg3}"), "mul.s");
}

TEST_F(AssemblerMIPS64Test, MulD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MulD, "mul.d ${reg1}, ${reg2}, ${reg3}"), "mul.d");
}

TEST_F(AssemblerMIPS64Test, DivS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::DivS, "div.s ${reg1}, ${reg2}, ${reg3}"), "div.s");
}

TEST_F(AssemblerMIPS64Test, DivD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::DivD, "div.d ${reg1}, ${reg2}, ${reg3}"), "div.d");
}

TEST_F(AssemblerMIPS64Test, SqrtS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::SqrtS, "sqrt.s ${reg1}, ${reg2}"), "sqrt.s");
}

TEST_F(AssemblerMIPS64Test, SqrtD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::SqrtD, "sqrt.d ${reg1}, ${reg2}"), "sqrt.d");
}

TEST_F(AssemblerMIPS64Test, AbsS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::AbsS, "abs.s ${reg1}, ${reg2}"), "abs.s");
}

TEST_F(AssemblerMIPS64Test, AbsD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::AbsD, "abs.d ${reg1}, ${reg2}"), "abs.d");
}

TEST_F(AssemblerMIPS64Test, MovS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::MovS, "mov.s ${reg1}, ${reg2}"), "mov.s");
}

TEST_F(AssemblerMIPS64Test, MovD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::MovD, "mov.d ${reg1}, ${reg2}"), "mov.d");
}

TEST_F(AssemblerMIPS64Test, NegS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::NegS, "neg.s ${reg1}, ${reg2}"), "neg.s");
}

TEST_F(AssemblerMIPS64Test, NegD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::NegD, "neg.d ${reg1}, ${reg2}"), "neg.d");
}

TEST_F(AssemblerMIPS64Test, RoundLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundLS, "round.l.s ${reg1}, ${reg2}"), "round.l.s");
}

TEST_F(AssemblerMIPS64Test, RoundLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundLD, "round.l.d ${reg1}, ${reg2}"), "round.l.d");
}

TEST_F(AssemblerMIPS64Test, RoundWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundWS, "round.w.s ${reg1}, ${reg2}"), "round.w.s");
}

TEST_F(AssemblerMIPS64Test, RoundWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RoundWD, "round.w.d ${reg1}, ${reg2}"), "round.w.d");
}

TEST_F(AssemblerMIPS64Test, CeilLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilLS, "ceil.l.s ${reg1}, ${reg2}"), "ceil.l.s");
}

TEST_F(AssemblerMIPS64Test, CeilLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilLD, "ceil.l.d ${reg1}, ${reg2}"), "ceil.l.d");
}

TEST_F(AssemblerMIPS64Test, CeilWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilWS, "ceil.w.s ${reg1}, ${reg2}"), "ceil.w.s");
}

TEST_F(AssemblerMIPS64Test, CeilWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::CeilWD, "ceil.w.d ${reg1}, ${reg2}"), "ceil.w.d");
}

TEST_F(AssemblerMIPS64Test, FloorLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorLS, "floor.l.s ${reg1}, ${reg2}"), "floor.l.s");
}

TEST_F(AssemblerMIPS64Test, FloorLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorLD, "floor.l.d ${reg1}, ${reg2}"), "floor.l.d");
}

TEST_F(AssemblerMIPS64Test, FloorWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorWS, "floor.w.s ${reg1}, ${reg2}"), "floor.w.s");
}

TEST_F(AssemblerMIPS64Test, FloorWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::FloorWD, "floor.w.d ${reg1}, ${reg2}"), "floor.w.d");
}

TEST_F(AssemblerMIPS64Test, SelS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelS, "sel.s ${reg1}, ${reg2}, ${reg3}"), "sel.s");
}

TEST_F(AssemblerMIPS64Test, SelD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelD, "sel.d ${reg1}, ${reg2}, ${reg3}"), "sel.d");
}

TEST_F(AssemblerMIPS64Test, SeleqzS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SeleqzS, "seleqz.s ${reg1}, ${reg2}, ${reg3}"),
            "seleqz.s");
}

TEST_F(AssemblerMIPS64Test, SeleqzD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SeleqzD, "seleqz.d ${reg1}, ${reg2}, ${reg3}"),
            "seleqz.d");
}

TEST_F(AssemblerMIPS64Test, SelnezS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelnezS, "selnez.s ${reg1}, ${reg2}, ${reg3}"),
            "selnez.s");
}

TEST_F(AssemblerMIPS64Test, SelnezD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::SelnezD, "selnez.d ${reg1}, ${reg2}, ${reg3}"),
            "selnez.d");
}

TEST_F(AssemblerMIPS64Test, RintS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RintS, "rint.s ${reg1}, ${reg2}"), "rint.s");
}

TEST_F(AssemblerMIPS64Test, RintD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::RintD, "rint.d ${reg1}, ${reg2}"), "rint.d");
}

TEST_F(AssemblerMIPS64Test, ClassS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::ClassS, "class.s ${reg1}, ${reg2}"), "class.s");
}

TEST_F(AssemblerMIPS64Test, ClassD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::ClassD, "class.d ${reg1}, ${reg2}"), "class.d");
}

TEST_F(AssemblerMIPS64Test, MinS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MinS, "min.s ${reg1}, ${reg2}, ${reg3}"), "min.s");
}

TEST_F(AssemblerMIPS64Test, MinD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MinD, "min.d ${reg1}, ${reg2}, ${reg3}"), "min.d");
}

TEST_F(AssemblerMIPS64Test, MaxS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MaxS, "max.s ${reg1}, ${reg2}, ${reg3}"), "max.s");
}

TEST_F(AssemblerMIPS64Test, MaxD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::MaxD, "max.d ${reg1}, ${reg2}, ${reg3}"), "max.d");
}

TEST_F(AssemblerMIPS64Test, CmpUnS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUnS, "cmp.un.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.s");
}

TEST_F(AssemblerMIPS64Test, CmpEqS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpEqS, "cmp.eq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.s");
}

TEST_F(AssemblerMIPS64Test, CmpUeqS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUeqS, "cmp.ueq.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.s");
}

TEST_F(AssemblerMIPS64Test, CmpLtS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLtS, "cmp.lt.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.s");
}

TEST_F(AssemblerMIPS64Test, CmpUltS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUltS, "cmp.ult.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.s");
}

TEST_F(AssemblerMIPS64Test, CmpLeS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLeS, "cmp.le.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.s");
}

TEST_F(AssemblerMIPS64Test, CmpUleS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUleS, "cmp.ule.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.s");
}

TEST_F(AssemblerMIPS64Test, CmpOrS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpOrS, "cmp.or.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.s");
}

TEST_F(AssemblerMIPS64Test, CmpUneS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUneS, "cmp.une.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.s");
}

TEST_F(AssemblerMIPS64Test, CmpNeS) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpNeS, "cmp.ne.s ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.s");
}

TEST_F(AssemblerMIPS64Test, CmpUnD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUnD, "cmp.un.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.un.d");
}

TEST_F(AssemblerMIPS64Test, CmpEqD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpEqD, "cmp.eq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.eq.d");
}

TEST_F(AssemblerMIPS64Test, CmpUeqD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUeqD, "cmp.ueq.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ueq.d");
}

TEST_F(AssemblerMIPS64Test, CmpLtD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLtD, "cmp.lt.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.lt.d");
}

TEST_F(AssemblerMIPS64Test, CmpUltD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUltD, "cmp.ult.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ult.d");
}

TEST_F(AssemblerMIPS64Test, CmpLeD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpLeD, "cmp.le.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.le.d");
}

TEST_F(AssemblerMIPS64Test, CmpUleD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUleD, "cmp.ule.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ule.d");
}

TEST_F(AssemblerMIPS64Test, CmpOrD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpOrD, "cmp.or.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.or.d");
}

TEST_F(AssemblerMIPS64Test, CmpUneD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpUneD, "cmp.une.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.une.d");
}

TEST_F(AssemblerMIPS64Test, CmpNeD) {
  DriverStr(RepeatFFF(&mips64::Mips64Assembler::CmpNeD, "cmp.ne.d ${reg1}, ${reg2}, ${reg3}"),
            "cmp.ne.d");
}

TEST_F(AssemblerMIPS64Test, CvtDL) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtdl, "cvt.d.l ${reg1}, ${reg2}"), "cvt.d.l");
}

TEST_F(AssemblerMIPS64Test, CvtDS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtds, "cvt.d.s ${reg1}, ${reg2}"), "cvt.d.s");
}

TEST_F(AssemblerMIPS64Test, CvtDW) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtdw, "cvt.d.w ${reg1}, ${reg2}"), "cvt.d.w");
}

TEST_F(AssemblerMIPS64Test, CvtSL) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsl, "cvt.s.l ${reg1}, ${reg2}"), "cvt.s.l");
}

TEST_F(AssemblerMIPS64Test, CvtSD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsd, "cvt.s.d ${reg1}, ${reg2}"), "cvt.s.d");
}

TEST_F(AssemblerMIPS64Test, CvtSW) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::Cvtsw, "cvt.s.w ${reg1}, ${reg2}"), "cvt.s.w");
}

TEST_F(AssemblerMIPS64Test, TruncWS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncWS, "trunc.w.s ${reg1}, ${reg2}"), "trunc.w.s");
}

TEST_F(AssemblerMIPS64Test, TruncWD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncWD, "trunc.w.d ${reg1}, ${reg2}"), "trunc.w.d");
}

TEST_F(AssemblerMIPS64Test, TruncLS) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncLS, "trunc.l.s ${reg1}, ${reg2}"), "trunc.l.s");
}

TEST_F(AssemblerMIPS64Test, TruncLD) {
  DriverStr(RepeatFF(&mips64::Mips64Assembler::TruncLD, "trunc.l.d ${reg1}, ${reg2}"), "trunc.l.d");
}

TEST_F(AssemblerMIPS64Test, Mfc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mfc1, "mfc1 ${reg1}, ${reg2}"), "Mfc1");
}

TEST_F(AssemblerMIPS64Test, Mfhc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mfhc1, "mfhc1 ${reg1}, ${reg2}"), "Mfhc1");
}

TEST_F(AssemblerMIPS64Test, Mtc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mtc1, "mtc1 ${reg1}, ${reg2}"), "Mtc1");
}

TEST_F(AssemblerMIPS64Test, Mthc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Mthc1, "mthc1 ${reg1}, ${reg2}"), "Mthc1");
}

TEST_F(AssemblerMIPS64Test, Dmfc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Dmfc1, "dmfc1 ${reg1}, ${reg2}"), "Dmfc1");
}

TEST_F(AssemblerMIPS64Test, Dmtc1) {
  DriverStr(RepeatRF(&mips64::Mips64Assembler::Dmtc1, "dmtc1 ${reg1}, ${reg2}"), "Dmtc1");
}

TEST_F(AssemblerMIPS64Test, Lwc1) {
  DriverStr(RepeatFRIb(&mips64::Mips64Assembler::Lwc1, -16, "lwc1 ${reg1}, {imm}(${reg2})"),
            "lwc1");
}

TEST_F(AssemblerMIPS64Test, Ldc1) {
  DriverStr(RepeatFRIb(&mips64::Mips64Assembler::Ldc1, -16, "ldc1 ${reg1}, {imm}(${reg2})"),
            "ldc1");
}

TEST_F(AssemblerMIPS64Test, Swc1) {
  DriverStr(RepeatFRIb(&mips64::Mips64Assembler::Swc1, -16, "swc1 ${reg1}, {imm}(${reg2})"),
            "swc1");
}

TEST_F(AssemblerMIPS64Test, Sdc1) {
  DriverStr(RepeatFRIb(&mips64::Mips64Assembler::Sdc1, -16, "sdc1 ${reg1}, {imm}(${reg2})"),
            "sdc1");
}

//////////////
// BRANCHES //
//////////////

TEST_F(AssemblerMIPS64Test, Jalr) {
  DriverStr(".set noreorder\n" +
            RepeatRRNoDupes(&mips64::Mips64Assembler::Jalr, "jalr ${reg1}, ${reg2}"), "jalr");
}

TEST_F(AssemblerMIPS64Test, Bc) {
  BranchHelper(&mips64::Mips64Assembler::Bc, "Bc");
}

TEST_F(AssemblerMIPS64Test, Balc) {
  BranchHelper(&mips64::Mips64Assembler::Balc, "Balc");
}

TEST_F(AssemblerMIPS64Test, Beqzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Beqzc, "Beqzc");
}

TEST_F(AssemblerMIPS64Test, Bnezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bnezc, "Bnezc");
}

TEST_F(AssemblerMIPS64Test, Bltzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bltzc, "Bltzc");
}

TEST_F(AssemblerMIPS64Test, Bgezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgezc, "Bgezc");
}

TEST_F(AssemblerMIPS64Test, Blezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Blezc, "Blezc");
}

TEST_F(AssemblerMIPS64Test, Bgtzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgtzc, "Bgtzc");
}

TEST_F(AssemblerMIPS64Test, Beqc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Beqc, "Beqc");
}

TEST_F(AssemblerMIPS64Test, Bnec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bnec, "Bnec");
}

TEST_F(AssemblerMIPS64Test, Bltc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltc, "Bltc");
}

TEST_F(AssemblerMIPS64Test, Bgec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgec, "Bgec");
}

TEST_F(AssemblerMIPS64Test, Bltuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltuc, "Bltuc");
}

TEST_F(AssemblerMIPS64Test, Bgeuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgeuc, "Bgeuc");
}

TEST_F(AssemblerMIPS64Test, Bc1eqz) {
  BranchFpuCondHelper(&mips64::Mips64Assembler::Bc1eqz, "Bc1eqz");
}

TEST_F(AssemblerMIPS64Test, Bc1nez) {
  BranchFpuCondHelper(&mips64::Mips64Assembler::Bc1nez, "Bc1nez");
}

TEST_F(AssemblerMIPS64Test, BareBc) {
  BranchHelper(&mips64::Mips64Assembler::Bc, "Bc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBalc) {
  BranchHelper(&mips64::Mips64Assembler::Balc, "Balc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBeqzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Beqzc, "Beqzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBnezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bnezc, "Bnezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBltzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bltzc, "Bltzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgezc, "Bgezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBlezc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Blezc, "Blezc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgtzc) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgtzc, "Bgtzc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBeqc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Beqc, "Beqc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBnec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bnec, "Bnec", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBltc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltc, "Bltc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgec) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgec, "Bgec", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBltuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bltuc, "Bltuc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgeuc) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bgeuc, "Bgeuc", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBc1eqz) {
  BranchFpuCondHelper(&mips64::Mips64Assembler::Bc1eqz, "Bc1eqz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBc1nez) {
  BranchFpuCondHelper(&mips64::Mips64Assembler::Bc1nez, "Bc1nez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBeqz) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Beqz, "Beqz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBnez) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bnez, "Bnez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBltz) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bltz, "Bltz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgez) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgez, "Bgez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBlez) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Blez, "Blez", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBgtz) {
  BranchCondOneRegHelper(&mips64::Mips64Assembler::Bgtz, "Bgtz", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBeq) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Beq, "Beq", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, BareBne) {
  BranchCondTwoRegsHelper(&mips64::Mips64Assembler::Bne, "Bne", /* is_bare */ true);
}

TEST_F(AssemblerMIPS64Test, LongBeqc) {
  mips64::Mips64Label label;
  __ Beqc(mips64::A0, mips64::A1, &label);
  constexpr uint32_t kAdduCount1 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount1; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label);
  constexpr uint32_t kAdduCount2 = (1u << 15) + 1;
  for (uint32_t i = 0; i != kAdduCount2; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Beqc(mips64::A2, mips64::A3, &label);

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

TEST_F(AssemblerMIPS64Test, LongBeqzc) {
  constexpr uint32_t kNopCount1 = (1u << 20) + 1;
  constexpr uint32_t kNopCount2 = (1u << 20) + 1;
  constexpr uint32_t kRequiredCapacity = (kNopCount1 + kNopCount2 + 6u) * 4u;
  ASSERT_LT(__ GetBuffer()->Capacity(), kRequiredCapacity);
  __ GetBuffer()->ExtendCapacity(kRequiredCapacity);
  mips64::Mips64Label label;
  __ Beqzc(mips64::A0, &label);
  for (uint32_t i = 0; i != kNopCount1; ++i) {
    __ Nop();
  }
  __ Bind(&label);
  for (uint32_t i = 0; i != kNopCount2; ++i) {
    __ Nop();
  }
  __ Beqzc(mips64::A2, &label);

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

TEST_F(AssemblerMIPS64Test, LongBalc) {
  constexpr uint32_t kNopCount1 = (1u << 25) + 1;
  constexpr uint32_t kNopCount2 = (1u << 25) + 1;
  constexpr uint32_t kRequiredCapacity = (kNopCount1 + kNopCount2 + 6u) * 4u;
  ASSERT_LT(__ GetBuffer()->Capacity(), kRequiredCapacity);
  __ GetBuffer()->ExtendCapacity(kRequiredCapacity);
  mips64::Mips64Label label1, label2;
  __ Balc(&label1);
  for (uint32_t i = 0; i != kNopCount1; ++i) {
    __ Nop();
  }
  __ Bind(&label1);
  __ Balc(&label2);
  for (uint32_t i = 0; i != kNopCount2; ++i) {
    __ Nop();
  }
  __ Bind(&label2);
  __ Balc(&label1);

  uint32_t offset_forward1 = 2 + kNopCount1;  // 2: account for auipc and jialc.
  offset_forward1 <<= 2;
  offset_forward1 += (offset_forward1 & 0x8000) << 1;  // Account for sign extension in jialc.

  uint32_t offset_forward2 = 2 + kNopCount2;  // 2: account for auipc and jialc.
  offset_forward2 <<= 2;
  offset_forward2 += (offset_forward2 & 0x8000) << 1;  // Account for sign extension in jialc.

  uint32_t offset_back = -(2 + kNopCount2);  // 2: account for auipc and jialc.
  offset_back <<= 2;
  offset_back += (offset_back & 0x8000) << 1;  // Account for sign extension in jialc.

  // Note, we're using the ".fill" directive to tell the assembler to generate many NOPs
  // instead of generating them ourselves in the source code. This saves a few minutes
  // of test time.
  std::ostringstream oss;
  oss <<
      ".set noreorder\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward1) << "\n"
      "jialc $at, 0x" << std::hex << Low16Bits(offset_forward1) << "\n"
      ".fill 0x" << std::hex << kNopCount1 << " , 4, 0\n"
      "1:\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_forward2) << "\n"
      "jialc $at, 0x" << std::hex << Low16Bits(offset_forward2) << "\n"
      ".fill 0x" << std::hex << kNopCount2 << " , 4, 0\n"
      "2:\n"
      "auipc $at, 0x" << std::hex << High16Bits(offset_back) << "\n"
      "jialc $at, 0x" << std::hex << Low16Bits(offset_back) << "\n";
  std::string expected = oss.str();
  DriverStr(expected, "LongBalc");
}

//////////
// MISC //
//////////

TEST_F(AssemblerMIPS64Test, Lwpc) {
  // Lwpc() takes an unsigned 19-bit immediate, while the GNU assembler needs a signed offset,
  // hence the sign extension from bit 18 with `imm - ((imm & 0x40000) << 1)`.
  // The GNU assembler also wants the offset to be a multiple of 4, which it will shift right
  // by 2 positions when encoding, hence `<< 2` to compensate for that shift.
  // We capture the value of the immediate with `.set imm, {imm}` because the value is needed
  // twice for the sign extension, but `{imm}` is substituted only once.
  const char* code = ".set imm, {imm}\nlw ${reg}, ((imm - ((imm & 0x40000) << 1)) << 2)($pc)";
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Lwpc, 19, code), "Lwpc");
}

TEST_F(AssemblerMIPS64Test, Lwupc) {
  // The comment for the Lwpc test applies here as well.
  const char* code = ".set imm, {imm}\nlwu ${reg}, ((imm - ((imm & 0x40000) << 1)) << 2)($pc)";
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Lwupc, 19, code), "Lwupc");
}

TEST_F(AssemblerMIPS64Test, Ldpc) {
  // The comment for the Lwpc test applies here as well.
  const char* code = ".set imm, {imm}\nld ${reg}, ((imm - ((imm & 0x20000) << 1)) << 3)($pc)";
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Ldpc, 18, code), "Ldpc");
}

TEST_F(AssemblerMIPS64Test, Auipc) {
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Auipc, 16, "auipc ${reg}, {imm}"), "Auipc");
}

TEST_F(AssemblerMIPS64Test, Addiupc) {
  // The comment from the Lwpc() test applies to this Addiupc() test as well.
  const char* code = ".set imm, {imm}\naddiupc ${reg}, (imm - ((imm & 0x40000) << 1)) << 2";
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Addiupc, 19, code), "Addiupc");
}

TEST_F(AssemblerMIPS64Test, Addu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Addu, "addu ${reg1}, ${reg2}, ${reg3}"), "addu");
}

TEST_F(AssemblerMIPS64Test, Addiu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Addiu, -16, "addiu ${reg1}, ${reg2}, {imm}"),
            "addiu");
}

TEST_F(AssemblerMIPS64Test, Daddu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Daddu, "daddu ${reg1}, ${reg2}, ${reg3}"), "daddu");
}

TEST_F(AssemblerMIPS64Test, Daddiu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Daddiu, -16, "daddiu ${reg1}, ${reg2}, {imm}"),
            "daddiu");
}

TEST_F(AssemblerMIPS64Test, Subu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Subu, "subu ${reg1}, ${reg2}, ${reg3}"), "subu");
}

TEST_F(AssemblerMIPS64Test, Dsubu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dsubu, "dsubu ${reg1}, ${reg2}, ${reg3}"), "dsubu");
}

TEST_F(AssemblerMIPS64Test, MulR6) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::MulR6, "mul ${reg1}, ${reg2}, ${reg3}"), "mulR6");
}

TEST_F(AssemblerMIPS64Test, DivR6) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::DivR6, "div ${reg1}, ${reg2}, ${reg3}"), "divR6");
}

TEST_F(AssemblerMIPS64Test, ModR6) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::ModR6, "mod ${reg1}, ${reg2}, ${reg3}"), "modR6");
}

TEST_F(AssemblerMIPS64Test, DivuR6) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::DivuR6, "divu ${reg1}, ${reg2}, ${reg3}"),
            "divuR6");
}

TEST_F(AssemblerMIPS64Test, ModuR6) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::ModuR6, "modu ${reg1}, ${reg2}, ${reg3}"),
            "moduR6");
}

TEST_F(AssemblerMIPS64Test, Dmul) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dmul, "dmul ${reg1}, ${reg2}, ${reg3}"), "dmul");
}

TEST_F(AssemblerMIPS64Test, Ddiv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Ddiv, "ddiv ${reg1}, ${reg2}, ${reg3}"), "ddiv");
}

TEST_F(AssemblerMIPS64Test, Dmod) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dmod, "dmod ${reg1}, ${reg2}, ${reg3}"), "dmod");
}

TEST_F(AssemblerMIPS64Test, Ddivu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Ddivu, "ddivu ${reg1}, ${reg2}, ${reg3}"), "ddivu");
}

TEST_F(AssemblerMIPS64Test, Dmodu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dmodu, "dmodu ${reg1}, ${reg2}, ${reg3}"), "dmodu");
}

TEST_F(AssemblerMIPS64Test, And) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::And, "and ${reg1}, ${reg2}, ${reg3}"), "and");
}

TEST_F(AssemblerMIPS64Test, Andi) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Andi, 16, "andi ${reg1}, ${reg2}, {imm}"), "andi");
}

TEST_F(AssemblerMIPS64Test, Or) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Or, "or ${reg1}, ${reg2}, ${reg3}"), "or");
}

TEST_F(AssemblerMIPS64Test, Ori) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Ori, 16, "ori ${reg1}, ${reg2}, {imm}"), "ori");
}

TEST_F(AssemblerMIPS64Test, Xor) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Xor, "xor ${reg1}, ${reg2}, ${reg3}"), "xor");
}

TEST_F(AssemblerMIPS64Test, Xori) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Xori, 16, "xori ${reg1}, ${reg2}, {imm}"), "xori");
}

TEST_F(AssemblerMIPS64Test, Nor) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Nor, "nor ${reg1}, ${reg2}, ${reg3}"), "nor");
}

TEST_F(AssemblerMIPS64Test, Lb) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lb, -16, "lb ${reg1}, {imm}(${reg2})"), "lb");
}

TEST_F(AssemblerMIPS64Test, Lh) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lh, -16, "lh ${reg1}, {imm}(${reg2})"), "lh");
}

TEST_F(AssemblerMIPS64Test, Lw) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lw, -16, "lw ${reg1}, {imm}(${reg2})"), "lw");
}

TEST_F(AssemblerMIPS64Test, Ld) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Ld, -16, "ld ${reg1}, {imm}(${reg2})"), "ld");
}

TEST_F(AssemblerMIPS64Test, Lbu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lbu, -16, "lbu ${reg1}, {imm}(${reg2})"), "lbu");
}

TEST_F(AssemblerMIPS64Test, Lhu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lhu, -16, "lhu ${reg1}, {imm}(${reg2})"), "lhu");
}

TEST_F(AssemblerMIPS64Test, Lwu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lwu, -16, "lwu ${reg1}, {imm}(${reg2})"), "lwu");
}

TEST_F(AssemblerMIPS64Test, Lui) {
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Lui, 16, "lui ${reg}, {imm}"), "lui");
}

TEST_F(AssemblerMIPS64Test, Daui) {
  std::vector<mips64::GpuRegister*> reg1_registers = GetRegisters();
  std::vector<mips64::GpuRegister*> reg2_registers = GetRegisters();
  reg2_registers.erase(reg2_registers.begin());  // reg2 can't be ZERO, remove it.
  std::vector<int64_t> imms = CreateImmediateValuesBits(/* imm_bits */ 16, /* as_uint */ true);
  WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * imms.size());
  std::ostringstream expected;
  for (mips64::GpuRegister* reg1 : reg1_registers) {
    for (mips64::GpuRegister* reg2 : reg2_registers) {
      for (int64_t imm : imms) {
        __ Daui(*reg1, *reg2, imm);
        expected << "daui $" << *reg1 << ", $" << *reg2 << ", " << imm << "\n";
      }
    }
  }
  DriverStr(expected.str(), "daui");
}

TEST_F(AssemblerMIPS64Test, Dahi) {
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Dahi, 16, "dahi ${reg}, ${reg}, {imm}"), "dahi");
}

TEST_F(AssemblerMIPS64Test, Dati) {
  DriverStr(RepeatRIb(&mips64::Mips64Assembler::Dati, 16, "dati ${reg}, ${reg}, {imm}"), "dati");
}

TEST_F(AssemblerMIPS64Test, Sb) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sb, -16, "sb ${reg1}, {imm}(${reg2})"), "sb");
}

TEST_F(AssemblerMIPS64Test, Sh) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sh, -16, "sh ${reg1}, {imm}(${reg2})"), "sh");
}

TEST_F(AssemblerMIPS64Test, Sw) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sw, -16, "sw ${reg1}, {imm}(${reg2})"), "sw");
}

TEST_F(AssemblerMIPS64Test, Sd) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sd, -16, "sd ${reg1}, {imm}(${reg2})"), "sd");
}

TEST_F(AssemblerMIPS64Test, Slt) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Slt, "slt ${reg1}, ${reg2}, ${reg3}"), "slt");
}

TEST_F(AssemblerMIPS64Test, Sltu) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Sltu, "sltu ${reg1}, ${reg2}, ${reg3}"), "sltu");
}

TEST_F(AssemblerMIPS64Test, Slti) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Slti, -16, "slti ${reg1}, ${reg2}, {imm}"),
            "slti");
}

TEST_F(AssemblerMIPS64Test, Sltiu) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sltiu, -16, "sltiu ${reg1}, ${reg2}, {imm}"),
            "sltiu");
}

TEST_F(AssemblerMIPS64Test, Move) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Move, "or ${reg1}, ${reg2}, $zero"), "move");
}

TEST_F(AssemblerMIPS64Test, Clear) {
  DriverStr(RepeatR(&mips64::Mips64Assembler::Clear, "or ${reg}, $zero, $zero"), "clear");
}

TEST_F(AssemblerMIPS64Test, Not) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Not, "nor ${reg1}, ${reg2}, $zero"), "not");
}

TEST_F(AssemblerMIPS64Test, Bitswap) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Bitswap, "bitswap ${reg1}, ${reg2}"), "bitswap");
}

TEST_F(AssemblerMIPS64Test, Dbitswap) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dbitswap, "dbitswap ${reg1}, ${reg2}"), "dbitswap");
}

TEST_F(AssemblerMIPS64Test, Seb) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Seb, "seb ${reg1}, ${reg2}"), "seb");
}

TEST_F(AssemblerMIPS64Test, Seh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Seh, "seh ${reg1}, ${reg2}"), "seh");
}

TEST_F(AssemblerMIPS64Test, Dsbh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dsbh, "dsbh ${reg1}, ${reg2}"), "dsbh");
}

TEST_F(AssemblerMIPS64Test, Dshd) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dshd, "dshd ${reg1}, ${reg2}"), "dshd");
}

TEST_F(AssemblerMIPS64Test, Dext) {
  std::vector<mips64::GpuRegister*> reg1_registers = GetRegisters();
  std::vector<mips64::GpuRegister*> reg2_registers = GetRegisters();
  WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * 33 * 16);
  std::ostringstream expected;
  for (mips64::GpuRegister* reg1 : reg1_registers) {
    for (mips64::GpuRegister* reg2 : reg2_registers) {
      for (int32_t pos = 0; pos < 32; pos++) {
        for (int32_t size = 1; size <= 32; size++) {
          __ Dext(*reg1, *reg2, pos, size);
          expected << "dext $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
        }
      }
    }
  }

  DriverStr(expected.str(), "Dext");
}

TEST_F(AssemblerMIPS64Test, Ins) {
  std::vector<mips64::GpuRegister*> regs = GetRegisters();
  WarnOnCombinations(regs.size() * regs.size() * 33 * 16);
  std::string expected;
  for (mips64::GpuRegister* reg1 : regs) {
    for (mips64::GpuRegister* reg2 : regs) {
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

TEST_F(AssemblerMIPS64Test, DblIns) {
  std::vector<mips64::GpuRegister*> reg1_registers = GetRegisters();
  std::vector<mips64::GpuRegister*> reg2_registers = GetRegisters();
  WarnOnCombinations(reg1_registers.size() * reg2_registers.size() * 65 * 32);
  std::ostringstream expected;
  for (mips64::GpuRegister* reg1 : reg1_registers) {
    for (mips64::GpuRegister* reg2 : reg2_registers) {
      for (int32_t pos = 0; pos < 64; pos++) {
        for (int32_t size = 1; pos + size <= 64; size++) {
          __ DblIns(*reg1, *reg2, pos, size);
          expected << "dins $" << *reg1 << ", $" << *reg2 << ", " << pos << ", " << size << "\n";
        }
      }
    }
  }

  DriverStr(expected.str(), "DblIns");
}

TEST_F(AssemblerMIPS64Test, Lsa) {
  DriverStr(RepeatRRRIb(&mips64::Mips64Assembler::Lsa,
                        2,
                        "lsa ${reg1}, ${reg2}, ${reg3}, {imm}",
                        1),
            "lsa");
}

TEST_F(AssemblerMIPS64Test, Dlsa) {
  DriverStr(RepeatRRRIb(&mips64::Mips64Assembler::Dlsa,
                        2,
                        "dlsa ${reg1}, ${reg2}, ${reg3}, {imm}",
                        1),
            "dlsa");
}

TEST_F(AssemblerMIPS64Test, Wsbh) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Wsbh, "wsbh ${reg1}, ${reg2}"), "wsbh");
}

TEST_F(AssemblerMIPS64Test, Sll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sll, 5, "sll ${reg1}, ${reg2}, {imm}"), "sll");
}

TEST_F(AssemblerMIPS64Test, Srl) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Srl, 5, "srl ${reg1}, ${reg2}, {imm}"), "srl");
}

TEST_F(AssemblerMIPS64Test, Rotr) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Rotr, 5, "rotr ${reg1}, ${reg2}, {imm}"), "rotr");
}

TEST_F(AssemblerMIPS64Test, Sra) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sra, 5, "sra ${reg1}, ${reg2}, {imm}"), "sra");
}

TEST_F(AssemblerMIPS64Test, Sllv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Sllv, "sllv ${reg1}, ${reg2}, ${reg3}"), "sllv");
}

TEST_F(AssemblerMIPS64Test, Srlv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Srlv, "srlv ${reg1}, ${reg2}, ${reg3}"), "srlv");
}

TEST_F(AssemblerMIPS64Test, Rotrv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Rotrv, "rotrv ${reg1}, ${reg2}, ${reg3}"), "rotrv");
}

TEST_F(AssemblerMIPS64Test, Srav) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Srav, "srav ${reg1}, ${reg2}, ${reg3}"), "srav");
}

TEST_F(AssemblerMIPS64Test, Dsll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsll, 5, "dsll ${reg1}, ${reg2}, {imm}"), "dsll");
}

TEST_F(AssemblerMIPS64Test, Dsrl) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsrl, 5, "dsrl ${reg1}, ${reg2}, {imm}"), "dsrl");
}

TEST_F(AssemblerMIPS64Test, Drotr) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Drotr, 5, "drotr ${reg1}, ${reg2}, {imm}"),
            "drotr");
}

TEST_F(AssemblerMIPS64Test, Dsra) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsra, 5, "dsra ${reg1}, ${reg2}, {imm}"), "dsra");
}

TEST_F(AssemblerMIPS64Test, Dsll32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsll32, 5, "dsll32 ${reg1}, ${reg2}, {imm}"),
            "dsll32");
}

TEST_F(AssemblerMIPS64Test, Dsrl32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsrl32, 5, "dsrl32 ${reg1}, ${reg2}, {imm}"),
            "dsrl32");
}

TEST_F(AssemblerMIPS64Test, Drotr32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Drotr32, 5, "drotr32 ${reg1}, ${reg2}, {imm}"),
            "drotr32");
}

TEST_F(AssemblerMIPS64Test, Dsra32) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Dsra32, 5, "dsra32 ${reg1}, ${reg2}, {imm}"),
            "dsra32");
}

TEST_F(AssemblerMIPS64Test, Dsllv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dsllv, "dsllv ${reg1}, ${reg2}, ${reg3}"), "dsllv");
}

TEST_F(AssemblerMIPS64Test, Dsrlv) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dsrlv, "dsrlv ${reg1}, ${reg2}, ${reg3}"), "dsrlv");
}

TEST_F(AssemblerMIPS64Test, Dsrav) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Dsrav, "dsrav ${reg1}, ${reg2}, ${reg3}"), "dsrav");
}

TEST_F(AssemblerMIPS64Test, Sc) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Sc, -9, "sc ${reg1}, {imm}(${reg2})"), "sc");
}

TEST_F(AssemblerMIPS64Test, Scd) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Scd, -9, "scd ${reg1}, {imm}(${reg2})"), "scd");
}

TEST_F(AssemblerMIPS64Test, Ll) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Ll, -9, "ll ${reg1}, {imm}(${reg2})"), "ll");
}

TEST_F(AssemblerMIPS64Test, Lld) {
  DriverStr(RepeatRRIb(&mips64::Mips64Assembler::Lld, -9, "lld ${reg1}, {imm}(${reg2})"), "lld");
}

TEST_F(AssemblerMIPS64Test, Seleqz) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Seleqz, "seleqz ${reg1}, ${reg2}, ${reg3}"),
            "seleqz");
}

TEST_F(AssemblerMIPS64Test, Selnez) {
  DriverStr(RepeatRRR(&mips64::Mips64Assembler::Selnez, "selnez ${reg1}, ${reg2}, ${reg3}"),
            "selnez");
}

TEST_F(AssemblerMIPS64Test, Clz) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Clz, "clz ${reg1}, ${reg2}"), "clz");
}

TEST_F(AssemblerMIPS64Test, Clo) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Clo, "clo ${reg1}, ${reg2}"), "clo");
}

TEST_F(AssemblerMIPS64Test, Dclz) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dclz, "dclz ${reg1}, ${reg2}"), "dclz");
}

TEST_F(AssemblerMIPS64Test, Dclo) {
  DriverStr(RepeatRR(&mips64::Mips64Assembler::Dclo, "dclo ${reg1}, ${reg2}"), "dclo");
}

TEST_F(AssemblerMIPS64Test, LoadFromOffset) {
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 1);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x7FFF);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x8001);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x7FFFFFFE);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x7FFFFFFF);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadSignedByte, mips64::A0, mips64::A1, 0x80000001);

  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 1);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x7FFF);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x8001);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x7FFFFFFE);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x7FFFFFFF);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadUnsignedByte, mips64::A0, mips64::A1, 0x80000001);

  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 2);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x8002);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x7FFFFFFE);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadSignedHalfword, mips64::A0, mips64::A1, 0x80000002);

  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 2);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x8002);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x7FFFFFFE);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadUnsignedHalfword, mips64::A0, mips64::A1, 0x80000002);

  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x7FFFFFF8);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadWord, mips64::A0, mips64::A1, 0x80000004);

  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x7FFFFFF8);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadUnsignedWord, mips64::A0, mips64::A1, 0x80000004);

  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A0, 0);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 4);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 256);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 1000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x7FFC);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x8000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x8004);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x10000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x27FFC);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x12345678);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, -256);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, -32768);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0xABCDEF00);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x7FFFFFF8);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x80000000);
  __ LoadFromOffset(mips64::kLoadDoubleword, mips64::A0, mips64::A1, 0x80000004);

  const char* expected =
      "lb $a0, 0($a0)\n"
      "lb $a0, 0($a1)\n"
      "lb $a0, 1($a1)\n"
      "lb $a0, 256($a1)\n"
      "lb $a0, 1000($a1)\n"
      "lb $a0, 0x7FFF($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lb $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lb $a0, 9($at)\n"
      "daui $at, $a1, 1\n"
      "lb $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lb $a0, 0x5678($at)\n"
      "lb $a0, -256($a1)\n"
      "lb $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lb $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lb $a0, -2($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lb $a0, -1($at)\n"
      "daui $at, $a1, 32768\n"
      "lb $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lb $a0, 1($at)\n"

      "lbu $a0, 0($a0)\n"
      "lbu $a0, 0($a1)\n"
      "lbu $a0, 1($a1)\n"
      "lbu $a0, 256($a1)\n"
      "lbu $a0, 1000($a1)\n"
      "lbu $a0, 0x7FFF($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lbu $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lbu $a0, 9($at)\n"
      "daui $at, $a1, 1\n"
      "lbu $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lbu $a0, 0x5678($at)\n"
      "lbu $a0, -256($a1)\n"
      "lbu $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lbu $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lbu $a0, -2($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lbu $a0, -1($at)\n"
      "daui $at, $a1, 32768\n"
      "lbu $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lbu $a0, 1($at)\n"

      "lh $a0, 0($a0)\n"
      "lh $a0, 0($a1)\n"
      "lh $a0, 2($a1)\n"
      "lh $a0, 256($a1)\n"
      "lh $a0, 1000($a1)\n"
      "lh $a0, 0x7FFE($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lh $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lh $a0, 10($at)\n"
      "daui $at, $a1, 1\n"
      "lh $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lh $a0, 0x5678($at)\n"
      "lh $a0, -256($a1)\n"
      "lh $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lh $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lh $a0, -4($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lh $a0, -2($at)\n"
      "daui $at, $a1, 32768\n"
      "lh $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lh $a0, 2($at)\n"

      "lhu $a0, 0($a0)\n"
      "lhu $a0, 0($a1)\n"
      "lhu $a0, 2($a1)\n"
      "lhu $a0, 256($a1)\n"
      "lhu $a0, 1000($a1)\n"
      "lhu $a0, 0x7FFE($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lhu $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lhu $a0, 10($at)\n"
      "daui $at, $a1, 1\n"
      "lhu $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lhu $a0, 0x5678($at)\n"
      "lhu $a0, -256($a1)\n"
      "lhu $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lhu $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lhu $a0, -4($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lhu $a0, -2($at)\n"
      "daui $at, $a1, 32768\n"
      "lhu $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lhu $a0, 2($at)\n"

      "lw $a0, 0($a0)\n"
      "lw $a0, 0($a1)\n"
      "lw $a0, 4($a1)\n"
      "lw $a0, 256($a1)\n"
      "lw $a0, 1000($a1)\n"
      "lw $a0, 0x7FFC($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lw $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lw $a0, 12($at)\n"
      "daui $at, $a1, 1\n"
      "lw $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lw $a0, 0x5678($at)\n"
      "lw $a0, -256($a1)\n"
      "lw $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lw $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lw $a0, -8($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lw $a0, -4($at)\n"
      "daui $at, $a1, 32768\n"
      "lw $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lw $a0, 4($at)\n"

      "lwu $a0, 0($a0)\n"
      "lwu $a0, 0($a1)\n"
      "lwu $a0, 4($a1)\n"
      "lwu $a0, 256($a1)\n"
      "lwu $a0, 1000($a1)\n"
      "lwu $a0, 0x7FFC($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "lwu $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lwu $a0, 12($at)\n"
      "daui $at, $a1, 1\n"
      "lwu $a0, 0($at)\n"
      "daui $at, $a1, 0x1234\n"
      "lwu $a0, 0x5678($at)\n"
      "lwu $a0, -256($a1)\n"
      "lwu $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "lwu $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lwu $a0, -8($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lwu $a0, -4($at)\n"
      "daui $at, $a1, 32768\n"
      "lwu $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lwu $a0, 4($at)\n"

      "ld $a0, 0($a0)\n"
      "ld $a0, 0($a1)\n"
      "lwu $a0, 4($a1)\n"
      "lwu $t3, 8($a1)\n"
      "dinsu $a0, $t3, 32, 32\n"
      "ld $a0, 256($a1)\n"
      "ld $a0, 1000($a1)\n"
      "daddiu $at, $a1, 32760\n"
      "lwu $a0, 4($at)\n"
      "lwu $t3, 8($at)\n"
      "dinsu $a0, $t3, 32, 32\n"
      "daddiu $at, $a1, 32760\n"
      "ld $a0, 8($at)\n"
      "daddiu $at, $a1, 32760\n"
      "lwu $a0, 12($at)\n"
      "lwu $t3, 16($at)\n"
      "dinsu $a0, $t3, 32, 32\n"
      "daui $at, $a1, 1\n"
      "ld $a0, 0($at)\n"
      "daui $at, $a1, 2\n"
      "daddiu $at, $at, 8\n"
      "lwu $a0, 0x7ff4($at)\n"
      "lwu $t3, 0x7ff8($at)\n"
      "dinsu $a0, $t3, 32, 32\n"
      "daui $at, $a1, 0x1234\n"
      "ld $a0, 0x5678($at)\n"
      "ld $a0, -256($a1)\n"
      "ld $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "ld $a0, -4352($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "ld $a0, -8($at)\n"
      "daui $at, $a1, 32768\n"
      "dahi $at, $at, 1\n"
      "lwu $a0, -4($at)\n"
      "lwu $t3, 0($at)\n"
      "dinsu $a0, $t3, 32, 32\n"
      "daui $at, $a1, 32768\n"
      "ld $a0, 0($at)\n"
      "daui $at, $a1, 32768\n"
      "lwu $a0, 4($at)\n"
      "lwu $t3, 8($at)\n"
      "dinsu $a0, $t3, 32, 32\n";
  DriverStr(expected, "LoadFromOffset");
}

TEST_F(AssemblerMIPS64Test, LoadFpuFromOffset) {
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 4);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 256);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x7FFC);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x8000);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x8004);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x10000);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0x12345678);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, -256);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, -32768);
  __ LoadFpuFromOffset(mips64::kLoadWord, mips64::F0, mips64::A0, 0xABCDEF00);

  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 4);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 256);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x7FFC);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x8000);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x8004);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x10000);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0x12345678);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, -256);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, -32768);
  __ LoadFpuFromOffset(mips64::kLoadDoubleword, mips64::F0, mips64::A0, 0xABCDEF00);

  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 8);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 511);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 512);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 513);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 514);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 516);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1022);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1024);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1025);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1026);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 1028);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2044);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2048);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2049);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2050);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 2052);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4088);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4096);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4097);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4098);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4100);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 4104);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x7FFC);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x8000);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x10000);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x12345678);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x12350078);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -256);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -511);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -513);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -1022);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -1026);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -2044);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -2052);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -4096);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -4104);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, -32768);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0xABCDEF00);
  __ LoadFpuFromOffset(mips64::kLoadQuadword, mips64::F0, mips64::A0, 0x7FFFABCD);

  const char* expected =
      "lwc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lwc1 $f0, 256($a0)\n"
      "lwc1 $f0, 0x7FFC($a0)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "lwc1 $f0, 8($at)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "lwc1 $f0, 12($at)\n"
      "daui $at, $a0, 1\n"
      "lwc1 $f0, 0($at)\n"
      "daui $at, $a0, 4660 # 0x1234\n"
      "lwc1 $f0, 22136($at) # 0x5678\n"
      "lwc1 $f0, -256($a0)\n"
      "lwc1 $f0, -32768($a0)\n"
      "daui $at, $a0, 0xABCE\n"
      "lwc1 $f0, -0x1100($at) # 0xEF00\n"

      "ldc1 $f0, 0($a0)\n"
      "lwc1 $f0, 4($a0)\n"
      "lw $t3, 8($a0)\n"
      "mthc1 $t3, $f0\n"
      "ldc1 $f0, 256($a0)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "lwc1 $f0, 4($at)\n"
      "lw $t3, 8($at)\n"
      "mthc1 $t3, $f0\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "ldc1 $f0, 8($at)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "lwc1 $f0, 12($at)\n"
      "lw $t3, 16($at)\n"
      "mthc1 $t3, $f0\n"
      "daui $at, $a0, 1\n"
      "ldc1 $f0, 0($at)\n"
      "daui $at, $a0, 4660 # 0x1234\n"
      "ldc1 $f0, 22136($at) # 0x5678\n"
      "ldc1 $f0, -256($a0)\n"
      "ldc1 $f0, -32768($a0)\n"
      "daui $at, $a0, 0xABCE\n"
      "ldc1 $f0, -0x1100($at) # 0xEF00\n"

      "ld.d $w0, 0($a0)\n"
      "ld.b $w0, 1($a0)\n"
      "ld.h $w0, 2($a0)\n"
      "ld.w $w0, 4($a0)\n"
      "ld.d $w0, 8($a0)\n"
      "ld.b $w0, 511($a0)\n"
      "ld.d $w0, 512($a0)\n"
      "daddiu $at, $a0, 513\n"
      "ld.b $w0, 0($at)\n"
      "ld.h $w0, 514($a0)\n"
      "ld.w $w0, 516($a0)\n"
      "ld.h $w0, 1022($a0)\n"
      "ld.d $w0, 1024($a0)\n"
      "daddiu $at, $a0, 1025\n"
      "ld.b $w0, 0($at)\n"
      "daddiu $at, $a0, 1026\n"
      "ld.h $w0, 0($at)\n"
      "ld.w $w0, 1028($a0)\n"
      "ld.w $w0, 2044($a0)\n"
      "ld.d $w0, 2048($a0)\n"
      "daddiu $at, $a0, 2049\n"
      "ld.b $w0, 0($at)\n"
      "daddiu $at, $a0, 2050\n"
      "ld.h $w0, 0($at)\n"
      "daddiu $at, $a0, 2052\n"
      "ld.w $w0, 0($at)\n"
      "ld.d $w0, 4088($a0)\n"
      "daddiu $at, $a0, 4096\n"
      "ld.d $w0, 0($at)\n"
      "daddiu $at, $a0, 4097\n"
      "ld.b $w0, 0($at)\n"
      "daddiu $at, $a0, 4098\n"
      "ld.h $w0, 0($at)\n"
      "daddiu $at, $a0, 4100\n"
      "ld.w $w0, 0($at)\n"
      "daddiu $at, $a0, 4104\n"
      "ld.d $w0, 0($at)\n"
      "daddiu $at, $a0, 0x7FFC\n"
      "ld.w $w0, 0($at)\n"
      "daddiu $at, $a0, 0x7FF8\n"
      "ld.d $w0, 8($at)\n"
      "daui $at, $a0, 0x1\n"
      "ld.d $w0, 0($at)\n"
      "daui $at, $a0, 0x1234\n"
      "daddiu $at, $at, 0x6000\n"
      "ld.d $w0, -2440($at) # 0xF678\n"
      "daui $at, $a0, 0x1235\n"
      "ld.d $w0, 0x78($at)\n"
      "ld.d $w0, -256($a0)\n"
      "ld.b $w0, -511($a0)\n"
      "daddiu $at, $a0, -513\n"
      "ld.b $w0, 0($at)\n"
      "ld.h $w0, -1022($a0)\n"
      "daddiu $at, $a0, -1026\n"
      "ld.h $w0, 0($at)\n"
      "ld.w $w0, -2044($a0)\n"
      "daddiu $at, $a0, -2052\n"
      "ld.w $w0, 0($at)\n"
      "ld.d $w0, -4096($a0)\n"
      "daddiu $at, $a0, -4104\n"
      "ld.d $w0, 0($at)\n"
      "daddiu $at, $a0, -32768\n"
      "ld.d $w0, 0($at)\n"
      "daui $at, $a0, 0xABCE\n"
      "daddiu $at, $at, -8192 # 0xE000\n"
      "ld.d $w0, 0xF00($at)\n"
      "daui $at, $a0, 0x8000\n"
      "dahi $at, $at, 1\n"
      "daddiu $at, $at, -21504 # 0xAC00\n"
      "ld.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "LoadFpuFromOffset");
}

TEST_F(AssemblerMIPS64Test, StoreToOffset) {
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 1);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x7FFF);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x8001);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreByte, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 2);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x7FFE);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x8002);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreHalfword, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 4);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x7FFC);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x8004);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreWord, mips64::A0, mips64::A1, 0xABCDEF00);

  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A0, 0);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 4);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 256);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 1000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x7FFC);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x8000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x8004);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x10000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x12345678);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, -256);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, -32768);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0xABCDEF00);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x7FFFFFF8);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x7FFFFFFC);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x80000000);
  __ StoreToOffset(mips64::kStoreDoubleword, mips64::A0, mips64::A1, 0x80000004);

  const char* expected =
      "sb $a0, 0($a0)\n"
      "sb $a0, 0($a1)\n"
      "sb $a0, 1($a1)\n"
      "sb $a0, 256($a1)\n"
      "sb $a0, 1000($a1)\n"
      "sb $a0, 0x7FFF($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sb $a0, 8($at)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sb $a0, 9($at)\n"
      "daui $at, $a1, 1\n"
      "sb $a0, 0($at)\n"
      "daui $at, $a1, 4660 # 0x1234\n"
      "sb $a0, 22136($at) # 0x5678\n"
      "sb $a0, -256($a1)\n"
      "sb $a0, -32768($a1)\n"
      "daui $at, $a1, 43982 # 0xABCE\n"
      "sb $a0, -4352($at) # 0xEF00\n"

      "sh $a0, 0($a0)\n"
      "sh $a0, 0($a1)\n"
      "sh $a0, 2($a1)\n"
      "sh $a0, 256($a1)\n"
      "sh $a0, 1000($a1)\n"
      "sh $a0, 0x7FFE($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sh $a0, 8($at)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sh $a0, 10($at)\n"
      "daui $at, $a1, 1\n"
      "sh $a0, 0($at)\n"
      "daui $at, $a1, 4660 # 0x1234\n"
      "sh $a0, 22136($at) # 0x5678\n"
      "sh $a0, -256($a1)\n"
      "sh $a0, -32768($a1)\n"
      "daui $at, $a1, 43982 # 0xABCE\n"
      "sh $a0, -4352($at) # 0xEF00\n"

      "sw $a0, 0($a0)\n"
      "sw $a0, 0($a1)\n"
      "sw $a0, 4($a1)\n"
      "sw $a0, 256($a1)\n"
      "sw $a0, 1000($a1)\n"
      "sw $a0, 0x7FFC($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sw $a0, 8($at)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sw $a0, 12($at)\n"
      "daui $at, $a1, 1\n"
      "sw $a0, 0($at)\n"
      "daui $at, $a1, 4660 # 0x1234\n"
      "sw $a0, 22136($at) # 0x5678\n"
      "sw $a0, -256($a1)\n"
      "sw $a0, -32768($a1)\n"
      "daui $at, $a1, 43982 # 0xABCE\n"
      "sw $a0, -4352($at) # 0xEF00\n"

      "sd $a0, 0($a0)\n"
      "sd $a0, 0($a1)\n"
      "sw $a0, 4($a1)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($a1)\n"
      "sd $a0, 256($a1)\n"
      "sd $a0, 1000($a1)\n"
      "daddiu $at, $a1, 0x7FF8\n"
      "sw $a0, 4($at)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($at)\n"
      "daddiu $at, $a1, 32760 # 0x7FF8\n"
      "sd $a0, 8($at)\n"
      "daddiu $at, $a1, 32760 # 0x7FF8\n"
      "sw $a0, 12($at)\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 16($at)\n"
      "daui $at, $a1, 1\n"
      "sd $a0, 0($at)\n"
      "daui $at, $a1, 4660 # 0x1234\n"
      "sd $a0, 22136($at) # 0x5678\n"
      "sd $a0, -256($a1)\n"
      "sd $a0, -32768($a1)\n"
      "daui $at, $a1, 0xABCE\n"
      "sd $a0, -0x1100($at)\n"
      "daui $at, $a1, 0x8000\n"
      "dahi $at, $at, 1\n"
      "sd $a0, -8($at)\n"
      "daui $at, $a1, 0x8000\n"
      "dahi $at, $at, 1\n"
      "sw $a0, -4($at) # 0xFFFC\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 0($at) # 0x0\n"
      "daui $at, $a1, 0x8000\n"
      "sd $a0, 0($at) # 0x0\n"
      "daui $at, $a1, 0x8000\n"
      "sw $a0, 4($at) # 0x4\n"
      "dsrl32 $t3, $a0, 0\n"
      "sw $t3, 8($at) # 0x8\n";
  DriverStr(expected, "StoreToOffset");
}

TEST_F(AssemblerMIPS64Test, StoreFpuToOffset) {
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 4);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 256);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x7FFC);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x8000);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x8004);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x10000);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0x12345678);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, -256);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, -32768);
  __ StoreFpuToOffset(mips64::kStoreWord, mips64::F0, mips64::A0, 0xABCDEF00);

  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 4);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 256);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x7FFC);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x8000);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x8004);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x10000);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0x12345678);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, -256);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, -32768);
  __ StoreFpuToOffset(mips64::kStoreDoubleword, mips64::F0, mips64::A0, 0xABCDEF00);

  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 8);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 511);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 512);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 513);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 514);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 516);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1022);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1024);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1025);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1026);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 1028);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2044);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2048);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2049);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2050);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 2052);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4088);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4096);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4097);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4098);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4100);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 4104);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x7FFC);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x8000);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x10000);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x12345678);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x12350078);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -256);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -511);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -513);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -1022);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -1026);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -2044);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -2052);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -4096);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -4104);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, -32768);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0xABCDEF00);
  __ StoreFpuToOffset(mips64::kStoreQuadword, mips64::F0, mips64::A0, 0x7FFFABCD);

  const char* expected =
      "swc1 $f0, 0($a0)\n"
      "swc1 $f0, 4($a0)\n"
      "swc1 $f0, 256($a0)\n"
      "swc1 $f0, 0x7FFC($a0)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "swc1 $f0, 8($at)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "swc1 $f0, 12($at)\n"
      "daui $at, $a0, 1\n"
      "swc1 $f0, 0($at)\n"
      "daui $at, $a0, 4660 # 0x1234\n"
      "swc1 $f0, 22136($at) # 0x5678\n"
      "swc1 $f0, -256($a0)\n"
      "swc1 $f0, -32768($a0)\n"
      "daui $at, $a0, 0xABCE\n"
      "swc1 $f0, -0x1100($at)\n"

      "sdc1 $f0, 0($a0)\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 4($a0)\n"
      "sw $t3, 8($a0)\n"
      "sdc1 $f0, 256($a0)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 4($at)\n"
      "sw $t3, 8($at)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "sdc1 $f0, 8($at)\n"
      "daddiu $at, $a0, 32760 # 0x7FF8\n"
      "mfhc1 $t3, $f0\n"
      "swc1 $f0, 12($at)\n"
      "sw $t3, 16($at)\n"
      "daui $at, $a0, 1\n"
      "sdc1 $f0, 0($at)\n"
      "daui $at, $a0, 4660 # 0x1234\n"
      "sdc1 $f0, 22136($at) # 0x5678\n"
      "sdc1 $f0, -256($a0)\n"
      "sdc1 $f0, -32768($a0)\n"
      "daui $at, $a0, 0xABCE\n"
      "sdc1 $f0, -0x1100($at)\n"

      "st.d $w0, 0($a0)\n"
      "st.b $w0, 1($a0)\n"
      "st.h $w0, 2($a0)\n"
      "st.w $w0, 4($a0)\n"
      "st.d $w0, 8($a0)\n"
      "st.b $w0, 511($a0)\n"
      "st.d $w0, 512($a0)\n"
      "daddiu $at, $a0, 513\n"
      "st.b $w0, 0($at)\n"
      "st.h $w0, 514($a0)\n"
      "st.w $w0, 516($a0)\n"
      "st.h $w0, 1022($a0)\n"
      "st.d $w0, 1024($a0)\n"
      "daddiu $at, $a0, 1025\n"
      "st.b $w0, 0($at)\n"
      "daddiu $at, $a0, 1026\n"
      "st.h $w0, 0($at)\n"
      "st.w $w0, 1028($a0)\n"
      "st.w $w0, 2044($a0)\n"
      "st.d $w0, 2048($a0)\n"
      "daddiu $at, $a0, 2049\n"
      "st.b $w0, 0($at)\n"
      "daddiu $at, $a0, 2050\n"
      "st.h $w0, 0($at)\n"
      "daddiu $at, $a0, 2052\n"
      "st.w $w0, 0($at)\n"
      "st.d $w0, 4088($a0)\n"
      "daddiu $at, $a0, 4096\n"
      "st.d $w0, 0($at)\n"
      "daddiu $at, $a0, 4097\n"
      "st.b $w0, 0($at)\n"
      "daddiu $at, $a0, 4098\n"
      "st.h $w0, 0($at)\n"
      "daddiu $at, $a0, 4100\n"
      "st.w $w0, 0($at)\n"
      "daddiu $at, $a0, 4104\n"
      "st.d $w0, 0($at)\n"
      "daddiu $at, $a0, 0x7FFC\n"
      "st.w $w0, 0($at)\n"
      "daddiu $at, $a0, 0x7FF8\n"
      "st.d $w0, 8($at)\n"
      "daui $at, $a0, 0x1\n"
      "st.d $w0, 0($at)\n"
      "daui $at, $a0, 0x1234\n"
      "daddiu $at, $at, 0x6000\n"
      "st.d $w0, -2440($at) # 0xF678\n"
      "daui $at, $a0, 0x1235\n"
      "st.d $w0, 0x78($at)\n"
      "st.d $w0, -256($a0)\n"
      "st.b $w0, -511($a0)\n"
      "daddiu $at, $a0, -513\n"
      "st.b $w0, 0($at)\n"
      "st.h $w0, -1022($a0)\n"
      "daddiu $at, $a0, -1026\n"
      "st.h $w0, 0($at)\n"
      "st.w $w0, -2044($a0)\n"
      "daddiu $at, $a0, -2052\n"
      "st.w $w0, 0($at)\n"
      "st.d $w0, -4096($a0)\n"
      "daddiu $at, $a0, -4104\n"
      "st.d $w0, 0($at)\n"
      "daddiu $at, $a0, -32768\n"
      "st.d $w0, 0($at)\n"
      "daui $at, $a0, 0xABCE\n"
      "daddiu $at, $at, -8192 # 0xE000\n"
      "st.d $w0, 0xF00($at)\n"
      "daui $at, $a0, 0x8000\n"
      "dahi $at, $at, 1\n"
      "daddiu $at, $at, -21504 # 0xAC00\n"
      "st.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "StoreFpuToOffset");
}

TEST_F(AssemblerMIPS64Test, StoreConstToOffset) {
  __ StoreConstToOffset(mips64::kStoreByte, 0xFF, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreHalfword, 0xFFFF, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreWord, 0x12345678, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreDoubleword, 0x123456789ABCDEF0, mips64::A1, +0, mips64::T8);

  __ StoreConstToOffset(mips64::kStoreByte, 0, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreHalfword, 0, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreWord, 0, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreDoubleword, 0, mips64::A1, +0, mips64::T8);

  __ StoreConstToOffset(mips64::kStoreDoubleword, 0x1234567812345678, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreDoubleword, 0x1234567800000000, mips64::A1, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreDoubleword, 0x0000000012345678, mips64::A1, +0, mips64::T8);

  __ StoreConstToOffset(mips64::kStoreWord, 0, mips64::T8, +0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreWord, 0x12345678, mips64::T8, +0, mips64::T8);

  __ StoreConstToOffset(mips64::kStoreWord, 0, mips64::A1, -0xFFF0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreWord, 0x12345678, mips64::A1, +0xFFF0, mips64::T8);

  __ StoreConstToOffset(mips64::kStoreWord, 0, mips64::T8, -0xFFF0, mips64::T8);
  __ StoreConstToOffset(mips64::kStoreWord, 0x12345678, mips64::T8, +0xFFF0, mips64::T8);

  const char* expected =
      "ori $t8, $zero, 0xFF\n"
      "sb $t8, 0($a1)\n"
      "ori $t8, $zero, 0xFFFF\n"
      "sh $t8, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8,0x5678\n"
      "sw $t8, 0($a1)\n"
      "lui $t8, 0x9abc\n"
      "ori $t8, $t8,0xdef0\n"
      "dahi $t8, $t8, 0x5679\n"
      "dati $t8, $t8, 0x1234\n"
      "sd $t8, 0($a1)\n"
      "sb $zero, 0($a1)\n"
      "sh $zero, 0($a1)\n"
      "sw $zero, 0($a1)\n"
      "sd $zero, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8,0x5678\n"
      "dins $t8, $t8, 0x20, 0x20\n"
      "sd $t8, 0($a1)\n"
      "lui $t8, 0x246\n"
      "ori $t8, $t8, 0x8acf\n"
      "dsll32 $t8, $t8, 0x3\n"
      "sd $t8, 0($a1)\n"
      "lui $t8, 0x1234\n"
      "ori $t8, $t8, 0x5678\n"
      "sd $t8, 0($a1)\n"
      "sw $zero, 0($t8)\n"
      "lui $at,0x1234\n"
      "ori $at, $at, 0x5678\n"
      "sw  $at, 0($t8)\n"
      "daddiu $at, $a1, -32760 # 0x8008\n"
      "sw $zero, -32760($at) # 0x8008\n"
      "daddiu $at, $a1, 32760 # 0x7FF8\n"
      "lui $t8, 4660 # 0x1234\n"
      "ori $t8, $t8, 22136 # 0x5678\n"
      "sw $t8, 32760($at) # 0x7FF8\n"
      "daddiu $at, $t8, -32760 # 0x8008\n"
      "sw $zero, -32760($at) # 0x8008\n"
      "daddiu $at, $t8, 32760 # 0x7FF8\n"
      "lui $t8, 4660 # 0x1234\n"
      "ori $t8, $t8, 22136 # 0x5678\n"
      "sw $t8, 32760($at) # 0x7FF8\n";
  DriverStr(expected, "StoreConstToOffset");
}
//////////////////////////////
// Loading/adding Constants //
//////////////////////////////

TEST_F(AssemblerMIPS64Test, LoadConst32) {
  // IsUint<16>(value)
  __ LoadConst32(mips64::V0, 0);
  __ LoadConst32(mips64::V0, 65535);
  // IsInt<16>(value)
  __ LoadConst32(mips64::V0, -1);
  __ LoadConst32(mips64::V0, -32768);
  // Everything else
  __ LoadConst32(mips64::V0, 65536);
  __ LoadConst32(mips64::V0, 65537);
  __ LoadConst32(mips64::V0, 2147483647);
  __ LoadConst32(mips64::V0, -32769);
  __ LoadConst32(mips64::V0, -65536);
  __ LoadConst32(mips64::V0, -65537);
  __ LoadConst32(mips64::V0, -2147483647);
  __ LoadConst32(mips64::V0, -2147483648);

  const char* expected =
      // IsUint<16>(value)
      "ori $v0, $zero, 0\n"         // __ LoadConst32(mips64::V0, 0);
      "ori $v0, $zero, 65535\n"     // __ LoadConst32(mips64::V0, 65535);
      // IsInt<16>(value)
      "addiu $v0, $zero, -1\n"      // __ LoadConst32(mips64::V0, -1);
      "addiu $v0, $zero, -32768\n"  // __ LoadConst32(mips64::V0, -32768);
      // Everything else
      "lui $v0, 1\n"                // __ LoadConst32(mips64::V0, 65536);
      "lui $v0, 1\n"                // __ LoadConst32(mips64::V0, 65537);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32767\n"            // __ LoadConst32(mips64::V0, 2147483647);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips64::V0, -32769);
      "ori $v0, 32767\n"            //                 "
      "lui $v0, 65535\n"            // __ LoadConst32(mips64::V0, -65536);
      "lui $v0, 65534\n"            // __ LoadConst32(mips64::V0, -65537);
      "ori $v0, 65535\n"            //                 "
      "lui $v0, 32768\n"            // __ LoadConst32(mips64::V0, -2147483647);
      "ori $v0, 1\n"                //                 "
      "lui $v0, 32768\n";           // __ LoadConst32(mips64::V0, -2147483648);
  DriverStr(expected, "LoadConst32");
}

TEST_F(AssemblerMIPS64Test, Addiu32) {
  __ Addiu32(mips64::A1, mips64::A2, -0x8000);
  __ Addiu32(mips64::A1, mips64::A2, +0);
  __ Addiu32(mips64::A1, mips64::A2, +0x7FFF);
  __ Addiu32(mips64::A1, mips64::A2, -0x8001);
  __ Addiu32(mips64::A1, mips64::A2, +0x8000);
  __ Addiu32(mips64::A1, mips64::A2, -0x10000);
  __ Addiu32(mips64::A1, mips64::A2, +0x10000);
  __ Addiu32(mips64::A1, mips64::A2, +0x12345678);

  const char* expected =
      "addiu $a1, $a2, -0x8000\n"
      "addiu $a1, $a2, 0\n"
      "addiu $a1, $a2, 0x7FFF\n"
      "aui $a1, $a2, 0xFFFF\n"
      "addiu $a1, $a1, 0x7FFF\n"
      "aui $a1, $a2, 1\n"
      "addiu $a1, $a1, -0x8000\n"
      "aui $a1, $a2, 0xFFFF\n"
      "aui $a1, $a2, 1\n"
      "aui $a1, $a2, 0x1234\n"
      "addiu $a1, $a1, 0x5678\n";
  DriverStr(expected, "Addiu32");
}

static uint64_t SignExtend16To64(uint16_t n) {
  return static_cast<int16_t>(n);
}

// The art::mips64::Mips64Assembler::LoadConst64() method uses a template
// to minimize the number of instructions needed to load a 64-bit constant
// value into a register. The template calls various methods which emit
// MIPS machine instructions. This struct (class) uses the same template
// but overrides the definitions of the methods which emit MIPS instructions
// to use methods which simulate the operation of the corresponding MIPS
// instructions. After invoking LoadConst64() the target register should
// contain the same 64-bit value as was input to LoadConst64(). If the
// simulated register doesn't contain the correct value then there is probably
// an error in the template function.
struct LoadConst64Tester {
  LoadConst64Tester() {
    // Initialize all of the registers for simulation to zero.
    for (int r = 0; r < 32; r++) {
      regs_[r] = 0;
    }
    // Clear all of the path flags.
    loadconst64_paths_ = art::mips64::kLoadConst64PathZero;
  }
  void Addiu(mips64::GpuRegister rd, mips64::GpuRegister rs, uint16_t c) {
    regs_[rd] = static_cast<int32_t>(regs_[rs] + SignExtend16To64(c));
  }
  void Daddiu(mips64::GpuRegister rd, mips64::GpuRegister rs, uint16_t c) {
    regs_[rd] = regs_[rs] + SignExtend16To64(c);
  }
  void Dahi(mips64::GpuRegister rd, uint16_t c) {
    regs_[rd] += SignExtend16To64(c) << 32;
  }
  void Dati(mips64::GpuRegister rd, uint16_t c) {
    regs_[rd] += SignExtend16To64(c) << 48;
  }
  void Dinsu(mips64::GpuRegister rt, mips64::GpuRegister rs, int pos, int size) {
    CHECK(IsUint<5>(pos - 32)) << pos;
    CHECK(IsUint<5>(size - 1)) << size;
    CHECK(IsUint<5>(pos + size - 33)) << pos << " + " << size;
    uint64_t src_mask = (UINT64_C(1) << size) - 1;
    uint64_t dsk_mask = ~(src_mask << pos);

    regs_[rt] = (regs_[rt] & dsk_mask) | ((regs_[rs] & src_mask) << pos);
  }
  void Dsll(mips64::GpuRegister rd, mips64::GpuRegister rt, int shamt) {
    regs_[rd] = regs_[rt] << (shamt & 0x1f);
  }
  void Dsll32(mips64::GpuRegister rd, mips64::GpuRegister rt, int shamt) {
    regs_[rd] = regs_[rt] << (32 + (shamt & 0x1f));
  }
  void Dsrl(mips64::GpuRegister rd, mips64::GpuRegister rt, int shamt) {
    regs_[rd] = regs_[rt] >> (shamt & 0x1f);
  }
  void Dsrl32(mips64::GpuRegister rd, mips64::GpuRegister rt, int shamt) {
    regs_[rd] = regs_[rt] >> (32 + (shamt & 0x1f));
  }
  void Lui(mips64::GpuRegister rd, uint16_t c) {
    regs_[rd] = SignExtend16To64(c) << 16;
  }
  void Ori(mips64::GpuRegister rd, mips64::GpuRegister rs, uint16_t c) {
    regs_[rd] = regs_[rs] | c;
  }
  void LoadConst32(mips64::GpuRegister rd, int32_t c) {
    CHECK_NE(rd, 0);
    mips64::TemplateLoadConst32<LoadConst64Tester>(this, rd, c);
    CHECK_EQ(regs_[rd], static_cast<uint64_t>(c));
  }
  void LoadConst64(mips64::GpuRegister rd, int64_t c) {
    CHECK_NE(rd, 0);
    mips64::TemplateLoadConst64<LoadConst64Tester>(this, rd, c);
    CHECK_EQ(regs_[rd], static_cast<uint64_t>(c));
  }
  uint64_t regs_[32];

  // Getter function for loadconst64_paths_.
  int GetPathsCovered() {
    return loadconst64_paths_;
  }

  void RecordLoadConst64Path(int value) {
    loadconst64_paths_ |= value;
  }

 private:
  // This variable holds a bitmask to tell us which paths were taken
  // through the template function which loads 64-bit values.
  int loadconst64_paths_;
};

TEST_F(AssemblerMIPS64Test, LoadConst64) {
  const uint16_t imms[] = {
      0, 1, 2, 3, 4, 0x33, 0x66, 0x55, 0x99, 0xaa, 0xcc, 0xff, 0x5500, 0x5555,
      0x7ffc, 0x7ffd, 0x7ffe, 0x7fff, 0x8000, 0x8001, 0x8002, 0x8003, 0x8004,
      0xaaaa, 0xfffc, 0xfffd, 0xfffe, 0xffff
  };
  unsigned d0, d1, d2, d3;
  LoadConst64Tester tester;

  union {
    int64_t v64;
    uint16_t v16[4];
  } u;

  for (d3 = 0; d3 < sizeof imms / sizeof imms[0]; d3++) {
    u.v16[3] = imms[d3];

    for (d2 = 0; d2 < sizeof imms / sizeof imms[0]; d2++) {
      u.v16[2] = imms[d2];

      for (d1 = 0; d1 < sizeof imms / sizeof imms[0]; d1++) {
        u.v16[1] = imms[d1];

        for (d0 = 0; d0 < sizeof imms / sizeof imms[0]; d0++) {
          u.v16[0] = imms[d0];

          tester.LoadConst64(mips64::V0, u.v64);
        }
      }
    }
  }

  // Verify that we tested all paths through the "load 64-bit value"
  // function template.
  EXPECT_EQ(tester.GetPathsCovered(), art::mips64::kLoadConst64PathAllPaths);
}

TEST_F(AssemblerMIPS64Test, LoadFarthestNearLabelAddress) {
  mips64::Mips64Label label;
  __ LoadLabelAddress(mips64::V0, &label);
  constexpr uint32_t kAdduCount = 0x3FFDE;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      "lapc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n";
  DriverStr(expected, "LoadFarthestNearLabelAddress");
  EXPECT_EQ(__ GetLabelLocation(&label), (1 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadNearestFarLabelAddress) {
  mips64::Mips64Label label;
  __ LoadLabelAddress(mips64::V0, &label);
  constexpr uint32_t kAdduCount = 0x3FFDF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  __ Bind(&label);

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "daddiu $v0, $at, %lo(2f - 1b)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n";
  DriverStr(expected, "LoadNearestFarLabelAddress");
  EXPECT_EQ(__ GetLabelLocation(&label), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadFarthestNearLiteral) {
  mips64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips64::V0, mips64::kLoadWord, literal);
  constexpr uint32_t kAdduCount = 0x3FFDE;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "lwpc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadFarthestNearLiteral");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (1 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadNearestFarLiteral) {
  mips64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips64::V0, mips64::kLoadWord, literal);
  constexpr uint32_t kAdduCount = 0x3FFDF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "lw $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadNearestFarLiteral");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadFarthestNearLiteralUnsigned) {
  mips64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips64::V0, mips64::kLoadUnsignedWord, literal);
  constexpr uint32_t kAdduCount = 0x3FFDE;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "lwupc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadFarthestNearLiteralUnsigned");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (1 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadNearestFarLiteralUnsigned) {
  mips64::Literal* literal = __ NewLiteral<uint32_t>(0x12345678);
  __ LoadLiteral(mips64::V0, mips64::kLoadUnsignedWord, literal);
  constexpr uint32_t kAdduCount = 0x3FFDF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "lwu $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "LoadNearestFarLiteralUnsigned");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadFarthestNearLiteralLong) {
  mips64::Literal* literal = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  __ LoadLiteral(mips64::V0, mips64::kLoadDoubleword, literal);
  constexpr uint32_t kAdduCount = 0x3FFDD;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "ldpc $v0, 1f\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "1:\n"
      ".dword 0x0123456789ABCDEF\n";
  DriverStr(expected, "LoadFarthestNearLiteralLong");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (1 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LoadNearestFarLiteralLong) {
  mips64::Literal* literal = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  __ LoadLiteral(mips64::V0, mips64::kLoadDoubleword, literal);
  constexpr uint32_t kAdduCount = 0x3FFDE;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }

  std::string expected =
      "1:\n"
      "auipc $at, %hi(2f - 1b)\n"
      "ld $v0, %lo(2f - 1b)($at)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "2:\n"
      ".dword 0x0123456789ABCDEF\n";
  DriverStr(expected, "LoadNearestFarLiteralLong");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (2 + kAdduCount) * 4);
}

TEST_F(AssemblerMIPS64Test, LongLiteralAlignmentNop) {
  mips64::Literal* literal1 = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  mips64::Literal* literal2 = __ NewLiteral<uint64_t>(UINT64_C(0x5555555555555555));
  mips64::Literal* literal3 = __ NewLiteral<uint64_t>(UINT64_C(0xAAAAAAAAAAAAAAAA));
  __ LoadLiteral(mips64::A1, mips64::kLoadDoubleword, literal1);
  __ LoadLiteral(mips64::A2, mips64::kLoadDoubleword, literal2);
  __ LoadLiteral(mips64::A3, mips64::kLoadDoubleword, literal3);
  __ LoadLabelAddress(mips64::V0, literal1->GetLabel());
  __ LoadLabelAddress(mips64::V1, literal2->GetLabel());
  // A nop will be inserted here before the 64-bit literals.

  std::string expected =
      "ldpc $a1, 1f\n"
      // The GNU assembler incorrectly requires the ldpc instruction to be located
      // at an address that's a multiple of 8. TODO: Remove this workaround if/when
      // the assembler is fixed.
      // "ldpc $a2, 2f\n"
      ".word 0xECD80004\n"
      "ldpc $a3, 3f\n"
      "lapc $v0, 1f\n"
      "lapc $v1, 2f\n"
      "nop\n"
      "1:\n"
      ".dword 0x0123456789ABCDEF\n"
      "2:\n"
      ".dword 0x5555555555555555\n"
      "3:\n"
      ".dword 0xAAAAAAAAAAAAAAAA\n";
  DriverStr(expected, "LongLiteralAlignmentNop");
  EXPECT_EQ(__ GetLabelLocation(literal1->GetLabel()), 6 * 4u);
  EXPECT_EQ(__ GetLabelLocation(literal2->GetLabel()), 8 * 4u);
  EXPECT_EQ(__ GetLabelLocation(literal3->GetLabel()), 10 * 4u);
}

TEST_F(AssemblerMIPS64Test, LongLiteralAlignmentNoNop) {
  mips64::Literal* literal1 = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  mips64::Literal* literal2 = __ NewLiteral<uint64_t>(UINT64_C(0x5555555555555555));
  __ LoadLiteral(mips64::A1, mips64::kLoadDoubleword, literal1);
  __ LoadLiteral(mips64::A2, mips64::kLoadDoubleword, literal2);
  __ LoadLabelAddress(mips64::V0, literal1->GetLabel());
  __ LoadLabelAddress(mips64::V1, literal2->GetLabel());

  std::string expected =
      "ldpc $a1, 1f\n"
      // The GNU assembler incorrectly requires the ldpc instruction to be located
      // at an address that's a multiple of 8. TODO: Remove this workaround if/when
      // the assembler is fixed.
      // "ldpc $a2, 2f\n"
      ".word 0xECD80003\n"
      "lapc $v0, 1f\n"
      "lapc $v1, 2f\n"
      "1:\n"
      ".dword 0x0123456789ABCDEF\n"
      "2:\n"
      ".dword 0x5555555555555555\n";
  DriverStr(expected, "LongLiteralAlignmentNoNop");
  EXPECT_EQ(__ GetLabelLocation(literal1->GetLabel()), 4 * 4u);
  EXPECT_EQ(__ GetLabelLocation(literal2->GetLabel()), 6 * 4u);
}

TEST_F(AssemblerMIPS64Test, FarLongLiteralAlignmentNop) {
  mips64::Literal* literal = __ NewLiteral<uint64_t>(UINT64_C(0x0123456789ABCDEF));
  __ LoadLiteral(mips64::V0, mips64::kLoadDoubleword, literal);
  __ LoadLabelAddress(mips64::V1, literal->GetLabel());
  constexpr uint32_t kAdduCount = 0x3FFDF;
  for (uint32_t i = 0; i != kAdduCount; ++i) {
    __ Addu(mips64::ZERO, mips64::ZERO, mips64::ZERO);
  }
  // A nop will be inserted here before the 64-bit literal.

  std::string expected =
      "1:\n"
      "auipc $at, %hi(3f - 1b)\n"
      "ld $v0, %lo(3f - 1b)($at)\n"
      "2:\n"
      "auipc $at, %hi(3f - 2b)\n"
      "daddiu $v1, $at, %lo(3f - 2b)\n" +
      RepeatInsn(kAdduCount, "addu $zero, $zero, $zero\n") +
      "nop\n"
      "3:\n"
      ".dword 0x0123456789ABCDEF\n";
  DriverStr(expected, "FarLongLiteralAlignmentNop");
  EXPECT_EQ(__ GetLabelLocation(literal->GetLabel()), (5 + kAdduCount) * 4);
}

// MSA instructions.

TEST_F(AssemblerMIPS64Test, AndV) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::AndV, "and.v ${reg1}, ${reg2}, ${reg3}"), "and.v");
}

TEST_F(AssemblerMIPS64Test, OrV) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::OrV, "or.v ${reg1}, ${reg2}, ${reg3}"), "or.v");
}

TEST_F(AssemblerMIPS64Test, NorV) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::NorV, "nor.v ${reg1}, ${reg2}, ${reg3}"), "nor.v");
}

TEST_F(AssemblerMIPS64Test, XorV) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::XorV, "xor.v ${reg1}, ${reg2}, ${reg3}"), "xor.v");
}

TEST_F(AssemblerMIPS64Test, AddvB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::AddvB, "addv.b ${reg1}, ${reg2}, ${reg3}"),
            "addv.b");
}

TEST_F(AssemblerMIPS64Test, AddvH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::AddvH, "addv.h ${reg1}, ${reg2}, ${reg3}"),
            "addv.h");
}

TEST_F(AssemblerMIPS64Test, AddvW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::AddvW, "addv.w ${reg1}, ${reg2}, ${reg3}"),
            "addv.w");
}

TEST_F(AssemblerMIPS64Test, AddvD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::AddvD, "addv.d ${reg1}, ${reg2}, ${reg3}"),
            "addv.d");
}

TEST_F(AssemblerMIPS64Test, SubvB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SubvB, "subv.b ${reg1}, ${reg2}, ${reg3}"),
            "subv.b");
}

TEST_F(AssemblerMIPS64Test, SubvH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SubvH, "subv.h ${reg1}, ${reg2}, ${reg3}"),
            "subv.h");
}

TEST_F(AssemblerMIPS64Test, SubvW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SubvW, "subv.w ${reg1}, ${reg2}, ${reg3}"),
            "subv.w");
}

TEST_F(AssemblerMIPS64Test, SubvD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SubvD, "subv.d ${reg1}, ${reg2}, ${reg3}"),
            "subv.d");
}

TEST_F(AssemblerMIPS64Test, Asub_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_sB, "asub_s.b ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.b");
}

TEST_F(AssemblerMIPS64Test, Asub_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_sH, "asub_s.h ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.h");
}

TEST_F(AssemblerMIPS64Test, Asub_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_sW, "asub_s.w ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.w");
}

TEST_F(AssemblerMIPS64Test, Asub_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_sD, "asub_s.d ${reg1}, ${reg2}, ${reg3}"),
            "asub_s.d");
}

TEST_F(AssemblerMIPS64Test, Asub_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_uB, "asub_u.b ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.b");
}

TEST_F(AssemblerMIPS64Test, Asub_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_uH, "asub_u.h ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.h");
}

TEST_F(AssemblerMIPS64Test, Asub_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_uW, "asub_u.w ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.w");
}

TEST_F(AssemblerMIPS64Test, Asub_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Asub_uD, "asub_u.d ${reg1}, ${reg2}, ${reg3}"),
            "asub_u.d");
}

TEST_F(AssemblerMIPS64Test, MulvB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MulvB, "mulv.b ${reg1}, ${reg2}, ${reg3}"),
            "mulv.b");
}

TEST_F(AssemblerMIPS64Test, MulvH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MulvH, "mulv.h ${reg1}, ${reg2}, ${reg3}"),
            "mulv.h");
}

TEST_F(AssemblerMIPS64Test, MulvW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MulvW, "mulv.w ${reg1}, ${reg2}, ${reg3}"),
            "mulv.w");
}

TEST_F(AssemblerMIPS64Test, MulvD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MulvD, "mulv.d ${reg1}, ${reg2}, ${reg3}"),
            "mulv.d");
}

TEST_F(AssemblerMIPS64Test, Div_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_sB, "div_s.b ${reg1}, ${reg2}, ${reg3}"),
            "div_s.b");
}

TEST_F(AssemblerMIPS64Test, Div_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_sH, "div_s.h ${reg1}, ${reg2}, ${reg3}"),
            "div_s.h");
}

TEST_F(AssemblerMIPS64Test, Div_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_sW, "div_s.w ${reg1}, ${reg2}, ${reg3}"),
            "div_s.w");
}

TEST_F(AssemblerMIPS64Test, Div_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_sD, "div_s.d ${reg1}, ${reg2}, ${reg3}"),
            "div_s.d");
}

TEST_F(AssemblerMIPS64Test, Div_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_uB, "div_u.b ${reg1}, ${reg2}, ${reg3}"),
            "div_u.b");
}

TEST_F(AssemblerMIPS64Test, Div_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_uH, "div_u.h ${reg1}, ${reg2}, ${reg3}"),
            "div_u.h");
}

TEST_F(AssemblerMIPS64Test, Div_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_uW, "div_u.w ${reg1}, ${reg2}, ${reg3}"),
            "div_u.w");
}

TEST_F(AssemblerMIPS64Test, Div_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Div_uD, "div_u.d ${reg1}, ${reg2}, ${reg3}"),
            "div_u.d");
}

TEST_F(AssemblerMIPS64Test, Mod_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_sB, "mod_s.b ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.b");
}

TEST_F(AssemblerMIPS64Test, Mod_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_sH, "mod_s.h ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.h");
}

TEST_F(AssemblerMIPS64Test, Mod_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_sW, "mod_s.w ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.w");
}

TEST_F(AssemblerMIPS64Test, Mod_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_sD, "mod_s.d ${reg1}, ${reg2}, ${reg3}"),
            "mod_s.d");
}

TEST_F(AssemblerMIPS64Test, Mod_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_uB, "mod_u.b ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.b");
}

TEST_F(AssemblerMIPS64Test, Mod_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_uH, "mod_u.h ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.h");
}

TEST_F(AssemblerMIPS64Test, Mod_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_uW, "mod_u.w ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.w");
}

TEST_F(AssemblerMIPS64Test, Mod_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Mod_uD, "mod_u.d ${reg1}, ${reg2}, ${reg3}"),
            "mod_u.d");
}

TEST_F(AssemblerMIPS64Test, Add_aB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Add_aB, "add_a.b ${reg1}, ${reg2}, ${reg3}"),
            "add_a.b");
}

TEST_F(AssemblerMIPS64Test, Add_aH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Add_aH, "add_a.h ${reg1}, ${reg2}, ${reg3}"),
            "add_a.h");
}

TEST_F(AssemblerMIPS64Test, Add_aW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Add_aW, "add_a.w ${reg1}, ${reg2}, ${reg3}"),
            "add_a.w");
}

TEST_F(AssemblerMIPS64Test, Add_aD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Add_aD, "add_a.d ${reg1}, ${reg2}, ${reg3}"),
            "add_a.d");
}

TEST_F(AssemblerMIPS64Test, Ave_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_sB, "ave_s.b ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.b");
}

TEST_F(AssemblerMIPS64Test, Ave_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_sH, "ave_s.h ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.h");
}

TEST_F(AssemblerMIPS64Test, Ave_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_sW, "ave_s.w ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.w");
}

TEST_F(AssemblerMIPS64Test, Ave_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_sD, "ave_s.d ${reg1}, ${reg2}, ${reg3}"),
            "ave_s.d");
}

TEST_F(AssemblerMIPS64Test, Ave_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_uB, "ave_u.b ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.b");
}

TEST_F(AssemblerMIPS64Test, Ave_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_uH, "ave_u.h ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.h");
}

TEST_F(AssemblerMIPS64Test, Ave_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_uW, "ave_u.w ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.w");
}

TEST_F(AssemblerMIPS64Test, Ave_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Ave_uD, "ave_u.d ${reg1}, ${reg2}, ${reg3}"),
            "ave_u.d");
}

TEST_F(AssemblerMIPS64Test, Aver_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_sB, "aver_s.b ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.b");
}

TEST_F(AssemblerMIPS64Test, Aver_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_sH, "aver_s.h ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.h");
}

TEST_F(AssemblerMIPS64Test, Aver_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_sW, "aver_s.w ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.w");
}

TEST_F(AssemblerMIPS64Test, Aver_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_sD, "aver_s.d ${reg1}, ${reg2}, ${reg3}"),
            "aver_s.d");
}

TEST_F(AssemblerMIPS64Test, Aver_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_uB, "aver_u.b ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.b");
}

TEST_F(AssemblerMIPS64Test, Aver_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_uH, "aver_u.h ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.h");
}

TEST_F(AssemblerMIPS64Test, Aver_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_uW, "aver_u.w ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.w");
}

TEST_F(AssemblerMIPS64Test, Aver_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Aver_uD, "aver_u.d ${reg1}, ${reg2}, ${reg3}"),
            "aver_u.d");
}

TEST_F(AssemblerMIPS64Test, Max_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_sB, "max_s.b ${reg1}, ${reg2}, ${reg3}"),
            "max_s.b");
}

TEST_F(AssemblerMIPS64Test, Max_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_sH, "max_s.h ${reg1}, ${reg2}, ${reg3}"),
            "max_s.h");
}

TEST_F(AssemblerMIPS64Test, Max_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_sW, "max_s.w ${reg1}, ${reg2}, ${reg3}"),
            "max_s.w");
}

TEST_F(AssemblerMIPS64Test, Max_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_sD, "max_s.d ${reg1}, ${reg2}, ${reg3}"),
            "max_s.d");
}

TEST_F(AssemblerMIPS64Test, Max_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_uB, "max_u.b ${reg1}, ${reg2}, ${reg3}"),
            "max_u.b");
}

TEST_F(AssemblerMIPS64Test, Max_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_uH, "max_u.h ${reg1}, ${reg2}, ${reg3}"),
            "max_u.h");
}

TEST_F(AssemblerMIPS64Test, Max_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_uW, "max_u.w ${reg1}, ${reg2}, ${reg3}"),
            "max_u.w");
}

TEST_F(AssemblerMIPS64Test, Max_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Max_uD, "max_u.d ${reg1}, ${reg2}, ${reg3}"),
            "max_u.d");
}

TEST_F(AssemblerMIPS64Test, Min_sB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_sB, "min_s.b ${reg1}, ${reg2}, ${reg3}"),
            "min_s.b");
}

TEST_F(AssemblerMIPS64Test, Min_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_sH, "min_s.h ${reg1}, ${reg2}, ${reg3}"),
            "min_s.h");
}

TEST_F(AssemblerMIPS64Test, Min_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_sW, "min_s.w ${reg1}, ${reg2}, ${reg3}"),
            "min_s.w");
}

TEST_F(AssemblerMIPS64Test, Min_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_sD, "min_s.d ${reg1}, ${reg2}, ${reg3}"),
            "min_s.d");
}

TEST_F(AssemblerMIPS64Test, Min_uB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_uB, "min_u.b ${reg1}, ${reg2}, ${reg3}"),
            "min_u.b");
}

TEST_F(AssemblerMIPS64Test, Min_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_uH, "min_u.h ${reg1}, ${reg2}, ${reg3}"),
            "min_u.h");
}

TEST_F(AssemblerMIPS64Test, Min_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_uW, "min_u.w ${reg1}, ${reg2}, ${reg3}"),
            "min_u.w");
}

TEST_F(AssemblerMIPS64Test, Min_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Min_uD, "min_u.d ${reg1}, ${reg2}, ${reg3}"),
            "min_u.d");
}

TEST_F(AssemblerMIPS64Test, FaddW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FaddW, "fadd.w ${reg1}, ${reg2}, ${reg3}"),
            "fadd.w");
}

TEST_F(AssemblerMIPS64Test, FaddD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FaddD, "fadd.d ${reg1}, ${reg2}, ${reg3}"),
            "fadd.d");
}

TEST_F(AssemblerMIPS64Test, FsubW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FsubW, "fsub.w ${reg1}, ${reg2}, ${reg3}"),
            "fsub.w");
}

TEST_F(AssemblerMIPS64Test, FsubD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FsubD, "fsub.d ${reg1}, ${reg2}, ${reg3}"),
            "fsub.d");
}

TEST_F(AssemblerMIPS64Test, FmulW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmulW, "fmul.w ${reg1}, ${reg2}, ${reg3}"),
            "fmul.w");
}

TEST_F(AssemblerMIPS64Test, FmulD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmulD, "fmul.d ${reg1}, ${reg2}, ${reg3}"),
            "fmul.d");
}

TEST_F(AssemblerMIPS64Test, FdivW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FdivW, "fdiv.w ${reg1}, ${reg2}, ${reg3}"),
            "fdiv.w");
}

TEST_F(AssemblerMIPS64Test, FdivD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FdivD, "fdiv.d ${reg1}, ${reg2}, ${reg3}"),
            "fdiv.d");
}

TEST_F(AssemblerMIPS64Test, FmaxW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmaxW, "fmax.w ${reg1}, ${reg2}, ${reg3}"),
            "fmax.w");
}

TEST_F(AssemblerMIPS64Test, FmaxD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmaxD, "fmax.d ${reg1}, ${reg2}, ${reg3}"),
            "fmax.d");
}

TEST_F(AssemblerMIPS64Test, FminW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FminW, "fmin.w ${reg1}, ${reg2}, ${reg3}"),
            "fmin.w");
}

TEST_F(AssemblerMIPS64Test, FminD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FminD, "fmin.d ${reg1}, ${reg2}, ${reg3}"),
            "fmin.d");
}

TEST_F(AssemblerMIPS64Test, Ffint_sW) {
  DriverStr(RepeatVV(&mips64::Mips64Assembler::Ffint_sW, "ffint_s.w ${reg1}, ${reg2}"),
            "ffint_s.w");
}

TEST_F(AssemblerMIPS64Test, Ffint_sD) {
  DriverStr(RepeatVV(&mips64::Mips64Assembler::Ffint_sD, "ffint_s.d ${reg1}, ${reg2}"),
            "ffint_s.d");
}

TEST_F(AssemblerMIPS64Test, Ftint_sW) {
  DriverStr(RepeatVV(&mips64::Mips64Assembler::Ftint_sW, "ftint_s.w ${reg1}, ${reg2}"),
            "ftint_s.w");
}

TEST_F(AssemblerMIPS64Test, Ftint_sD) {
  DriverStr(RepeatVV(&mips64::Mips64Assembler::Ftint_sD, "ftint_s.d ${reg1}, ${reg2}"),
            "ftint_s.d");
}

TEST_F(AssemblerMIPS64Test, SllB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SllB, "sll.b ${reg1}, ${reg2}, ${reg3}"), "sll.b");
}

TEST_F(AssemblerMIPS64Test, SllH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SllH, "sll.h ${reg1}, ${reg2}, ${reg3}"), "sll.h");
}

TEST_F(AssemblerMIPS64Test, SllW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SllW, "sll.w ${reg1}, ${reg2}, ${reg3}"), "sll.w");
}

TEST_F(AssemblerMIPS64Test, SllD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SllD, "sll.d ${reg1}, ${reg2}, ${reg3}"), "sll.d");
}

TEST_F(AssemblerMIPS64Test, SraB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SraB, "sra.b ${reg1}, ${reg2}, ${reg3}"), "sra.b");
}

TEST_F(AssemblerMIPS64Test, SraH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SraH, "sra.h ${reg1}, ${reg2}, ${reg3}"), "sra.h");
}

TEST_F(AssemblerMIPS64Test, SraW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SraW, "sra.w ${reg1}, ${reg2}, ${reg3}"), "sra.w");
}

TEST_F(AssemblerMIPS64Test, SraD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SraD, "sra.d ${reg1}, ${reg2}, ${reg3}"), "sra.d");
}

TEST_F(AssemblerMIPS64Test, SrlB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SrlB, "srl.b ${reg1}, ${reg2}, ${reg3}"), "srl.b");
}

TEST_F(AssemblerMIPS64Test, SrlH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SrlH, "srl.h ${reg1}, ${reg2}, ${reg3}"), "srl.h");
}

TEST_F(AssemblerMIPS64Test, SrlW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SrlW, "srl.w ${reg1}, ${reg2}, ${reg3}"), "srl.w");
}

TEST_F(AssemblerMIPS64Test, SrlD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::SrlD, "srl.d ${reg1}, ${reg2}, ${reg3}"), "srl.d");
}

TEST_F(AssemblerMIPS64Test, SlliB) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SlliB, 3, "slli.b ${reg1}, ${reg2}, {imm}"),
            "slli.b");
}

TEST_F(AssemblerMIPS64Test, SlliH) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SlliH, 4, "slli.h ${reg1}, ${reg2}, {imm}"),
            "slli.h");
}

TEST_F(AssemblerMIPS64Test, SlliW) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SlliW, 5, "slli.w ${reg1}, ${reg2}, {imm}"),
            "slli.w");
}

TEST_F(AssemblerMIPS64Test, SlliD) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SlliD, 6, "slli.d ${reg1}, ${reg2}, {imm}"),
            "slli.d");
}

TEST_F(AssemblerMIPS64Test, MoveV) {
  DriverStr(RepeatVV(&mips64::Mips64Assembler::MoveV, "move.v ${reg1}, ${reg2}"), "move.v");
}

TEST_F(AssemblerMIPS64Test, SplatiB) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SplatiB, 4, "splati.b ${reg1}, ${reg2}[{imm}]"),
            "splati.b");
}

TEST_F(AssemblerMIPS64Test, SplatiH) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SplatiH, 3, "splati.h ${reg1}, ${reg2}[{imm}]"),
            "splati.h");
}

TEST_F(AssemblerMIPS64Test, SplatiW) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SplatiW, 2, "splati.w ${reg1}, ${reg2}[{imm}]"),
            "splati.w");
}

TEST_F(AssemblerMIPS64Test, SplatiD) {
  DriverStr(RepeatVVIb(&mips64::Mips64Assembler::SplatiD, 1, "splati.d ${reg1}, ${reg2}[{imm}]"),
            "splati.d");
}

TEST_F(AssemblerMIPS64Test, Copy_sB) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_sB, 4, "copy_s.b ${reg1}, ${reg2}[{imm}]"),
            "copy_s.b");
}

TEST_F(AssemblerMIPS64Test, Copy_sH) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_sH, 3, "copy_s.h ${reg1}, ${reg2}[{imm}]"),
            "copy_s.h");
}

TEST_F(AssemblerMIPS64Test, Copy_sW) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_sW, 2, "copy_s.w ${reg1}, ${reg2}[{imm}]"),
            "copy_s.w");
}

TEST_F(AssemblerMIPS64Test, Copy_sD) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_sD, 1, "copy_s.d ${reg1}, ${reg2}[{imm}]"),
            "copy_s.d");
}

TEST_F(AssemblerMIPS64Test, Copy_uB) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_uB, 4, "copy_u.b ${reg1}, ${reg2}[{imm}]"),
            "copy_u.b");
}

TEST_F(AssemblerMIPS64Test, Copy_uH) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_uH, 3, "copy_u.h ${reg1}, ${reg2}[{imm}]"),
            "copy_u.h");
}

TEST_F(AssemblerMIPS64Test, Copy_uW) {
  DriverStr(RepeatRVIb(&mips64::Mips64Assembler::Copy_uW, 2, "copy_u.w ${reg1}, ${reg2}[{imm}]"),
            "copy_u.w");
}

TEST_F(AssemblerMIPS64Test, InsertB) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::InsertB, 4, "insert.b ${reg1}[{imm}], ${reg2}"),
            "insert.b");
}

TEST_F(AssemblerMIPS64Test, InsertH) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::InsertH, 3, "insert.h ${reg1}[{imm}], ${reg2}"),
            "insert.h");
}

TEST_F(AssemblerMIPS64Test, InsertW) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::InsertW, 2, "insert.w ${reg1}[{imm}], ${reg2}"),
            "insert.w");
}

TEST_F(AssemblerMIPS64Test, InsertD) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::InsertD, 1, "insert.d ${reg1}[{imm}], ${reg2}"),
            "insert.d");
}

TEST_F(AssemblerMIPS64Test, FillB) {
  DriverStr(RepeatVR(&mips64::Mips64Assembler::FillB, "fill.b ${reg1}, ${reg2}"), "fill.b");
}

TEST_F(AssemblerMIPS64Test, FillH) {
  DriverStr(RepeatVR(&mips64::Mips64Assembler::FillH, "fill.h ${reg1}, ${reg2}"), "fill.h");
}

TEST_F(AssemblerMIPS64Test, FillW) {
  DriverStr(RepeatVR(&mips64::Mips64Assembler::FillW, "fill.w ${reg1}, ${reg2}"), "fill.w");
}

TEST_F(AssemblerMIPS64Test, FillD) {
  DriverStr(RepeatVR(&mips64::Mips64Assembler::FillD, "fill.d ${reg1}, ${reg2}"), "fill.d");
}

TEST_F(AssemblerMIPS64Test, LdiB) {
  DriverStr(RepeatVIb(&mips64::Mips64Assembler::LdiB, -8, "ldi.b ${reg}, {imm}"), "ldi.b");
}

TEST_F(AssemblerMIPS64Test, LdiH) {
  DriverStr(RepeatVIb(&mips64::Mips64Assembler::LdiH, -10, "ldi.h ${reg}, {imm}"), "ldi.h");
}

TEST_F(AssemblerMIPS64Test, LdiW) {
  DriverStr(RepeatVIb(&mips64::Mips64Assembler::LdiW, -10, "ldi.w ${reg}, {imm}"), "ldi.w");
}

TEST_F(AssemblerMIPS64Test, LdiD) {
  DriverStr(RepeatVIb(&mips64::Mips64Assembler::LdiD, -10, "ldi.d ${reg}, {imm}"), "ldi.d");
}

TEST_F(AssemblerMIPS64Test, LdB) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::LdB, -10, "ld.b ${reg1}, {imm}(${reg2})"), "ld.b");
}

TEST_F(AssemblerMIPS64Test, LdH) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::LdH, -10, "ld.h ${reg1}, {imm}(${reg2})", 0, 2),
            "ld.h");
}

TEST_F(AssemblerMIPS64Test, LdW) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::LdW, -10, "ld.w ${reg1}, {imm}(${reg2})", 0, 4),
            "ld.w");
}

TEST_F(AssemblerMIPS64Test, LdD) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::LdD, -10, "ld.d ${reg1}, {imm}(${reg2})", 0, 8),
            "ld.d");
}

TEST_F(AssemblerMIPS64Test, StB) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::StB, -10, "st.b ${reg1}, {imm}(${reg2})"), "st.b");
}

TEST_F(AssemblerMIPS64Test, StH) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::StH, -10, "st.h ${reg1}, {imm}(${reg2})", 0, 2),
            "st.h");
}

TEST_F(AssemblerMIPS64Test, StW) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::StW, -10, "st.w ${reg1}, {imm}(${reg2})", 0, 4),
            "st.w");
}

TEST_F(AssemblerMIPS64Test, StD) {
  DriverStr(RepeatVRIb(&mips64::Mips64Assembler::StD, -10, "st.d ${reg1}, {imm}(${reg2})", 0, 8),
            "st.d");
}

TEST_F(AssemblerMIPS64Test, IlvlB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvlB, "ilvl.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvl.b");
}

TEST_F(AssemblerMIPS64Test, IlvlH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvlH, "ilvl.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvl.h");
}

TEST_F(AssemblerMIPS64Test, IlvlW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvlW, "ilvl.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvl.w");
}

TEST_F(AssemblerMIPS64Test, IlvlD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvlD, "ilvl.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvl.d");
}

TEST_F(AssemblerMIPS64Test, IlvrB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvrB, "ilvr.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvr.b");
}

TEST_F(AssemblerMIPS64Test, IlvrH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvrH, "ilvr.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvr.h");
}

TEST_F(AssemblerMIPS64Test, IlvrW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvrW, "ilvr.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvr.w");
}

TEST_F(AssemblerMIPS64Test, IlvrD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvrD, "ilvr.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvr.d");
}

TEST_F(AssemblerMIPS64Test, IlvevB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvevB, "ilvev.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.b");
}

TEST_F(AssemblerMIPS64Test, IlvevH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvevH, "ilvev.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.h");
}

TEST_F(AssemblerMIPS64Test, IlvevW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvevW, "ilvev.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.w");
}

TEST_F(AssemblerMIPS64Test, IlvevD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvevD, "ilvev.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvev.d");
}

TEST_F(AssemblerMIPS64Test, IlvodB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvodB, "ilvod.b ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.b");
}

TEST_F(AssemblerMIPS64Test, IlvodH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvodH, "ilvod.h ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.h");
}

TEST_F(AssemblerMIPS64Test, IlvodW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvodW, "ilvod.w ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.w");
}

TEST_F(AssemblerMIPS64Test, IlvodD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::IlvodD, "ilvod.d ${reg1}, ${reg2}, ${reg3}"),
            "ilvod.d");
}

TEST_F(AssemblerMIPS64Test, MaddvB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MaddvB, "maddv.b ${reg1}, ${reg2}, ${reg3}"),
            "maddv.b");
}

TEST_F(AssemblerMIPS64Test, MaddvH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MaddvH, "maddv.h ${reg1}, ${reg2}, ${reg3}"),
            "maddv.h");
}

TEST_F(AssemblerMIPS64Test, MaddvW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MaddvW, "maddv.w ${reg1}, ${reg2}, ${reg3}"),
            "maddv.w");
}

TEST_F(AssemblerMIPS64Test, MaddvD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MaddvD, "maddv.d ${reg1}, ${reg2}, ${reg3}"),
            "maddv.d");
}

TEST_F(AssemblerMIPS64Test, Hadd_sH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_sH, "hadd_s.h ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.h");
}

TEST_F(AssemblerMIPS64Test, Hadd_sW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_sW, "hadd_s.w ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.w");
}

TEST_F(AssemblerMIPS64Test, Hadd_sD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_sD, "hadd_s.d ${reg1}, ${reg2}, ${reg3}"),
            "hadd_s.d");
}

TEST_F(AssemblerMIPS64Test, Hadd_uH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_uH, "hadd_u.h ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.h");
}

TEST_F(AssemblerMIPS64Test, Hadd_uW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_uW, "hadd_u.w ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.w");
}

TEST_F(AssemblerMIPS64Test, Hadd_uD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::Hadd_uD, "hadd_u.d ${reg1}, ${reg2}, ${reg3}"),
            "hadd_u.d");
}

TEST_F(AssemblerMIPS64Test, MsubvB) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MsubvB, "msubv.b ${reg1}, ${reg2}, ${reg3}"),
            "msubv.b");
}

TEST_F(AssemblerMIPS64Test, MsubvH) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MsubvH, "msubv.h ${reg1}, ${reg2}, ${reg3}"),
            "msubv.h");
}

TEST_F(AssemblerMIPS64Test, MsubvW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MsubvW, "msubv.w ${reg1}, ${reg2}, ${reg3}"),
            "msubv.w");
}

TEST_F(AssemblerMIPS64Test, MsubvD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::MsubvD, "msubv.d ${reg1}, ${reg2}, ${reg3}"),
            "msubv.d");
}

TEST_F(AssemblerMIPS64Test, FmaddW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmaddW, "fmadd.w ${reg1}, ${reg2}, ${reg3}"),
            "fmadd.w");
}

TEST_F(AssemblerMIPS64Test, FmaddD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmaddD, "fmadd.d ${reg1}, ${reg2}, ${reg3}"),
            "fmadd.d");
}

TEST_F(AssemblerMIPS64Test, FmsubW) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmsubW, "fmsub.w ${reg1}, ${reg2}, ${reg3}"),
            "fmsub.w");
}

TEST_F(AssemblerMIPS64Test, FmsubD) {
  DriverStr(RepeatVVV(&mips64::Mips64Assembler::FmsubD, "fmsub.d ${reg1}, ${reg2}, ${reg3}"),
            "fmsub.d");
}

#undef __

}  // namespace art
