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

#include "managed_register_x86_64.h"
#include "globals.h"
#include "gtest/gtest.h"

namespace art {
namespace x86_64 {

TEST(X86_64ManagedRegister, NoRegister) {
  X86_64ManagedRegister reg = ManagedRegister::NoRegister().AsX86();
  EXPECT_TRUE(reg.IsNoRegister());
  EXPECT_TRUE(!reg.Overlaps(reg));
}

TEST(X86_64ManagedRegister, CpuRegister) {
  X86_64ManagedRegister reg = X86_64ManagedRegister::FromCpuRegister(RAX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(RAX, reg.AsCpuRegister());

  reg = X86_64ManagedRegister::FromCpuRegister(RBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(RBX, reg.AsCpuRegister());

  reg = X86_64ManagedRegister::FromCpuRegister(RCX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(RCX, reg.AsCpuRegister());

  reg = X86_64ManagedRegister::FromCpuRegister(RDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(RDI, reg.AsCpuRegister());
}

TEST(X86_64ManagedRegister, XmmRegister) {
  X86_64ManagedRegister reg = X86_64ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM0, reg.AsXmmRegister());

  reg = X86_64ManagedRegister::FromXmmRegister(XMM1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM1, reg.AsXmmRegister());

  reg = X86_64ManagedRegister::FromXmmRegister(XMM7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM7, reg.AsXmmRegister());
}

TEST(X86_64ManagedRegister, X87Register) {
  X86_64ManagedRegister reg = X86_64ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST0, reg.AsX87Register());

  reg = X86_64ManagedRegister::FromX87Register(ST1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST1, reg.AsX87Register());

  reg = X86_64ManagedRegister::FromX87Register(ST7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST7, reg.AsX87Register());
}

TEST(X86_64ManagedRegister, RegisterPair) {
  X86_64ManagedRegister reg = X86_64ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RAX, reg.AsRegisterPairLow());
  EXPECT_EQ(RDX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EAX_ECX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RAX, reg.AsRegisterPairLow());
  EXPECT_EQ(RCX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EAX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RAX, reg.AsRegisterPairLow());
  EXPECT_EQ(RBX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EAX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RAX, reg.AsRegisterPairLow());
  EXPECT_EQ(RDI, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EDX_ECX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RDX, reg.AsRegisterPairLow());
  EXPECT_EQ(RCX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EDX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RDX, reg.AsRegisterPairLow());
  EXPECT_EQ(RBX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EDX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RDX, reg.AsRegisterPairLow());
  EXPECT_EQ(RDI, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(ECX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RCX, reg.AsRegisterPairLow());
  EXPECT_EQ(RBX, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(ECX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RCX, reg.AsRegisterPairLow());
  EXPECT_EQ(RDI, reg.AsRegisterPairHigh());

  reg = X86_64ManagedRegister::FromRegisterPair(EBX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(RBX, reg.AsRegisterPairLow());
  EXPECT_EQ(RDI, reg.AsRegisterPairHigh());
}

TEST(X86_64ManagedRegister, Equals) {
  X86_64ManagedRegister reg_eax = X86_64ManagedRegister::FromCpuRegister(RAX);
  EXPECT_TRUE(reg_eax.Equals(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_eax.Equals(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86_64ManagedRegister reg_xmm0 = X86_64ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(reg_xmm0.Equals(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86_64ManagedRegister reg_st0 = X86_64ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(reg_st0.Equals(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_st0.Equals(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86_64ManagedRegister reg_pair = X86_64ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg_pair.Equals(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_pair.Equals(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));
}

TEST(X86_64ManagedRegister, Overlaps) {
  X86_64ManagedRegister reg = X86_64ManagedRegister::FromCpuRegister(RAX);
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromCpuRegister(RDX);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromCpuRegister(RDI);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromCpuRegister(RBX);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EDX_ECX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86_64ManagedRegister::FromRegisterPair(EBX_EDI);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EDX_EBX)));

  reg = X86_64ManagedRegister::FromRegisterPair(EDX_ECX);
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RAX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RBX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromCpuRegister(RDI)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EBX_EDI)));
  EXPECT_TRUE(reg.Overlaps(X86_64ManagedRegister::FromRegisterPair(EDX_EBX)));
}

}  // namespace x86_64
}  // namespace art
