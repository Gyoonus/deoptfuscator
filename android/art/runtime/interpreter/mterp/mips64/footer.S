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
