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

#include "managed_register_arm64.h"

#include "assembler_arm64.h"
#include "globals.h"
#include "gtest/gtest.h"

namespace art {
namespace arm64 {

TEST(Arm64ManagedRegister, NoRegister) {
  Arm64ManagedRegister reg = ManagedRegister::NoRegister().AsArm64();
  EXPECT_TRUE(reg.IsNoRegister());
  EXPECT_TRUE(!reg.Overlaps(reg));
}

// X Register test.
TEST(Arm64ManagedRegister, XRegister) {
  Arm64ManagedRegister reg = Arm64ManagedRegister::FromXRegister(X0);
  Arm64ManagedRegister wreg = Arm64ManagedRegister::FromWRegister(W0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(X0, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(X1);
  wreg = Arm64ManagedRegister::FromWRegister(W1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(X1, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(X7);
  wreg = Arm64ManagedRegister::FromWRegister(W7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(X7, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(X15);
  wreg = Arm64ManagedRegister::FromWRegister(W15);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(X15, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(X19);
  wreg = Arm64ManagedRegister::FromWRegister(W19);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(X19, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(X16);
  wreg = Arm64ManagedRegister::FromWRegister(W16);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(wreg));
  EXPECT_EQ(IP0, reg.AsXRegister());

  reg = Arm64ManagedRegister::FromXRegister(SP);
  wreg = Arm64ManagedRegister::FromWRegister(WZR);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.Overlaps(wreg));
  EXPECT_EQ(SP, reg.AsXRegister());
}

// W register test.
TEST(Arm64ManagedRegister, WRegister) {
  Arm64ManagedRegister reg = Arm64ManagedRegister::FromWRegister(W0);
  Arm64ManagedRegister xreg = Arm64ManagedRegister::FromXRegister(X0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(xreg));
  EXPECT_EQ(W0, reg.AsWRegister());

  reg = Arm64ManagedRegister::FromWRegister(W5);
  xreg = Arm64ManagedRegister::FromXRegister(X5);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(xreg));
  EXPECT_EQ(W5, reg.AsWRegister());

  reg = Arm64ManagedRegister::FromWRegister(W6);
  xreg = Arm64ManagedRegister::FromXRegister(X6);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(xreg));
  EXPECT_EQ(W6, reg.AsWRegister());

  reg = Arm64ManagedRegister::FromWRegister(W18);
  xreg = Arm64ManagedRegister::FromXRegister(X18);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(xreg));
  EXPECT_EQ(W18, reg.AsWRegister());

  reg = Arm64ManagedRegister::FromWRegister(W29);
  xreg = Arm64ManagedRegister::FromXRegister(FP);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(xreg));
  EXPECT_EQ(W29, reg.AsWRegister());

  reg = Arm64ManagedRegister::FromWRegister(WZR);
  xreg = Arm64ManagedRegister::FromXRegister(SP);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(reg.IsWRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.Overlaps(xreg));
}

// D Register test.
TEST(Arm64ManagedRegister, DRegister) {
  Arm64ManagedRegister reg = Arm64ManagedRegister::FromDRegister(D0);
  Arm64ManagedRegister sreg = Arm64ManagedRegister::FromSRegister(S0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(sreg));
  EXPECT_EQ(D0, reg.AsDRegister());
  EXPECT_EQ(S0, reg.AsOverlappingSRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromDRegister(D0)));

  reg = Arm64ManagedRegister::FromDRegister(D1);
  sreg = Arm64ManagedRegister::FromSRegister(S1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(sreg));
  EXPECT_EQ(D1, reg.AsDRegister());
  EXPECT_EQ(S1, reg.AsOverlappingSRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromDRegister(D1)));

  reg = Arm64ManagedRegister::FromDRegister(D20);
  sreg = Arm64ManagedRegister::FromSRegister(S20);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(sreg));
  EXPECT_EQ(D20, reg.AsDRegister());
  EXPECT_EQ(S20, reg.AsOverlappingSRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromDRegister(D20)));

  reg = Arm64ManagedRegister::FromDRegister(D31);
  sreg = Arm64ManagedRegister::FromSRegister(S31);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.Overlaps(sreg));
  EXPECT_EQ(D31, reg.AsDRegister());
  EXPECT_EQ(S31, reg.AsOverlappingSRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromDRegister(D31)));
}

// S Register test.
TEST(Arm64ManagedRegister, SRegister) {
  Arm64ManagedRegister reg = Arm64ManagedRegister::FromSRegister(S0);
  Arm64ManagedRegister dreg = Arm64ManagedRegister::FromDRegister(D0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(reg.Overlaps(dreg));
  EXPECT_EQ(S0, reg.AsSRegister());
  EXPECT_EQ(D0, reg.AsOverlappingDRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromSRegister(S0)));

  reg = Arm64ManagedRegister::FromSRegister(S5);
  dreg = Arm64ManagedRegister::FromDRegister(D5);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(reg.Overlaps(dreg));
  EXPECT_EQ(S5, reg.AsSRegister());
  EXPECT_EQ(D5, reg.AsOverlappingDRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromSRegister(S5)));

  reg = Arm64ManagedRegister::FromSRegister(S7);
  dreg = Arm64ManagedRegister::FromDRegister(D7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(reg.Overlaps(dreg));
  EXPECT_EQ(S7, reg.AsSRegister());
  EXPECT_EQ(D7, reg.AsOverlappingDRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromSRegister(S7)));

  reg = Arm64ManagedRegister::FromSRegister(S31);
  dreg = Arm64ManagedRegister::FromDRegister(D31);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsXRegister());
  EXPECT_TRUE(!reg.IsWRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(reg.Overlaps(dreg));
  EXPECT_EQ(S31, reg.AsSRegister());
  EXPECT_EQ(D31, reg.AsOverlappingDRegister());
  EXPECT_TRUE(reg.Equals(Arm64ManagedRegister::FromSRegister(S31)));
}

TEST(Arm64ManagedRegister, Equals) {
  ManagedRegister no_reg = ManagedRegister::NoRegister();
  EXPECT_TRUE(no_reg.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!no_reg.Equals(Arm64ManagedRegister::FromSRegister(S0)));

  Arm64ManagedRegister reg_X0 = Arm64ManagedRegister::FromXRegister(X0);
  EXPECT_TRUE(!reg_X0.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(reg_X0.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_X0.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_X0.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!reg_X0.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_X0.Equals(Arm64ManagedRegister::FromDRegister(D0)));

  Arm64ManagedRegister reg_X1 = Arm64ManagedRegister::FromXRegister(X1);
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(reg_X1.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_X1.Equals(Arm64ManagedRegister::FromSRegister(S1)));

  Arm64ManagedRegister reg_SP = Arm64ManagedRegister::FromXRegister(SP);
  EXPECT_TRUE(!reg_SP.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_SP.Equals(Arm64ManagedRegister::FromXRegister(XZR)));
  EXPECT_TRUE(!reg_SP.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_SP.Equals(Arm64ManagedRegister::FromDRegister(D0)));

  Arm64ManagedRegister reg_W8 = Arm64ManagedRegister::FromWRegister(W8);
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromXRegister(X8)));
  EXPECT_TRUE(reg_W8.Equals(Arm64ManagedRegister::FromWRegister(W8)));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_W8.Equals(Arm64ManagedRegister::FromSRegister(S1)));

  Arm64ManagedRegister reg_W12 = Arm64ManagedRegister::FromWRegister(W12);
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromXRegister(X8)));
  EXPECT_TRUE(reg_W12.Equals(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_W12.Equals(Arm64ManagedRegister::FromSRegister(S1)));

  Arm64ManagedRegister reg_S0 = Arm64ManagedRegister::FromSRegister(S0);
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(reg_S0.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S0.Equals(Arm64ManagedRegister::FromDRegister(D1)));

  Arm64ManagedRegister reg_S1 = Arm64ManagedRegister::FromSRegister(S1);
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg_S1.Equals(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S1.Equals(Arm64ManagedRegister::FromDRegister(D1)));

  Arm64ManagedRegister reg_S31 = Arm64ManagedRegister::FromSRegister(S31);
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg_S31.Equals(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S31.Equals(Arm64ManagedRegister::FromDRegister(D1)));

  Arm64ManagedRegister reg_D0 = Arm64ManagedRegister::FromDRegister(D0);
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(reg_D0.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D0.Equals(Arm64ManagedRegister::FromDRegister(D1)));

  Arm64ManagedRegister reg_D15 = Arm64ManagedRegister::FromDRegister(D15);
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D15.Equals(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(reg_D15.Equals(Arm64ManagedRegister::FromDRegister(D15)));
}

TEST(Arm64ManagedRegister, Overlaps) {
  Arm64ManagedRegister reg = Arm64ManagedRegister::FromXRegister(X0);
  Arm64ManagedRegister reg_o = Arm64ManagedRegister::FromWRegister(W0);
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromXRegister(X0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(SP)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromWRegister(W0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_EQ(X0, reg_o.AsOverlappingXRegister());
  EXPECT_EQ(W0, reg.AsOverlappingWRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromXRegister(X10);
  reg_o = Arm64ManagedRegister::FromWRegister(W10);
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromXRegister(X10)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(SP)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromWRegister(W10)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_EQ(X10, reg_o.AsOverlappingXRegister());
  EXPECT_EQ(W10, reg.AsOverlappingWRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromXRegister(IP1);
  reg_o = Arm64ManagedRegister::FromWRegister(W17);
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromXRegister(X17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(SP)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromWRegister(W17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_EQ(X17, reg_o.AsOverlappingXRegister());
  EXPECT_EQ(W17, reg.AsOverlappingWRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromXRegister(XZR);
  reg_o = Arm64ManagedRegister::FromWRegister(WZR);
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(SP)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W19)));
  EXPECT_NE(SP, reg_o.AsOverlappingXRegister());
  EXPECT_EQ(XZR, reg_o.AsOverlappingXRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromXRegister(SP);
  reg_o = Arm64ManagedRegister::FromWRegister(WZR);
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromWRegister(W1);
  reg_o = Arm64ManagedRegister::FromXRegister(X1);
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W30)));
  EXPECT_EQ(W1, reg_o.AsOverlappingWRegister());
  EXPECT_EQ(X1, reg.AsOverlappingXRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromWRegister(W21);
  reg_o = Arm64ManagedRegister::FromXRegister(X21);
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromWRegister(W21)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromXRegister(X21)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W30)));
  EXPECT_EQ(W21, reg_o.AsOverlappingWRegister());
  EXPECT_EQ(X21, reg.AsOverlappingXRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));


  reg = Arm64ManagedRegister::FromSRegister(S1);
  reg_o = Arm64ManagedRegister::FromDRegister(D1);
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W30)));
  EXPECT_EQ(S1, reg_o.AsOverlappingSRegister());
  EXPECT_EQ(D1, reg.AsOverlappingDRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));

  reg = Arm64ManagedRegister::FromSRegister(S15);
  reg_o = Arm64ManagedRegister::FromDRegister(D15);
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W30)));
  EXPECT_EQ(S15, reg_o.AsOverlappingSRegister());
  EXPECT_EQ(D15, reg.AsOverlappingDRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S16)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D20)));

  reg = Arm64ManagedRegister::FromDRegister(D15);
  reg_o = Arm64ManagedRegister::FromSRegister(S15);
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X30)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromXRegister(X15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(WZR)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W1)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W12)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromWRegister(W30)));
  EXPECT_EQ(S15, reg.AsOverlappingSRegister());
  EXPECT_EQ(D15, reg_o.AsOverlappingDRegister());
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S16)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(reg.Overlaps(Arm64ManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D2)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D17)));
  EXPECT_TRUE(!reg.Overlaps(Arm64ManagedRegister::FromDRegister(D20)));
}

TEST(Arm64ManagedRegister, VixlRegisters) {
  // X Registers.
  EXPECT_TRUE(vixl::aarch64::x0.Is(Arm64Assembler::reg_x(X0)));
  EXPECT_TRUE(vixl::aarch64::x1.Is(Arm64Assembler::reg_x(X1)));
  EXPECT_TRUE(vixl::aarch64::x2.Is(Arm64Assembler::reg_x(X2)));
  EXPECT_TRUE(vixl::aarch64::x3.Is(Arm64Assembler::reg_x(X3)));
  EXPECT_TRUE(vixl::aarch64::x4.Is(Arm64Assembler::reg_x(X4)));
  EXPECT_TRUE(vixl::aarch64::x5.Is(Arm64Assembler::reg_x(X5)));
  EXPECT_TRUE(vixl::aarch64::x6.Is(Arm64Assembler::reg_x(X6)));
  EXPECT_TRUE(vixl::aarch64::x7.Is(Arm64Assembler::reg_x(X7)));
  EXPECT_TRUE(vixl::aarch64::x8.Is(Arm64Assembler::reg_x(X8)));
  EXPECT_TRUE(vixl::aarch64::x9.Is(Arm64Assembler::reg_x(X9)));
  EXPECT_TRUE(vixl::aarch64::x10.Is(Arm64Assembler::reg_x(X10)));
  EXPECT_TRUE(vixl::aarch64::x11.Is(Arm64Assembler::reg_x(X11)));
  EXPECT_TRUE(vixl::aarch64::x12.Is(Arm64Assembler::reg_x(X12)));
  EXPECT_TRUE(vixl::aarch64::x13.Is(Arm64Assembler::reg_x(X13)));
  EXPECT_TRUE(vixl::aarch64::x14.Is(Arm64Assembler::reg_x(X14)));
  EXPECT_TRUE(vixl::aarch64::x15.Is(Arm64Assembler::reg_x(X15)));
  EXPECT_TRUE(vixl::aarch64::x16.Is(Arm64Assembler::reg_x(X16)));
  EXPECT_TRUE(vixl::aarch64::x17.Is(Arm64Assembler::reg_x(X17)));
  EXPECT_TRUE(vixl::aarch64::x18.Is(Arm64Assembler::reg_x(X18)));
  EXPECT_TRUE(vixl::aarch64::x19.Is(Arm64Assembler::reg_x(X19)));
  EXPECT_TRUE(vixl::aarch64::x20.Is(Arm64Assembler::reg_x(X20)));
  EXPECT_TRUE(vixl::aarch64::x21.Is(Arm64Assembler::reg_x(X21)));
  EXPECT_TRUE(vixl::aarch64::x22.Is(Arm64Assembler::reg_x(X22)));
  EXPECT_TRUE(vixl::aarch64::x23.Is(Arm64Assembler::reg_x(X23)));
  EXPECT_TRUE(vixl::aarch64::x24.Is(Arm64Assembler::reg_x(X24)));
  EXPECT_TRUE(vixl::aarch64::x25.Is(Arm64Assembler::reg_x(X25)));
  EXPECT_TRUE(vixl::aarch64::x26.Is(Arm64Assembler::reg_x(X26)));
  EXPECT_TRUE(vixl::aarch64::x27.Is(Arm64Assembler::reg_x(X27)));
  EXPECT_TRUE(vixl::aarch64::x28.Is(Arm64Assembler::reg_x(X28)));
  EXPECT_TRUE(vixl::aarch64::x29.Is(Arm64Assembler::reg_x(X29)));
  EXPECT_TRUE(vixl::aarch64::x30.Is(Arm64Assembler::reg_x(X30)));

  EXPECT_TRUE(vixl::aarch64::x19.Is(Arm64Assembler::reg_x(TR)));
  EXPECT_TRUE(vixl::aarch64::ip0.Is(Arm64Assembler::reg_x(IP0)));
  EXPECT_TRUE(vixl::aarch64::ip1.Is(Arm64Assembler::reg_x(IP1)));
  EXPECT_TRUE(vixl::aarch64::x29.Is(Arm64Assembler::reg_x(FP)));
  EXPECT_TRUE(vixl::aarch64::lr.Is(Arm64Assembler::reg_x(LR)));
  EXPECT_TRUE(vixl::aarch64::sp.Is(Arm64Assembler::reg_x(SP)));
  EXPECT_TRUE(vixl::aarch64::xzr.Is(Arm64Assembler::reg_x(XZR)));

  // W Registers.
  EXPECT_TRUE(vixl::aarch64::w0.Is(Arm64Assembler::reg_w(W0)));
  EXPECT_TRUE(vixl::aarch64::w1.Is(Arm64Assembler::reg_w(W1)));
  EXPECT_TRUE(vixl::aarch64::w2.Is(Arm64Assembler::reg_w(W2)));
  EXPECT_TRUE(vixl::aarch64::w3.Is(Arm64Assembler::reg_w(W3)));
  EXPECT_TRUE(vixl::aarch64::w4.Is(Arm64Assembler::reg_w(W4)));
  EXPECT_TRUE(vixl::aarch64::w5.Is(Arm64Assembler::reg_w(W5)));
  EXPECT_TRUE(vixl::aarch64::w6.Is(Arm64Assembler::reg_w(W6)));
  EXPECT_TRUE(vixl::aarch64::w7.Is(Arm64Assembler::reg_w(W7)));
  EXPECT_TRUE(vixl::aarch64::w8.Is(Arm64Assembler::reg_w(W8)));
  EXPECT_TRUE(vixl::aarch64::w9.Is(Arm64Assembler::reg_w(W9)));
  EXPECT_TRUE(vixl::aarch64::w10.Is(Arm64Assembler::reg_w(W10)));
  EXPECT_TRUE(vixl::aarch64::w11.Is(Arm64Assembler::reg_w(W11)));
  EXPECT_TRUE(vixl::aarch64::w12.Is(Arm64Assembler::reg_w(W12)));
  EXPECT_TRUE(vixl::aarch64::w13.Is(Arm64Assembler::reg_w(W13)));
  EXPECT_TRUE(vixl::aarch64::w14.Is(Arm64Assembler::reg_w(W14)));
  EXPECT_TRUE(vixl::aarch64::w15.Is(Arm64Assembler::reg_w(W15)));
  EXPECT_TRUE(vixl::aarch64::w16.Is(Arm64Assembler::reg_w(W16)));
  EXPECT_TRUE(vixl::aarch64::w17.Is(Arm64Assembler::reg_w(W17)));
  EXPECT_TRUE(vixl::aarch64::w18.Is(Arm64Assembler::reg_w(W18)));
  EXPECT_TRUE(vixl::aarch64::w19.Is(Arm64Assembler::reg_w(W19)));
  EXPECT_TRUE(vixl::aarch64::w20.Is(Arm64Assembler::reg_w(W20)));
  EXPECT_TRUE(vixl::aarch64::w21.Is(Arm64Assembler::reg_w(W21)));
  EXPECT_TRUE(vixl::aarch64::w22.Is(Arm64Assembler::reg_w(W22)));
  EXPECT_TRUE(vixl::aarch64::w23.Is(Arm64Assembler::reg_w(W23)));
  EXPECT_TRUE(vixl::aarch64::w24.Is(Arm64Assembler::reg_w(W24)));
  EXPECT_TRUE(vixl::aarch64::w25.Is(Arm64Assembler::reg_w(W25)));
  EXPECT_TRUE(vixl::aarch64::w26.Is(Arm64Assembler::reg_w(W26)));
  EXPECT_TRUE(vixl::aarch64::w27.Is(Arm64Assembler::reg_w(W27)));
  EXPECT_TRUE(vixl::aarch64::w28.Is(Arm64Assembler::reg_w(W28)));
  EXPECT_TRUE(vixl::aarch64::w29.Is(Arm64Assembler::reg_w(W29)));
  EXPECT_TRUE(vixl::aarch64::w30.Is(Arm64Assembler::reg_w(W30)));
  EXPECT_TRUE(vixl::aarch64::w31.Is(Arm64Assembler::reg_w(WZR)));
  EXPECT_TRUE(vixl::aarch64::wzr.Is(Arm64Assembler::reg_w(WZR)));
  EXPECT_TRUE(vixl::aarch64::wsp.Is(Arm64Assembler::reg_w(WSP)));

  // D Registers.
  EXPECT_TRUE(vixl::aarch64::d0.Is(Arm64Assembler::reg_d(D0)));
  EXPECT_TRUE(vixl::aarch64::d1.Is(Arm64Assembler::reg_d(D1)));
  EXPECT_TRUE(vixl::aarch64::d2.Is(Arm64Assembler::reg_d(D2)));
  EXPECT_TRUE(vixl::aarch64::d3.Is(Arm64Assembler::reg_d(D3)));
  EXPECT_TRUE(vixl::aarch64::d4.Is(Arm64Assembler::reg_d(D4)));
  EXPECT_TRUE(vixl::aarch64::d5.Is(Arm64Assembler::reg_d(D5)));
  EXPECT_TRUE(vixl::aarch64::d6.Is(Arm64Assembler::reg_d(D6)));
  EXPECT_TRUE(vixl::aarch64::d7.Is(Arm64Assembler::reg_d(D7)));
  EXPECT_TRUE(vixl::aarch64::d8.Is(Arm64Assembler::reg_d(D8)));
  EXPECT_TRUE(vixl::aarch64::d9.Is(Arm64Assembler::reg_d(D9)));
  EXPECT_TRUE(vixl::aarch64::d10.Is(Arm64Assembler::reg_d(D10)));
  EXPECT_TRUE(vixl::aarch64::d11.Is(Arm64Assembler::reg_d(D11)));
  EXPECT_TRUE(vixl::aarch64::d12.Is(Arm64Assembler::reg_d(D12)));
  EXPECT_TRUE(vixl::aarch64::d13.Is(Arm64Assembler::reg_d(D13)));
  EXPECT_TRUE(vixl::aarch64::d14.Is(Arm64Assembler::reg_d(D14)));
  EXPECT_TRUE(vixl::aarch64::d15.Is(Arm64Assembler::reg_d(D15)));
  EXPECT_TRUE(vixl::aarch64::d16.Is(Arm64Assembler::reg_d(D16)));
  EXPECT_TRUE(vixl::aarch64::d17.Is(Arm64Assembler::reg_d(D17)));
  EXPECT_TRUE(vixl::aarch64::d18.Is(Arm64Assembler::reg_d(D18)));
  EXPECT_TRUE(vixl::aarch64::d19.Is(Arm64Assembler::reg_d(D19)));
  EXPECT_TRUE(vixl::aarch64::d20.Is(Arm64Assembler::reg_d(D20)));
  EXPECT_TRUE(vixl::aarch64::d21.Is(Arm64Assembler::reg_d(D21)));
  EXPECT_TRUE(vixl::aarch64::d22.Is(Arm64Assembler::reg_d(D22)));
  EXPECT_TRUE(vixl::aarch64::d23.Is(Arm64Assembler::reg_d(D23)));
  EXPECT_TRUE(vixl::aarch64::d24.Is(Arm64Assembler::reg_d(D24)));
  EXPECT_TRUE(vixl::aarch64::d25.Is(Arm64Assembler::reg_d(D25)));
  EXPECT_TRUE(vixl::aarch64::d26.Is(Arm64Assembler::reg_d(D26)));
  EXPECT_TRUE(vixl::aarch64::d27.Is(Arm64Assembler::reg_d(D27)));
  EXPECT_TRUE(vixl::aarch64::d28.Is(Arm64Assembler::reg_d(D28)));
  EXPECT_TRUE(vixl::aarch64::d29.Is(Arm64Assembler::reg_d(D29)));
  EXPECT_TRUE(vixl::aarch64::d30.Is(Arm64Assembler::reg_d(D30)));
  EXPECT_TRUE(vixl::aarch64::d31.Is(Arm64Assembler::reg_d(D31)));

  // S Registers.
  EXPECT_TRUE(vixl::aarch64::s0.Is(Arm64Assembler::reg_s(S0)));
  EXPECT_TRUE(vixl::aarch64::s1.Is(Arm64Assembler::reg_s(S1)));
  EXPECT_TRUE(vixl::aarch64::s2.Is(Arm64Assembler::reg_s(S2)));
  EXPECT_TRUE(vixl::aarch64::s3.Is(Arm64Assembler::reg_s(S3)));
  EXPECT_TRUE(vixl::aarch64::s4.Is(Arm64Assembler::reg_s(S4)));
  EXPECT_TRUE(vixl::aarch64::s5.Is(Arm64Assembler::reg_s(S5)));
  EXPECT_TRUE(vixl::aarch64::s6.Is(Arm64Assembler::reg_s(S6)));
  EXPECT_TRUE(vixl::aarch64::s7.Is(Arm64Assembler::reg_s(S7)));
  EXPECT_TRUE(vixl::aarch64::s8.Is(Arm64Assembler::reg_s(S8)));
  EXPECT_TRUE(vixl::aarch64::s9.Is(Arm64Assembler::reg_s(S9)));
  EXPECT_TRUE(vixl::aarch64::s10.Is(Arm64Assembler::reg_s(S10)));
  EXPECT_TRUE(vixl::aarch64::s11.Is(Arm64Assembler::reg_s(S11)));
  EXPECT_TRUE(vixl::aarch64::s12.Is(Arm64Assembler::reg_s(S12)));
  EXPECT_TRUE(vixl::aarch64::s13.Is(Arm64Assembler::reg_s(S13)));
  EXPECT_TRUE(vixl::aarch64::s14.Is(Arm64Assembler::reg_s(S14)));
  EXPECT_TRUE(vixl::aarch64::s15.Is(Arm64Assembler::reg_s(S15)));
  EXPECT_TRUE(vixl::aarch64::s16.Is(Arm64Assembler::reg_s(S16)));
  EXPECT_TRUE(vixl::aarch64::s17.Is(Arm64Assembler::reg_s(S17)));
  EXPECT_TRUE(vixl::aarch64::s18.Is(Arm64Assembler::reg_s(S18)));
  EXPECT_TRUE(vixl::aarch64::s19.Is(Arm64Assembler::reg_s(S19)));
  EXPECT_TRUE(vixl::aarch64::s20.Is(Arm64Assembler::reg_s(S20)));
  EXPECT_TRUE(vixl::aarch64::s21.Is(Arm64Assembler::reg_s(S21)));
  EXPECT_TRUE(vixl::aarch64::s22.Is(Arm64Assembler::reg_s(S22)));
  EXPECT_TRUE(vixl::aarch64::s23.Is(Arm64Assembler::reg_s(S23)));
  EXPECT_TRUE(vixl::aarch64::s24.Is(Arm64Assembler::reg_s(S24)));
  EXPECT_TRUE(vixl::aarch64::s25.Is(Arm64Assembler::reg_s(S25)));
  EXPECT_TRUE(vixl::aarch64::s26.Is(Arm64Assembler::reg_s(S26)));
  EXPECT_TRUE(vixl::aarch64::s27.Is(Arm64Assembler::reg_s(S27)));
  EXPECT_TRUE(vixl::aarch64::s28.Is(Arm64Assembler::reg_s(S28)));
  EXPECT_TRUE(vixl::aarch64::s29.Is(Arm64Assembler::reg_s(S29)));
  EXPECT_TRUE(vixl::aarch64::s30.Is(Arm64Assembler::reg_s(S30)));
  EXPECT_TRUE(vixl::aarch64::s31.Is(Arm64Assembler::reg_s(S31)));
}

}  // namespace arm64
}  // namespace art
