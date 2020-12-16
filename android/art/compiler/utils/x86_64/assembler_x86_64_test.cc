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

#include "assembler_x86_64.h"

#include <inttypes.h>
#include <map>
#include <random>

#include "base/bit_utils.h"
#include "base/stl_util.h"
#include "jni_macro_assembler_x86_64.h"
#include "utils/assembler_test.h"
#include "utils/jni_macro_assembler_test.h"

namespace art {

TEST(AssemblerX86_64, CreateBuffer) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  AssemblerBuffer buffer(&allocator);
  AssemblerBuffer::EnsureCapacity ensured(&buffer);
  buffer.Emit<uint8_t>(0x42);
  ASSERT_EQ(static_cast<size_t>(1), buffer.Size());
  buffer.Emit<int32_t>(42);
  ASSERT_EQ(static_cast<size_t>(5), buffer.Size());
}

#ifdef ART_TARGET_ANDROID
static constexpr size_t kRandomIterations = 1000;  // Devices might be puny, don't stress them...
#else
static constexpr size_t kRandomIterations = 100000;  // Hosts are pretty powerful.
#endif

TEST(AssemblerX86_64, SignExtension) {
  // 32bit.
  for (int32_t i = 0; i < 128; i++) {
    EXPECT_TRUE(IsInt<8>(i)) << i;
  }
  for (int32_t i = 128; i < 255; i++) {
    EXPECT_FALSE(IsInt<8>(i)) << i;
  }
  // Do some higher ones randomly.
  std::random_device rd;
  std::default_random_engine e1(rd());
  std::uniform_int_distribution<int32_t> uniform_dist(256, INT32_MAX);
  for (size_t i = 0; i < kRandomIterations; i++) {
    int32_t value = uniform_dist(e1);
    EXPECT_FALSE(IsInt<8>(value)) << value;
  }

  // Negative ones.
  for (int32_t i = -1; i >= -128; i--) {
    EXPECT_TRUE(IsInt<8>(i)) << i;
  }

  for (int32_t i = -129; i > -256; i--) {
    EXPECT_FALSE(IsInt<8>(i)) << i;
  }

  // Do some lower ones randomly.
  std::uniform_int_distribution<int32_t> uniform_dist2(INT32_MIN, -256);
  for (size_t i = 0; i < 100; i++) {
    int32_t value = uniform_dist2(e1);
    EXPECT_FALSE(IsInt<8>(value)) << value;
  }

  // 64bit.
  for (int64_t i = 0; i < 128; i++) {
    EXPECT_TRUE(IsInt<8>(i)) << i;
  }
  for (int32_t i = 128; i < 255; i++) {
    EXPECT_FALSE(IsInt<8>(i)) << i;
  }
  // Do some higher ones randomly.
  std::uniform_int_distribution<int64_t> uniform_dist3(256, INT64_MAX);
  for (size_t i = 0; i < 100; i++) {
    int64_t value = uniform_dist3(e1);
    EXPECT_FALSE(IsInt<8>(value)) << value;
  }

  // Negative ones.
  for (int64_t i = -1; i >= -128; i--) {
    EXPECT_TRUE(IsInt<8>(i)) << i;
  }

  for (int64_t i = -129; i > -256; i--) {
    EXPECT_FALSE(IsInt<8>(i)) << i;
  }

  // Do some lower ones randomly.
  std::uniform_int_distribution<int64_t> uniform_dist4(INT64_MIN, -256);
  for (size_t i = 0; i < kRandomIterations; i++) {
    int64_t value = uniform_dist4(e1);
    EXPECT_FALSE(IsInt<8>(value)) << value;
  }

  int64_t value = INT64_C(0x1200000010);
  x86_64::Immediate imm(value);
  EXPECT_FALSE(imm.is_int8());
  EXPECT_FALSE(imm.is_int16());
  EXPECT_FALSE(imm.is_int32());
  value = INT64_C(0x8000000000000001);
  x86_64::Immediate imm2(value);
  EXPECT_FALSE(imm2.is_int8());
  EXPECT_FALSE(imm2.is_int16());
  EXPECT_FALSE(imm2.is_int32());
}

struct X86_64CpuRegisterCompare {
    bool operator()(const x86_64::CpuRegister& a, const x86_64::CpuRegister& b) const {
        return a.AsRegister() < b.AsRegister();
    }
};

//
// Test fixture.
//

class AssemblerX86_64Test : public AssemblerTest<x86_64::X86_64Assembler,
                                                 x86_64::Address,
                                                 x86_64::CpuRegister,
                                                 x86_64::XmmRegister,
                                                 x86_64::Immediate> {
 public:
  typedef AssemblerTest<x86_64::X86_64Assembler,
                        x86_64::Address,
                        x86_64::CpuRegister,
                        x86_64::XmmRegister,
                        x86_64::Immediate> Base;

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "x86_64";
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mi386:x86-64 -Mx86-64,addr64,data32 --no-show-raw-insn";
  }

  void SetUpHelpers() OVERRIDE {
    if (addresses_singleton_.size() == 0) {
      // One addressing mode to test the repeat drivers.
      addresses_singleton_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RAX),
                          x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_1, -1));
    }

    if (addresses_.size() == 0) {
      // Several addressing modes.
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RDI),
                          x86_64::CpuRegister(x86_64::RAX), x86_64::TIMES_1, 15));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RDI),
                          x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_2, 16));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RDI),
                          x86_64::CpuRegister(x86_64::RCX), x86_64::TIMES_4, 17));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RDI),
                          x86_64::CpuRegister(x86_64::RDX), x86_64::TIMES_8, 18));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RAX), -1));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RBX), 0));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RSI), 1));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RDI), 987654321));
      // Several addressing modes with the special ESP.
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RSP),
                          x86_64::CpuRegister(x86_64::RAX), x86_64::TIMES_1, 15));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RSP),
                          x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_2, 16));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RSP),
                          x86_64::CpuRegister(x86_64::RCX), x86_64::TIMES_4, 17));
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::RSP),
                          x86_64::CpuRegister(x86_64::RDX), x86_64::TIMES_8, 18));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RSP), -1));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RSP), 0));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RSP), 1));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::RSP), 987654321));
      // Several addressing modes with the higher registers.
      addresses_.push_back(
          x86_64::Address(x86_64::CpuRegister(x86_64::R8),
                          x86_64::CpuRegister(x86_64::R15), x86_64::TIMES_2, -1));
      addresses_.push_back(x86_64::Address(x86_64::CpuRegister(x86_64::R15), 123456789));
    }

    if (registers_.size() == 0) {
      registers_.push_back(new x86_64::CpuRegister(x86_64::RAX));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RBX));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RCX));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RDX));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RBP));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RSP));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RSI));
      registers_.push_back(new x86_64::CpuRegister(x86_64::RDI));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R8));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R9));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R10));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R11));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R12));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R13));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R14));
      registers_.push_back(new x86_64::CpuRegister(x86_64::R15));

      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RAX), "eax");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBX), "ebx");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RCX), "ecx");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDX), "edx");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBP), "ebp");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSP), "esp");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSI), "esi");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDI), "edi");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R8), "r8d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R9), "r9d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R10), "r10d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R11), "r11d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R12), "r12d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R13), "r13d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R14), "r14d");
      secondary_register_names_.emplace(x86_64::CpuRegister(x86_64::R15), "r15d");

      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RAX), "ax");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBX), "bx");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RCX), "cx");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDX), "dx");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBP), "bp");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSP), "sp");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSI), "si");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDI), "di");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R8), "r8w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R9), "r9w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R10), "r10w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R11), "r11w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R12), "r12w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R13), "r13w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R14), "r14w");
      tertiary_register_names_.emplace(x86_64::CpuRegister(x86_64::R15), "r15w");

      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RAX), "al");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBX), "bl");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RCX), "cl");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDX), "dl");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RBP), "bpl");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSP), "spl");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RSI), "sil");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::RDI), "dil");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R8), "r8b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R9), "r9b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R10), "r10b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R11), "r11b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R12), "r12b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R13), "r13b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R14), "r14b");
      quaternary_register_names_.emplace(x86_64::CpuRegister(x86_64::R15), "r15b");

      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM0));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM1));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM2));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM3));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM4));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM5));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM6));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM7));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM8));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM9));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM10));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM11));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM12));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM13));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM14));
      fp_registers_.push_back(new x86_64::XmmRegister(x86_64::XMM15));
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
  }

  std::vector<x86_64::Address> GetAddresses() {
    return addresses_;
  }

  std::vector<x86_64::CpuRegister*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<x86_64::XmmRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  x86_64::Immediate CreateImmediate(int64_t imm_value) OVERRIDE {
    return x86_64::Immediate(imm_value);
  }

  std::string GetSecondaryRegisterName(const x86_64::CpuRegister& reg) OVERRIDE {
    CHECK(secondary_register_names_.find(reg) != secondary_register_names_.end());
    return secondary_register_names_[reg];
  }

  std::string GetTertiaryRegisterName(const x86_64::CpuRegister& reg) OVERRIDE {
    CHECK(tertiary_register_names_.find(reg) != tertiary_register_names_.end());
    return tertiary_register_names_[reg];
  }

  std::string GetQuaternaryRegisterName(const x86_64::CpuRegister& reg) OVERRIDE {
    CHECK(quaternary_register_names_.find(reg) != quaternary_register_names_.end());
    return quaternary_register_names_[reg];
  }

  std::vector<x86_64::Address> addresses_singleton_;

 private:
  std::vector<x86_64::Address> addresses_;
  std::vector<x86_64::CpuRegister*> registers_;
  std::map<x86_64::CpuRegister, std::string, X86_64CpuRegisterCompare> secondary_register_names_;
  std::map<x86_64::CpuRegister, std::string, X86_64CpuRegisterCompare> tertiary_register_names_;
  std::map<x86_64::CpuRegister, std::string, X86_64CpuRegisterCompare> quaternary_register_names_;
  std::vector<x86_64::XmmRegister*> fp_registers_;
};

//
// Test some repeat drivers used in the tests.
//

TEST_F(AssemblerX86_64Test, RepeatI4) {
  EXPECT_EQ("$0\n$-1\n$18\n$4660\n$-4660\n$305419896\n$-305419896\n",
            RepeatI(/*f*/ nullptr, /*imm_bytes*/ 4U, "${imm}"));
}

TEST_F(AssemblerX86_64Test, RepeatI8) {
  EXPECT_EQ("$0\n$-1\n$18\n$4660\n$-4660\n$305419896\n$-305419896\n"
            "$20015998343868\n$-20015998343868\n$1311768467463790320\n"
            "$-1311768467463790320\n",
            RepeatI(/*f*/ nullptr, /*imm_bytes*/ 8U, "${imm}"));
}

TEST_F(AssemblerX86_64Test, Repeatr) {
  EXPECT_EQ("%eax\n%ebx\n%ecx\n%edx\n%ebp\n%esp\n%esi\n%edi\n"
            "%r8d\n%r9d\n%r10d\n%r11d\n%r12d\n%r13d\n%r14d\n%r15d\n",
            Repeatr(/*f*/ nullptr, "%{reg}"));
}

TEST_F(AssemblerX86_64Test, RepeatrI) {
  EXPECT_NE(RepeatrI(/*f*/ nullptr, /*imm_bytes*/ 1U, "%{reg} ${imm}").
            find("%eax $0\n%eax $-1\n%eax $18\n%ebx $0\n%ebx $-1\n%ebx $18\n"
                 "%ecx $0\n%ecx $-1\n%ecx $18\n%edx $0\n%edx $-1\n%edx $18\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, Repeatrr) {
  EXPECT_NE(Repeatrr(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%eax %eax\n%eax %ebx\n%eax %ecx\n%eax %edx\n"
                  "%eax %ebp\n%eax %esp\n%eax %esi\n%eax %edi\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, Repeatrb) {
  EXPECT_NE(Repeatrb(/*f*/ nullptr, "%{reg1} %{reg2}").
            find("%eax %al\n%eax %bl\n%eax %cl\n%eax %dl\n%eax %bpl\n"
                 "%eax %spl\n%eax %sil\n%eax %dil\n%eax %r8b\n%eax %r9b\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatrF) {
  EXPECT_NE(RepeatrF(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%eax %xmm0\n%eax %xmm1\n%eax %xmm2\n%eax %xmm3\n"
                  "%eax %xmm4\n%eax %xmm5\n%eax %xmm6\n%eax %xmm7\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatR) {
  EXPECT_EQ("%rax\n%rbx\n%rcx\n%rdx\n%rbp\n%rsp\n%rsi\n%rdi\n"
            "%r8\n%r9\n%r10\n%r11\n%r12\n%r13\n%r14\n%r15\n",
            RepeatR(/*f*/ nullptr, "%{reg}"));
}

TEST_F(AssemblerX86_64Test, RepeatRI) {
  EXPECT_NE(RepeatRI(/*f*/ nullptr, /*imm_bytes*/ 1U, "%{reg} ${imm}")
            .find("%rax $0\n%rax $-1\n%rax $18\n%rbx $0\n%rbx $-1\n%rbx $18\n"
                  "%rcx $0\n%rcx $-1\n%rcx $18\n%rdx $0\n%rdx $-1\n%rdx $18\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatRr) {
  EXPECT_NE(RepeatRr(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%rax %eax\n%rax %ebx\n%rax %ecx\n%rax %edx\n%rax %ebp\n"
                  "%rax %esp\n%rax %esi\n%rax %edi\n%rax %r8d\n%rax %r9d\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatRR) {
  EXPECT_NE(RepeatRR(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%rax %rax\n%rax %rbx\n%rax %rcx\n%rax %rdx\n%rax %rbp\n"
                  "%rax %rsp\n%rax %rsi\n%rax %rdi\n%rax %r8\n%rax %r9\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatRF) {
  EXPECT_NE(RepeatRF(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%rax %xmm0\n%rax %xmm1\n%rax %xmm2\n%rax %xmm3\n%rax %xmm4\n"
                  "%rax %xmm5\n%rax %xmm6\n%rax %xmm7\n%rax %xmm8\n%rax %xmm9\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatFF) {
  EXPECT_NE(RepeatFF(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%xmm0 %xmm0\n%xmm0 %xmm1\n%xmm0 %xmm2\n%xmm0 %xmm3\n%xmm0 %xmm4\n"
                  "%xmm0 %xmm5\n%xmm0 %xmm6\n%xmm0 %xmm7\n%xmm0 %xmm8\n%xmm0 %xmm9\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatFFI) {
  EXPECT_NE(RepeatFFI(/*f*/ nullptr, /*imm_bytes*/ 1U, "%{reg1} %{reg2} ${imm}")
            .find("%xmm0 %xmm0 $0\n%xmm0 %xmm0 $-1\n%xmm0 %xmm0 $18\n"
                  "%xmm0 %xmm1 $0\n%xmm0 %xmm1 $-1\n%xmm0 %xmm1 $18\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatA) {
  EXPECT_EQ("-1(%rax,%rbx,1)\n", RepeatA(/*f*/ nullptr, addresses_singleton_, "{mem}"));
}

TEST_F(AssemblerX86_64Test, RepeatAFull) {
  EXPECT_EQ("15(%rdi,%rax,1)\n16(%rdi,%rbx,2)\n17(%rdi,%rcx,4)\n18(%rdi,%rdx,8)\n"
            "-1(%rax)\n(%rbx)\n1(%rsi)\n987654321(%rdi)\n15(%rsp,%rax,1)\n"
            "16(%rsp,%rbx,2)\n17(%rsp,%rcx,4)\n18(%rsp,%rdx,8)\n-1(%rsp)\n"
            "(%rsp)\n1(%rsp)\n987654321(%rsp)\n-1(%r8,%r15,2)\n123456789(%r15)\n",
            RepeatA(/*f*/ nullptr, "{mem}"));
}

TEST_F(AssemblerX86_64Test, RepeatAI) {
  EXPECT_EQ("-1(%rax,%rbx,1) $0\n-1(%rax,%rbx,1) $-1\n-1(%rax,%rbx,1) $18\n",
            RepeatAI(/*f*/ nullptr, /*imm_bytes*/ 1U, addresses_singleton_, "{mem} ${imm}"));
}

TEST_F(AssemblerX86_64Test, RepeatRA) {
  EXPECT_NE(RepeatRA(/*f*/ nullptr, addresses_singleton_, "%{reg} {mem}")
            .find("%rax -1(%rax,%rbx,1)\n%rbx -1(%rax,%rbx,1)\n%rcx -1(%rax,%rbx,1)\n"
                  "%rdx -1(%rax,%rbx,1)\n%rbp -1(%rax,%rbx,1)\n%rsp -1(%rax,%rbx,1)\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatrA) {
  EXPECT_NE(RepeatrA(/*f*/ nullptr, addresses_singleton_, "%{reg} {mem}")
            .find("%eax -1(%rax,%rbx,1)\n%ebx -1(%rax,%rbx,1)\n%ecx -1(%rax,%rbx,1)\n"
                  "%edx -1(%rax,%rbx,1)\n%ebp -1(%rax,%rbx,1)\n%esp -1(%rax,%rbx,1)\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatAR) {
  EXPECT_NE(RepeatAR(/*f*/ nullptr, addresses_singleton_, "{mem} %{reg}")
            .find("-1(%rax,%rbx,1) %rax\n-1(%rax,%rbx,1) %rbx\n-1(%rax,%rbx,1) %rcx\n"
                  "-1(%rax,%rbx,1) %rdx\n-1(%rax,%rbx,1) %rbp\n-1(%rax,%rbx,1) %rsp\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatAr) {
  EXPECT_NE(RepeatAr(/*f*/ nullptr, addresses_singleton_, "{mem} %{reg}")
            .find("-1(%rax,%rbx,1) %eax\n-1(%rax,%rbx,1) %ebx\n-1(%rax,%rbx,1) %ecx\n"
                  "-1(%rax,%rbx,1) %edx\n-1(%rax,%rbx,1) %ebp\n-1(%rax,%rbx,1) %esp\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatFA) {
  EXPECT_NE(RepeatFA(/*f*/ nullptr, addresses_singleton_, "%{reg} {mem}").
            find("%xmm0 -1(%rax,%rbx,1)\n%xmm1 -1(%rax,%rbx,1)\n%xmm2 -1(%rax,%rbx,1)\n"
                 "%xmm3 -1(%rax,%rbx,1)\n%xmm4 -1(%rax,%rbx,1)\n%xmm5 -1(%rax,%rbx,1)\n"),
            std::string::npos);
}

TEST_F(AssemblerX86_64Test, RepeatAF) {
  EXPECT_NE(RepeatAF(/*f*/ nullptr, addresses_singleton_, "{mem} %{reg}")
            .find("-1(%rax,%rbx,1) %xmm0\n-1(%rax,%rbx,1) %xmm1\n-1(%rax,%rbx,1) %xmm2\n"
                  "-1(%rax,%rbx,1) %xmm3\n-1(%rax,%rbx,1) %xmm4\n-1(%rax,%rbx,1) %xmm5\n"),
            std::string::npos);
}

//
// Actual x86-64 instruction assembler tests.
//

TEST_F(AssemblerX86_64Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

TEST_F(AssemblerX86_64Test, PopqAllAddresses) {
  // Make sure all addressing modes combinations are tested at least once.
  std::vector<x86_64::Address> all_addresses;
  for (x86_64::CpuRegister* base : GetRegisters()) {
    // Base only.
    all_addresses.push_back(x86_64::Address(*base, -1));
    all_addresses.push_back(x86_64::Address(*base, 0));
    all_addresses.push_back(x86_64::Address(*base, 1));
    all_addresses.push_back(x86_64::Address(*base, 123456789));
    for (x86_64::CpuRegister* index : GetRegisters()) {
      if (index->AsRegister() == x86_64::RSP) {
        // Index cannot be RSP.
        continue;
      } else if (base->AsRegister() == index->AsRegister()) {
       // Index only.
       all_addresses.push_back(x86_64::Address(*index, x86_64::TIMES_1, -1));
       all_addresses.push_back(x86_64::Address(*index, x86_64::TIMES_2, 0));
       all_addresses.push_back(x86_64::Address(*index, x86_64::TIMES_4, 1));
       all_addresses.push_back(x86_64::Address(*index, x86_64::TIMES_8, 123456789));
      }
      // Base and index.
      all_addresses.push_back(x86_64::Address(*base, *index, x86_64::TIMES_1, -1));
      all_addresses.push_back(x86_64::Address(*base, *index, x86_64::TIMES_2, 0));
      all_addresses.push_back(x86_64::Address(*base, *index, x86_64::TIMES_4, 1));
      all_addresses.push_back(x86_64::Address(*base, *index, x86_64::TIMES_8, 123456789));
    }
  }
  DriverStr(RepeatA(&x86_64::X86_64Assembler::popq, all_addresses, "popq {mem}"), "popq");
}

TEST_F(AssemblerX86_64Test, PushqRegs) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::pushq, "pushq %{reg}"), "pushq");
}

TEST_F(AssemblerX86_64Test, PushqImm) {
  DriverStr(RepeatI(&x86_64::X86_64Assembler::pushq, /*imm_bytes*/ 4U,
                    "pushq ${imm}"), "pushqi");
}

TEST_F(AssemblerX86_64Test, MovqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::movq, "movq %{reg2}, %{reg1}"), "movq");
}

TEST_F(AssemblerX86_64Test, MovqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::movq, /*imm_bytes*/ 8U,
                     "movq ${imm}, %{reg}"), "movqi");
}

TEST_F(AssemblerX86_64Test, MovlRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::movl, "mov %{reg2}, %{reg1}"), "movl");
}

TEST_F(AssemblerX86_64Test, MovlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::movl, /*imm_bytes*/ 4U,
                     "mov ${imm}, %{reg}"), "movli");
}

TEST_F(AssemblerX86_64Test, AddqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::addq, "addq %{reg2}, %{reg1}"), "addq");
}

TEST_F(AssemblerX86_64Test, AddqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::addq, /*imm_bytes*/ 4U,
                     "addq ${imm}, %{reg}"), "addqi");
}

TEST_F(AssemblerX86_64Test, AddlRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::addl, "add %{reg2}, %{reg1}"), "addl");
}

TEST_F(AssemblerX86_64Test, AddlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::addl, /*imm_bytes*/ 4U,
                     "add ${imm}, %{reg}"), "addli");
}

TEST_F(AssemblerX86_64Test, Addw) {
  DriverStr(
      RepeatAI(&x86_64::X86_64Assembler::addw, /*imm_bytes*/2U, "addw ${imm}, {mem}"), "addw");
}

TEST_F(AssemblerX86_64Test, ImulqReg1) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::imulq, "imulq %{reg}"), "imulq");
}

TEST_F(AssemblerX86_64Test, ImulqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::imulq, "imulq %{reg2}, %{reg1}"), "imulq");
}

TEST_F(AssemblerX86_64Test, ImulqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::imulq, /*imm_bytes*/ 4U,
                     "imulq ${imm}, %{reg}, %{reg}"),
            "imulqi");
}

TEST_F(AssemblerX86_64Test, ImullRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::imull, "imul %{reg2}, %{reg1}"), "imull");
}

TEST_F(AssemblerX86_64Test, ImullImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::imull, /*imm_bytes*/ 4U,
                     "imull ${imm}, %{reg}, %{reg}"),
            "imulli");
}

TEST_F(AssemblerX86_64Test, Mull) {
  DriverStr(Repeatr(&x86_64::X86_64Assembler::mull, "mull %{reg}"), "mull");
}

TEST_F(AssemblerX86_64Test, SubqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::subq, "subq %{reg2}, %{reg1}"), "subq");
}

TEST_F(AssemblerX86_64Test, SubqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::subq, /*imm_bytes*/ 4U,
                     "subq ${imm}, %{reg}"), "subqi");
}

TEST_F(AssemblerX86_64Test, SublRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::subl, "sub %{reg2}, %{reg1}"), "subl");
}

TEST_F(AssemblerX86_64Test, SublImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::subl, /*imm_bytes*/ 4U,
                     "sub ${imm}, %{reg}"), "subli");
}

// Shll only allows CL as the shift count.
std::string shll_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->shll(*reg, shifter);
    str << "shll %cl, %" << assembler_test->GetSecondaryRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, ShllReg) {
  DriverFn(&shll_fn, "shll");
}

TEST_F(AssemblerX86_64Test, ShllImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::shll, /*imm_bytes*/ 1U,
                     "shll ${imm}, %{reg}"), "shlli");
}

// Shlq only allows CL as the shift count.
std::string shlq_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->shlq(*reg, shifter);
    str << "shlq %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, ShlqReg) {
  DriverFn(&shlq_fn, "shlq");
}

TEST_F(AssemblerX86_64Test, ShlqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::shlq, /*imm_bytes*/ 1U,
                     "shlq ${imm}, %{reg}"), "shlqi");
}

// Shrl only allows CL as the shift count.
std::string shrl_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->shrl(*reg, shifter);
    str << "shrl %cl, %" << assembler_test->GetSecondaryRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, ShrlReg) {
  DriverFn(&shrl_fn, "shrl");
}

TEST_F(AssemblerX86_64Test, ShrlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::shrl, /*imm_bytes*/ 1U, "shrl ${imm}, %{reg}"), "shrli");
}

// Shrq only allows CL as the shift count.
std::string shrq_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->shrq(*reg, shifter);
    str << "shrq %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, ShrqReg) {
  DriverFn(&shrq_fn, "shrq");
}

TEST_F(AssemblerX86_64Test, ShrqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::shrq, /*imm_bytes*/ 1U, "shrq ${imm}, %{reg}"), "shrqi");
}

// Sarl only allows CL as the shift count.
std::string sarl_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->sarl(*reg, shifter);
    str << "sarl %cl, %" << assembler_test->GetSecondaryRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, SarlReg) {
  DriverFn(&sarl_fn, "sarl");
}

TEST_F(AssemblerX86_64Test, SarlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::sarl, /*imm_bytes*/ 1U, "sarl ${imm}, %{reg}"), "sarli");
}

// Sarq only allows CL as the shift count.
std::string sarq_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->sarq(*reg, shifter);
    str << "sarq %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, SarqReg) {
  DriverFn(&sarq_fn, "sarq");
}

TEST_F(AssemblerX86_64Test, SarqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::sarq, /*imm_bytes*/ 1U, "sarq ${imm}, %{reg}"), "sarqi");
}

// Rorl only allows CL as the shift count.
std::string rorl_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->rorl(*reg, shifter);
    str << "rorl %cl, %" << assembler_test->GetSecondaryRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, RorlReg) {
  DriverFn(&rorl_fn, "rorl");
}

TEST_F(AssemblerX86_64Test, RorlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::rorl, /*imm_bytes*/ 1U, "rorl ${imm}, %{reg}"), "rorli");
}

// Roll only allows CL as the shift count.
std::string roll_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->roll(*reg, shifter);
    str << "roll %cl, %" << assembler_test->GetSecondaryRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, RollReg) {
  DriverFn(&roll_fn, "roll");
}

TEST_F(AssemblerX86_64Test, RollImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::roll, /*imm_bytes*/ 1U, "roll ${imm}, %{reg}"), "rolli");
}

// Rorq only allows CL as the shift count.
std::string rorq_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->rorq(*reg, shifter);
    str << "rorq %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, RorqReg) {
  DriverFn(&rorq_fn, "rorq");
}

TEST_F(AssemblerX86_64Test, RorqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::rorq, /*imm_bytes*/ 1U, "rorq ${imm}, %{reg}"), "rorqi");
}

// Rolq only allows CL as the shift count.
std::string rolq_fn(AssemblerX86_64Test::Base* assembler_test, x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  x86_64::CpuRegister shifter(x86_64::RCX);
  for (auto reg : registers) {
    assembler->rolq(*reg, shifter);
    str << "rolq %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86_64Test, RolqReg) {
  DriverFn(&rolq_fn, "rolq");
}

TEST_F(AssemblerX86_64Test, RolqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::rolq, /*imm_bytes*/ 1U, "rolq ${imm}, %{reg}"), "rolqi");
}

TEST_F(AssemblerX86_64Test, CmpqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::cmpq, "cmpq %{reg2}, %{reg1}"), "cmpq");
}

TEST_F(AssemblerX86_64Test, CmpqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::cmpq,
                     /*imm_bytes*/ 4U,
                     "cmpq ${imm}, %{reg}"), "cmpqi");  // only imm32
}

TEST_F(AssemblerX86_64Test, CmplRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::cmpl, "cmp %{reg2}, %{reg1}"), "cmpl");
}

TEST_F(AssemblerX86_64Test, CmplImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::cmpl, /*imm_bytes*/ 4U, "cmpl ${imm}, %{reg}"), "cmpli");
}

TEST_F(AssemblerX86_64Test, Testl) {
  // Note: uses different order for GCC than usual. This makes GCC happy, and doesn't have an
  // impact on functional correctness.
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::testl, "testl %{reg1}, %{reg2}"), "testl");
}

TEST_F(AssemblerX86_64Test, Negq) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::negq, "negq %{reg}"), "negq");
}

TEST_F(AssemblerX86_64Test, Negl) {
  DriverStr(Repeatr(&x86_64::X86_64Assembler::negl, "negl %{reg}"), "negl");
}

TEST_F(AssemblerX86_64Test, Notq) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::notq, "notq %{reg}"), "notq");
}

TEST_F(AssemblerX86_64Test, Notl) {
  DriverStr(Repeatr(&x86_64::X86_64Assembler::notl, "notl %{reg}"), "notl");
}

TEST_F(AssemblerX86_64Test, AndqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::andq, "andq %{reg2}, %{reg1}"), "andq");
}

TEST_F(AssemblerX86_64Test, AndqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::andq,
                     /*imm_bytes*/ 4U,
                     "andq ${imm}, %{reg}"), "andqi");  // only imm32
}

TEST_F(AssemblerX86_64Test, AndlRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::andl, "andl %{reg2}, %{reg1}"), "andl");
}

TEST_F(AssemblerX86_64Test, AndlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::andl,
                     /*imm_bytes*/ 4U,
                     "andl ${imm}, %{reg}"), "andli");
}

TEST_F(AssemblerX86_64Test, OrqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::orq, "orq %{reg2}, %{reg1}"), "orq");
}

TEST_F(AssemblerX86_64Test, OrlRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::orl, "orl %{reg2}, %{reg1}"), "orl");
}

TEST_F(AssemblerX86_64Test, OrlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::orl,
                     /*imm_bytes*/ 4U, "orl ${imm}, %{reg}"), "orli");
}

TEST_F(AssemblerX86_64Test, XorqRegs) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::xorq, "xorq %{reg2}, %{reg1}"), "xorq");
}

TEST_F(AssemblerX86_64Test, XorqImm) {
  DriverStr(RepeatRI(&x86_64::X86_64Assembler::xorq,
                     /*imm_bytes*/ 4U, "xorq ${imm}, %{reg}"), "xorqi");
}

TEST_F(AssemblerX86_64Test, XorlRegs) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::xorl, "xor %{reg2}, %{reg1}"), "xorl");
}

TEST_F(AssemblerX86_64Test, XorlImm) {
  DriverStr(RepeatrI(&x86_64::X86_64Assembler::xorl,
                     /*imm_bytes*/ 4U, "xor ${imm}, %{reg}"), "xorli");
}

TEST_F(AssemblerX86_64Test, Xchgq) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::xchgq, "xchgq %{reg2}, %{reg1}"), "xchgq");
}

TEST_F(AssemblerX86_64Test, Xchgl) {
  // TODO: Test is disabled because GCC generates 0x87 0xC0 for xchgl eax, eax. All other cases
  // are the same. Anyone know why it doesn't emit a simple 0x90? It does so for xchgq rax, rax...
  // DriverStr(Repeatrr(&x86_64::X86_64Assembler::xchgl, "xchgl %{reg2}, %{reg1}"), "xchgl");
}

TEST_F(AssemblerX86_64Test, LockCmpxchgl) {
  DriverStr(RepeatAr(&x86_64::X86_64Assembler::LockCmpxchgl,
                     "lock cmpxchgl %{reg}, {mem}"), "lock_cmpxchgl");
}

TEST_F(AssemblerX86_64Test, LockCmpxchgq) {
  DriverStr(RepeatAR(&x86_64::X86_64Assembler::LockCmpxchgq,
                     "lock cmpxchg %{reg}, {mem}"), "lock_cmpxchg");
}

TEST_F(AssemblerX86_64Test, MovqStore) {
  DriverStr(RepeatAR(&x86_64::X86_64Assembler::movq, "movq %{reg}, {mem}"), "movq_s");
}

TEST_F(AssemblerX86_64Test, MovqLoad) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::movq, "movq {mem}, %{reg}"), "movq_l");
}

TEST_F(AssemblerX86_64Test, MovlStore) {
  DriverStr(RepeatAr(&x86_64::X86_64Assembler::movl, "movl %{reg}, {mem}"), "movl_s");
}

TEST_F(AssemblerX86_64Test, MovlLoad) {
  DriverStr(RepeatrA(&x86_64::X86_64Assembler::movl, "movl {mem}, %{reg}"), "movl_l");
}

TEST_F(AssemblerX86_64Test, MovwStore) {
  DriverStr(RepeatAw(&x86_64::X86_64Assembler::movw, "movw %{reg}, {mem}"), "movw_s");
}

TEST_F(AssemblerX86_64Test, MovbStore) {
  DriverStr(RepeatAb(&x86_64::X86_64Assembler::movb, "movb %{reg}, {mem}"), "movb_s");
}

TEST_F(AssemblerX86_64Test, Cmpw) {
  DriverStr(
      RepeatAI(&x86_64::X86_64Assembler::cmpw, /*imm_bytes*/ 2U, "cmpw ${imm}, {mem}"), "cmpw");
}

TEST_F(AssemblerX86_64Test, MovqAddrImm) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::movq,
                     /*imm_bytes*/ 4U,
                     "movq ${imm}, {mem}"), "movq");  // only imm32
}

TEST_F(AssemblerX86_64Test, MovlAddrImm) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::movl,
                     /*imm_bytes*/ 4U, "movl ${imm}, {mem}"), "movl");
}

TEST_F(AssemblerX86_64Test, MovwAddrImm) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::movw,
                     /*imm_bytes*/ 2U, "movw ${imm}, {mem}"), "movw");
}

TEST_F(AssemblerX86_64Test, MovbAddrImm) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::movb,
                     /*imm_bytes*/ 1U, "movb ${imm}, {mem}"), "movb");
}

TEST_F(AssemblerX86_64Test, Movntl) {
  DriverStr(RepeatAr(&x86_64::X86_64Assembler::movntl, "movntil %{reg}, {mem}"), "movntl");
}

TEST_F(AssemblerX86_64Test, Movntq) {
  DriverStr(RepeatAR(&x86_64::X86_64Assembler::movntq, "movntiq %{reg}, {mem}"), "movntq");
}

TEST_F(AssemblerX86_64Test, Cvtsi2ssAddr) {
  GetAssembler()->cvtsi2ss(x86_64::XmmRegister(x86_64::XMM0),
                           x86_64::Address(x86_64::CpuRegister(x86_64::RAX), 0),
                           /*is64bit*/ false);
  GetAssembler()->cvtsi2ss(x86_64::XmmRegister(x86_64::XMM0),
                           x86_64::Address(x86_64::CpuRegister(x86_64::RAX), 0),
                           /*is64bit*/ true);
  const char* expected = "cvtsi2ss 0(%RAX), %xmm0\n"
                         "cvtsi2ssq 0(%RAX), %xmm0\n";
  DriverStr(expected, "cvtsi2ss");
}

TEST_F(AssemblerX86_64Test, Cvtsi2sdAddr) {
  GetAssembler()->cvtsi2sd(x86_64::XmmRegister(x86_64::XMM0),
                           x86_64::Address(x86_64::CpuRegister(x86_64::RAX), 0),
                           /*is64bit*/ false);
  GetAssembler()->cvtsi2sd(x86_64::XmmRegister(x86_64::XMM0),
                           x86_64::Address(x86_64::CpuRegister(x86_64::RAX), 0),
                           /*is64bit*/ true);
  const char* expected = "cvtsi2sd 0(%RAX), %xmm0\n"
                         "cvtsi2sdq 0(%RAX), %xmm0\n";
  DriverStr(expected, "cvtsi2sd");
}

TEST_F(AssemblerX86_64Test, CmpqAddr) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::cmpq, "cmpq {mem}, %{reg}"), "cmpq");
}

TEST_F(AssemblerX86_64Test, MovsxdAddr) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::movsxd, "movslq {mem}, %{reg}"), "movsxd");
}

TEST_F(AssemblerX86_64Test, TestqAddr) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::testq, "testq {mem}, %{reg}"), "testq");
}

TEST_F(AssemblerX86_64Test, AddqAddr) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::addq, "addq {mem}, %{reg}"), "addq");
}

TEST_F(AssemblerX86_64Test, SubqAddr) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::subq, "subq {mem}, %{reg}"), "subq");
}

TEST_F(AssemblerX86_64Test, Cvtss2sdAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::cvtss2sd, "cvtss2sd {mem}, %{reg}"), "cvtss2sd");
}

TEST_F(AssemblerX86_64Test, Cvtsd2ssAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::cvtsd2ss, "cvtsd2ss {mem}, %{reg}"), "cvtsd2ss");
}

TEST_F(AssemblerX86_64Test, ComissAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::comiss, "comiss {mem}, %{reg}"), "comiss");
}

TEST_F(AssemblerX86_64Test, ComisdAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::comisd, "comisd {mem}, %{reg}"), "comisd");
}

TEST_F(AssemblerX86_64Test, UComissAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::ucomiss, "ucomiss {mem}, %{reg}"), "ucomiss");
}

TEST_F(AssemblerX86_64Test, UComisdAddr) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::ucomisd, "ucomisd {mem}, %{reg}"), "ucomisd");
}

TEST_F(AssemblerX86_64Test, Andq) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::andq, "andq {mem}, %{reg}"), "andq");
}

TEST_F(AssemblerX86_64Test, Orq) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::orq, "orq {mem}, %{reg}"), "orq");
}

TEST_F(AssemblerX86_64Test, Xorq) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::xorq, "xorq {mem}, %{reg}"), "xorq");
}

TEST_F(AssemblerX86_64Test, RepneScasb) {
  GetAssembler()->repne_scasb();
  const char* expected = "repne scasb\n";
  DriverStr(expected, "repne_scasb");
}

TEST_F(AssemblerX86_64Test, RepneScasw) {
  GetAssembler()->repne_scasw();
  const char* expected = "repne scasw\n";
  DriverStr(expected, "repne_scasw");
}

TEST_F(AssemblerX86_64Test, RepMovsw) {
  GetAssembler()->rep_movsw();
  const char* expected = "rep movsw\n";
  DriverStr(expected, "rep_movsw");
}

TEST_F(AssemblerX86_64Test, Movsxd) {
  DriverStr(RepeatRr(&x86_64::X86_64Assembler::movsxd, "movsxd %{reg2}, %{reg1}"), "movsxd");
}

TEST_F(AssemblerX86_64Test, Movaps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::movaps, "movaps %{reg2}, %{reg1}"), "movaps");
}

TEST_F(AssemblerX86_64Test, MovapsStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movaps, "movaps %{reg}, {mem}"), "movaps_s");
}

TEST_F(AssemblerX86_64Test, MovapsLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movaps, "movaps {mem}, %{reg}"), "movaps_l");
}

TEST_F(AssemblerX86_64Test, MovupsStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movups, "movups %{reg}, {mem}"), "movups_s");
}

TEST_F(AssemblerX86_64Test, MovupsLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movups, "movups {mem}, %{reg}"), "movups_l");
}

TEST_F(AssemblerX86_64Test, Movss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::movss, "movss %{reg2}, %{reg1}"), "movss");
}

TEST_F(AssemblerX86_64Test, Movapd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::movapd, "movapd %{reg2}, %{reg1}"), "movapd");
}

TEST_F(AssemblerX86_64Test, MovapdStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movapd, "movapd %{reg}, {mem}"), "movapd_s");
}

TEST_F(AssemblerX86_64Test, MovapdLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movapd, "movapd {mem}, %{reg}"), "movapd_l");
}

TEST_F(AssemblerX86_64Test, MovupdStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movupd, "movupd %{reg}, {mem}"), "movupd_s");
}

TEST_F(AssemblerX86_64Test, MovupdLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movupd, "movupd {mem}, %{reg}"), "movupd_l");
}

TEST_F(AssemblerX86_64Test, Movsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::movsd, "movsd %{reg2}, %{reg1}"), "movsd");
}

TEST_F(AssemblerX86_64Test, Movdqa) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::movdqa, "movdqa %{reg2}, %{reg1}"), "movdqa");
}

TEST_F(AssemblerX86_64Test, MovdqaStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movdqa, "movdqa %{reg}, {mem}"), "movdqa_s");
}

TEST_F(AssemblerX86_64Test, MovdqaLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movdqa, "movdqa {mem}, %{reg}"), "movdqa_l");
}

TEST_F(AssemblerX86_64Test, MovdquStore) {
  DriverStr(RepeatAF(&x86_64::X86_64Assembler::movdqu, "movdqu %{reg}, {mem}"), "movdqu_s");
}

TEST_F(AssemblerX86_64Test, MovdquLoad) {
  DriverStr(RepeatFA(&x86_64::X86_64Assembler::movdqu, "movdqu {mem}, %{reg}"), "movdqu_l");
}

TEST_F(AssemblerX86_64Test, Movd1) {
  DriverStr(RepeatFR(&x86_64::X86_64Assembler::movd, "movd %{reg2}, %{reg1}"), "movd.1");
}

TEST_F(AssemblerX86_64Test, Movd2) {
  DriverStr(RepeatRF(&x86_64::X86_64Assembler::movd, "movd %{reg2}, %{reg1}"), "movd.2");
}

TEST_F(AssemblerX86_64Test, Addss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::addss, "addss %{reg2}, %{reg1}"), "addss");
}

TEST_F(AssemblerX86_64Test, Addsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::addsd, "addsd %{reg2}, %{reg1}"), "addsd");
}

TEST_F(AssemblerX86_64Test, Addps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::addps, "addps %{reg2}, %{reg1}"), "addps");
}

TEST_F(AssemblerX86_64Test, Addpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::addpd, "addpd %{reg2}, %{reg1}"), "addpd");
}

TEST_F(AssemblerX86_64Test, Subss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::subss, "subss %{reg2}, %{reg1}"), "subss");
}

TEST_F(AssemblerX86_64Test, Subsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::subsd, "subsd %{reg2}, %{reg1}"), "subsd");
}

TEST_F(AssemblerX86_64Test, Subps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::subps, "subps %{reg2}, %{reg1}"), "subps");
}

TEST_F(AssemblerX86_64Test, Subpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::subpd, "subpd %{reg2}, %{reg1}"), "subpd");
}

TEST_F(AssemblerX86_64Test, Mulss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::mulss, "mulss %{reg2}, %{reg1}"), "mulss");
}

TEST_F(AssemblerX86_64Test, Mulsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::mulsd, "mulsd %{reg2}, %{reg1}"), "mulsd");
}

TEST_F(AssemblerX86_64Test, Mulps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::mulps, "mulps %{reg2}, %{reg1}"), "mulps");
}

TEST_F(AssemblerX86_64Test, Mulpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::mulpd, "mulpd %{reg2}, %{reg1}"), "mulpd");
}

TEST_F(AssemblerX86_64Test, Divss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::divss, "divss %{reg2}, %{reg1}"), "divss");
}

TEST_F(AssemblerX86_64Test, Divsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::divsd, "divsd %{reg2}, %{reg1}"), "divsd");
}

TEST_F(AssemblerX86_64Test, Divps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::divps, "divps %{reg2}, %{reg1}"), "divps");
}

TEST_F(AssemblerX86_64Test, Divpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::divpd, "divpd %{reg2}, %{reg1}"), "divpd");
}

TEST_F(AssemblerX86_64Test, Paddb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::paddb, "paddb %{reg2}, %{reg1}"), "paddb");
}

TEST_F(AssemblerX86_64Test, Psubb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::psubb, "psubb %{reg2}, %{reg1}"), "psubb");
}

TEST_F(AssemblerX86_64Test, Paddw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::paddw, "paddw %{reg2}, %{reg1}"), "paddw");
}

TEST_F(AssemblerX86_64Test, Psubw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::psubw, "psubw %{reg2}, %{reg1}"), "psubw");
}

TEST_F(AssemblerX86_64Test, Pmullw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmullw, "pmullw %{reg2}, %{reg1}"), "pmullw");
}

TEST_F(AssemblerX86_64Test, Paddd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::paddd, "paddd %{reg2}, %{reg1}"), "paddd");
}

TEST_F(AssemblerX86_64Test, Psubd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::psubd, "psubd %{reg2}, %{reg1}"), "psubd");
}

TEST_F(AssemblerX86_64Test, Pmulld) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmulld, "pmulld %{reg2}, %{reg1}"), "pmulld");
}

TEST_F(AssemblerX86_64Test, Paddq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::paddq, "paddq %{reg2}, %{reg1}"), "paddq");
}

TEST_F(AssemblerX86_64Test, Psubq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::psubq, "psubq %{reg2}, %{reg1}"), "psubq");
}

TEST_F(AssemblerX86_64Test, Cvtsi2ss) {
  DriverStr(RepeatFr(&x86_64::X86_64Assembler::cvtsi2ss, "cvtsi2ss %{reg2}, %{reg1}"), "cvtsi2ss");
}

TEST_F(AssemblerX86_64Test, Cvtsi2sd) {
  DriverStr(RepeatFr(&x86_64::X86_64Assembler::cvtsi2sd, "cvtsi2sd %{reg2}, %{reg1}"), "cvtsi2sd");
}

TEST_F(AssemblerX86_64Test, Cvtss2si) {
  DriverStr(RepeatrF(&x86_64::X86_64Assembler::cvtss2si, "cvtss2si %{reg2}, %{reg1}"), "cvtss2si");
}

TEST_F(AssemblerX86_64Test, Cvtss2sd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::cvtss2sd, "cvtss2sd %{reg2}, %{reg1}"), "cvtss2sd");
}

TEST_F(AssemblerX86_64Test, Cvtsd2si) {
  DriverStr(RepeatrF(&x86_64::X86_64Assembler::cvtsd2si, "cvtsd2si %{reg2}, %{reg1}"), "cvtsd2si");
}

TEST_F(AssemblerX86_64Test, Cvttss2si) {
  DriverStr(RepeatrF(&x86_64::X86_64Assembler::cvttss2si, "cvttss2si %{reg2}, %{reg1}"),
            "cvttss2si");
}

TEST_F(AssemblerX86_64Test, Cvttsd2si) {
  DriverStr(RepeatrF(&x86_64::X86_64Assembler::cvttsd2si, "cvttsd2si %{reg2}, %{reg1}"),
            "cvttsd2si");
}

TEST_F(AssemblerX86_64Test, Cvtsd2ss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::cvtsd2ss, "cvtsd2ss %{reg2}, %{reg1}"), "cvtsd2ss");
}

TEST_F(AssemblerX86_64Test, Cvtdq2ps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::cvtdq2ps, "cvtdq2ps %{reg2}, %{reg1}"), "cvtdq2ps");
}

TEST_F(AssemblerX86_64Test, Cvtdq2pd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::cvtdq2pd, "cvtdq2pd %{reg2}, %{reg1}"), "cvtdq2pd");
}

TEST_F(AssemblerX86_64Test, Comiss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::comiss, "comiss %{reg2}, %{reg1}"), "comiss");
}

TEST_F(AssemblerX86_64Test, Comisd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::comisd, "comisd %{reg2}, %{reg1}"), "comisd");
}

TEST_F(AssemblerX86_64Test, Ucomiss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::ucomiss, "ucomiss %{reg2}, %{reg1}"), "ucomiss");
}

TEST_F(AssemblerX86_64Test, Ucomisd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::ucomisd, "ucomisd %{reg2}, %{reg1}"), "ucomisd");
}

TEST_F(AssemblerX86_64Test, Sqrtss) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::sqrtss, "sqrtss %{reg2}, %{reg1}"), "sqrtss");
}

TEST_F(AssemblerX86_64Test, Sqrtsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::sqrtsd, "sqrtsd %{reg2}, %{reg1}"), "sqrtsd");
}

TEST_F(AssemblerX86_64Test, Roundss) {
  DriverStr(RepeatFFI(&x86_64::X86_64Assembler::roundss, /*imm_bytes*/ 1U,
                      "roundss ${imm}, %{reg2}, %{reg1}"), "roundss");
}

TEST_F(AssemblerX86_64Test, Roundsd) {
  DriverStr(RepeatFFI(&x86_64::X86_64Assembler::roundsd, /*imm_bytes*/ 1U,
                      "roundsd ${imm}, %{reg2}, %{reg1}"), "roundsd");
}

TEST_F(AssemblerX86_64Test, Xorps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::xorps, "xorps %{reg2}, %{reg1}"), "xorps");
}

TEST_F(AssemblerX86_64Test, Xorpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::xorpd, "xorpd %{reg2}, %{reg1}"), "xorpd");
}

TEST_F(AssemblerX86_64Test, Pxor) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pxor, "pxor %{reg2}, %{reg1}"), "pxor");
}

TEST_F(AssemblerX86_64Test, Andps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::andps, "andps %{reg2}, %{reg1}"), "andps");
}

TEST_F(AssemblerX86_64Test, Andpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::andpd, "andpd %{reg2}, %{reg1}"), "andpd");
}

TEST_F(AssemblerX86_64Test, Pand) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pand, "pand %{reg2}, %{reg1}"), "pand");
}

TEST_F(AssemblerX86_64Test, andnpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::andnpd, "andnpd %{reg2}, %{reg1}"), "andnpd");
}

TEST_F(AssemblerX86_64Test, andnps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::andnps, "andnps %{reg2}, %{reg1}"), "andnps");
}

TEST_F(AssemblerX86_64Test, Pandn) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pandn, "pandn %{reg2}, %{reg1}"), "pandn");
}

TEST_F(AssemblerX86_64Test, Orps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::orps, "orps %{reg2}, %{reg1}"), "orps");
}

TEST_F(AssemblerX86_64Test, Orpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::orpd, "orpd %{reg2}, %{reg1}"), "orpd");
}

TEST_F(AssemblerX86_64Test, Por) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::por, "por %{reg2}, %{reg1}"), "por");
}

TEST_F(AssemblerX86_64Test, Pavgb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pavgb, "pavgb %{reg2}, %{reg1}"), "pavgb");
}

TEST_F(AssemblerX86_64Test, Pavgw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pavgw, "pavgw %{reg2}, %{reg1}"), "pavgw");
}

TEST_F(AssemblerX86_64Test, Psadbw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::psadbw, "psadbw %{reg2}, %{reg1}"), "psadbw");
}

TEST_F(AssemblerX86_64Test, Pmaddwd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaddwd, "pmaddwd %{reg2}, %{reg1}"), "pmadwd");
}

TEST_F(AssemblerX86_64Test, Phaddw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::phaddw, "phaddw %{reg2}, %{reg1}"), "phaddw");
}

TEST_F(AssemblerX86_64Test, Phaddd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::phaddd, "phaddd %{reg2}, %{reg1}"), "phaddd");
}

TEST_F(AssemblerX86_64Test, Haddps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::haddps, "haddps %{reg2}, %{reg1}"), "haddps");
}

TEST_F(AssemblerX86_64Test, Haddpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::haddpd, "haddpd %{reg2}, %{reg1}"), "haddpd");
}

TEST_F(AssemblerX86_64Test, Phsubw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::phsubw, "phsubw %{reg2}, %{reg1}"), "phsubw");
}

TEST_F(AssemblerX86_64Test, Phsubd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::phsubd, "phsubd %{reg2}, %{reg1}"), "phsubd");
}

TEST_F(AssemblerX86_64Test, Hsubps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::hsubps, "hsubps %{reg2}, %{reg1}"), "hsubps");
}

TEST_F(AssemblerX86_64Test, Hsubpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::hsubpd, "hsubpd %{reg2}, %{reg1}"), "hsubpd");
}

TEST_F(AssemblerX86_64Test, Pminsb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminsb, "pminsb %{reg2}, %{reg1}"), "pminsb");
}

TEST_F(AssemblerX86_64Test, Pmaxsb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxsb, "pmaxsb %{reg2}, %{reg1}"), "pmaxsb");
}

TEST_F(AssemblerX86_64Test, Pminsw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminsw, "pminsw %{reg2}, %{reg1}"), "pminsw");
}

TEST_F(AssemblerX86_64Test, Pmaxsw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxsw, "pmaxsw %{reg2}, %{reg1}"), "pmaxsw");
}

TEST_F(AssemblerX86_64Test, Pminsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminsd, "pminsd %{reg2}, %{reg1}"), "pminsd");
}

TEST_F(AssemblerX86_64Test, Pmaxsd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxsd, "pmaxsd %{reg2}, %{reg1}"), "pmaxsd");
}

TEST_F(AssemblerX86_64Test, Pminub) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminub, "pminub %{reg2}, %{reg1}"), "pminub");
}

TEST_F(AssemblerX86_64Test, Pmaxub) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxub, "pmaxub %{reg2}, %{reg1}"), "pmaxub");
}

TEST_F(AssemblerX86_64Test, Pminuw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminuw, "pminuw %{reg2}, %{reg1}"), "pminuw");
}

TEST_F(AssemblerX86_64Test, Pmaxuw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxuw, "pmaxuw %{reg2}, %{reg1}"), "pmaxuw");
}

TEST_F(AssemblerX86_64Test, Pminud) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pminud, "pminud %{reg2}, %{reg1}"), "pminud");
}

TEST_F(AssemblerX86_64Test, Pmaxud) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pmaxud, "pmaxud %{reg2}, %{reg1}"), "pmaxud");
}

TEST_F(AssemblerX86_64Test, Minps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::minps, "minps %{reg2}, %{reg1}"), "minps");
}

TEST_F(AssemblerX86_64Test, Maxps) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::maxps, "maxps %{reg2}, %{reg1}"), "maxps");
}

TEST_F(AssemblerX86_64Test, Minpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::minpd, "minpd %{reg2}, %{reg1}"), "minpd");
}

TEST_F(AssemblerX86_64Test, Maxpd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::maxpd, "maxpd %{reg2}, %{reg1}"), "maxpd");
}

TEST_F(AssemblerX86_64Test, PCmpeqb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpeqb, "pcmpeqb %{reg2}, %{reg1}"), "pcmpeqb");
}

TEST_F(AssemblerX86_64Test, PCmpeqw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpeqw, "pcmpeqw %{reg2}, %{reg1}"), "pcmpeqw");
}

TEST_F(AssemblerX86_64Test, PCmpeqd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpeqd, "pcmpeqd %{reg2}, %{reg1}"), "pcmpeqd");
}

TEST_F(AssemblerX86_64Test, PCmpeqq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpeqq, "pcmpeqq %{reg2}, %{reg1}"), "pcmpeqq");
}

TEST_F(AssemblerX86_64Test, PCmpgtb) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpgtb, "pcmpgtb %{reg2}, %{reg1}"), "pcmpgtb");
}

TEST_F(AssemblerX86_64Test, PCmpgtw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpgtw, "pcmpgtw %{reg2}, %{reg1}"), "pcmpgtw");
}

TEST_F(AssemblerX86_64Test, PCmpgtd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpgtd, "pcmpgtd %{reg2}, %{reg1}"), "pcmpgtd");
}

TEST_F(AssemblerX86_64Test, PCmpgtq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::pcmpgtq, "pcmpgtq %{reg2}, %{reg1}"), "pcmpgtq");
}

TEST_F(AssemblerX86_64Test, Shufps) {
  DriverStr(RepeatFFI(&x86_64::X86_64Assembler::shufps, /*imm_bytes*/ 1U,
                      "shufps ${imm}, %{reg2}, %{reg1}"), "shufps");
}

TEST_F(AssemblerX86_64Test, Shufpd) {
  DriverStr(RepeatFFI(&x86_64::X86_64Assembler::shufpd, /*imm_bytes*/ 1U,
                      "shufpd ${imm}, %{reg2}, %{reg1}"), "shufpd");
}

TEST_F(AssemblerX86_64Test, PShufd) {
  DriverStr(RepeatFFI(&x86_64::X86_64Assembler::pshufd, /*imm_bytes*/ 1U,
                      "pshufd ${imm}, %{reg2}, %{reg1}"), "pshufd");
}

TEST_F(AssemblerX86_64Test, Punpcklbw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpcklbw,
                     "punpcklbw %{reg2}, %{reg1}"), "punpcklbw");
}

TEST_F(AssemblerX86_64Test, Punpcklwd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpcklwd,
                     "punpcklwd %{reg2}, %{reg1}"), "punpcklwd");
}

TEST_F(AssemblerX86_64Test, Punpckldq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpckldq,
                     "punpckldq %{reg2}, %{reg1}"), "punpckldq");
}

TEST_F(AssemblerX86_64Test, Punpcklqdq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpcklqdq,
                     "punpcklqdq %{reg2}, %{reg1}"), "punpcklqdq");
}

TEST_F(AssemblerX86_64Test, Punpckhbw) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpckhbw,
                     "punpckhbw %{reg2}, %{reg1}"), "punpckhbw");
}

TEST_F(AssemblerX86_64Test, Punpckhwd) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpckhwd,
                     "punpckhwd %{reg2}, %{reg1}"), "punpckhwd");
}

TEST_F(AssemblerX86_64Test, Punpckhdq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpckhdq,
                     "punpckhdq %{reg2}, %{reg1}"), "punpckhdq");
}

TEST_F(AssemblerX86_64Test, Punpckhqdq) {
  DriverStr(RepeatFF(&x86_64::X86_64Assembler::punpckhqdq,
                     "punpckhqdq %{reg2}, %{reg1}"), "punpckhqdq");
}

TEST_F(AssemblerX86_64Test, Psllw) {
  GetAssembler()->psllw(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psllw(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psllw $1, %xmm0\n"
            "psllw $2, %xmm15\n", "psllwi");
}

TEST_F(AssemblerX86_64Test, Pslld) {
  GetAssembler()->pslld(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->pslld(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("pslld $1, %xmm0\n"
            "pslld $2, %xmm15\n", "pslldi");
}

TEST_F(AssemblerX86_64Test, Psllq) {
  GetAssembler()->psllq(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psllq(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psllq $1, %xmm0\n"
            "psllq $2, %xmm15\n", "psllqi");
}

TEST_F(AssemblerX86_64Test, Psraw) {
  GetAssembler()->psraw(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psraw(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psraw $1, %xmm0\n"
            "psraw $2, %xmm15\n", "psrawi");
}

TEST_F(AssemblerX86_64Test, Psrad) {
  GetAssembler()->psrad(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psrad(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psrad $1, %xmm0\n"
            "psrad $2, %xmm15\n", "psradi");
}

TEST_F(AssemblerX86_64Test, Psrlw) {
  GetAssembler()->psrlw(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psrlw(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psrlw $1, %xmm0\n"
            "psrlw $2, %xmm15\n", "psrlwi");
}

TEST_F(AssemblerX86_64Test, Psrld) {
  GetAssembler()->psrld(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psrld(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psrld $1, %xmm0\n"
            "psrld $2, %xmm15\n", "psrldi");
}

TEST_F(AssemblerX86_64Test, Psrlq) {
  GetAssembler()->psrlq(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psrlq(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psrlq $1, %xmm0\n"
            "psrlq $2, %xmm15\n", "psrlqi");
}

TEST_F(AssemblerX86_64Test, Psrldq) {
  GetAssembler()->psrldq(x86_64::XmmRegister(x86_64::XMM0),  x86_64::Immediate(1));
  GetAssembler()->psrldq(x86_64::XmmRegister(x86_64::XMM15), x86_64::Immediate(2));
  DriverStr("psrldq $1, %xmm0\n"
            "psrldq $2, %xmm15\n", "psrldqi");
}

std::string x87_fn(AssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
                   x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;

  assembler->fincstp();
  str << "fincstp\n";

  assembler->fsin();
  str << "fsin\n";

  assembler->fcos();
  str << "fcos\n";

  assembler->fptan();
  str << "fptan\n";

  return str.str();
}

TEST_F(AssemblerX86_64Test, X87) {
  DriverFn(&x87_fn, "x87");
}

TEST_F(AssemblerX86_64Test, FPUIntegerLoads) {
  DriverStr(RepeatA(&x86_64::X86_64Assembler::filds,
                    addresses_singleton_,  // no ext addressing
                    "fildl {mem}"), "filds");
}

TEST_F(AssemblerX86_64Test, FPUIntegerLoadl) {
  DriverStr(RepeatA(&x86_64::X86_64Assembler::fildl,
                    addresses_singleton_,  // no ext addressing
                    "fildll {mem}"), "fildl");
}

TEST_F(AssemblerX86_64Test, FPUIntegerStores) {
  DriverStr(RepeatA(&x86_64::X86_64Assembler::fistps,
                    addresses_singleton_,  // no ext addressing
                    "fistpl {mem}"), "fistps");
}

TEST_F(AssemblerX86_64Test, FPUIntegerStorel) {
  DriverStr(RepeatA(&x86_64::X86_64Assembler::fistpl,
                    addresses_singleton_,  // no ext addressing
                    "fistpll {mem}"), "fistpl");
}

TEST_F(AssemblerX86_64Test, Call) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::call, "call *%{reg}"), "call");
}

TEST_F(AssemblerX86_64Test, Jmp) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::jmp, "jmp *%{reg}"), "jmp");
}

TEST_F(AssemblerX86_64Test, Enter) {
  DriverStr(RepeatI(&x86_64::X86_64Assembler::enter,
                    /*imm_bytes*/ 2U,
                    "enter ${imm}, $0", /*non-negative*/ true), "enter");
}

TEST_F(AssemblerX86_64Test, RetImm) {
  DriverStr(RepeatI(&x86_64::X86_64Assembler::ret,
                    /*imm_bytes*/ 2U,
                    "ret ${imm}", /*non-negative*/ true), "ret");
}

std::string ret_and_leave_fn(AssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
                             x86_64::X86_64Assembler* assembler) {
  std::ostringstream str;

  assembler->ret();
  str << "ret\n";

  assembler->leave();
  str << "leave\n";

  return str.str();
}

TEST_F(AssemblerX86_64Test, RetAndLeave) {
  DriverFn(&ret_and_leave_fn, "retleave");
}

TEST_F(AssemblerX86_64Test, Bswapl) {
  DriverStr(Repeatr(&x86_64::X86_64Assembler::bswapl, "bswap %{reg}"), "bswapl");
}

TEST_F(AssemblerX86_64Test, Bswapq) {
  DriverStr(RepeatR(&x86_64::X86_64Assembler::bswapq, "bswap %{reg}"), "bswapq");
}

TEST_F(AssemblerX86_64Test, Bsfl) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::bsfl, "bsfl %{reg2}, %{reg1}"), "bsfl");
}

TEST_F(AssemblerX86_64Test, BsflAddress) {
  DriverStr(RepeatrA(&x86_64::X86_64Assembler::bsfl, "bsfl {mem}, %{reg}"), "bsfl_address");
}

TEST_F(AssemblerX86_64Test, Bsfq) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::bsfq, "bsfq %{reg2}, %{reg1}"), "bsfq");
}

TEST_F(AssemblerX86_64Test, BsfqAddress) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::bsfq, "bsfq {mem}, %{reg}"), "bsfq_address");
}

TEST_F(AssemblerX86_64Test, Bsrl) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::bsrl, "bsrl %{reg2}, %{reg1}"), "bsrl");
}

TEST_F(AssemblerX86_64Test, BsrlAddress) {
  DriverStr(RepeatrA(&x86_64::X86_64Assembler::bsrl, "bsrl {mem}, %{reg}"), "bsrl_address");
}

TEST_F(AssemblerX86_64Test, Bsrq) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::bsrq, "bsrq %{reg2}, %{reg1}"), "bsrq");
}

TEST_F(AssemblerX86_64Test, BsrqAddress) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::bsrq, "bsrq {mem}, %{reg}"), "bsrq_address");
}

TEST_F(AssemblerX86_64Test, Popcntl) {
  DriverStr(Repeatrr(&x86_64::X86_64Assembler::popcntl, "popcntl %{reg2}, %{reg1}"), "popcntl");
}

TEST_F(AssemblerX86_64Test, PopcntlAddress) {
  DriverStr(RepeatrA(&x86_64::X86_64Assembler::popcntl, "popcntl {mem}, %{reg}"), "popcntl_address");
}

TEST_F(AssemblerX86_64Test, Popcntq) {
  DriverStr(RepeatRR(&x86_64::X86_64Assembler::popcntq, "popcntq %{reg2}, %{reg1}"), "popcntq");
}

TEST_F(AssemblerX86_64Test, PopcntqAddress) {
  DriverStr(RepeatRA(&x86_64::X86_64Assembler::popcntq, "popcntq {mem}, %{reg}"), "popcntq_address");
}

TEST_F(AssemblerX86_64Test, CmovlAddress) {
  GetAssembler()->cmov(x86_64::kEqual, x86_64::CpuRegister(x86_64::R10), x86_64::Address(
      x86_64::CpuRegister(x86_64::RDI), x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_4, 12), false);
  GetAssembler()->cmov(x86_64::kNotEqual, x86_64::CpuRegister(x86_64::RDI), x86_64::Address(
      x86_64::CpuRegister(x86_64::R10), x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_4, 12), false);
  GetAssembler()->cmov(x86_64::kEqual, x86_64::CpuRegister(x86_64::RDI), x86_64::Address(
      x86_64::CpuRegister(x86_64::RDI), x86_64::CpuRegister(x86_64::R9), x86_64::TIMES_4, 12), false);
  const char* expected =
    "cmovzl 0xc(%RDI,%RBX,4), %R10d\n"
    "cmovnzl 0xc(%R10,%RBX,4), %edi\n"
    "cmovzl 0xc(%RDI,%R9,4), %edi\n";
  DriverStr(expected, "cmovl_address");
}

TEST_F(AssemblerX86_64Test, CmovqAddress) {
  GetAssembler()->cmov(x86_64::kEqual, x86_64::CpuRegister(x86_64::R10), x86_64::Address(
      x86_64::CpuRegister(x86_64::RDI), x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_4, 12), true);
  GetAssembler()->cmov(x86_64::kNotEqual, x86_64::CpuRegister(x86_64::RDI), x86_64::Address(
      x86_64::CpuRegister(x86_64::R10), x86_64::CpuRegister(x86_64::RBX), x86_64::TIMES_4, 12), true);
  GetAssembler()->cmov(x86_64::kEqual, x86_64::CpuRegister(x86_64::RDI), x86_64::Address(
      x86_64::CpuRegister(x86_64::RDI), x86_64::CpuRegister(x86_64::R9), x86_64::TIMES_4, 12), true);
  const char* expected =
    "cmovzq 0xc(%RDI,%RBX,4), %R10\n"
    "cmovnzq 0xc(%R10,%RBX,4), %rdi\n"
    "cmovzq 0xc(%RDI,%R9,4), %rdi\n";
  DriverStr(expected, "cmovq_address");
}

TEST_F(AssemblerX86_64Test, Jrcxz) {
  x86_64::NearLabel target;
  GetAssembler()->jrcxz(&target);
  GetAssembler()->addl(x86_64::CpuRegister(x86_64::RDI),
                       x86_64::Address(x86_64::CpuRegister(x86_64::RSP), 4));
  GetAssembler()->Bind(&target);
  const char* expected =
    "jrcxz 1f\n"
    "addl 4(%RSP),%EDI\n"
    "1:\n";

  DriverStr(expected, "jrcxz");
}

TEST_F(AssemblerX86_64Test, NearLabel) {
  // Test both forward and backward branches.
  x86_64::NearLabel start, target;
  GetAssembler()->Bind(&start);
  GetAssembler()->j(x86_64::kEqual, &target);
  GetAssembler()->jmp(&target);
  GetAssembler()->jrcxz(&target);
  GetAssembler()->addl(x86_64::CpuRegister(x86_64::RDI),
                       x86_64::Address(x86_64::CpuRegister(x86_64::RSP), 4));
  GetAssembler()->Bind(&target);
  GetAssembler()->j(x86_64::kNotEqual, &start);
  GetAssembler()->jmp(&start);
  const char* expected =
    "1: je 2f\n"
    "jmp 2f\n"
    "jrcxz 2f\n"
    "addl 4(%RSP),%EDI\n"
    "2: jne 1b\n"
    "jmp 1b\n";

  DriverStr(expected, "near_label");
}

std::string setcc_test_fn(AssemblerX86_64Test::Base* assembler_test,
                          x86_64::X86_64Assembler* assembler) {
  // From Condition
  /*
  kOverflow     =  0,
  kNoOverflow   =  1,
  kBelow        =  2,
  kAboveEqual   =  3,
  kEqual        =  4,
  kNotEqual     =  5,
  kBelowEqual   =  6,
  kAbove        =  7,
  kSign         =  8,
  kNotSign      =  9,
  kParityEven   = 10,
  kParityOdd    = 11,
  kLess         = 12,
  kGreaterEqual = 13,
  kLessEqual    = 14,
  */
  std::string suffixes[15] = { "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "pe", "po",
                               "l", "ge", "le" };

  std::vector<x86_64::CpuRegister*> registers = assembler_test->GetRegisters();
  std::ostringstream str;

  for (auto reg : registers) {
    for (size_t i = 0; i < 15; ++i) {
      assembler->setcc(static_cast<x86_64::Condition>(i), *reg);
      str << "set" << suffixes[i] << " %" << assembler_test->GetQuaternaryRegisterName(*reg) << "\n";
    }
  }

  return str.str();
}

TEST_F(AssemblerX86_64Test, SetCC) {
  DriverFn(&setcc_test_fn, "setcc");
}

TEST_F(AssemblerX86_64Test, MovzxbRegs) {
  DriverStr(Repeatrb(&x86_64::X86_64Assembler::movzxb, "movzbl %{reg2}, %{reg1}"), "movzxb");
}

TEST_F(AssemblerX86_64Test, MovsxbRegs) {
  DriverStr(Repeatrb(&x86_64::X86_64Assembler::movsxb, "movsbl %{reg2}, %{reg1}"), "movsxb");
}

TEST_F(AssemblerX86_64Test, Repnescasw) {
  GetAssembler()->repne_scasw();
  const char* expected = "repne scasw\n";
  DriverStr(expected, "Repnescasw");
}

TEST_F(AssemblerX86_64Test, Repecmpsw) {
  GetAssembler()->repe_cmpsw();
  const char* expected = "repe cmpsw\n";
  DriverStr(expected, "Repecmpsw");
}

TEST_F(AssemblerX86_64Test, Repecmpsl) {
  GetAssembler()->repe_cmpsl();
  const char* expected = "repe cmpsl\n";
  DriverStr(expected, "Repecmpsl");
}

TEST_F(AssemblerX86_64Test, Repecmpsq) {
  GetAssembler()->repe_cmpsq();
  const char* expected = "repe cmpsq\n";
  DriverStr(expected, "Repecmpsq");
}

TEST_F(AssemblerX86_64Test, Cmpb) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::cmpb,
                     /*imm_bytes*/ 1U,
                     "cmpb ${imm}, {mem}"), "cmpb");
}

TEST_F(AssemblerX86_64Test, TestbAddressImmediate) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::testb,
                     /*imm_bytes*/ 1U,
                     "testb ${imm}, {mem}"), "testbi");
}

TEST_F(AssemblerX86_64Test, TestlAddressImmediate) {
  DriverStr(RepeatAI(&x86_64::X86_64Assembler::testl,
                     /*imm_bytes*/ 4U,
                     "testl ${imm}, {mem}"), "testli");
}

class JNIMacroAssemblerX86_64Test : public JNIMacroAssemblerTest<x86_64::X86_64JNIMacroAssembler> {
 public:
  using Base = JNIMacroAssemblerTest<x86_64::X86_64JNIMacroAssembler>;

 protected:
  // Get the typically used name for this architecture, e.g., aarch64, x86-64, ...
  std::string GetArchitectureString() OVERRIDE {
    return "x86_64";
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mi386:x86-64 -Mx86-64,addr64,data32 --no-show-raw-insn";
  }

 private:
};

static x86_64::X86_64ManagedRegister ManagedFromCpu(x86_64::Register r) {
  return x86_64::X86_64ManagedRegister::FromCpuRegister(r);
}

static x86_64::X86_64ManagedRegister ManagedFromFpu(x86_64::FloatRegister r) {
  return x86_64::X86_64ManagedRegister::FromXmmRegister(r);
}

std::string buildframe_test_fn(JNIMacroAssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
                               x86_64::X86_64JNIMacroAssembler* assembler) {
  // TODO: more interesting spill registers / entry spills.

  // Two random spill regs.
  const ManagedRegister raw_spill_regs[] = {
      ManagedFromCpu(x86_64::R10),
      ManagedFromCpu(x86_64::RSI)
  };
  ArrayRef<const ManagedRegister> spill_regs(raw_spill_regs);

  // Three random entry spills.
  ManagedRegisterEntrySpills entry_spills;
  ManagedRegisterSpill spill(ManagedFromCpu(x86_64::RAX), 8, 0);
  entry_spills.push_back(spill);
  ManagedRegisterSpill spill2(ManagedFromCpu(x86_64::RBX), 8, 8);
  entry_spills.push_back(spill2);
  ManagedRegisterSpill spill3(ManagedFromFpu(x86_64::XMM1), 8, 16);
  entry_spills.push_back(spill3);

  x86_64::X86_64ManagedRegister method_reg = ManagedFromCpu(x86_64::RDI);

  size_t frame_size = 10 * kStackAlignment;
  assembler->BuildFrame(frame_size, method_reg, spill_regs, entry_spills);

  // Construct assembly text counterpart.
  std::ostringstream str;
  // (1) Push the spill_regs.
  str << "pushq %rsi\n";
  str << "pushq %r10\n";
  // (2) Move down the stack pointer.
  ssize_t displacement = static_cast<ssize_t>(frame_size) - (spill_regs.size() * 8 + 8);
  str << "subq $" << displacement << ", %rsp\n";
  // (3) Store method reference.
  str << "movq %rdi, (%rsp)\n";
  // (4) Entry spills.
  str << "movq %rax, " << frame_size + 0 << "(%rsp)\n";
  str << "movq %rbx, " << frame_size + 8 << "(%rsp)\n";
  str << "movsd %xmm1, " << frame_size + 16 << "(%rsp)\n";

  return str.str();
}

TEST_F(JNIMacroAssemblerX86_64Test, BuildFrame) {
  DriverFn(&buildframe_test_fn, "BuildFrame");
}

std::string removeframe_test_fn(JNIMacroAssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
                                x86_64::X86_64JNIMacroAssembler* assembler) {
  // TODO: more interesting spill registers / entry spills.

  // Two random spill regs.
  const ManagedRegister raw_spill_regs[] = {
      ManagedFromCpu(x86_64::R10),
      ManagedFromCpu(x86_64::RSI)
  };
  ArrayRef<const ManagedRegister> spill_regs(raw_spill_regs);

  size_t frame_size = 10 * kStackAlignment;
  assembler->RemoveFrame(frame_size, spill_regs, /* may_suspend */ true);

  // Construct assembly text counterpart.
  std::ostringstream str;
  // (1) Move up the stack pointer.
  ssize_t displacement = static_cast<ssize_t>(frame_size) - spill_regs.size() * 8 - 8;
  str << "addq $" << displacement << ", %rsp\n";
  // (2) Pop spill regs.
  str << "popq %r10\n";
  str << "popq %rsi\n";
  str << "ret\n";

  return str.str();
}

TEST_F(JNIMacroAssemblerX86_64Test, RemoveFrame) {
  DriverFn(&removeframe_test_fn, "RemoveFrame");
}

std::string increaseframe_test_fn(
    JNIMacroAssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
    x86_64::X86_64JNIMacroAssembler* assembler) {
  assembler->IncreaseFrameSize(0U);
  assembler->IncreaseFrameSize(kStackAlignment);
  assembler->IncreaseFrameSize(10 * kStackAlignment);

  // Construct assembly text counterpart.
  std::ostringstream str;
  str << "addq $0, %rsp\n";
  str << "addq $-" << kStackAlignment << ", %rsp\n";
  str << "addq $-" << 10 * kStackAlignment << ", %rsp\n";

  return str.str();
}

TEST_F(JNIMacroAssemblerX86_64Test, IncreaseFrame) {
  DriverFn(&increaseframe_test_fn, "IncreaseFrame");
}

std::string decreaseframe_test_fn(
    JNIMacroAssemblerX86_64Test::Base* assembler_test ATTRIBUTE_UNUSED,
    x86_64::X86_64JNIMacroAssembler* assembler) {
  assembler->DecreaseFrameSize(0U);
  assembler->DecreaseFrameSize(kStackAlignment);
  assembler->DecreaseFrameSize(10 * kStackAlignment);

  // Construct assembly text counterpart.
  std::ostringstream str;
  str << "addq $0, %rsp\n";
  str << "addq $" << kStackAlignment << ", %rsp\n";
  str << "addq $" << 10 * kStackAlignment << ", %rsp\n";

  return str.str();
}

TEST_F(JNIMacroAssemblerX86_64Test, DecreaseFrame) {
  DriverFn(&decreaseframe_test_fn, "DecreaseFrame");
}

}  // namespace art
