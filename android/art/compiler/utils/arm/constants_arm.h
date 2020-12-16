/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_
#define ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_

#include <stdint.h>

#include <iosfwd>

#include <android-base/logging.h>

#include "arch/arm/registers_arm.h"
#include "base/casts.h"
#include "globals.h"

namespace art {
namespace arm {

// Defines constants and accessor classes to assemble, disassemble and
// simulate ARM instructions.
//
// Section references in the code refer to the "ARM Architecture
// Reference Manual ARMv7-A and ARMv7-R edition", issue C.b (24 July
// 2012).
//
// Constants for specific fields are defined in their respective named enums.
// General constants are in an anonymous enum in class Instr.

// 4 bits option for the dmb instruction.
// Order and values follows those of the ARM Architecture Reference Manual.
enum DmbOptions {
  SY = 0xf,
  ST = 0xe,
  ISH = 0xb,
  ISHST = 0xa,
  NSH = 0x7,
  NSHST = 0x6
};

enum ScaleFactor {
  TIMES_1 = 0,
  TIMES_2 = 1,
  TIMES_4 = 2,
  TIMES_8 = 3
};

// Values for double-precision floating point registers.
enum DRegister {  // private marker to avoid generate-operator-out.py from processing.
  D0  = 0,
  D1  = 1,
  D2  = 2,
  D3  = 3,
  D4  = 4,
  D5  = 5,
  D6  = 6,
  D7  = 7,
  D8  = 8,
  D9  = 9,
  D10 = 10,
  D11 = 11,
  D12 = 12,
  D13 = 13,
  D14 = 14,
  D15 = 15,
  D16 = 16,
  D17 = 17,
  D18 = 18,
  D19 = 19,
  D20 = 20,
  D21 = 21,
  D22 = 22,
  D23 = 23,
  D24 = 24,
  D25 = 25,
  D26 = 26,
  D27 = 27,
  D28 = 28,
  D29 = 29,
  D30 = 30,
  D31 = 31,
  kNumberOfDRegisters = 32,
  kNumberOfOverlappingDRegisters = 16,
  kNoDRegister = -1,
};
std::ostream& operator<<(std::ostream& os, const DRegister& rhs);

// Opcodes for Data-processing instructions (instructions with a type 0 and 1)
// as defined in section A3.4
enum Opcode {
  kNoOperand = -1,
  AND = 0,   // Logical AND
  EOR = 1,   // Logical Exclusive OR
  SUB = 2,   // Subtract
  RSB = 3,   // Reverse Subtract
  ADD = 4,   // Add
  ADC = 5,   // Add with Carry
  SBC = 6,   // Subtract with Carry
  RSC = 7,   // Reverse Subtract with Carry
  TST = 8,   // Test
  TEQ = 9,   // Test Equivalence
  CMP = 10,  // Compare
  CMN = 11,  // Compare Negated
  ORR = 12,  // Logical (inclusive) OR
  MOV = 13,  // Move
  BIC = 14,  // Bit Clear
  MVN = 15,  // Move Not
  ORN = 16,  // Logical OR NOT.
  kMaxOperand = 17
};

// Size (in bytes) of registers.
const int kRegisterSize = 4;

// List of registers used in load/store multiple.
typedef uint16_t RegList;


}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_UTILS_ARM_CONSTANTS_ARM_H_
