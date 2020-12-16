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

#include "managed_register_mips64.h"

#include "globals.h"

namespace art {
namespace mips64 {

bool Mips64ManagedRegister::Overlaps(const Mips64ManagedRegister& other) const {
  if (IsNoRegister() || other.IsNoRegister()) return false;
  CHECK(IsValidManagedRegister());
  CHECK(other.IsValidManagedRegister());
  if (Equals(other)) return true;
  if (IsFpuRegister() && other.IsVectorRegister()) {
    return (AsFpuRegister() == other.AsOverlappingFpuRegister());
  } else if (IsVectorRegister() && other.IsFpuRegister()) {
    return (AsVectorRegister() == other.AsOverlappingVectorRegister());
  }
  return false;
}

void Mips64ManagedRegister::Print(std::ostream& os) const {
  if (!IsValidManagedRegister()) {
    os << "No Register";
  } else if (IsGpuRegister()) {
    os << "GPU: " << static_cast<int>(AsGpuRegister());
  } else if (IsFpuRegister()) {
     os << "FpuRegister: " << static_cast<int>(AsFpuRegister());
  } else if (IsVectorRegister()) {
     os << "VectorRegister: " << static_cast<int>(AsVectorRegister());
  } else {
    os << "??: " << RegId();
  }
}

std::ostream& operator<<(std::ostream& os, const Mips64ManagedRegister& reg) {
  reg.Print(os);
  return os;
}

}  // namespace mips64
}  // namespace art
