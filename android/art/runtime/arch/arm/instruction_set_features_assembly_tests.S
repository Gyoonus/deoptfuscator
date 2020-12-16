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

#include "asm_support_arm.S"

.section .text
// These functions are used to check for the CPU's support for the sdiv and
// ARMv8-A instructions at runtime. They will either return the value 1 or will
// cause an invalid instruction trap (SIGILL signal), for which the signal handler
// (bad_instr_handle(), in instruction_set_features_arm.cc) must arrange to set
// the r0 register to 0 and move the pc forward by 4 bytes (to skip the invalid
// instruction).
// Note: For ARM T32, instructions can be either 16b or 32b, but bad_instr_handle()
// deals only with 32b instructions for now.

ENTRY artCheckForArmSdivInstruction
  mov r1,#1
  // Depending on the architecture, the assembler will not allow an
  // sdiv instruction, so we will have to output the bytes directly.

  // The T32 encoding for sdiv r0,r1,r1 is two 16bit words: 0xfb91 0xf0f1, with little endianness.
  .byte 0x91,0xfb
  .byte 0xf1,0xf0

  // If the divide worked, r0 will have the value #1 (result of sdiv).
  // It will have 0 otherwise (set by the signal handler)
  // the value is just returned from this function.
  bx lr
END artCheckForArmSdivInstruction

ENTRY artCheckForArmv8AInstructions
  // Depending on the architecture, the assembler will not allow a
  // `vrint` instruction, so we will have to output the bytes directly.

  // Move `true` into the result register. The signal handler will set it to 0
  // if execution of the instruction below fails
  mov r0,#1

  // Store S0 in the caller saved R1. If the instruction below succeeds, S0 will
  // be clobbered but it will not be caller saved (ARM still uses soft FP).
  vmov r1, s0

  // The T32 encoding for vrinta.f32.f32 s0,s0 is two 16bit words: 0xfeb8,0x0a40, with little
  // endianness.
  .byte 0xb8,0xfe
  .byte 0x40,0x0a

  // Restore S0 (see above comment).
  vmov s0, r1

  bx lr
END artCheckForArmv8AInstructions
