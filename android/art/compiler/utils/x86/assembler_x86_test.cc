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

#include "assembler_x86.h"

#include "base/arena_allocator.h"
#include "base/stl_util.h"
#include "utils/assembler_test.h"

namespace art {

TEST(AssemblerX86, CreateBuffer) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  AssemblerBuffer buffer(&allocator);
  AssemblerBuffer::EnsureCapacity ensured(&buffer);
  buffer.Emit<uint8_t>(0x42);
  ASSERT_EQ(static_cast<size_t>(1), buffer.Size());
  buffer.Emit<int32_t>(42);
  ASSERT_EQ(static_cast<size_t>(5), buffer.Size());
}

//
// Test fixture.
//

class AssemblerX86Test : public AssemblerTest<x86::X86Assembler,
                                              x86::Address,
                                              x86::Register,
                                              x86::XmmRegister,
                                              x86::Immediate> {
 public:
  typedef AssemblerTest<x86::X86Assembler,
                        x86::Address,
                        x86::Register,
                        x86::XmmRegister,
                        x86::Immediate> Base;

 protected:
  std::string GetArchitectureString() OVERRIDE {
    return "x86";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    return " --32";
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -mi386 --no-show-raw-insn";
  }

  void SetUpHelpers() OVERRIDE {
    if (addresses_singleton_.size() == 0) {
      // One addressing mode to test the repeat drivers.
      addresses_singleton_.push_back(x86::Address(x86::EAX, x86::EBX, x86::TIMES_1, 2));
    }

    if (addresses_.size() == 0) {
      // Several addressing modes.
      addresses_.push_back(x86::Address(x86::EDI, x86::EAX, x86::TIMES_1, 15));
      addresses_.push_back(x86::Address(x86::EDI, x86::EBX, x86::TIMES_2, 16));
      addresses_.push_back(x86::Address(x86::EDI, x86::ECX, x86::TIMES_4, 17));
      addresses_.push_back(x86::Address(x86::EDI, x86::EDX, x86::TIMES_8, 18));
      addresses_.push_back(x86::Address(x86::EAX, -1));
      addresses_.push_back(x86::Address(x86::EBX, 0));
      addresses_.push_back(x86::Address(x86::ESI, 1));
      addresses_.push_back(x86::Address(x86::EDI, 987654321));
      // Several addressing modes with the special ESP.
      addresses_.push_back(x86::Address(x86::ESP, x86::EAX, x86::TIMES_1, 15));
      addresses_.push_back(x86::Address(x86::ESP, x86::EBX, x86::TIMES_2, 16));
      addresses_.push_back(x86::Address(x86::ESP, x86::ECX, x86::TIMES_4, 17));
      addresses_.push_back(x86::Address(x86::ESP, x86::EDX, x86::TIMES_8, 18));
      addresses_.push_back(x86::Address(x86::ESP, -1));
      addresses_.push_back(x86::Address(x86::ESP, 0));
      addresses_.push_back(x86::Address(x86::ESP, 1));
      addresses_.push_back(x86::Address(x86::ESP, 987654321));
    }
    if (registers_.size() == 0) {
      registers_.insert(end(registers_),
                        {
                          new x86::Register(x86::EAX),
                          new x86::Register(x86::EBX),
                          new x86::Register(x86::ECX),
                          new x86::Register(x86::EDX),
                          new x86::Register(x86::EBP),
                          new x86::Register(x86::ESP),
                          new x86::Register(x86::ESI),
                          new x86::Register(x86::EDI)
                        });
    }

    if (fp_registers_.size() == 0) {
      fp_registers_.insert(end(fp_registers_),
                           {
                             new x86::XmmRegister(x86::XMM0),
                             new x86::XmmRegister(x86::XMM1),
                             new x86::XmmRegister(x86::XMM2),
                             new x86::XmmRegister(x86::XMM3),
                             new x86::XmmRegister(x86::XMM4),
                             new x86::XmmRegister(x86::XMM5),
                             new x86::XmmRegister(x86::XMM6),
                             new x86::XmmRegister(x86::XMM7)
                           });
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
    STLDeleteElements(&fp_registers_);
  }

  std::vector<x86::Address> GetAddresses() OVERRIDE {
    return addresses_;
  }

  std::vector<x86::Register*> GetRegisters() OVERRIDE {
    return registers_;
  }

  std::vector<x86::XmmRegister*> GetFPRegisters() OVERRIDE {
    return fp_registers_;
  }

  x86::Immediate CreateImmediate(int64_t imm_value) OVERRIDE {
    return x86::Immediate(imm_value);
  }

  std::vector<x86::Address> addresses_singleton_;

 private:
  std::vector<x86::Address> addresses_;
  std::vector<x86::Register*> registers_;
  std::vector<x86::XmmRegister*> fp_registers_;
};

//
// Test some repeat drivers used in the tests.
//

TEST_F(AssemblerX86Test, RepeatRR) {
  EXPECT_NE(RepeatRR(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%eax %eax\n%eax %ebx\n%eax %ecx\n%eax %edx\n%eax %ebp\n%eax %esp\n%eax %esi\n"
                  "%eax %edi\n%ebx %eax\n%ebx %ebx\n%ebx %ecx\n%ebx %edx\n%ebx %ebp\n%ebx %esp\n"),
            std::string::npos);
}

TEST_F(AssemblerX86Test, RepeatRI) {
  EXPECT_EQ("%eax $0\n%eax $-1\n%eax $18\n%ebx $0\n%ebx $-1\n%ebx $18\n%ecx $0\n%ecx $-1\n"
            "%ecx $18\n%edx $0\n%edx $-1\n%edx $18\n%ebp $0\n%ebp $-1\n%ebp $18\n%esp $0\n"
            "%esp $-1\n%esp $18\n%esi $0\n%esi $-1\n%esi $18\n%edi $0\n%edi $-1\n%edi $18\n",
            RepeatRI(/*f*/ nullptr, /*imm_bytes*/ 1U, "%{reg} ${imm}"));
}

TEST_F(AssemblerX86Test, RepeatFF) {
  EXPECT_NE(RepeatFF(/*f*/ nullptr, "%{reg1} %{reg2}")
            .find("%XMM0 %XMM0\n%XMM0 %XMM1\n%XMM0 %XMM2\n%XMM0 %XMM3\n%XMM0 %XMM4\n%XMM0 %XMM5\n"
                  "%XMM0 %XMM6\n%XMM0 %XMM7\n%XMM1 %XMM0\n%XMM1 %XMM1\n%XMM1 %XMM2\n%XMM1 %XMM3\n"),
            std::string::npos);
}

TEST_F(AssemblerX86Test, RepeatFFI) {
  EXPECT_NE(RepeatFFI(/*f*/ nullptr, /*imm_bytes*/ 1U, "%{reg1} %{reg2} ${imm}")
            .find("%XMM0 %XMM0 $0\n%XMM0 %XMM0 $-1\n%XMM0 %XMM0 $18\n"
                  "%XMM0 %XMM1 $0\n%XMM0 %XMM1 $-1\n%XMM0 %XMM1 $18\n"),
            std::string::npos);
}

TEST_F(AssemblerX86Test, RepeatA) {
  EXPECT_EQ("2(%eax,%ebx,1)\n", RepeatA(/*f*/ nullptr, addresses_singleton_, "{mem}"));
}

TEST_F(AssemblerX86Test, RepeatAI) {
  EXPECT_EQ("2(%eax,%ebx,1) $0\n2(%eax,%ebx,1) $-1\n2(%eax,%ebx,1) $18\n",
            RepeatAI(/*f*/ nullptr, /*imm_bytes*/ 1U, addresses_singleton_, "{mem} ${imm}"));
}

TEST_F(AssemblerX86Test, RepeatRA) {
  EXPECT_EQ("%eax 2(%eax,%ebx,1)\n%ebx 2(%eax,%ebx,1)\n%ecx 2(%eax,%ebx,1)\n"
            "%edx 2(%eax,%ebx,1)\n%ebp 2(%eax,%ebx,1)\n%esp 2(%eax,%ebx,1)\n"
            "%esi 2(%eax,%ebx,1)\n%edi 2(%eax,%ebx,1)\n",
            RepeatRA(/*f*/ nullptr, addresses_singleton_, "%{reg} {mem}"));
}

TEST_F(AssemblerX86Test, RepeatAR) {
  EXPECT_EQ("2(%eax,%ebx,1) %eax\n2(%eax,%ebx,1) %ebx\n2(%eax,%ebx,1) %ecx\n"
            "2(%eax,%ebx,1) %edx\n2(%eax,%ebx,1) %ebp\n2(%eax,%ebx,1) %esp\n"
            "2(%eax,%ebx,1) %esi\n2(%eax,%ebx,1) %edi\n",
            RepeatAR(/*f*/ nullptr, addresses_singleton_, "{mem} %{reg}"));
}

TEST_F(AssemblerX86Test, RepeatFA) {
  EXPECT_EQ("%XMM0 2(%eax,%ebx,1)\n%XMM1 2(%eax,%ebx,1)\n%XMM2 2(%eax,%ebx,1)\n"
            "%XMM3 2(%eax,%ebx,1)\n%XMM4 2(%eax,%ebx,1)\n%XMM5 2(%eax,%ebx,1)\n"
            "%XMM6 2(%eax,%ebx,1)\n%XMM7 2(%eax,%ebx,1)\n",
            RepeatFA(/*f*/ nullptr, addresses_singleton_, "%{reg} {mem}"));
}

TEST_F(AssemblerX86Test, RepeatAF) {
  EXPECT_EQ("2(%eax,%ebx,1) %XMM0\n2(%eax,%ebx,1) %XMM1\n2(%eax,%ebx,1) %XMM2\n"
            "2(%eax,%ebx,1) %XMM3\n2(%eax,%ebx,1) %XMM4\n2(%eax,%ebx,1) %XMM5\n"
            "2(%eax,%ebx,1) %XMM6\n2(%eax,%ebx,1) %XMM7\n",
            RepeatAF(/*f*/ nullptr, addresses_singleton_, "{mem} %{reg}"));
}

//
// Actual x86 instruction assembler tests.
//

TEST_F(AssemblerX86Test, PoplAllAddresses) {
  // Make sure all addressing modes combinations are tested at least once.
  std::vector<x86::Address> all_addresses;
  for (x86::Register* base : GetRegisters()) {
    // Base only.
    all_addresses.push_back(x86::Address(*base, -1));
    all_addresses.push_back(x86::Address(*base, 0));
    all_addresses.push_back(x86::Address(*base, 1));
    all_addresses.push_back(x86::Address(*base, 123456789));
    for (x86::Register* index : GetRegisters()) {
      if (*index == x86::ESP) {
        // Index cannot be ESP.
        continue;
      } else if (*base == *index) {
       // Index only.
       all_addresses.push_back(x86::Address(*index, x86::TIMES_1, -1));
       all_addresses.push_back(x86::Address(*index, x86::TIMES_2, 0));
       all_addresses.push_back(x86::Address(*index, x86::TIMES_4, 1));
       all_addresses.push_back(x86::Address(*index, x86::TIMES_8, 123456789));
      }
      // Base and index.
      all_addresses.push_back(x86::Address(*base, *index, x86::TIMES_1, -1));
      all_addresses.push_back(x86::Address(*base, *index, x86::TIMES_2, 0));
      all_addresses.push_back(x86::Address(*base, *index, x86::TIMES_4, 1));
      all_addresses.push_back(x86::Address(*base, *index, x86::TIMES_8, 123456789));
    }
  }
  DriverStr(RepeatA(&x86::X86Assembler::popl, all_addresses, "popl {mem}"), "popq");
}

TEST_F(AssemblerX86Test, Movl) {
  DriverStr(RepeatRR(&x86::X86Assembler::movl, "movl %{reg2}, %{reg1}"), "movl");
}

TEST_F(AssemblerX86Test, MovlLoad) {
  DriverStr(RepeatRA(&x86::X86Assembler::movl, "movl {mem}, %{reg}"), "movl-load");
}

TEST_F(AssemblerX86Test, Addw) {
  DriverStr(RepeatAI(&x86::X86Assembler::addw, /*imm_bytes*/ 2U, "addw ${imm}, {mem}"), "addw");
}

TEST_F(AssemblerX86Test, MovlStore) {
  DriverStr(RepeatAR(&x86::X86Assembler::movl, "movl %{reg}, {mem}"), "movl-store");
}

TEST_F(AssemblerX86Test, Movntl) {
  DriverStr(RepeatAR(&x86::X86Assembler::movntl, "movntil %{reg}, {mem}"), "movntl");
}

TEST_F(AssemblerX86Test, LoadLongConstant) {
  GetAssembler()->LoadLongConstant(x86::XMM0, 51);
  const char* expected =
      "push $0x0\n"
      "push $0x33\n"
      "movsd 0(%esp), %xmm0\n"
      "add $8, %esp\n";
  DriverStr(expected, "LoadLongConstant");
}

TEST_F(AssemblerX86Test, LockCmpxchgl) {
  DriverStr(RepeatAR(&x86::X86Assembler::LockCmpxchgl,
                     "lock cmpxchgl %{reg}, {mem}"), "lock_cmpxchgl");
}

TEST_F(AssemblerX86Test, LockCmpxchg8b) {
  DriverStr(RepeatA(&x86::X86Assembler::LockCmpxchg8b,
                    "lock cmpxchg8b {mem}"), "lock_cmpxchg8b");
}

TEST_F(AssemblerX86Test, FPUIntegerLoadS) {
  DriverStr(RepeatA(&x86::X86Assembler::filds, "fildl {mem}"), "fildd");
}

TEST_F(AssemblerX86Test, FPUIntegerLoadL) {
  DriverStr(RepeatA(&x86::X86Assembler::fildl, "fildll {mem}"), "fildl");
}

TEST_F(AssemblerX86Test, FPUIntegerStoreS) {
  DriverStr(RepeatA(&x86::X86Assembler::fistps, "fistpl {mem}"), "fistps");
}

TEST_F(AssemblerX86Test, FPUIntegerStoreL) {
  DriverStr(RepeatA(&x86::X86Assembler::fistpl, "fistpll {mem}"), "fistpl");
}

TEST_F(AssemblerX86Test, Repnescasb) {
  GetAssembler()->repne_scasb();
  const char* expected = "repne scasb\n";
  DriverStr(expected, "Repnescasb");
}

TEST_F(AssemblerX86Test, Repnescasw) {
  GetAssembler()->repne_scasw();
  const char* expected = "repne scasw\n";
  DriverStr(expected, "Repnescasw");
}

TEST_F(AssemblerX86Test, Repecmpsb) {
  GetAssembler()->repe_cmpsb();
  const char* expected = "repe cmpsb\n";
  DriverStr(expected, "Repecmpsb");
}

TEST_F(AssemblerX86Test, Repecmpsw) {
  GetAssembler()->repe_cmpsw();
  const char* expected = "repe cmpsw\n";
  DriverStr(expected, "Repecmpsw");
}

TEST_F(AssemblerX86Test, Repecmpsl) {
  GetAssembler()->repe_cmpsl();
  const char* expected = "repe cmpsl\n";
  DriverStr(expected, "Repecmpsl");
}

TEST_F(AssemblerX86Test, RepMovsb) {
  GetAssembler()->rep_movsb();
  const char* expected = "rep movsb\n";
  DriverStr(expected, "rep_movsb");
}

TEST_F(AssemblerX86Test, RepMovsw) {
  GetAssembler()->rep_movsw();
  const char* expected = "rep movsw\n";
  DriverStr(expected, "rep_movsw");
}

TEST_F(AssemblerX86Test, Bsfl) {
  DriverStr(RepeatRR(&x86::X86Assembler::bsfl, "bsfl %{reg2}, %{reg1}"), "bsfl");
}

TEST_F(AssemblerX86Test, BsflAddress) {
  DriverStr(RepeatRA(&x86::X86Assembler::bsfl, "bsfl {mem}, %{reg}"), "bsfl_address");
}

TEST_F(AssemblerX86Test, Bsrl) {
  DriverStr(RepeatRR(&x86::X86Assembler::bsrl, "bsrl %{reg2}, %{reg1}"), "bsrl");
}

TEST_F(AssemblerX86Test, BsrlAddress) {
  DriverStr(RepeatRA(&x86::X86Assembler::bsrl, "bsrl {mem}, %{reg}"), "bsrl_address");
}

TEST_F(AssemblerX86Test, Popcntl) {
  DriverStr(RepeatRR(&x86::X86Assembler::popcntl, "popcntl %{reg2}, %{reg1}"), "popcntl");
}

TEST_F(AssemblerX86Test, PopcntlAddress) {
  DriverStr(RepeatRA(&x86::X86Assembler::popcntl, "popcntl {mem}, %{reg}"), "popcntl_address");
}

// Rorl only allows CL as the shift count.
std::string rorl_fn(AssemblerX86Test::Base* assembler_test, x86::X86Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86::Register*> registers = assembler_test->GetRegisters();
  x86::Register shifter(x86::ECX);
  for (auto reg : registers) {
    assembler->rorl(*reg, shifter);
    str << "rorl %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86Test, RorlReg) {
  DriverFn(&rorl_fn, "rorl");
}

TEST_F(AssemblerX86Test, RorlImm) {
  DriverStr(RepeatRI(&x86::X86Assembler::rorl, /*imm_bytes*/ 1U, "rorl ${imm}, %{reg}"), "rorli");
}

// Roll only allows CL as the shift count.
std::string roll_fn(AssemblerX86Test::Base* assembler_test, x86::X86Assembler* assembler) {
  std::ostringstream str;
  std::vector<x86::Register*> registers = assembler_test->GetRegisters();
  x86::Register shifter(x86::ECX);
  for (auto reg : registers) {
    assembler->roll(*reg, shifter);
    str << "roll %cl, %" << assembler_test->GetRegisterName(*reg) << "\n";
  }
  return str.str();
}

TEST_F(AssemblerX86Test, RollReg) {
  DriverFn(&roll_fn, "roll");
}

TEST_F(AssemblerX86Test, RollImm) {
  DriverStr(RepeatRI(&x86::X86Assembler::roll, /*imm_bytes*/ 1U, "roll ${imm}, %{reg}"), "rolli");
}

TEST_F(AssemblerX86Test, Cvtdq2ps) {
  DriverStr(RepeatFF(&x86::X86Assembler::cvtdq2ps, "cvtdq2ps %{reg2}, %{reg1}"), "cvtdq2ps");
}

TEST_F(AssemblerX86Test, Cvtdq2pd) {
  DriverStr(RepeatFF(&x86::X86Assembler::cvtdq2pd, "cvtdq2pd %{reg2}, %{reg1}"), "cvtdq2pd");
}

TEST_F(AssemblerX86Test, ComissAddr) {
  DriverStr(RepeatFA(&x86::X86Assembler::comiss, "comiss {mem}, %{reg}"), "comiss");
}

TEST_F(AssemblerX86Test, UComissAddr) {
  DriverStr(RepeatFA(&x86::X86Assembler::ucomiss, "ucomiss {mem}, %{reg}"), "ucomiss");
}

TEST_F(AssemblerX86Test, ComisdAddr) {
  DriverStr(RepeatFA(&x86::X86Assembler::comisd, "comisd {mem}, %{reg}"), "comisd");
}

TEST_F(AssemblerX86Test, UComisdAddr) {
  DriverStr(RepeatFA(&x86::X86Assembler::ucomisd, "ucomisd {mem}, %{reg}"), "ucomisd");
}

TEST_F(AssemblerX86Test, RoundSS) {
  DriverStr(RepeatFFI(&x86::X86Assembler::roundss, /*imm_bytes*/ 1U,
                      "roundss ${imm}, %{reg2}, %{reg1}"), "roundss");
}

TEST_F(AssemblerX86Test, RoundSD) {
  DriverStr(RepeatFFI(&x86::X86Assembler::roundsd, /*imm_bytes*/ 1U,
                      "roundsd ${imm}, %{reg2}, %{reg1}"), "roundsd");
}

TEST_F(AssemblerX86Test, CmovlAddress) {
  GetAssembler()->cmovl(x86::kEqual, x86::Register(x86::EAX), x86::Address(
      x86::Register(x86::EDI), x86::Register(x86::EBX), x86::TIMES_4, 12));
  GetAssembler()->cmovl(x86::kNotEqual, x86::Register(x86::EDI), x86::Address(
      x86::Register(x86::ESI), x86::Register(x86::EBX), x86::TIMES_4, 12));
  GetAssembler()->cmovl(x86::kEqual, x86::Register(x86::EDI), x86::Address(
      x86::Register(x86::EDI), x86::Register(x86::EAX), x86::TIMES_4, 12));
  const char* expected =
    "cmovzl 0xc(%EDI,%EBX,4), %eax\n"
    "cmovnzl 0xc(%ESI,%EBX,4), %edi\n"
    "cmovzl 0xc(%EDI,%EAX,4), %edi\n";
  DriverStr(expected, "cmovl_address");
}

TEST_F(AssemblerX86Test, TestbAddressImmediate) {
  DriverStr(RepeatAI(&x86::X86Assembler::testb, /*imm_bytes*/ 1U, "testb ${imm}, {mem}"), "testb");
}

TEST_F(AssemblerX86Test, TestlAddressImmediate) {
  DriverStr(RepeatAI(&x86::X86Assembler::testl, /*imm_bytes*/ 4U, "testl ${imm}, {mem}"), "testl");
}

TEST_F(AssemblerX86Test, Movaps) {
  DriverStr(RepeatFF(&x86::X86Assembler::movaps, "movaps %{reg2}, %{reg1}"), "movaps");
}

TEST_F(AssemblerX86Test, MovapsLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movaps, "movaps {mem}, %{reg}"), "movaps_load");
}

TEST_F(AssemblerX86Test, MovapsStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movaps, "movaps %{reg}, {mem}"), "movaps_store");
}

TEST_F(AssemblerX86Test, MovupsLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movups, "movups {mem}, %{reg}"), "movups_load");
}

TEST_F(AssemblerX86Test, MovupsStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movups, "movups %{reg}, {mem}"), "movups_store");
}

TEST_F(AssemblerX86Test, Movapd) {
  DriverStr(RepeatFF(&x86::X86Assembler::movapd, "movapd %{reg2}, %{reg1}"), "movapd");
}

TEST_F(AssemblerX86Test, MovapdLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movapd, "movapd {mem}, %{reg}"), "movapd_load");
}

TEST_F(AssemblerX86Test, MovapdStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movapd, "movapd %{reg}, {mem}"), "movapd_store");
}

TEST_F(AssemblerX86Test, MovupdLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movupd, "movupd {mem}, %{reg}"), "movupd_load");
}

TEST_F(AssemblerX86Test, MovupdStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movupd, "movupd %{reg}, {mem}"), "movupd_store");
}

TEST_F(AssemblerX86Test, Movdqa) {
  DriverStr(RepeatFF(&x86::X86Assembler::movdqa, "movdqa %{reg2}, %{reg1}"), "movdqa");
}

TEST_F(AssemblerX86Test, MovdqaLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movdqa, "movdqa {mem}, %{reg}"), "movdqa_load");
}

TEST_F(AssemblerX86Test, MovdqaStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movdqa, "movdqa %{reg}, {mem}"), "movdqa_store");
}

TEST_F(AssemblerX86Test, MovdquLoad) {
  DriverStr(RepeatFA(&x86::X86Assembler::movdqu, "movdqu {mem}, %{reg}"), "movdqu_load");
}

TEST_F(AssemblerX86Test, MovdquStore) {
  DriverStr(RepeatAF(&x86::X86Assembler::movdqu, "movdqu %{reg}, {mem}"), "movdqu_store");
}

TEST_F(AssemblerX86Test, AddPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::addps, "addps %{reg2}, %{reg1}"), "addps");
}

TEST_F(AssemblerX86Test, AddPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::addpd, "addpd %{reg2}, %{reg1}"), "addpd");
}

TEST_F(AssemblerX86Test, SubPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::subps, "subps %{reg2}, %{reg1}"), "subps");
}

TEST_F(AssemblerX86Test, SubPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::subpd, "subpd %{reg2}, %{reg1}"), "subpd");
}

TEST_F(AssemblerX86Test, MulPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::mulps, "mulps %{reg2}, %{reg1}"), "mulps");
}

TEST_F(AssemblerX86Test, MulPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::mulpd, "mulpd %{reg2}, %{reg1}"), "mulpd");
}

TEST_F(AssemblerX86Test, DivPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::divps, "divps %{reg2}, %{reg1}"), "divps");
}

TEST_F(AssemblerX86Test, DivPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::divpd, "divpd %{reg2}, %{reg1}"), "divpd");
}

TEST_F(AssemblerX86Test, PAddB) {
  DriverStr(RepeatFF(&x86::X86Assembler::paddb, "paddb %{reg2}, %{reg1}"), "paddb");
}

TEST_F(AssemblerX86Test, PSubB) {
  DriverStr(RepeatFF(&x86::X86Assembler::psubb, "psubb %{reg2}, %{reg1}"), "psubb");
}

TEST_F(AssemblerX86Test, PAddW) {
  DriverStr(RepeatFF(&x86::X86Assembler::paddw, "paddw %{reg2}, %{reg1}"), "paddw");
}

TEST_F(AssemblerX86Test, PSubW) {
  DriverStr(RepeatFF(&x86::X86Assembler::psubw, "psubw %{reg2}, %{reg1}"), "psubw");
}

TEST_F(AssemblerX86Test, PMullW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmullw, "pmullw %{reg2}, %{reg1}"), "pmullw");
}

TEST_F(AssemblerX86Test, PAddD) {
  DriverStr(RepeatFF(&x86::X86Assembler::paddd, "paddd %{reg2}, %{reg1}"), "paddd");
}

TEST_F(AssemblerX86Test, PSubD) {
  DriverStr(RepeatFF(&x86::X86Assembler::psubd, "psubd %{reg2}, %{reg1}"), "psubd");
}

TEST_F(AssemblerX86Test, PMullD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmulld, "pmulld %{reg2}, %{reg1}"), "pmulld");
}

TEST_F(AssemblerX86Test, PAddQ) {
  DriverStr(RepeatFF(&x86::X86Assembler::paddq, "paddq %{reg2}, %{reg1}"), "paddq");
}

TEST_F(AssemblerX86Test, PSubQ) {
  DriverStr(RepeatFF(&x86::X86Assembler::psubq, "psubq %{reg2}, %{reg1}"), "psubq");
}

TEST_F(AssemblerX86Test, XorPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::xorpd, "xorpd %{reg2}, %{reg1}"), "xorpd");
}

TEST_F(AssemblerX86Test, XorPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::xorps, "xorps %{reg2}, %{reg1}"), "xorps");
}

TEST_F(AssemblerX86Test, PXor) {
  DriverStr(RepeatFF(&x86::X86Assembler::pxor, "pxor %{reg2}, %{reg1}"), "pxor");
}

TEST_F(AssemblerX86Test, AndPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::andpd, "andpd %{reg2}, %{reg1}"), "andpd");
}

TEST_F(AssemblerX86Test, AndPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::andps, "andps %{reg2}, %{reg1}"), "andps");
}

TEST_F(AssemblerX86Test, PAnd) {
  DriverStr(RepeatFF(&x86::X86Assembler::pand, "pand %{reg2}, %{reg1}"), "pand");
}

TEST_F(AssemblerX86Test, AndnPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::andnpd, "andnpd %{reg2}, %{reg1}"), "andnpd");
}

TEST_F(AssemblerX86Test, AndnPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::andnps, "andnps %{reg2}, %{reg1}"), "andnps");
}

TEST_F(AssemblerX86Test, PAndn) {
  DriverStr(RepeatFF(&x86::X86Assembler::pandn, "pandn %{reg2}, %{reg1}"), "pandn");
}

TEST_F(AssemblerX86Test, OrPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::orpd, "orpd %{reg2}, %{reg1}"), "orpd");
}

TEST_F(AssemblerX86Test, OrPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::orps, "orps %{reg2}, %{reg1}"), "orps");
}

TEST_F(AssemblerX86Test, POr) {
  DriverStr(RepeatFF(&x86::X86Assembler::por, "por %{reg2}, %{reg1}"), "por");
}

TEST_F(AssemblerX86Test, PAvgB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pavgb, "pavgb %{reg2}, %{reg1}"), "pavgb");
}

TEST_F(AssemblerX86Test, PAvgW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pavgw, "pavgw %{reg2}, %{reg1}"), "pavgw");
}

TEST_F(AssemblerX86Test, PSadBW) {
  DriverStr(RepeatFF(&x86::X86Assembler::psadbw, "psadbw %{reg2}, %{reg1}"), "psadbw");
}

TEST_F(AssemblerX86Test, PMAddWD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaddwd, "pmaddwd %{reg2}, %{reg1}"), "pmaddwd");
}

TEST_F(AssemblerX86Test, PHAddW) {
  DriverStr(RepeatFF(&x86::X86Assembler::phaddw, "phaddw %{reg2}, %{reg1}"), "phaddw");
}

TEST_F(AssemblerX86Test, PHAddD) {
  DriverStr(RepeatFF(&x86::X86Assembler::phaddd, "phaddd %{reg2}, %{reg1}"), "phaddd");
}

TEST_F(AssemblerX86Test, HAddPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::haddps, "haddps %{reg2}, %{reg1}"), "haddps");
}

TEST_F(AssemblerX86Test, HAddPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::haddpd, "haddpd %{reg2}, %{reg1}"), "haddpd");
}

TEST_F(AssemblerX86Test, PHSubW) {
  DriverStr(RepeatFF(&x86::X86Assembler::phsubw, "phsubw %{reg2}, %{reg1}"), "phsubw");
}

TEST_F(AssemblerX86Test, PHSubD) {
  DriverStr(RepeatFF(&x86::X86Assembler::phsubd, "phsubd %{reg2}, %{reg1}"), "phsubd");
}

TEST_F(AssemblerX86Test, HSubPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::hsubps, "hsubps %{reg2}, %{reg1}"), "hsubps");
}

TEST_F(AssemblerX86Test, HSubPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::hsubpd, "hsubpd %{reg2}, %{reg1}"), "hsubpd");
}

TEST_F(AssemblerX86Test, PMinSB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminsb, "pminsb %{reg2}, %{reg1}"), "pminsb");
}

TEST_F(AssemblerX86Test, PMaxSB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxsb, "pmaxsb %{reg2}, %{reg1}"), "pmaxsb");
}

TEST_F(AssemblerX86Test, PMinSW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminsw, "pminsw %{reg2}, %{reg1}"), "pminsw");
}

TEST_F(AssemblerX86Test, PMaxSW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxsw, "pmaxsw %{reg2}, %{reg1}"), "pmaxsw");
}

TEST_F(AssemblerX86Test, PMinSD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminsd, "pminsd %{reg2}, %{reg1}"), "pminsd");
}

TEST_F(AssemblerX86Test, PMaxSD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxsd, "pmaxsd %{reg2}, %{reg1}"), "pmaxsd");
}

TEST_F(AssemblerX86Test, PMinUB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminub, "pminub %{reg2}, %{reg1}"), "pminub");
}

TEST_F(AssemblerX86Test, PMaxUB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxub, "pmaxub %{reg2}, %{reg1}"), "pmaxub");
}

TEST_F(AssemblerX86Test, PMinUW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminuw, "pminuw %{reg2}, %{reg1}"), "pminuw");
}

TEST_F(AssemblerX86Test, PMaxUW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxuw, "pmaxuw %{reg2}, %{reg1}"), "pmaxuw");
}

TEST_F(AssemblerX86Test, PMinUD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pminud, "pminud %{reg2}, %{reg1}"), "pminud");
}

TEST_F(AssemblerX86Test, PMaxUD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pmaxud, "pmaxud %{reg2}, %{reg1}"), "pmaxud");
}

TEST_F(AssemblerX86Test, MinPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::minps, "minps %{reg2}, %{reg1}"), "minps");
}

TEST_F(AssemblerX86Test, MaxPS) {
  DriverStr(RepeatFF(&x86::X86Assembler::maxps, "maxps %{reg2}, %{reg1}"), "maxps");
}

TEST_F(AssemblerX86Test, MinPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::minpd, "minpd %{reg2}, %{reg1}"), "minpd");
}

TEST_F(AssemblerX86Test, MaxPD) {
  DriverStr(RepeatFF(&x86::X86Assembler::maxpd, "maxpd %{reg2}, %{reg1}"), "maxpd");
}

TEST_F(AssemblerX86Test, PCmpeqB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpeqb, "pcmpeqb %{reg2}, %{reg1}"), "cmpeqb");
}

TEST_F(AssemblerX86Test, PCmpeqW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpeqw, "pcmpeqw %{reg2}, %{reg1}"), "cmpeqw");
}

TEST_F(AssemblerX86Test, PCmpeqD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpeqd, "pcmpeqd %{reg2}, %{reg1}"), "cmpeqd");
}

TEST_F(AssemblerX86Test, PCmpeqQ) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpeqq, "pcmpeqq %{reg2}, %{reg1}"), "cmpeqq");
}

TEST_F(AssemblerX86Test, PCmpgtB) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpgtb, "pcmpgtb %{reg2}, %{reg1}"), "cmpgtb");
}

TEST_F(AssemblerX86Test, PCmpgtW) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpgtw, "pcmpgtw %{reg2}, %{reg1}"), "cmpgtw");
}

TEST_F(AssemblerX86Test, PCmpgtD) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpgtd, "pcmpgtd %{reg2}, %{reg1}"), "cmpgtd");
}

TEST_F(AssemblerX86Test, PCmpgtQ) {
  DriverStr(RepeatFF(&x86::X86Assembler::pcmpgtq, "pcmpgtq %{reg2}, %{reg1}"), "cmpgtq");
}

TEST_F(AssemblerX86Test, ShufPS) {
  DriverStr(RepeatFFI(&x86::X86Assembler::shufps, 1, "shufps ${imm}, %{reg2}, %{reg1}"), "shufps");
}

TEST_F(AssemblerX86Test, ShufPD) {
  DriverStr(RepeatFFI(&x86::X86Assembler::shufpd, 1, "shufpd ${imm}, %{reg2}, %{reg1}"), "shufpd");
}

TEST_F(AssemblerX86Test, PShufD) {
  DriverStr(RepeatFFI(&x86::X86Assembler::pshufd, 1, "pshufd ${imm}, %{reg2}, %{reg1}"), "pshufd");
}

TEST_F(AssemblerX86Test, Punpcklbw) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpcklbw, "punpcklbw %{reg2}, %{reg1}"), "punpcklbw");
}

TEST_F(AssemblerX86Test, Punpcklwd) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpcklwd, "punpcklwd %{reg2}, %{reg1}"), "punpcklwd");
}

TEST_F(AssemblerX86Test, Punpckldq) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpckldq, "punpckldq %{reg2}, %{reg1}"), "punpckldq");
}

TEST_F(AssemblerX86Test, Punpcklqdq) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpcklqdq, "punpcklqdq %{reg2}, %{reg1}"), "punpcklqdq");
}

TEST_F(AssemblerX86Test, Punpckhbw) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpckhbw, "punpckhbw %{reg2}, %{reg1}"), "punpckhbw");
}

TEST_F(AssemblerX86Test, Punpckhwd) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpckhwd, "punpckhwd %{reg2}, %{reg1}"), "punpckhwd");
}

TEST_F(AssemblerX86Test, Punpckhdq) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpckhdq, "punpckhdq %{reg2}, %{reg1}"), "punpckhdq");
}

TEST_F(AssemblerX86Test, Punpckhqdq) {
  DriverStr(RepeatFF(&x86::X86Assembler::punpckhqdq, "punpckhqdq %{reg2}, %{reg1}"), "punpckhqdq");
}

TEST_F(AssemblerX86Test, psllw) {
  GetAssembler()->psllw(x86::XMM0, CreateImmediate(16));
  DriverStr("psllw $0x10, %xmm0\n", "psllwi");
}

TEST_F(AssemblerX86Test, pslld) {
  GetAssembler()->pslld(x86::XMM0, CreateImmediate(16));
  DriverStr("pslld $0x10, %xmm0\n", "pslldi");
}

TEST_F(AssemblerX86Test, psllq) {
  GetAssembler()->psllq(x86::XMM0, CreateImmediate(16));
  DriverStr("psllq $0x10, %xmm0\n", "psllqi");
}

TEST_F(AssemblerX86Test, psraw) {
  GetAssembler()->psraw(x86::XMM0, CreateImmediate(16));
  DriverStr("psraw $0x10, %xmm0\n", "psrawi");
}

TEST_F(AssemblerX86Test, psrad) {
  GetAssembler()->psrad(x86::XMM0, CreateImmediate(16));
  DriverStr("psrad $0x10, %xmm0\n", "psradi");
}

TEST_F(AssemblerX86Test, psrlw) {
  GetAssembler()->psrlw(x86::XMM0, CreateImmediate(16));
  DriverStr("psrlw $0x10, %xmm0\n", "psrlwi");
}

TEST_F(AssemblerX86Test, psrld) {
  GetAssembler()->psrld(x86::XMM0, CreateImmediate(16));
  DriverStr("psrld $0x10, %xmm0\n", "psrldi");
}

TEST_F(AssemblerX86Test, psrlq) {
  GetAssembler()->psrlq(x86::XMM0, CreateImmediate(16));
  DriverStr("psrlq $0x10, %xmm0\n", "psrlqi");
}

TEST_F(AssemblerX86Test, psrldq) {
  GetAssembler()->psrldq(x86::XMM0, CreateImmediate(16));
  DriverStr("psrldq $0x10, %xmm0\n", "psrldqi");
}

TEST_F(AssemblerX86Test, Jecxz) {
  x86::NearLabel target;
  GetAssembler()->jecxz(&target);
  GetAssembler()->addl(x86::EDI, x86::Address(x86::ESP, 4));
  GetAssembler()->Bind(&target);
  const char* expected =
    "jecxz 1f\n"
    "addl 4(%ESP),%EDI\n"
    "1:\n";
  DriverStr(expected, "jecxz");
}

TEST_F(AssemblerX86Test, NearLabel) {
  // Test both forward and backward branches.
  x86::NearLabel start, target;
  GetAssembler()->Bind(&start);
  GetAssembler()->j(x86::kEqual, &target);
  GetAssembler()->jmp(&target);
  GetAssembler()->jecxz(&target);
  GetAssembler()->addl(x86::EDI, x86::Address(x86::ESP, 4));
  GetAssembler()->Bind(&target);
  GetAssembler()->j(x86::kNotEqual, &start);
  GetAssembler()->jmp(&start);
  const char* expected =
    "1: je 2f\n"
    "jmp 2f\n"
    "jecxz 2f\n"
    "addl 4(%ESP),%EDI\n"
    "2: jne 1b\n"
    "jmp 1b\n";
  DriverStr(expected, "near_label");
}

TEST_F(AssemblerX86Test, Cmpb) {
  DriverStr(RepeatAI(&x86::X86Assembler::cmpb,
                     /*imm_bytes*/ 1U,
                     "cmpb ${imm}, {mem}"), "cmpb");
}

TEST_F(AssemblerX86Test, Cmpw) {
  DriverStr(RepeatAI(&x86::X86Assembler::cmpw, /*imm_bytes*/ 2U, "cmpw ${imm}, {mem}"), "cmpw");
}

}  // namespace art
