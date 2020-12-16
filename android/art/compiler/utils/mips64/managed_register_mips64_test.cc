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

#include "managed_register_mips64.h"
#include "globals.h"
#include "gtest/gtest.h"

namespace art {
namespace mips64 {

TEST(Mips64ManagedRegister, NoRegister) {
  Mips64ManagedRegister reg = ManagedRegister::NoRegister().AsMips64();
  EXPECT_TRUE(reg.IsNoRegister());
  EXPECT_FALSE(reg.Overlaps(reg));
}

TEST(Mips64ManagedRegister, GpuRegister) {
  Mips64ManagedRegister reg = Mips64ManagedRegister::FromGpuRegister(ZERO);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(ZERO, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(AT);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(AT, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(V0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(V0, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(A0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(A0, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(A7);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(A7, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(T0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(T0, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(T3);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(T3, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(S0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(S0, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(GP);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(GP, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(SP);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(SP, reg.AsGpuRegister());

  reg = Mips64ManagedRegister::FromGpuRegister(RA);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_TRUE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_EQ(RA, reg.AsGpuRegister());
}

TEST(Mips64ManagedRegister, FpuRegister) {
  Mips64ManagedRegister reg = Mips64ManagedRegister::FromFpuRegister(F0);
  Mips64ManagedRegister vreg = Mips64ManagedRegister::FromVectorRegister(W0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F0, reg.AsFpuRegister());
  EXPECT_EQ(W0, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));

  reg = Mips64ManagedRegister::FromFpuRegister(F1);
  vreg = Mips64ManagedRegister::FromVectorRegister(W1);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F1, reg.AsFpuRegister());
  EXPECT_EQ(W1, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromFpuRegister(F1)));

  reg = Mips64ManagedRegister::FromFpuRegister(F20);
  vreg = Mips64ManagedRegister::FromVectorRegister(W20);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F20, reg.AsFpuRegister());
  EXPECT_EQ(W20, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromFpuRegister(F20)));

  reg = Mips64ManagedRegister::FromFpuRegister(F31);
  vreg = Mips64ManagedRegister::FromVectorRegister(W31);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_TRUE(reg.IsFpuRegister());
  EXPECT_FALSE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(vreg));
  EXPECT_EQ(F31, reg.AsFpuRegister());
  EXPECT_EQ(W31, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromFpuRegister(F31)));
}

TEST(Mips64ManagedRegister, VectorRegister) {
  Mips64ManagedRegister reg = Mips64ManagedRegister::FromVectorRegister(W0);
  Mips64ManagedRegister freg = Mips64ManagedRegister::FromFpuRegister(F0);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_TRUE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(freg));
  EXPECT_EQ(W0, reg.AsVectorRegister());
  EXPECT_EQ(F0, reg.AsOverlappingFpuRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  reg = Mips64ManagedRegister::FromVectorRegister(W2);
  freg = Mips64ManagedRegister::FromFpuRegister(F2);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_TRUE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(freg));
  EXPECT_EQ(W2, reg.AsVectorRegister());
  EXPECT_EQ(F2, reg.AsOverlappingFpuRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromVectorRegister(W2)));

  reg = Mips64ManagedRegister::FromVectorRegister(W13);
  freg = Mips64ManagedRegister::FromFpuRegister(F13);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_TRUE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(freg));
  EXPECT_EQ(W13, reg.AsVectorRegister());
  EXPECT_EQ(F13, reg.AsOverlappingFpuRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromVectorRegister(W13)));

  reg = Mips64ManagedRegister::FromVectorRegister(W29);
  freg = Mips64ManagedRegister::FromFpuRegister(F29);
  EXPECT_FALSE(reg.IsNoRegister());
  EXPECT_FALSE(reg.IsGpuRegister());
  EXPECT_FALSE(reg.IsFpuRegister());
  EXPECT_TRUE(reg.IsVectorRegister());
  EXPECT_TRUE(reg.Overlaps(freg));
  EXPECT_EQ(W29, reg.AsVectorRegister());
  EXPECT_EQ(F29, reg.AsOverlappingFpuRegister());
  EXPECT_TRUE(reg.Equals(Mips64ManagedRegister::FromVectorRegister(W29)));
}

TEST(Mips64ManagedRegister, Equals) {
  ManagedRegister no_reg = ManagedRegister::NoRegister();
  EXPECT_TRUE(no_reg.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(no_reg.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(no_reg.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(no_reg.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(no_reg.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(no_reg.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_ZERO = Mips64ManagedRegister::FromGpuRegister(ZERO);
  EXPECT_FALSE(reg_ZERO.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_TRUE(reg_ZERO.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_ZERO.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_ZERO.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_ZERO.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_ZERO.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_A1 = Mips64ManagedRegister::FromGpuRegister(A1);
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_TRUE(reg_A1.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_A1.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_S2 = Mips64ManagedRegister::FromGpuRegister(S2);
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::FromGpuRegister(S1)));
  EXPECT_TRUE(reg_S2.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_S2.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_F0 = Mips64ManagedRegister::FromFpuRegister(F0);
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_TRUE(reg_F0.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromFpuRegister(F1)));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg_F0.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_F31 = Mips64ManagedRegister::FromFpuRegister(F31);
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromGpuRegister(S2)));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromFpuRegister(F1)));
  EXPECT_TRUE(reg_F31.Equals(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg_F31.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));

  Mips64ManagedRegister reg_W0 = Mips64ManagedRegister::FromVectorRegister(W0);
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromGpuRegister(S1)));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_TRUE(reg_W0.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromVectorRegister(W1)));
  EXPECT_FALSE(reg_W0.Equals(Mips64ManagedRegister::FromVectorRegister(W31)));

  Mips64ManagedRegister reg_W31 = Mips64ManagedRegister::FromVectorRegister(W31);
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::NoRegister()));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromGpuRegister(A1)));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromGpuRegister(S1)));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg_W31.Equals(Mips64ManagedRegister::FromVectorRegister(W1)));
  EXPECT_TRUE(reg_W31.Equals(Mips64ManagedRegister::FromVectorRegister(W31)));
}

TEST(Mips64ManagedRegister, Overlaps) {
  Mips64ManagedRegister reg = Mips64ManagedRegister::FromFpuRegister(F0);
  Mips64ManagedRegister reg_o = Mips64ManagedRegister::FromVectorRegister(W0);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(F0, reg_o.AsOverlappingFpuRegister());
  EXPECT_EQ(W0, reg.AsOverlappingVectorRegister());
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromFpuRegister(F4);
  reg_o = Mips64ManagedRegister::FromVectorRegister(W4);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(F4, reg_o.AsOverlappingFpuRegister());
  EXPECT_EQ(W4, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromFpuRegister(F16);
  reg_o = Mips64ManagedRegister::FromVectorRegister(W16);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(F16, reg_o.AsOverlappingFpuRegister());
  EXPECT_EQ(W16, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromFpuRegister(F31);
  reg_o = Mips64ManagedRegister::FromVectorRegister(W31);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(F31, reg_o.AsOverlappingFpuRegister());
  EXPECT_EQ(W31, reg.AsOverlappingVectorRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromVectorRegister(W0);
  reg_o = Mips64ManagedRegister::FromFpuRegister(F0);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(W0, reg_o.AsOverlappingVectorRegister());
  EXPECT_EQ(F0, reg.AsOverlappingFpuRegister());
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromVectorRegister(W4);
  reg_o = Mips64ManagedRegister::FromFpuRegister(F4);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(W4, reg_o.AsOverlappingVectorRegister());
  EXPECT_EQ(F4, reg.AsOverlappingFpuRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromVectorRegister(W16);
  reg_o = Mips64ManagedRegister::FromFpuRegister(F16);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(W16, reg_o.AsOverlappingVectorRegister());
  EXPECT_EQ(F16, reg.AsOverlappingFpuRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromVectorRegister(W31);
  reg_o = Mips64ManagedRegister::FromFpuRegister(F31);
  EXPECT_TRUE(reg.Overlaps(reg_o));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_EQ(W31, reg_o.AsOverlappingVectorRegister());
  EXPECT_EQ(F31, reg.AsOverlappingFpuRegister());
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromGpuRegister(ZERO);
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromGpuRegister(A0);
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromGpuRegister(S0);
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));

  reg = Mips64ManagedRegister::FromGpuRegister(RA);
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(ZERO)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(A0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(Mips64ManagedRegister::FromGpuRegister(RA)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromFpuRegister(F31)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W0)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W4)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W16)));
  EXPECT_FALSE(reg.Overlaps(Mips64ManagedRegister::FromVectorRegister(W31)));
}

}  // namespace mips64
}  // namespace art
