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
#include "globals.h"

namespace art {
namespace arm64 {

// TODO: Define convention
//
// Do not use APCS callee saved regs for now. Use:
//  * [X0, X15]
//  * [W0, W15]
//  * [D0, D31]
//  * [S0, S31]
// static const int kNumberOfAvailableXRegisters = (X15 - X0) + 1;
// static const int kNumberOfAvailableWRegisters = (W15 - W0) + 1;
// static const int kNumberOfAvailableDRegisters = kNumberOfDRegisters;
// static const int kNumberOfAvailableSRegisters = kNumberOfSRegisters;

// Returns true if this managed-register overlaps the other managed-register.
// GP Register Bank:
//       31____0 W[n]
// 63__________0 X[n]
//
// FP Register Bank:
//       31____0 S[n]
// 63__________0 D[n]
bool Arm64ManagedRegister::Overlaps(const Arm64ManagedRegister& other) const {
  if (IsNoRegister() || other.IsNoRegister()) return false;
  return (IsGPRegister() == other.IsGPRegister()) && (RegNo() == other.RegNo());
}

int Arm64ManagedRegister::RegNo() const {
  CHECK(!IsNoRegister());
  int no;
  if (IsXRegister()) {
    no = static_cast<int>(AsXRegister());
  } else if (IsWRegister()) {
    no = static_cast<int>(AsWRegister());
  } else if (IsDRegister()) {
    no = static_cast<int>(AsDRegister());
  } else if (IsSRegister()) {
    no = static_cast<int>(AsSRegister());
  } else {
    no = kNoRegister;
  }
  return no;
}

int Arm64ManagedRegister::RegIdLow() const {
  CHECK(IsXRegister() || IsDRegister());
  int low = RegNo();
  if (IsXRegister()) {
    low += kNumberOfXRegIds;
  } else if (IsDRegister()) {
    low += kNumberOfXRegIds + kNumberOfWRegIds + kNumberOfDRegIds;
  }
  return low;
}

// FIXME: Find better naming.
int Arm64ManagedRegister::RegIdHigh() const {
  CHECK(IsWRegister() || IsSRegister());
  int high = RegNo();
  if (IsSRegister()) {
    high += kNumberOfXRegIds + kNumberOfWRegIds;
  }
  return high;
}

void Arm64ManagedRegister::Print(std::ostream& os) const {
  if (!IsValidManagedRegister()) {
    os << "No Register";
  } else if (IsXRegister()) {
    os << "XCore: " << static_cast<int>(AsXRegister());
  } else if (IsWRegister()) {
    os << "WCore: " << static_cast<int>(AsWRegister());
  } else if (IsDRegister()) {
    os << "DRegister: " << static_cast<int>(AsDRegister());
  } else if (IsSRegister()) {
    os << "SRegister: " << static_cast<int>(AsSRegister());
  } else {
    os << "??: " << RegId();
  }
}

std::ostream& operator<<(std::ostream& os, const Arm64ManagedRegister& reg) {
  reg.Print(os);
  return os;
}

}  // namespace arm64
}  // namespace art
