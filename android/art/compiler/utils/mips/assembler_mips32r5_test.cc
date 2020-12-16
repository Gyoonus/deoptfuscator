/*
 * Copyright (C) 2017 The Android Open Source Project
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

class AssemblerMIPS32r5Test : public AssemblerTest<mips::MipsAssembler,
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

  AssemblerMIPS32r5Test() :
    instruction_set_features_(MipsInstructionSetFeatures::FromVariant("mips32r5", nullptr)) {
  }

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "mips";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    return " --no-warn -32 -march=mips32r5 -mmsa";
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
    return " -D -bbinary -mmips:isa32r5";
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

 private:
  std::vector<mips::Register*> registers_;
  std::map<mips::Register, std::string, MIPSCpuRegisterCompare> secondary_register_names_;

  std::vector<mips::FRegister*> fp_registers_;
  std::vector<mips::VectorRegister*> vec_registers_;
  std::unique_ptr<const MipsInstructionSetFeatures> instruction_set_features_;
};

TEST_F(AssemblerMIPS32r5Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

TEST_F(AssemblerMIPS32r5Test, LoadQFromOffset) {
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
  __ LoadQFromOffset(mips::F0, mips::A0, -36856);
  __ LoadQFromOffset(mips::F0, mips::A0, 36856);
  __ LoadQFromOffset(mips::F0, mips::A0, -69608);
  __ LoadQFromOffset(mips::F0, mips::A0, 69608);
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
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 32760\n"
      "ld.d $w0, 16($at)\n"
      "lui $at, 4660\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, 24576\n"
      "ld.d $w0, -2440($at) # 0xF678\n"
      "lui $at, 4661\n"
      "addu $at, $at, $a0\n"
      "ld.d $w0, 120($at)\n"
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
      "addiu $at, $a0, -32760\n"
      "addiu $at, $at, -4096\n"
      "ld.d $w0, 0($at)\n"
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 4096\n"
      "ld.d $w0, 0($at)\n"
      "addiu $at, $a0, -32760\n"
      "addiu $at, $at, -32760\n"
      "ld.d $w0, -4088($at)\n"
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 32760\n"
      "ld.d $w0, 4088($at)\n"
      "lui $at, 0xABCE\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, -8192 # 0xE000\n"
      "ld.d $w0, 0xF00($at)\n"
      "lui $at, 0x8000\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, -21504 # 0xAC00\n"
      "ld.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "LoadQFromOffset");
}

TEST_F(AssemblerMIPS32r5Test, StoreQToOffset) {
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
  __ StoreQToOffset(mips::F0, mips::A0, -36856);
  __ StoreQToOffset(mips::F0, mips::A0, 36856);
  __ StoreQToOffset(mips::F0, mips::A0, -69608);
  __ StoreQToOffset(mips::F0, mips::A0, 69608);
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
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 32760\n"
      "st.d $w0, 16($at)\n"
      "lui $at, 4660\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, 24576\n"
      "st.d $w0, -2440($at) # 0xF678\n"
      "lui $at, 4661\n"
      "addu $at, $at, $a0\n"
      "st.d $w0, 120($at)\n"
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
      "addiu $at, $a0, -32760\n"
      "addiu $at, $at, -4096\n"
      "st.d $w0, 0($at)\n"
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 4096\n"
      "st.d $w0, 0($at)\n"
      "addiu $at, $a0, -32760\n"
      "addiu $at, $at, -32760\n"
      "st.d $w0, -4088($at)\n"
      "addiu $at, $a0, 32760\n"
      "addiu $at, $at, 32760\n"
      "st.d $w0, 4088($at)\n"
      "lui $at, 0xABCE\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, -8192 # 0xE000\n"
      "st.d $w0, 0xF00($at)\n"
      "lui $at, 0x8000\n"
      "addu $at, $at, $a0\n"
      "addiu $at, $at, -21504 # 0xAC00\n"
      "st.b $w0, -51($at) # 0xFFCD\n";
  DriverStr(expected, "StoreQToOffset");
}

#undef __
}  // namespace art
