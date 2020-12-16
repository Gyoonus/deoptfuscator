/*
 * This file was generated automatically by gen-mterp.py for 'mips'.
 *
 * --> DO NOT EDIT <--
 */

/* File: mips/header.S */
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
  Art assembly interpreter notes:

  First validate assembly code by implementing ExecuteXXXImpl() style body (doesn't
  handle invoke, allows higher-level code to create frame & shadow frame.

  Once that's working, support direct entry code & eliminate shadow frame (and
  excess locals allocation.

  Some (hopefully) temporary ugliness.  We'll treat rFP as pointing to the
  base of the vreg array within the shadow frame.  Access the other fields,
  dex_pc_, method_ and number_of_vregs_ via negative offsets.  For now, we'll continue
  the shadow frame mechanism of double-storing object references - via rFP &
  number_of_vregs_.

 */

#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

#if (__mips==32) && (__mips_isa_rev>=2)
#define MIPS32REVGE2    /* mips32r2 and greater */
#if (__mips==32) && (__mips_isa_rev>=5)
#define FPU64           /* 64 bit FPU */
#if (__mips==32) && (__mips_isa_rev>=6)
#define MIPS32REVGE6    /* mips32r6 and greater */
#endif
#endif
#endif

/* MIPS definitions and declarations

   reg  nick      purpose
   s0   rPC       interpreted program counter, used for fetching instructions
   s1   rFP       interpreted frame pointer, used for accessing locals and args
   s2   rSELF     self (Thread) pointer
   s3   rIBASE    interpreted instruction base pointer, used for computed goto
   s4   rINST     first 16-bit code unit of current instruction
   s5   rOBJ      object pointer
   s6   rREFS     base of object references in shadow frame (ideally, we'll get rid of this later).
   s7   rTEMP     used as temp storage that can survive a function call
   s8   rPROFILE  branch profiling countdown

*/

/* single-purpose registers, given names for clarity */
#define rPC s0
#define CFI_DEX 16  // DWARF register number of the register holding dex-pc (s0).
#define CFI_TMP 4   // DWARF register number of the first argument register (a0).
#define rFP s1
#define rSELF s2
#define rIBASE s3
#define rINST s4
#define rOBJ s5
#define rREFS s6
#define rTEMP s7
#define rPROFILE s8

#define rARG0 a0
#define rARG1 a1
#define rARG2 a2
#define rARG3 a3
#define rRESULT0 v0
#define rRESULT1 v1

/* GP register definitions */
#define zero    $0      /* always zero */
#define AT      $at     /* assembler temp */
#define v0      $2      /* return value */
#define v1      $3
#define a0      $4      /* argument registers */
#define a1      $5
#define a2      $6
#define a3      $7
#define t0      $8      /* temp registers (not saved across subroutine calls) */
#define t1      $9
#define t2      $10
#define t3      $11
#define t4      $12
#define t5      $13
#define t6      $14
#define t7      $15
#define ta0     $12     /* alias */
#define ta1     $13
#define ta2     $14
#define ta3     $15
#define s0      $16     /* saved across subroutine calls (callee saved) */
#define s1      $17
#define s2      $18
#define s3      $19
#define s4      $20
#define s5      $21
#define s6      $22
#define s7      $23
#define t8      $24     /* two more temp registers */
#define t9      $25
#define k0      $26     /* kernel temporary */
#define k1      $27
#define gp      $28     /* global pointer */
#define sp      $29     /* stack pointer */
#define s8      $30     /* one more callee saved */
#define ra      $31     /* return address */

/* FP register definitions */
#define fv0    $f0
#define fv0f   $f1
#define fv1    $f2
#define fv1f   $f3
#define fa0    $f12
#define fa0f   $f13
#define fa1    $f14
#define fa1f   $f15
#define ft0    $f4
#define ft0f   $f5
#define ft1    $f6
#define ft1f   $f7
#define ft2    $f8
#define ft2f   $f9
#define ft3    $f10
#define ft3f   $f11
#define ft4    $f16
#define ft4f   $f17
#define ft5    $f18
#define ft5f   $f19
#define fs0    $f20
#define fs0f   $f21
#define fs1    $f22
#define fs1f   $f23
#define fs2    $f24
#define fs2f   $f25
#define fs3    $f26
#define fs3f   $f27
#define fs4    $f28
#define fs4f   $f29
#define fs5    $f30
#define fs5f   $f31

#ifndef MIPS32REVGE6
#define fcc0   $fcc0
#define fcc1   $fcc1
#endif

#ifdef MIPS32REVGE2
#define SEB(rd, rt) \
    seb       rd, rt
#define SEH(rd, rt) \
    seh       rd, rt
#define INSERT_HIGH_HALF(rd_lo, rt_hi) \
    ins       rd_lo, rt_hi, 16, 16
#else
#define SEB(rd, rt) \
    sll       rd, rt, 24; \
    sra       rd, rd, 24
#define SEH(rd, rt) \
    sll       rd, rt, 16; \
    sra       rd, rd, 16
/* Clobbers rt_hi on pre-R2. */
#define INSERT_HIGH_HALF(rd_lo, rt_hi) \
    sll       rt_hi, rt_hi, 16; \
    or        rd_lo, rt_hi
#endif

#ifdef FPU64
#define MOVE_TO_FPU_HIGH(r, flo, fhi) \
    mthc1     r, flo
#else
#define MOVE_TO_FPU_HIGH(r, flo, fhi) \
    mtc1      r, fhi
#endif

#ifdef MIPS32REVGE6
#define JR(rt) \
    jic       rt, 0
#define LSA(rd, rs, rt, sa) \
    .if sa; \
    lsa       rd, rs, rt, sa; \
    .else; \
    addu      rd, rs, rt; \
    .endif
#else
#define JR(rt) \
    jalr      zero, rt
#define LSA(rd, rs, rt, sa) \
    .if sa; \
    .set      push; \
    .set      noat; \
    sll       AT, rs, sa; \
    addu      rd, AT, rt; \
    .set      pop; \
    .else; \
    addu      rd, rs, rt; \
    .endif
#endif

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
#define EXPORT_PC() \
    sw        rPC, OFF_FP_DEX_PC_PTR(rFP)

#define EXPORT_DEX_PC(tmp) \
    lw        tmp, OFF_FP_DEX_INSTRUCTIONS(rFP); \
    sw        rPC, OFF_FP_DEX_PC_PTR(rFP); \
    subu      tmp, rPC, tmp; \
    sra       tmp, tmp, 1; \
    sw        tmp, OFF_FP_DEX_PC(rFP)

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
#define FETCH_INST() lhu rINST, (rPC)

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
 * to point to the next instruction.  "_count" is in 16-bit code units.
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC().)
 */
#define FETCH_ADVANCE_INST(_count) \
    lhu       rINST, ((_count)*2)(rPC); \
    addu      rPC, rPC, ((_count) * 2)

/*
 * Similar to FETCH_ADVANCE_INST, but does not update rPC.  Used to load
 * rINST ahead of possible exception point.  Be sure to manually advance rPC
 * later.
 */
#define PREFETCH_INST(_count) lhu rINST, ((_count)*2)(rPC)

/* Advance rPC by some number of code units. */
#define ADVANCE(_count) addu rPC, rPC, ((_count) * 2)

/*
 * Fetch the next instruction from an offset specified by rd.  Updates
 * rPC to point to the next instruction.  "rd" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.
 */
#define FETCH_ADVANCE_INST_RB(rd) \
    addu      rPC, rPC, rd; \
    lhu       rINST, (rPC)

/*
 * Fetch a half-word code unit from an offset past the current PC.  The
 * "_count" value is in 16-bit code units.  Does not advance rPC.
 *
 * The "_S" variant works the same but treats the value as signed.
 */
#define FETCH(rd, _count) lhu rd, ((_count) * 2)(rPC)
#define FETCH_S(rd, _count) lh rd, ((_count) * 2)(rPC)

/*
 * Fetch one byte from an offset past the current PC.  Pass in the same
 * "_count" as you would for FETCH, and an additional 0/1 indicating which
 * byte of the halfword you want (lo/hi).
 */
#define FETCH_B(rd, _count, _byte) lbu rd, ((_count) * 2 + _byte)(rPC)

/*
 * Put the instruction's opcode field into the specified register.
 */
#define GET_INST_OPCODE(rd) and rd, rINST, 0xFF

/*
 * Transform opcode into branch target address.
 */
#define GET_OPCODE_TARGET(rd) \
    sll       rd, rd, 7; \
    addu      rd, rIBASE, rd

/*
 * Begin executing the opcode in rd.
 */
#define GOTO_OPCODE(rd) \
    GET_OPCODE_TARGET(rd); \
    JR(rd)

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
#define GET_VREG(rd, rix) LOAD_eas2(rd, rFP, rix)

#define GET_VREG_F(rd, rix) \
    .set noat; \
    EAS2(AT, rFP, rix); \
    l.s       rd, (AT); \
    .set at

#ifdef MIPS32REVGE6
#define SET_VREG(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8)
#else
#define SET_VREG(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG_OBJECT(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        rd, 0(t8)
#else
#define SET_VREG_OBJECT(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        rd, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG64(rlo, rhi, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#else
#define SET_VREG64(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG_F(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    s.s       rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8)
#else
#define SET_VREG_F(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG64_F(rlo, rhi, rix) \
    lsa       t8, rix, rFP, 2; \
    .set noat; \
    mfhc1     AT, rlo; \
    s.s       rlo, 0(t8); \
    sw        AT, 4(t8); \
    .set at; \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#elif defined(FPU64)
#define SET_VREG64_F(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rREFS, AT; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8); \
    addu      t8, rFP, AT; \
    mfhc1     AT, rlo; \
    sw        AT, 4(t8); \
    .set at; \
    s.s       rlo, 0(t8)
#else
#define SET_VREG64_F(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rlo, 0(t8); \
    s.s       rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#endif

/* Combination of the SET_VREG and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#else
#define SET_VREG_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG_OBJECT and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_OBJECT_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        rd, 0(t8); \
    .set reorder
#else
#define SET_VREG_OBJECT_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        rd, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG64 and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG64_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#else
#define SET_VREG64_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG_F and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_F_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    s.s       rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#else
#define SET_VREG_F_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG64_F and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    .set noat; \
    mfhc1     AT, rlo; \
    s.s       rlo, 0(t8); \
    sw        AT, 4(t8); \
    .set at; \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#elif defined(FPU64)
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rREFS, AT; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8); \
    addu      t8, rFP, AT; \
    mfhc1     AT, rlo; \
    sw        AT, 4(t8); \
    .set at; \
    jalr      zero, dst; \
    s.s       rlo, 0(t8); \
    .set reorder
#else
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rlo, 0(t8); \
    s.s       rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#endif

#define GET_OPA(rd) srl rd, rINST, 8
#ifdef MIPS32REVGE2
#define GET_OPA4(rd) ext rd, rINST, 8, 4
#else
#define GET_OPA4(rd) GET_OPA(rd); and rd, 0xf
#endif
#define GET_OPB(rd) srl rd, rINST, 12

/*
 * Form an Effective Address rd = rbase + roff<<shift;
 * Uses reg AT on pre-R6.
 */
#define EASN(rd, rbase, roff, shift) LSA(rd, roff, rbase, shift)

#define EAS1(rd, rbase, roff) EASN(rd, rbase, roff, 1)
#define EAS2(rd, rbase, roff) EASN(rd, rbase, roff, 2)
#define EAS3(rd, rbase, roff) EASN(rd, rbase, roff, 3)
#define EAS4(rd, rbase, roff) EASN(rd, rbase, roff, 4)

#define LOAD_eas2(rd, rbase, roff) \
    .set noat; \
    EAS2(AT, rbase, roff); \
    lw        rd, 0(AT); \
    .set at

#define STORE_eas2(rd, rbase, roff) \
    .set noat; \
    EAS2(AT, rbase, roff); \
    sw        rd, 0(AT); \
    .set at

#define LOAD_RB_OFF(rd, rbase, off) lw rd, off(rbase)
#define STORE_RB_OFF(rd, rbase, off) sw rd, off(rbase)

#define STORE64_off(rlo, rhi, rbase, off) \
    sw        rlo, off(rbase); \
    sw        rhi, (off+4)(rbase)
#define LOAD64_off(rlo, rhi, rbase, off) \
    lw        rlo, off(rbase); \
    lw        rhi, (off+4)(rbase)

#define STORE64(rlo, rhi, rbase) STORE64_off(rlo, rhi, rbase, 0)
#define LOAD64(rlo, rhi, rbase) LOAD64_off(rlo, rhi, rbase, 0)

#ifdef FPU64
#define STORE64_off_F(rlo, rhi, rbase, off) \
    s.s       rlo, off(rbase); \
    .set noat; \
    mfhc1     AT, rlo; \
    sw        AT, (off+4)(rbase); \
    .set at
#define LOAD64_off_F(rlo, rhi, rbase, off) \
    l.s       rlo, off(rbase); \
    .set noat; \
    lw        AT, (off+4)(rbase); \
    mthc1     AT, rlo; \
    .set at
#else
#define STORE64_off_F(rlo, rhi, rbase, off) \
    s.s       rlo, off(rbase); \
    s.s       rhi, (off+4)(rbase)
#define LOAD64_off_F(rlo, rhi, rbase, off) \
    l.s       rlo, off(rbase); \
    l.s       rhi, (off+4)(rbase)
#endif

#define STORE64_F(rlo, rhi, rbase) STORE64_off_F(rlo, rhi, rbase, 0)
#define LOAD64_F(rlo, rhi, rbase) LOAD64_off_F(rlo, rhi, rbase, 0)


#define LOAD_base_offMirrorArray_length(rd, rbase) LOAD_RB_OFF(rd, rbase, MIRROR_ARRAY_LENGTH_OFFSET)

#define STACK_STORE(rd, off) sw rd, off(sp)
#define STACK_LOAD(rd, off) lw rd, off(sp)
#define CREATE_STACK(n) subu sp, sp, n
#define DELETE_STACK(n) addu sp, sp, n

#define LOAD_ADDR(dest, addr) la dest, addr
#define LOAD_IMM(dest, imm) li dest, imm
#define MOVE_REG(dest, src) move dest, src
#define STACK_SIZE 128

#define STACK_OFFSET_ARG04 16
#define STACK_OFFSET_ARG05 20
#define STACK_OFFSET_ARG06 24
#define STACK_OFFSET_ARG07 28
#define STACK_OFFSET_GP    84

#define JAL(n) jal n
#define BAL(n) bal n

/*
 * FP register usage restrictions:
 * 1) We don't use the callee save FP registers so we don't have to save them.
 * 2) We don't use the odd FP registers so we can share code with mips32r6.
 */
#define STACK_STORE_FULL() CREATE_STACK(STACK_SIZE); \
    STACK_STORE(ra, 124); \
    STACK_STORE(s8, 120); \
    STACK_STORE(s0, 116); \
    STACK_STORE(s1, 112); \
    STACK_STORE(s2, 108); \
    STACK_STORE(s3, 104); \
    STACK_STORE(s4, 100); \
    STACK_STORE(s5, 96); \
    STACK_STORE(s6, 92); \
    STACK_STORE(s7, 88);

#define STACK_LOAD_FULL() STACK_LOAD(gp, STACK_OFFSET_GP); \
    STACK_LOAD(s7, 88); \
    STACK_LOAD(s6, 92); \
    STACK_LOAD(s5, 96); \
    STACK_LOAD(s4, 100); \
    STACK_LOAD(s3, 104); \
    STACK_LOAD(s2, 108); \
    STACK_LOAD(s1, 112); \
    STACK_LOAD(s0, 116); \
    STACK_LOAD(s8, 120); \
    STACK_LOAD(ra, 124); \
    DELETE_STACK(STACK_SIZE)

#define REFRESH_IBASE() \
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)

/* Constants for float/double_to_int/long conversions */
#define INT_MIN                 0x80000000
#define INT_MIN_AS_FLOAT        0xCF000000
#define INT_MIN_AS_DOUBLE_HIGH  0xC1E00000
#define LONG_MIN_HIGH           0x80000000
#define LONG_MIN_AS_FLOAT       0xDF000000
#define LONG_MIN_AS_DOUBLE_HIGH 0xC3E00000

/* File: mips/entry.S */
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
    .align 2
    .global ExecuteMterpImpl
    .ent    ExecuteMterpImpl
    .frame sp, STACK_SIZE, ra
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
    .set noreorder
    .cpload t9
    .set reorder
/* Save to the stack. Frame size = STACK_SIZE */
    STACK_STORE_FULL()
/* This directive will make sure all subsequent jal restore gp at a known offset */
    .cprestore STACK_OFFSET_GP

    /* Remember the return register */
    sw      a3, SHADOWFRAME_RESULT_REGISTER_OFFSET(a2)

    /* Remember the dex instruction pointer */
    sw      a1, SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET(a2)

    /* set up "named" registers */
    move    rSELF, a0
    lw      a0, SHADOWFRAME_NUMBER_OF_VREGS_OFFSET(a2)
    addu    rFP, a2, SHADOWFRAME_VREGS_OFFSET     # point to vregs.
    EAS2(rREFS, rFP, a0)                          # point to reference array in shadow frame
    lw      a0, SHADOWFRAME_DEX_PC_OFFSET(a2)     # Get starting dex_pc
    EAS1(rPC, a1, a0)                             # Create direct pointer to 1st dex opcode
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)

    EXPORT_PC()

    /* Starting ibase */
    lw      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)

    /* Set up for backwards branches & osr profiling */
    lw      a0, OFF_FP_METHOD(rFP)
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    JAL(MterpSetUpHotnessCountdown)        # (method, shadow_frame, self)
    move    rPROFILE, v0                   # Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST()                           # load rINST from rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction
    /* NOTE: no fallthrough */


    .global artMterpAsmInstructionStart
artMterpAsmInstructionStart = .L_op_nop
    .text

/* ------------------------------ */
    .balign 128
.L_op_nop: /* 0x00 */
/* File: mips/op_nop.S */
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_move: /* 0x01 */
/* File: mips/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[A] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2
    .endif

/* ------------------------------ */
    .balign 128
.L_op_move_from16: /* 0x02 */
/* File: mips/op_move_from16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AA] <- a2
    .endif

/* ------------------------------ */
    .balign 128
.L_op_move_16: /* 0x03 */
/* File: mips/op_move_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(a1, 2)                           #  a1 <- BBBB
    FETCH(a0, 1)                           #  a0 <- AAAA
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AAAA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AAAA] <- a2
    .endif

/* ------------------------------ */
    .balign 128
.L_op_move_wide: /* 0x04 */
/* File: mips/op_move_wide.S */
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    GET_OPA4(a2)                           #  a2 <- A(+)
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[B]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[A] <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_move_wide_from16: /* 0x05 */
/* File: mips/op_move_wide_from16.S */
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 1)                           #  a3 <- BBBB
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AA] <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_move_wide_16: /* 0x06 */
/* File: mips/op_move_wide_16.S */
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 2)                           #  a3 <- BBBB
    FETCH(a2, 1)                           #  a2 <- AAAA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AAAA] <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_move_object: /* 0x07 */
/* File: mips/op_move_object.S */
/* File: mips/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    .if 1
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[A] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2
    .endif


/* ------------------------------ */
    .balign 128
.L_op_move_object_from16: /* 0x08 */
/* File: mips/op_move_object_from16.S */
/* File: mips/op_move_from16.S */
    /* for: move/from16, move-object/from16 */
    /* op vAA, vBBBB */
    FETCH(a1, 1)                           #  a1 <- BBBB
    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 1
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AA] <- a2
    .endif


/* ------------------------------ */
    .balign 128
.L_op_move_object_16: /* 0x09 */
/* File: mips/op_move_object_16.S */
/* File: mips/op_move_16.S */
    /* for: move/16, move-object/16 */
    /* op vAAAA, vBBBB */
    FETCH(a1, 2)                           #  a1 <- BBBB
    FETCH(a0, 1)                           #  a0 <- AAAA
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[BBBB]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 1
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[AAAA] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[AAAA] <- a2
    .endif


/* ------------------------------ */
    .balign 128
.L_op_move_result: /* 0x0a */
/* File: mips/op_move_result.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    lw    a0, OFF_FP_RESULT_REGISTER(rFP)  #  get pointer to result JType
    lw    a0, 0(a0)                        #  a0 <- result.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(a0, a2, t0)       #  fp[AA] <- a0
    .else
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0
    .endif

/* ------------------------------ */
    .balign 128
.L_op_move_result_wide: /* 0x0b */
/* File: mips/op_move_result_wide.S */
    /* move-result-wide vAA */
    GET_OPA(a2)                            #  a2 <- AA
    lw    a3, OFF_FP_RESULT_REGISTER(rFP)  #  get pointer to result JType
    LOAD64(a0, a1, a3)                     #  a0/a1 <- retval.j
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[AA] <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_move_result_object: /* 0x0c */
/* File: mips/op_move_result_object.S */
/* File: mips/op_move_result.S */
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    lw    a0, OFF_FP_RESULT_REGISTER(rFP)  #  get pointer to result JType
    lw    a0, 0(a0)                        #  a0 <- result.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 1
    SET_VREG_OBJECT_GOTO(a0, a2, t0)       #  fp[AA] <- a0
    .else
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_move_exception: /* 0x0d */
/* File: mips/op_move_exception.S */
    /* move-exception vAA */
    GET_OPA(a2)                                 #  a2 <- AA
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)    #  get exception obj
    FETCH_ADVANCE_INST(1)                       #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                         #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    SET_VREG_OBJECT(a3, a2)                     #  fp[AA] <- exception obj
    sw    zero, THREAD_EXCEPTION_OFFSET(rSELF)  #  clear exception
    JR(t0)                                      #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_return_void: /* 0x0e */
/* File: mips/op_return_void.S */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    move      v0, zero
    move      v1, zero
    b         MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return: /* 0x0f */
/* File: mips/op_return.S */
    /*
     * Return a 32-bit value.
     *
     * for: return, return-object
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(v0, a2)                       #  v0 <- vAA
    move      v1, zero
    b         MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return_wide: /* 0x10 */
/* File: mips/op_return_wide.S */
    /*
     * Return a 64-bit value.
     */
    /* return-wide vAA */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    GET_OPA(a2)                            #  a2 <- AA
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AA]
    LOAD64(v0, v1, a2)                     #  v0/v1 <- vAA/vAA+1
    b         MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_return_object: /* 0x11 */
/* File: mips/op_return_object.S */
/* File: mips/op_return.S */
    /*
     * Return a 32-bit value.
     *
     * for: return, return-object
     */
    /* op vAA */
    .extern MterpThreadFenceForConstructor
    JAL(MterpThreadFenceForConstructor)
    lw        ra, THREAD_FLAGS_OFFSET(rSELF)
    move      a0, rSELF
    and       ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz      ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(v0, a2)                       #  v0 <- vAA
    move      v1, zero
    b         MterpReturn


/* ------------------------------ */
    .balign 128
.L_op_const_4: /* 0x12 */
/* File: mips/op_const_4.S */
    /* const/4 vA, +B */
    sll       a1, rINST, 16                #  a1 <- Bxxx0000
    GET_OPA(a0)                            #  a0 <- A+
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sra       a1, a1, 28                   #  a1 <- sssssssB (sign-extended)
    and       a0, a0, 15
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a1, a0, t0)              #  fp[A] <- a1

/* ------------------------------ */
    .balign 128
.L_op_const_16: /* 0x13 */
/* File: mips/op_const_16.S */
    /* const/16 vAA, +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

/* ------------------------------ */
    .balign 128
.L_op_const: /* 0x14 */
/* File: mips/op_const.S */
    /* const vAA, +BBBBbbbb */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

/* ------------------------------ */
    .balign 128
.L_op_const_high16: /* 0x15 */
/* File: mips/op_const_high16.S */
    /* const/high16 vAA, +BBBB0000 */
    FETCH(a0, 1)                           #  a0 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a0, a0, 16                   #  a0 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

/* ------------------------------ */
    .balign 128
.L_op_const_wide_16: /* 0x16 */
/* File: mips/op_const_wide_16.S */
    /* const-wide/16 vAA, +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sra       a1, a0, 31                   #  a1 <- ssssssss
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_const_wide_32: /* 0x17 */
/* File: mips/op_const_wide_32.S */
    /* const-wide/32 vAA, +BBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- 0000bbbb (low)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_S(a2, 2)                         #  a2 <- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    INSERT_HIGH_HALF(a0, a2)               #  a0 <- BBBBbbbb
    sra       a1, a0, 31                   #  a1 <- ssssssss
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_const_wide: /* 0x18 */
/* File: mips/op_const_wide.S */
    /* const-wide vAA, +HHHHhhhhBBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (low middle)
    FETCH(a2, 3)                           #  a2 <- hhhh (high middle)
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb (low word)
    FETCH(a3, 4)                           #  a3 <- HHHH (high)
    GET_OPA(t1)                            #  t1 <- AA
    INSERT_HIGH_HALF(a2, a3)               #  a2 <- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a2, t1, t0)        #  vAA/vAA+1 <- a0/a2

/* ------------------------------ */
    .balign 128
.L_op_const_wide_high16: /* 0x19 */
/* File: mips/op_const_wide_high16.S */
    /* const-wide/high16 vAA, +BBBB000000000000 */
    FETCH(a1, 1)                           #  a1 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    li        a0, 0                        #  a0 <- 00000000
    sll       a1, 16                       #  a1 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a3, t0)        #  vAA/vAA+1 <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_const_string: /* 0x1a */
/* File: mips/op_const_string.S */
/* File: mips/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstString
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstString)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_const_string_jumbo: /* 0x1b */
/* File: mips/op_const_string_jumbo.S */
    /* const/string vAA, string@BBBBBBBB */
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- bbbb (low)
    FETCH(a2, 2)                        # a2 <- BBBB (high)
    GET_OPA(a1)                         # a1 <- AA
    INSERT_HIGH_HALF(a0, a2)            # a0 <- BBBBbbbb
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstString)               # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(3)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(3)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_const_class: /* 0x1c */
/* File: mips/op_const_class.S */
/* File: mips/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstClass
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstClass)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_monitor_enter: /* 0x1d */
/* File: mips/op_monitor_enter.S */
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    EXPORT_PC()
    GET_OPA(a2)                            # a2 <- AA
    GET_VREG(a0, a2)                       # a0 <- vAA (object)
    move   a1, rSELF                       # a1 <- self
    JAL(artLockObjectFromCode)             # v0 <- artLockObject(obj, self)
    bnez v0, MterpException
    FETCH_ADVANCE_INST(1)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_monitor_exit: /* 0x1e */
/* File: mips/op_monitor_exit.S */
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    EXPORT_PC()
    GET_OPA(a2)                            # a2 <- AA
    GET_VREG(a0, a2)                       # a0 <- vAA (object)
    move   a1, rSELF                       # a1 <- self
    JAL(artUnlockObjectFromCode)           # v0 <- artUnlockObject(obj, self)
    bnez v0, MterpException
    FETCH_ADVANCE_INST(1)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_check_cast: /* 0x1f */
/* File: mips/op_check_cast.S */
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           #  a0 <- BBBB
    GET_OPA(a1)                            #  a1 <- AA
    EAS2(a1, rFP, a1)                      #  a1 <- &object
    lw     a2, OFF_FP_METHOD(rFP)          #  a2 <- method
    move   a3, rSELF                       #  a3 <- self
    JAL(MterpCheckCast)                    #  v0 <- CheckCast(index, &obj, method, self)
    PREFETCH_INST(2)
    bnez   v0, MterpPossibleException
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_instance_of: /* 0x20 */
/* File: mips/op_instance_of.S */
    /*
     * Check to see if an object reference is an instance of a class.
     *
     * Most common situation is a non-null object, being compared against
     * an already-resolved class.
     */
    /* instance-of vA, vB, class@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- CCCC
    GET_OPB(a1)                            # a1 <- B
    EAS2(a1, rFP, a1)                      # a1 <- &object
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    GET_OPA4(rOBJ)                         # rOBJ <- A+
    JAL(MterpInstanceOf)                   # v0 <- Mterp(index, &obj, method, self)
    lw   a1, THREAD_EXCEPTION_OFFSET(rSELF)
    PREFETCH_INST(2)                       # load rINST
    bnez a1, MterpException
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            # vA <- v0

/* ------------------------------ */
    .balign 128
.L_op_array_length: /* 0x21 */
/* File: mips/op_array_length.S */
    /*
     * Return the length of an array.
     */
    /* array-length vA, vB */
    GET_OPB(a1)                            #  a1 <- B
    GET_OPA4(a2)                           #  a2 <- A+
    GET_VREG(a0, a1)                       #  a0 <- vB (object ref)
    # is object null?
    beqz      a0, common_errNullObject     #  yup, fail
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- array length
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a3, a2, t0)              #  vA <- length

/* ------------------------------ */
    .balign 128
.L_op_new_instance: /* 0x22 */
/* File: mips/op_new_instance.S */
    /*
     * Create a new instance of a class.
     */
    /* new-instance vAA, class@BBBB */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rSELF
    move   a2, rINST
    JAL(MterpNewInstance)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_new_array: /* 0x23 */
/* File: mips/op_new_array.S */
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    move   a3, rSELF
    JAL(MterpNewArray)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_filled_new_array: /* 0x24 */
/* File: mips/op_filled_new_array.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    .extern MterpFilledNewArray
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME     # a0 <- shadow frame
    move   a1, rPC
    move   a2, rSELF
    JAL(MterpFilledNewArray)                           #  v0 <- helper(shadow_frame, pc, self)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_filled_new_array_range: /* 0x25 */
/* File: mips/op_filled_new_array_range.S */
/* File: mips/op_filled_new_array.S */
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    .extern MterpFilledNewArrayRange
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME     # a0 <- shadow frame
    move   a1, rPC
    move   a2, rSELF
    JAL(MterpFilledNewArrayRange)                           #  v0 <- helper(shadow_frame, pc, self)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_fill_array_data: /* 0x26 */
/* File: mips/op_fill_array_data.S */
    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC()
    FETCH(a1, 1)                           #  a1 <- bbbb (lo)
    FETCH(a0, 2)                           #  a0 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a1, a0)               #  a1 <- BBBBbbbb
    GET_VREG(a0, a3)                       #  a0 <- vAA (array object)
    EAS1(a1, rPC, a1)                      #  a1 <- PC + BBBBbbbb*2 (array data off.)
    JAL(MterpFillArrayData)                #  v0 <- Mterp(obj, payload)
    beqz      v0,  MterpPossibleException  #  has exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_throw: /* 0x27 */
/* File: mips/op_throw.S */
    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC()                              #  exception handler can throw
    GET_OPA(a2)                              #  a2 <- AA
    GET_VREG(a1, a2)                         #  a1 <- vAA (exception object)
    # null object?
    beqz  a1, common_errNullObject           #  yes, throw an NPE instead
    sw    a1, THREAD_EXCEPTION_OFFSET(rSELF) #  thread->exception <- obj
    b         MterpException

/* ------------------------------ */
    .balign 128
.L_op_goto: /* 0x28 */
/* File: mips/op_goto.S */
    /*
     * Unconditional branch, 8-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto +AA */
    sll       a0, rINST, 16                #  a0 <- AAxx0000
    sra       rINST, a0, 24                #  rINST <- ssssssAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_goto_16: /* 0x29 */
/* File: mips/op_goto_16.S */
    /*
     * Unconditional branch, 16-bit offset.
     *
     * The branch distance is a signed code-unit offset, which we need to
     * double to get a byte offset.
     */
    /* goto/16 +AAAA */
    FETCH_S(rINST, 1)                      #  rINST <- ssssAAAA (sign-extended)
    b       MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_goto_32: /* 0x2a */
/* File: mips/op_goto_32.S */
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
    FETCH(rINST, 1)                        #  rINST <- aaaa (lo)
    FETCH(a1, 2)                           #  a1 <- AAAA (hi)
    INSERT_HIGH_HALF(rINST, a1)            #  rINST <- AAAAaaaa
    b         MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_packed_switch: /* 0x2b */
/* File: mips/op_packed_switch.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL(MterpDoPackedSwitch)                             #  a0 <- code-unit branch offset
    move      rINST, v0
    b         MterpCommonTakenBranchNoFlags

/* ------------------------------ */
    .balign 128
.L_op_sparse_switch: /* 0x2c */
/* File: mips/op_sparse_switch.S */
/* File: mips/op_packed_switch.S */
    /*
     * Handle a packed-switch or sparse-switch instruction.  In both cases
     * we decode it and hand it off to a helper function.
     *
     * We don't really expect backward branches in a switch statement, but
     * they're perfectly legal, so we check for them here.
     *
     * for: packed-switch, sparse-switch
     */
    /* op vAA, +BBBB */
    FETCH(a0, 1)                           #  a0 <- bbbb (lo)
    FETCH(a1, 2)                           #  a1 <- BBBB (hi)
    GET_OPA(a3)                            #  a3 <- AA
    INSERT_HIGH_HALF(a0, a1)               #  a0 <- BBBBbbbb
    GET_VREG(a1, a3)                       #  a1 <- vAA
    EAS1(a0, rPC, a0)                      #  a0 <- PC + BBBBbbbb*2
    JAL(MterpDoSparseSwitch)                             #  a0 <- code-unit branch offset
    move      rINST, v0
    b         MterpCommonTakenBranchNoFlags


/* ------------------------------ */
    .balign 128
.L_op_cmpl_float: /* 0x2d */
/* File: mips/op_cmpl_float.S */
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
#ifdef MIPS32REVGE6
    cmp.eq.s  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if 0
    cmp.lt.s  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.s  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if 0
    c.olt.s   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

/* ------------------------------ */
    .balign 128
.L_op_cmpg_float: /* 0x2e */
/* File: mips/op_cmpg_float.S */
/* File: mips/op_cmpl_float.S */
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
#ifdef MIPS32REVGE6
    cmp.eq.s  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if 1
    cmp.lt.s  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.s  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if 1
    c.olt.s   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP


/* ------------------------------ */
    .balign 128
.L_op_cmpl_double: /* 0x2f */
/* File: mips/op_cmpl_double.S */
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  rOBJ <- BB
    srl       t0, a0, 8                    #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[BB]
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[CC]
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, t0)
#ifdef MIPS32REVGE6
    cmp.eq.d  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if 0
    cmp.lt.d  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.d  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if 0
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP

/* ------------------------------ */
    .balign 128
.L_op_cmpg_double: /* 0x30 */
/* File: mips/op_cmpg_double.S */
/* File: mips/op_cmpl_double.S */
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  rOBJ <- BB
    srl       t0, a0, 8                    #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[BB]
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[CC]
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, t0)
#ifdef MIPS32REVGE6
    cmp.eq.d  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if 1
    cmp.lt.d  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.d  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if 1
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.d   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP


/* ------------------------------ */
    .balign 128
.L_op_cmp_long: /* 0x31 */
/* File: mips/op_cmp_long.S */
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;        # (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;        # (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)                     #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, a3)                     #  a2/a3 <- vCC/vCC+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    slt       t0, a1, a3                   #  compare hi
    sgt       t1, a1, a3
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0)
    bnez      v0, .Lop_cmp_long_finish
    # at this point x.hi==y.hi
    sltu      t0, a0, a2                   #  compare lo
    sgtu      t1, a0, a2
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0) for [< > =]

.Lop_cmp_long_finish:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(v0, rOBJ, t0)            #  vAA <- v0

/* ------------------------------ */
    .balign 128
.L_op_if_eq: /* 0x32 */
/* File: mips/op_if_eq.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    beq a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ne: /* 0x33 */
/* File: mips/op_if_ne.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    bne a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_lt: /* 0x34 */
/* File: mips/op_if_lt.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    blt a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ge: /* 0x35 */
/* File: mips/op_if_ge.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    bge a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gt: /* 0x36 */
/* File: mips/op_if_gt.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    bgt a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_le: /* 0x37 */
/* File: mips/op_if_le.S */
/* File: mips/bincmp.S */
    /*
     * Generic two-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * For: if-eq, if-ne, if-lt, if-ge, if-gt, if-le
     */
    /* if-cmp vA, vB, +CCCC */
    GET_OPA4(a0)                           #  a0 <- A+
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a3, a1)                       #  a3 <- vB
    GET_VREG(a0, a0)                       #  a0 <- vA
    FETCH_S(rINST, 1)                      #  rINST<- branch offset, in code units
    ble a0, a3, MterpCommonTakenBranchNoFlags  #  compare (vA, vB)
    li        t0, JIT_CHECK_OSR
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_eqz: /* 0x38 */
/* File: mips/op_if_eqz.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    beq a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_nez: /* 0x39 */
/* File: mips/op_if_nez.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    bne a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_ltz: /* 0x3a */
/* File: mips/op_if_ltz.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    blt a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gez: /* 0x3b */
/* File: mips/op_if_gez.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    bge a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_gtz: /* 0x3c */
/* File: mips/op_if_gtz.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    bgt a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_if_lez: /* 0x3d */
/* File: mips/op_if_lez.S */
/* File: mips/zcmp.S */
    /*
     * Generic one-operand compare-and-branch operation.  Provide a "condition"
     * fragment that specifies the comparison to perform.
     *
     * for: if-eqz, if-nez, if-ltz, if-gez, if-gtz, if-lez
     */
    /* if-cmp vAA, +BBBB */
    GET_OPA(a0)                            #  a0 <- AA
    GET_VREG(a0, a0)                       #  a0 <- vAA
    FETCH_S(rINST, 1)                      #  rINST <- branch offset, in code units
    ble a0, zero, MterpCommonTakenBranchNoFlags
    li        t0, JIT_CHECK_OSR            # possible OSR re-entry?
    beq       rPROFILE, t0, .L_check_not_taken_osr
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_unused_3e: /* 0x3e */
/* File: mips/op_unused_3e.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_3f: /* 0x3f */
/* File: mips/op_unused_3f.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_40: /* 0x40 */
/* File: mips/op_unused_40.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_41: /* 0x41 */
/* File: mips/op_unused_41.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_42: /* 0x42 */
/* File: mips/op_unused_42.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_43: /* 0x43 */
/* File: mips/op_unused_43.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_aget: /* 0x44 */
/* File: mips/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 2)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lw a2, MIRROR_INT_ARRAY_DATA_OFFSET(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2

/* ------------------------------ */
    .balign 128
.L_op_aget_wide: /* 0x45 */
/* File: mips/op_aget_wide.S */
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     * Arrays of long/double are 64-bit aligned.
     */
    /* aget-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64_off(a2, a3, a0, MIRROR_WIDE_ARRAY_DATA_OFFSET)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a2, a3, rOBJ, t0)      #  vAA/vAA+1 <- a2/a3

/* ------------------------------ */
    .balign 128
.L_op_aget_object: /* 0x46 */
/* File: mips/op_aget_object.S */
    /*
     * Array object get.  vAA <- vBB[vCC].
     *
     * for: aget-object
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    EXPORT_PC()
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    JAL(artAGetObjectFromMterp)            #  v0 <- GetObj(array, index)
    lw   a1, THREAD_EXCEPTION_OFFSET(rSELF)
    PREFETCH_INST(2)                       #  load rINST
    bnez a1, MterpException
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_OBJECT_GOTO(v0, rOBJ, t0)     #  vAA <- v0

/* ------------------------------ */
    .balign 128
.L_op_aget_boolean: /* 0x47 */
/* File: mips/op_aget_boolean.S */
/* File: mips/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lbu a2, MIRROR_BOOLEAN_ARRAY_DATA_OFFSET(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2


/* ------------------------------ */
    .balign 128
.L_op_aget_byte: /* 0x48 */
/* File: mips/op_aget_byte.S */
/* File: mips/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lb a2, MIRROR_BYTE_ARRAY_DATA_OFFSET(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2


/* ------------------------------ */
    .balign 128
.L_op_aget_char: /* 0x49 */
/* File: mips/op_aget_char.S */
/* File: mips/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lhu a2, MIRROR_CHAR_ARRAY_DATA_OFFSET(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2


/* ------------------------------ */
    .balign 128
.L_op_aget_short: /* 0x4a */
/* File: mips/op_aget_short.S */
/* File: mips/op_aget.S */
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * Note: using the usual FETCH/and/shift stuff, this fits in exactly 17
     * instructions.  We use a pair of FETCH_Bs instead.
     *
     * for: aget, aget-boolean, aget-byte, aget-char, aget-short
     *
     * NOTE: assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    # a1 >= a3; compare unsigned index
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    lh a2, MIRROR_SHORT_ARRAY_DATA_OFFSET(a0)             #  a2 <- vBB[vCC]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a2, rOBJ, t0)            #  vAA <- a2


/* ------------------------------ */
    .balign 128
.L_op_aput: /* 0x4b */
/* File: mips/op_aput.S */

    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 2)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    sw a2, MIRROR_INT_ARRAY_DATA_OFFSET(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_wide: /* 0x4c */
/* File: mips/op_aput_wide.S */
    /*
     * Array put, 64 bits.  vBB[vCC] <- vAA.
     */
    /* aput-wide vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t0 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EAS3(a0, a0, a1)                       #  a0 <- arrayObj + index*width
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]
    # compare unsigned index, length
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    LOAD64(a2, a3, rOBJ)                   #  a2/a3 <- vAA/vAA+1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    STORE64_off(a2, a3, a0, MIRROR_WIDE_ARRAY_DATA_OFFSET) #  a2/a3 <- vBB[vCC]
    JR(t0)                                 #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_object: /* 0x4d */
/* File: mips/op_aput_object.S */
    /*
     * Store an object into an array.  vBB[vCC] <- vAA.
     *
     */
    /* op vAA, vBB, vCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    JAL(MterpAputObject)
    beqz   v0, MterpPossibleException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_aput_boolean: /* 0x4e */
/* File: mips/op_aput_boolean.S */
/* File: mips/op_aput.S */

    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    sb a2, MIRROR_BOOLEAN_ARRAY_DATA_OFFSET(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_byte: /* 0x4f */
/* File: mips/op_aput_byte.S */
/* File: mips/op_aput.S */

    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 0)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    sb a2, MIRROR_BYTE_ARRAY_DATA_OFFSET(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_char: /* 0x50 */
/* File: mips/op_aput_char.S */
/* File: mips/op_aput.S */

    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    sh a2, MIRROR_CHAR_ARRAY_DATA_OFFSET(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_aput_short: /* 0x51 */
/* File: mips/op_aput_short.S */
/* File: mips/op_aput.S */

    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     *
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     *
     * NOTE: this assumes data offset for arrays is the same for all non-wide types.
     * If this changes, specialize.
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1, 0)                      #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_B(a3, 1, 1)                      #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offMirrorArray_length(a3, a0) #  a3 <- arrayObj->length
    EASN(a0, a0, a1, 1)               #  a0 <- arrayObj + index*width
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    sh a2, MIRROR_SHORT_ARRAY_DATA_OFFSET(a0)            #  vBB[vCC] <- a2
    JR(t0)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget: /* 0x52 */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGet32InstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif

/* ------------------------------ */
    .balign 128
.L_op_iget_wide: /* 0x53 */
/* File: mips/op_iget_wide.S */
    /*
     * 64-bit instance field get.
     *
     * for: iget-wide
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field byte offset
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGet64InstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez a3, MterpException                # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, a2, t0)        # fp[A] <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_iget_object: /* 0x54 */
/* File: mips/op_iget_object.S */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGetObjInstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 1
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_iget_boolean: /* 0x55 */
/* File: mips/op_iget_boolean.S */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGetBooleanInstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_iget_byte: /* 0x56 */
/* File: mips/op_iget_byte.S */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGetByteInstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_iget_char: /* 0x57 */
/* File: mips/op_iget_char.S */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGetCharInstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_iget_short: /* 0x58 */
/* File: mips/op_iget_short.S */
/* File: mips/op_iget.S */
    /*
     * General instance field get.
     *
     * for: iget, iget-object, iget-boolean, iget-byte, iget-char, iget-short
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- referrer
    move  a3, rSELF                        # a3 <- self
    JAL(artGetShortInstanceFromCode)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           # a2<- A+
    PREFETCH_INST(2)                       # load rINST
    bnez  a3, MterpPossibleException        # bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[A] <- v0
    .else
    SET_VREG_GOTO(v0, a2, t0)              # fp[A] <- v0
    .endif


/* ------------------------------ */
    .balign 128
.L_op_iput: /* 0x59 */
/* File: mips/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    .extern artSet32InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    GET_VREG(a2, a2)                       # a2 <- fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet32InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_wide: /* 0x5a */
/* File: mips/op_iput_wide.S */
    /* iput-wide vA, vB, field@CCCC */
    .extern artSet64InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    EAS2(a2, rFP, a2)                      # a2 <- &fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet64InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_object: /* 0x5b */
/* File: mips/op_iput_object.S */
    /*
     * 32-bit instance field put.
     *
     * for: iput-object, iput-object-volatile
     */
    /* op vA, vB, field@CCCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    move   a3, rSELF
    JAL(MterpIputObject)
    beqz   v0, MterpException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_boolean: /* 0x5c */
/* File: mips/op_iput_boolean.S */
/* File: mips/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    .extern artSet8InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    GET_VREG(a2, a2)                       # a2 <- fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet8InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_byte: /* 0x5d */
/* File: mips/op_iput_byte.S */
/* File: mips/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    .extern artSet8InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    GET_VREG(a2, a2)                       # a2 <- fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet8InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_char: /* 0x5e */
/* File: mips/op_iput_char.S */
/* File: mips/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    .extern artSet16InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    GET_VREG(a2, a2)                       # a2 <- fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet16InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_short: /* 0x5f */
/* File: mips/op_iput_short.S */
/* File: mips/op_iput.S */
    /*
     * General 32-bit instance field put.
     *
     * for: iput, iput-boolean, iput-byte, iput-char, iput-short
     */
    /* op vA, vB, field@CCCC */
    .extern artSet16InstanceFromMterp
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPB(a1)                            # a1 <- B
    GET_VREG(a1, a1)                       # a1 <- fp[B], the object pointer
    GET_OPA4(a2)                           # a2 <- A+
    GET_VREG(a2, a2)                       # a2 <- fp[A]
    lw    a3, OFF_FP_METHOD(rFP)           # a3 <- referrer
    PREFETCH_INST(2)                       # load rINST
    JAL(artSet16InstanceFromMterp)
    bnez  v0, MterpPossibleException       # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sget: /* 0x60 */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGet32Static
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGet32Static)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif

/* ------------------------------ */
    .balign 128
.L_op_sget_wide: /* 0x61 */
/* File: mips/op_sget_wide.S */
    /*
     * 64-bit SGET handler.
     */
    /* sget-wide vAA, field@BBBB */
    .extern MterpGet64Static
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGet64Static)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    bnez  a3, MterpException
    GET_OPA(a1)                            # a1 <- AA
    FETCH_ADVANCE_INST(2)                  # advance rPC, load rINST
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, a1, t0)        # vAA/vAA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_sget_object: /* 0x62 */
/* File: mips/op_sget_object.S */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGetObjStatic
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGetObjStatic)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 1
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif


/* ------------------------------ */
    .balign 128
.L_op_sget_boolean: /* 0x63 */
/* File: mips/op_sget_boolean.S */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGetBooleanStatic
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGetBooleanStatic)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif


/* ------------------------------ */
    .balign 128
.L_op_sget_byte: /* 0x64 */
/* File: mips/op_sget_byte.S */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGetByteStatic
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGetByteStatic)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif


/* ------------------------------ */
    .balign 128
.L_op_sget_char: /* 0x65 */
/* File: mips/op_sget_char.S */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGetCharStatic
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGetCharStatic)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif


/* ------------------------------ */
    .balign 128
.L_op_sget_short: /* 0x66 */
/* File: mips/op_sget_short.S */
/* File: mips/op_sget.S */
    /*
     * General SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    .extern MterpGetShortStatic
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    lw    a1, OFF_FP_METHOD(rFP)           # a1 <- method
    move  a2, rSELF                        # a2 <- self
    JAL(MterpGetShortStatic)
    lw    a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA(a2)                            # a2 <- AA
    PREFETCH_INST(2)
    bnez  a3, MterpException               # bail out
    ADVANCE(2)
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
.if 0
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       # fp[AA] <- v0
.else
    SET_VREG_GOTO(v0, a2, t0)              # fp[AA] <- v0
.endif


/* ------------------------------ */
    .balign 128
.L_op_sput: /* 0x67 */
/* File: mips/op_sput.S */
    /*
     * General SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    GET_OPA(a3)                            # a3 <- AA
    GET_VREG(a1, a3)                       # a1 <- fp[AA], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSet32Static)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_wide: /* 0x68 */
/* File: mips/op_sput_wide.S */
    /*
     * 64-bit SPUT handler.
     */
    /* sput-wide vAA, field@BBBB */
    .extern MterpSet64Static
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref CCCC
    GET_OPA(a1)                            # a1 <- AA
    EAS2(a1, rFP, a1)                      # a1 <- &fp[AA]
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSet64Static)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_object: /* 0x69 */
/* File: mips/op_sput_object.S */
    /*
     * General 32-bit SPUT handler.
     *
     * for: sput-object,
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    move   a3, rSELF
    JAL(MterpSputObject)
    beqz   v0, MterpException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_sput_boolean: /* 0x6a */
/* File: mips/op_sput_boolean.S */
/* File: mips/op_sput.S */
    /*
     * General SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    GET_OPA(a3)                            # a3 <- AA
    GET_VREG(a1, a3)                       # a1 <- fp[AA], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSetBooleanStatic)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_byte: /* 0x6b */
/* File: mips/op_sput_byte.S */
/* File: mips/op_sput.S */
    /*
     * General SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    GET_OPA(a3)                            # a3 <- AA
    GET_VREG(a1, a3)                       # a1 <- fp[AA], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSetByteStatic)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_char: /* 0x6c */
/* File: mips/op_sput_char.S */
/* File: mips/op_sput.S */
    /*
     * General SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    GET_OPA(a3)                            # a3 <- AA
    GET_VREG(a1, a3)                       # a1 <- fp[AA], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSetCharStatic)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_sput_short: /* 0x6d */
/* File: mips/op_sput_short.S */
/* File: mips/op_sput.S */
    /*
     * General SPUT handler.
     *
     * for: sput, sput-boolean, sput-byte, sput-char, sput-short
     */
    /* op vAA, field@BBBB */
    EXPORT_PC()
    FETCH(a0, 1)                           # a0 <- field ref BBBB
    GET_OPA(a3)                            # a3 <- AA
    GET_VREG(a1, a3)                       # a1 <- fp[AA], the object pointer
    lw    a2, OFF_FP_METHOD(rFP)           # a2 <- method
    move  a3, rSELF                        # a3 <- self
    PREFETCH_INST(2)                       # load rINST
    JAL(MterpSetShortStatic)
    bnez  v0, MterpException               # bail out
    ADVANCE(2)                             # advance rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual: /* 0x6e */
/* File: mips/op_invoke_virtual.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtual
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeVirtual)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_super: /* 0x6f */
/* File: mips/op_invoke_super.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeSuper
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeSuper)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_direct: /* 0x70 */
/* File: mips/op_invoke_direct.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeDirect
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeDirect)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_static: /* 0x71 */
/* File: mips/op_invoke_static.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeStatic
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeStatic)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_interface: /* 0x72 */
/* File: mips/op_invoke_interface.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeInterface
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeInterface)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_return_void_no_barrier: /* 0x73 */
/* File: mips/op_return_void_no_barrier.S */
    lw     ra, THREAD_FLAGS_OFFSET(rSELF)
    move   a0, rSELF
    and    ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    beqz   ra, 1f
    JAL(MterpSuspendCheck)                 # (self)
1:
    move   v0, zero
    move   v1, zero
    b      MterpReturn

/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_range: /* 0x74 */
/* File: mips/op_invoke_virtual_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeVirtualRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_super_range: /* 0x75 */
/* File: mips/op_invoke_super_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeSuperRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeSuperRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_direct_range: /* 0x76 */
/* File: mips/op_invoke_direct_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeDirectRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeDirectRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_static_range: /* 0x77 */
/* File: mips/op_invoke_static_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeStaticRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeStaticRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_interface_range: /* 0x78 */
/* File: mips/op_invoke_interface_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeInterfaceRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeInterfaceRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_unused_79: /* 0x79 */
/* File: mips/op_unused_79.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_7a: /* 0x7a */
/* File: mips/op_unused_7a.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_neg_int: /* 0x7b */
/* File: mips/op_neg_int.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    negu a0, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_not_int: /* 0x7c */
/* File: mips/op_not_int.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    not a0, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_neg_long: /* 0x7d */
/* File: mips/op_neg_long.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0/result1 = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double,
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    negu v0, a0                              #  optional op
    negu v1, a1; sltu a0, zero, v0; subu v1, v1, a0                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_not_long: /* 0x7e */
/* File: mips/op_not_long.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0/result1 = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double,
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    not a0, a0                              #  optional op
    not a1, a1                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_neg_float: /* 0x7f */
/* File: mips/op_neg_float.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    addu a0, a0, 0x80000000                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_neg_double: /* 0x80 */
/* File: mips/op_neg_double.S */
/* File: mips/unopWide.S */
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0/result1 = op a0/a1".
     * This could be MIPS instruction or a function call.
     *
     * For: neg-long, not-long, neg-double,
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    addu a1, a1, 0x80000000                                 #  a0/a1 <- op, a2-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_int_to_long: /* 0x81 */
/* File: mips/op_int_to_long.S */
/* File: mips/unopWider.S */
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result0/result1 = op a0".
     *
     * For: int-to-long
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    sra a1, a0, 31                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_int_to_float: /* 0x82 */
/* File: mips/op_int_to_float.S */
/* File: mips/funop.S */
    /*
     * Generic 32-bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.s.w fv0, fa0
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t1)         #  vA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_int_to_double: /* 0x83 */
/* File: mips/op_int_to_double.S */
/* File: mips/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.d.w fv0, fa0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_long_to_int: /* 0x84 */
/* File: mips/op_long_to_int.S */
/* we ignore the high word, making this equivalent to a 32-bit reg move */
/* File: mips/op_move.S */
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    .if 0
    SET_VREG_OBJECT_GOTO(a2, a0, t0)       #  fp[A] <- a2
    .else
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2
    .endif


/* ------------------------------ */
    .balign 128
.L_op_long_to_float: /* 0x85 */
/* File: mips/op_long_to_float.S */
    /*
     * long-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.s.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdisf)
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0

/* ------------------------------ */
    .balign 128
.L_op_long_to_double: /* 0x86 */
/* File: mips/op_long_to_double.S */
    /*
     * long-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]

#ifdef MIPS32REVGE6
    LOAD64_F(fv0, fv0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.d.l   fv0, fv0
#else
    LOAD64(rARG0, rARG1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(__floatdidf)                       #  a0/a1 <- op, a2-a3 changed
#endif

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- result

/* ------------------------------ */
    .balign 128
.L_op_float_to_int: /* 0x87 */
/* File: mips/op_float_to_int.S */
    /*
     * float-to-int
     *
     * We have to clip values to int min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifndef MIPS32REVGE6
    li        t0, INT_MIN_AS_FLOAT
    mtc1      t0, fa1
    c.ole.s   fcc0, fa1, fa0
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
#ifndef MIPS32REVGE6
    bc1t      fcc0, 1f                     #  if INT_MIN <= vB, proceed to truncation
    c.eq.s    fcc0, fa0, fa0
    mtc1      zero, fa0
    movt.s    fa0, fa1, fcc0               #  fa0 = ordered(vB) ? INT_MIN_AS_FLOAT : 0
1:
#endif
    trunc.w.s fa0, fa0
    SET_VREG_F_GOTO(fa0, rOBJ, t1)         #  vA <- result

/* ------------------------------ */
    .balign 128
.L_op_float_to_long: /* 0x88 */
/* File: mips/op_float_to_long.S */
    /*
     * float-to-long
     *
     * We have to clip values to long min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifdef MIPS32REVGE6
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    trunc.l.s fa0, fa0
    SET_VREG64_F_GOTO(fa0, fa0f, rOBJ, t1) #  vA <- result
#else
    c.eq.s    fcc0, fa0, fa0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1f      fcc0, .Lop_float_to_long_get_opcode

    li        t0, LONG_MIN_AS_FLOAT
    mtc1      t0, fa1
    c.ole.s   fcc0, fa0, fa1
    li        rRESULT1, LONG_MIN_HIGH
    bc1t      fcc0, .Lop_float_to_long_get_opcode

    neg.s     fa1, fa1
    c.ole.s   fcc0, fa1, fa0
    nor       rRESULT0, rRESULT0, zero
    nor       rRESULT1, rRESULT1, zero
    bc1t      fcc0, .Lop_float_to_long_get_opcode

    JAL(__fixsfdi)
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    b         .Lop_float_to_long_set_vreg
#endif

/* ------------------------------ */
    .balign 128
.L_op_float_to_double: /* 0x89 */
/* File: mips/op_float_to_double.S */
/* File: mips/funopWider.S */
    /*
     * Generic 32bit-to-64bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.d.s fv0, fa0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0) #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_double_to_int: /* 0x8a */
/* File: mips/op_double_to_int.S */
    /*
     * double-to-int
     *
     * We have to clip values to int min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifndef MIPS32REVGE6
    li        t0, INT_MIN_AS_DOUBLE_HIGH
    mtc1      zero, fa1
    MOVE_TO_FPU_HIGH(t0, fa1, fa1f)
    c.ole.d   fcc0, fa1, fa0
#endif
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
#ifndef MIPS32REVGE6
    bc1t      fcc0, 1f                     #  if INT_MIN <= vB, proceed to truncation
    c.eq.d    fcc0, fa0, fa0
    mtc1      zero, fa0
    MOVE_TO_FPU_HIGH(zero, fa0, fa0f)
    movt.d    fa0, fa1, fcc0               #  fa0 = ordered(vB) ? INT_MIN_AS_DOUBLE : 0
1:
#endif
    trunc.w.d fa0, fa0
    SET_VREG_F_GOTO(fa0, rOBJ, t1)         #  vA <- result

/* ------------------------------ */
    .balign 128
.L_op_double_to_long: /* 0x8b */
/* File: mips/op_double_to_long.S */
    /*
     * double-to-long
     *
     * We have to clip values to long min/max per the specification.  The
     * expected common case is a "reasonable" value that converts directly
     * to modest integer.  The EABI convert function isn't doing this for us
     * for pre-R6.
     */
    /* unop vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

#ifdef MIPS32REVGE6
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    trunc.l.d fa0, fa0
    SET_VREG64_F_GOTO(fa0, fa0f, rOBJ, t1) #  vA <- result
#else
    c.eq.d    fcc0, fa0, fa0
    li        rRESULT0, 0
    li        rRESULT1, 0
    bc1f      fcc0, .Lop_double_to_long_get_opcode

    li        t0, LONG_MIN_AS_DOUBLE_HIGH
    mtc1      zero, fa1
    MOVE_TO_FPU_HIGH(t0, fa1, fa1f)
    c.ole.d   fcc0, fa0, fa1
    li        rRESULT1, LONG_MIN_HIGH
    bc1t      fcc0, .Lop_double_to_long_get_opcode

    neg.d     fa1, fa1
    c.ole.d   fcc0, fa1, fa0
    nor       rRESULT0, rRESULT0, zero
    nor       rRESULT1, rRESULT1, zero
    bc1t      fcc0, .Lop_double_to_long_get_opcode

    JAL(__fixdfdi)
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    b         .Lop_double_to_long_set_vreg
#endif

/* ------------------------------ */
    .balign 128
.L_op_double_to_float: /* 0x8c */
/* File: mips/op_double_to_float.S */
/* File: mips/unopNarrower.S */
    /*
     * Generic 64bit-to-32bit floating-point unary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = op fa0".
     *
     * For: double-to-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    LOAD64_F(fa0, fa0f, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    cvt.s.d fv0, fa0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_int_to_byte: /* 0x8d */
/* File: mips/op_int_to_byte.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    SEB(a0, a0)                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_int_to_char: /* 0x8e */
/* File: mips/op_int_to_char.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    and a0, 0xffff                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_int_to_short: /* 0x8f */
/* File: mips/op_int_to_short.S */
/* File: mips/unop.S */
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result0 = op a0".
     * This could be a MIPS instruction or a function call.
     *
     * for: int-to-byte, int-to-char, int-to-short,
     *      neg-int, not-int, neg-float
     */
    /* unop vA, vB */
    GET_OPB(a3)                            #  a3 <- B
    GET_OPA4(t0)                           #  t0 <- A+
    GET_VREG(a0, a3)                       #  a0 <- vB
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
                                  #  optional op
    SEH(a0, a0)                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, t0, t1)        #  vA <- result0


/* ------------------------------ */
    .balign 128
.L_op_add_int: /* 0x90 */
/* File: mips/op_add_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_sub_int: /* 0x91 */
/* File: mips/op_sub_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    subu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_mul_int: /* 0x92 */
/* File: mips/op_mul_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_div_int: /* 0x93 */
/* File: mips/op_div_int.S */
#ifdef MIPS32REVGE6
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    div a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#else
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    div zero, a0, a1                              #  optional op
    mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_rem_int: /* 0x94 */
/* File: mips/op_rem_int.S */
#ifdef MIPS32REVGE6
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    mod a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#else
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    div zero, a0, a1                              #  optional op
    mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_and_int: /* 0x95 */
/* File: mips/op_and_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_or_int: /* 0x96 */
/* File: mips/op_or_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_xor_int: /* 0x97 */
/* File: mips/op_xor_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shl_int: /* 0x98 */
/* File: mips/op_shl_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shr_int: /* 0x99 */
/* File: mips/op_shr_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_ushr_int: /* 0x9a */
/* File: mips/op_ushr_int.S */
/* File: mips/binop.S */
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
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
                                  #  optional op
    srl a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_add_long: /* 0x9b */
/* File: mips/op_add_long.S */
/*
 *  The compiler generates the following sequence for
 *  [v1 v0] =  [a1 a0] + [a3 a2];
 *    addu v0,a2,a0
 *    addu a1,a3,a1
 *    sltu v1,v0,a2
 *    addu v1,v1,a1
 */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    addu v0, a2, a0                              #  optional op
    addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vAA/vAA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_sub_long: /* 0x9c */
/* File: mips/op_sub_long.S */
/*
 * For little endian the code sequence looks as follows:
 *    subu    v0,a0,a2
 *    subu    v1,a1,a3
 *    sltu    a0,a0,v0
 *    subu    v1,v1,a0
 */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    subu v0, a0, a2                              #  optional op
    subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vAA/vAA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_mul_long: /* 0x9d */
/* File: mips/op_mul_long.S */
    /*
     * Signed 64-bit integer multiply.
     *         a1   a0
     *   x     a3   a2
     *   -------------
     *       a2a1 a2a0
     *       a3a0
     *  a3a1 (<= unused)
     *  ---------------
     *         v1   v0
     */
    /* mul-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    and       t0, a0, 255                  #  a2 <- BB
    srl       t1, a0, 8                    #  a3 <- CC
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[BB]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vBB/vBB+1

    EAS2(t1, rFP, t1)                      #  t0 <- &fp[CC]
    LOAD64(a2, a3, t1)                     #  a2/a3 <- vCC/vCC+1

    mul       v1, a3, a0                   #  v1= a3a0
#ifdef MIPS32REVGE6
    mulu      v0, a2, a0                   #  v0= a2a0
    muhu      t1, a2, a0
#else
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
#endif
    mul       t0, a2, a1                   #  t0= a2a1
    addu      v1, v1, t1                   #  v1+= hi(a2a0)
    addu      v1, v1, t0                   #  v1= a3a0 + a2a1;

    GET_OPA(a0)                            #  a0 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    b         .Lop_mul_long_finish

/* ------------------------------ */
    .balign 128
.L_op_div_long: /* 0x9e */
/* File: mips/op_div_long.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vAA/vAA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_rem_long: /* 0x9f */
/* File: mips/op_rem_long.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vAA/vAA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_and_long: /* 0xa0 */
/* File: mips/op_and_long.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    and a0, a0, a2                              #  optional op
    and a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vAA/vAA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_or_long: /* 0xa1 */
/* File: mips/op_or_long.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    or a0, a0, a2                              #  optional op
    or a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vAA/vAA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_xor_long: /* 0xa2 */
/* File: mips/op_xor_long.S */
/* File: mips/binopWide.S */
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a2-a3).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, t1)               #  a2/a3 <- vCC/vCC+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    xor a0, a0, a2                              #  optional op
    xor a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vAA/vAA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_shl_long: /* 0xa3 */
/* File: mips/op_shl_long.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t2)                            #  t2 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v1, a2, 0x20                   #  shift< shift & 0x20
    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    bnez    v1, .Lop_shl_long_finish
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    SET_VREG64_GOTO(v0, v1, t2, t0)        #  vAA/vAA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_shr_long: /* 0xa4 */
/* File: mips/op_shr_long.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t3)                            #  t3 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v0, a2, 0x20                   #  shift & 0x20
    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    bnez    v0, .Lop_shr_long_finish
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t3, t0)        #  vAA/VAA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_ushr_long: /* 0xa5 */
/* File: mips/op_ushr_long.S */
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi      v0, a2, 0x20                 #  shift & 0x20
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    bnez      v0, .Lop_ushr_long_finish
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)      #  vAA/vAA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_add_float: /* 0xa6 */
/* File: mips/op_add_float.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    add.s fv0, fa0, fa1                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_sub_float: /* 0xa7 */
/* File: mips/op_sub_float.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    sub.s fv0, fa0, fa1                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_mul_float: /* 0xa8 */
/* File: mips/op_mul_float.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    mul.s fv0, fa0, fa1                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_div_float: /* 0xa9 */
/* File: mips/op_div_float.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    div.s fv0, fa0, fa1                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_rem_float: /* 0xaa */
/* File: mips/op_rem_float.S */
/* File: mips/fbinop.S */
    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp, rem-fp
     */

    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG_F(fa1, a3)                    #  a1 <- vCC
    GET_VREG_F(fa0, a2)                    #  a0 <- vBB

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    JAL(fmodf)                                 #  f0 = result
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vAA <- fv0


/* ------------------------------ */
    .balign 128
.L_op_add_double: /* 0xab */
/* File: mips/op_add_double.S */
/* File: mips/fbinopWide.S */
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    add.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_sub_double: /* 0xac */
/* File: mips/op_sub_double.S */
/* File: mips/fbinopWide.S */
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    sub.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_mul_double: /* 0xad */
/* File: mips/op_mul_double.S */
/* File: mips/fbinopWide.S */
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    mul.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_div_double: /* 0xae */
/* File: mips/op_div_double.S */
/* File: mips/fbinopWide.S */
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    div.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_rem_double: /* 0xaf */
/* File: mips/op_rem_double.S */
/* File: mips/fbinopWide.S */
    /*
     * Generic 64-bit floating-point binary operation.  Provide an "instr"
     * line that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * for: add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    JAL(fmod)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vAA/vAA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_add_int_2addr: /* 0xb0 */
/* File: mips/op_add_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_sub_int_2addr: /* 0xb1 */
/* File: mips/op_sub_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    subu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_mul_int_2addr: /* 0xb2 */
/* File: mips/op_mul_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_div_int_2addr: /* 0xb3 */
/* File: mips/op_div_int_2addr.S */
#ifdef MIPS32REVGE6
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    div a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#else
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_rem_int_2addr: /* 0xb4 */
/* File: mips/op_rem_int_2addr.S */
#ifdef MIPS32REVGE6
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    mod a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#else
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_and_int_2addr: /* 0xb5 */
/* File: mips/op_and_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_or_int_2addr: /* 0xb6 */
/* File: mips/op_or_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_xor_int_2addr: /* 0xb7 */
/* File: mips/op_xor_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shl_int_2addr: /* 0xb8 */
/* File: mips/op_shl_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shr_int_2addr: /* 0xb9 */
/* File: mips/op_shr_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_ushr_int_2addr: /* 0xba */
/* File: mips/op_ushr_int_2addr.S */
/* File: mips/binop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    srl a0, a0, a1                                  #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_add_long_2addr: /* 0xbb */
/* File: mips/op_add_long_2addr.S */
/*
 * See op_add_long.S for details
 */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    addu v0, a2, a0                              #  optional op
    addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vA/vA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_sub_long_2addr: /* 0xbc */
/* File: mips/op_sub_long_2addr.S */
/*
 * See op_sub_long.S for more details
 */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    subu v0, a0, a2                              #  optional op
    subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vA/vA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_mul_long_2addr: /* 0xbd */
/* File: mips/op_mul_long_2addr.S */
    /*
     * See op_mul_long.S for more details
     */
    /* mul-long/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+

    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  vAA.low / high

    GET_OPB(t1)                            #  t1 <- B
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[B]
    LOAD64(a2, a3, t1)                     #  vBB.low / high

    mul       v1, a3, a0                   #  v1= a3a0
#ifdef MIPS32REVGE6
    mulu      v0, a2, a0                   #  v0= a2a0
    muhu      t1, a2, a0
#else
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
 #endif
    mul       t2, a2, a1                   #  t2= a2a1
    addu      v1, v1, t1                   #  v1= a3a0 + hi(a2a0)
    addu      v1, v1, t2                   #  v1= v1 + a2a1;

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t1)      #  vA/vA+1 <- v0(low)/v1(high)

/* ------------------------------ */
    .balign 128
.L_op_div_long_2addr: /* 0xbe */
/* File: mips/op_div_long_2addr.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__divdi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vA/vA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_rem_long_2addr: /* 0xbf */
/* File: mips/op_rem_long_2addr.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 1
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

                                  #  optional op
    JAL(__moddi3)                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)   #  vA/vA+1 <- v0/v1


/* ------------------------------ */
    .balign 128
.L_op_and_long_2addr: /* 0xc0 */
/* File: mips/op_and_long_2addr.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    and a0, a0, a2                              #  optional op
    and a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_or_long_2addr: /* 0xc1 */
/* File: mips/op_or_long_2addr.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    or a0, a0, a2                              #  optional op
    or a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_xor_long_2addr: /* 0xc2 */
/* File: mips/op_xor_long_2addr.S */
/* File: mips/binopWide2addr.S */
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register pair other than a0-a1, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vB (a2-a3).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64(a2, a3, a1)               #  a2/a3 <- vB/vB+1
    LOAD64(a0, a1, t0)               #  a0/a1 <- vA/vA+1
    .if 0
    or        t0, a2, a3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    xor a0, a0, a2                              #  optional op
    xor a1, a1, a3                                 #  result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, rOBJ, t0)   #  vA/vA+1 <- a0/a1


/* ------------------------------ */
    .balign 128
.L_op_shl_long_2addr: /* 0xc3 */
/* File: mips/op_shl_long_2addr.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t2, rFP, rOBJ)                    #  t2 <- &fp[A]
    LOAD64(a0, a1, t2)                     #  a0/a1 <- vA/vA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v1, a2, 0x20                   #  shift< shift & 0x20
    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    bnez    v1, .Lop_shl_long_2addr_finish
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    SET_VREG64_GOTO(v0, v1, rOBJ, t0)      #  vA/vA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_shr_long_2addr: /* 0xc4 */
/* File: mips/op_shr_long_2addr.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t0, rFP, t2)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vA/vA+1
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi    v0, a2, 0x20                   #  shift & 0x20
    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    bnez    v0, .Lop_shr_long_2addr_finish
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t2, t0)        #  vA/vA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_ushr_long_2addr: /* 0xc5 */
/* File: mips/op_ushr_long_2addr.S */
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* ushr-long/2addr vA, vB */
    GET_OPA4(t3)                           #  t3 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t0, rFP, t3)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  a0/a1 <- vA/vA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST

    andi      v0, a2, 0x20                 #  shift & 0x20
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    bnez      v0, .Lop_ushr_long_2addr_finish
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    SET_VREG64_GOTO(v0, v1, t3, t0)        #  vA/vA+1 <- v0/v1

/* ------------------------------ */
    .balign 128
.L_op_add_float_2addr: /* 0xc6 */
/* File: mips/op_add_float_2addr.S */
/* File: mips/fbinop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    add.s fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result


/* ------------------------------ */
    .balign 128
.L_op_sub_float_2addr: /* 0xc7 */
/* File: mips/op_sub_float_2addr.S */
/* File: mips/fbinop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    sub.s fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result


/* ------------------------------ */
    .balign 128
.L_op_mul_float_2addr: /* 0xc8 */
/* File: mips/op_mul_float_2addr.S */
/* File: mips/fbinop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    mul.s fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result


/* ------------------------------ */
    .balign 128
.L_op_div_float_2addr: /* 0xc9 */
/* File: mips/op_div_float_2addr.S */
/* File: mips/fbinop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    div.s fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result


/* ------------------------------ */
    .balign 128
.L_op_rem_float_2addr: /* 0xca */
/* File: mips/op_rem_float_2addr.S */
/* File: mips/fbinop2addr.S */
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr"
     * that specifies an instruction that performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     *      div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    JAL(fmodf)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_F_GOTO(fv0, rOBJ, t0)         #  vA <- result


/* ------------------------------ */
    .balign 128
.L_op_add_double_2addr: /* 0xcb */
/* File: mips/op_add_double_2addr.S */
/* File: mips/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    add.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_sub_double_2addr: /* 0xcc */
/* File: mips/op_sub_double_2addr.S */
/* File: mips/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sub.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_mul_double_2addr: /* 0xcd */
/* File: mips/op_mul_double_2addr.S */
/* File: mips/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    mul.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_div_double_2addr: /* 0xce */
/* File: mips/op_div_double_2addr.S */
/* File: mips/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    div.d fv0, fa0, fa1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_rem_double_2addr: /* 0xcf */
/* File: mips/op_rem_double_2addr.S */
/* File: mips/fbinopWide2addr.S */
    /*
     * Generic 64-bit floating-point "/2addr" binary operation.
     * Provide an "instr" line that specifies an instruction that
     * performs "fv0 = fa0 op fa1".
     * This could be an MIPS instruction or a function call.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *      div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(t0, rFP, rOBJ)                    #  t0 <- &fp[A]
    LOAD64_F(fa0, fa0f, t0)
    LOAD64_F(fa1, fa1f, a1)

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(fmod)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_F_GOTO(fv0, fv0f, rOBJ, t0)  #  vA/vA+1 <- fv0


/* ------------------------------ */
    .balign 128
.L_op_add_int_lit16: /* 0xd0 */
/* File: mips/op_add_int_lit16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_rsub_int: /* 0xd1 */
/* File: mips/op_rsub_int.S */
/* this op is "rsub-int", but can be thought of as "rsub-int/lit16" */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    subu a0, a1, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_mul_int_lit16: /* 0xd2 */
/* File: mips/op_mul_int_lit16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_div_int_lit16: /* 0xd3 */
/* File: mips/op_div_int_lit16.S */
#ifdef MIPS32REVGE6
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    div a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#else
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_rem_int_lit16: /* 0xd4 */
/* File: mips/op_rem_int_lit16.S */
#ifdef MIPS32REVGE6
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    mod a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#else
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 1
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_and_int_lit16: /* 0xd5 */
/* File: mips/op_and_int_lit16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_or_int_lit16: /* 0xd6 */
/* File: mips/op_or_int_lit16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_xor_int_lit16: /* 0xd7 */
/* File: mips/op_xor_int_lit16.S */
/* File: mips/binopLit16.S */
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    .if 0
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vA <- a0


/* ------------------------------ */
    .balign 128
.L_op_add_int_lit8: /* 0xd8 */
/* File: mips/op_add_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    addu a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_rsub_int_lit8: /* 0xd9 */
/* File: mips/op_rsub_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    subu a0, a1, a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_mul_int_lit8: /* 0xda */
/* File: mips/op_mul_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    mul a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_div_int_lit8: /* 0xdb */
/* File: mips/op_div_int_lit8.S */
#ifdef MIPS32REVGE6
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    div a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#else
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mflo a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_rem_int_lit8: /* 0xdc */
/* File: mips/op_rem_int_lit8.S */
#ifdef MIPS32REVGE6
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    mod a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#else
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 1
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    div zero, a0, a1                              #  optional op
    mfhi a0                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0

#endif

/* ------------------------------ */
    .balign 128
.L_op_and_int_lit8: /* 0xdd */
/* File: mips/op_and_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    and a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_or_int_lit8: /* 0xde */
/* File: mips/op_or_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    or a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_xor_int_lit8: /* 0xdf */
/* File: mips/op_xor_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    xor a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shl_int_lit8: /* 0xe0 */
/* File: mips/op_shl_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    sll a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_shr_int_lit8: /* 0xe1 */
/* File: mips/op_shr_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    sra a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_ushr_int_lit8: /* 0xe2 */
/* File: mips/op_ushr_int_lit8.S */
/* File: mips/binopLit8.S */
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if 0
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

                                  #  optional op
    srl a0, a0, a1                                 #  a0 <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, rOBJ, t0)       #  vAA <- a0


/* ------------------------------ */
    .balign 128
.L_op_iget_quick: /* 0xe3 */
/* File: mips/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    lw     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0

/* ------------------------------ */
    .balign 128
.L_op_iget_wide_quick: /* 0xe4 */
/* File: mips/op_iget_wide_quick.S */
    /* iget-wide-quick vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1                   #  t0 <- a3 + a1
    LOAD64(a0, a1, t0)                     #  a0 <- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(a0, a1, a2, t0)        #  fp[A] <- a0/a1

/* ------------------------------ */
    .balign 128
.L_op_iget_object_quick: /* 0xe5 */
/* File: mips/op_iget_object_quick.S */
    /* For: iget-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    FETCH(a1, 1)                           #  a1 <- field byte offset
    EXPORT_PC()
    GET_VREG(a0, a2)                       #  a0 <- object we're operating on
    JAL(artIGetObjectFromMterp)            #  v0 <- GetObj(obj, offset)
    lw   a3, THREAD_EXCEPTION_OFFSET(rSELF)
    GET_OPA4(a2)                           #  a2<- A+
    PREFETCH_INST(2)                       #  load rINST
    bnez a3, MterpPossibleException        #  bail out
    ADVANCE(2)                             #  advance rPC
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_OBJECT_GOTO(v0, a2, t0)       #  fp[A] <- v0

/* ------------------------------ */
    .balign 128
.L_op_iput_quick: /* 0xe6 */
/* File: mips/op_iput_quick.S */
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    sw    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_wide_quick: /* 0xe7 */
/* File: mips/op_iput_wide_quick.S */
    /* iput-wide-quick vA, vB, offset@CCCC */
    GET_OPA4(a0)                           #  a0 <- A(+)
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a2, a1)                       #  a2 <- fp[B], the object pointer
    # check object for null
    beqz      a2, common_errNullObject     #  object was null
    EAS2(a3, rFP, a0)                      #  a3 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[A]
    FETCH(a3, 1)                           #  a3 <- field byte offset
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      a2, a2, a3                   #  obj.field (64 bits, aligned) <- a0/a1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t0)
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0/a1
    JR(t0)                                 #  jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_iput_object_quick: /* 0xe8 */
/* File: mips/op_iput_object_quick.S */
    /* For: iput-object-quick */
    /* op vA, vB, offset@CCCC */
    EXPORT_PC()
    addu   a0, rFP, OFF_FP_SHADOWFRAME
    move   a1, rPC
    move   a2, rINST
    JAL(MterpIputObjectQuick)
    beqz   v0, MterpException
    FETCH_ADVANCE_INST(2)               # advance rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_quick: /* 0xe9 */
/* File: mips/op_invoke_virtual_quick.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualQuick
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeVirtualQuick)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_virtual_range_quick: /* 0xea */
/* File: mips/op_invoke_virtual_range_quick.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeVirtualQuickRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeVirtualQuickRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_iput_boolean_quick: /* 0xeb */
/* File: mips/op_iput_boolean_quick.S */
/* File: mips/op_iput_quick.S */
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    sb    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_byte_quick: /* 0xec */
/* File: mips/op_iput_byte_quick.S */
/* File: mips/op_iput_quick.S */
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    sb    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_char_quick: /* 0xed */
/* File: mips/op_iput_char_quick.S */
/* File: mips/op_iput_quick.S */
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    sh    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iput_short_quick: /* 0xee */
/* File: mips/op_iput_short_quick.S */
/* File: mips/op_iput_quick.S */
    /* For: iput-quick, iput-object-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- fp[B], the object pointer
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    beqz      a3, common_errNullObject     #  object was null
    GET_VREG(a0, a2)                       #  a0 <- fp[A]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      t0, a3, a1
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    GET_OPCODE_TARGET(t1)
    sh    a0, 0(t0)                    #  obj.field (8/16/32 bits) <- a0
    JR(t1)                                 #  jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_iget_boolean_quick: /* 0xef */
/* File: mips/op_iget_boolean_quick.S */
/* File: mips/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    lbu     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0


/* ------------------------------ */
    .balign 128
.L_op_iget_byte_quick: /* 0xf0 */
/* File: mips/op_iget_byte_quick.S */
/* File: mips/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    lb     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0


/* ------------------------------ */
    .balign 128
.L_op_iget_char_quick: /* 0xf1 */
/* File: mips/op_iget_char_quick.S */
/* File: mips/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    lhu     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0


/* ------------------------------ */
    .balign 128
.L_op_iget_short_quick: /* 0xf2 */
/* File: mips/op_iget_short_quick.S */
/* File: mips/op_iget_quick.S */
    /* For: iget-quick, iget-boolean-quick, iget-byte-quick, iget-char-quick, iget-short-quick */
    /* op vA, vB, offset@CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1
    lh     a0, 0(t0)                    #  a0 <- obj.field (8/16/32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[A] <- a0


/* ------------------------------ */
    .balign 128
.L_op_unused_f3: /* 0xf3 */
/* File: mips/op_unused_f3.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f4: /* 0xf4 */
/* File: mips/op_unused_f4.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f5: /* 0xf5 */
/* File: mips/op_unused_f5.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f6: /* 0xf6 */
/* File: mips/op_unused_f6.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f7: /* 0xf7 */
/* File: mips/op_unused_f7.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f8: /* 0xf8 */
/* File: mips/op_unused_f8.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_unused_f9: /* 0xf9 */
/* File: mips/op_unused_f9.S */
/* File: mips/unused.S */
/*
 * Bail to reference interpreter to throw.
 */
  b MterpFallback


/* ------------------------------ */
    .balign 128
.L_op_invoke_polymorphic: /* 0xfa */
/* File: mips/op_invoke_polymorphic.S */
/* File: mips/invoke_polymorphic.S */
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern MterpInvokePolymorphic
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokePolymorphic)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(4)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_polymorphic_range: /* 0xfb */
/* File: mips/op_invoke_polymorphic_range.S */
/* File: mips/invoke_polymorphic.S */
    /*
     * invoke-polymorphic handler wrapper.
     */
    /* op {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB, proto@HHHH */
    .extern MterpInvokePolymorphicRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokePolymorphicRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(4)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_custom: /* 0xfc */
/* File: mips/op_invoke_custom.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeCustom
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeCustom)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_invoke_custom_range: /* 0xfd */
/* File: mips/op_invoke_custom_range.S */
/* File: mips/invoke.S */
    /*
     * Generic invoke handler wrapper.
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    .extern MterpInvokeCustomRange
    EXPORT_PC()
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    move    a3, rINST
    JAL(MterpInvokeCustomRange)
    beqz    v0, MterpException
    FETCH_ADVANCE_INST(3)
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)


/* ------------------------------ */
    .balign 128
.L_op_const_method_handle: /* 0xfe */
/* File: mips/op_const_method_handle.S */
/* File: mips/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstMethodHandle
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstMethodHandle)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction


/* ------------------------------ */
    .balign 128
.L_op_const_method_type: /* 0xff */
/* File: mips/op_const_method_type.S */
/* File: mips/const.S */
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern MterpConstMethodType
    EXPORT_PC()
    FETCH(a0, 1)                        # a0 <- BBBB
    GET_OPA(a1)                         # a1 <- AA
    addu   a2, rFP, OFF_FP_SHADOWFRAME  # a2 <- shadow frame
    move   a3, rSELF
    JAL(MterpConstMethodType)                        # v0 <- Mterp(index, tgt_reg, shadow_frame, self)
    PREFETCH_INST(2)                    # load rINST
    bnez   v0, MterpPossibleException
    ADVANCE(2)                          # advance rPC
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction


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

/* continuation for op_float_to_long */

#ifndef MIPS32REVGE6
.Lop_float_to_long_get_opcode:
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
.Lop_float_to_long_set_vreg:
    SET_VREG64_GOTO(rRESULT0, rRESULT1, rOBJ, t1)   #  vA/vA+1 <- v0/v1
#endif

/* continuation for op_double_to_long */

#ifndef MIPS32REVGE6
.Lop_double_to_long_get_opcode:
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
.Lop_double_to_long_set_vreg:
    SET_VREG64_GOTO(rRESULT0, rRESULT1, rOBJ, t1)   #  vA/vA+1 <- v0/v1
#endif

/* continuation for op_mul_long */

.Lop_mul_long_finish:
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG64_GOTO(v0, v1, a0, t0)        #  vAA/vAA+1 <- v0(low)/v1(high)

/* continuation for op_shl_long */

.Lop_shl_long_finish:
    SET_VREG64_GOTO(zero, v0, t2, t0)      #  vAA/vAA+1 <- rlo/rhi

/* continuation for op_shr_long */

.Lop_shr_long_finish:
    sra     a3, a1, 31                     #  a3<- sign(ah)
    SET_VREG64_GOTO(v1, a3, t3, t0)        #  vAA/VAA+1 <- rlo/rhi

/* continuation for op_ushr_long */

.Lop_ushr_long_finish:
    SET_VREG64_GOTO(v1, zero, rOBJ, t0)    #  vAA/vAA+1 <- rlo/rhi

/* continuation for op_shl_long_2addr */

.Lop_shl_long_2addr_finish:
    SET_VREG64_GOTO(zero, v0, rOBJ, t0)    #  vA/vA+1 <- rlo/rhi

/* continuation for op_shr_long_2addr */

.Lop_shr_long_2addr_finish:
    sra     a3, a1, 31                     #  a3<- sign(ah)
    SET_VREG64_GOTO(v1, a3, t2, t0)        #  vA/vA+1 <- rlo/rhi

/* continuation for op_ushr_long_2addr */

.Lop_ushr_long_2addr_finish:
    SET_VREG64_GOTO(v1, zero, t3, t0)      #  vA/vA+1 <- rlo/rhi
    .global artMterpAsmSisterEnd
artMterpAsmSisterEnd:


    .global artMterpAsmAltInstructionStart
    .text

artMterpAsmAltInstructionStart = .L_ALT_op_nop
/* ------------------------------ */
    .balign 128
.L_ALT_op_nop: /* 0x00 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (0 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move: /* 0x01 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (1 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_from16: /* 0x02 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (2 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_16: /* 0x03 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (3 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide: /* 0x04 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (4 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide_from16: /* 0x05 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (5 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_wide_16: /* 0x06 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (6 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object: /* 0x07 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (7 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object_from16: /* 0x08 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (8 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_object_16: /* 0x09 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (9 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result: /* 0x0a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (10 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result_wide: /* 0x0b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (11 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_result_object: /* 0x0c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (12 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_move_exception: /* 0x0d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (13 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_void: /* 0x0e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (14 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_return: /* 0x0f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (15 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_wide: /* 0x10 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (16 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_object: /* 0x11 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (17 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_4: /* 0x12 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (18 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_16: /* 0x13 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (19 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const: /* 0x14 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (20 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_high16: /* 0x15 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (21 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_16: /* 0x16 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (22 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_32: /* 0x17 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (23 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide: /* 0x18 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (24 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_wide_high16: /* 0x19 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (25 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_string: /* 0x1a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (26 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_string_jumbo: /* 0x1b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (27 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_class: /* 0x1c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (28 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_monitor_enter: /* 0x1d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (29 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_monitor_exit: /* 0x1e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (30 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_check_cast: /* 0x1f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (31 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_instance_of: /* 0x20 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (32 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_array_length: /* 0x21 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (33 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_new_instance: /* 0x22 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (34 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_new_array: /* 0x23 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (35 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_filled_new_array: /* 0x24 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (36 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_filled_new_array_range: /* 0x25 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (37 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_fill_array_data: /* 0x26 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (38 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_throw: /* 0x27 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (39 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto: /* 0x28 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (40 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto_16: /* 0x29 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (41 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_goto_32: /* 0x2a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (42 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_packed_switch: /* 0x2b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (43 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sparse_switch: /* 0x2c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (44 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpl_float: /* 0x2d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (45 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpg_float: /* 0x2e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (46 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpl_double: /* 0x2f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (47 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmpg_double: /* 0x30 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (48 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_cmp_long: /* 0x31 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (49 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_eq: /* 0x32 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (50 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ne: /* 0x33 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (51 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_lt: /* 0x34 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (52 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ge: /* 0x35 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (53 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gt: /* 0x36 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (54 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_le: /* 0x37 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (55 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_eqz: /* 0x38 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (56 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_nez: /* 0x39 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (57 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_ltz: /* 0x3a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (58 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gez: /* 0x3b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (59 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_gtz: /* 0x3c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (60 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_if_lez: /* 0x3d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (61 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_3e: /* 0x3e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (62 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_3f: /* 0x3f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (63 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_40: /* 0x40 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (64 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_41: /* 0x41 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (65 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_42: /* 0x42 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (66 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_43: /* 0x43 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (67 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget: /* 0x44 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (68 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_wide: /* 0x45 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (69 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_object: /* 0x46 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (70 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_boolean: /* 0x47 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (71 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_byte: /* 0x48 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (72 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_char: /* 0x49 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (73 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aget_short: /* 0x4a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (74 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput: /* 0x4b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (75 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_wide: /* 0x4c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (76 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_object: /* 0x4d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (77 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_boolean: /* 0x4e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (78 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_byte: /* 0x4f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (79 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_char: /* 0x50 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (80 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_aput_short: /* 0x51 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (81 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget: /* 0x52 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (82 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_wide: /* 0x53 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (83 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_object: /* 0x54 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (84 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_boolean: /* 0x55 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (85 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_byte: /* 0x56 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (86 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_char: /* 0x57 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (87 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_short: /* 0x58 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (88 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput: /* 0x59 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (89 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_wide: /* 0x5a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (90 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_object: /* 0x5b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (91 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_boolean: /* 0x5c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (92 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_byte: /* 0x5d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (93 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_char: /* 0x5e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (94 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_short: /* 0x5f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (95 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget: /* 0x60 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (96 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_wide: /* 0x61 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (97 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_object: /* 0x62 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (98 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_boolean: /* 0x63 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (99 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_byte: /* 0x64 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (100 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_char: /* 0x65 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (101 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sget_short: /* 0x66 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (102 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput: /* 0x67 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (103 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_wide: /* 0x68 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (104 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_object: /* 0x69 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (105 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_boolean: /* 0x6a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (106 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_byte: /* 0x6b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (107 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_char: /* 0x6c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (108 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sput_short: /* 0x6d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (109 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual: /* 0x6e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (110 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_super: /* 0x6f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (111 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_direct: /* 0x70 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (112 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_static: /* 0x71 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (113 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_interface: /* 0x72 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (114 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_return_void_no_barrier: /* 0x73 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (115 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_range: /* 0x74 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (116 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_super_range: /* 0x75 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (117 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_direct_range: /* 0x76 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (118 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_static_range: /* 0x77 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (119 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_interface_range: /* 0x78 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (120 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_79: /* 0x79 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (121 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_7a: /* 0x7a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (122 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_int: /* 0x7b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (123 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_not_int: /* 0x7c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (124 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_long: /* 0x7d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (125 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_not_long: /* 0x7e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (126 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_float: /* 0x7f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (127 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_neg_double: /* 0x80 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (128 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_long: /* 0x81 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (129 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_float: /* 0x82 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (130 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_double: /* 0x83 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (131 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_int: /* 0x84 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (132 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_float: /* 0x85 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (133 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_long_to_double: /* 0x86 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (134 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_int: /* 0x87 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (135 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_long: /* 0x88 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (136 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_float_to_double: /* 0x89 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (137 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_int: /* 0x8a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (138 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_long: /* 0x8b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (139 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_double_to_float: /* 0x8c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (140 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_byte: /* 0x8d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (141 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_char: /* 0x8e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (142 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_int_to_short: /* 0x8f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (143 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int: /* 0x90 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (144 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_int: /* 0x91 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (145 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int: /* 0x92 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (146 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int: /* 0x93 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (147 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int: /* 0x94 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (148 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int: /* 0x95 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (149 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int: /* 0x96 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (150 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int: /* 0x97 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (151 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int: /* 0x98 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (152 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int: /* 0x99 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (153 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int: /* 0x9a */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (154 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_long: /* 0x9b */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (155 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_long: /* 0x9c */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (156 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_long: /* 0x9d */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (157 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_long: /* 0x9e */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (158 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_long: /* 0x9f */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (159 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_long: /* 0xa0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (160 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_long: /* 0xa1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (161 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_long: /* 0xa2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (162 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_long: /* 0xa3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (163 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_long: /* 0xa4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (164 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_long: /* 0xa5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (165 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_float: /* 0xa6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (166 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_float: /* 0xa7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (167 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_float: /* 0xa8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (168 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_float: /* 0xa9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (169 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_float: /* 0xaa */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (170 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_double: /* 0xab */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (171 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_double: /* 0xac */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (172 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_double: /* 0xad */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (173 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_double: /* 0xae */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (174 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_double: /* 0xaf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (175 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_2addr: /* 0xb0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (176 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_int_2addr: /* 0xb1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (177 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_2addr: /* 0xb2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (178 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_2addr: /* 0xb3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (179 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_2addr: /* 0xb4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (180 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_2addr: /* 0xb5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (181 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_2addr: /* 0xb6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (182 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_2addr: /* 0xb7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (183 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int_2addr: /* 0xb8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (184 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int_2addr: /* 0xb9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (185 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int_2addr: /* 0xba */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (186 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_long_2addr: /* 0xbb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (187 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_long_2addr: /* 0xbc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (188 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_long_2addr: /* 0xbd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (189 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_long_2addr: /* 0xbe */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (190 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_long_2addr: /* 0xbf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (191 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_long_2addr: /* 0xc0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (192 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_long_2addr: /* 0xc1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (193 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_long_2addr: /* 0xc2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (194 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_long_2addr: /* 0xc3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (195 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_long_2addr: /* 0xc4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (196 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_long_2addr: /* 0xc5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (197 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_float_2addr: /* 0xc6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (198 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_float_2addr: /* 0xc7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (199 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_float_2addr: /* 0xc8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (200 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_float_2addr: /* 0xc9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (201 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_float_2addr: /* 0xca */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (202 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_double_2addr: /* 0xcb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (203 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_sub_double_2addr: /* 0xcc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (204 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_double_2addr: /* 0xcd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (205 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_double_2addr: /* 0xce */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (206 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_double_2addr: /* 0xcf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (207 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_lit16: /* 0xd0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (208 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rsub_int: /* 0xd1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (209 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_lit16: /* 0xd2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (210 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_lit16: /* 0xd3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (211 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_lit16: /* 0xd4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (212 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_lit16: /* 0xd5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (213 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_lit16: /* 0xd6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (214 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_lit16: /* 0xd7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (215 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_add_int_lit8: /* 0xd8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (216 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rsub_int_lit8: /* 0xd9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (217 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_mul_int_lit8: /* 0xda */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (218 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_div_int_lit8: /* 0xdb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (219 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_rem_int_lit8: /* 0xdc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (220 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_and_int_lit8: /* 0xdd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (221 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_or_int_lit8: /* 0xde */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (222 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_xor_int_lit8: /* 0xdf */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (223 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shl_int_lit8: /* 0xe0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (224 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_shr_int_lit8: /* 0xe1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (225 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_ushr_int_lit8: /* 0xe2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (226 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_quick: /* 0xe3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (227 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_wide_quick: /* 0xe4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (228 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_object_quick: /* 0xe5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (229 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_quick: /* 0xe6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (230 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_wide_quick: /* 0xe7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (231 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_object_quick: /* 0xe8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (232 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_quick: /* 0xe9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (233 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_virtual_range_quick: /* 0xea */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (234 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_boolean_quick: /* 0xeb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (235 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_byte_quick: /* 0xec */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (236 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_char_quick: /* 0xed */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (237 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iput_short_quick: /* 0xee */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (238 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_boolean_quick: /* 0xef */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (239 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_byte_quick: /* 0xf0 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (240 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_char_quick: /* 0xf1 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (241 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_iget_short_quick: /* 0xf2 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (242 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f3: /* 0xf3 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (243 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f4: /* 0xf4 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (244 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f5: /* 0xf5 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (245 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f6: /* 0xf6 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (246 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f7: /* 0xf7 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (247 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f8: /* 0xf8 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (248 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_unused_f9: /* 0xf9 */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (249 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_polymorphic: /* 0xfa */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (250 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_polymorphic_range: /* 0xfb */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (251 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_custom: /* 0xfc */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (252 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_invoke_custom_range: /* 0xfd */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (253 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_method_handle: /* 0xfe */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (254 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

/* ------------------------------ */
    .balign 128
.L_ALT_op_const_method_type: /* 0xff */
/* File: mips/alt_stub.S */
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (255 * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

    .balign 128
    .global artMterpAsmAltInstructionEnd
artMterpAsmAltInstructionEnd:
/* File: mips/footer.S */
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align 2

/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */
common_errDivideByZero:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogDivideByZeroException)
#endif
    b MterpCommonFallback

common_errArrayIndex:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogArrayIndexException)
#endif
    b MterpCommonFallback

common_errNegativeArraySize:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNegativeArraySizeException)
#endif
    b MterpCommonFallback

common_errNoSuchMethod:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNoSuchMethodException)
#endif
    b MterpCommonFallback

common_errNullObject:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNullObjectException)
#endif
    b MterpCommonFallback

common_exceptionThrown:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogExceptionThrownException)
#endif
    b MterpCommonFallback

MterpSuspendFallback:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    lw    a2, THREAD_FLAGS_OFFSET(rSELF)
    JAL(MterpLogSuspendFallback)
#endif
    b MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    lw      a0, THREAD_EXCEPTION_OFFSET(rSELF)
    beqz    a0, MterpFallback          # If exception, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
MterpException:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpHandleException)                    # (self, shadow_frame)
    beqz    v0, MterpExceptionReturn             # no local catch, back to caller.
    lw      a0, OFF_FP_DEX_INSTRUCTIONS(rFP)
    lw      a1, OFF_FP_DEX_PC(rFP)
    lw      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)
    EAS1(rPC, a0, a1)                            # generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    /* resume execution at catch block */
    EXPORT_PC()
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
 *    rPROFILE       <= signed hotness countdown (expanded to 32 bits)
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
 */
MterpCommonTakenBranchNoFlags:
    bgtz    rINST, .L_forward_branch    # don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
#if JIT_CHECK_OSR != -1
#  error "JIT_CHECK_OSR must be -1."
#endif
    li      t0, JIT_CHECK_OSR
    beq     rPROFILE, t0, .L_osr_check
    blt     rPROFILE, t0, .L_resume_backward_branch
    subu    rPROFILE, 1
    beqz    rPROFILE, .L_add_batch      # counted down to zero - report
.L_resume_backward_branch:
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    REFRESH_IBASE()
    addu    a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB(a2)           # update rPC, load rINST
    and     ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    bnez    ra, .L_suspend_request_pending
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC()
    move    a0, rSELF
    JAL(MterpSuspendCheck)              # (self)
    bnez    v0, MterpFallback
    REFRESH_IBASE()                     # might have changed during suspend
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_no_count_backwards:
    li      t0, JIT_CHECK_OSR           # check for possible OSR re-entry
    bne     rPROFILE, t0, .L_resume_backward_branch
.L_osr_check:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    li      t0, JIT_CHECK_OSR           # check for possible OSR re-entry
    beq     rPROFILE, t0, .L_check_osr_forward
.L_resume_forward_branch:
    add     a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB(a2)           # update rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_check_osr_forward:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    lw      a0, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    JAL(MterpAddHotnessBatch)           # (method, shadow_frame, self)
    move    rPROFILE, v0                # restore new hotness countdown to rPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    li      a2, 2
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    FETCH_ADVANCE_INST(2)
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    JAL(MterpLogOSR)
#endif
    li      v0, 1                       # Signal normal return
    b       MterpDone

/*
 * Bail out to reference interpreter.
 */
MterpFallback:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogFallback)
#endif
MterpCommonFallback:
    move    v0, zero                    # signal retry with reference interpreter.
    b       MterpDone
/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and LR.  Here we restore SP, restore the registers, and then restore
 * LR to PC.
 *
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    li      v0, 1                       # signal return to caller.
    b       MterpDone
MterpReturn:
    lw      a2, OFF_FP_RESULT_REGISTER(rFP)
    sw      v0, 0(a2)
    sw      v1, 4(a2)
    li      v0, 1                       # signal return to caller.
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
    lw      a0, OFF_FP_METHOD(rFP)
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    JAL(MterpAddHotnessBatch)           # (method, shadow_frame, self)
    move    v0, rINST                   # restore return value

.L_pop_and_return:
/* Restore from the stack and return. Frame size = STACK_SIZE */
    STACK_LOAD_FULL()
    jalr    zero, ra

    .cfi_endproc
    .end ExecuteMterpImpl

