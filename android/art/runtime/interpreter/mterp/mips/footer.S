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
