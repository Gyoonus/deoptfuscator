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

#ifndef ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_H_
#define ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_H_

#if !defined(__APPLE__)
  /*
   * Define the DEX PC (memory address of the currently interpreted bytecode)
   * within the CFI stream of the current function (stored in .eh_frame).
   * This allows libunwind to detect that the frame is in the interpreter,
   * and to resolve the memory address into human readable Java method name.
   * The CFI instruction is recognised by the magic bytes in the expression
   * (we push magic "DEX1" constant on the DWARF stack and drop it again).
   *
   * As with any other CFI opcode, the expression needs to be associated with
   * a register. Any caller-save register will do as those are unused in CFI.
   * Better solution would be to store the expression in Android-specific
   * DWARF register (CFI registers don't have to correspond to real hardware
   * registers), however, gdb handles any unknown registers very poorly.
   * Similarly, we could also use some of the user-defined opcodes defined
   * in the DWARF specification, but gdb doesn't support those either.
   *
   * The DEX PC is generally advanced in the middle of the bytecode handler,
   * which will result in the reported DEX PC to be off by an instruction.
   * Therefore the macro allows adding/subtracting an offset to compensate.
   * TODO: Add the offsets to handlers to get line-accurate DEX PC reporting.
   */
  #define CFI_DEFINE_DEX_PC_WITH_OFFSET(tmpReg, dexReg, dexOffset) .cfi_escape \
    0x16 /* DW_CFA_val_expression */, tmpReg, 0x09 /* size */,                 \
    0x0c /* DW_OP_const4u */, 0x44, 0x45, 0x58, 0x31, /* magic = "DEX1" */     \
    0x13 /* DW_OP_drop */,                                                     \
    0x92 /* DW_OP_bregx */, dexReg, (dexOffset & 0x7F) /* 1-byte SLEB128 */
#else
  // Mac OS doesn't like cfi_* directives.
  #define CFI_DEFINE_DEX_PC_WITH_OFFSET(tmpReg, dexReg, dexOffset)
#endif

#endif  // ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_H_
