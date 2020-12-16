/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_DEBUG_DWARF_REGISTER_H_
#define ART_COMPILER_DEBUG_DWARF_REGISTER_H_

namespace art {
namespace dwarf {

// Represents DWARF register.
class Reg {
 public:
  explicit Reg(int reg_num) : num_(reg_num) { }
  int num() const { return num_; }

  // TODO: Arm S0–S31 register mapping is obsolescent.
  //   We should use VFP-v3/Neon D0-D31 mapping instead.
  //   However, D0 is aliased to pair of S0 and S1, so using that
  //   mapping we cannot easily say S0 is spilled and S1 is not.
  //   There are ways around this in DWARF but they are complex.
  //   It would be much simpler to always spill whole D registers.
  //   Arm64 mapping is correct since we already do this there.
  //   libunwind might struggle with the new mapping as well.

  static Reg ArmCore(int num) { return Reg(num); }  // R0-R15.
  static Reg ArmFp(int num) { return Reg(64 + num); }  // S0–S31.
  static Reg ArmDp(int num) { return Reg(256 + num); }  // D0–D31.
  static Reg Arm64Core(int num) { return Reg(num); }  // X0-X31.
  static Reg Arm64Fp(int num) { return Reg(64 + num); }  // V0-V31.
  static Reg MipsCore(int num) { return Reg(num); }
  static Reg Mips64Core(int num) { return Reg(num); }
  static Reg MipsFp(int num) { return Reg(32 + num); }
  static Reg Mips64Fp(int num) { return Reg(32 + num); }
  static Reg X86Core(int num) { return Reg(num); }
  static Reg X86Fp(int num) { return Reg(21 + num); }
  static Reg X86_64Core(int num) {
    static const int map[8] = {0, 2, 1, 3, 7, 6, 4, 5};
    return Reg(num < 8 ? map[num] : num);
  }
  static Reg X86_64Fp(int num) { return Reg(17 + num); }

 private:
  int num_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_REGISTER_H_
