/*
 * This file was generated automatically by gen-mterp.py for 'mips64'.
 *
 * --> DO NOT EDIT <--
 */

/* File: mips64/header.S */
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

#define zero $0  /* always zero */
#define AT   $at /* assembler temp */
#define v0   $2  /* return value */
#define v1   $3
#define a0   $4  /* argument registers */
#define a1   $5
#define a2   $6
#define a3   $7
#define a4   $8  /* expanded register arguments */
#define a5   $9
#define a6   $10
#define a7   $11
#define ta0  $8  /* alias */
#define ta1  $9
#define ta2  $10
#define ta3  $11
#define t0   $12 /* temp registers (not saved across subroutine calls) */
#define t1   $13
#define t2   $14
#define t3   $15

#define s0   $16 /* saved across subroutine calls (callee saved) */
#define s1   $17
#define s2   $18
#define s3   $19
#define s4   $20
#define s5   $21
#define s6   $22
#define s7   $23
#define t8   $24 /* two more temp registers */
#define t9   $25
#define k0   $26 /* kernel temporary */
#define k1   $27
#define gp   $28 /* global pointer */
#define sp   $29 /* stack pointer */
#define s8   $30 /* one more callee saved */
#define ra   $31 /* return address */

#define f0   $f0
#define f1   $f1
#define f2   $f2
#define f3   $f3
#define f12  $f12
#define f13  $f13

/*
 * It looks like the GNU assembler currently does not support the blec and bgtc
 * idioms, which should translate into bgec and bltc respectively with swapped
 * left and right register operands.
 * TODO: remove these macros when the assembler is fixed.
 */
.macro blec lreg, rreg, target
    bgec    \rreg, \lreg, \target
.endm
.macro bgtc lreg, rreg, target
    bltc    \rreg, \lreg, \target
.endm

/*
Mterp and MIPS64 notes:

The following registers have fixed assignments:

  reg nick      purpose
  s0  rPC       interpreted program counter, used for fetching instructions
  s1  rFP       interpreted frame pointer, used for accessing locals and args
  s2  rSELF     self (Thread) pointer
  s3  rINST     first 16-bit code unit of current instruction
  s4  rIBASE    interpreted instruction base pointer, used for computed goto
  s5  rREFS     base of object references in shadow frame  (ideally, we'll get rid of this later).
  s6  rPROFILE  jit profile hotness countdown
*/

/* During bringup, we'll use the shadow frame model instead of rFP */
/* single-purpose registers, given names for clarity */
#define rPC      s0
#define CFI_DEX  16  // DWARF register number of the register holding dex-pc (s0).
#define CFI_TMP  4   // DWARF register number of the first argument register (a0).
#define rFP      s1
#define rSELF    s2
#define rINST    s3
#define rIBASE   s4
#define rREFS    s5
#define rPROFILE s6

/*
 * This is a #include, not a %include, because we want the C pre-processor
 * to expand the macros into assembler assignment statements.
 */
#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

/*
 * Instead of holding a pointer to the shadow frame, we keep rFP at the base of the vregs.  So,
 * to access other shadow frame fields, we need to use a backwards offset.  Define those here.
 */
#define OFF_FP(a) (a - SHADOWFRAME_VREGS_OFFSET)
#define OFF_FP_NUMBER_OF_VREGS OFF_FP(SHADOWFRAME_NUMBER_OF_VREGS_OFFSET)
#define OFF_FP_DEX_PC OFF_FP(SHADOWFRAME_DEX_PC_OFFSET)
#define OFF_FP_LINK OFF_FP(SHADOWFRAME_LINK_OFFSET)
#define OFF_FP_METHOD OFF_FP(SHADOWFRAME_METHOD_OFFSET)
#define OFF_FP_RESULT_REGISTER OFF_FP(SHADOWFRAME_RESULT_REGISTER_OFFSET)
#define OFF_FP_DEX_PC_PTR OFF_FP(SHADOWFRAME_DEX_PC_PTR_OFFSET)
#define OFF_FP_DEX_INSTRUCTIONS OFF_FP(SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET)
#define OFF_FP_SHADOWFRAME OFF_FP(0)

#define MTERP_PROFILE_BRANCHES 1
#define MTERP_LOGGING 0

/*
 * "export" the PC to dex_pc field in the shadow frame, f/b/o future exception objects.  Must
 * be done *before* something throws.
 *
 * It's okay to do this more than once.
 *
 * NOTE: the fast interpreter keeps track of dex pc as a direct pointer to the mapped
 * dex byte codes.  However, the rest of the runtime expects dex pc to be an instruction
 * offset into the code_items_[] array.  For effiency, we will "export" the
 * current dex pc as a direct pointer using the EXPORT_PC macro, and rely on GetDexPC
 * to convert to a dex pc when needed.
 */
.macro EXPORT_PC
    sd      rPC, OFF_FP_DEX_PC_PTR(rFP)
.endm

/*
 * Refresh handler table.
 */
.macro REFRESH_IBASE
    ld      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)
.endm

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
.macro FETCH_INST
    lhu     rINST, 0(rPC)
.endm

/* Advance rPC by some number of code units. */
.macro ADVANCE count
    daddu   rPC, rPC, (\count) * 2
.endm

/*
 * Fetch the next instruction from an offset specified by _reg and advance xPC.
 * xPC to point to the next instruction.  "_reg" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.  Must not set flags.
 *
 */
.macro FETCH_ADVANCE_INST_RB reg
    daddu   rPC, rPC, \reg
    FETCH_INST
.endm

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
 * to point to the next instruction.
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC.)
 */
.macro FETCH_ADVANCE_INST count
    ADVANCE \count
    FETCH_INST
.endm

/*
 * Similar to FETCH_ADVANCE_INST, but does not update rPC.  Used to load
 * rINST ahead of possible exception point.  Be sure to manually advance rPC
 * later.
 */
.macro PREFETCH_INST count
    lhu     rINST, ((\count) * 2)(rPC)
.endm

/*
 * Put the instruction's opcode field into the specified register.
 */
.macro GET_INST_OPCODE reg
    and     \reg, rINST, 255
.endm

/*
 * Begin executing the opcode in _reg.
 */
.macro GOTO_OPCODE reg
    .set noat
    sll     AT, \reg, 7
    daddu   AT, rIBASE, AT
    jic     AT, 0
    .set at
.endm

/*
 * Get/set the 32-bit value from a Dalvik register.
 * Note, GET_VREG does sign extension to 64 bits while
 * GET_VREG_U does zero extension to 64 bits.
 * One is useful for arithmetic while the other is
 * useful for storing the result value as 64-bit.
 */
.macro GET_VREG reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lw      \reg, 0(AT)
    .set at
.endm
.macro GET_VREG_U reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwu     \reg, 0(AT)
    .set at
.endm
.macro GET_VREG_FLOAT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwc1    \reg, 0(AT)
    .set at
.endm
.macro SET_VREG reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    .set at
.endm
.macro SET_VREG_OBJECT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      \reg, 0(AT)
    .set at
.endm
.macro SET_VREG_FLOAT reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    swc1    \reg, 0(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    .set at
.endm

/*
 * Get/set the 64-bit value from a Dalvik register.
 * Avoid unaligned memory accesses.
 * Note, SET_VREG_WIDE clobbers the register containing the value being stored.
 * Note, SET_VREG_DOUBLE clobbers the register containing the Dalvik register number.
 */
.macro GET_VREG_WIDE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lw      \reg, 0(AT)
    lw      AT, 4(AT)
    dinsu   \reg, AT, 32, 32
    .set at
.endm
.macro GET_VREG_DOUBLE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    lwc1    \reg, 0(AT)
    lw      AT, 4(AT)
    mthc1   AT, \reg
    .set at
.endm
.macro SET_VREG_WIDE reg, vreg
    .set noat
    dlsa    AT, \vreg, rFP, 2
    sw      \reg, 0(AT)
    drotr32 \reg, \reg, 0
    sw      \reg, 4(AT)
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    sw      zero, 4(AT)
    .set at
.endm
.macro SET_VREG_DOUBLE reg, vreg
    .set noat
    dlsa    AT, \vreg, rREFS, 2
    sw      zero, 0(AT)
    sw      zero, 4(AT)
    dlsa    AT, \vreg, rFP, 2
    swc1    \reg, 0(AT)
    mfhc1   \vreg, \reg
    sw      \vreg, 4(AT)
    .set at
.endm

/*
 * On-stack offsets for spilling/unspilling callee-saved registers
 * and the frame size.
 */
#define STACK_OFFSET_RA 0
#define STACK_OFFSET_GP 8
#define STACK_OFFSET_S0 16
#define STACK_OFFSET_S1 24
#define STACK_OFFSET_S2 32
#define STACK_OFFSET_S3 40
#define STACK_OFFSET_S4 48
#define STACK_OFFSET_S5 56
#define STACK_OFFSET_S6 64
#define STACK_SIZE      80    /* needs 16 byte alignment */

/* Constants for float/double_to_int/long conversions */
#define INT_MIN             0x80000000
#define INT_MIN_AS_FLOAT    0xCF000000
#define INT_MIN_AS_DOUBLE   0xC1E0000000000000
#define LONG_MIN            0x8000000000000000
#define LONG_MIN_AS_FLOAT   0xDF000000
#define LONG_MIN_AS_DOUBLE  0xC3E0000000000000

/* File: mips64/entry.S */
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

    .set    reorder

    .text
    .global ExecuteMterpImpl
    .type   ExecuteMterpImpl, %function
    .balign 16
/*
 * On entry:
 *  a0  Thread* self
 *  a1  dex_instructions
 *  a2  ShadowFrame
 *  a3  JValue* result_register
 *
 */
ExecuteMterpImpl:
    .cfi_startproc
    .cpsetup t9, t8, ExecuteMterpImpl

    .cfi_def_cfa sp, 0
    daddu   sp, sp, -STACK_SIZE
    .cfi_adjust_cfa_offset STACK_SIZE

    sd      t8, STACK_OFFSET_GP(sp)
    .cfi_rel_offset 28, STACK_OFFSET_GP
    sd      ra, STACK_OFFSET_RA(sp)
    .cfi_rel_offset 31, STACK_OFFSET_RA

    sd      s0, STACK_OFFSET_S0(sp)
    .cfi_rel_offset 16, STACK_OFFSET_S0
    sd      s1, STACK_OFFSET_S1(sp)
    .cfi_rel_offset 17, STACK_OFFSET_S1
    sd      s2, STACK_OFFSET_S2(sp)
    .cfi_rel_offset 18, STACK_OFFSET_S2
    sd      s3, STACK_OFFSET_S3(sp)
    .cfi_rel_offset 19, STACK_OFFSET_S3
    sd      s4, STACK_OFFSET_S4(sp)
    .cfi_rel_offset 20, STACK_OFFSET_S4
    sd      s5, STACK_OFFSET_S5(sp)
    .cfi_rel_offset 21, STACK_OFFSET_S5
    sd      s6, STACK_OFFSET_S6(sp)
    .cfi_rel_offset 22, STACK_OFFSET_S6

    /* Remember the return register */
    sd      a3, SHADOWFRAME_RESULT_REGISTER_OFFSET(a2)

    /* Remember the dex instruction pointer */
    sd      a1, SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET(a2)

    /* set up "named" registers */
    move    rSELF, a0
    daddu   rFP, a2, SHADOWFRAME_VREGS_OFFSET
    lw      v0, SHADOWFRAME_NUMBER_OF_VREGS_OFFSET(a2)
    dlsa    rREFS, v0, rFP, 2
    lw      v0, SHADOWFRAME_DEX_PC_OFFSET(a2)
    dlsa    rPC, v0, a1, 1
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)
    EXPORT_PC

    /* Starting ibase */
    REFRESH_IBASE

    /* Set up for backwards branches & osr profiling */
    ld      a0, OFF_FP_METHOD(rFP)
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    jal     MterpSetUpHotnessCountdown
    move    rPROFILE, v0                # Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST
    GET_INST_OPCODE v0
    GOTO_OPCODE v0

    /* NOTE: no fallthrough */


    .global artMterpAsmInstructionStart
artMterpAsmInstructionStart = .L_op_nop
    .text

/* ------------------------------ */
    .balign 128
.L_op_nop: /* 0x00 */
/* File: mips64/op_nop.S */
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move: /* 0x01 */
/* File: mips64/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 0
    SET_VREG_OBJECT a0, a2              # vA <- vB
    .else
    SET_VREG a0, a2                     # vA <- vB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_from16: /* 0x02 */
/* File: mips64/op_move_from16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    lhu     a3, 2(rPC)                  # a3 <- BBBB
    srl     a2, rINST, 8                # a2 <- AA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vBBBB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 0
    SET_VREG_OBJECT a0, a2              # vAA <- vBBBB
    .else
    SET_VREG a0, a2                     # vAA <- vBBBB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_16: /* 0x03 */
/* File: mips64/op_move_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    lhu     a3, 4(rPC)                  # a3 <- BBBB
    lhu     a2, 2(rPC)                  # a2 <- AAAA
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vBBBB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 0
    SET_VREG_OBJECT a0, a2              # vAAAA <- vBBBB
    .else
    SET_VREG a0, a2                     # vAAAA <- vBBBB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_wide: /* 0x04 */
/* File: mips64/op_move_wide.S */
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    ext     a3, rINST, 12, 4            # a3 <- B
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG_WIDE a0, a3                # a0 <- vB
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- vB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_wide_from16: /* 0x05 */
/* File: mips64/op_move_wide_from16.S */
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    lhu     a3, 2(rPC)                  # a3 <- BBBB
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_WIDE a0, a3                # a0 <- vBBBB
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAA <- vBBBB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_wide_16: /* 0x06 */
/* File: mips64/op_move_wide_16.S */
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    lhu     a3, 4(rPC)                  # a3 <- BBBB
    lhu     a2, 2(rPC)                  # a2 <- AAAA
    GET_VREG_WIDE a0, a3                # a0 <- vBBBB
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAAAA <- vBBBB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_object: /* 0x07 */
/* File: mips64/op_move_object.S */
/* File: mips64/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 1
    SET_VREG_OBJECT a0, a2              # vA <- vB
    .else
    SET_VREG a0, a2                     # vA <- vB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_move_object_from16: /* 0x08 */
/* File: mips64/op_move_object_from16.S */
/* File: mips64/op_move_from16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    lhu     a3, 2(rPC)                  # a3 <- BBBB
    srl     a2, rINST, 8                # a2 <- AA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vBBBB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 1
    SET_VREG_OBJECT a0, a2              # vAA <- vBBBB
    .else
    SET_VREG a0, a2                     # vAA <- vBBBB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_move_object_16: /* 0x09 */
/* File: mips64/op_move_object_16.S */
/* File: mips64/op_move_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    lhu     a3, 4(rPC)                  # a3 <- BBBB
    lhu     a2, 2(rPC)                  # a2 <- AAAA
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vBBBB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 1
    SET_VREG_OBJECT a0, a2              # vAAAA <- vBBBB
    .else
    SET_VREG a0, a2                     # vAAAA <- vBBBB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_move_result: /* 0x0a */
/* File: mips64/op_move_result.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    srl     a2, rINST, 8                # a2 <- AA
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    ld      a0, OFF_FP_RESULT_REGISTER(rFP)  # get pointer to result JType
    lw      a0, 0(a0)                   # a0 <- result.i
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 0
    SET_VREG_OBJECT a0, a2              # vAA <- result
    .else
    SET_VREG a0, a2                     # vAA <- result
    .endif
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_result_wide: /* 0x0b */
/* File: mips64/op_move_result_wide.S */
    /* for: move-result-wide */
    /* op vAA */
    srl     a2, rINST, 8                # a2 <- AA
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    ld      a0, OFF_FP_RESULT_REGISTER(rFP)  # get pointer to result JType
    ld      a0, 0(a0)                   # a0 <- result.j
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAA <- result
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move_result_object: /* 0x0c */
/* File: mips64/op_move_result_object.S */
/* File: mips64/op_move_result.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    srl     a2, rINST, 8                # a2 <- AA
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    ld      a0, OFF_FP_RESULT_REGISTER(rFP)  # get pointer to result JType
    lw      a0, 0(a0)                   # a0 <- result.i
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 1
    SET_VREG_OBJECT a0, a2              # vAA <- result
    .else
    SET_VREG a0, a2                     # vAA <- result
    .endif
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_move_exception: /* 0x0d */
/* File: mips64/op_move_exception.S */
    /* move-exception vAA */
    srl     a2, rINST, 8                # a2 <- AA
    ld      a0, THREAD_EXCEPTION_OFFSET(rSELF)  # load exception obj
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    SET_VREG_OBJECT a0, a2              # vAA <- exception obj
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sd      zero, THREAD_EXCEPTION_OFFSET(rSELF)  # clear exception
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_return_void: /* 0x0e */
/* File: mips64/op_return_void.S */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    li      a0, 0
    b       MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return: /* 0x0f */
/* File: mips64/op_return.S */
    /*
     * Return a 32-bit value.
     *
     * for: return (sign-extend), return-object (zero-extend)
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG  a0, a2                      # a0 <- vAA
    b       MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return_wide: /* 0x10 */
/* File: mips64/op_return_wide.S */
    /*
     * Return a 64-bit value.
     */
    /* return-wide vAA */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_WIDE a0, a2                # a0 <- vAA
    b       MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return_object: /* 0x11 */
/* File: mips64/op_return_object.S */
/* File: mips64/op_return.S */
    /*
     * Return a 32-bit value.
     *
     * for: return (sign-extend), return-object (zero-extend)
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    .extern MterpSuspendCheck
    jal     MterpThreadFenceForConstructor
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_U  a0, a2                      # a0 <- vAA
    b       MterpReturn


/* ------------------------------ */
    .balign 128
.L_op_const_4: /* 0x12 */
/* File: mips64/op_const_4.S */
    /* const/4 vA, #+B */
    ext     a2, rINST, 8, 4             # a2 <- A
    seh     a0, rINST                   # sign extend B in rINST
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    sra     a0, a0, 12                  # shift B into its final position
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- +B
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_16: /* 0x13 */
/* File: mips64/op_const_16.S */
    /* const/16 vAA, #+BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- sign-extended BBBB
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vAA <- +BBBB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const: /* 0x14 */
/* File: mips64/op_const.S */
    /* const vAA, #+BBBBbbbb */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- bbbb (low)
    lh      a1, 4(rPC)                  # a1 <- BBBB (high)
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    ins     a0, a1, 16, 16              # a0 = BBBBbbbb
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vAA <- +BBBBbbbb
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_high16: /* 0x15 */
/* File: mips64/op_const_high16.S */
    /* const/high16 vAA, #+BBBB0000 */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- BBBB
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    sll     a0, a0, 16                  # a0 <- BBBB0000
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vAA <- +BBBB0000
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_wide_16: /* 0x16 */
/* File: mips64/op_const_wide_16.S */
    /* const-wide/16 vAA, #+BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- sign-extended BBBB
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAA <- +BBBB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_wide_32: /* 0x17 */
/* File: mips64/op_const_wide_32.S */
    /* const-wide/32 vAA, #+BBBBbbbb */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- bbbb (low)
    lh      a1, 4(rPC)                  # a1 <- BBBB (high)
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    ins     a0, a1, 16, 16              # a0 = BBBBbbbb
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAA <- +BBBBbbbb
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_wide: /* 0x18 */
/* File: mips64/op_const_wide.S */
    /* const-wide vAA, #+HHHHhhhhBBBBbbbb */
    srl     a4, rINST, 8                # a4 <- AA
    lh      a0, 2(rPC)                  # a0 <- bbbb (low)
    lh      a1, 4(rPC)                  # a1 <- BBBB (low middle)
    lh      a2, 6(rPC)                  # a2 <- hhhh (high middle)
    lh      a3, 8(rPC)                  # a3 <- HHHH (high)
    FETCH_ADVANCE_INST 5                # advance rPC, load rINST
    ins     a0, a1, 16, 16              # a0 = BBBBbbbb
    ins     a2, a3, 16, 16              # a2 = HHHHhhhh
    dinsu   a0, a2, 32, 32              # a0 = HHHHhhhhBBBBbbbb
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4                # vAA <- +HHHHhhhhBBBBbbbb
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_wide_high16: /* 0x19 */
/* File: mips64/op_const_wide_high16.S */
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    srl     a2, rINST, 8                # a2 <- AA
    lh      a0, 2(rPC)                  # a0 <- BBBB
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    dsll32  a0, a0, 16                  # a0 <- BBBB000000000000
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vAA <- +BBBB000000000000
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_string: /* 0x1a */
/* File: mips64/op_const_string.S */
/* File: mips64/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstString
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     MterpConstString                     # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_const_string_jumbo: /* 0x1b */
/* File: mips64/op_const_string_jumbo.S */
    /* const/string vAA, String//BBBBBBBB */
    .extern MterpConstString
    EXPORT_PC
    lh      a0, 2(rPC)                  # a0 <- bbbb (low)
    lh      a4, 4(rPC)                  # a4 <- BBBB (high)
    srl     a1, rINST, 8                # a1 <- AA
    ins     a0, a4, 16, 16              # a0 <- BBBBbbbb
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     MterpConstString            # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 3                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 3                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_class: /* 0x1c */
/* File: mips64/op_const_class.S */
/* File: mips64/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstClass
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     MterpConstClass                     # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_monitor_enter: /* 0x1d */
/* File: mips64/op_monitor_enter.S */
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    .extern artLockObjectFromCode
    EXPORT_PC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_U a0, a2                   # a0 <- vAA (object)
    move    a1, rSELF                   # a1 <- self
    jal     artLockObjectFromCode
    bnezc   v0, MterpException
    FETCH_ADVANCE_INST 1
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_monitor_exit: /* 0x1e */
/* File: mips64/op_monitor_exit.S */
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    .extern artUnlockObjectFromCode
    EXPORT_PC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_U a0, a2                   # a0 <- vAA (object)
    move    a1, rSELF                   # a1 <- self
    jal     artUnlockObjectFromCode     # v0 <- success for unlock(self, obj)
    bnezc   v0, MterpException
    FETCH_ADVANCE_INST 1                # before throw: advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_check_cast: /* 0x1f */
/* File: mips64/op_check_cast.S */
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class//BBBB */
    .extern MterpCheckCast
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    dlsa    a1, a1, rFP, 2              # a1 <- &object
    ld      a2, OFF_FP_METHOD(rFP)      # a2 <- method
    move    a3, rSELF                   # a3 <- self
    jal     MterpCheckCast              # (index, &obj, method, self)
    PREFETCH_INST 2
    bnez    v0, MterpPossibleException
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_instance_of: /* 0x20 */
/* File: mips64/op_instance_of.S */
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class//CCCC */
    .extern MterpInstanceOf
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- CCCC
    srl     a1, rINST, 12               # a1 <- B
    dlsa    a1, a1, rFP, 2              # a1 <- &object
    ld      a2, OFF_FP_METHOD(rFP)      # a2 <- method
    move    a3, rSELF                   # a3 <- self
    jal     MterpInstanceOf             # (index, &obj, method, self)
    ld      a1, THREAD_EXCEPTION_OFFSET(rSELF)
    ext     a2, rINST, 8, 4             # a2 <- A
    PREFETCH_INST 2
    bnez    a1, MterpException
    ADVANCE 2                           # advance rPC
    SET_VREG v0, a2                     # vA <- v0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_array_length: /* 0x21 */
/* File: mips64/op_array_length.S */
    /*
     * Return the length of an array.
     */
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a0, a1                   # a0 <- vB (object ref)
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a0, common_errNullObject    # yup, fail
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- array length
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a3, a2                     # vB <- length
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_new_instance: /* 0x22 */
/* File: mips64/op_new_instance.S */
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class//BBBB */
    .extern MterpNewInstance
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rSELF
    move    a2, rINST
    jal     MterpNewInstance            # (shadow_frame, self, inst_data)
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_new_array: /* 0x23 */
/* File: mips64/op_new_array.S */
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class//CCCC */
    .extern MterpNewArray
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    move    a3, rSELF
    jal     MterpNewArray
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_filled_new_array: /* 0x24 */
/* File: mips64/op_filled_new_array.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class//CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type//BBBB */
    .extern MterpFilledNewArray
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rSELF
    jal     MterpFilledNewArray
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_filled_new_array_range: /* 0x25 */
/* File: mips64/op_filled_new_array_range.S */
/* File: mips64/op_filled_new_array.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class//CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type//BBBB */
    .extern MterpFilledNewArrayRange
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rSELF
    jal     MterpFilledNewArrayRange
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_fill_array_data: /* 0x26 */
/* File: mips64/op_fill_array_data.S */
    /* fill-array-data vAA, +BBBBBBBB */
    .extern MterpFillArrayData
    EXPORT_PC
    lh      a1, 2(rPC)                  # a1 <- bbbb (lo)
    lh      a0, 4(rPC)                  # a0 <- BBBB (hi)
    srl     a3, rINST, 8                # a3 <- AA
    ins     a1, a0, 16, 16              # a1 <- BBBBbbbb
    GET_VREG_U a0, a3                   # a0 <- vAA (array object)
    dlsa    a1, a1, rPC, 1              # a1 <- PC + BBBBbbbb*2 (array data off.)
    jal     MterpFillArrayData          # (obj, payload)
    beqzc   v0, MterpPossibleException  # exception?
    FETCH_ADVANCE_INST 3                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_throw: /* 0x27 */
/* File: mips64/op_throw.S */
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG_U a0, a2                   # a0 <- vAA (exception object)
    beqzc   a0, common_errNullObject
    sd      a0, THREAD_EXCEPTION_OFFSET(rSELF)  # thread->exception <- obj
    b       MterpException

/* ------------------------------ */
    .balign 128
.L_op_goto: /* 0x28 */
/* File: mips64/op_goto.S */
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    srl     rINST, rINST, 8
    seb     rINST, rINST                # rINST <- offset (sign-extended AA)
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_goto_16: /* 0x29 */
/* File: mips64/op_goto_16.S */
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended AAAA)
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_goto_32: /* 0x2a */
/* File: mips64/op_goto_32.S */
    /*
     * Unconditional branch, 32-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     *
     * Unlike most opcodes, this one is allowed to branch to itself, so
     * our "backward branch" test must be "<=0" instead of "<0".
     */
    /* goto/32 +AAAAAAAA */
    lh      rINST, 2(rPC)               # rINST <- aaaa (low)
    lh      a1, 4(rPC)                  # a1 <- AAAA (high)
    ins     rINST, a1, 16, 16           # rINST <- offset (sign-extended AAAAaaaa)
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_packed_switch: /* 0x2b */
/* File: mips64/op_packed_switch.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBBBBBB */
    .extern MterpDoPackedSwitch
    lh      a0, 2(rPC)                  # a0 <- bbbb (lo)
    lh      a1, 4(rPC)                  # a1 <- BBBB (hi)
    srl     a3, rINST, 8                # a3 <- AA
    ins     a0, a1, 16, 16              # a0 <- BBBBbbbb
    GET_VREG a1, a3                     # a1 <- vAA
    dlsa    a0, a0, rPC, 1              # a0 <- PC + BBBBbbbb*2
    jal     MterpDoPackedSwitch                       # v0 <- code-unit branch offset
    move    rINST, v0
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_sparse_switch: /* 0x2c */
/* File: mips64/op_sparse_switch.S */
/* File: mips64/op_packed_switch.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBBBBBB */
    .extern MterpDoSparseSwitch
    lh      a0, 2(rPC)                  # a0 <- bbbb (lo)
    lh      a1, 4(rPC)                  # a1 <- BBBB (hi)
    srl     a3, rINST, 8                # a3 <- AA
    ins     a0, a1, 16, 16              # a0 <- BBBBbbbb
    GET_VREG a1, a3                     # a1 <- vAA
    dlsa    a0, a0, rPC, 1              # a0 <- PC + BBBBbbbb*2
    jal     MterpDoSparseSwitch                       # v0 <- code-unit branch offset
    move    rINST, v0
    b       MterpCommonTakenBranchNoFlags


/* ------------------------------ */
    .balign 128
.L_op_cmpl_float: /* 0x2d */
/* File: mips64/op_cmpl_float.S */
/* File: mips64/fcmp.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * For: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    cmp.eq.s f2, f0, f1
    li      a0, 0
    bc1nez  f2, 1f                      # done if vBB == vCC (ordered)
    .if 0
    cmp.lt.s f2, f0, f1
    li      a0, -1
    bc1nez  f2, 1f                      # done if vBB < vCC (ordered)
    li      a0, 1                       # vBB > vCC or unordered
    .else
    cmp.lt.s f2, f1, f0
    li      a0, 1
    bc1nez  f2, 1f                      # done if vBB > vCC (ordered)
    li      a0, -1                      # vBB < vCC or unordered
    .endif
1:
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_cmpg_float: /* 0x2e */
/* File: mips64/op_cmpg_float.S */
/* File: mips64/fcmp.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * For: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    cmp.eq.s f2, f0, f1
    li      a0, 0
    bc1nez  f2, 1f                      # done if vBB == vCC (ordered)
    .if 1
    cmp.lt.s f2, f0, f1
    li      a0, -1
    bc1nez  f2, 1f                      # done if vBB < vCC (ordered)
    li      a0, 1                       # vBB > vCC or unordered
    .else
    cmp.lt.s f2, f1, f0
    li      a0, 1
    bc1nez  f2, 1f                      # done if vBB > vCC (ordered)
    li      a0, -1                      # vBB < vCC or unordered
    .endif
1:
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_cmpl_double: /* 0x2f */
/* File: mips64/op_cmpl_double.S */
/* File: mips64/fcmpWide.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    cmp.eq.d f2, f0, f1
    li      a0, 0
    bc1nez  f2, 1f                      # done if vBB == vCC (ordered)
    .if 0
    cmp.lt.d f2, f0, f1
    li      a0, -1
    bc1nez  f2, 1f                      # done if vBB < vCC (ordered)
    li      a0, 1                       # vBB > vCC or unordered
    .else
    cmp.lt.d f2, f1, f0
    li      a0, 1
    bc1nez  f2, 1f                      # done if vBB > vCC (ordered)
    li      a0, -1                      # vBB < vCC or unordered
    .endif
1:
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_cmpg_double: /* 0x30 */
/* File: mips64/op_cmpg_double.S */
/* File: mips64/fcmpWide.S */
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    cmp.eq.d f2, f0, f1
    li      a0, 0
    bc1nez  f2, 1f                      # done if vBB == vCC (ordered)
    .if 1
    cmp.lt.d f2, f0, f1
    li      a0, -1
    bc1nez  f2, 1f                      # done if vBB < vCC (ordered)
    li      a0, 1                       # vBB > vCC or unordered
    .else
    cmp.lt.d f2, f1, f0
    li      a0, 1
    bc1nez  f2, 1f                      # done if vBB > vCC (ordered)
    li      a0, -1                      # vBB < vCC or unordered
    .endif
1:
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_cmp_long: /* 0x31 */
/* File: mips64/op_cmp_long.S */
    /* cmp-long vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    slt     a2, a0, a1
    slt     a0, a1, a0
    subu    a0, a0, a2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                     # vAA <- result
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_if_eq: /* 0x32 */
/* File: mips64/op_if_eq.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    beqc a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ne: /* 0x33 */
/* File: mips64/op_if_ne.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    bnec a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_lt: /* 0x34 */
/* File: mips64/op_if_lt.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    bltc a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ge: /* 0x35 */
/* File: mips64/op_if_ge.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    bgec a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gt: /* 0x36 */
/* File: mips64/op_if_gt.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    bgtc a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_le: /* 0x37 */
/* File: mips64/op_if_le.S */
/* File: mips64/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-le" you would use "le".
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended CCCC)
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    blec a0, a1, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_eqz: /* 0x38 */
/* File: mips64/op_if_eqz.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    beqzc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_nez: /* 0x39 */
/* File: mips64/op_if_nez.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    bnezc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ltz: /* 0x3a */
/* File: mips64/op_if_ltz.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    bltzc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gez: /* 0x3b */
/* File: mips64/op_if_gez.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    bgezc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gtz: /* 0x3c */
/* File: mips64/op_if_gtz.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    bgtzc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_lez: /* 0x3d */
/* File: mips64/op_if_lez.S */
/* File: mips64/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform, e.g. for
     * "if-lez" you would use "le".
     *
     * For: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    srl     a2, rINST, 8                # a2 <- AA
    lh      rINST, 2(rPC)               # rINST <- offset (sign-extended BBBB)
    GET_VREG a0, a2                     # a0 <- vAA
    blezc a0, MterpCommonTakenBranchNoFlags
    li      v0, JIT_CHECK_OSR           # possible OSR re-entry?
    beqc    rPROFILE, v0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_unused_3e: /* 0x3e */
/* File: mips64/op_unused_3e.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_3f: /* 0x3f */
/* File: mips64/op_unused_3f.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_40: /* 0x40 */
/* File: mips64/op_unused_40.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_41: /* 0x41 */
/* File: mips64/op_unused_41.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_42: /* 0x42 */
/* File: mips64/op_unused_42.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_43: /* 0x43 */
/* File: mips64/op_unused_43.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_aget: /* 0x44 */
/* File: mips64/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 2
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 2          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lw   a2, MIRROR_INT_ARRAY_DATA_OFFSET(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aget_wide: /* 0x45 */
/* File: mips64/op_aget_wide.S */
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     */
    /* aget-wide vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    dlsa    a0, a1, a0, 3               # a0 <- arrayObj + index*width
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lw      a2, MIRROR_WIDE_ARRAY_DATA_OFFSET(a0)
    lw      a3, (MIRROR_WIDE_ARRAY_DATA_OFFSET+4)(a0)
    dinsu   a2, a3, 32, 32              # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a2, a4                # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aget_object: /* 0x46 */
/* File: mips64/op_aget_object.S */
    /*
     * Array object get.  vAA <- vBB[vCC].
     *
     * for: aget-object
     */
    /* op vAA, vBB, vCC */
    .extern artAGetObjectFromMterp
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    EXPORT_PC
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    jal     artAGetObjectFromMterp      # (array, index)
    ld      a1, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a4, rINST, 8                # a4 <- AA
    PREFETCH_INST 2
    bnez    a1, MterpException
    SET_VREG_OBJECT v0, a4              # vAA <- v0
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aget_boolean: /* 0x47 */
/* File: mips64/op_aget_boolean.S */
/* File: mips64/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 0
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 0          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lbu   a2, MIRROR_BOOLEAN_ARRAY_DATA_OFFSET(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aget_byte: /* 0x48 */
/* File: mips64/op_aget_byte.S */
/* File: mips64/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 0
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 0          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lb   a2, MIRROR_BYTE_ARRAY_DATA_OFFSET(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aget_char: /* 0x49 */
/* File: mips64/op_aget_char.S */
/* File: mips64/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 1
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 1          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lhu   a2, MIRROR_CHAR_ARRAY_DATA_OFFSET(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aget_short: /* 0x4a */
/* File: mips64/op_aget_short.S */
/* File: mips64/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 1
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 1          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    lh   a2, MIRROR_SHORT_ARRAY_DATA_OFFSET(a0)        # a2 <- vBB[vCC]
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a2, a4                     # vAA <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput: /* 0x4b */
/* File: mips64/op_aput.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 2
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 2          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sw  a2, MIRROR_INT_ARRAY_DATA_OFFSET(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_wide: /* 0x4c */
/* File: mips64/op_aput_wide.S */
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     *
     */
    /* aput-wide vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    dlsa    a0, a1, a0, 3               # a0 <- arrayObj + index*width
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    GET_VREG_WIDE a2, a4                # a2 <- vAA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sw      a2, MIRROR_WIDE_ARRAY_DATA_OFFSET(a0)
    dsrl32  a2, a2, 0
    sw      a2, (MIRROR_WIDE_ARRAY_DATA_OFFSET+4)(a0)  # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_object: /* 0x4d */
/* File: mips64/op_aput_object.S */
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     */
    /* op vAA, vBB, vCC */
    .extern MterpAputObject
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    jal     MterpAputObject
    beqzc   v0, MterpPossibleException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_boolean: /* 0x4e */
/* File: mips64/op_aput_boolean.S */
/* File: mips64/op_aput.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 0
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 0          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sb  a2, MIRROR_BOOLEAN_ARRAY_DATA_OFFSET(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_byte: /* 0x4f */
/* File: mips64/op_aput_byte.S */
/* File: mips64/op_aput.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 0
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 0          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sb  a2, MIRROR_BYTE_ARRAY_DATA_OFFSET(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_char: /* 0x50 */
/* File: mips64/op_aput_char.S */
/* File: mips64/op_aput.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 1
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 1          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sh  a2, MIRROR_CHAR_ARRAY_DATA_OFFSET(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_short: /* 0x51 */
/* File: mips64/op_aput_short.S */
/* File: mips64/op_aput.S */
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    srl     a4, rINST, 8                # a4 <- AA
    GET_VREG_U a0, a2                   # a0 <- vBB (array object)
    GET_VREG a1, a3                     # a1 <- vCC (requested index)
    beqz    a0, common_errNullObject    # bail if null array object
    lw      a3, MIRROR_ARRAY_LENGTH_OFFSET(a0)  # a3 <- arrayObj->length
    .if 1
    # [d]lsa does not support shift count of 0.
    dlsa    a0, a1, a0, 1          # a0 <- arrayObj + index*width
    .else
    daddu   a0, a1, a0                  # a0 <- arrayObj + index*width
    .endif
    bgeu    a1, a3, common_errArrayIndex  # unsigned compare: index >= length, bail
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_VREG a2, a4                     # a2 <- vAA
    GET_INST_OPCODE v0                  # extract opcode from rINST
    sh  a2, MIRROR_SHORT_ARRAY_DATA_OFFSET(a0)        # vBB[vCC] <- a2
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget: /* 0x52 */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGet32InstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGet32InstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iget_wide: /* 0x53 */
/* File: mips64/op_iget_wide.S */
    /*
     * 64-bit instance field get.
     *
     * for: iget-wide
     */
    .extern artGet64InstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGet64InstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    SET_VREG_WIDE v0, a2                # fp[A] <- v0
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iget_object: /* 0x54 */
/* File: mips64/op_iget_object.S */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGetObjInstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGetObjInstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 1
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_boolean: /* 0x55 */
/* File: mips64/op_iget_boolean.S */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGetBooleanInstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGetBooleanInstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_byte: /* 0x56 */
/* File: mips64/op_iget_byte.S */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGetByteInstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGetByteInstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_char: /* 0x57 */
/* File: mips64/op_iget_char.S */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGetCharInstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGetCharInstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_short: /* 0x58 */
/* File: mips64/op_iget_short.S */
/* File: mips64/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    .extern artGetShortInstanceFromCode
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ld       a2, OFF_FP_METHOD(rFP)     # a2 <- referrer
    move     a3, rSELF                  # a3 <- self
    jal      artGetShortInstanceFromCode
    ld       a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext      a2, rINST, 8, 4            # a2 <- A
    PREFETCH_INST 2
    bnez     a3, MterpPossibleException # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    .else
    SET_VREG v0, a2                     # fp[A] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput: /* 0x59 */
/* File: mips64/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field//CCCC */
    .extern artSet32InstanceFromMterp
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref CCCC
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG a2, a2                     # a2 <- fp[A]
    ld      a3, OFF_FP_METHOD(rFP)      # a3 <- referrer
    PREFETCH_INST 2
    jal     artSet32InstanceFromMterp
    bnez    v0, MterpPossibleException  # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_wide: /* 0x5a */
/* File: mips64/op_iput_wide.S */
    /* iput-wide vA, vB, field//CCCC */
    .extern artSet64InstanceFromMterp
    EXPORT_PC
    lhu      a0, 2(rPC)                 # a0 <- field ref CCCC
    srl      a1, rINST, 12              # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext      a2, rINST, 8, 4            # a2 <- A
    dlsa     a2, a2, rFP, 2             # a2 <- &fp[A]
    ld       a3, OFF_FP_METHOD(rFP)     # a3 <- referrer
    PREFETCH_INST 2
    jal      artSet64InstanceFromMterp
    bnez     v0, MterpPossibleException # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_object: /* 0x5b */
/* File: mips64/op_iput_object.S */
    .extern MterpIputObject
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    move    a3, rSELF
    jal     MterpIputObject
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_boolean: /* 0x5c */
/* File: mips64/op_iput_boolean.S */
/* File: mips64/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field//CCCC */
    .extern artSet8InstanceFromMterp
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref CCCC
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG a2, a2                     # a2 <- fp[A]
    ld      a3, OFF_FP_METHOD(rFP)      # a3 <- referrer
    PREFETCH_INST 2
    jal     artSet8InstanceFromMterp
    bnez    v0, MterpPossibleException  # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_byte: /* 0x5d */
/* File: mips64/op_iput_byte.S */
/* File: mips64/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field//CCCC */
    .extern artSet8InstanceFromMterp
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref CCCC
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG a2, a2                     # a2 <- fp[A]
    ld      a3, OFF_FP_METHOD(rFP)      # a3 <- referrer
    PREFETCH_INST 2
    jal     artSet8InstanceFromMterp
    bnez    v0, MterpPossibleException  # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_char: /* 0x5e */
/* File: mips64/op_iput_char.S */
/* File: mips64/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field//CCCC */
    .extern artSet16InstanceFromMterp
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref CCCC
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG a2, a2                     # a2 <- fp[A]
    ld      a3, OFF_FP_METHOD(rFP)      # a3 <- referrer
    PREFETCH_INST 2
    jal     artSet16InstanceFromMterp
    bnez    v0, MterpPossibleException  # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_short: /* 0x5f */
/* File: mips64/op_iput_short.S */
/* File: mips64/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field//CCCC */
    .extern artSet16InstanceFromMterp
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref CCCC
    srl     a1, rINST, 12               # a1 <- B
    GET_VREG_U a1, a1                   # a1 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    GET_VREG a2, a2                     # a2 <- fp[A]
    ld      a3, OFF_FP_METHOD(rFP)      # a3 <- referrer
    PREFETCH_INST 2
    jal     artSet16InstanceFromMterp
    bnez    v0, MterpPossibleException  # bail out
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sget: /* 0x60 */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGet32Static
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGet32Static
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0

/* ------------------------------ */
    .balign 128
.L_op_sget_wide: /* 0x61 */
/* File: mips64/op_sget_wide.S */
    /*
     * SGET_WIDE handler wrapper.
     *
     */
    /* sget-wide vAA, field//BBBB */
    .extern MterpGet64Static
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGet64Static
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a4, rINST, 8                # a4 <- AA
    bnez    a3, MterpException          # bail out
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG_WIDE v0, a4
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sget_object: /* 0x62 */
/* File: mips64/op_sget_object.S */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGetObjStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGetObjStatic
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 1
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_sget_boolean: /* 0x63 */
/* File: mips64/op_sget_boolean.S */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGetBooleanStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGetBooleanStatic
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    and v0, v0, 0xff
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_sget_byte: /* 0x64 */
/* File: mips64/op_sget_byte.S */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGetByteStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGetByteStatic
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    seb v0, v0
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_sget_char: /* 0x65 */
/* File: mips64/op_sget_char.S */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGetCharStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGetCharStatic
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    and v0, v0, 0xffff
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_sget_short: /* 0x66 */
/* File: mips64/op_sget_short.S */
/* File: mips64/op_sget.S */
    /*
     * General SGET handler wrapper.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field//BBBB */
    .extern MterpGetShortStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    ld      a1, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpGetShortStatic
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    srl     a2, rINST, 8                # a2 <- AA
    seh v0, v0
    PREFETCH_INST 2
    bnez    a3, MterpException          # bail out
    .if 0
    SET_VREG_OBJECT v0, a2              # fp[AA] <- v0
    .else
    SET_VREG v0, a2                     # fp[AA] <- v0
    .endif
    ADVANCE 2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_sput: /* 0x67 */
/* File: mips64/op_sput.S */
    /*
     * General SPUT handler wrapper.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field//BBBB */
    .extern MterpSet32Static
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a3, rINST, 8                # a3 <- AA
    GET_VREG a1, a3                     # a1 <- fp[AA]
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSet32Static
    bnezc   v0, MterpException          # 0 on success
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_wide: /* 0x68 */
/* File: mips64/op_sput_wide.S */
    /*
     * SPUT_WIDE handler wrapper.
     *
     */
    /* sput-wide vAA, field//BBBB */
    .extern MterpSet64Static
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a1, rINST, 8                # a2 <- AA
    dlsa    a1, a1, rFP, 2
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSet64Static
    bnezc   v0, MterpException          # 0 on success, -1 on failure
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_object: /* 0x69 */
/* File: mips64/op_sput_object.S */
    .extern MterpSputObject
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    move    a3, rSELF
    jal     MterpSputObject
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_boolean: /* 0x6a */
/* File: mips64/op_sput_boolean.S */
/* File: mips64/op_sput.S */
    /*
     * General SPUT handler wrapper.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field//BBBB */
    .extern MterpSetBooleanStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a3, rINST, 8                # a3 <- AA
    GET_VREG a1, a3                     # a1 <- fp[AA]
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSetBooleanStatic
    bnezc   v0, MterpException          # 0 on success
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_byte: /* 0x6b */
/* File: mips64/op_sput_byte.S */
/* File: mips64/op_sput.S */
    /*
     * General SPUT handler wrapper.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field//BBBB */
    .extern MterpSetByteStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a3, rINST, 8                # a3 <- AA
    GET_VREG a1, a3                     # a1 <- fp[AA]
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSetByteStatic
    bnezc   v0, MterpException          # 0 on success
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_char: /* 0x6c */
/* File: mips64/op_sput_char.S */
/* File: mips64/op_sput.S */
    /*
     * General SPUT handler wrapper.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field//BBBB */
    .extern MterpSetCharStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a3, rINST, 8                # a3 <- AA
    GET_VREG a1, a3                     # a1 <- fp[AA]
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSetCharStatic
    bnezc   v0, MterpException          # 0 on success
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_short: /* 0x6d */
/* File: mips64/op_sput_short.S */
/* File: mips64/op_sput.S */
    /*
     * General SPUT handler wrapper.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field//BBBB */
    .extern MterpSetShortStatic
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- field ref BBBB
    srl     a3, rINST, 8                # a3 <- AA
    GET_VREG a1, a3                     # a1 <- fp[AA]
    ld      a2, OFF_FP_METHOD(rFP)
    move    a3, rSELF
    PREFETCH_INST 2                     # Get next inst, but don't advance rPC
    jal     MterpSetShortStatic
    bnezc   v0, MterpException          # 0 on success
    ADVANCE 2                           # Past exception point - now advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual: /* 0x6e */
/* File: mips64/op_invoke_virtual.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtual
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeVirtual
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0

    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

/* ------------------------------ */
    .balign 128
.L_op_invoke_super: /* 0x6f */
/* File: mips64/op_invoke_super.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeSuper
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeSuper
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0

    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

/* ------------------------------ */
    .balign 128
.L_op_invoke_direct: /* 0x70 */
/* File: mips64/op_invoke_direct.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeDirect
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeDirect
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_static: /* 0x71 */
/* File: mips64/op_invoke_static.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeStatic
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeStatic
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_interface: /* 0x72 */
/* File: mips64/op_invoke_interface.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeInterface
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeInterface
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0

    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */

/* ------------------------------ */
    .balign 128
.L_op_return_void_no_barrier: /* 0x73 */
/* File: mips64/op_return_void_no_barrier.S */
    .extern MterpSuspendCheck
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    move    a0, rSELF
    and     ra, ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqzc   ra, 1f
    jal     MterpSuspendCheck           # (self)
1:
    li      a0, 0
    b       MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_range: /* 0x74 */
/* File: mips64/op_invoke_virtual_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeVirtualRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_super_range: /* 0x75 */
/* File: mips64/op_invoke_super_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeSuperRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeSuperRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_direct_range: /* 0x76 */
/* File: mips64/op_invoke_direct_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeDirectRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeDirectRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_static_range: /* 0x77 */
/* File: mips64/op_invoke_static_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeStaticRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeStaticRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_interface_range: /* 0x78 */
/* File: mips64/op_invoke_interface_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeInterfaceRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeInterfaceRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_unused_79: /* 0x79 */
/* File: mips64/op_unused_79.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_7a: /* 0x7a */
/* File: mips64/op_unused_7a.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_neg_int: /* 0x7b */
/* File: mips64/op_neg_int.S */
/* File: mips64/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    subu    a0, zero, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_not_int: /* 0x7c */
/* File: mips64/op_not_int.S */
/* File: mips64/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    nor     a0, zero, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_neg_long: /* 0x7d */
/* File: mips64/op_neg_long.S */
/* File: mips64/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * For: not-long, neg-long
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a3                # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    dsubu   a0, zero, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_not_long: /* 0x7e */
/* File: mips64/op_not_long.S */
/* File: mips64/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * For: not-long, neg-long
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a3                # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    nor     a0, zero, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_neg_float: /* 0x7f */
/* File: mips64/op_neg_float.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    neg.s   f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_neg_double: /* 0x80 */
/* File: mips64/op_neg_double.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    neg.d   f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_int_to_long: /* 0x81 */
/* File: mips64/op_int_to_long.S */
    /* int-to-long vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB (sign-extended to 64 bits)
    ext     a2, rINST, 8, 4             # a2 <- A
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2                # vA <- vB
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_int_to_float: /* 0x82 */
/* File: mips64/op_int_to_float.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.s.w f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_int_to_double: /* 0x83 */
/* File: mips64/op_int_to_double.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.d.w f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_long_to_int: /* 0x84 */
/* File: mips64/op_long_to_int.S */
/* we ignore the high word, making this equivalent to a 32-bit reg move */
/* File: mips64/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_VREG a0, a3                     # a0 <- vB
    GET_INST_OPCODE v0                  # extract opcode from rINST
    .if 0
    SET_VREG_OBJECT a0, a2              # vA <- vB
    .else
    SET_VREG a0, a2                     # vA <- vB
    .endif
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_long_to_float: /* 0x85 */
/* File: mips64/op_long_to_float.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.s.l f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_long_to_double: /* 0x86 */
/* File: mips64/op_long_to_double.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.d.l f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_float_to_int: /* 0x87 */
/* File: mips64/op_float_to_int.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    trunc.w.s f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_float_to_long: /* 0x88 */
/* File: mips64/op_float_to_long.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    trunc.l.s f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_float_to_double: /* 0x89 */
/* File: mips64/op_float_to_double.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_FLOAT f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.d.s f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_double_to_int: /* 0x8a */
/* File: mips64/op_double_to_int.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    trunc.w.d f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_double_to_long: /* 0x8b */
/* File: mips64/op_double_to_long.S */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    trunc.l.d f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_double_to_float: /* 0x8c */
/* File: mips64/op_double_to_float.S */
    /*
     * Conversion from or to floating-point happens in a floating-point register.
     * Therefore we load the input and store the output into or from a
     * floating-point register irrespective of the type.
     */
/* File: mips64/fcvtHeader.S */
    /*
     * Loads a specified register from vB. Used primarily for conversions
     * from or to a floating-point type.
     *
     * Sets up a1 = A and a2 = B. a2 is later used by fcvtFooter.S to
     * store the result in vA and jump to the next instruction.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     */
    ext     a1, rINST, 8, 4             # a1 <- A
    srl     a2, rINST, 12               # a2 <- B
    GET_VREG_DOUBLE f0, a2
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST

    cvt.s.d f0, f0
/* File: mips64/fcvtFooter.S */
    /*
     * Stores a specified register containing the result of conversion
     * from or to a floating-point type and jumps to the next instruction.
     *
     * Expects a1 to contain the destination Dalvik register number.
     * a1 is set up by fcvtHeader.S.
     *
     * For: int-to-float, int-to-double, long-to-float, long-to-double,
     *      float-to-int, float-to-long, float-to-double, double-to-int,
     *      double-to-long, double-to-float, neg-float, neg-double.
     *
     * Note that this file can't be included after a break in other files
     * and in those files its contents appear as a copy.
     * See: float-to-int, float-to-long, double-to-int, double-to-long.
     */
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a1
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_int_to_byte: /* 0x8d */
/* File: mips64/op_int_to_byte.S */
/* File: mips64/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    seb     a0, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_int_to_char: /* 0x8e */
/* File: mips64/op_int_to_char.S */
/* File: mips64/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    and     a0, a0, 0xffff                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_int_to_short: /* 0x8f */
/* File: mips64/op_int_to_short.S */
/* File: mips64/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "a0 = op a0".
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      not-int, neg-int
     */
    /* unop vA, vB */
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    ext     a2, rINST, 8, 4             # a2 <- A
                               # optional op
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    seh     a0, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                     # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_add_int: /* 0x90 */
/* File: mips64/op_add_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    addu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_int: /* 0x91 */
/* File: mips64/op_sub_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    subu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_int: /* 0x92 */
/* File: mips64/op_mul_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_int: /* 0x93 */
/* File: mips64/op_div_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    div a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_int: /* 0x94 */
/* File: mips64/op_rem_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_and_int: /* 0x95 */
/* File: mips64/op_and_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_or_int: /* 0x96 */
/* File: mips64/op_or_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_xor_int: /* 0x97 */
/* File: mips64/op_xor_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shl_int: /* 0x98 */
/* File: mips64/op_shl_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    sll a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shr_int: /* 0x99 */
/* File: mips64/op_shr_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    sra a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_ushr_int: /* 0x9a */
/* File: mips64/op_ushr_int.S */
/* File: mips64/binop.S */
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG a0, a2                     # a0 <- vBB
    GET_VREG a1, a3                     # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    srl a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a4                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_add_long: /* 0x9b */
/* File: mips64/op_add_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    daddu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_long: /* 0x9c */
/* File: mips64/op_sub_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dsubu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_long: /* 0x9d */
/* File: mips64/op_mul_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dmul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_long: /* 0x9e */
/* File: mips64/op_div_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    ddiv a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_long: /* 0x9f */
/* File: mips64/op_rem_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dmod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_and_long: /* 0xa0 */
/* File: mips64/op_and_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_or_long: /* 0xa1 */
/* File: mips64/op_or_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_xor_long: /* 0xa2 */
/* File: mips64/op_xor_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shl_long: /* 0xa3 */
/* File: mips64/op_shl_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dsll a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shr_long: /* 0xa4 */
/* File: mips64/op_shr_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dsra a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_ushr_long: /* 0xa5 */
/* File: mips64/op_ushr_long.S */
/* File: mips64/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long, sub-long, mul-long, div-long, rem-long, and-long, or-long,
     *      xor-long, shl-long, shr-long, ushr-long
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_WIDE a0, a2                # a0 <- vBB
    GET_VREG_WIDE a1, a3                # a1 <- vCC
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    dsrl a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a4           # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_add_float: /* 0xa6 */
/* File: mips64/op_add_float.S */
/* File: mips64/fbinop.S */
    /*:
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    add.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a4               # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_float: /* 0xa7 */
/* File: mips64/op_sub_float.S */
/* File: mips64/fbinop.S */
    /*:
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    sub.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a4               # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_float: /* 0xa8 */
/* File: mips64/op_mul_float.S */
/* File: mips64/fbinop.S */
    /*:
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    mul.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a4               # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_float: /* 0xa9 */
/* File: mips64/op_div_float.S */
/* File: mips64/fbinop.S */
    /*:
     * Generic 32-bit floating-point operation.
     *
     * For: add-float, sub-float, mul-float, div-float.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f0, a2               # f0 <- vBB
    GET_VREG_FLOAT f1, a3               # f1 <- vCC
    div.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a4               # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_float: /* 0xaa */
/* File: mips64/op_rem_float.S */
    /* rem-float vAA, vBB, vCC */
    .extern fmodf
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_FLOAT f12, a2              # f12 <- vBB
    GET_VREG_FLOAT f13, a3              # f13 <- vCC
    jal     fmodf                       # f0 <- f12 op f13
    srl     a4, rINST, 8                # a4 <- AA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a4               # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_add_double: /* 0xab */
/* File: mips64/op_add_double.S */
/* File: mips64/fbinopWide.S */
    /*:
     * Generic 64-bit floating-point operation.
     *
     * For: add-double, sub-double, mul-double, div-double.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    add.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a4              # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_double: /* 0xac */
/* File: mips64/op_sub_double.S */
/* File: mips64/fbinopWide.S */
    /*:
     * Generic 64-bit floating-point operation.
     *
     * For: add-double, sub-double, mul-double, div-double.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    sub.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a4              # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_double: /* 0xad */
/* File: mips64/op_mul_double.S */
/* File: mips64/fbinopWide.S */
    /*:
     * Generic 64-bit floating-point operation.
     *
     * For: add-double, sub-double, mul-double, div-double.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    mul.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a4              # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_double: /* 0xae */
/* File: mips64/op_div_double.S */
/* File: mips64/fbinopWide.S */
    /*:
     * Generic 64-bit floating-point operation.
     *
     * For: add-double, sub-double, mul-double, div-double.
     * form: <op> f0, f0, f1
     */
    /* binop vAA, vBB, vCC */
    srl     a4, rINST, 8                # a4 <- AA
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f0, a2              # f0 <- vBB
    GET_VREG_DOUBLE f1, a3              # f1 <- vCC
    div.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a4              # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_double: /* 0xaf */
/* File: mips64/op_rem_double.S */
    /* rem-double vAA, vBB, vCC */
    .extern fmod
    lbu     a2, 2(rPC)                  # a2 <- BB
    lbu     a3, 3(rPC)                  # a3 <- CC
    GET_VREG_DOUBLE f12, a2             # f12 <- vBB
    GET_VREG_DOUBLE f13, a3             # f13 <- vCC
    jal     fmod                        # f0 <- f12 op f13
    srl     a4, rINST, 8                # a4 <- AA
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a4              # vAA <- f0
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_add_int_2addr: /* 0xb0 */
/* File: mips64/op_add_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    addu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_int_2addr: /* 0xb1 */
/* File: mips64/op_sub_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    subu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_int_2addr: /* 0xb2 */
/* File: mips64/op_mul_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    mul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_int_2addr: /* 0xb3 */
/* File: mips64/op_div_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    div a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_int_2addr: /* 0xb4 */
/* File: mips64/op_rem_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    mod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_and_int_2addr: /* 0xb5 */
/* File: mips64/op_and_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_or_int_2addr: /* 0xb6 */
/* File: mips64/op_or_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_xor_int_2addr: /* 0xb7 */
/* File: mips64/op_xor_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shl_int_2addr: /* 0xb8 */
/* File: mips64/op_shl_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    sll a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shr_int_2addr: /* 0xb9 */
/* File: mips64/op_shr_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    sra a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_ushr_int_2addr: /* 0xba */
/* File: mips64/op_ushr_int_2addr.S */
/* File: mips64/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a2                     # a0 <- vA
    GET_VREG a1, a3                     # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    srl a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_add_long_2addr: /* 0xbb */
/* File: mips64/op_add_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    daddu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_long_2addr: /* 0xbc */
/* File: mips64/op_sub_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dsubu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_long_2addr: /* 0xbd */
/* File: mips64/op_mul_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dmul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_long_2addr: /* 0xbe */
/* File: mips64/op_div_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    ddiv a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_long_2addr: /* 0xbf */
/* File: mips64/op_rem_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dmod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_and_long_2addr: /* 0xc0 */
/* File: mips64/op_and_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_or_long_2addr: /* 0xc1 */
/* File: mips64/op_or_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_xor_long_2addr: /* 0xc2 */
/* File: mips64/op_xor_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shl_long_2addr: /* 0xc3 */
/* File: mips64/op_shl_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dsll a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_shr_long_2addr: /* 0xc4 */
/* File: mips64/op_shr_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dsra a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_ushr_long_2addr: /* 0xc5 */
/* File: mips64/op_ushr_long_2addr.S */
/* File: mips64/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (LONG_MIN / -1) here, because the CPU handles it
     * correctly.
     *
     * For: add-long/2addr, sub-long/2addr, mul-long/2addr, div-long/2addr,
     *      rem-long/2addr, and-long/2addr, or-long/2addr, xor-long/2addr,
     *      shl-long/2addr, shr-long/2addr, ushr-long/2addr
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_WIDE a0, a2                # a0 <- vA
    GET_VREG_WIDE a1, a3                # a1 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
                               # optional op
    dsrl a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_WIDE a0, a2           # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_add_float_2addr: /* 0xc6 */
/* File: mips64/op_add_float_2addr.S */
/* File: mips64/fbinop2addr.S */
    /*:
     * Generic 32-bit "/2addr" floating-point operation.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_FLOAT f0, a2               # f0 <- vA
    GET_VREG_FLOAT f1, a3               # f1 <- vB
    add.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a2               # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_float_2addr: /* 0xc7 */
/* File: mips64/op_sub_float_2addr.S */
/* File: mips64/fbinop2addr.S */
    /*:
     * Generic 32-bit "/2addr" floating-point operation.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_FLOAT f0, a2               # f0 <- vA
    GET_VREG_FLOAT f1, a3               # f1 <- vB
    sub.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a2               # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_float_2addr: /* 0xc8 */
/* File: mips64/op_mul_float_2addr.S */
/* File: mips64/fbinop2addr.S */
    /*:
     * Generic 32-bit "/2addr" floating-point operation.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_FLOAT f0, a2               # f0 <- vA
    GET_VREG_FLOAT f1, a3               # f1 <- vB
    mul.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a2               # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_float_2addr: /* 0xc9 */
/* File: mips64/op_div_float_2addr.S */
/* File: mips64/fbinop2addr.S */
    /*:
     * Generic 32-bit "/2addr" floating-point operation.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr, div-float/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_FLOAT f0, a2               # f0 <- vA
    GET_VREG_FLOAT f1, a3               # f1 <- vB
    div.s f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a2               # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_float_2addr: /* 0xca */
/* File: mips64/op_rem_float_2addr.S */
    /* rem-float/2addr vA, vB */
    .extern fmodf
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_FLOAT f12, a2              # f12 <- vA
    GET_VREG_FLOAT f13, a3              # f13 <- vB
    jal     fmodf                       # f0 <- f12 op f13
    ext     a2, rINST, 8, 4             # a2 <- A
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_FLOAT f0, a2               # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_add_double_2addr: /* 0xcb */
/* File: mips64/op_add_double_2addr.S */
/* File: mips64/fbinopWide2addr.S */
    /*:
     * Generic 64-bit "/2addr" floating-point operation.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr, div-double/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_DOUBLE f0, a2              # f0 <- vA
    GET_VREG_DOUBLE f1, a3              # f1 <- vB
    add.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a2              # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sub_double_2addr: /* 0xcc */
/* File: mips64/op_sub_double_2addr.S */
/* File: mips64/fbinopWide2addr.S */
    /*:
     * Generic 64-bit "/2addr" floating-point operation.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr, div-double/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_DOUBLE f0, a2              # f0 <- vA
    GET_VREG_DOUBLE f1, a3              # f1 <- vB
    sub.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a2              # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_mul_double_2addr: /* 0xcd */
/* File: mips64/op_mul_double_2addr.S */
/* File: mips64/fbinopWide2addr.S */
    /*:
     * Generic 64-bit "/2addr" floating-point operation.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr, div-double/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_DOUBLE f0, a2              # f0 <- vA
    GET_VREG_DOUBLE f1, a3              # f1 <- vB
    mul.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a2              # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_div_double_2addr: /* 0xce */
/* File: mips64/op_div_double_2addr.S */
/* File: mips64/fbinopWide2addr.S */
    /*:
     * Generic 64-bit "/2addr" floating-point operation.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr, div-double/2addr.
     * form: <op> f0, f0, f1
     */
    /* binop/2addr vA, vB */
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_DOUBLE f0, a2              # f0 <- vA
    GET_VREG_DOUBLE f1, a3              # f1 <- vB
    div.d f0, f0, f1                              # f0 <- f0 op f1
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a2              # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_rem_double_2addr: /* 0xcf */
/* File: mips64/op_rem_double_2addr.S */
    /* rem-double/2addr vA, vB */
    .extern fmod
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG_DOUBLE f12, a2             # f12 <- vA
    GET_VREG_DOUBLE f13, a3             # f13 <- vB
    jal     fmod                        # f0 <- f12 op f13
    ext     a2, rINST, 8, 4             # a2 <- A
    FETCH_ADVANCE_INST 1                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG_DOUBLE f0, a2              # vA <- f0
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_add_int_lit16: /* 0xd0 */
/* File: mips64/op_add_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    addu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_rsub_int: /* 0xd1 */
/* File: mips64/op_rsub_int.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    subu a0, a1, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_mul_int_lit16: /* 0xd2 */
/* File: mips64/op_mul_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_div_int_lit16: /* 0xd3 */
/* File: mips64/op_div_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    div a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_rem_int_lit16: /* 0xd4 */
/* File: mips64/op_rem_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_and_int_lit16: /* 0xd5 */
/* File: mips64/op_and_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_or_int_lit16: /* 0xd6 */
/* File: mips64/op_or_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_xor_int_lit16: /* 0xd7 */
/* File: mips64/op_xor_int_lit16.S */
/* File: mips64/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CCCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    lh      a1, 2(rPC)                  # a1 <- sign-extended CCCC
    ext     a2, rINST, 8, 4             # a2 <- A
    ext     a3, rINST, 12, 4            # a3 <- B
    GET_VREG a0, a3                     # a0 <- vB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_add_int_lit8: /* 0xd8 */
/* File: mips64/op_add_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    addu a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_rsub_int_lit8: /* 0xd9 */
/* File: mips64/op_rsub_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    subu a0, a1, a0                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_mul_int_lit8: /* 0xda */
/* File: mips64/op_mul_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mul a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_div_int_lit8: /* 0xdb */
/* File: mips64/op_div_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    div a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_rem_int_lit8: /* 0xdc */
/* File: mips64/op_rem_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 1
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    mod a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_and_int_lit8: /* 0xdd */
/* File: mips64/op_and_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    and a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_or_int_lit8: /* 0xde */
/* File: mips64/op_or_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    or a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_xor_int_lit8: /* 0xdf */
/* File: mips64/op_xor_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    xor a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_shl_int_lit8: /* 0xe0 */
/* File: mips64/op_shl_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    sll a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_shr_int_lit8: /* 0xe1 */
/* File: mips64/op_shr_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    sra a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_ushr_int_lit8: /* 0xe2 */
/* File: mips64/op_ushr_int_lit8.S */
/* File: mips64/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * CC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    lbu     a3, 2(rPC)                  # a3 <- BB
    lb      a1, 3(rPC)                  # a1 <- sign-extended CC
    srl     a2, rINST, 8                # a2 <- AA
    GET_VREG a0, a3                     # a0 <- vBB
    .if 0
    beqz    a1, common_errDivideByZero  # is second operand zero?
    .endif
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
                               # optional op
    srl a0, a0, a1                              # a0 <- op, a0-a3 changed
    GET_INST_OPCODE v0                  # extract opcode from rINST
    SET_VREG a0, a2                # vAA <- a0
    GOTO_OPCODE v0                      # jump to next instruction



/* ------------------------------ */
    .balign 128
.L_op_iget_quick: /* 0xe3 */
/* File: mips64/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    lw   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iget_wide_quick: /* 0xe4 */
/* File: mips64/op_iget_wide_quick.S */
    /* iget-wide-quick vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a4, 2(rPC)                  # a4 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    daddu   a4, a3, a4                  # create direct pointer
    lw      a0, 0(a4)
    lw      a1, 4(a4)
    dinsu   a0, a1, 32, 32
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG_WIDE a0, a2
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iget_object_quick: /* 0xe5 */
/* File: mips64/op_iget_object_quick.S */
    /* For: iget-object-quick */
    /* op vA, vB, offset//CCCC */
    .extern artIGetObjectFromMterp
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    EXPORT_PC
    GET_VREG_U a0, a2                   # a0 <- object we're operating on
    jal     artIGetObjectFromMterp      # (obj, offset)
    ld      a3, THREAD_EXCEPTION_OFFSET(rSELF)
    ext     a2, rINST, 8, 4             # a2 <- A
    PREFETCH_INST 2
    bnez    a3, MterpPossibleException  # bail out
    SET_VREG_OBJECT v0, a2              # fp[A] <- v0
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_quick: /* 0xe6 */
/* File: mips64/op_iput_quick.S */
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    sw  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_wide_quick: /* 0xe7 */
/* File: mips64/op_iput_wide_quick.S */
    /* iput-wide-quick vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a3, 2(rPC)                  # a3 <- field byte offset
    GET_VREG_U a2, a2                   # a2 <- fp[B], the object pointer
    ext     a0, rINST, 8, 4             # a0 <- A
    beqz    a2, common_errNullObject    # object was null
    GET_VREG_WIDE a0, a0                # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a2, a3                  # create a direct pointer
    sw      a0, 0(a1)
    dsrl32  a0, a0, 0
    sw      a0, 4(a1)
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_object_quick: /* 0xe8 */
/* File: mips64/op_iput_object_quick.S */
    .extern MterpIputObjectQuick
    EXPORT_PC
    daddu   a0, rFP, OFF_FP_SHADOWFRAME
    move    a1, rPC
    move    a2, rINST
    jal     MterpIputObjectQuick
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_quick: /* 0xe9 */
/* File: mips64/op_invoke_virtual_quick.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualQuick
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeVirtualQuick
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_range_quick: /* 0xea */
/* File: mips64/op_invoke_virtual_range_quick.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualQuickRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeVirtualQuickRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_iput_boolean_quick: /* 0xeb */
/* File: mips64/op_iput_boolean_quick.S */
/* File: mips64/op_iput_quick.S */
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    sb  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_byte_quick: /* 0xec */
/* File: mips64/op_iput_byte_quick.S */
/* File: mips64/op_iput_quick.S */
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    sb  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_char_quick: /* 0xed */
/* File: mips64/op_iput_char_quick.S */
/* File: mips64/op_iput_quick.S */
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    sh  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_short_quick: /* 0xee */
/* File: mips64/op_iput_short_quick.S */
/* File: mips64/op_iput_quick.S */
    /* For: iput-quick, iput-boolean-quick, iput-byte-quick, iput-char-quick, iput-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- fp[B], the object pointer
    ext     a2, rINST, 8, 4             # a2 <- A
    beqz    a3, common_errNullObject    # object was null
    GET_VREG a0, a2                     # a0 <- fp[A]
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    daddu   a1, a1, a3
    sh  a0, 0(a1)                   # obj.field <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_boolean_quick: /* 0xef */
/* File: mips64/op_iget_boolean_quick.S */
/* File: mips64/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    lbu   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_byte_quick: /* 0xf0 */
/* File: mips64/op_iget_byte_quick.S */
/* File: mips64/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    lb   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_char_quick: /* 0xf1 */
/* File: mips64/op_iget_char_quick.S */
/* File: mips64/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    lhu   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_short_quick: /* 0xf2 */
/* File: mips64/op_iget_short_quick.S */
/* File: mips64/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset//CCCC */
    srl     a2, rINST, 12               # a2 <- B
    lhu     a1, 2(rPC)                  # a1 <- field byte offset
    GET_VREG_U a3, a2                   # a3 <- object we're operating on
    ext     a4, rINST, 8, 4             # a4 <- A
    daddu   a1, a1, a3
    beqz    a3, common_errNullObject    # object was null
    lh   a0, 0(a1)                   # a0 <- obj.field
    FETCH_ADVANCE_INST 2                # advance rPC, load rINST
    SET_VREG a0, a4                     # fp[A] <- a0
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_unused_f3: /* 0xf3 */
/* File: mips64/op_unused_f3.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f4: /* 0xf4 */
/* File: mips64/op_unused_f4.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f5: /* 0xf5 */
/* File: mips64/op_unused_f5.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f6: /* 0xf6 */
/* File: mips64/op_unused_f6.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f7: /* 0xf7 */
/* File: mips64/op_unused_f7.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f8: /* 0xf8 */
/* File: mips64/op_unused_f8.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f9: /* 0xf9 */
/* File: mips64/op_unused_f9.S */
/* File: mips64/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
    b       MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_invoke_polymorphic: /* 0xfa */
/* File: mips64/op_invoke_polymorphic.S */
/* File: mips64/invoke_polymorphic.S */
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern MterpInvokePolymorphic
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokePolymorphic
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 4
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_polymorphic_range: /* 0xfb */
/* File: mips64/op_invoke_polymorphic_range.S */
/* File: mips64/invoke_polymorphic.S */
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern MterpInvokePolymorphicRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokePolymorphicRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 4
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_custom: /* 0xfc */
/* File: mips64/op_invoke_custom.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeCustom
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeCustom
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_invoke_custom_range: /* 0xfd */
/* File: mips64/op_invoke_custom_range.S */
/* File: mips64/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeCustomRange
    .extern MterpShouldSwitchInterpreters
    EXPORT_PC
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    jal     MterpInvokeCustomRange
    beqzc   v0, MterpException
    FETCH_ADVANCE_INST 3
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    GET_INST_OPCODE v0
    GOTO_OPCODE v0


/* ------------------------------ */
    .balign 128
.L_op_const_method_handle: /* 0xfe */
/* File: mips64/op_const_method_handle.S */
/* File: mips64/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstMethodHandle
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     MterpConstMethodHandle                     # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_const_method_type: /* 0xff */
/* File: mips64/op_const_method_type.S */
/* File: mips64/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstMethodType
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     MterpConstMethodType                     # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction


    .balign 128
    .global artMterpAsmInstructionEnd
artMterpAsmInstructionEnd:

/*
 * ===========================================================================
 *  Sister implementations
 * ===========================================================================
 */
    .global artMterpAsmSisterStart
    .text
    .balign 4
artMterpAsmSisterStart:
    .global artMterpAsmSisterEnd
artMterpAsmSisterEnd:


    .global artMterpAsmAltInstructionStart
    .text

artMterpAsmAltInstructionStart = .L_ALT_op_nop
/* ------------------------------ */
    .balign 128
.L_ALT_op_nop: /* 0x00 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (0 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move: /* 0x01 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (1 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_from16: /* 0x02 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (2 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_16: /* 0x03 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (3 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide: /* 0x04 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (4 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide_from16: /* 0x05 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (5 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide_16: /* 0x06 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (6 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object: /* 0x07 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (7 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object_from16: /* 0x08 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (8 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object_16: /* 0x09 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (9 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result: /* 0x0a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (10 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result_wide: /* 0x0b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (11 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result_object: /* 0x0c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (12 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_exception: /* 0x0d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (13 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_void: /* 0x0e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (14 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_return: /* 0x0f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (15 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_wide: /* 0x10 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (16 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_object: /* 0x11 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (17 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_4: /* 0x12 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (18 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_16: /* 0x13 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (19 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const: /* 0x14 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (20 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_high16: /* 0x15 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (21 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_16: /* 0x16 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (22 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_32: /* 0x17 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (23 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide: /* 0x18 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (24 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_high16: /* 0x19 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (25 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_string: /* 0x1a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (26 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_string_jumbo: /* 0x1b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (27 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_class: /* 0x1c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (28 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_monitor_enter: /* 0x1d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (29 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_monitor_exit: /* 0x1e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (30 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_check_cast: /* 0x1f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (31 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_instance_of: /* 0x20 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (32 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_array_length: /* 0x21 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (33 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_new_instance: /* 0x22 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (34 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_new_array: /* 0x23 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (35 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_filled_new_array: /* 0x24 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (36 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_filled_new_array_range: /* 0x25 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (37 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_fill_array_data: /* 0x26 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (38 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_throw: /* 0x27 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (39 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto: /* 0x28 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (40 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto_16: /* 0x29 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (41 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto_32: /* 0x2a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (42 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_packed_switch: /* 0x2b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (43 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sparse_switch: /* 0x2c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (44 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpl_float: /* 0x2d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (45 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpg_float: /* 0x2e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (46 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpl_double: /* 0x2f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (47 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpg_double: /* 0x30 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (48 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmp_long: /* 0x31 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (49 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_eq: /* 0x32 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (50 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ne: /* 0x33 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (51 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_lt: /* 0x34 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (52 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ge: /* 0x35 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (53 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gt: /* 0x36 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (54 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_le: /* 0x37 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (55 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_eqz: /* 0x38 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (56 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_nez: /* 0x39 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (57 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ltz: /* 0x3a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (58 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gez: /* 0x3b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (59 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gtz: /* 0x3c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (60 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_lez: /* 0x3d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (61 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_3e: /* 0x3e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (62 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_3f: /* 0x3f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (63 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_40: /* 0x40 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (64 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_41: /* 0x41 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (65 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_42: /* 0x42 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (66 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_43: /* 0x43 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (67 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget: /* 0x44 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (68 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_wide: /* 0x45 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (69 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_object: /* 0x46 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (70 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_boolean: /* 0x47 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (71 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_byte: /* 0x48 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (72 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_char: /* 0x49 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (73 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_short: /* 0x4a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (74 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput: /* 0x4b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (75 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_wide: /* 0x4c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (76 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_object: /* 0x4d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (77 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_boolean: /* 0x4e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (78 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_byte: /* 0x4f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (79 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_char: /* 0x50 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (80 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_short: /* 0x51 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (81 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget: /* 0x52 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (82 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_wide: /* 0x53 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (83 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_object: /* 0x54 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (84 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_boolean: /* 0x55 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (85 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_byte: /* 0x56 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (86 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_char: /* 0x57 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (87 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_short: /* 0x58 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (88 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput: /* 0x59 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (89 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_wide: /* 0x5a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (90 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_object: /* 0x5b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (91 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_boolean: /* 0x5c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (92 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_byte: /* 0x5d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (93 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_char: /* 0x5e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (94 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_short: /* 0x5f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (95 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget: /* 0x60 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (96 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_wide: /* 0x61 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (97 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_object: /* 0x62 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (98 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_boolean: /* 0x63 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (99 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_byte: /* 0x64 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (100 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_char: /* 0x65 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (101 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_short: /* 0x66 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (102 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput: /* 0x67 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (103 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_wide: /* 0x68 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (104 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_object: /* 0x69 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (105 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_boolean: /* 0x6a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (106 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_byte: /* 0x6b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (107 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_char: /* 0x6c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (108 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_short: /* 0x6d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (109 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual: /* 0x6e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (110 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_super: /* 0x6f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (111 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_direct: /* 0x70 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (112 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_static: /* 0x71 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (113 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_interface: /* 0x72 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (114 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_void_no_barrier: /* 0x73 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (115 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_range: /* 0x74 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (116 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_super_range: /* 0x75 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (117 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_direct_range: /* 0x76 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (118 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_static_range: /* 0x77 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (119 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_interface_range: /* 0x78 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (120 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_79: /* 0x79 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (121 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_7a: /* 0x7a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (122 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_int: /* 0x7b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (123 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_not_int: /* 0x7c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (124 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_long: /* 0x7d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (125 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_not_long: /* 0x7e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (126 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_float: /* 0x7f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (127 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_double: /* 0x80 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (128 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_long: /* 0x81 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (129 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_float: /* 0x82 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (130 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_double: /* 0x83 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (131 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_int: /* 0x84 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (132 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_float: /* 0x85 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (133 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_double: /* 0x86 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (134 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_int: /* 0x87 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (135 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_long: /* 0x88 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (136 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_double: /* 0x89 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (137 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_int: /* 0x8a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (138 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_long: /* 0x8b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (139 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_float: /* 0x8c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (140 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_byte: /* 0x8d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (141 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_char: /* 0x8e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (142 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_short: /* 0x8f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (143 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int: /* 0x90 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (144 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_int: /* 0x91 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (145 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int: /* 0x92 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (146 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int: /* 0x93 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (147 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int: /* 0x94 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (148 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int: /* 0x95 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (149 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int: /* 0x96 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (150 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int: /* 0x97 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (151 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int: /* 0x98 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (152 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int: /* 0x99 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (153 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int: /* 0x9a */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (154 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_long: /* 0x9b */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (155 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_long: /* 0x9c */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (156 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_long: /* 0x9d */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (157 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_long: /* 0x9e */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (158 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_long: /* 0x9f */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (159 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_long: /* 0xa0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (160 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_long: /* 0xa1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (161 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_long: /* 0xa2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (162 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_long: /* 0xa3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (163 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_long: /* 0xa4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (164 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_long: /* 0xa5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (165 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_float: /* 0xa6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (166 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_float: /* 0xa7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (167 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_float: /* 0xa8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (168 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_float: /* 0xa9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (169 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_float: /* 0xaa */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (170 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_double: /* 0xab */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (171 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_double: /* 0xac */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (172 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_double: /* 0xad */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (173 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_double: /* 0xae */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (174 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_double: /* 0xaf */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (175 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_2addr: /* 0xb0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (176 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_int_2addr: /* 0xb1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (177 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_2addr: /* 0xb2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (178 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_2addr: /* 0xb3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (179 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_2addr: /* 0xb4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (180 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_2addr: /* 0xb5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (181 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_2addr: /* 0xb6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (182 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_2addr: /* 0xb7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (183 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int_2addr: /* 0xb8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (184 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int_2addr: /* 0xb9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (185 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int_2addr: /* 0xba */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (186 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_long_2addr: /* 0xbb */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (187 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_long_2addr: /* 0xbc */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (188 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_long_2addr: /* 0xbd */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (189 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_long_2addr: /* 0xbe */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (190 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_long_2addr: /* 0xbf */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (191 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_long_2addr: /* 0xc0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (192 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_long_2addr: /* 0xc1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (193 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_long_2addr: /* 0xc2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (194 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_long_2addr: /* 0xc3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (195 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_long_2addr: /* 0xc4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (196 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_long_2addr: /* 0xc5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (197 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_float_2addr: /* 0xc6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (198 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_float_2addr: /* 0xc7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (199 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_float_2addr: /* 0xc8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (200 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_float_2addr: /* 0xc9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (201 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_float_2addr: /* 0xca */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (202 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_double_2addr: /* 0xcb */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (203 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_double_2addr: /* 0xcc */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (204 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_double_2addr: /* 0xcd */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (205 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_double_2addr: /* 0xce */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (206 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_double_2addr: /* 0xcf */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (207 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_lit16: /* 0xd0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (208 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rsub_int: /* 0xd1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (209 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_lit16: /* 0xd2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (210 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_lit16: /* 0xd3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (211 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_lit16: /* 0xd4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (212 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_lit16: /* 0xd5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (213 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_lit16: /* 0xd6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (214 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_lit16: /* 0xd7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (215 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_lit8: /* 0xd8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (216 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rsub_int_lit8: /* 0xd9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (217 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_lit8: /* 0xda */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (218 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_lit8: /* 0xdb */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (219 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_lit8: /* 0xdc */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (220 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_lit8: /* 0xdd */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (221 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_lit8: /* 0xde */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (222 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_lit8: /* 0xdf */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (223 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int_lit8: /* 0xe0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (224 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int_lit8: /* 0xe1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (225 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int_lit8: /* 0xe2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (226 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_quick: /* 0xe3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (227 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_wide_quick: /* 0xe4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (228 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_object_quick: /* 0xe5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (229 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_quick: /* 0xe6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (230 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_wide_quick: /* 0xe7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (231 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_object_quick: /* 0xe8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (232 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_quick: /* 0xe9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (233 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_range_quick: /* 0xea */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (234 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_boolean_quick: /* 0xeb */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (235 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_byte_quick: /* 0xec */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (236 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_char_quick: /* 0xed */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (237 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_short_quick: /* 0xee */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (238 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_boolean_quick: /* 0xef */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (239 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_byte_quick: /* 0xf0 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (240 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_char_quick: /* 0xf1 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (241 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_short_quick: /* 0xf2 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (242 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f3: /* 0xf3 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (243 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f4: /* 0xf4 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (244 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f5: /* 0xf5 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (245 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f6: /* 0xf6 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (246 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f7: /* 0xf7 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (247 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f8: /* 0xf8 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (248 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f9: /* 0xf9 */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (249 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_polymorphic: /* 0xfa */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (250 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_polymorphic_range: /* 0xfb */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (251 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_custom: /* 0xfc */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (252 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_custom_range: /* 0xfd */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (253 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_method_handle: /* 0xfe */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (254 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_method_type: /* 0xff */
/* File: mips64/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (255 * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.

    .balign 128
    .global artMterpAsmAltInstructionEnd
artMterpAsmAltInstructionEnd:
/* File: mips64/footer.S */
/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */

    .extern MterpLogDivideByZeroException
common_errDivideByZero:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogDivideByZeroException
#endif
    b       MterpCommonFallback

    .extern MterpLogArrayIndexException
common_errArrayIndex:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogArrayIndexException
#endif
    b       MterpCommonFallback

    .extern MterpLogNullObjectException
common_errNullObject:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogNullObjectException
#endif
    b       MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    ld      a0, THREAD_EXCEPTION_OFFSET(rSELF)
    beqzc   a0, MterpFallback                       # If not, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
    .extern MterpHandleException
    .extern MterpShouldSwitchInterpreters
MterpException:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpHandleException                    # (self, shadow_frame)
    beqzc   v0, MterpExceptionReturn                # no local catch, back to caller.
    ld      a0, OFF_FP_DEX_INSTRUCTIONS(rFP)
    lwu     a1, OFF_FP_DEX_PC(rFP)
    REFRESH_IBASE
    dlsa    rPC, a1, a0, 1                          # generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    jal     MterpShouldSwitchInterpreters
    bnezc   v0, MterpFallback
    /* resume execution at catch block */
    EXPORT_PC
    FETCH_INST
    GET_INST_OPCODE v0
    GOTO_OPCODE v0
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
 *    rPROFILE       <= signed hotness countdown (expanded to 64 bits)
 *
 * We have quite a few different cases for branch profiling, OSR detection and
 * suspend check support here.
 *
 * Taken backward branches:
 *    If profiling active, do hotness countdown and report if we hit zero.
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *    Is there a pending suspend request?  If so, suspend.
 *
 * Taken forward branches and not-taken backward branches:
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *
 * Our most common case is expected to be a taken backward branch with active jit profiling,
 * but no full OSR check and no pending suspend request.
 * Next most common case is not-taken branch with no full OSR check.
 *
 */
MterpCommonTakenBranchNoFlags:
    bgtzc   rINST, .L_forward_branch    # don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
    li      v0, JIT_CHECK_OSR
    beqc    rPROFILE, v0, .L_osr_check
    bltc    rPROFILE, v0, .L_resume_backward_branch
    dsubu   rPROFILE, 1
    beqzc   rPROFILE, .L_add_batch      # counted down to zero - report
.L_resume_backward_branch:
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    REFRESH_IBASE
    daddu   a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB a2            # update rPC, load rINST
    and     ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    bnezc   ra, .L_suspend_request_pending
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC
    move    a0, rSELF
    jal     MterpSuspendCheck           # (self)
    bnezc   v0, MterpFallback
    REFRESH_IBASE                       # might have changed during suspend
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_no_count_backwards:
    li      v0, JIT_CHECK_OSR           # check for possible OSR re-entry
    bnec    rPROFILE, v0, .L_resume_backward_branch
.L_osr_check:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC
    jal MterpMaybeDoOnStackReplacement  # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    li      v0, JIT_CHECK_OSR           # check for possible OSR re-entry
    beqc    rPROFILE, v0, .L_check_osr_forward
.L_resume_forward_branch:
    daddu   a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB a2            # update rPC, load rINST
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

.L_check_osr_forward:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC
    jal     MterpMaybeDoOnStackReplacement # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    ld      a0, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    jal     MterpAddHotnessBatch        # (method, shadow_frame, self)
    move    rPROFILE, v0                # restore new hotness countdown to rPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    li      a2, 2
    EXPORT_PC
    jal     MterpMaybeDoOnStackReplacement # (self, shadow_frame, offset)
    bnezc   v0, MterpOnStackReplacement
    FETCH_ADVANCE_INST 2 
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST                               # rINST contains offset
    jal     MterpLogOSR
#endif
    li      v0, 1                                   # Signal normal return
    b       MterpDone

/*
 * Bail out to reference interpreter.
 */
    .extern MterpLogFallback
MterpFallback:
    EXPORT_PC
#if MTERP_LOGGING
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    jal     MterpLogFallback
#endif
MterpCommonFallback:
    li      v0, 0                                   # signal retry with reference interpreter.
    b       MterpDone

/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and RA.  Here we restore SP, restore the registers, and then restore
 * RA to PC.
 *
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    li      v0, 1                                   # signal return to caller.
    b       MterpDone
/*
 * Returned value is expected in a0 and if it's not 64-bit, the 32 most
 * significant bits of a0 must be zero-extended or sign-extended
 * depending on the return type.
 */
MterpReturn:
    ld      a2, OFF_FP_RESULT_REGISTER(rFP)
    sd      a0, 0(a2)
    li      v0, 1                                   # signal return to caller.
MterpDone:
/*
 * At this point, we expect rPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending rPROFILE and the cached hotness counter).  rPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    blez    rPROFILE, .L_pop_and_return # if > 0, we may have some counts to report.

MterpProfileActive:
    move    rINST, v0                   # stash return value
    /* Report cached hotness counts */
    ld      a0, OFF_FP_METHOD(rFP)
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    jal     MterpAddHotnessBatch        # (method, shadow_frame, self)
    move    v0, rINST                   # restore return value

.L_pop_and_return:
    ld      s6, STACK_OFFSET_S6(sp)
    .cfi_restore 22
    ld      s5, STACK_OFFSET_S5(sp)
    .cfi_restore 21
    ld      s4, STACK_OFFSET_S4(sp)
    .cfi_restore 20
    ld      s3, STACK_OFFSET_S3(sp)
    .cfi_restore 19
    ld      s2, STACK_OFFSET_S2(sp)
    .cfi_restore 18
    ld      s1, STACK_OFFSET_S1(sp)
    .cfi_restore 17
    ld      s0, STACK_OFFSET_S0(sp)
    .cfi_restore 16

    ld      ra, STACK_OFFSET_RA(sp)
    .cfi_restore 31

    ld      t8, STACK_OFFSET_GP(sp)
    .cpreturn
    .cfi_restore 28

    .set    noreorder
    jr      ra
    daddu   sp, sp, STACK_SIZE
    .cfi_adjust_cfa_offset -STACK_SIZE

    .cfi_endproc
    .set    reorder
    .size ExecuteMterpImpl, .-ExecuteMterpImpl

