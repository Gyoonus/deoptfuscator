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

#include "managed_register_x86.h"

#include "globals.h"

namespace art {
namespace x86 {

// Define register pairs.
// This list must be kept in sync with the RegisterPair enum.
#define REGISTER_PAIR_LIST(P) \
  P(EAX, EDX)                 \
  P(EAX, ECX)                 \
  P(EAX, EBX)                 \
  P(EAX, EDI)                 \
  P(EDX, ECX)                 \
  P(EDX, EBX)                 \
  P(EDX, EDI)                 \
  P(ECX, EBX)                 \
  P(ECX, EDI)                 \
  P(EBX, EDI)                 \
  P(ECX, EDX)


struct RegisterPairDescriptor {
  RegisterPair reg;  // Used to verify that the enum is in sync.
  Register low;
  Register high;
};


static const RegisterPairDescriptor kRegisterPairs[] = {
#define REGISTER_PAIR_ENUMERATION(low, high) { low##_##high, low, high },
  REGISTER_PAIR_LIST(REGISTER_PAIR_ENUMERATION)
#undef REGISTER_PAIR_ENUMERATION
};

std::ostream& operator<<(std::ostream& os, const RegisterPair& reg) {
  if (reg == kNoRegisterPair) {
    os << "kNoRegisterPair";
  } else {
    os << X86ManagedRegister::FromRegisterPair(reg);
  }
  return os;
}

bool X86ManagedRegister::Overlaps(const X86ManagedRegister& other) const {
  if (IsNoRegister() || other.IsNoRegister()) return false;
  CHECK(IsValidManagedRegister());
  CHECK(other.IsValidManagedRegister());
  if (Equals(other)) return true;
  if (IsRegisterPair()) {
    Register low = AsRegisterPairLow();
    Register high = AsRegisterPairHigh();
    return X86ManagedRegister::FromCpuRegister(low).Overlaps(other) ||
        X86ManagedRegister::FromCpuRegister(high).Overlaps(other);
  }
  if (other.IsRegisterPair()) {
    return other.Overlaps(*this);
  }
  return false;
}


int X86ManagedRegister::AllocIdLow() const {
  CHECK(IsRegisterPair());
  const int r = RegId() - (kNumberOfCpuRegIds + kNumberOfXmmRegIds +
                           kNumberOfX87RegIds);
  CHECK_EQ(r, kRegisterPairs[r].reg);
  return kRegisterPairs[r].low;
}


int X86ManagedRegister::AllocIdHigh() const {
  CHECK(IsRegisterPair());
  const int r = RegId() - (kNumberOfCpuRegIds + kNumberOfXmmRegIds +
                           kNumberOfX87RegIds);
  CHECK_EQ(r, kRegisterPairs[r].reg);
  return kRegisterPairs[r].high;
}


void X86ManagedRegister::Print(std::ostream& os) const {
  if (!IsValidManagedRegister()) {
    os << "No Register";
  } else if (IsXmmRegister()) {
    os << "XMM: " << AsXmmRegister();
  } else if (IsX87Register()) {
    os << "X87: " << AsX87Register();
  } else if (IsCpuRegister()) {
    os << "CPU: " << AsCpuRegister();
  } else if (IsRegisterPair()) {
    os << "Pair: " << AsRegisterPairLow() << ", " << AsRegisterPairHigh();
  } else {
    os << "??: " << RegId();
  }
}

std::ostream& operator<<(std::ostream& os, const X86ManagedRegister& reg) {
  reg.Print(os);
  return os;
}

}  // namespace x86
}  // namespace art
