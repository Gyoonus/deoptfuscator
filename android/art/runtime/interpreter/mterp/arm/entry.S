/*
 * Copyright (C) 2016 The Android Open Source Project
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
/*
 * Interpreter entry point.
 */

    .text
    .align  2

/*
 * On entry:
 *  r0  Thread* self/
 *  r1  insns_
 *  r2  ShadowFrame
 *  r3  JValue* result_register
 *
 */

ENTRY ExecuteMterpImpl
    stmfd   sp!, {r3-r10,fp,lr}         @ save 10 regs, (r3 just to align 64)
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r3, 0
    .cfi_rel_offset r4, 4
    .cfi_rel_offset r5, 8
    .cfi_rel_offset r6, 12
    .cfi_rel_offset r7, 16
    .cfi_rel_offset r8, 20
    .cfi_rel_offset r9, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset fp, 32
    .cfi_rel_offset lr, 36

    /* Remember the return register */
    str     r3, [r2, #SHADOWFRAME_RESULT_REGISTER_OFFSET]

    /* Remember the dex instruction pointer */
    str     r1, [r2, #SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET]

    /* set up "named" registers */
    mov     rSELF, r0
    ldr     r0, [r2, #SHADOWFRAME_NUMBER_OF_VREGS_OFFSET]
    add     rFP, r2, #SHADOWFRAME_VREGS_OFFSET     @ point to vregs.
    VREG_INDEX_TO_ADDR rREFS, r0                   @ point to reference array in shadow frame
    ldr     r0, [r2, #SHADOWFRAME_DEX_PC_OFFSET]   @ Get starting dex_pc.
    add     rPC, r1, r0, lsl #1                    @ Create direct pointer to 1st dex opcode
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Starting ibase */
    ldr     rIBASE, [rSELF, #THREAD_CURRENT_IBASE_OFFSET]

    /* Set up for backwards branches & osr profiling */
    ldr     r0, [rFP, #OFF_FP_METHOD]
    add     r1, rFP, #OFF_FP_SHADOWFRAME
    mov     r2, rSELF
    bl      MterpSetUpHotnessCountdown
    mov     rPROFILE, r0                @ Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST                          @ load rINST from rPC
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
    /* NOTE: no fallthrough */
