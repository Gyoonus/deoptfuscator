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

#ifndef ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_
#define ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_

#include <android-base/logging.h>

#include "arch/arm64/registers_arm64.h"
#include "debug/dwarf/register.h"
#include "utils/managed_register.h"

namespace art {
namespace arm64 {

const int kNumberOfXRegIds = kNumberOfXRegisters;
const int kNumberOfWRegIds = kNumberOfWRegisters;
const int kNumberOfDRegIds = kNumberOfDRegisters;
const int kNumberOfSRegIds = kNumberOfSRegisters;

const int kNumberOfRegIds = kNumberOfXRegIds + kNumberOfWRegIds +
  kNumberOfDRegIds + kNumberOfSRegIds;

// Register ids map:
//  [0..X[  core registers 64bit (enum XRegister)
//  [X..W[  core registers 32bit (enum WRegister)
//  [W..D[  double precision VFP registers (enum DRegister)
//  [D..S[  single precision VFP registers (enum SRegister)
//
// where:
//  X = kNumberOfXRegIds
//  W = X + kNumberOfWRegIds
//  D = W + kNumberOfDRegIds
//  S = D + kNumberOfSRegIds
//
// An instance of class 'ManagedRegister' represents a single Arm64
// register. A register can be one of the following:
//  * core register 64bit context (enum XRegister)
//  * core register 32bit context (enum WRegister)
//  * VFP double precision register (enum DRegister)
//  * VFP single precision register (enum SRegister)
//
// There is a one to one mapping between ManagedRegister and register id.

class Arm64ManagedRegister : public ManagedRegister {
 public:
  constexpr XRegister AsXRegister() const {
    CHECK(IsXRegister());
    return static_cast<XRegister>(id_);
  }

  constexpr WRegister AsWRegister() const {
    CHECK(IsWRegister());
    return static_cast<WRegister>(id_ - kNumberOfXRegIds);
  }

  constexpr DRegister AsDRegister() const {
    CHECK(IsDRegister());
    return static_cast<DRegister>(id_ - kNumberOfXRegIds - kNumberOfWRegIds);
  }

  constexpr SRegister AsSRegister() const {
    CHECK(IsSRegister());
    return static_cast<SRegister>(id_ - kNumberOfXRegIds - kNumberOfWRegIds -
                                  kNumberOfDRegIds);
  }

  constexpr WRegister AsOverlappingWRegister() const {
    CHECK(IsValidManagedRegister());
    if (IsZeroRegister()) return WZR;
    return static_cast<WRegister>(AsXRegister());
  }

  constexpr XRegister AsOverlappingXRegister() const {
    CHECK(IsValidManagedRegister());
    return static_cast<XRegister>(AsWRegister());
  }

  constexpr SRegister AsOverlappingSRegister() const {
    CHECK(IsValidManagedRegister());
    return static_cast<SRegister>(AsDRegister());
  }

  constexpr DRegister AsOverlappingDRegister() const {
    CHECK(IsValidManagedRegister());
    return static_cast<DRegister>(AsSRegister());
  }

  constexpr bool IsXRegister() const {
    CHECK(IsValidManagedRegister());
    return (0 <= id_) && (id_ < kNumberOfXRegIds);
  }

  constexpr bool IsWRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - kNumberOfXRegIds;
    return (0 <= test) && (test < kNumberOfWRegIds);
  }

  constexpr bool IsDRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - (kNumberOfXRegIds + kNumberOfWRegIds);
    return (0 <= test) && (test < kNumberOfDRegIds);
  }

  constexpr bool IsSRegister() const {
    CHECK(IsValidManagedRegister());
    const int test = id_ - (kNumberOfXRegIds + kNumberOfWRegIds + kNumberOfDRegIds);
    return (0 <= test) && (test < kNumberOfSRegIds);
  }

  constexpr bool IsGPRegister() const {
    return IsXRegister() || IsWRegister();
  }

  constexpr bool IsFPRegister() const {
    return IsDRegister() || IsSRegister();
  }

  constexpr bool IsSameType(Arm64ManagedRegister test) const {
    CHECK(IsValidManagedRegister() && test.IsValidManagedRegister());
    return
      (IsXRegister() && test.IsXRegister()) ||
      (IsWRegister() && test.IsWRegister()) ||
      (IsDRegister() && test.IsDRegister()) ||
      (IsSRegister() && test.IsSRegister());
  }

  // Returns true if the two managed-registers ('this' and 'other') overlap.
  // Either managed-register may be the NoRegister. If both are the NoRegister
  // then false is returned.
  bool Overlaps(const Arm64ManagedRegister& other) const;

  void Print(std::ostream& os) const;

  static constexpr Arm64ManagedRegister FromXRegister(XRegister r) {
    CHECK_NE(r, kNoRegister);
    return FromRegId(r);
  }

  static constexpr Arm64ManagedRegister FromWRegister(WRegister r) {
    CHECK_NE(r, kNoWRegister);
    return FromRegId(r + kNumberOfXRegIds);
  }

  static constexpr Arm64ManagedRegister FromDRegister(DRegister r) {
    CHECK_NE(r, kNoDRegister);
    return FromRegId(r + (kNumberOfXRegIds + kNumberOfWRegIds));
  }

  static constexpr Arm64ManagedRegister FromSRegister(SRegister r) {
    CHECK_NE(r, kNoSRegister);
    return FromRegId(r + (kNumberOfXRegIds + kNumberOfWRegIds +
                          kNumberOfDRegIds));
  }

  // Returns the X register overlapping W register r.
  static constexpr Arm64ManagedRegister FromWRegisterX(WRegister r) {
    CHECK_NE(r, kNoWRegister);
    return FromRegId(r);
  }

  // Return the D register overlapping S register r.
  static constexpr Arm64ManagedRegister FromSRegisterD(SRegister r) {
    CHECK_NE(r, kNoSRegister);
    return FromRegId(r + (kNumberOfXRegIds + kNumberOfWRegIds));
  }

 private:
  constexpr bool IsValidManagedRegister() const {
    return (0 <= id_) && (id_ < kNumberOfRegIds);
  }

  constexpr bool IsStackPointer() const {
    return IsXRegister() && (id_ == SP);
  }

  constexpr bool IsZeroRegister() const {
    return IsXRegister() && (id_ == XZR);
  }

  constexpr int RegId() const {
    CHECK(!IsNoRegister());
    return id_;
  }

  int RegNo() const;
  int RegIdLow() const;
  int RegIdHigh() const;

  friend class ManagedRegister;

  explicit constexpr Arm64ManagedRegister(int reg_id) : ManagedRegister(reg_id) {}

  static constexpr Arm64ManagedRegister FromRegId(int reg_id) {
    Arm64ManagedRegister reg(reg_id);
    CHECK(reg.IsValidManagedRegister());
    return reg;
  }
};

std::ostream& operator<<(std::ostream& os, const Arm64ManagedRegister& reg);

}  // namespace arm64

constexpr inline arm64::Arm64ManagedRegister ManagedRegister::AsArm64() const {
  arm64::Arm64ManagedRegister reg(id_);
  CHECK(reg.IsNoRegister() || reg.IsValidManagedRegister());
  return reg;
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM64_MANAGED_REGISTER_ARM64_H_
