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

#include "asm_support_mips64.S"

#include "arch/quick_alloc_entrypoints.S"

    .set noreorder
    .balign 16

    /* Deliver the given exception */
    .extern artDeliverExceptionFromCode
    /* Deliver an exception pending on a thread */
    .extern artDeliverPendingExceptionFromCode

    /*
     * Macro that sets up $gp and stores the previous $gp value to $t8.
     * This macro modifies v1 and t8.
     */
.macro SETUP_GP
    move $v1, $ra
    bal 1f
    nop
1:
    .cpsetup $ra, $t8, 1b
    move $ra, $v1
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     * callee-save: padding + $f24-$f31 + $s0-$s7 + $gp + $ra + $s8 = 19 total + 1x8 bytes padding
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    daddiu $sp, $sp, -160
    .cfi_adjust_cfa_offset 160

     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 160)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(MIPS64) size not as expected."
#endif

    sd     $ra, 152($sp)
    .cfi_rel_offset 31, 152
    sd     $s8, 144($sp)
    .cfi_rel_offset 30, 144
    sd     $t8, 136($sp)           # t8 holds caller's gp, now save it to the stack.
    .cfi_rel_offset 28, 136        # Value from gp is pushed, so set the cfi offset accordingly.
    sd     $s7, 128($sp)
    .cfi_rel_offset 23, 128
    sd     $s6, 120($sp)
    .cfi_rel_offset 22, 120
    sd     $s5, 112($sp)
    .cfi_rel_offset 21, 112
    sd     $s4, 104($sp)
    .cfi_rel_offset 20, 104
    sd     $s3,  96($sp)
    .cfi_rel_offset 19, 96
    sd     $s2,  88($sp)
    .cfi_rel_offset 18, 88
    sd     $s1,  80($sp)
    .cfi_rel_offset 17, 80
    sd     $s0,  72($sp)
    .cfi_rel_offset 16, 72

    // FP callee-saves
    s.d    $f31, 64($sp)
    s.d    $f30, 56($sp)
    s.d    $f29, 48($sp)
    s.d    $f28, 40($sp)
    s.d    $f27, 32($sp)
    s.d    $f26, 24($sp)
    s.d    $f25, 16($sp)
    s.d    $f24,  8($sp)

    # load appropriate callee-save-method
    ld      $t1, %got(_ZN3art7Runtime9instance_E)($gp)
    ld      $t1, 0($t1)
    ld      $t1, RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET($t1)
    sd      $t1, 0($sp)                                # Place ArtMethod* at bottom of stack.
    sd      $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly). Restoration assumes
     * non-moving GC.
     * Does not include rSUSPEND or rSELF
     * callee-save: padding + $s2-$s7 + $gp + $ra + $s8 = 9 total + 1x8 bytes padding
     */
.macro SETUP_SAVE_REFS_ONLY_FRAME
    daddiu $sp, $sp, -80
    .cfi_adjust_cfa_offset 80

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 80)
#error "FRAME_SIZE_SAVE_REFS_ONLY(MIPS64) size not as expected."
#endif

    sd     $ra, 72($sp)
    .cfi_rel_offset 31, 72
    sd     $s8, 64($sp)
    .cfi_rel_offset 30, 64
    sd     $t8, 56($sp)            # t8 holds caller's gp, now save it to the stack.
    .cfi_rel_offset 28, 56         # Value from gp is pushed, so set the cfi offset accordingly.
    sd     $s7, 48($sp)
    .cfi_rel_offset 23, 48
    sd     $s6, 40($sp)
    .cfi_rel_offset 22, 40
    sd     $s5, 32($sp)
    .cfi_rel_offset 21, 32
    sd     $s4, 24($sp)
    .cfi_rel_offset 20, 24
    sd     $s3, 16($sp)
    .cfi_rel_offset 19, 16
    sd     $s2, 8($sp)
    .cfi_rel_offset 18, 8
    # load appropriate callee-save-method
    ld      $t1, %got(_ZN3art7Runtime9instance_E)($gp)
    ld      $t1, 0($t1)
    ld      $t1, RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET($t1)
    sd      $t1, 0($sp)                                # Place Method* at bottom of stack.
    sd      $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
.endm

.macro RESTORE_SAVE_REFS_ONLY_FRAME
    ld     $ra, 72($sp)
    .cfi_restore 31
    ld     $s8, 64($sp)
    .cfi_restore 30
    ld     $t8, 56($sp)            # Restore gp back to it's temp storage.
    .cfi_restore 28
    ld     $s7, 48($sp)
    .cfi_restore 23
    ld     $s6, 40($sp)
    .cfi_restore 22
    ld     $s5, 32($sp)
    .cfi_restore 21
    ld     $s4, 24($sp)
    .cfi_restore 20
    ld     $s3, 16($sp)
    .cfi_restore 19
    ld     $s2, 8($sp)
    .cfi_restore 18
    daddiu $sp, $sp, 80
    .cfi_adjust_cfa_offset -80
    .cpreturn
.endm

.macro RESTORE_SAVE_REFS_ONLY_FRAME_AND_RETURN
    ld     $ra, 72($sp)
    .cfi_restore 31
    ld     $s8, 64($sp)
    .cfi_restore 30
    ld     $t8, 56($sp)            # Restore gp back to it's temp storage.
    .cfi_restore 28
    ld     $s7, 48($sp)
    .cfi_restore 23
    ld     $s6, 40($sp)
    .cfi_restore 22
    ld     $s5, 32($sp)
    .cfi_restore 21
    ld     $s4, 24($sp)
    .cfi_restore 20
    ld     $s3, 16($sp)
    .cfi_restore 19
    ld     $s2, 8($sp)
    .cfi_restore 18
    .cpreturn
    jalr   $zero, $ra
    daddiu $sp, $sp, 80
    .cfi_adjust_cfa_offset -80
.endm

// This assumes the top part of these stack frame types are identical.
#define REFS_AND_ARGS_MINUS_REFS_SIZE (FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY)

    /*
     * Individually usable part of macro SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL.
     */
.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_S4_THRU_S8
    sd      $s8, 192($sp)
    .cfi_rel_offset 30, 192
    sd      $s7, 176($sp)
    .cfi_rel_offset 23, 176
    sd      $s6, 168($sp)
    .cfi_rel_offset 22, 168
    sd      $s5, 160($sp)
    .cfi_rel_offset 21, 160
    sd      $s4, 152($sp)
    .cfi_rel_offset 20, 152
.endm

.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL save_s4_thru_s8=1
    daddiu  $sp, $sp, -208
    .cfi_adjust_cfa_offset 208

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 208)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(MIPS64) size not as expected."
#endif

    sd      $ra, 200($sp)           # = kQuickCalleeSaveFrame_RefAndArgs_LrOffset
    .cfi_rel_offset 31, 200
    sd      $t8, 184($sp)           # t8 holds caller's gp, now save it to the stack.
    .cfi_rel_offset 28, 184         # Value from gp is pushed, so set the cfi offset accordingly.
    .if \save_s4_thru_s8
      SETUP_SAVE_REFS_AND_ARGS_FRAME_S4_THRU_S8
    .endif
    sd      $s3, 144($sp)
    .cfi_rel_offset 19, 144
    sd      $s2, 136($sp)
    .cfi_rel_offset 18, 136
    sd      $a7, 128($sp)
    .cfi_rel_offset 11, 128
    sd      $a6, 120($sp)
    .cfi_rel_offset 10, 120
    sd      $a5, 112($sp)
    .cfi_rel_offset 9, 112
    sd      $a4, 104($sp)
    .cfi_rel_offset 8, 104
    sd      $a3,  96($sp)
    .cfi_rel_offset 7, 96
    sd      $a2,  88($sp)
    .cfi_rel_offset 6, 88
    sd      $a1,  80($sp)           # = kQuickCalleeSaveFrame_RefAndArgs_Gpr1Offset
    .cfi_rel_offset 5, 80

    s.d     $f19, 72($sp)
    s.d     $f18, 64($sp)
    s.d     $f17, 56($sp)
    s.d     $f16, 48($sp)
    s.d     $f15, 40($sp)
    s.d     $f14, 32($sp)
    s.d     $f13, 24($sp)           # = kQuickCalleeSaveFrame_RefAndArgs_Fpr1Offset
    s.d     $f12, 16($sp)           # This isn't necessary to store.
    # 1x8 bytes padding + Method*
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs). Restoration assumes
     * non-moving GC.
     * callee-save: padding + $f12-$f19 + $a1-$a7 + $s2-$s7 + $gp + $ra + $s8 = 24 total + 1 words padding + Method*
     */
.macro SETUP_SAVE_REFS_AND_ARGS_FRAME save_s4_thru_s8_only=0
    .if \save_s4_thru_s8_only
      // It is expected that `SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL /* save_s4_thru_s8 */ 0`
      // has been done prior to `SETUP_SAVE_REFS_AND_ARGS_FRAME /* save_s4_thru_s8_only */ 1`.
      SETUP_SAVE_REFS_AND_ARGS_FRAME_S4_THRU_S8
    .else
      SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
    .endif
    # load appropriate callee-save-method
    ld      $t1, %got(_ZN3art7Runtime9instance_E)($gp)
    ld      $t1, 0($t1)
    ld      $t1, RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET($t1)
    sd      $t1, 0($sp)                                # Place Method* at bottom of stack.
    sd      $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
.endm

.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_A0
    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
    sd      $a0, 0($sp)                                # Place Method* at bottom of stack.
    sd      $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)  # Place sp in Thread::Current()->top_quick_frame.
.endm

    /*
     * Individually usable part of macro RESTORE_SAVE_REFS_AND_ARGS_FRAME.
     */
.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME_A1
    ld      $a1,  80($sp)
    .cfi_restore 5
.endm

.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME restore_s4_thru_s8=1
    ld      $ra, 200($sp)
    .cfi_restore 31
    .if \restore_s4_thru_s8
      ld    $s8, 192($sp)
      .cfi_restore 30
    .endif
    ld      $t8, 184($sp)           # Restore gp back to it's temp storage.
    .cfi_restore 28
    .if \restore_s4_thru_s8
      ld    $s7, 176($sp)
      .cfi_restore 23
      ld    $s6, 168($sp)
      .cfi_restore 22
      ld    $s5, 160($sp)
      .cfi_restore 21
      ld    $s4, 152($sp)
      .cfi_restore 20
    .endif
    ld      $s3, 144($sp)
    .cfi_restore 19
    ld      $s2, 136($sp)
    .cfi_restore 18
    ld      $a7, 128($sp)
    .cfi_restore 11
    ld      $a6, 120($sp)
    .cfi_restore 10
    ld      $a5, 112($sp)
    .cfi_restore 9
    ld      $a4, 104($sp)
    .cfi_restore 8
    ld      $a3,  96($sp)
    .cfi_restore 7
    ld      $a2,  88($sp)
    .cfi_restore 6
    RESTORE_SAVE_REFS_AND_ARGS_FRAME_A1

    l.d     $f19, 72($sp)
    l.d     $f18, 64($sp)
    l.d     $f17, 56($sp)
    l.d     $f16, 48($sp)
    l.d     $f15, 40($sp)
    l.d     $f14, 32($sp)
    l.d     $f13, 24($sp)
    l.d     $f12, 16($sp)

    .cpreturn
    daddiu  $sp, $sp, 208
    .cfi_adjust_cfa_offset -208
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything).
     * when the $sp has already been decremented by FRAME_SIZE_SAVE_EVERYTHING.
     * callee-save: $at + $v0-$v1 + $a0-$a7 + $t0-$t3 + $s0-$s7 + $t8-$t9 + $gp + $s8 + $ra + $s8,
     *              $f0-$f31; 28(GPR)+ 32(FPR) + 1x8 bytes padding + method*
     * This macro sets up $gp; entrypoints using it should start with ENTRY_NO_GP.
     */
.macro SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_EVERYTHING != 496)
#error "FRAME_SIZE_SAVE_EVERYTHING(MIPS64) size not as expected."
#endif

    // Save core registers.
    sd     $ra, 488($sp)
    .cfi_rel_offset 31, 488
    sd     $s8, 480($sp)
    .cfi_rel_offset 30, 480
    sd     $t9, 464($sp)
    .cfi_rel_offset 25, 464
    sd     $t8, 456($sp)
    .cfi_rel_offset 24, 456
    sd     $s7, 448($sp)
    .cfi_rel_offset 23, 448
    sd     $s6, 440($sp)
    .cfi_rel_offset 22, 440
    sd     $s5, 432($sp)
    .cfi_rel_offset 21, 432
    sd     $s4, 424($sp)
    .cfi_rel_offset 20, 424
    sd     $s3,  416($sp)
    .cfi_rel_offset 19, 416
    sd     $s2,  408($sp)
    .cfi_rel_offset 18, 408
    sd     $s1,  400($sp)
    .cfi_rel_offset 17, 400
    sd     $s0,  392($sp)
    .cfi_rel_offset 16, 392
    sd     $t3,  384($sp)
    .cfi_rel_offset 15, 384
    sd     $t2,  376($sp)
    .cfi_rel_offset 14, 376
    sd     $t1,  368($sp)
    .cfi_rel_offset 13, 368
    sd     $t0,  360($sp)
    .cfi_rel_offset 12, 360
    sd     $a7, 352($sp)
    .cfi_rel_offset 11, 352
    sd     $a6, 344($sp)
    .cfi_rel_offset 10, 344
    sd     $a5, 336($sp)
    .cfi_rel_offset 9, 336
    sd     $a4, 328($sp)
    .cfi_rel_offset 8, 328
    sd     $a3,  320($sp)
    .cfi_rel_offset 7, 320
    sd     $a2,  312($sp)
    .cfi_rel_offset 6, 312
    sd     $a1,  304($sp)
    .cfi_rel_offset 5, 304
    sd     $a0,  296($sp)
    .cfi_rel_offset 4, 296
    sd     $v1,  288($sp)
    .cfi_rel_offset 3, 288
    sd     $v0,  280($sp)
    .cfi_rel_offset 2, 280

    // Set up $gp, clobbering $ra and using the branch delay slot for a useful instruction.
    bal 1f
    .set push
    .set noat
    sd     $at,  272($sp)
    .cfi_rel_offset 1, 272
    .set pop
1:
    .cpsetup $ra, 472, 1b

    // Save FP registers.
    s.d    $f31, 264($sp)
    s.d    $f30, 256($sp)
    s.d    $f29, 248($sp)
    s.d    $f28, 240($sp)
    s.d    $f27, 232($sp)
    s.d    $f26, 224($sp)
    s.d    $f25, 216($sp)
    s.d    $f24, 208($sp)
    s.d    $f23, 200($sp)
    s.d    $f22, 192($sp)
    s.d    $f21, 184($sp)
    s.d    $f20, 176($sp)
    s.d    $f19, 168($sp)
    s.d    $f18, 160($sp)
    s.d    $f17, 152($sp)
    s.d    $f16, 144($sp)
    s.d    $f15, 136($sp)
    s.d    $f14, 128($sp)
    s.d    $f13, 120($sp)
    s.d    $f12, 112($sp)
    s.d    $f11, 104($sp)
    s.d    $f10, 96($sp)
    s.d    $f9, 88($sp)
    s.d    $f8, 80($sp)
    s.d    $f7, 72($sp)
    s.d    $f6, 64($sp)
    s.d    $f5, 56($sp)
    s.d    $f4, 48($sp)
    s.d    $f3, 40($sp)
    s.d    $f2, 32($sp)
    s.d    $f1, 24($sp)
    s.d    $f0, 16($sp)

    # load appropriate callee-save-method
    ld      $t1, %got(_ZN3art7Runtime9instance_E)($gp)
    ld      $t1, 0($t1)
    ld      $t1, \runtime_method_offset($t1)
    sd      $t1, 0($sp)                                # Place ArtMethod* at bottom of stack.
    # Place sp in Thread::Current()->top_quick_frame.
    sd      $sp, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything).
     * callee-save: $at + $v0-$v1 + $a0-$a7 + $t0-$t3 + $s0-$s7 + $t8-$t9 + $gp + $s8 + $ra + $s8,
     *              $f0-$f31; 28(GPR)+ 32(FPR) + 1x8 bytes padding + method*
     * This macro sets up $gp; entrypoints using it should start with ENTRY_NO_GP.
     */
.macro SETUP_SAVE_EVERYTHING_FRAME runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    daddiu $sp, $sp, -(FRAME_SIZE_SAVE_EVERYTHING)
    .cfi_adjust_cfa_offset (FRAME_SIZE_SAVE_EVERYTHING)
    SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP \runtime_method_offset
.endm

.macro RESTORE_SAVE_EVERYTHING_FRAME restore_a0=1
    // Restore FP registers.
    l.d    $f31, 264($sp)
    l.d    $f30, 256($sp)
    l.d    $f29, 248($sp)
    l.d    $f28, 240($sp)
    l.d    $f27, 232($sp)
    l.d    $f26, 224($sp)
    l.d    $f25, 216($sp)
    l.d    $f24, 208($sp)
    l.d    $f23, 200($sp)
    l.d    $f22, 192($sp)
    l.d    $f21, 184($sp)
    l.d    $f20, 176($sp)
    l.d    $f19, 168($sp)
    l.d    $f18, 160($sp)
    l.d    $f17, 152($sp)
    l.d    $f16, 144($sp)
    l.d    $f15, 136($sp)
    l.d    $f14, 128($sp)
    l.d    $f13, 120($sp)
    l.d    $f12, 112($sp)
    l.d    $f11, 104($sp)
    l.d    $f10, 96($sp)
    l.d    $f9, 88($sp)
    l.d    $f8, 80($sp)
    l.d    $f7, 72($sp)
    l.d    $f6, 64($sp)
    l.d    $f5, 56($sp)
    l.d    $f4, 48($sp)
    l.d    $f3, 40($sp)
    l.d    $f2, 32($sp)
    l.d    $f1, 24($sp)
    l.d    $f0, 16($sp)

    // Restore core registers.
    .cpreturn
    ld     $ra, 488($sp)
    .cfi_restore 31
    ld     $s8, 480($sp)
    .cfi_restore 30
    ld     $t9, 464($sp)
    .cfi_restore 25
    ld     $t8, 456($sp)
    .cfi_restore 24
    ld     $s7, 448($sp)
    .cfi_restore 23
    ld     $s6, 440($sp)
    .cfi_restore 22
    ld     $s5, 432($sp)
    .cfi_restore 21
    ld     $s4, 424($sp)
    .cfi_restore 20
    ld     $s3,  416($sp)
    .cfi_restore 19
    ld     $s2,  408($sp)
    .cfi_restore 18
    ld     $s1,  400($sp)
    .cfi_restore 17
    ld     $s0,  392($sp)
    .cfi_restore 16
    ld     $t3,  384($sp)
    .cfi_restore 15
    ld     $t2,  376($sp)
    .cfi_restore 14
    ld     $t1,  368($sp)
    .cfi_restore 13
    ld     $t0,  360($sp)
    .cfi_restore 12
    ld     $a7, 352($sp)
    .cfi_restore 11
    ld     $a6, 344($sp)
    .cfi_restore 10
    ld     $a5, 336($sp)
    .cfi_restore 9
    ld     $a4, 328($sp)
    .cfi_restore 8
    ld     $a3,  320($sp)
    .cfi_restore 7
    ld     $a2,  312($sp)
    .cfi_restore 6
    ld     $a1,  304($sp)
    .cfi_restore 5
    .if \restore_a0
    ld     $a0,  296($sp)
    .cfi_restore 4
    .endif
    ld     $v1,  288($sp)
    .cfi_restore 3
    ld     $v0,  280($sp)
    .cfi_restore 2
    .set push
    .set noat
    ld     $at,  272($sp)
    .cfi_restore 1
    .set pop

    daddiu $sp, $sp, 496
    .cfi_adjust_cfa_offset -496
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     * Requires $gp properly set up.
     */
.macro DELIVER_PENDING_EXCEPTION_FRAME_READY
    dla     $t9, artDeliverPendingExceptionFromCode
    jalr    $zero, $t9                   # artDeliverPendingExceptionFromCode(Thread*)
    move    $a0, rSELF                   # pass Thread::Current
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
.macro DELIVER_PENDING_EXCEPTION
    SETUP_GP
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME    # save callee saves for throw
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.endm

.macro RETURN_IF_NO_EXCEPTION
    ld     $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    RESTORE_SAVE_REFS_ONLY_FRAME
    bne    $t0, $zero, 1f                      # success if no exception is pending
    nop
    jalr   $zero, $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_ZERO
    RESTORE_SAVE_REFS_ONLY_FRAME
    bne    $v0, $zero, 1f                # success?
    nop
    jalr   $zero, $ra                    # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
    RESTORE_SAVE_REFS_ONLY_FRAME
    beq    $v0, $zero, 1f                # success?
    nop
    jalr   $zero, $ra                    # return on success
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm

    /*
     * On stack replacement stub.
     * On entry:
     *   a0 = stack to copy
     *   a1 = size of stack
     *   a2 = pc to call
     *   a3 = JValue* result
     *   a4 = shorty
     *   a5 = thread
     */
ENTRY art_quick_osr_stub
    move   $t0, $sp               # save stack pointer
    daddiu $t1, $sp, -112         # reserve stack space
    dsrl   $t1, $t1, 4            # enforce 16 byte stack alignment
    dsll   $sp, $t1, 4            # update stack pointer

    // Save callee general purpose registers, SP, T8(GP), RA, A3, and A4 (8x14 bytes)
    sd     $ra, 104($sp)
    .cfi_rel_offset 31, 104
    sd     $s8, 96($sp)
    .cfi_rel_offset 30, 96
    sd     $t0, 88($sp)           # save original stack pointer stored in t0
    .cfi_rel_offset 29, 88
    sd     $t8, 80($sp)           # t8 holds caller's gp, now save it to the stack.
    .cfi_rel_offset 28, 80        # Value from gp is pushed, so set the cfi offset accordingly.
    sd     $s7, 72($sp)
    .cfi_rel_offset 23, 72
    sd     $s6, 64($sp)
    .cfi_rel_offset 22, 64
    sd     $s5, 56($sp)
    .cfi_rel_offset 21, 56
    sd     $s4, 48($sp)
    .cfi_rel_offset 20, 48
    sd     $s3, 40($sp)
    .cfi_rel_offset 19, 40
    sd     $s2, 32($sp)
    .cfi_rel_offset 18, 32
    sd     $s1, 24($sp)
    .cfi_rel_offset 17, 24
    sd     $s0, 16($sp)
    .cfi_rel_offset 16, 16
    sd     $a4, 8($sp)
    .cfi_rel_offset 8, 8
    sd     $a3, 0($sp)
    .cfi_rel_offset 7, 0
    move   rSELF, $a5                      # Save managed thread pointer into rSELF

    daddiu $sp, $sp, -16
    jal    .Losr_entry
    sd     $zero, 0($sp)                   # Store null for ArtMethod* at bottom of frame
    daddiu $sp, $sp, 16

    // Restore return value address and shorty address
    ld     $a4, 8($sp)                     # shorty address
    .cfi_restore 8
    ld     $a3, 0($sp)                     # result value address
    .cfi_restore 7

    lbu    $t1, 0($a4)                     # load return type
    li     $t2, 'D'                        # put char 'D' into t2
    beq    $t1, $t2, .Losr_fp_result       # branch if result type char == 'D'
    li     $t2, 'F'                        # put char 'F' into t2
    beq    $t1, $t2, .Losr_fp_result       # branch if result type char == 'F'
    nop
    b      .Losr_exit
    dsrl   $v1, $v0, 32                    # put high half of result in v1
.Losr_fp_result:
    mfc1   $v0, $f0
    mfhc1  $v1, $f0                        # put high half of FP result in v1
.Losr_exit:
    sw     $v0, 0($a3)                     # store low half of result
    sw     $v1, 4($a3)                     # store high half of result

    // Restore callee registers
    ld     $ra, 104($sp)
    .cfi_restore 31
    ld     $s8, 96($sp)
    .cfi_restore 30
    ld     $t0, 88($sp)                    # save SP into t0 for now
    .cfi_restore 29
    ld     $t8, 80($sp)                    # Restore gp back to it's temp storage.
    .cfi_restore 28
    ld     $s7, 72($sp)
    .cfi_restore 23
    ld     $s6, 64($sp)
    .cfi_restore 22
    ld     $s5, 56($sp)
    .cfi_restore 21
    ld     $s4, 48($sp)
    .cfi_restore 20
    ld     $s3, 40($sp)
    .cfi_restore 19
    ld     $s2, 32($sp)
    .cfi_restore 18
    ld     $s1, 24($sp)
    .cfi_restore 17
    ld     $s0, 16($sp)
    .cfi_restore 16
    jalr   $zero, $ra
    move   $sp, $t0

.Losr_entry:
    dsubu  $sp, $sp, $a1                   # Reserve space for callee stack
    daddiu $a1, $a1, -8
    daddu  $t0, $a1, $sp
    sw     $ra, 0($t0)                     # Store low half of RA per compiler ABI
    dsrl   $t1, $ra, 32
    sw     $t1, 4($t0)                     # Store high half of RA per compiler ABI

    // Copy arguments into callee stack
    // Use simple copy routine for now.
    // 4 bytes per slot.
    // a0 = source address
    // a1 = args length in bytes (does not include 8 bytes for RA)
    // sp = destination address
    beqz   $a1, .Losr_loop_exit
    daddiu $a1, $a1, -4
    daddu  $t1, $a0, $a1
    daddu  $t2, $sp, $a1
.Losr_loop_entry:
    lw     $t0, 0($t1)
    daddiu $t1, $t1, -4
    sw     $t0, 0($t2)
    bne    $sp, $t2, .Losr_loop_entry
    daddiu $t2, $t2, -4

.Losr_loop_exit:
    move   $t9, $a2
    jalr   $zero, $t9                      # Jump to the OSR entry point.
    nop
END art_quick_osr_stub

    /*
     * On entry $a0 is uint32_t* gprs_ and $a1 is uint32_t* fprs_
     * FIXME: just guessing about the shape of the jmpbuf.  Where will pc be?
     */
ENTRY_NO_GP art_quick_do_long_jump
    l.d     $f0, 0($a1)
    l.d     $f1, 8($a1)
    l.d     $f2, 16($a1)
    l.d     $f3, 24($a1)
    l.d     $f4, 32($a1)
    l.d     $f5, 40($a1)
    l.d     $f6, 48($a1)
    l.d     $f7, 56($a1)
    l.d     $f8, 64($a1)
    l.d     $f9, 72($a1)
    l.d     $f10, 80($a1)
    l.d     $f11, 88($a1)
    l.d     $f12, 96($a1)
    l.d     $f13, 104($a1)
    l.d     $f14, 112($a1)
    l.d     $f15, 120($a1)
    l.d     $f16, 128($a1)
    l.d     $f17, 136($a1)
    l.d     $f18, 144($a1)
    l.d     $f19, 152($a1)
    l.d     $f20, 160($a1)
    l.d     $f21, 168($a1)
    l.d     $f22, 176($a1)
    l.d     $f23, 184($a1)
    l.d     $f24, 192($a1)
    l.d     $f25, 200($a1)
    l.d     $f26, 208($a1)
    l.d     $f27, 216($a1)
    l.d     $f28, 224($a1)
    l.d     $f29, 232($a1)
    l.d     $f30, 240($a1)
    l.d     $f31, 248($a1)
    .set push
    .set nomacro
    .set noat
# no need to load zero
    ld      $at, 8($a0)
    .set pop
    ld      $v0, 16($a0)
    ld      $v1, 24($a0)
# a0 has to be loaded last
    ld      $a1, 40($a0)
    ld      $a2, 48($a0)
    ld      $a3, 56($a0)
    ld      $a4, 64($a0)
    ld      $a5, 72($a0)
    ld      $a6, 80($a0)
    ld      $a7, 88($a0)
    ld      $t0, 96($a0)
    ld      $t1, 104($a0)
    ld      $t2, 112($a0)
    ld      $t3, 120($a0)
    ld      $s0, 128($a0)
    ld      $s1, 136($a0)
    ld      $s2, 144($a0)
    ld      $s3, 152($a0)
    ld      $s4, 160($a0)
    ld      $s5, 168($a0)
    ld      $s6, 176($a0)
    ld      $s7, 184($a0)
    ld      $t8, 192($a0)
    ld      $t9, 200($a0)
# no need to load k0, k1
    ld      $gp, 224($a0)
    ld      $sp, 232($a0)
    ld      $s8, 240($a0)
    ld      $ra, 248($a0)
    ld      $a0, 32($a0)
    move    $v0, $zero          # clear result registers v0 and v1
    jalr    $zero, $t9          # do long jump (do not use ra, it must not be clobbered)
    move    $v1, $zero
END art_quick_do_long_jump

    /*
     * Called by managed code, saves most registers (forms basis of long jump
     * context) and passes the bottom of the stack.
     * artDeliverExceptionFromCode will place the callee save Method* at
     * the bottom of the thread. On entry a0 holds Throwable*
     */
ENTRY art_quick_deliver_exception
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    dla  $t9, artDeliverExceptionFromCode
    jalr $zero, $t9                 # artDeliverExceptionFromCode(Throwable*, Thread*)
    move $a1, rSELF                 # pass Thread::Current
END art_quick_deliver_exception

    /*
     * Called by managed code to create and deliver a NullPointerException
     */
    .extern artThrowNullPointerExceptionFromCode
ENTRY_NO_GP art_quick_throw_null_pointer_exception
    // Note that setting up $gp does not rely on $t9 here, so branching here directly is OK,
    // even after clobbering any registers we don't need to preserve, such as $gp or $t0.
    SETUP_SAVE_EVERYTHING_FRAME
    dla  $t9, artThrowNullPointerExceptionFromCode
    jalr $zero, $t9                 # artThrowNullPointerExceptionFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_null_pointer_exception

    /*
     * Call installed by a signal handler to create and deliver a NullPointerException
     */
    .extern artThrowNullPointerExceptionFromSignal
ENTRY_NO_GP_CUSTOM_CFA art_quick_throw_null_pointer_exception_from_signal, FRAME_SIZE_SAVE_EVERYTHING
    SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP
    # Retrieve the fault address from the padding where the signal handler stores it.
    ld   $a0, (__SIZEOF_POINTER__)($sp)
    dla  $t9, artThrowNullPointerExceptionFromSignal
    jalr $zero, $t9                 # artThrowNullPointerExceptionFromSignal(uinptr_t, Thread*)
    move $a1, rSELF                 # pass Thread::Current
END art_quick_throw_null_pointer_exception_from_signal

    /*
     * Called by managed code to create and deliver an ArithmeticException
     */
    .extern artThrowDivZeroFromCode
ENTRY_NO_GP art_quick_throw_div_zero
    SETUP_SAVE_EVERYTHING_FRAME
    dla  $t9, artThrowDivZeroFromCode
    jalr $zero, $t9                 # artThrowDivZeroFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_div_zero

    /*
     * Called by managed code to create and deliver an
     * ArrayIndexOutOfBoundsException
     */
    .extern artThrowArrayBoundsFromCode
ENTRY_NO_GP art_quick_throw_array_bounds
    // Note that setting up $gp does not rely on $t9 here, so branching here directly is OK,
    // even after clobbering any registers we don't need to preserve, such as $gp or $t0.
    SETUP_SAVE_EVERYTHING_FRAME
    dla  $t9, artThrowArrayBoundsFromCode
    jalr $zero, $t9                 # artThrowArrayBoundsFromCode(index, limit, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_throw_array_bounds

    /*
     * Called by managed code to create and deliver a StringIndexOutOfBoundsException
     * as if thrown from a call to String.charAt().
     */
    .extern artThrowStringBoundsFromCode
ENTRY_NO_GP art_quick_throw_string_bounds
    SETUP_SAVE_EVERYTHING_FRAME
    dla  $t9, artThrowStringBoundsFromCode
    jalr $zero, $t9                 # artThrowStringBoundsFromCode(index, limit, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_throw_string_bounds

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
    .extern artThrowStackOverflowFromCode
ENTRY art_quick_throw_stack_overflow
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    dla  $t9, artThrowStackOverflowFromCode
    jalr $zero, $t9                 # artThrowStackOverflowFromCode(Thread*)
    move $a0, rSELF                 # pass Thread::Current
END art_quick_throw_stack_overflow

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/$a0 with the target Method*, arg0/$a0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visable argument of the target, and so can be found in arg1/$a1.
     *
     * The helper will attempt to locate the target and return a 128-bit result in $v0/$v1 consisting
     * of the target Method* in $v0 and method->code_ in $v1.
     *
     * If unsuccessful, the helper will return null/null. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the ra
     * pointing back to the original caller.
     */
.macro INVOKE_TRAMPOLINE_BODY cxx_name, save_s4_thru_s8_only=0
    .extern \cxx_name
    SETUP_SAVE_REFS_AND_ARGS_FRAME \save_s4_thru_s8_only  # save callee saves in case
                                                          # allocation triggers GC
    move  $a2, rSELF                       # pass Thread::Current
    jal   \cxx_name                        # (method_idx, this, Thread*, $sp)
    move  $a3, $sp                         # pass $sp
    move  $a0, $v0                         # save target Method*
    move  $t9, $v1                         # save $v0->code_
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    beq   $v0, $zero, 1f
    nop
    jalr  $zero, $t9
    nop
1:
    DELIVER_PENDING_EXCEPTION
.endm
.macro INVOKE_TRAMPOLINE c_name, cxx_name
ENTRY \c_name
    INVOKE_TRAMPOLINE_BODY \cxx_name
END \c_name
.endm

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck

    # On entry:
    #   t0 = shorty
    #   t1 = ptr to arg_array
    #   t2 = number of argument bytes remain
    #   v0 = ptr to stack frame where to copy arg_array
    # This macro modifies t3, t9 and v0
.macro LOOP_OVER_SHORTY_LOADING_REG gpu, fpu, label
    lbu    $t3, 0($t0)           # get argument type from shorty
    beqz   $t3, \label
    daddiu $t0, 1
    li     $t9, 68               # put char 'D' into t9
    beq    $t9, $t3, 1f          # branch if result type char == 'D'
    li     $t9, 70               # put char 'F' into t9
    beq    $t9, $t3, 2f          # branch if result type char == 'F'
    li     $t9, 74               # put char 'J' into t9
    beq    $t9, $t3, 3f          # branch if result type char == 'J'
    nop
    lw     $\gpu, 0($t1)
    sw     $\gpu, 0($v0)
    daddiu $v0, 4
    daddiu $t1, 4
    b      4f
    daddiu $t2, -4               # delay slot

1:  # found double
    lwu    $t3, 0($t1)
    mtc1   $t3, $\fpu
    sw     $t3, 0($v0)
    lwu    $t3, 4($t1)
    mthc1  $t3, $\fpu
    sw     $t3, 4($v0)
    daddiu $v0, 8
    daddiu $t1, 8
    b      4f
    daddiu $t2, -8               # delay slot

2:  # found float
    lwu    $t3, 0($t1)
    mtc1   $t3, $\fpu
    sw     $t3, 0($v0)
    daddiu $v0, 4
    daddiu $t1, 4
    b      4f
    daddiu $t2, -4               # delay slot

3:  # found long (8 bytes)
    lwu    $t3, 0($t1)
    sw     $t3, 0($v0)
    lwu    $t9, 4($t1)
    sw     $t9, 4($v0)
    dsll   $t9, $t9, 32
    or     $\gpu, $t9, $t3
    daddiu $v0, 8
    daddiu $t1, 8
    daddiu $t2, -8
4:
.endm

    /*
     * Invocation stub for quick code.
     * On entry:
     *   a0 = method pointer
     *   a1 = argument array that must at least contain the this ptr.
     *   a2 = size of argument array in bytes
     *   a3 = (managed) thread pointer
     *   a4 = JValue* result
     *   a5 = shorty
     */
ENTRY_NO_GP art_quick_invoke_stub
    # push a4, a5, s0(rSUSPEND), s1(rSELF), s8, ra onto the stack
    daddiu $sp, $sp, -48
    .cfi_adjust_cfa_offset 48
    sd     $ra, 40($sp)
    .cfi_rel_offset 31, 40
    sd     $s8, 32($sp)
    .cfi_rel_offset 30, 32
    sd     $s1, 24($sp)
    .cfi_rel_offset 17, 24
    sd     $s0, 16($sp)
    .cfi_rel_offset 16, 16
    sd     $a5, 8($sp)
    .cfi_rel_offset 9, 8
    sd     $a4, 0($sp)
    .cfi_rel_offset 8, 0

    move   $s1, $a3              # move managed thread pointer into s1 (rSELF)
    move   $s8, $sp              # save sp in s8 (fp)

    daddiu $t3, $a2, 24          # add 8 for ArtMethod* and 16 for stack alignment
    dsrl   $t3, $t3, 4           # shift the frame size right 4
    dsll   $t3, $t3, 4           # shift the frame size left 4 to align to 16 bytes
    dsubu  $sp, $sp, $t3         # reserve stack space for argument array

    daddiu $t0, $a5, 1           # t0 = shorty[1] (skip 1 for return type)
    daddiu $t1, $a1, 4           # t1 = ptr to arg_array[4] (skip this ptr)
    daddiu $t2, $a2, -4          # t2 = number of argument bytes remain (skip this ptr)
    daddiu $v0, $sp, 12          # v0 points to where to copy arg_array
    LOOP_OVER_SHORTY_LOADING_REG a2, f14, call_fn
    LOOP_OVER_SHORTY_LOADING_REG a3, f15, call_fn
    LOOP_OVER_SHORTY_LOADING_REG a4, f16, call_fn
    LOOP_OVER_SHORTY_LOADING_REG a5, f17, call_fn
    LOOP_OVER_SHORTY_LOADING_REG a6, f18, call_fn
    LOOP_OVER_SHORTY_LOADING_REG a7, f19, call_fn

    # copy arguments onto stack (t2 should be multiples of 4)
    ble    $t2, $zero, call_fn   # t2 = number of argument bytes remain
1:
    lw     $t3, 0($t1)           # load from argument array
    daddiu $t1, $t1, 4
    sw     $t3, 0($v0)           # save to stack
    daddiu $t2, -4
    bgt    $t2, $zero, 1b        # t2 = number of argument bytes remain
    daddiu $v0, $v0, 4

call_fn:
    # call method (a0 and a1 have been untouched)
    lwu    $a1, 0($a1)           # make a1 = this ptr
    sw     $a1, 8($sp)           # copy this ptr (skip 8 bytes for ArtMethod*)
    sd     $zero, 0($sp)         # store null for ArtMethod* at bottom of frame
    ld     $t9, ART_METHOD_QUICK_CODE_OFFSET_64($a0)  # get pointer to the code
    jalr   $t9                   # call the method
    nop
    move   $sp, $s8              # restore sp

    # pop a4, a5, s1(rSELF), s8, ra off of the stack
    ld     $a4, 0($sp)
    .cfi_restore 8
    ld     $a5, 8($sp)
    .cfi_restore 9
    ld     $s0, 16($sp)
    .cfi_restore 16
    ld     $s1, 24($sp)
    .cfi_restore 17
    ld     $s8, 32($sp)
    .cfi_restore 30
    ld     $ra, 40($sp)
    .cfi_restore 31
    daddiu $sp, $sp, 48
    .cfi_adjust_cfa_offset -48

    # a4 = JValue* result
    # a5 = shorty string
    lbu   $t1, 0($a5)           # get result type from shorty
    li    $t2, 68               # put char 'D' into t2
    beq   $t1, $t2, 1f          # branch if result type char == 'D'
    li    $t3, 70               # put char 'F' into t3
    beq   $t1, $t3, 1f          # branch if result type char == 'F'
    sw    $v0, 0($a4)           # store the result
    dsrl  $v1, $v0, 32
    jalr  $zero, $ra
    sw    $v1, 4($a4)           # store the other half of the result
1:
    mfc1  $v0, $f0
    mfhc1 $v1, $f0
    sw    $v0, 0($a4)           # store the result
    jalr  $zero, $ra
    sw    $v1, 4($a4)           # store the other half of the result
END art_quick_invoke_stub

    /*
     * Invocation static stub for quick code.
     * On entry:
     *   a0 = method pointer
     *   a1 = argument array that must at least contain the this ptr.
     *   a2 = size of argument array in bytes
     *   a3 = (managed) thread pointer
     *   a4 = JValue* result
     *   a5 = shorty
     */
ENTRY_NO_GP art_quick_invoke_static_stub

    # push a4, a5, s0(rSUSPEND), s1(rSELF), s8, ra, onto the stack
    daddiu $sp, $sp, -48
    .cfi_adjust_cfa_offset 48
    sd     $ra, 40($sp)
    .cfi_rel_offset 31, 40
    sd     $s8, 32($sp)
    .cfi_rel_offset 30, 32
    sd     $s1, 24($sp)
    .cfi_rel_offset 17, 24
    sd     $s0, 16($sp)
    .cfi_rel_offset 16, 16
    sd     $a5, 8($sp)
    .cfi_rel_offset 9, 8
    sd     $a4, 0($sp)
    .cfi_rel_offset 8, 0

    move   $s1, $a3              # move managed thread pointer into s1 (rSELF)
    move   $s8, $sp              # save sp in s8 (fp)

    daddiu $t3, $a2, 24          # add 8 for ArtMethod* and 16 for stack alignment
    dsrl   $t3, $t3, 4           # shift the frame size right 4
    dsll   $t3, $t3, 4           # shift the frame size left 4 to align to 16 bytes
    dsubu  $sp, $sp, $t3         # reserve stack space for argument array

    daddiu $t0, $a5, 1           # t0 = shorty[1] (skip 1 for return type)
    move   $t1, $a1              # t1 = arg_array
    move   $t2, $a2              # t2 = number of argument bytes remain
    daddiu $v0, $sp, 8           # v0 points to where to copy arg_array
    LOOP_OVER_SHORTY_LOADING_REG a1, f13, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a2, f14, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a3, f15, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a4, f16, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a5, f17, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a6, f18, call_sfn
    LOOP_OVER_SHORTY_LOADING_REG a7, f19, call_sfn

    # copy arguments onto stack (t2 should be multiples of 4)
    ble    $t2, $zero, call_sfn  # t2 = number of argument bytes remain
1:
    lw     $t3, 0($t1)           # load from argument array
    daddiu $t1, $t1, 4
    sw     $t3, 0($v0)           # save to stack
    daddiu $t2, -4
    bgt    $t2, $zero, 1b        # t2 = number of argument bytes remain
    daddiu $v0, $v0, 4

call_sfn:
    # call method (a0 has been untouched)
    sd     $zero, 0($sp)         # store null for ArtMethod* at bottom of frame
    ld     $t9, ART_METHOD_QUICK_CODE_OFFSET_64($a0)  # get pointer to the code
    jalr   $t9                   # call the method
    nop
    move   $sp, $s8              # restore sp

    # pop a4, a5, s0(rSUSPEND), s1(rSELF), s8, ra off of the stack
    ld     $a4, 0($sp)
    .cfi_restore 8
    ld     $a5, 8($sp)
    .cfi_restore 9
    ld     $s0, 16($sp)
    .cfi_restore 16
    ld     $s1, 24($sp)
    .cfi_restore 17
    ld     $s8, 32($sp)
    .cfi_restore 30
    ld     $ra, 40($sp)
    .cfi_restore 31
    daddiu $sp, $sp, 48
    .cfi_adjust_cfa_offset -48

    # a4 = JValue* result
    # a5 = shorty string
    lbu   $t1, 0($a5)           # get result type from shorty
    li    $t2, 68               # put char 'D' into t2
    beq   $t1, $t2, 1f          # branch if result type char == 'D'
    li    $t3, 70               # put char 'F' into t3
    beq   $t1, $t3, 1f          # branch if result type char == 'F'
    sw    $v0, 0($a4)           # store the result
    dsrl  $v1, $v0, 32
    jalr  $zero, $ra
    sw    $v1, 4($a4)           # store the other half of the result
1:
    mfc1  $v0, $f0
    mfhc1 $v1, $f0
    sw    $v0, 0($a4)           # store the result
    jalr  $zero, $ra
    sw    $v1, 4($a4)           # store the other half of the result
END art_quick_invoke_static_stub

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and
     * delivers exception on failure.
     */
    .extern artHandleFillArrayDataFromCode
ENTRY art_quick_handle_fill_data
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case exception allocation triggers GC
    ld      $a2, FRAME_SIZE_SAVE_REFS_ONLY($sp)         # pass referrer's Method*
    jal     artHandleFillArrayDataFromCode              # (payload offset, Array*, method, Thread*)
    move    $a3, rSELF                                  # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_handle_fill_data

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC.
     */
    .extern artLockObjectFromCode
ENTRY_NO_GP art_quick_lock_object
    beqzc   $a0, art_quick_throw_null_pointer_exception
    li      $t8, LOCK_WORD_THIN_LOCK_COUNT_ONE
    li      $t3, LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED
.Lretry_lock:
    lw      $t0, THREAD_ID_OFFSET(rSELF)  # TODO: Can the thread ID really change during the loop?
    ll      $t1, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
    and     $t2, $t1, $t3                 # zero the gc bits
    bnezc   $t2, .Lnot_unlocked           # already thin locked
    # Unlocked case - $t1: original lock word that's zero except for the read barrier bits.
    or      $t2, $t1, $t0                 # $t2 holds thread id with count of 0 with preserved read barrier bits
    sc      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
    beqzc   $t2, .Lretry_lock             # store failed, retry
    sync                                  # full (LoadLoad|LoadStore) memory barrier
    jic     $ra, 0
.Lnot_unlocked:
    # $t1: original lock word, $t0: thread_id with count of 0 and zero read barrier bits
    srl     $t2, $t1, LOCK_WORD_STATE_SHIFT
    bnezc   $t2, .Lslow_lock              # if either of the top two bits are set, go slow path
    xor     $t2, $t1, $t0                 # lock_word.ThreadId() ^ self->ThreadId()
    andi    $t2, $t2, 0xFFFF              # zero top 16 bits
    bnezc   $t2, .Lslow_lock              # lock word and self thread id's match -> recursive lock
                                          # otherwise contention, go to slow path
    and     $t2, $t1, $t3                 # zero the gc bits
    addu    $t2, $t2, $t8                 # increment count in lock word
    srl     $t2, $t2, LOCK_WORD_STATE_SHIFT  # if the first gc state bit is set, we overflowed.
    bnezc   $t2, .Lslow_lock              # if we overflow the count go slow path
    addu    $t2, $t1, $t8                 # increment count for real
    sc      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
    beqzc   $t2, .Lretry_lock             # store failed, retry
    nop
    jic     $ra, 0
.Lslow_lock:
    .cpsetup $t9, $t8, art_quick_lock_object
    SETUP_SAVE_REFS_ONLY_FRAME            # save callee saves in case we block
    jal     artLockObjectFromCode         # (Object* obj, Thread*)
    move    $a1, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_lock_object

ENTRY_NO_GP art_quick_lock_object_no_inline
    beq     $a0, $zero, art_quick_throw_null_pointer_exception
    nop
    .cpsetup $t9, $t8, art_quick_lock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME            # save callee saves in case we block
    jal     artLockObjectFromCode         # (Object* obj, Thread*)
    move    $a1, rSELF                    # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_lock_object_no_inline

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     */
    .extern artUnlockObjectFromCode
ENTRY_NO_GP art_quick_unlock_object
    beqzc   $a0, art_quick_throw_null_pointer_exception
    li      $t8, LOCK_WORD_THIN_LOCK_COUNT_ONE
    li      $t3, LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED
.Lretry_unlock:
#ifndef USE_READ_BARRIER
    lw      $t1, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
#else
    ll      $t1, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)  # Need to use atomic read-modify-write for read barrier
#endif
    srl     $t2, $t1, LOCK_WORD_STATE_SHIFT
    bnezc   $t2, .Lslow_unlock         # if either of the top two bits are set, go slow path
    lw      $t0, THREAD_ID_OFFSET(rSELF)
    and     $t2, $t1, $t3              # zero the gc bits
    xor     $t2, $t2, $t0              # lock_word.ThreadId() ^ self->ThreadId()
    andi    $t2, $t2, 0xFFFF           # zero top 16 bits
    bnezc   $t2, .Lslow_unlock         # do lock word and self thread id's match?
    and     $t2, $t1, $t3              # zero the gc bits
    bgeuc   $t2, $t8, .Lrecursive_thin_unlock
    # transition to unlocked
    nor     $t2, $zero, $t3            # $t2 = LOCK_WORD_GC_STATE_MASK_SHIFTED
    and     $t2, $t1, $t2              # $t2: zero except for the preserved gc bits
    sync                               # full (LoadStore|StoreStore) memory barrier
#ifndef USE_READ_BARRIER
    sw      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
#else
    sc      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
    beqzc   $t2, .Lretry_unlock        # store failed, retry
    nop
#endif
    jic     $ra, 0
.Lrecursive_thin_unlock:
    # t1: original lock word
    subu    $t2, $t1, $t8              # decrement count
#ifndef USE_READ_BARRIER
    sw      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
#else
    sc      $t2, MIRROR_OBJECT_LOCK_WORD_OFFSET($a0)
    beqzc   $t2, .Lretry_unlock        # store failed, retry
    nop
#endif
    jic     $ra, 0
.Lslow_unlock:
    .cpsetup $t9, $t8, art_quick_unlock_object
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case exception allocation triggers GC
    jal     artUnlockObjectFromCode    # (Object* obj, Thread*)
    move    $a1, rSELF                 # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_unlock_object

ENTRY_NO_GP art_quick_unlock_object_no_inline
    beq     $a0, $zero, art_quick_throw_null_pointer_exception
    nop
    .cpsetup $t9, $t8, art_quick_unlock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case exception allocation triggers GC
    jal     artUnlockObjectFromCode    # (Object* obj, Thread*)
    move    $a1, rSELF                 # pass Thread::Current
    RETURN_IF_ZERO
END art_quick_unlock_object_no_inline

    /*
     * Entry from managed code that calls artInstanceOfFromCode and delivers exception on failure.
     */
    .extern artInstanceOfFromCode
    .extern artThrowClassCastExceptionForObject
ENTRY art_quick_check_instance_of
    daddiu $sp, $sp, -32
    .cfi_adjust_cfa_offset 32
    sd     $ra, 24($sp)
    .cfi_rel_offset 31, 24
    sd     $t9, 16($sp)
    sd     $a1, 8($sp)
    sd     $a0, 0($sp)
    jal    artInstanceOfFromCode
    .cpreturn                       # Restore gp from t8 in branch delay slot.
                                    # t8 may be clobbered in artIsAssignableFromCode.
    beq    $v0, $zero, .Lthrow_class_cast_exception
    ld     $ra, 24($sp)
    jalr   $zero, $ra
    daddiu $sp, $sp, 32
    .cfi_adjust_cfa_offset -32
.Lthrow_class_cast_exception:
    ld     $t9, 16($sp)
    ld     $a1, 8($sp)
    ld     $a0, 0($sp)
    daddiu $sp, $sp, 32
    .cfi_adjust_cfa_offset -32
    SETUP_GP
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    dla  $t9, artThrowClassCastExceptionForObject
    jalr $zero, $t9                 # artThrowClassCastException (Object*, Class*, Thread*)
    move $a2, rSELF                 # pass Thread::Current
END art_quick_check_instance_of


    /*
     * Restore rReg's value from offset($sp) if rReg is not the same as rExclude.
     * nReg is the register number for rReg.
     */
.macro POP_REG_NE rReg, nReg, offset, rExclude
    .ifnc \rReg, \rExclude
        ld \rReg, \offset($sp)      # restore rReg
        .cfi_restore \nReg
    .endif
.endm

    /*
     * Macro to insert read barrier, only used in art_quick_aput_obj.
     * rObj and rDest are registers, offset is a defined literal such as MIRROR_OBJECT_CLASS_OFFSET.
     * TODO: When read barrier has a fast path, add heap unpoisoning support for the fast path.
     */
.macro READ_BARRIER rDest, rObj, offset
#ifdef USE_READ_BARRIER
    # saved registers used in art_quick_aput_obj: a0-a2, t0-t1, t9, ra. 16B-aligned.
    daddiu  $sp, $sp, -64
    .cfi_adjust_cfa_offset 64
    sd     $ra, 56($sp)
    .cfi_rel_offset 31, 56
    sd     $t9, 48($sp)
    .cfi_rel_offset 25, 48
    sd     $t1, 40($sp)
    .cfi_rel_offset 13, 40
    sd     $t0, 32($sp)
    .cfi_rel_offset 12, 32
    sd     $a2, 16($sp)             # padding slot at offset 24 (padding can be any slot in the 64B)
    .cfi_rel_offset 6, 16
    sd     $a1, 8($sp)
    .cfi_rel_offset 5, 8
    sd     $a0, 0($sp)
    .cfi_rel_offset 4, 0

    # move $a0, \rRef               # pass ref in a0 (no-op for now since parameter ref is unused)
    .ifnc \rObj, $a1
        move $a1, \rObj             # pass rObj
    .endif
    daddiu $a2, $zero, \offset      # pass offset
    jal artReadBarrierSlow          # artReadBarrierSlow(ref, rObj, offset)
    .cpreturn                       # Restore gp from t8 in branch delay slot.
                                    # t8 may be clobbered in artReadBarrierSlow.
    # No need to unpoison return value in v0, artReadBarrierSlow() would do the unpoisoning.
    move \rDest, $v0                # save return value in rDest
                                    # (rDest cannot be v0 in art_quick_aput_obj)

    ld     $a0, 0($sp)              # restore registers except rDest
                                    # (rDest can only be t0 or t1 in art_quick_aput_obj)
    .cfi_restore 4
    ld     $a1, 8($sp)
    .cfi_restore 5
    ld     $a2, 16($sp)
    .cfi_restore 6
    POP_REG_NE $t0, 12, 32, \rDest
    POP_REG_NE $t1, 13, 40, \rDest
    ld     $t9, 48($sp)
    .cfi_restore 25
    ld     $ra, 56($sp)             # restore $ra
    .cfi_restore 31
    daddiu  $sp, $sp, 64
    .cfi_adjust_cfa_offset -64
    SETUP_GP                        # set up gp because we are not returning
#else
    lwu     \rDest, \offset(\rObj)
    UNPOISON_HEAP_REF \rDest
#endif  // USE_READ_BARRIER
.endm

ENTRY art_quick_aput_obj
    beq  $a2, $zero, .Ldo_aput_null
    nop
    READ_BARRIER $t0, $a0, MIRROR_OBJECT_CLASS_OFFSET
    READ_BARRIER $t1, $a2, MIRROR_OBJECT_CLASS_OFFSET
    READ_BARRIER $t0, $t0, MIRROR_CLASS_COMPONENT_TYPE_OFFSET
    bne $t1, $t0, .Lcheck_assignability  # value's type == array's component type - trivial assignability
    nop
.Ldo_aput:
    dsll  $a1, $a1, 2
    daddu $t0, $a0, $a1
    POISON_HEAP_REF $a2
    sw   $a2, MIRROR_OBJECT_ARRAY_DATA_OFFSET($t0)
    ld   $t0, THREAD_CARD_TABLE_OFFSET(rSELF)
    dsrl  $t1, $a0, CARD_TABLE_CARD_SHIFT
    daddu $t1, $t1, $t0
    sb   $t0, ($t1)
    jalr $zero, $ra
    .cpreturn                       # Restore gp from t8 in branch delay slot.
.Ldo_aput_null:
    dsll  $a1, $a1, 2
    daddu $t0, $a0, $a1
    sw   $a2, MIRROR_OBJECT_ARRAY_DATA_OFFSET($t0)
    jalr $zero, $ra
    .cpreturn                       # Restore gp from t8 in branch delay slot.
.Lcheck_assignability:
    daddiu $sp, $sp, -64
    .cfi_adjust_cfa_offset 64
    sd     $ra, 56($sp)
    .cfi_rel_offset 31, 56
    sd     $t9, 24($sp)
    sd     $a2, 16($sp)
    sd     $a1, 8($sp)
    sd     $a0, 0($sp)
    move   $a1, $t1
    move   $a0, $t0
    jal    artIsAssignableFromCode  # (Class*, Class*)
    .cpreturn                       # Restore gp from t8 in branch delay slot.
                                    # t8 may be clobbered in artIsAssignableFromCode.
    ld     $ra, 56($sp)
    ld     $t9, 24($sp)
    ld     $a2, 16($sp)
    ld     $a1, 8($sp)
    ld     $a0, 0($sp)
    daddiu $sp, $sp, 64
    .cfi_adjust_cfa_offset -64
    SETUP_GP
    bne    $v0, $zero, .Ldo_aput
    nop
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    move   $a1, $a2
    dla  $t9, artThrowArrayStoreException
    jalr $zero, $t9                 # artThrowArrayStoreException(Class*, Class*, Thread*)
    move   $a2, rSELF               # pass Thread::Current
END art_quick_aput_obj

// Macros taking opportunity of code similarities for downcalls.
.macro ONE_ARG_REF_DOWNCALL name, entrypoint, return, extend=0
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        # save callee saves in case of GC
    dla     $t9, \entrypoint
    jalr    $t9                       # (field_idx, Thread*)
    move    $a1, rSELF                # pass Thread::Current
    .if     \extend
    sll     $v0, $v0, 0               # sign-extend 32-bit result
    .endif
    \return                           # RETURN_IF_NO_EXCEPTION or RETURN_IF_ZERO
END \name
.endm

.macro TWO_ARG_REF_DOWNCALL name, entrypoint, return, extend=0
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        # save callee saves in case of GC
    dla     $t9, \entrypoint
    jalr    $t9                       # (field_idx, Object*, Thread*) or
                                      # (field_idx, new_val, Thread*)
    move    $a2, rSELF                # pass Thread::Current
    .if     \extend
    sll     $v0, $v0, 0               # sign-extend 32-bit result
    .endif
    \return                           # RETURN_IF_NO_EXCEPTION or RETURN_IF_ZERO
END \name
.endm

.macro THREE_ARG_REF_DOWNCALL name, entrypoint, return, extend=0
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        # save callee saves in case of GC
    dla     $t9, \entrypoint
    jalr    $t9                       # (field_idx, Object*, new_val, Thread*)
    move    $a3, rSELF                # pass Thread::Current
    .if     \extend
    sll     $v0, $v0, 0               # sign-extend 32-bit result
    .endif
    \return                           # RETURN_IF_NO_EXCEPTION or RETURN_IF_ZERO
END \name
.endm

    /*
     * Called by managed code to resolve a static/instance field and load/store a value.
     *
     * Note: Functions `art{Get,Set}<Kind>{Static,Instance}FromCompiledCode` are
     * defined with a macro in runtime/entrypoints/quick/quick_field_entrypoints.cc.
     */
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCompiledCode, RETURN_IF_NO_EXCEPTION, 1
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION, 1
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCompiledCode, RETURN_IF_NO_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCompiledCode, RETURN_IF_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCompiledCode, RETURN_IF_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCompiledCode, RETURN_IF_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCompiledCode, RETURN_IF_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set64_static, artSet64StaticFromCompiledCode, RETURN_IF_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCompiledCode, RETURN_IF_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCompiledCode, RETURN_IF_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCompiledCode, RETURN_IF_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCompiledCode, RETURN_IF_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set64_instance, artSet64InstanceFromCompiledCode, RETURN_IF_ZERO

// Macro to facilitate adding new allocation entrypoints.
.macro ONE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case of GC
    jal     \entrypoint
    move    $a1, rSELF                 # pass Thread::Current
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case of GC
    jal     \entrypoint
    move    $a2, rSELF                 # pass Thread::Current
    \return
END \name
.endm

.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case of GC
    jal     \entrypoint
    move    $a3, rSELF                 # pass Thread::Current
    \return
END \name
.endm

.macro FOUR_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME         # save callee saves in case of GC
    jal     \entrypoint
    move    $a4, rSELF                 # pass Thread::Current
    \return
END \name
.endm

// Generate the allocation entrypoints for each allocator.
GENERATE_ALLOC_ENTRYPOINTS_FOR_NON_TLAB_ALLOCATORS
// Comment out allocators that have mips64 specific asm.
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED8(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED16(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED32(_region_tlab, RegionTLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED64(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_region_tlab, RegionTLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_region_tlab, RegionTLAB)

// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_WITH_ACCESS_CHECK(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED8(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED16(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED32(_tlab, TLAB)
// GENERATE_ALLOC_ENTRYPOINTS_ALLOC_ARRAY_RESOLVED64(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_BYTES(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_CHARS(_tlab, TLAB)
GENERATE_ALLOC_ENTRYPOINTS_ALLOC_STRING_FROM_STRING(_tlab, TLAB)

// A hand-written override for:
//   GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc, RosAlloc)
//   GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_rosalloc, RosAlloc)
.macro ART_QUICK_ALLOC_OBJECT_ROSALLOC c_name, cxx_name, isInitialized
ENTRY_NO_GP \c_name
    # Fast path rosalloc allocation
    # a0: type
    # s1: Thread::Current
    # -----------------------------
    # t1: object size
    # t2: rosalloc run
    # t3: thread stack top offset
    # a4: thread stack bottom offset
    # v0: free list head
    #
    # a5, a6 : temps
    ld     $t3, THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET($s1)    # Check if thread local allocation stack
    ld     $a4, THREAD_LOCAL_ALLOC_STACK_END_OFFSET($s1)    # has any room left.
    bgeuc  $t3, $a4, .Lslow_path_\c_name

    lwu    $t1, MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET($a0)  # Load object size (t1).
    li     $a5, ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE      # Check if size is for a thread local
                                                            # allocation. Also does the initialized
                                                            # and finalizable checks.
    # When isInitialized == 0, then the class is potentially not yet initialized.
    # If the class is not yet initialized, the object size will be very large to force the branch
    # below to be taken.
    #
    # See InitializeClassVisitors in class-inl.h for more details.
    bltuc  $a5, $t1, .Lslow_path_\c_name

    # Compute the rosalloc bracket index from the size. Since the size is already aligned we can
    # combine the two shifts together.
    dsrl   $t1, $t1, (ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT - POINTER_SIZE_SHIFT)

    daddu  $t2, $t1, $s1
    ld     $t2, (THREAD_ROSALLOC_RUNS_OFFSET - __SIZEOF_POINTER__)($t2)  # Load rosalloc run (t2).

    # Load the free list head (v0).
    # NOTE: this will be the return val.
    ld     $v0, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)($t2)
    beqzc  $v0, .Lslow_path_\c_name

    # Load the next pointer of the head and update the list head with the next pointer.
    ld     $a5, ROSALLOC_SLOT_NEXT_OFFSET($v0)
    sd     $a5, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)($t2)

    # Store the class pointer in the header. This also overwrites the first pointer. The offsets are
    # asserted to match.

#if ROSALLOC_SLOT_NEXT_OFFSET != MIRROR_OBJECT_CLASS_OFFSET
#error "Class pointer needs to overwrite next pointer."
#endif

    POISON_HEAP_REF $a0
    sw     $a0, MIRROR_OBJECT_CLASS_OFFSET($v0)

    # Push the new object onto the thread local allocation stack and increment the thread local
    # allocation stack top.
    sw     $v0, 0($t3)
    daddiu $t3, $t3, COMPRESSED_REFERENCE_SIZE
    sd     $t3, THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET($s1)

    # Decrement the size of the free list.
    lw     $a5, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)($t2)
    addiu  $a5, $a5, -1
    sw     $a5, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)($t2)

.if \isInitialized == 0
    # This barrier is only necessary when the allocation also requires a class initialization check.
    #
    # If the class is already observably initialized, then new-instance allocations are protected
    # from publishing by the compiler which inserts its own StoreStore barrier.
    sync                                         # Fence.
.endif
    jic    $ra, 0

.Lslow_path_\c_name:
    SETUP_GP
    SETUP_SAVE_REFS_ONLY_FRAME
    jal    \cxx_name
    move   $a1 ,$s1                              # Pass self as argument.
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \c_name
.endm

ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_resolved_rosalloc, artAllocObjectFromCodeResolvedRosAlloc, /* isInitialized */ 0
ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_initialized_rosalloc, artAllocObjectFromCodeInitializedRosAlloc, /* isInitialized */ 1

// The common fast path code for art_quick_alloc_object_resolved/initialized_tlab
// and art_quick_alloc_object_resolved/initialized_region_tlab.
//
// a0: type, s1(rSELF): Thread::Current
// Need to preserve a0 to the slow path.
//
// If isInitialized=1 then the compiler assumes the object's class has already been initialized.
// If isInitialized=0 the compiler can only assume it's been at least resolved.
.macro ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH slowPathLabel isInitialized
    ld     $v0, THREAD_LOCAL_POS_OFFSET(rSELF)         # Load thread_local_pos.
    ld     $a2, THREAD_LOCAL_END_OFFSET(rSELF)         # Load thread_local_end.
    lwu    $t0, MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET($a0)  # Load the object size.
    daddu  $a3, $v0, $t0                               # Add object size to tlab pos.

    # When isInitialized == 0, then the class is potentially not yet initialized.
    # If the class is not yet initialized, the object size will be very large to force the branch
    # below to be taken.
    #
    # See InitializeClassVisitors in class-inl.h for more details.
    bltuc  $a2, $a3, \slowPathLabel                    # Check if it fits, overflow works since the
                                                       # tlab pos and end are 32 bit values.
    # "Point of no slow path". Won't go to the slow path from here on.
    sd     $a3, THREAD_LOCAL_POS_OFFSET(rSELF)         # Store new thread_local_pos.
    ld     $a2, THREAD_LOCAL_OBJECTS_OFFSET(rSELF)     # Increment thread_local_objects.
    daddiu $a2, $a2, 1
    sd     $a2, THREAD_LOCAL_OBJECTS_OFFSET(rSELF)
    POISON_HEAP_REF $a0
    sw     $a0, MIRROR_OBJECT_CLASS_OFFSET($v0)        # Store the class pointer.

.if \isInitialized == 0
    # This barrier is only necessary when the allocation also requires a class initialization check.
    #
    # If the class is already observably initialized, then new-instance allocations are protected
    # from publishing by the compiler which inserts its own StoreStore barrier.
    sync                                               # Fence.
.endif
    jic    $ra, 0
.endm

// The common code for art_quick_alloc_object_resolved/initialized_tlab
// and art_quick_alloc_object_resolved/initialized_region_tlab.
.macro GENERATE_ALLOC_OBJECT_TLAB name, entrypoint, isInitialized
ENTRY_NO_GP \name
    # Fast path tlab allocation.
    # a0: type, s1(rSELF): Thread::Current.
    ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH .Lslow_path_\name, \isInitialized
.Lslow_path_\name:
    SETUP_GP
    SETUP_SAVE_REFS_ONLY_FRAME                         # Save callee saves in case of GC.
    jal    \entrypoint                                 # (mirror::Class*, Thread*)
    move   $a1, rSELF                                  # Pass Thread::Current.
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

GENERATE_ALLOC_OBJECT_TLAB art_quick_alloc_object_resolved_region_tlab, artAllocObjectFromCodeResolvedRegionTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_TLAB art_quick_alloc_object_initialized_region_tlab, artAllocObjectFromCodeInitializedRegionTLAB, /* isInitialized */ 1
GENERATE_ALLOC_OBJECT_TLAB art_quick_alloc_object_resolved_tlab, artAllocObjectFromCodeResolvedTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_TLAB art_quick_alloc_object_initialized_tlab, artAllocObjectFromCodeInitializedTLAB, /* isInitialized */ 1

// The common fast path code for art_quick_alloc_array_resolved/initialized_tlab
// and art_quick_alloc_array_resolved/initialized_region_tlab.
//
// a0: type, a1: component_count, a2: total_size, s1(rSELF): Thread::Current.
// Need to preserve a0 and a1 to the slow path.
.macro ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE slowPathLabel
    dli    $a3, OBJECT_ALIGNMENT_MASK_TOGGLED64        # Apply alignemnt mask (addr + 7) & ~7.
    and    $a2, $a2, $a3                               # The mask must be 64 bits to keep high
                                                       # bits in case of overflow.
    # Negative sized arrays are handled here since a1 holds a zero extended 32 bit value.
    # Negative ints become large 64 bit unsigned ints which will always be larger than max signed
    # 32 bit int. Since the max shift for arrays is 3, it can not become a negative 64 bit int.
    dli    $a3, MIN_LARGE_OBJECT_THRESHOLD
    bgeuc  $a2, $a3, \slowPathLabel                    # Possibly a large object, go slow path.

    ld     $v0, THREAD_LOCAL_POS_OFFSET(rSELF)         # Load thread_local_pos.
    ld     $t1, THREAD_LOCAL_END_OFFSET(rSELF)         # Load thread_local_end.
    dsubu  $t2, $t1, $v0                               # Compute the remaining buffer size.
    bltuc  $t2, $a2, \slowPathLabel                    # Check tlab for space, note that we use
                                                       # (end - begin) to handle negative size
                                                       # arrays. It is assumed that a negative size
                                                       # will always be greater unsigned than region
                                                       # size.

    # "Point of no slow path". Won't go to the slow path from here on.
    daddu  $a2, $v0, $a2                               # Add object size to tlab pos.
    sd     $a2, THREAD_LOCAL_POS_OFFSET(rSELF)         # Store new thread_local_pos.
    ld     $a2, THREAD_LOCAL_OBJECTS_OFFSET(rSELF)     # Increment thread_local_objects.
    daddiu $a2, $a2, 1
    sd     $a2, THREAD_LOCAL_OBJECTS_OFFSET(rSELF)
    POISON_HEAP_REF $a0
    sw     $a0, MIRROR_OBJECT_CLASS_OFFSET($v0)        # Store the class pointer.
    sw     $a1, MIRROR_ARRAY_LENGTH_OFFSET($v0)        # Store the array length.

    jic    $ra, 0
.endm

.macro GENERATE_ALLOC_ARRAY_TLAB name, entrypoint, size_setup
ENTRY_NO_GP \name
    # Fast path array allocation for region tlab allocation.
    # a0: mirror::Class* type
    # a1: int32_t component_count
    # s1(rSELF): Thread::Current
    dext   $a4, $a1, 0, 32                             # Create zero-extended component_count. Value
                                                       # in a1 is preserved in a case of slow path.
    \size_setup .Lslow_path_\name
    ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE .Lslow_path_\name
.Lslow_path_\name:
    # a0: mirror::Class* type
    # a1: int32_t component_count
    # a2: Thread* self
    SETUP_GP
    SETUP_SAVE_REFS_ONLY_FRAME                         # Save callee saves in case of GC.
    jal    \entrypoint
    move   $a2, rSELF                                  # Pass Thread::Current.
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

.macro COMPUTE_ARRAY_SIZE_UNKNOWN slow_path
    # Array classes are never finalizable or uninitialized, no need to check.
    lwu    $a3, MIRROR_CLASS_COMPONENT_TYPE_OFFSET($a0) # Load component type.
    UNPOISON_HEAP_REF $a3
    lw     $a3, MIRROR_CLASS_OBJECT_PRIMITIVE_TYPE_OFFSET($a3)
    dsrl   $a3, $a3, PRIMITIVE_TYPE_SIZE_SHIFT_SHIFT   # Component size shift is in high 16 bits.
    dsllv  $a2, $a4, $a3                               # Calculate data size.
                                                       # Add array data offset and alignment.
    daddiu $a2, $a2, (MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
#if MIRROR_WIDE_ARRAY_DATA_OFFSET != MIRROR_INT_ARRAY_DATA_OFFSET + 4
#error Long array data offset must be 4 greater than int array data offset.
#endif

    daddiu $a3, $a3, 1                                 # Add 4 to the length only if the component
    andi   $a3, $a3, 4                                 # size shift is 3 (for 64 bit alignment).
    daddu  $a2, $a2, $a3
.endm

.macro COMPUTE_ARRAY_SIZE_8 slow_path
    # Add array data offset and alignment.
    daddiu $a2, $a4, (MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_16 slow_path
    dsll   $a2, $a4, 1
    # Add array data offset and alignment.
    daddiu $a2, $a2, (MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_32 slow_path
    dsll   $a2, $a4, 2
    # Add array data offset and alignment.
    daddiu $a2, $a2, (MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_64 slow_path
    dsll   $a2, $a4, 3
    # Add array data offset and alignment.
    daddiu $a2, $a2, (MIRROR_WIDE_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved_region_tlab, artAllocArrayFromCodeResolvedRegionTLAB, COMPUTE_ARRAY_SIZE_UNKNOWN
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved8_region_tlab, artAllocArrayFromCodeResolvedRegionTLAB, COMPUTE_ARRAY_SIZE_8
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved16_region_tlab, artAllocArrayFromCodeResolvedRegionTLAB, COMPUTE_ARRAY_SIZE_16
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved32_region_tlab, artAllocArrayFromCodeResolvedRegionTLAB, COMPUTE_ARRAY_SIZE_32
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved64_region_tlab, artAllocArrayFromCodeResolvedRegionTLAB, COMPUTE_ARRAY_SIZE_64

GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved_tlab, artAllocArrayFromCodeResolvedTLAB, COMPUTE_ARRAY_SIZE_UNKNOWN
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved8_tlab, artAllocArrayFromCodeResolvedTLAB, COMPUTE_ARRAY_SIZE_8
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved16_tlab, artAllocArrayFromCodeResolvedTLAB, COMPUTE_ARRAY_SIZE_16
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved32_tlab, artAllocArrayFromCodeResolvedTLAB, COMPUTE_ARRAY_SIZE_32
GENERATE_ALLOC_ARRAY_TLAB art_quick_alloc_array_resolved64_tlab, artAllocArrayFromCodeResolvedTLAB, COMPUTE_ARRAY_SIZE_64

// Macro for string and type resolution and initialization.
// $a0 is both input and output.
.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL name, entrypoint, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    .extern \entrypoint
ENTRY_NO_GP \name
    SETUP_SAVE_EVERYTHING_FRAME \runtime_method_offset  # Save everything in case of GC.
    dla     $t9, \entrypoint
    jalr    $t9                       # (uint32_t index, Thread*)
    move    $a1, rSELF                # Pass Thread::Current (in delay slot).
    beqz    $v0, 1f                   # Success?
    move    $a0, $v0                  # Move result to $a0 (in delay slot).
    RESTORE_SAVE_EVERYTHING_FRAME 0   # Restore everything except $a0.
    jic     $ra, 0                    # Return on success.
1:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END \name
.endm

.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT name, entrypoint
    ONE_ARG_SAVE_EVERYTHING_DOWNCALL \name, \entrypoint, RUNTIME_SAVE_EVERYTHING_FOR_CLINIT_METHOD_OFFSET
.endm

    /*
     * Entry from managed code to resolve a string, this stub will allocate a String and deliver an
     * exception on error. On success the String is returned. A0 holds the string index. The fast
     * path check for hit in strings cache has already been performed.
     */
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_resolve_string, artResolveStringFromCode

    /*
     * Entry from managed code when uninitialized static storage, this stub will run the class
     * initializer and deliver the exception on error. On success the static storage base is
     * returned.
     */
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_static_storage, artInitializeStaticStorageFromCode

    /*
     * Entry from managed code when dex cache misses for a type_idx.
     */
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_type, artInitializeTypeFromCode

    /*
     * Entry from managed code when type_idx needs to be checked for access and dex cache may also
     * miss.
     */
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode

    /*
     * Called by managed code when the value in rSUSPEND has been decremented to 0.
     */
    .extern artTestSuspendFromCode
ENTRY_NO_GP art_quick_test_suspend
    SETUP_SAVE_EVERYTHING_FRAME RUNTIME_SAVE_EVERYTHING_FOR_SUSPEND_CHECK_METHOD_OFFSET
                                              # save everything for stack crawl
    jal    artTestSuspendFromCode             # (Thread*)
    move   $a0, rSELF
    RESTORE_SAVE_EVERYTHING_FRAME
    jalr   $zero, $ra
    nop
END art_quick_test_suspend

    /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * r0 holds the proxy method; r1, r2 and r3 may contain arguments.
     */
    .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_A0
    move    $a2, rSELF             # pass Thread::Current
    jal     artQuickProxyInvokeHandler  # (Method* proxy method, receiver, Thread*, SP)
    move    $a3, $sp               # pass $sp
    ld      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    daddiu  $sp, $sp, REFS_AND_ARGS_MINUS_REFS_SIZE  # skip a0-a7 and f12-f19
    RESTORE_SAVE_REFS_ONLY_FRAME
    bne     $t0, $zero, 1f
    dmtc1   $v0, $f0               # place return value to FP return value
    jalr    $zero, $ra
    dmtc1   $v1, $f1               # place return value to FP return value
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * a0 is the conflict ArtMethod.
     * t0 is a hidden argument that holds the target interface method's dex method index.
     *
     * Mote that this stub writes to v0-v1, a0, t0-t3, t8-t9, f0-f11, f20-f23.
     */
    .extern artLookupResolvedMethod
    .extern __atomic_load_16        # For __int128_t std::atomic::load(std::memory_order).
ENTRY art_quick_imt_conflict_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL /* save_s4_thru_s8 */ 0

    ld      $t1, FRAME_SIZE_SAVE_REFS_AND_ARGS($sp)  # $t1 = referrer.
    // If the method is obsolete, just go through the dex cache miss slow path.
    // The obsolete flag is set with suspended threads, so we do not need an acquire operation here.
    lw      $t9, ART_METHOD_ACCESS_FLAGS_OFFSET($t1)  # $t9 = access flags.
    sll     $t9, $t9, 31 - ACC_OBSOLETE_METHOD_SHIFT  # Move obsolete method bit to sign bit.
    bltzc   $t9, .Limt_conflict_trampoline_dex_cache_miss
    lwu     $t1, ART_METHOD_DECLARING_CLASS_OFFSET($t1)  # $t1 = declaring class (no read barrier).
    lwu     $t1, MIRROR_CLASS_DEX_CACHE_OFFSET($t1)  # $t1 = dex cache (without read barrier).
    UNPOISON_HEAP_REF $t1
    dla     $t9, __atomic_load_16
    ld      $t1, MIRROR_DEX_CACHE_RESOLVED_METHODS_OFFSET($t1)  # $t1 = dex cache methods array.

    dext    $s2, $t0, 0, 32                         # $s2 = zero-extended method index
                                                    # (callee-saved).
    ld      $s3, ART_METHOD_JNI_OFFSET_64($a0)      # $s3 = ImtConflictTable (callee-saved).

    dext    $t0, $t0, 0, METHOD_DEX_CACHE_HASH_BITS  # $t0 = slot index.

    li      $a1, STD_MEMORY_ORDER_RELAXED           # $a1 = std::memory_order_relaxed.
    jalr    $t9                                     # [$v0, $v1] = __atomic_load_16($a0, $a1).
    dlsa    $a0, $t0, $t1, POINTER_SIZE_SHIFT + 1   # $a0 = DexCache method slot address.

    bnec    $v1, $s2, .Limt_conflict_trampoline_dex_cache_miss  # Branch if method index miss.

.Limt_table_iterate:
    ld      $t1, 0($s3)                             # Load next entry in ImtConflictTable.
    # Branch if found.
    beq     $t1, $v0, .Limt_table_found
    nop
    # If the entry is null, the interface method is not in the ImtConflictTable.
    beqzc   $t1, .Lconflict_trampoline
    # Iterate over the entries of the ImtConflictTable.
    daddiu  $s3, $s3, 2 * __SIZEOF_POINTER__        # Iterate to the next entry.
    bc      .Limt_table_iterate

.Limt_table_found:
    # We successfully hit an entry in the table. Load the target method and jump to it.
    .cfi_remember_state
    ld      $a0, __SIZEOF_POINTER__($s3)
    ld      $t9, ART_METHOD_QUICK_CODE_OFFSET_64($a0)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME /* restore_s4_thru_s8 */ 0
    jic     $t9, 0
    .cfi_restore_state

.Lconflict_trampoline:
    # Call the runtime stub to populate the ImtConflictTable and jump to the resolved method.
    .cfi_remember_state
    RESTORE_SAVE_REFS_AND_ARGS_FRAME_A1             # Restore this.
    move    $a0, $v0                                # Load interface method.
    INVOKE_TRAMPOLINE_BODY artInvokeInterfaceTrampoline, /* save_s4_thru_s8_only */ 1
    .cfi_restore_state

.Limt_conflict_trampoline_dex_cache_miss:
    # We're not creating a proper runtime method frame here,
    # artLookupResolvedMethod() is not allowed to walk the stack.
    dla     $t9, artLookupResolvedMethod
    ld      $a1, FRAME_SIZE_SAVE_REFS_AND_ARGS($sp)  # $a1 = referrer.
    jalr    $t9                                     # (uint32_t method_index, ArtMethod* referrer).
    sll     $a0, $s2, 0                             # $a0 = sign-extended method index.

    # If the method wasn't resolved, skip the lookup and go to artInvokeInterfaceTrampoline().
    beqzc   $v0, .Lconflict_trampoline
    nop
    bc      .Limt_table_iterate
END art_quick_imt_conflict_trampoline

    .extern artQuickResolutionTrampoline
ENTRY art_quick_resolution_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    move    $a2, rSELF             # pass Thread::Current
    jal     artQuickResolutionTrampoline  # (Method* called, receiver, Thread*, SP)
    move    $a3, $sp               # pass $sp
    beq     $v0, $zero, 1f
    ld      $a0, 0($sp)            # load resolved method in $a0
                                   # artQuickResolutionTrampoline puts resolved method in *SP
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    move    $t9, $v0               # code pointer must be in $t9 to generate the global pointer
    jalr    $zero, $t9             # tail call to method
    nop
1:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

    .extern artQuickGenericJniTrampoline
    .extern artQuickGenericJniEndTrampoline
ENTRY art_quick_generic_jni_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_A0
    move    $s8, $sp               # save $sp

    # prepare for call to artQuickGenericJniTrampoline(Thread*, SP)
    move    $a0, rSELF             # pass Thread::Current
    move    $a1, $sp               # pass $sp
    jal     artQuickGenericJniTrampoline   # (Thread*, SP)
    daddiu  $sp, $sp, -5120        # reserve space on the stack

    # The C call will have registered the complete save-frame on success.
    # The result of the call is:
    # v0: ptr to native code, 0 on error.
    # v1: ptr to the bottom of the used area of the alloca, can restore stack till here.
    beq     $v0, $zero, 1f         # check entry error
    move    $t9, $v0               # save the code ptr
    move    $sp, $v1               # release part of the alloca

    # Load parameters from stack into registers
    ld      $a0,   0($sp)
    ld      $a1,   8($sp)
    ld      $a2,  16($sp)
    ld      $a3,  24($sp)
    ld      $a4,  32($sp)
    ld      $a5,  40($sp)
    ld      $a6,  48($sp)
    ld      $a7,  56($sp)
    # Load FPRs the same as GPRs. Look at BuildNativeCallFrameStateMachine.
    l.d     $f12,  0($sp)
    l.d     $f13,  8($sp)
    l.d     $f14, 16($sp)
    l.d     $f15, 24($sp)
    l.d     $f16, 32($sp)
    l.d     $f17, 40($sp)
    l.d     $f18, 48($sp)
    l.d     $f19, 56($sp)
    jalr    $t9                    # native call
    daddiu  $sp, $sp, 64

    # result sign extension is handled in C code
    # prepare for call to artQuickGenericJniEndTrampoline(Thread*, result, result_f)
    move    $a0, rSELF             # pass Thread::Current
    move    $a1, $v0
    jal     artQuickGenericJniEndTrampoline
    dmfc1   $a2, $f0

    ld      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    bne     $t0, $zero, 1f         # check for pending exceptions
    move    $sp, $s8               # tear down the alloca

    # tear dpown the callee-save frame
    RESTORE_SAVE_REFS_AND_ARGS_FRAME

    jalr    $zero, $ra
    dmtc1   $v0, $f0               # place return value to FP return value

1:
    ld      $t0, THREAD_TOP_QUICK_FRAME_OFFSET(rSELF)
    daddiu  $sp, $t0, -1  // Remove the GenericJNI tag.
    # This will create a new save-all frame, required by the runtime.
    DELIVER_PENDING_EXCEPTION
END art_quick_generic_jni_trampoline

    .extern artQuickToInterpreterBridge
ENTRY art_quick_to_interpreter_bridge
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    move    $a1, rSELF             # pass Thread::Current
    jal     artQuickToInterpreterBridge    # (Method* method, Thread*, SP)
    move    $a2, $sp               # pass $sp
    ld      $t0, THREAD_EXCEPTION_OFFSET(rSELF) # load Thread::Current()->exception_
    daddiu  $sp, $sp, REFS_AND_ARGS_MINUS_REFS_SIZE  # skip a0-a7 and f12-f19
    RESTORE_SAVE_REFS_ONLY_FRAME
    bne     $t0, $zero, 1f
    dmtc1   $v0, $f0               # place return value to FP return value
    jalr    $zero, $ra
    dmtc1   $v1, $f1               # place return value to FP return value
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

    .extern artInvokeObsoleteMethod
ENTRY art_invoke_obsolete_method_stub
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    jal     artInvokeObsoleteMethod    # (Method* method, Thread* self)
    move    $a1, rSELF                 # pass Thread::Current
END art_invoke_obsolete_method_stub

    /*
     * Routine that intercepts method calls and returns.
     */
    .extern artInstrumentationMethodEntryFromCode
    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_entry
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    # Preserve $a0 knowing there is a spare slot in kSaveRefsAndArgs.
    sd      $a0, 8($sp)     # Save arg0.
    move    $a3, $sp        # Pass $sp.
    jal     artInstrumentationMethodEntryFromCode  # (Method*, Object*, Thread*, SP)
    move    $a2, rSELF      # pass Thread::Current
    beqzc   $v0, .Ldeliver_instrumentation_entry_exception
                            # Deliver exception if we got nullptr as function.
    move    $t9, $v0        # $t9 holds reference to code
    ld      $a0, 8($sp)     # Restore arg0.
    dla     $v0, art_quick_instrumentation_exit
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    move    $ra, $v0
    jic     $t9, 0          # call method, returning to art_quick_instrumentation_exit
.Ldeliver_instrumentation_entry_exception:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_instrumentation_entry

ENTRY_NO_GP art_quick_instrumentation_exit
    move    $ra, $zero      # RA points here, so clobber with 0 for later checks.
    SETUP_SAVE_EVERYTHING_FRAME

    daddiu  $a3, $sp, 16    # Pass fpr_res pointer ($f0 in SAVE_EVERYTHING_FRAME).
    daddiu  $a2, $sp, 280   # Pass gpr_res pointer ($v0 in SAVE_EVERYTHING_FRAME).
    move    $a1, $sp        # Pass $sp.
    jal     artInstrumentationMethodExitFromCode  # (Thread*, SP, gpr_res*, fpr_res*)
    move    $a0, rSELF      # pass Thread::Current

    beqzc   $v0, .Ldo_deliver_instrumentation_exception
                            # Deliver exception if we got nullptr as function.
    nop
    bnez    $v1, .Ldeoptimize

    # Normal return.
    sd      $v0, (FRAME_SIZE_SAVE_EVERYTHING-8)($sp)  # Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    jic     $ra, 0
.Ldo_deliver_instrumentation_exception:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.Ldeoptimize:
    b       art_quick_deoptimize
    sd      $v1, (FRAME_SIZE_SAVE_EVERYTHING-8)($sp)
                            # Fake a call from instrumentation return pc.
END art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimize
ENTRY_NO_GP_CUSTOM_CFA art_quick_deoptimize, FRAME_SIZE_SAVE_EVERYTHING
    # SETUP_SAVE_EVERYTHING_FRAME has been done by art_quick_instrumentation_exit.
    .cfi_rel_offset 31, 488
    .cfi_rel_offset 30, 480
    .cfi_rel_offset 28, 472
    .cfi_rel_offset 25, 464
    .cfi_rel_offset 24, 456
    .cfi_rel_offset 23, 448
    .cfi_rel_offset 22, 440
    .cfi_rel_offset 21, 432
    .cfi_rel_offset 20, 424
    .cfi_rel_offset 19, 416
    .cfi_rel_offset 18, 408
    .cfi_rel_offset 17, 400
    .cfi_rel_offset 16, 392
    .cfi_rel_offset 15, 384
    .cfi_rel_offset 14, 376
    .cfi_rel_offset 13, 368
    .cfi_rel_offset 12, 360
    .cfi_rel_offset 11, 352
    .cfi_rel_offset 10, 344
    .cfi_rel_offset 9, 336
    .cfi_rel_offset 8, 328
    .cfi_rel_offset 7, 320
    .cfi_rel_offset 6, 312
    .cfi_rel_offset 5, 304
    .cfi_rel_offset 4, 296
    .cfi_rel_offset 3, 288
    .cfi_rel_offset 2, 280
    .cfi_rel_offset 1, 272

    jal     artDeoptimize   # artDeoptimize(Thread*)
    move    $a0, rSELF      # pass Thread::current
    break
END art_quick_deoptimize

    /*
     * Compiled code has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimizeFromCompiledCode
ENTRY_NO_GP art_quick_deoptimize_from_compiled_code
    SETUP_SAVE_EVERYTHING_FRAME
    jal      artDeoptimizeFromCompiledCode    # (DeoptimizationKind, Thread*)
    move     $a1, rSELF                       # pass Thread::current
END art_quick_deoptimize_from_compiled_code

  .set push
  .set noat
/* java.lang.String.compareTo(String anotherString) */
ENTRY_NO_GP art_quick_string_compareto
/* $a0 holds address of "this" */
/* $a1 holds address of "anotherString" */
    move   $a2, $zero
    beq    $a0, $a1, .Lstring_compareto_length_diff # this and anotherString are the same object
    move   $a3, $zero                               # return 0 (it returns a2 - a3)

#if (STRING_COMPRESSION_FEATURE)
    lw     $a4, MIRROR_STRING_COUNT_OFFSET($a0)     # 'count' field of this
    lw     $a5, MIRROR_STRING_COUNT_OFFSET($a1)     # 'count' field of anotherString
    sra    $a2, $a4, 1                              # this.length()
    sra    $a3, $a5, 1                              # anotherString.length()
#else
    lw     $a2, MIRROR_STRING_COUNT_OFFSET($a0)     # this.length()
    lw     $a3, MIRROR_STRING_COUNT_OFFSET($a1)     # anotherString.length()
#endif

    MINu   $t2, $a2, $a3
    # $t2 now holds min(this.length(),anotherString.length())

    # while min(this.length(),anotherString.length())-i != 0
    beqzc  $t2, .Lstring_compareto_length_diff # if $t2==0
                                               #     return (this.length() - anotherString.length())

#if (STRING_COMPRESSION_FEATURE)
    # Differ cases:
    dext   $a6, $a4, 0, 1
    beqz   $a6, .Lstring_compareto_this_is_compressed
    dext   $a6, $a5, 0, 1                      # In branch delay slot.
    beqz   $a6, .Lstring_compareto_that_is_compressed
    nop
    b      .Lstring_compareto_both_not_compressed
    nop

.Lstring_compareto_this_is_compressed:
    beqzc  $a6, .Lstring_compareto_both_compressed
    /* If (this->IsCompressed() && that->IsCompressed() == false) */
.Lstring_compareto_loop_comparison_this_compressed:
    lbu    $t0, MIRROR_STRING_VALUE_OFFSET($a0)
    lhu    $t1, MIRROR_STRING_VALUE_OFFSET($a1)
    bnec   $t0, $t1, .Lstring_compareto_char_diff
    daddiu $a0, $a0, 1      # point at this.charAt(i++) - compressed
    subu   $t2, $t2, 1      # new value of min(this.length(),anotherString.length())-i
    bnez   $t2, .Lstring_compareto_loop_comparison_this_compressed
    daddiu $a1, $a1, 2      # point at anotherString.charAt(i++) - uncompressed
    jalr   $zero, $ra
    subu   $v0, $a2, $a3    # return (this.length() - anotherString.length())

.Lstring_compareto_that_is_compressed:
    lhu    $t0, MIRROR_STRING_VALUE_OFFSET($a0)
    lbu    $t1, MIRROR_STRING_VALUE_OFFSET($a1)
    bnec   $t0, $t1, .Lstring_compareto_char_diff
    daddiu $a0, $a0, 2      # point at this.charAt(i++) - uncompressed
    subu   $t2, $t2, 1      # new value of min(this.length(),anotherString.length())-i
    bnez   $t2, .Lstring_compareto_that_is_compressed
    daddiu $a1, $a1, 1      # point at anotherString.charAt(i++) - compressed
    jalr   $zero, $ra
    subu   $v0, $a2, $a3    # return (this.length() - anotherString.length())

.Lstring_compareto_both_compressed:
    lbu    $t0, MIRROR_STRING_VALUE_OFFSET($a0)
    lbu    $t1, MIRROR_STRING_VALUE_OFFSET($a1)
    bnec   $t0, $t1, .Lstring_compareto_char_diff
    daddiu $a0, $a0, 1      # point at this.charAt(i++) - compressed
    subu   $t2, $t2, 1      # new value of min(this.length(),anotherString.length())-i
    bnez   $t2, .Lstring_compareto_both_compressed
    daddiu $a1, $a1, 1      # point at anotherString.charAt(i++) - compressed
    jalr   $zero, $ra
    subu   $v0, $a2, $a3    # return (this.length() - anotherString.length())
#endif

.Lstring_compareto_both_not_compressed:
    lhu    $t0, MIRROR_STRING_VALUE_OFFSET($a0)    # while this.charAt(i) == anotherString.charAt(i)
    lhu    $t1, MIRROR_STRING_VALUE_OFFSET($a1)
    bnec   $t0, $t1, .Lstring_compareto_char_diff  # if this.charAt(i) != anotherString.charAt(i)
                            #     return (this.charAt(i) - anotherString.charAt(i))
    daddiu $a0, $a0, 2      # point at this.charAt(i++)
    subu   $t2, $t2, 1      # new value of min(this.length(),anotherString.length())-i
    bnez   $t2, .Lstring_compareto_both_not_compressed
    daddiu $a1, $a1, 2      # point at anotherString.charAt(i++)

.Lstring_compareto_length_diff:
    jalr   $zero, $ra
    subu   $v0, $a2, $a3    # return (this.length() - anotherString.length())

.Lstring_compareto_char_diff:
    jalr   $zero, $ra
    subu   $v0, $t0, $t1    # return (this.charAt(i) - anotherString.charAt(i))
END art_quick_string_compareto

/* java.lang.String.indexOf(int ch, int fromIndex=0) */
ENTRY_NO_GP art_quick_indexof
/* $a0 holds address of "this" */
/* $a1 holds "ch" */
/* $a2 holds "fromIndex" */
#if (STRING_COMPRESSION_FEATURE)
    lw     $a3, MIRROR_STRING_COUNT_OFFSET($a0)     # 'count' field of this
#else
    lw     $t0, MIRROR_STRING_COUNT_OFFSET($a0)     # this.length()
#endif
    slt    $at, $a2, $zero  # if fromIndex < 0
    seleqz $a2, $a2, $at    #     fromIndex = 0;
#if (STRING_COMPRESSION_FEATURE)
    srl   $t0, $a3, 1       # $a3 holds count (with flag) and $t0 holds actual length
#endif
    subu   $t0, $t0, $a2    # this.length() - fromIndex
    blez   $t0, 6f          # if this.length()-fromIndex <= 0
    li     $v0, -1          #     return -1;

#if (STRING_COMPRESSION_FEATURE)
    dext   $a3, $a3, 0, 1   # Extract compression flag.
    beqzc  $a3, .Lstring_indexof_compressed
#endif

    sll    $v0, $a2, 1      # $a0 += $a2 * 2
    daddu  $a0, $a0, $v0    #  "  ditto  "
    move   $v0, $a2         # Set i to fromIndex.

1:
    lhu    $t3, MIRROR_STRING_VALUE_OFFSET($a0)     # if this.charAt(i) == ch
    beq    $t3, $a1, 6f                             #     return i;
    daddu  $a0, $a0, 2      # i++
    subu   $t0, $t0, 1      # this.length() - i
    bnez   $t0, 1b          # while this.length() - i > 0
    addu   $v0, $v0, 1      # i++

    li     $v0, -1          # if this.length() - i <= 0
                            #     return -1;

6:
    j      $ra
    nop

#if (STRING_COMPRESSION_FEATURE)
.Lstring_indexof_compressed:
    move   $a4, $a0         # Save a copy in $a4 to later compute result.
    daddu  $a0, $a0, $a2    # $a0 += $a2

.Lstring_indexof_compressed_loop:
    lbu    $t3, MIRROR_STRING_VALUE_OFFSET($a0)
    beq    $t3, $a1, .Lstring_indexof_compressed_matched
    subu   $t0, $t0, 1
    bgtz   $t0, .Lstring_indexof_compressed_loop
    daddu  $a0, $a0, 1

.Lstring_indexof_nomatch:
    jalr   $zero, $ra
    li     $v0, -1          # return -1;

.Lstring_indexof_compressed_matched:
    jalr   $zero, $ra
    dsubu  $v0, $a0, $a4    # return (current - start);
#endif
END art_quick_indexof

    /*
     * Create a function `name` calling the ReadBarrier::Mark routine,
     * getting its argument and returning its result through register
     * `reg`, saving and restoring all caller-save registers.
     */
.macro READ_BARRIER_MARK_REG name, reg
ENTRY \name
    // Null check so that we can load the lock word.
    bnezc   \reg, .Lnot_null_\name
    nop
.Lret_rb_\name:
    jic     $ra, 0
.Lnot_null_\name:
    // Check lock word for mark bit, if marked return.
    lw      $t9, MIRROR_OBJECT_LOCK_WORD_OFFSET(\reg)
    .set push
    .set noat
    sll     $at, $t9, 31 - LOCK_WORD_MARK_BIT_SHIFT     # Move mark bit to sign bit.
    bltzc   $at, .Lret_rb_\name
#if (LOCK_WORD_STATE_SHIFT != 30) || (LOCK_WORD_STATE_FORWARDING_ADDRESS != 3)
    // The below code depends on the lock word state being in the highest bits
    // and the "forwarding address" state having all bits set.
#error "Unexpected lock word state shift or forwarding address state value."
#endif
    // Test that both the forwarding state bits are 1.
    sll     $at, $t9, 1
    and     $at, $at, $t9                               # Sign bit = 1 IFF both bits are 1.
    bltzc   $at, .Lret_forwarding_address\name
    .set pop

    daddiu  $sp, $sp, -320
    .cfi_adjust_cfa_offset 320

    sd      $ra, 312($sp)
    .cfi_rel_offset 31, 312
    sd      $t8, 304($sp)       # save t8 holding caller's gp
    .cfi_rel_offset 24, 304
    sd      $t3, 296($sp)
    .cfi_rel_offset 15, 296
    sd      $t2, 288($sp)
    .cfi_rel_offset 14, 288
    sd      $t1, 280($sp)
    .cfi_rel_offset 13, 280
    sd      $t0, 272($sp)
    .cfi_rel_offset 12, 272
    sd      $a7, 264($sp)
    .cfi_rel_offset 11, 264
    sd      $a6, 256($sp)
    .cfi_rel_offset 10, 256
    sd      $a5, 248($sp)
    .cfi_rel_offset 9, 248
    sd      $a4, 240($sp)
    .cfi_rel_offset 8, 240
    sd      $a3, 232($sp)
    .cfi_rel_offset 7, 232
    sd      $a2, 224($sp)
    .cfi_rel_offset 6, 224
    sd      $a1, 216($sp)
    .cfi_rel_offset 5, 216
    sd      $a0, 208($sp)
    .cfi_rel_offset 4, 208
    sd      $v1, 200($sp)
    .cfi_rel_offset 3, 200
    sd      $v0, 192($sp)
    .cfi_rel_offset 2, 192

    dla     $t9, artReadBarrierMark

    sdc1    $f23, 184($sp)
    sdc1    $f22, 176($sp)
    sdc1    $f21, 168($sp)
    sdc1    $f20, 160($sp)
    sdc1    $f19, 152($sp)
    sdc1    $f18, 144($sp)
    sdc1    $f17, 136($sp)
    sdc1    $f16, 128($sp)
    sdc1    $f15, 120($sp)
    sdc1    $f14, 112($sp)
    sdc1    $f13, 104($sp)
    sdc1    $f12,  96($sp)
    sdc1    $f11,  88($sp)
    sdc1    $f10,  80($sp)
    sdc1    $f9,   72($sp)
    sdc1    $f8,   64($sp)
    sdc1    $f7,   56($sp)
    sdc1    $f6,   48($sp)
    sdc1    $f5,   40($sp)
    sdc1    $f4,   32($sp)
    sdc1    $f3,   24($sp)
    sdc1    $f2,   16($sp)
    sdc1    $f1,    8($sp)

    .ifnc \reg, $a0
      move  $a0, \reg           # pass obj from `reg` in a0
    .endif
    jalr    $t9                 # v0 <- artReadBarrierMark(obj)
    sdc1    $f0,    0($sp)      # in delay slot

    ld      $ra, 312($sp)
    .cfi_restore 31
    ld      $t8, 304($sp)       # restore t8 holding caller's gp
    .cfi_restore 24
    ld      $t3, 296($sp)
    .cfi_restore 15
    ld      $t2, 288($sp)
    .cfi_restore 14
    ld      $t1, 280($sp)
    .cfi_restore 13
    ld      $t0, 272($sp)
    .cfi_restore 12
    ld      $a7, 264($sp)
    .cfi_restore 11
    ld      $a6, 256($sp)
    .cfi_restore 10
    ld      $a5, 248($sp)
    .cfi_restore 9
    ld      $a4, 240($sp)
    .cfi_restore 8
    ld      $a3, 232($sp)
    .cfi_restore 7
    ld      $a2, 224($sp)
    .cfi_restore 6
    ld      $a1, 216($sp)
    .cfi_restore 5
    ld      $a0, 208($sp)
    .cfi_restore 4
    ld      $v1, 200($sp)
    .cfi_restore 3

    .ifnc \reg, $v0
      move  \reg, $v0           # `reg` <- v0
      ld    $v0, 192($sp)
      .cfi_restore 2
    .endif

    ldc1    $f23, 184($sp)
    ldc1    $f22, 176($sp)
    ldc1    $f21, 168($sp)
    ldc1    $f20, 160($sp)
    ldc1    $f19, 152($sp)
    ldc1    $f18, 144($sp)
    ldc1    $f17, 136($sp)
    ldc1    $f16, 128($sp)
    ldc1    $f15, 120($sp)
    ldc1    $f14, 112($sp)
    ldc1    $f13, 104($sp)
    ldc1    $f12,  96($sp)
    ldc1    $f11,  88($sp)
    ldc1    $f10,  80($sp)
    ldc1    $f9,   72($sp)
    ldc1    $f8,   64($sp)
    ldc1    $f7,   56($sp)
    ldc1    $f6,   48($sp)
    ldc1    $f5,   40($sp)
    ldc1    $f4,   32($sp)
    ldc1    $f3,   24($sp)
    ldc1    $f2,   16($sp)
    ldc1    $f1,    8($sp)
    ldc1    $f0,    0($sp)

    .cpreturn                   # restore caller's gp from t8
    jalr    $zero, $ra
    daddiu  $sp, $sp, 320
    .cfi_adjust_cfa_offset -320

.Lret_forwarding_address\name:
    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    sll     \reg, $t9, LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    jalr    $zero, $ra
    dext    \reg, \reg, 0, 32   # Make sure the address is zero-extended.
END \name
.endm

// Note that art_quick_read_barrier_mark_regXX corresponds to register XX+1.
// ZERO (register 0) is reserved.
// AT (register 1) is reserved as a temporary/scratch register.
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg01, $v0
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg02, $v1
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg03, $a0
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg04, $a1
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg05, $a2
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg06, $a3
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg07, $a4
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg08, $a5
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg09, $a6
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg10, $a7
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg11, $t0
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg12, $t1
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg13, $t2
// T3 (register 15) is reserved as a temporary/scratch register.
// S0 and S1 (registers 16 and 17) are reserved as suspended and thread registers.
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg17, $s2
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg18, $s3
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg19, $s4
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg20, $s5
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg21, $s6
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg22, $s7
// T8 and T9 (registers 24 and 25) are reserved as temporary/scratch registers.
// K0, K1, GP, SP (registers 26 - 29) are reserved.
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg29, $s8
// RA (register 31) is reserved.

// Caller code:
// Short constant offset/index:
//  ld      $t9, pReadBarrierMarkReg00
//  beqzc   $t9, skip_call
//  nop
//  jialc   $t9, thunk_disp
// skip_call:
//  lwu     `out`, ofs(`obj`)
// [dsubu   `out`, $zero, `out`
//  dext    `out`, `out`, 0, 32]  # Unpoison reference.
.macro BRB_FIELD_SHORT_OFFSET_ENTRY obj
    # Explicit null check. May be redundant (for array elements or when the field
    # offset is larger than the page size, 4KB).
    # $ra will be adjusted to point to lwu's stack map when throwing NPE.
    beqzc   \obj, .Lintrospection_throw_npe
    lapc    $t3, .Lintrospection_exits                  # $t3 = address of .Lintrospection_exits.
    .set push
    .set noat
    lw      $at, MIRROR_OBJECT_LOCK_WORD_OFFSET(\obj)
    sll     $at, $at, 31 - LOCK_WORD_READ_BARRIER_STATE_SHIFT   # Move barrier state bit
                                                                # to sign bit.
    bltz    $at, .Lintrospection_field_array            # If gray, load reference, mark.
    move    $t8, \obj                                   # Move `obj` to $t8 for common code.
    .set pop
    jalr    $zero, $ra                                  # Otherwise, load-load barrier and return.
    sync
.endm

// Caller code:
// Long constant offset/index:   | Variable index:
//  ld      $t9, pReadBarrierMarkReg00
//  beqz    $t9, skip_call       |  beqz    $t9, skip_call
//  daui    $t8, `obj`, ofs_hi   |  dlsa    $t8, `index`, `obj`, 2
//  jialc   $t9, thunk_disp      |  jialc   $t9, thunk_disp
// skip_call:                    | skip_call:
//  lwu     `out`, ofs_lo($t8)   |  lwu     `out`, ofs($t8)
// [dsubu   `out`, $zero, `out`  | [dsubu   `out`, $zero, `out`
//  dext    `out`, `out`, 0, 32] |  dext    `out`, `out`, 0, 32]  # Unpoison reference.
.macro BRB_FIELD_LONG_OFFSET_ENTRY obj
    # No explicit null check for variable indices or large constant indices/offsets
    # as it must have been done earlier.
    lapc    $t3, .Lintrospection_exits                  # $t3 = address of .Lintrospection_exits.
    .set push
    .set noat
    lw      $at, MIRROR_OBJECT_LOCK_WORD_OFFSET(\obj)
    sll     $at, $at, 31 - LOCK_WORD_READ_BARRIER_STATE_SHIFT   # Move barrier state bit
                                                                # to sign bit.
    bltzc   $at, .Lintrospection_field_array            # If gray, load reference, mark.
    .set pop
    sync                                                # Otherwise, load-load barrier and return.
    jic     $ra, 0
    break                                               # Padding to 8 instructions.
    break
.endm

.macro BRB_GC_ROOT_ENTRY root
    lapc    $t3, .Lintrospection_exit_\root             # $t3 = exit point address.
    bnez    \root, .Lintrospection_common
    move    $t8, \root                                  # Move reference to $t8 for common code.
    jic     $ra, 0                                      # Return if null.
.endm

.macro BRB_FIELD_EXIT out
.Lintrospection_exit_\out:
    jalr    $zero, $ra
    move    \out, $t8                                   # Return reference in expected register.
.endm

.macro BRB_FIELD_EXIT_BREAK
    break
    break
.endm

ENTRY_NO_GP art_quick_read_barrier_mark_introspection
    # Entry points for offsets/indices not fitting into int16_t and for variable indices.
    BRB_FIELD_LONG_OFFSET_ENTRY $v0
    BRB_FIELD_LONG_OFFSET_ENTRY $v1
    BRB_FIELD_LONG_OFFSET_ENTRY $a0
    BRB_FIELD_LONG_OFFSET_ENTRY $a1
    BRB_FIELD_LONG_OFFSET_ENTRY $a2
    BRB_FIELD_LONG_OFFSET_ENTRY $a3
    BRB_FIELD_LONG_OFFSET_ENTRY $a4
    BRB_FIELD_LONG_OFFSET_ENTRY $a5
    BRB_FIELD_LONG_OFFSET_ENTRY $a6
    BRB_FIELD_LONG_OFFSET_ENTRY $a7
    BRB_FIELD_LONG_OFFSET_ENTRY $t0
    BRB_FIELD_LONG_OFFSET_ENTRY $t1
    BRB_FIELD_LONG_OFFSET_ENTRY $t2
    BRB_FIELD_LONG_OFFSET_ENTRY $s2
    BRB_FIELD_LONG_OFFSET_ENTRY $s3
    BRB_FIELD_LONG_OFFSET_ENTRY $s4
    BRB_FIELD_LONG_OFFSET_ENTRY $s5
    BRB_FIELD_LONG_OFFSET_ENTRY $s6
    BRB_FIELD_LONG_OFFSET_ENTRY $s7
    BRB_FIELD_LONG_OFFSET_ENTRY $s8

    # Entry points for offsets/indices fitting into int16_t.
    BRB_FIELD_SHORT_OFFSET_ENTRY $v0
    BRB_FIELD_SHORT_OFFSET_ENTRY $v1
    BRB_FIELD_SHORT_OFFSET_ENTRY $a0
    BRB_FIELD_SHORT_OFFSET_ENTRY $a1
    BRB_FIELD_SHORT_OFFSET_ENTRY $a2
    BRB_FIELD_SHORT_OFFSET_ENTRY $a3
    BRB_FIELD_SHORT_OFFSET_ENTRY $a4
    BRB_FIELD_SHORT_OFFSET_ENTRY $a5
    BRB_FIELD_SHORT_OFFSET_ENTRY $a6
    BRB_FIELD_SHORT_OFFSET_ENTRY $a7
    BRB_FIELD_SHORT_OFFSET_ENTRY $t0
    BRB_FIELD_SHORT_OFFSET_ENTRY $t1
    BRB_FIELD_SHORT_OFFSET_ENTRY $t2
    BRB_FIELD_SHORT_OFFSET_ENTRY $s2
    BRB_FIELD_SHORT_OFFSET_ENTRY $s3
    BRB_FIELD_SHORT_OFFSET_ENTRY $s4
    BRB_FIELD_SHORT_OFFSET_ENTRY $s5
    BRB_FIELD_SHORT_OFFSET_ENTRY $s6
    BRB_FIELD_SHORT_OFFSET_ENTRY $s7
    BRB_FIELD_SHORT_OFFSET_ENTRY $s8

    .global art_quick_read_barrier_mark_introspection_gc_roots
art_quick_read_barrier_mark_introspection_gc_roots:
    # Entry points for GC roots.
    BRB_GC_ROOT_ENTRY $v0
    BRB_GC_ROOT_ENTRY $v1
    BRB_GC_ROOT_ENTRY $a0
    BRB_GC_ROOT_ENTRY $a1
    BRB_GC_ROOT_ENTRY $a2
    BRB_GC_ROOT_ENTRY $a3
    BRB_GC_ROOT_ENTRY $a4
    BRB_GC_ROOT_ENTRY $a5
    BRB_GC_ROOT_ENTRY $a6
    BRB_GC_ROOT_ENTRY $a7
    BRB_GC_ROOT_ENTRY $t0
    BRB_GC_ROOT_ENTRY $t1
    BRB_GC_ROOT_ENTRY $t2
    BRB_GC_ROOT_ENTRY $s2
    BRB_GC_ROOT_ENTRY $s3
    BRB_GC_ROOT_ENTRY $s4
    BRB_GC_ROOT_ENTRY $s5
    BRB_GC_ROOT_ENTRY $s6
    BRB_GC_ROOT_ENTRY $s7
    BRB_GC_ROOT_ENTRY $s8
    .global art_quick_read_barrier_mark_introspection_end_of_entries
art_quick_read_barrier_mark_introspection_end_of_entries:

.Lintrospection_throw_npe:
    b       art_quick_throw_null_pointer_exception
    daddiu  $ra, $ra, 4         # Skip lwu, make $ra point to lwu's stack map.

    .set push
    .set noat

    // Fields and array elements.

.Lintrospection_field_array:
    // Get the field/element address using $t8 and the offset from the lwu instruction.
    lh      $at, 0($ra)         # $ra points to lwu: $at = low 16 bits of field/element offset.
    daddiu  $ra, $ra, 4 + HEAP_POISON_INSTR_SIZE   # Skip lwu(+dsubu+dext).
    daddu   $t8, $t8, $at       # $t8 = field/element address.

    // Calculate the address of the exit point, store it in $t3 and load the reference into $t8.
    lb      $at, (-HEAP_POISON_INSTR_SIZE - 2)($ra)   # $ra-HEAP_POISON_INSTR_SIZE-4 points to
                                                      # "lwu `out`, ...".
    andi    $at, $at, 31        # Extract `out` from lwu.

    lwu     $t8, 0($t8)         # $t8 = reference.
    UNPOISON_HEAP_REF $t8

    // Return if null reference.
    bnez    $t8, .Lintrospection_common
    dlsa    $t3, $at, $t3, 3    # $t3 = address of the exit point
                                # (BRB_FIELD_EXIT* macro is 8 bytes).

    // Early return through the exit point.
.Lintrospection_return_early:
    jic     $t3, 0              # Move $t8 to `out` and return.

    // Code common for GC roots, fields and array elements.

.Lintrospection_common:
    // Check lock word for mark bit, if marked return.
    lw      $t9, MIRROR_OBJECT_LOCK_WORD_OFFSET($t8)
    sll     $at, $t9, 31 - LOCK_WORD_MARK_BIT_SHIFT     # Move mark bit to sign bit.
    bltzc   $at, .Lintrospection_return_early
#if (LOCK_WORD_STATE_SHIFT != 30) || (LOCK_WORD_STATE_FORWARDING_ADDRESS != 3)
    // The below code depends on the lock word state being in the highest bits
    // and the "forwarding address" state having all bits set.
#error "Unexpected lock word state shift or forwarding address state value."
#endif
    // Test that both the forwarding state bits are 1.
    sll     $at, $t9, 1
    and     $at, $at, $t9                               # Sign bit = 1 IFF both bits are 1.
    bgezc   $at, .Lintrospection_mark

    .set pop

    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    sll     $t8, $t9, LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    jalr    $zero, $t3          # Move $t8 to `out` and return.
    dext    $t8, $t8, 0, 32     # Make sure the address is zero-extended.

.Lintrospection_mark:
    // Partially set up the stack frame preserving only $ra.
    daddiu  $sp, $sp, -320
    .cfi_adjust_cfa_offset 320
    sd      $ra, 312($sp)
    .cfi_rel_offset 31, 312

    // Set up $gp, clobbering $ra.
    lapc    $ra, 1f
1:
    .cpsetup $ra, 304, 1b       # Save old $gp in 304($sp).

    // Finalize the stack frame and call.
    sd      $t3, 296($sp)       # Preserve the exit point address.
    sd      $t2, 288($sp)
    .cfi_rel_offset 14, 288
    sd      $t1, 280($sp)
    .cfi_rel_offset 13, 280
    sd      $t0, 272($sp)
    .cfi_rel_offset 12, 272
    sd      $a7, 264($sp)
    .cfi_rel_offset 11, 264
    sd      $a6, 256($sp)
    .cfi_rel_offset 10, 256
    sd      $a5, 248($sp)
    .cfi_rel_offset 9, 248
    sd      $a4, 240($sp)
    .cfi_rel_offset 8, 240
    sd      $a3, 232($sp)
    .cfi_rel_offset 7, 232
    sd      $a2, 224($sp)
    .cfi_rel_offset 6, 224
    sd      $a1, 216($sp)
    .cfi_rel_offset 5, 216
    sd      $a0, 208($sp)
    .cfi_rel_offset 4, 208
    sd      $v1, 200($sp)
    .cfi_rel_offset 3, 200
    sd      $v0, 192($sp)
    .cfi_rel_offset 2, 192

    dla     $t9, artReadBarrierMark

    sdc1    $f23, 184($sp)
    sdc1    $f22, 176($sp)
    sdc1    $f21, 168($sp)
    sdc1    $f20, 160($sp)
    sdc1    $f19, 152($sp)
    sdc1    $f18, 144($sp)
    sdc1    $f17, 136($sp)
    sdc1    $f16, 128($sp)
    sdc1    $f15, 120($sp)
    sdc1    $f14, 112($sp)
    sdc1    $f13, 104($sp)
    sdc1    $f12,  96($sp)
    sdc1    $f11,  88($sp)
    sdc1    $f10,  80($sp)
    sdc1    $f9,   72($sp)
    sdc1    $f8,   64($sp)
    sdc1    $f7,   56($sp)
    sdc1    $f6,   48($sp)
    sdc1    $f5,   40($sp)
    sdc1    $f4,   32($sp)
    sdc1    $f3,   24($sp)
    sdc1    $f2,   16($sp)
    sdc1    $f1,    8($sp)
    sdc1    $f0,    0($sp)

    jalr    $t9                 # $v0 <- artReadBarrierMark(reference)
    move    $a0, $t8            # Pass reference in $a0.
    move    $t8, $v0

    ld      $ra, 312($sp)
    .cfi_restore 31
    .cpreturn                   # Restore old $gp from 304($sp).
    ld      $t3, 296($sp)       # $t3 = address of the exit point.
    ld      $t2, 288($sp)
    .cfi_restore 14
    ld      $t1, 280($sp)
    .cfi_restore 13
    ld      $t0, 272($sp)
    .cfi_restore 12
    ld      $a7, 264($sp)
    .cfi_restore 11
    ld      $a6, 256($sp)
    .cfi_restore 10
    ld      $a5, 248($sp)
    .cfi_restore 9
    ld      $a4, 240($sp)
    .cfi_restore 8
    ld      $a3, 232($sp)
    .cfi_restore 7
    ld      $a2, 224($sp)
    .cfi_restore 6
    ld      $a1, 216($sp)
    .cfi_restore 5
    ld      $a0, 208($sp)
    .cfi_restore 4
    ld      $v1, 200($sp)
    .cfi_restore 3
    ld      $v0, 192($sp)
    .cfi_restore 2

    ldc1    $f23, 184($sp)
    ldc1    $f22, 176($sp)
    ldc1    $f21, 168($sp)
    ldc1    $f20, 160($sp)
    ldc1    $f19, 152($sp)
    ldc1    $f18, 144($sp)
    ldc1    $f17, 136($sp)
    ldc1    $f16, 128($sp)
    ldc1    $f15, 120($sp)
    ldc1    $f14, 112($sp)
    ldc1    $f13, 104($sp)
    ldc1    $f12,  96($sp)
    ldc1    $f11,  88($sp)
    ldc1    $f10,  80($sp)
    ldc1    $f9,   72($sp)
    ldc1    $f8,   64($sp)
    ldc1    $f7,   56($sp)
    ldc1    $f6,   48($sp)
    ldc1    $f5,   40($sp)
    ldc1    $f4,   32($sp)
    ldc1    $f3,   24($sp)
    ldc1    $f2,   16($sp)
    ldc1    $f1,    8($sp)
    ldc1    $f0,    0($sp)

    // Return through the exit point.
    jalr    $zero, $t3          # Move $t8 to `out` and return.
    daddiu  $sp, $sp, 320
    .cfi_adjust_cfa_offset -320

.Lintrospection_exits:
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT $v0
    BRB_FIELD_EXIT $v1
    BRB_FIELD_EXIT $a0
    BRB_FIELD_EXIT $a1
    BRB_FIELD_EXIT $a2
    BRB_FIELD_EXIT $a3
    BRB_FIELD_EXIT $a4
    BRB_FIELD_EXIT $a5
    BRB_FIELD_EXIT $a6
    BRB_FIELD_EXIT $a7
    BRB_FIELD_EXIT $t0
    BRB_FIELD_EXIT $t1
    BRB_FIELD_EXIT $t2
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT $s2
    BRB_FIELD_EXIT $s3
    BRB_FIELD_EXIT $s4
    BRB_FIELD_EXIT $s5
    BRB_FIELD_EXIT $s6
    BRB_FIELD_EXIT $s7
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT_BREAK
    BRB_FIELD_EXIT $s8
    BRB_FIELD_EXIT_BREAK
END art_quick_read_barrier_mark_introspection

.extern artInvokePolymorphic
ENTRY art_quick_invoke_polymorphic
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    move   $a2, rSELF                          # Make $a2 an alias for the current Thread.
    move   $a3, $sp                            # Make $a3 a pointer to the saved frame context.
    daddiu $sp, $sp, -8                        # Reserve space for JValue result.
    .cfi_adjust_cfa_offset 8
    sd     $zero, 0($sp)                       # Initialize JValue result.
    jal    artInvokePolymorphic                # artInvokePolymorphic(result, receiver, Thread*, context)
    move   $a0, $sp                            # Make $a0 a pointer to the JValue result
.macro MATCH_RETURN_TYPE c, handler
    li     $t0, \c
    beq    $v0, $t0, \handler
.endm
    MATCH_RETURN_TYPE 'V', .Lcleanup_and_return
    MATCH_RETURN_TYPE 'L', .Lstore_ref_result
    MATCH_RETURN_TYPE 'I', .Lstore_long_result
    MATCH_RETURN_TYPE 'J', .Lstore_long_result
    MATCH_RETURN_TYPE 'B', .Lstore_long_result
    MATCH_RETURN_TYPE 'C', .Lstore_char_result
    MATCH_RETURN_TYPE 'D', .Lstore_double_result
    MATCH_RETURN_TYPE 'F', .Lstore_float_result
    MATCH_RETURN_TYPE 'S', .Lstore_long_result
    MATCH_RETURN_TYPE 'Z', .Lstore_boolean_result
.purgem MATCH_RETURN_TYPE
    nop
    b .Lcleanup_and_return
    nop
.Lstore_boolean_result:
    b      .Lcleanup_and_return
    lbu    $v0, 0($sp)                         # Move byte from JValue result to return value register.
.Lstore_char_result:
    b      .Lcleanup_and_return
    lhu    $v0, 0($sp)                         # Move char from JValue result to return value register.
.Lstore_double_result:
.Lstore_float_result:
    b      .Lcleanup_and_return
    l.d    $f0, 0($sp)                         # Move double/float from JValue result to return value register.
.Lstore_ref_result:
    b      .Lcleanup_and_return
    lwu    $v0, 0($sp)                         # Move zero extended lower 32-bits to return value register.
.Lstore_long_result:
    ld     $v0, 0($sp)                         # Move long from JValue result to return value register.
    // Fall-through to clean up and return.
.Lcleanup_and_return:
    daddiu $sp, $sp, 8                         # Remove space for JValue result.
    .cfi_adjust_cfa_offset -8
    ld     $t0, THREAD_EXCEPTION_OFFSET(rSELF) # Load Thread::Current()->exception_
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    bnez   $t0, 1f                             # Success if no exception is pending.
    nop
    jalr   $zero, $ra
    nop
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_invoke_polymorphic

  .set pop
