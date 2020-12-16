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
#define zero    $$0      /* always zero */
#define AT      $$at     /* assembler temp */
#define v0      $$2      /* return value */
#define v1      $$3
#define a0      $$4      /* argument registers */
#define a1      $$5
#define a2      $$6
#define a3      $$7
#define t0      $$8      /* temp registers (not saved across subroutine calls) */
#define t1      $$9
#define t2      $$10
#define t3      $$11
#define t4      $$12
#define t5      $$13
#define t6      $$14
#define t7      $$15
#define ta0     $$12     /* alias */
#define ta1     $$13
#define ta2     $$14
#define ta3     $$15
#define s0      $$16     /* saved across subroutine calls (callee saved) */
#define s1      $$17
#define s2      $$18
#define s3      $$19
#define s4      $$20
#define s5      $$21
#define s6      $$22
#define s7      $$23
#define t8      $$24     /* two more temp registers */
#define t9      $$25
#define k0      $$26     /* kernel temporary */
#define k1      $$27
#define gp      $$28     /* global pointer */
#define sp      $$29     /* stack pointer */
#define s8      $$30     /* one more callee saved */
#define ra      $$31     /* return address */

/* FP register definitions */
#define fv0    $$f0
#define fv0f   $$f1
#define fv1    $$f2
#define fv1f   $$f3
#define fa0    $$f12
#define fa0f   $$f13
#define fa1    $$f14
#define fa1f   $$f15
#define ft0    $$f4
#define ft0f   $$f5
#define ft1    $$f6
#define ft1f   $$f7
#define ft2    $$f8
#define ft2f   $$f9
#define ft3    $$f10
#define ft3f   $$f11
#define ft4    $$f16
#define ft4f   $$f17
#define ft5    $$f18
#define ft5f   $$f19
#define fs0    $$f20
#define fs0f   $$f21
#define fs1    $$f22
#define fs1f   $$f23
#define fs2    $$f24
#define fs2f   $$f25
#define fs3    $$f26
#define fs3f   $$f27
#define fs4    $$f28
#define fs4f   $$f29
#define fs5    $$f30
#define fs5f   $$f31

#ifndef MIPS32REVGE6
#define fcc0   $$fcc0
#define fcc1   $$fcc1
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
    sll       rd, rd, ${handler_size_bits}; \
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
