/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "asm_support_x86_64.S"
#include "interpreter/cfi_asm_support.h"

#include "arch/quick_alloc_entrypoints.S"

MACRO0(ASSERT_USE_READ_BARRIER)
#if !defined(USE_READ_BARRIER)
    int3
    int3
#endif
END_MACRO

MACRO0(SETUP_FP_CALLEE_SAVE_FRAME)
    // Create space for ART FP callee-saved registers
    subq MACRO_LITERAL(4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    movq %xmm12, 0(%rsp)
    movq %xmm13, 8(%rsp)
    movq %xmm14, 16(%rsp)
    movq %xmm15, 24(%rsp)
END_MACRO

MACRO0(RESTORE_FP_CALLEE_SAVE_FRAME)
    // Restore ART FP callee-saved registers
    movq 0(%rsp), %xmm12
    movq 8(%rsp), %xmm13
    movq 16(%rsp), %xmm14
    movq 24(%rsp), %xmm15
    addq MACRO_LITERAL(4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(- 4 * 8)
END_MACRO

// For x86, the CFA is esp+4, the address above the pushed return address on the stack.

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     */
MACRO0(SETUP_SAVE_ALL_CALLEE_SAVES_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee save registers to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    // Create space for FPR args, plus space for ArtMethod*.
    subq MACRO_LITERAL(4 * 8 + 8), %rsp
    CFI_ADJUST_CFA_OFFSET(4 * 8 + 8)
    // Save FPRs.
    movq %xmm12, 8(%rsp)
    movq %xmm13, 16(%rsp)
    movq %xmm14, 24(%rsp)
    movq %xmm15, 32(%rsp)
    // R10 := ArtMethod* for save all callee save frame method.
    movq RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET(%r10), %r10
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the top quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 6 * 8 + 4 * 8 + 8 + 8)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly)
     */
MACRO0(SETUP_SAVE_REFS_ONLY_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    // Create space for FPR args, plus space for ArtMethod*.
    subq LITERAL(8 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(8 + 4 * 8)
    // Save FPRs.
    movq %xmm12, 8(%rsp)
    movq %xmm13, 16(%rsp)
    movq %xmm14, 24(%rsp)
    movq %xmm15, 32(%rsp)
    // R10 := ArtMethod* for refs only callee save frame method.
    movq RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET(%r10), %r10
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the stop quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 6 * 8 + 4 * 8 + 8 + 8)
#error "FRAME_SIZE_SAVE_REFS_ONLY(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

MACRO0(RESTORE_SAVE_REFS_ONLY_FRAME)
    movq 8(%rsp), %xmm12
    movq 16(%rsp), %xmm13
    movq 24(%rsp), %xmm14
    movq 32(%rsp), %xmm15
    addq LITERAL(8 + 4*8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8 - 4*8)
    // TODO: optimize by not restoring callee-saves restored by the ABI
    POP rbx
    POP rbp
    POP r12
    POP r13
    POP r14
    POP r15
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs)
     */
MACRO0(SETUP_SAVE_REFS_AND_ARGS_FRAME)
#if defined(__APPLE__)
    int3
    int3
#else
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH r9   // Quick arg 5.
    PUSH r8   // Quick arg 4.
    PUSH rsi  // Quick arg 1.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    PUSH rdx  // Quick arg 2.
    PUSH rcx  // Quick arg 3.
    // Create space for FPR args and create 2 slots for ArtMethod*.
    subq MACRO_LITERAL(16 + 12 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(16 + 12 * 8)
    // R10 := ArtMethod* for ref and args callee save frame method.
    movq RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET(%r10), %r10
    // Save FPRs.
    movq %xmm0, 16(%rsp)
    movq %xmm1, 24(%rsp)
    movq %xmm2, 32(%rsp)
    movq %xmm3, 40(%rsp)
    movq %xmm4, 48(%rsp)
    movq %xmm5, 56(%rsp)
    movq %xmm6, 64(%rsp)
    movq %xmm7, 72(%rsp)
    movq %xmm12, 80(%rsp)
    movq %xmm13, 88(%rsp)
    movq %xmm14, 96(%rsp)
    movq %xmm15, 104(%rsp)
    // Store ArtMethod* to bottom of stack.
    movq %r10, 0(%rsp)
    // Store rsp as the top quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 11 * 8 + 12 * 8 + 16 + 8)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

MACRO0(SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_RDI)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH r15  // Callee save.
    PUSH r14  // Callee save.
    PUSH r13  // Callee save.
    PUSH r12  // Callee save.
    PUSH r9   // Quick arg 5.
    PUSH r8   // Quick arg 4.
    PUSH rsi  // Quick arg 1.
    PUSH rbp  // Callee save.
    PUSH rbx  // Callee save.
    PUSH rdx  // Quick arg 2.
    PUSH rcx  // Quick arg 3.
    // Create space for FPR args and create 2 slots for ArtMethod*.
    subq LITERAL(80 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(80 + 4 * 8)
    // Save FPRs.
    movq %xmm0, 16(%rsp)
    movq %xmm1, 24(%rsp)
    movq %xmm2, 32(%rsp)
    movq %xmm3, 40(%rsp)
    movq %xmm4, 48(%rsp)
    movq %xmm5, 56(%rsp)
    movq %xmm6, 64(%rsp)
    movq %xmm7, 72(%rsp)
    movq %xmm12, 80(%rsp)
    movq %xmm13, 88(%rsp)
    movq %xmm14, 96(%rsp)
    movq %xmm15, 104(%rsp)
    // Store ArtMethod to bottom of stack.
    movq %rdi, 0(%rsp)
    // Store rsp as the stop quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET
END_MACRO

MACRO0(RESTORE_SAVE_REFS_AND_ARGS_FRAME)
    // Restore FPRs.
    movq 16(%rsp), %xmm0
    movq 24(%rsp), %xmm1
    movq 32(%rsp), %xmm2
    movq 40(%rsp), %xmm3
    movq 48(%rsp), %xmm4
    movq 56(%rsp), %xmm5
    movq 64(%rsp), %xmm6
    movq 72(%rsp), %xmm7
    movq 80(%rsp), %xmm12
    movq 88(%rsp), %xmm13
    movq 96(%rsp), %xmm14
    movq 104(%rsp), %xmm15
    addq MACRO_LITERAL(80 + 4 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(80 + 4 * 8))
    // Restore callee and GPR args, mixed together to agree with core spills bitmap.
    POP rcx
    POP rdx
    POP rbx
    POP rbp
    POP rsi
    POP r8
    POP r9
    POP r12
    POP r13
    POP r14
    POP r15
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when R14 and R15 are already saved.
     */
MACRO1(SETUP_SAVE_EVERYTHING_FRAME_R14_R15_SAVED, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
#if defined(__APPLE__)
    int3
    int3
#else
    // Save core registers from highest to lowest to agree with core spills bitmap.
    // R14 and R15, or at least placeholders for them, are already on the stack.
    PUSH r13
    PUSH r12
    PUSH r11
    PUSH r10
    PUSH r9
    PUSH r8
    PUSH rdi
    PUSH rsi
    PUSH rbp
    PUSH rbx
    PUSH rdx
    PUSH rcx
    PUSH rax
    // Create space for FPRs and stack alignment padding.
    subq MACRO_LITERAL(8 + 16 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(8 + 16 * 8)
    // R10 := Runtime::Current()
    movq _ZN3art7Runtime9instance_E@GOTPCREL(%rip), %r10
    movq (%r10), %r10
    // Save FPRs.
    movq %xmm0, 8(%rsp)
    movq %xmm1, 16(%rsp)
    movq %xmm2, 24(%rsp)
    movq %xmm3, 32(%rsp)
    movq %xmm4, 40(%rsp)
    movq %xmm5, 48(%rsp)
    movq %xmm6, 56(%rsp)
    movq %xmm7, 64(%rsp)
    movq %xmm8, 72(%rsp)
    movq %xmm9, 80(%rsp)
    movq %xmm10, 88(%rsp)
    movq %xmm11, 96(%rsp)
    movq %xmm12, 104(%rsp)
    movq %xmm13, 112(%rsp)
    movq %xmm14, 120(%rsp)
    movq %xmm15, 128(%rsp)
    // Push ArtMethod* for save everything frame method.
    pushq \runtime_method_offset(%r10)
    CFI_ADJUST_CFA_OFFSET(8)
    // Store rsp as the top quick frame.
    movq %rsp, %gs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +8: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_EVERYTHING != 15 * 8 + 16 * 8 + 16 + 8)
#error "FRAME_SIZE_SAVE_EVERYTHING(X86_64) size not as expected."
#endif
#endif  // __APPLE__
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when R15 is already saved.
     */
MACRO1(SETUP_SAVE_EVERYTHING_FRAME_R15_SAVED, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    PUSH r14
    SETUP_SAVE_EVERYTHING_FRAME_R14_R15_SAVED \runtime_method_offset
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     */
MACRO1(SETUP_SAVE_EVERYTHING_FRAME, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    PUSH r15
    SETUP_SAVE_EVERYTHING_FRAME_R15_SAVED \runtime_method_offset
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_FRPS)
    // Restore FPRs. Method and padding is still on the stack.
    movq 16(%rsp), %xmm0
    movq 24(%rsp), %xmm1
    movq 32(%rsp), %xmm2
    movq 40(%rsp), %xmm3
    movq 48(%rsp), %xmm4
    movq 56(%rsp), %xmm5
    movq 64(%rsp), %xmm6
    movq 72(%rsp), %xmm7
    movq 80(%rsp), %xmm8
    movq 88(%rsp), %xmm9
    movq 96(%rsp), %xmm10
    movq 104(%rsp), %xmm11
    movq 112(%rsp), %xmm12
    movq 120(%rsp), %xmm13
    movq 128(%rsp), %xmm14
    movq 136(%rsp), %xmm15
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_RAX)
    // Restore callee and GPR args (except RAX), mixed together to agree with core spills bitmap.
    POP rcx
    POP rdx
    POP rbx
    POP rbp
    POP rsi
    POP rdi
    POP r8
    POP r9
    POP r10
    POP r11
    POP r12
    POP r13
    POP r14
    POP r15
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME)
    RESTORE_SAVE_EVERYTHING_FRAME_FRPS

    // Remove save everything callee save method, stack alignment padding and FPRs.
    addq MACRO_LITERAL(16 + 16 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(16 + 16 * 8))

    POP rax
    RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_RAX
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_KEEP_RAX)
    RESTORE_SAVE_EVERYTHING_FRAME_FRPS

    // Remove save everything callee save method, stack alignment padding and FPRs, skip RAX.
    addq MACRO_LITERAL(16 + 16 * 8 + 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(16 + 16 * 8 + 8))

    RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_RAX
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     */
MACRO0(DELIVER_PENDING_EXCEPTION_FRAME_READY)
    // (Thread*) setup
    movq %gs:THREAD_SELF_OFFSET, %rdi
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*)
    UNREACHABLE
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME        // save callee saves for throw
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME  // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdi  // pass Thread::Current()
    call CALLVAR(cxx_name)             // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME        // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdi  // pass Thread::Current()
    call CALLVAR(cxx_name)             // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME  // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi  // pass Thread::Current()
    call CALLVAR(cxx_name)             // cxx_name(arg1, Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME        // save all registers as basis for long jump context
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx  // pass Thread::Current()
    call CALLVAR(cxx_name)             // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Call installed by a signal handler to create and deliver a NullPointerException.
     */
DEFINE_FUNCTION_CUSTOM_CFA art_quick_throw_null_pointer_exception_from_signal, 2 * __SIZEOF_POINTER__
    // Fault address and return address were saved by the fault handler.
    // Save all registers as basis for long jump context; R15 will replace fault address later.
    SETUP_SAVE_EVERYTHING_FRAME_R15_SAVED
    // Retrieve fault address and save R15.
    movq (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__)(%rsp), %rdi
    movq %r15, (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__)(%rsp)
    CFI_REL_OFFSET(%r15, (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__))
    // Outgoing argument set up; RDI already contains the fault address.
    movq %gs:THREAD_SELF_OFFSET, %rsi  // pass Thread::Current()
    call SYMBOL(artThrowNullPointerExceptionFromSignal)  // (addr, self)
    UNREACHABLE
END_FUNCTION art_quick_throw_null_pointer_exception_from_signal

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_div_zero, artThrowDivZeroFromCode

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException. Arg1 holds
     * index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_array_bounds, artThrowArrayBoundsFromCode

    /*
     * Called by managed code to create and deliver a StringIndexOutOfBoundsException
     * as if thrown from a call to String.charAt(). Arg1 holds index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_string_bounds, artThrowStringBoundsFromCode

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/rdi with the target Method*, arg0/rdi will contain
     * the method_idx.  This wrapper will save arg1-arg3, and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/rsi.
     *
     * The helper will attempt to locate the target and return a 128-bit result in rax/rdx consisting
     * of the target Method* in rax and method->code_ in rdx.
     *
     * If unsuccessful, the helper will return null/????. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the return
     * location on the stack.
     *
     * Adapted from x86 code.
     */
MACRO1(INVOKE_TRAMPOLINE_BODY, cxx_name)
    SETUP_SAVE_REFS_AND_ARGS_FRAME  // save callee saves in case allocation triggers GC
    // Helper signature is always
    // (method_idx, *this_object, *caller_method, *self, sp)

    movq %gs:THREAD_SELF_OFFSET, %rdx                      // pass Thread
    movq %rsp, %rcx                                        // pass SP

    call CALLVAR(cxx_name)                                 // cxx_name(arg1, arg2, Thread*, SP)
                                                           // save the code pointer
    movq %rax, %rdi
    movq %rdx, %rax
    RESTORE_SAVE_REFS_AND_ARGS_FRAME

    testq %rdi, %rdi
    jz 1f

    // Tail call to intended method.
    jmp *%rax
1:
    DELIVER_PENDING_EXCEPTION
END_MACRO
MACRO2(INVOKE_TRAMPOLINE, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    INVOKE_TRAMPOLINE_BODY RAW_VAR(cxx_name)
    END_FUNCTION VAR(c_name)
END_MACRO

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck


    /*
     * Helper for quick invocation stub to set up XMM registers. Assumes r10 == shorty,
     * r11 == arg_array. Clobbers r10, r11 and al. Branches to xmm_setup_finished if it encounters
     * the end of the shorty.
     */
MACRO2(LOOP_OVER_SHORTY_LOADING_XMMS, xmm_reg, finished)
1: // LOOP
    movb (%r10), %al              // al := *shorty
    addq MACRO_LITERAL(1), %r10   // shorty++
    cmpb MACRO_LITERAL(0), %al    // if (al == '\0') goto xmm_setup_finished
    je VAR(finished)
    cmpb MACRO_LITERAL(68), %al   // if (al == 'D') goto FOUND_DOUBLE
    je 2f
    cmpb MACRO_LITERAL(70), %al   // if (al == 'F') goto FOUND_FLOAT
    je 3f
    addq MACRO_LITERAL(4), %r11   // arg_array++
    //  Handle extra space in arg array taken by a long.
    cmpb MACRO_LITERAL(74), %al   // if (al != 'J') goto LOOP
    jne 1b
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 1b                        // goto LOOP
2:  // FOUND_DOUBLE
    movsd (%r11), REG_VAR(xmm_reg)
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 4f
3:  // FOUND_FLOAT
    movss (%r11), REG_VAR(xmm_reg)
    addq MACRO_LITERAL(4), %r11   // arg_array++
4:
END_MACRO

    /*
     * Helper for quick invocation stub to set up GPR registers. Assumes r10 == shorty,
     * r11 == arg_array. Clobbers r10, r11 and al. Branches to gpr_setup_finished if it encounters
     * the end of the shorty.
     */
MACRO3(LOOP_OVER_SHORTY_LOADING_GPRS, gpr_reg64, gpr_reg32, finished)
1: // LOOP
    movb (%r10), %al              // al := *shorty
    addq MACRO_LITERAL(1), %r10   // shorty++
    cmpb MACRO_LITERAL(0), %al    // if (al == '\0') goto gpr_setup_finished
    je  VAR(finished)
    cmpb MACRO_LITERAL(74), %al   // if (al == 'J') goto FOUND_LONG
    je 2f
    cmpb MACRO_LITERAL(70), %al   // if (al == 'F') goto SKIP_FLOAT
    je 3f
    cmpb MACRO_LITERAL(68), %al   // if (al == 'D') goto SKIP_DOUBLE
    je 4f
    movl (%r11), REG_VAR(gpr_reg32)
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 5f
2:  // FOUND_LONG
    movq (%r11), REG_VAR(gpr_reg64)
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 5f
3:  // SKIP_FLOAT
    addq MACRO_LITERAL(4), %r11   // arg_array++
    jmp 1b
4:  // SKIP_DOUBLE
    addq MACRO_LITERAL(8), %r11   // arg_array+=2
    jmp 1b
5:
END_MACRO

    /*
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   rdi = method pointer
     *   rsi = argument array that must at least contain the this pointer.
     *   rdx = size of argument array in bytes
     *   rcx = (managed) thread pointer
     *   r8 = JValue* result
     *   r9 = char* shorty
     */
DEFINE_FUNCTION art_quick_invoke_stub
#if defined(__APPLE__)
    int3
    int3
#else
    // Set up argument XMM registers.
    leaq 1(%r9), %r10             // R10 := shorty + 1  ; ie skip return arg character.
    leaq 4(%rsi), %r11            // R11 := arg_array + 4 ; ie skip this pointer.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm4, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm5, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm6, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm7, .Lxmm_setup_finished
    .balign 16
.Lxmm_setup_finished:
    PUSH rbp                      // Save rbp.
    PUSH r8                       // Save r8/result*.
    PUSH r9                       // Save r9/shorty*.
    PUSH rbx                      // Save native callee save rbx
    PUSH r12                      // Save native callee save r12
    PUSH r13                      // Save native callee save r13
    PUSH r14                      // Save native callee save r14
    PUSH r15                      // Save native callee save r15
    movq %rsp, %rbp               // Copy value of stack pointer into base pointer.
    CFI_DEF_CFA_REGISTER(rbp)

    movl %edx, %r10d
    addl LITERAL(100), %edx        // Reserve space for return addr, StackReference<method>, rbp,
                                   // r8, r9, rbx, r12, r13, r14, and r15 in frame.
    andl LITERAL(0xFFFFFFF0), %edx // Align frame size to 16 bytes.
    subl LITERAL(72), %edx         // Remove space for return address, rbp, r8, r9, rbx, r12,
                                   // r13, r14, and r15
    subq %rdx, %rsp                // Reserve stack space for argument array.

#if (STACK_REFERENCE_SIZE != 4)
#error "STACK_REFERENCE_SIZE(X86_64) size not as expected."
#endif
    movq LITERAL(0), (%rsp)       // Store null for method*

    movl %r10d, %ecx              // Place size of args in rcx.
    movq %rdi, %rax               // rax := method to be called
    movq %rsi, %r11               // r11 := arg_array
    leaq 8(%rsp), %rdi            // rdi is pointing just above the ArtMethod* in the stack
                                  // arguments.
    // Copy arg array into stack.
    rep movsb                     // while (rcx--) { *rdi++ = *rsi++ }
    leaq 1(%r9), %r10             // r10 := shorty + 1  ; ie skip return arg character
    movq %rax, %rdi               // rdi := method to be called
    movl (%r11), %esi             // rsi := this pointer
    addq LITERAL(4), %r11         // arg_array++
    LOOP_OVER_SHORTY_LOADING_GPRS rdx, edx, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS rcx, ecx, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS r8, r8d, .Lgpr_setup_finished
    LOOP_OVER_SHORTY_LOADING_GPRS r9, r9d, .Lgpr_setup_finished
.Lgpr_setup_finished:
    call *ART_METHOD_QUICK_CODE_OFFSET_64(%rdi) // Call the method.
    movq %rbp, %rsp               // Restore stack pointer.
    POP r15                       // Pop r15
    POP r14                       // Pop r14
    POP r13                       // Pop r13
    POP r12                       // Pop r12
    POP rbx                       // Pop rbx
    POP r9                        // Pop r9 - shorty*
    POP r8                        // Pop r8 - result*.
    POP rbp                       // Pop rbp
    cmpb LITERAL(68), (%r9)       // Test if result type char == 'D'.
    je .Lreturn_double_quick
    cmpb LITERAL(70), (%r9)       // Test if result type char == 'F'.
    je .Lreturn_float_quick
    movq %rax, (%r8)              // Store the result assuming its a long, int or Object*
    ret
.Lreturn_double_quick:
    movsd %xmm0, (%r8)            // Store the double floating point result.
    ret
.Lreturn_float_quick:
    movss %xmm0, (%r8)            // Store the floating point result.
    ret
#endif  // __APPLE__
END_FUNCTION art_quick_invoke_stub

    /*
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   rdi = method pointer
     *   rsi = argument array or null if no arguments.
     *   rdx = size of argument array in bytes
     *   rcx = (managed) thread pointer
     *   r8 = JValue* result
     *   r9 = char* shorty
     */
DEFINE_FUNCTION art_quick_invoke_static_stub
#if defined(__APPLE__)
    int3
    int3
#else
    // Set up argument XMM registers.
    leaq 1(%r9), %r10             // R10 := shorty + 1  ; ie skip return arg character
    movq %rsi, %r11               // R11 := arg_array
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm4, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm5, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm6, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm7, .Lxmm_setup_finished2
    .balign 16
.Lxmm_setup_finished2:
    PUSH rbp                      // Save rbp.
    PUSH r8                       // Save r8/result*.
    PUSH r9                       // Save r9/shorty*.
    PUSH rbx                      // Save rbx
    PUSH r12                      // Save r12
    PUSH r13                      // Save r13
    PUSH r14                      // Save r14
    PUSH r15                      // Save r15
    movq %rsp, %rbp               // Copy value of stack pointer into base pointer.
    CFI_DEF_CFA_REGISTER(rbp)

    movl %edx, %r10d
    addl LITERAL(100), %edx        // Reserve space for return addr, StackReference<method>, rbp,
                                   // r8, r9, r12, r13, r14, and r15 in frame.
    andl LITERAL(0xFFFFFFF0), %edx // Align frame size to 16 bytes.
    subl LITERAL(72), %edx         // Remove space for return address, rbp, r8, r9, rbx, r12,
                                   // r13, r14, and r15.
    subq %rdx, %rsp                // Reserve stack space for argument array.

#if (STACK_REFERENCE_SIZE != 4)
#error "STACK_REFERENCE_SIZE(X86_64) size not as expected."
#endif
    movq LITERAL(0), (%rsp)        // Store null for method*

    movl %r10d, %ecx               // Place size of args in rcx.
    movq %rdi, %rax                // rax := method to be called
    movq %rsi, %r11                // r11 := arg_array
    leaq 8(%rsp), %rdi             // rdi is pointing just above the ArtMethod* in the
                                   // stack arguments.
    // Copy arg array into stack.
    rep movsb                      // while (rcx--) { *rdi++ = *rsi++ }
    leaq 1(%r9), %r10              // r10 := shorty + 1  ; ie skip return arg character
    movq %rax, %rdi                // rdi := method to be called
    LOOP_OVER_SHORTY_LOADING_GPRS rsi, esi, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS rdx, edx, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS rcx, ecx, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS r8, r8d, .Lgpr_setup_finished2
    LOOP_OVER_SHORTY_LOADING_GPRS r9, r9d, .Lgpr_setup_finished2
.Lgpr_setup_finished2:
    call *ART_METHOD_QUICK_CODE_OFFSET_64(%rdi) // Call the method.
    movq %rbp, %rsp                // Restore stack pointer.
    POP r15                        // Pop r15
    POP r14                        // Pop r14
    POP r13                        // Pop r13
    POP r12                        // Pop r12
    POP rbx                        // Pop rbx
    POP r9                         // Pop r9 - shorty*.
    POP r8                         // Pop r8 - result*.
    POP rbp                        // Pop rbp
    cmpb LITERAL(68), (%r9)        // Test if result type char == 'D'.
    je .Lreturn_double_quick2
    cmpb LITERAL(70), (%r9)        // Test if result type char == 'F'.
    je .Lreturn_float_quick2
    movq %rax, (%r8)               // Store the result assuming its a long, int or Object*
    ret
.Lreturn_double_quick2:
    movsd %xmm0, (%r8)             // Store the double floating point result.
    ret
.Lreturn_float_quick2:
    movss %xmm0, (%r8)             // Store the floating point result.
    ret
#endif  // __APPLE__
END_FUNCTION art_quick_invoke_static_stub

    /*
     * Long jump stub.
     * On entry:
     *   rdi = gprs
     *   rsi = fprs
     */
DEFINE_FUNCTION art_quick_do_long_jump
#if defined(__APPLE__)
    int3
    int3
#else
    // Restore FPRs.
    movq 0(%rsi), %xmm0
    movq 8(%rsi), %xmm1
    movq 16(%rsi), %xmm2
    movq 24(%rsi), %xmm3
    movq 32(%rsi), %xmm4
    movq 40(%rsi), %xmm5
    movq 48(%rsi), %xmm6
    movq 56(%rsi), %xmm7
    movq 64(%rsi), %xmm8
    movq 72(%rsi), %xmm9
    movq 80(%rsi), %xmm10
    movq 88(%rsi), %xmm11
    movq 96(%rsi), %xmm12
    movq 104(%rsi), %xmm13
    movq 112(%rsi), %xmm14
    movq 120(%rsi), %xmm15
    // Restore FPRs.
    movq %rdi, %rsp   // RSP points to gprs.
    // Load all registers except RSP and RIP with values in gprs.
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    addq LITERAL(8), %rsp   // Skip rsp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    popq %rsp      // Load stack pointer.
    ret            // From higher in the stack pop rip.
#endif  // __APPLE__
END_FUNCTION art_quick_do_long_jump

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME           // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi    // pass Thread::Current()
    call CALLVAR(cxx_name)               // cxx_name(arg0, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME         // restore frame up to return address
    CALL_MACRO(return_macro)             // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME           // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx    // pass Thread::Current()
    call CALLVAR(cxx_name)               // cxx_name(arg0, arg1, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME         // restore frame up to return address
    CALL_MACRO(return_macro)             // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME          // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rcx   // pass Thread::Current()
    call CALLVAR(cxx_name)              // cxx_name(arg0, arg1, arg2, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME        // restore frame up to return address
    CALL_MACRO(return_macro)            // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(FOUR_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME          // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %r8    // pass Thread::Current()
    call CALLVAR(cxx_name)              // cxx_name(arg1, arg2, arg3, arg4, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME        // restore frame up to return address
    CALL_MACRO(return_macro)            // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(ONE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME
                                        // arg0 is in rdi
    movq %gs:THREAD_SELF_OFFSET, %rsi   // pass Thread::Current()
    call CALLVAR(cxx_name)              // cxx_name(arg0, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME        // restore frame up to return address
    CALL_MACRO(return_macro)
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(TWO_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME
                                        // arg0 and arg1 are in rdi/rsi
    movq %gs:THREAD_SELF_OFFSET, %rdx   // pass Thread::Current()
    call CALLVAR(cxx_name)              // (arg0, arg1, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME        // restore frame up to return address
    CALL_MACRO(return_macro)
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(THREE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME
                                        // arg0, arg1, and arg2 are in rdi/rsi/rdx
    movq %gs:THREAD_SELF_OFFSET, %rcx   // pass Thread::Current()
    call CALLVAR(cxx_name)              // cxx_name(arg0, arg1, arg2, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME        // restore frame up to return address
    CALL_MACRO(return_macro)            // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

// Macro for string and type resolution and initialization.
MACRO3(ONE_ARG_SAVE_EVERYTHING_DOWNCALL, c_name, cxx_name, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME \runtime_method_offset  // save everything for GC
    // Outgoing argument set up
    movl %eax, %edi                               // pass string index
    movq %gs:THREAD_SELF_OFFSET, %rsi             // pass Thread::Current()
    call CALLVAR(cxx_name)                        // cxx_name(arg0, Thread*)
    testl %eax, %eax                              // If result is null, deliver the OOME.
    jz 1f
    CFI_REMEMBER_STATE
    RESTORE_SAVE_EVERYTHING_FRAME_KEEP_RAX        // restore frame up to return address
    ret
    CFI_RESTORE_STATE
    CFI_DEF_CFA(rsp, FRAME_SIZE_SAVE_EVERYTHING)  // workaround for clang bug: 31975598
1:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT, c_name, cxx_name)
    ONE_ARG_SAVE_EVERYTHING_DOWNCALL \c_name, \cxx_name, RUNTIME_SAVE_EVERYTHING_FOR_CLINIT_METHOD_OFFSET
END_MACRO

MACRO0(RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER)
    testq %rax, %rax               // rax == 0 ?
    jz  1f                         // if rax == 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_IF_EAX_ZERO)
    testl %eax, %eax               // eax == 0 ?
    jnz  1f                        // if eax != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_OR_DELIVER_PENDING_EXCEPTION)
    movq %gs:THREAD_EXCEPTION_OFFSET, %rcx // get exception field
    testq %rcx, %rcx               // rcx == 0 ?
    jnz 1f                         // if rcx != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

// Generate the allocation entrypoints for each allocator.
GENERATE_ALLOC_ENTRYPOINTS_FOR_NON_TLAB_ALLOCATORS

// Comment out allocators that have x86_64 specific asm.
// Region TLAB:
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
// Normal TLAB:
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


// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_rosalloc, RosAlloc).
MACRO2(ART_QUICK_ALLOC_OBJECT_ROSALLOC, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    // Fast path rosalloc allocation.
    // RDI: mirror::Class*, RAX: return value
    // RSI, RDX, RCX, R8, R9: free.
                                                           // Check if the thread local
                                                           // allocation stack has room.
    movq   %gs:THREAD_SELF_OFFSET, %r8                     // r8 = thread
    movq   THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET(%r8), %rcx  // rcx = alloc stack top.
    cmpq   THREAD_LOCAL_ALLOC_STACK_END_OFFSET(%r8), %rcx
    jae    .Lslow_path\c_name
                                                           // Load the object size
    movl   MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET(%rdi), %eax
                                                           // Check if the size is for a thread
                                                           // local allocation. Also does the
                                                           // initialized and finalizable checks.
    cmpl   LITERAL(ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE), %eax
    ja     .Lslow_path\c_name
                                                           // Compute the rosalloc bracket index
                                                           // from the size.
    shrq   LITERAL(ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT), %rax
                                                           // Load the rosalloc run (r9)
                                                           // Subtract __SIZEOF_POINTER__ to
                                                           // subtract one from edi as there is no
                                                           // 0 byte run and the size is already
                                                           // aligned.
    movq   (THREAD_ROSALLOC_RUNS_OFFSET - __SIZEOF_POINTER__)(%r8, %rax, __SIZEOF_POINTER__), %r9
                                                           // Load the free list head (rax). This
                                                           // will be the return val.
    movq   (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)(%r9), %rax
    testq  %rax, %rax
    jz     .Lslow_path\c_name
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber rdi and rsi.
                                                           // Push the new object onto the thread
                                                           // local allocation stack and
                                                           // increment the thread local
                                                           // allocation stack top.
    movl   %eax, (%rcx)
    addq   LITERAL(COMPRESSED_REFERENCE_SIZE), %rcx
    movq   %rcx, THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET(%r8)
                                                           // Load the next pointer of the head
                                                           // and update the list head with the
                                                           // next pointer.
    movq   ROSALLOC_SLOT_NEXT_OFFSET(%rax), %rcx
    movq   %rcx, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)(%r9)
                                                           // Store the class pointer in the
                                                           // header. This also overwrites the
                                                           // next pointer. The offsets are
                                                           // asserted to match.
#if ROSALLOC_SLOT_NEXT_OFFSET != MIRROR_OBJECT_CLASS_OFFSET
#error "Class pointer needs to overwrite next pointer."
#endif
    POISON_HEAP_REF edi
    movl   %edi, MIRROR_OBJECT_CLASS_OFFSET(%rax)
                                                           // Decrement the size of the free list
    decl   (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)(%r9)
                                                           // No fence necessary for x86.
    ret
.Lslow_path\c_name:
    SETUP_SAVE_REFS_ONLY_FRAME                             // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi                      // pass Thread::Current()
    call CALLVAR(cxx_name)                                 // cxx_name(arg0, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME                           // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER                // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_resolved_rosalloc, artAllocObjectFromCodeResolvedRosAlloc
ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_initialized_rosalloc, artAllocObjectFromCodeInitializedRosAlloc

// The common fast path code for art_quick_alloc_object_resolved_region_tlab.
// TODO: delete ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH since it is the same as
// ALLOC_OBJECT_INITIALIZED_TLAB_FAST_PATH.
//
// RDI: the class, RAX: return value.
// RCX, RSI, RDX: scratch, r8: Thread::Current().
MACRO1(ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH, slowPathLabel)
    ALLOC_OBJECT_INITIALIZED_TLAB_FAST_PATH(RAW_VAR(slowPathLabel))
END_MACRO

// The fast path code for art_quick_alloc_object_initialized_region_tlab.
//
// RDI: the class, RSI: ArtMethod*, RAX: return value.
// RCX, RSI, RDX: scratch, r8: Thread::Current().
MACRO1(ALLOC_OBJECT_INITIALIZED_TLAB_FAST_PATH, slowPathLabel)
    movq %gs:THREAD_SELF_OFFSET, %r8                           // r8 = thread
    movl MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET(%rdi), %ecx // Load the object size.
    movq THREAD_LOCAL_POS_OFFSET(%r8), %rax
    addq %rax, %rcx                                            // Add size to pos, note that these
                                                               // are both 32 bit ints, overflow
                                                               // will cause the add to be past the
                                                               // end of the thread local region.
    cmpq THREAD_LOCAL_END_OFFSET(%r8), %rcx                    // Check if it fits.
    ja   RAW_VAR(slowPathLabel)
    movq %rcx, THREAD_LOCAL_POS_OFFSET(%r8)                    // Update thread_local_pos.
    incq THREAD_LOCAL_OBJECTS_OFFSET(%r8)                      // Increase thread_local_objects.
                                                               // Store the class pointer in the
                                                               // header.
                                                               // No fence needed for x86.
    POISON_HEAP_REF edi
    movl %edi, MIRROR_OBJECT_CLASS_OFFSET(%rax)
    ret                                                        // Fast path succeeded.
END_MACRO

// The fast path code for art_quick_alloc_array_region_tlab.
// Inputs: RDI: the class, RSI: int32_t component_count, R9: total_size
// Free temps: RCX, RDX, R8
// Output: RAX: return value.
MACRO1(ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE, slowPathLabel)
    movq %gs:THREAD_SELF_OFFSET, %rcx                          // rcx = thread
    // Mask out the unaligned part to make sure we are 8 byte aligned.
    andq LITERAL(OBJECT_ALIGNMENT_MASK_TOGGLED64), %r9
    movq THREAD_LOCAL_POS_OFFSET(%rcx), %rax
    addq %rax, %r9
    cmpq THREAD_LOCAL_END_OFFSET(%rcx), %r9                    // Check if it fits.
    ja   RAW_VAR(slowPathLabel)
    movq %r9, THREAD_LOCAL_POS_OFFSET(%rcx)                    // Update thread_local_pos.
    addq LITERAL(1), THREAD_LOCAL_OBJECTS_OFFSET(%rcx)         // Increase thread_local_objects.
                                                               // Store the class pointer in the
                                                               // header.
                                                               // No fence needed for x86.
    POISON_HEAP_REF edi
    movl %edi, MIRROR_OBJECT_CLASS_OFFSET(%rax)
    movl %esi, MIRROR_ARRAY_LENGTH_OFFSET(%rax)
    ret                                                        // Fast path succeeded.
END_MACRO

// The common slow path code for art_quick_alloc_object_{resolved, initialized}_tlab
// and art_quick_alloc_object_{resolved, initialized}_region_tlab.
MACRO1(ALLOC_OBJECT_TLAB_SLOW_PATH, cxx_name)
    SETUP_SAVE_REFS_ONLY_FRAME                             // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rsi                      // pass Thread::Current()
    call CALLVAR(cxx_name)                                 // cxx_name(arg0, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME                           // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER                // return or deliver exception
END_MACRO

// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_tlab, TLAB). May be
// called with CC if the GC is not active.
DEFINE_FUNCTION art_quick_alloc_object_resolved_tlab
    // RDI: mirror::Class* klass
    // RDX, RSI, RCX, R8, R9: free. RAX: return val.
    ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH .Lart_quick_alloc_object_resolved_tlab_slow_path
.Lart_quick_alloc_object_resolved_tlab_slow_path:
    ALLOC_OBJECT_TLAB_SLOW_PATH artAllocObjectFromCodeResolvedTLAB
END_FUNCTION art_quick_alloc_object_resolved_tlab

// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_tlab, TLAB).
// May be called with CC if the GC is not active.
DEFINE_FUNCTION art_quick_alloc_object_initialized_tlab
    // RDI: mirror::Class* klass
    // RDX, RSI, RCX, R8, R9: free. RAX: return val.
    ALLOC_OBJECT_INITIALIZED_TLAB_FAST_PATH .Lart_quick_alloc_object_initialized_tlab_slow_path
.Lart_quick_alloc_object_initialized_tlab_slow_path:
    ALLOC_OBJECT_TLAB_SLOW_PATH artAllocObjectFromCodeInitializedTLAB
END_FUNCTION art_quick_alloc_object_initialized_tlab

MACRO0(COMPUTE_ARRAY_SIZE_UNKNOWN)
    movl MIRROR_CLASS_COMPONENT_TYPE_OFFSET(%rdi), %ecx        // Load component type.
    UNPOISON_HEAP_REF ecx
    movl MIRROR_CLASS_OBJECT_PRIMITIVE_TYPE_OFFSET(%rcx), %ecx // Load primitive type.
    shrq MACRO_LITERAL(PRIMITIVE_TYPE_SIZE_SHIFT_SHIFT), %rcx        // Get component size shift.
    movq %rsi, %r9
    salq %cl, %r9                                              // Calculate array count shifted.
    // Add array header + alignment rounding.
    addq MACRO_LITERAL(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %r9
    // Add 4 extra bytes if we are doing a long array.
    addq MACRO_LITERAL(1), %rcx
    andq MACRO_LITERAL(4), %rcx
#if MIRROR_LONG_ARRAY_DATA_OFFSET != MIRROR_INT_ARRAY_DATA_OFFSET + 4
#error Long array data offset must be 4 greater than int array data offset.
#endif
    addq %rcx, %r9
END_MACRO

MACRO0(COMPUTE_ARRAY_SIZE_8)
    // RDI: mirror::Class* klass, RSI: int32_t component_count
    // RDX, RCX, R8, R9: free. RAX: return val.
    movq %rsi, %r9
    // Add array header + alignment rounding.
    addq MACRO_LITERAL(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %r9
END_MACRO

MACRO0(COMPUTE_ARRAY_SIZE_16)
    // RDI: mirror::Class* klass, RSI: int32_t component_count
    // RDX, RCX, R8, R9: free. RAX: return val.
    movq %rsi, %r9
    salq MACRO_LITERAL(1), %r9
    // Add array header + alignment rounding.
    addq MACRO_LITERAL(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %r9
END_MACRO

MACRO0(COMPUTE_ARRAY_SIZE_32)
    // RDI: mirror::Class* klass, RSI: int32_t component_count
    // RDX, RCX, R8, R9: free. RAX: return val.
    movq %rsi, %r9
    salq MACRO_LITERAL(2), %r9
    // Add array header + alignment rounding.
    addq MACRO_LITERAL(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %r9
END_MACRO

MACRO0(COMPUTE_ARRAY_SIZE_64)
    // RDI: mirror::Class* klass, RSI: int32_t component_count
    // RDX, RCX, R8, R9: free. RAX: return val.
    movq %rsi, %r9
    salq MACRO_LITERAL(3), %r9
    // Add array header + alignment rounding.
    addq MACRO_LITERAL(MIRROR_WIDE_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %r9
END_MACRO

MACRO3(GENERATE_ALLOC_ARRAY_TLAB, c_entrypoint, cxx_name, size_setup)
    DEFINE_FUNCTION VAR(c_entrypoint)
    // RDI: mirror::Class* klass, RSI: int32_t component_count
    // RDX, RCX, R8, R9: free. RAX: return val.
    CALL_MACRO(size_setup)
    ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE .Lslow_path\c_entrypoint
.Lslow_path\c_entrypoint:
    SETUP_SAVE_REFS_ONLY_FRAME                                 // save ref containing registers for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdx                          // pass Thread::Current()
    call CALLVAR(cxx_name)                                     // cxx_name(arg0, arg1, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME                               // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER                    // return or deliver exception
    END_FUNCTION VAR(c_entrypoint)
END_MACRO


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

// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_RESOLVED(_region_tlab, RegionTLAB).
DEFINE_FUNCTION art_quick_alloc_object_resolved_region_tlab
    // Fast path region tlab allocation.
    // RDI: mirror::Class* klass
    // RDX, RSI, RCX, R8, R9: free. RAX: return val.
    ASSERT_USE_READ_BARRIER
    ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH .Lart_quick_alloc_object_resolved_region_tlab_slow_path
.Lart_quick_alloc_object_resolved_region_tlab_slow_path:
    ALLOC_OBJECT_TLAB_SLOW_PATH artAllocObjectFromCodeResolvedRegionTLAB
END_FUNCTION art_quick_alloc_object_resolved_region_tlab

// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_OBJECT_INITIALIZED(_region_tlab, RegionTLAB).
DEFINE_FUNCTION art_quick_alloc_object_initialized_region_tlab
    // Fast path region tlab allocation.
    // RDI: mirror::Class* klass
    // RDX, RSI, RCX, R8, R9: free. RAX: return val.
    ASSERT_USE_READ_BARRIER
    // No read barrier since the caller is responsible for that.
    ALLOC_OBJECT_INITIALIZED_TLAB_FAST_PATH .Lart_quick_alloc_object_initialized_region_tlab_slow_path
.Lart_quick_alloc_object_initialized_region_tlab_slow_path:
    ALLOC_OBJECT_TLAB_SLOW_PATH artAllocObjectFromCodeInitializedRegionTLAB
END_FUNCTION art_quick_alloc_object_initialized_region_tlab

ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_static_storage, artInitializeStaticStorageFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_type, artInitializeTypeFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_resolve_string, artResolveStringFromCode

TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_EAX_ZERO

DEFINE_FUNCTION art_quick_lock_object
    testl %edi, %edi                      // Null check object/rdi.
    jz   .Lslow_lock
.Lretry_lock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi), %ecx  // ecx := lock word.
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx         // Test the 2 high bits.
    jne  .Lslow_lock                      // Slow path if either of the two high bits are set.
    movl %ecx, %edx                       // save lock word (edx) to keep read barrier bits.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %ecx  // zero the gc bits.
    test %ecx, %ecx
    jnz  .Lalready_thin                   // Lock word contains a thin lock.
    // unlocked case - edx: original lock word, edi: obj.
    movl %edx, %eax                       // eax: lock word zero except for read barrier bits.
    movl %gs:THREAD_ID_OFFSET, %edx       // edx := thread id
    or   %eax, %edx                       // edx: thread id with count of 0 + read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
    jnz  .Lretry_lock                     // cmpxchg failed retry
    ret
.Lalready_thin:  // edx: lock word (with high 2 bits zero and original rb bits), edi: obj.
    movl %gs:THREAD_ID_OFFSET, %ecx       // ecx := thread id
    cmpw %cx, %dx                         // do we hold the lock already?
    jne  .Lslow_lock
    movl %edx, %ecx                       // copy the lock word to check count overflow.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %ecx  // zero the gc bits.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // increment recursion count
    test LITERAL(LOCK_WORD_READ_BARRIER_STATE_MASK), %ecx  // overflowed if the upper bit (28) is set
    jne  .Lslow_lock                      // count overflowed so go slow
    movl %edx, %eax                       // copy the lock word as the old val for cmpxchg.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx   // increment recursion count again for real.
    // update lockword, cmpxchg necessary for read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, edx: new val.
    jnz  .Lretry_lock                     // cmpxchg failed retry
    ret
.Lslow_lock:
    SETUP_SAVE_REFS_ONLY_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artLockObjectFromCode)    // artLockObjectFromCode(object, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object

DEFINE_FUNCTION art_quick_lock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artLockObjectFromCode)    // artLockObjectFromCode(object, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object_no_inline

DEFINE_FUNCTION art_quick_unlock_object
    testl %edi, %edi                      // null check object/edi
    jz   .Lslow_unlock
.Lretry_unlock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi), %ecx  // ecx := lock word
    movl %gs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx
    jnz  .Lslow_unlock                    // lock word contains a monitor
    cmpw %cx, %dx                         // does the thread id match?
    jne  .Lslow_unlock
    movl %ecx, %edx                       // copy the lock word to detect new count of 0.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %edx  // zero the gc bits.
    cmpl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx
    jae  .Lrecursive_thin_unlock
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %ecx, %eax                       // eax: old lock word.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED), %ecx  // ecx: new lock word zero except original gc bits.
#ifndef USE_READ_BARRIER
    movl %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, ecx: new val.
    jnz  .Lretry_unlock                   // cmpxchg failed retry
#endif
    ret
.Lrecursive_thin_unlock:  // ecx: original lock word, edi: obj
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %ecx, %eax                       // eax: old lock word.
    subl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx
#ifndef USE_READ_BARRIER
    mov  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edi)  // eax: old val, ecx: new val.
    jnz  .Lretry_unlock                   // cmpxchg failed retry
#endif
    ret
.Lslow_unlock:
    SETUP_SAVE_REFS_ONLY_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object

DEFINE_FUNCTION art_quick_unlock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rsi     // pass Thread::Current()
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object_no_inline

DEFINE_FUNCTION art_quick_check_instance_of
    // We could check the super classes here but that is usually already checked in the caller.
    PUSH rdi                          // Save args for exc
    PUSH rsi
    subq LITERAL(8), %rsp             // Alignment padding.
    CFI_ADJUST_CFA_OFFSET(8)
    SETUP_FP_CALLEE_SAVE_FRAME
    call SYMBOL(artInstanceOfFromCode)  // (Object* obj, Class* ref_klass)
    testq %rax, %rax
    jz .Lthrow_class_cast_exception   // jump forward if not assignable
    CFI_REMEMBER_STATE
    RESTORE_FP_CALLEE_SAVE_FRAME
    addq LITERAL(24), %rsp            // pop arguments
    CFI_ADJUST_CFA_OFFSET(-24)
    ret
    CFI_RESTORE_STATE                 // Reset unwind info so following code unwinds.

.Lthrow_class_cast_exception:
    RESTORE_FP_CALLEE_SAVE_FRAME
    addq LITERAL(8), %rsp             // pop padding
    CFI_ADJUST_CFA_OFFSET(-8)
    POP rsi                           // Pop arguments
    POP rdi

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME // save all registers as basis for long jump context
    mov %gs:THREAD_SELF_OFFSET, %rdx  // pass Thread::Current()
    call SYMBOL(artThrowClassCastExceptionForObject)  // (Object* src, Class* dest, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_check_instance_of


// Restore reg's value if reg is not the same as exclude_reg, otherwise just adjust stack.
MACRO2(POP_REG_NE, reg, exclude_reg)
    .ifc RAW_VAR(reg), RAW_VAR(exclude_reg)
      addq MACRO_LITERAL(8), %rsp
      CFI_ADJUST_CFA_OFFSET(-8)
    .else
      POP RAW_VAR(reg)
    .endif
END_MACRO

    /*
     * Macro to insert read barrier, used in art_quick_aput_obj.
     * obj_reg and dest_reg{32|64} are registers, offset is a defined literal such as
     * MIRROR_OBJECT_CLASS_OFFSET. dest_reg needs two versions to handle the mismatch between
     * 64b PUSH/POP and 32b argument.
     * TODO: When read barrier has a fast path, add heap unpoisoning support for the fast path.
     *
     * As with art_quick_aput_obj function, the 64b versions are in comments.
     */
MACRO4(READ_BARRIER, obj_reg, offset, dest_reg32, dest_reg64)
#ifdef USE_READ_BARRIER
    PUSH rax                            // save registers that might be used
    PUSH rdi
    PUSH rsi
    PUSH rdx
    PUSH rcx
    SETUP_FP_CALLEE_SAVE_FRAME
    // Outgoing argument set up
    // movl REG_VAR(ref_reg32), %edi    // pass ref, no-op for now since parameter ref is unused
    // // movq REG_VAR(ref_reg64), %rdi
    movl REG_VAR(obj_reg), %esi         // pass obj_reg
    // movq REG_VAR(obj_reg), %rsi
    movl MACRO_LITERAL((RAW_VAR(offset))), %edx // pass offset, double parentheses are necessary
    // movq MACRO_LITERAL((RAW_VAR(offset))), %rdx
    call SYMBOL(artReadBarrierSlow)     // artReadBarrierSlow(ref, obj_reg, offset)
    // No need to unpoison return value in rax, artReadBarrierSlow() would do the unpoisoning.
    .ifnc RAW_VAR(dest_reg32), eax
    // .ifnc RAW_VAR(dest_reg64), rax
      movl %eax, REG_VAR(dest_reg32)    // save loaded ref in dest_reg
      // movq %rax, REG_VAR(dest_reg64)
    .endif
    RESTORE_FP_CALLEE_SAVE_FRAME
    POP_REG_NE rcx, RAW_VAR(dest_reg64) // Restore registers except dest_reg
    POP_REG_NE rdx, RAW_VAR(dest_reg64)
    POP_REG_NE rsi, RAW_VAR(dest_reg64)
    POP_REG_NE rdi, RAW_VAR(dest_reg64)
    POP_REG_NE rax, RAW_VAR(dest_reg64)
#else
    movl RAW_VAR(offset)(REG_VAR(obj_reg)), REG_VAR(dest_reg32)
    // movq RAW_VAR(offset)(REG_VAR(obj_reg)), REG_VAR(dest_reg64)
    UNPOISON_HEAP_REF RAW_VAR(dest_reg32) // UNPOISON_HEAP_REF only takes a 32b register
#endif  // USE_READ_BARRIER
END_MACRO

DEFINE_FUNCTION art_quick_aput_obj
    testl %edx, %edx                // store of null
//  test %rdx, %rdx
    jz .Ldo_aput_null
    READ_BARRIER edi, MIRROR_OBJECT_CLASS_OFFSET, ecx, rcx
    // READ_BARRIER rdi, MIRROR_OBJECT_CLASS_OFFSET, ecx, rcx
    READ_BARRIER ecx, MIRROR_CLASS_COMPONENT_TYPE_OFFSET, ecx, rcx
    // READ_BARRIER rcx, MIRROR_CLASS_COMPONENT_TYPE_OFFSET, ecx, rcx
#if defined(USE_HEAP_POISONING) || defined(USE_READ_BARRIER)
    READ_BARRIER edx, MIRROR_OBJECT_CLASS_OFFSET, eax, rax  // rax is free.
    // READ_BARRIER rdx, MIRROR_OBJECT_CLASS_OFFSET, eax, rax
    cmpl %eax, %ecx  // value's type == array's component type - trivial assignability
#else
    cmpl MIRROR_OBJECT_CLASS_OFFSET(%edx), %ecx // value's type == array's component type - trivial assignability
//  cmpq MIRROR_CLASS_OFFSET(%rdx), %rcx
#endif
    jne .Lcheck_assignability
.Ldo_aput:
    POISON_HEAP_REF edx
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    movq %gs:THREAD_CARD_TABLE_OFFSET, %rdx
    shrl LITERAL(CARD_TABLE_CARD_SHIFT), %edi
//  shrl LITERAL(CARD_TABLE_CARD_SHIFT), %rdi
    movb %dl, (%rdx, %rdi)                       // Note: this assumes that top 32b of %rdi are zero
    ret
.Ldo_aput_null:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    ret
.Lcheck_assignability:
    // Save arguments.
    PUSH rdi
    PUSH rsi
    PUSH rdx
    SETUP_FP_CALLEE_SAVE_FRAME

#if defined(USE_HEAP_POISONING) || defined(USE_READ_BARRIER)
    // The load of MIRROR_OBJECT_CLASS_OFFSET(%edx) is redundant, eax still holds the value.
    movl %eax, %esi               // Pass arg2 = value's class.
    // movq %rax, %rsi
#else
                                     // "Uncompress" = do nothing, as already zero-extended on load.
    movl MIRROR_OBJECT_CLASS_OFFSET(%edx), %esi  // Pass arg2 = value's class.
#endif
    movq %rcx, %rdi               // Pass arg1 = array's component type.

    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b)

    // Exception?
    testq %rax, %rax
    jz   .Lthrow_array_store_exception

    RESTORE_FP_CALLEE_SAVE_FRAME
    // Restore arguments.
    POP  rdx
    POP  rsi
    POP  rdi

    POISON_HEAP_REF edx
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%edi, %esi, 4)
//  movq %rdx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%rdi, %rsi, 4)
    movq %gs:THREAD_CARD_TABLE_OFFSET, %rdx
    shrl LITERAL(CARD_TABLE_CARD_SHIFT), %edi
//  shrl LITERAL(CARD_TABLE_CARD_SHIFT), %rdi
    movb %dl, (%rdx, %rdi)                       // Note: this assumes that top 32b of %rdi are zero
//  movb %dl, (%rdx, %rdi)
    ret
    CFI_ADJUST_CFA_OFFSET(24 + 4 * 8)  // Reset unwind info so following code unwinds.
.Lthrow_array_store_exception:
    RESTORE_FP_CALLEE_SAVE_FRAME
    // Restore arguments.
    POP  rdx
    POP  rsi
    POP  rdi

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME  // Save all registers as basis for long jump context.

    // Outgoing argument set up.
    movq %rdx, %rsi                         // Pass arg 2 = value.
    movq %gs:THREAD_SELF_OFFSET, %rdx       // Pass arg 3 = Thread::Current().
                                            // Pass arg 1 = array.
    call SYMBOL(artThrowArrayStoreException) // (array, value, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_aput_obj

// TODO: This is quite silly on X86_64 now.
DEFINE_FUNCTION art_quick_memcpy
    call PLT_SYMBOL(memcpy)       // (void*, const void*, size_t)
    ret
END_FUNCTION art_quick_memcpy

DEFINE_FUNCTION art_quick_test_suspend
    SETUP_SAVE_EVERYTHING_FRAME RUNTIME_SAVE_EVERYTHING_FOR_SUSPEND_CHECK_METHOD_OFFSET  // save everything for GC
    // Outgoing argument set up
    movq %gs:THREAD_SELF_OFFSET, %rdi           // pass Thread::Current()
    call SYMBOL(artTestSuspendFromCode)         // (Thread*)
    RESTORE_SAVE_EVERYTHING_FRAME               // restore frame up to return address
    ret
END_FUNCTION art_quick_test_suspend

UNIMPLEMENTED art_quick_ldiv
UNIMPLEMENTED art_quick_lmod
UNIMPLEMENTED art_quick_lmul
UNIMPLEMENTED art_quick_lshl
UNIMPLEMENTED art_quick_lshr
UNIMPLEMENTED art_quick_lushr

// Note: Functions `art{Get,Set}<Kind>{Static,Instance}FromCompiledCode` are
// defined with a macro in runtime/entrypoints/quick/quick_field_entrypoints.cc.

THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set64_instance, artSet64InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCompiledCode, RETURN_IF_EAX_ZERO

TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set64_static, artSet64StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCompiledCode, RETURN_IF_EAX_ZERO

ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_RDI

    movq %gs:THREAD_SELF_OFFSET, %rdx       // Pass Thread::Current().
    movq %rsp, %rcx                         // Pass SP.
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    movq %rax, %xmm0                        // Copy return value in case of float returns.
    RETURN_OR_DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * rdi is the conflict ArtMethod.
     * rax is a hidden argument that holds the target interface method's dex method index.
     *
     * Note that this stub writes to r10 and rdi.
     */
DEFINE_FUNCTION art_quick_imt_conflict_trampoline
#if defined(__APPLE__)
    int3
    int3
#else
    movq __SIZEOF_POINTER__(%rsp), %r10 // Load referrer.
    // If the method is obsolete, just go through the dex cache miss slow path.
    // The obsolete flag is set with suspended threads, so we do not need an acquire operation here.
    testl LITERAL(ACC_OBSOLETE_METHOD), ART_METHOD_ACCESS_FLAGS_OFFSET(%r10)
    jnz .Limt_conflict_trampoline_dex_cache_miss
    movl ART_METHOD_DECLARING_CLASS_OFFSET(%r10), %r10d  // Load declaring class (no read barrier).
    movl MIRROR_CLASS_DEX_CACHE_OFFSET(%r10), %r10d    // Load the DexCache (without read barrier).
    UNPOISON_HEAP_REF r10d
    movq MIRROR_DEX_CACHE_RESOLVED_METHODS_OFFSET(%r10), %r10  // Load the resolved methods.
    mov %eax, %r11d  // Remember method index in R11.
    andl LITERAL(METHOD_DEX_CACHE_SIZE_MINUS_ONE), %eax  // Calculate DexCache method slot index.
    shll LITERAL(1), %eax       // Multiply by 2 as entries have size 2 * __SIZEOF_POINTER__.
    leaq 0(%r10, %rax, __SIZEOF_POINTER__), %r10 // Load DexCache method slot address.
    PUSH rdx                    // Preserve RDX as we need to clobber it by LOCK CMPXCHG16B.
    mov %rcx, %rdx              // Make RDX:RAX == RCX:RBX so that LOCK CMPXCHG16B makes no changes.
    mov %rbx, %rax              // (The actual value does not matter.)
    lock cmpxchg16b (%r10)      // Relaxed atomic load RDX:RAX from the dex cache slot.
    movq ART_METHOD_JNI_OFFSET_64(%rdi), %rdi  // Load ImtConflictTable
    cmp %rdx, %r11              // Compare method index to see if we had a DexCache method hit.
    jne .Limt_conflict_trampoline_dex_cache_miss
.Limt_table_iterate:
    cmpq %rax, 0(%rdi)
    jne .Limt_table_next_entry
    // We successfully hit an entry in the table. Load the target method
    // and jump to it.
    movq __SIZEOF_POINTER__(%rdi), %rdi
    CFI_REMEMBER_STATE
    POP rdx
    jmp *ART_METHOD_QUICK_CODE_OFFSET_64(%rdi)
    CFI_RESTORE_STATE
.Limt_table_next_entry:
    // If the entry is null, the interface method is not in the ImtConflictTable.
    cmpq LITERAL(0), 0(%rdi)
    jz .Lconflict_trampoline
    // Iterate over the entries of the ImtConflictTable.
    addq LITERAL(2 * __SIZEOF_POINTER__), %rdi
    jmp .Limt_table_iterate
.Lconflict_trampoline:
    // Call the runtime stub to populate the ImtConflictTable and jump to the
    // resolved method.
    CFI_REMEMBER_STATE
    POP rdx
    movq %rax, %rdi  // Load interface method
    INVOKE_TRAMPOLINE_BODY artInvokeInterfaceTrampoline
    CFI_RESTORE_STATE
.Limt_conflict_trampoline_dex_cache_miss:
    // We're not creating a proper runtime method frame here,
    // artLookupResolvedMethod() is not allowed to walk the stack.

    // Save GPR args and ImtConflictTable; RDX is already saved.
    PUSH r9   // Quick arg 5.
    PUSH r8   // Quick arg 4.
    PUSH rsi  // Quick arg 1.
    PUSH rcx  // Quick arg 3.
    PUSH rdi  // ImtConflictTable
    // Save FPR args and callee-saves, align stack to 16B.
    subq MACRO_LITERAL(12 * 8 + 8), %rsp
    CFI_ADJUST_CFA_OFFSET(12 * 8 + 8)
    movq %xmm0, 0(%rsp)
    movq %xmm1, 8(%rsp)
    movq %xmm2, 16(%rsp)
    movq %xmm3, 24(%rsp)
    movq %xmm4, 32(%rsp)
    movq %xmm5, 40(%rsp)
    movq %xmm6, 48(%rsp)
    movq %xmm7, 56(%rsp)
    movq %xmm12, 64(%rsp)  // XMM12-15 are callee-save in ART compiled code ABI
    movq %xmm13, 72(%rsp)  // but caller-save in native ABI.
    movq %xmm14, 80(%rsp)
    movq %xmm15, 88(%rsp)

    movq %r11, %rdi             // Pass method index.
    movq 12 * 8 + 8 + 6 * 8 + 8(%rsp), %rsi   // Pass referrer.
    call SYMBOL(artLookupResolvedMethod)  // (uint32_t method_index, ArtMethod* referrer)

    // Restore FPRs.
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    movq 64(%rsp), %xmm12
    movq 72(%rsp), %xmm13
    movq 80(%rsp), %xmm14
    movq 88(%rsp), %xmm15
    addq MACRO_LITERAL(12 * 8 + 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(12 * 8 + 8))
    // Restore ImtConflictTable and GPR args.
    POP rdi
    POP rcx
    POP rsi
    POP r8
    POP r9

    cmp LITERAL(0), %rax        // If the method wasn't resolved,
    je .Lconflict_trampoline    //   skip the lookup and go to artInvokeInterfaceTrampoline().
    jmp .Limt_table_iterate
#endif  // __APPLE__
END_FUNCTION art_quick_imt_conflict_trampoline

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    movq %gs:THREAD_SELF_OFFSET, %rdx
    movq %rsp, %rcx
    call SYMBOL(artQuickResolutionTrampoline) // (called, receiver, Thread*, SP)
    movq %rax, %r10               // Remember returned code pointer in R10.
    movq (%rsp), %rdi             // Load called method into RDI.
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    testq %r10, %r10              // If code pointer is null goto deliver pending exception.
    jz 1f
    jmp *%r10                     // Tail call into method.
1:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

/* Generic JNI frame layout:
 *
 * #-------------------#
 * |                   |
 * | caller method...  |
 * #-------------------#    <--- SP on entry
 *
 *          |
 *          V
 *
 * #-------------------#
 * | caller method...  |
 * #-------------------#
 * | Return            |
 * | R15               |    callee save
 * | R14               |    callee save
 * | R13               |    callee save
 * | R12               |    callee save
 * | R9                |    arg5
 * | R8                |    arg4
 * | RSI/R6            |    arg1
 * | RBP/R5            |    callee save
 * | RBX/R3            |    callee save
 * | RDX/R2            |    arg2
 * | RCX/R1            |    arg3
 * | XMM7              |    float arg 8
 * | XMM6              |    float arg 7
 * | XMM5              |    float arg 6
 * | XMM4              |    float arg 5
 * | XMM3              |    float arg 4
 * | XMM2              |    float arg 3
 * | XMM1              |    float arg 2
 * | XMM0              |    float arg 1
 * | RDI/Method*       |  <- sp
 * #-------------------#
 * | Scratch Alloca    |    5K scratch space
 * #---------#---------#
 * |         | sp*     |
 * | Tramp.  #---------#
 * | args    | thread  |
 * | Tramp.  #---------#
 * |         | method  |
 * #-------------------#    <--- SP on artQuickGenericJniTrampoline
 *
 *           |
 *           v              artQuickGenericJniTrampoline
 *
 * #-------------------#
 * | caller method...  |
 * #-------------------#
 * | Return            |
 * | Callee-Save Data  |
 * #-------------------#
 * | handle scope      |
 * #-------------------#
 * | Method*           |    <--- (1)
 * #-------------------#
 * | local ref cookie  | // 4B
 * | handle scope size | // 4B   TODO: roll into call stack alignment?
 * #-------------------#
 * | JNI Call Stack    |
 * #-------------------#    <--- SP on native call
 * |                   |
 * | Stack for Regs    |    The trampoline assembly will pop these values
 * |                   |    into registers for native call
 * #-------------------#
 * | Native code ptr   |
 * #-------------------#
 * | Free scratch      |
 * #-------------------#
 * | Ptr to (1)        |    <--- RSP
 * #-------------------#
 */
    /*
     * Called to do a generic JNI down-call
     */
DEFINE_FUNCTION art_quick_generic_jni_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_RDI

    movq %rsp, %rbp                 // save SP at (old) callee-save frame
    CFI_DEF_CFA_REGISTER(rbp)

    //
    // reserve a lot of space
    //
    //      4    local state ref
    //      4    padding
    //   4196    4k scratch space, enough for 2x 256 8-byte parameters (TODO: handle scope overhead?)
    //     16    handle scope member fields ?
    // +  112    14x 8-byte stack-2-register space
    // ------
    //   4332
    // 16-byte aligned: 4336
    // Note: 14x8 = 7*16, so the stack stays aligned for the native call...
    //       Also means: the padding is somewhere in the middle
    //
    //
    // New test: use 5K and release
    // 5k = 5120
    subq LITERAL(5120), %rsp
    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //    rdi    rsi      <= C calling convention
    //  gs:...   rbp      <= where they are
    movq %gs:THREAD_SELF_OFFSET, %rdi
    movq %rbp, %rsi
    call SYMBOL(artQuickGenericJniTrampoline)  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // %rax: pointer to native code, 0 on error.
    // %rdx: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    test %rax, %rax
    jz .Lexception_in_native

    // Release part of the alloca.
    movq %rdx, %rsp

    // pop from the register-passing alloca region
    // what's the right layout?
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    // TODO: skip floating point if unused, some flag.
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    addq LITERAL(64), %rsp          // floating-point done

    // native call
    call *%rax

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*,  result, result_f)
    //   rdi      rsi   rdx       <= C calling convention
    //  gs:...    rax   xmm0      <= where they are
    movq %gs:THREAD_SELF_OFFSET, %rdi
    movq %rax, %rsi
    movq %xmm0, %rdx
    call SYMBOL(artQuickGenericJniEndTrampoline)

    // Pending exceptions possible.
    // TODO: use cmpq, needs direct encoding because of gas bug
    movq %gs:THREAD_EXCEPTION_OFFSET, %rcx
    test %rcx, %rcx
    jnz .Lexception_in_native

    // Tear down the alloca.
    movq %rbp, %rsp
    CFI_DEF_CFA_REGISTER(rsp)

    // Tear down the callee-save frame.
    // Load FPRs.
    // movq %xmm0, 16(%rsp)         // doesn't make sense!!!
    movq 24(%rsp), %xmm1            // neither does this!!!
    movq 32(%rsp), %xmm2
    movq 40(%rsp), %xmm3
    movq 48(%rsp), %xmm4
    movq 56(%rsp), %xmm5
    movq 64(%rsp), %xmm6
    movq 72(%rsp), %xmm7
    movq 80(%rsp), %xmm12
    movq 88(%rsp), %xmm13
    movq 96(%rsp), %xmm14
    movq 104(%rsp), %xmm15
    // was 80 bytes
    addq LITERAL(80 + 4*8), %rsp
    CFI_ADJUST_CFA_OFFSET(-80 - 4*8)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    POP rcx  // Arg.
    POP rdx  // Arg.
    POP rbx  // Callee save.
    POP rbp  // Callee save.
    POP rsi  // Arg.
    POP r8   // Arg.
    POP r9   // Arg.
    POP r12  // Callee save.
    POP r13  // Callee save.
    POP r14  // Callee save.
    POP r15  // Callee save.
    // store into fpr, for when it's a fpr return...
    movq %rax, %xmm0
    ret
.Lexception_in_native:
    pushq %gs:THREAD_TOP_QUICK_FRAME_OFFSET
    addq LITERAL(-1), (%rsp)  // Remove the GenericJNI tag.
    movq (%rsp), %rsp
    CFI_DEF_CFA_REGISTER(rsp)
    // Do a call to push a new save-all frame required by the runtime.
    call .Lexception_call
.Lexception_call:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_generic_jni_trampoline

    /*
     * Called to bridge from the quick to interpreter ABI. On entry the arguments match those
     * of a quick call:
     * RDI = method being called / to bridge to.
     * RSI, RDX, RCX, R8, R9 are arguments to that method.
     */
DEFINE_FUNCTION art_quick_to_interpreter_bridge
    SETUP_SAVE_REFS_AND_ARGS_FRAME     // Set up frame and save arguments.
    movq %gs:THREAD_SELF_OFFSET, %rsi  // RSI := Thread::Current()
    movq %rsp, %rdx                    // RDX := sp
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME   // TODO: no need to restore arguments in this case.
    movq %rax, %xmm0                   // Place return value also into floating point return value.
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Called to catch an attempt to invoke an obsolete method.
     * RDI = method being called.
     */
ONE_ARG_RUNTIME_EXCEPTION art_invoke_obsolete_method_stub, artInvokeObsoleteMethod

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
#if defined(__APPLE__)
    int3
    int3
#else
    SETUP_SAVE_REFS_AND_ARGS_FRAME

    movq %rdi, %r12               // Preserve method pointer in a callee-save.

    movq %gs:THREAD_SELF_OFFSET, %rdx   // Pass thread.
    movq %rsp, %rcx                     // Pass SP.

    call SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, SP)

                                  // %rax = result of call.
    testq %rax, %rax
    jz 1f

    movq %r12, %rdi               // Reload method pointer.
    leaq art_quick_instrumentation_exit(%rip), %r12   // Set up return through instrumentation
    movq %r12, FRAME_SIZE_SAVE_REFS_AND_ARGS-8(%rsp)  // exit.

    RESTORE_SAVE_REFS_AND_ARGS_FRAME

    jmp *%rax                     // Tail call to intended method.
1:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
#endif  // __APPLE__
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION_CUSTOM_CFA art_quick_instrumentation_exit, 0
    pushq LITERAL(0)          // Push a fake return PC as there will be none on the stack.
    CFI_ADJUST_CFA_OFFSET(8)

    SETUP_SAVE_EVERYTHING_FRAME

    leaq 16(%rsp), %rcx       // Pass floating-point result pointer, in kSaveEverything frame.
    leaq 144(%rsp), %rdx      // Pass integer result pointer, in kSaveEverything frame.
    movq %rsp, %rsi           // Pass SP.
    movq %gs:THREAD_SELF_OFFSET, %rdi  // Pass Thread.

    call SYMBOL(artInstrumentationMethodExitFromCode)   // (Thread*, SP, gpr_res*, fpr_res*)

    testq %rax, %rax          // Check if we have a return-pc to go to. If we don't then there was
                              // an exception
    jz .Ldo_deliver_instrumentation_exception
    testq %rdx, %rdx
    jnz .Ldeoptimize
    // Normal return.
    movq %rax, FRAME_SIZE_SAVE_EVERYTHING-8(%rsp)  // Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    ret
.Ldeoptimize:
    movq %rdx, FRAME_SIZE_SAVE_EVERYTHING-8(%rsp)  // Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    // Jump to art_quick_deoptimize.
    jmp SYMBOL(art_quick_deoptimize)
.Ldo_deliver_instrumentation_exception:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    SETUP_SAVE_EVERYTHING_FRAME        // Stack should be aligned now.
    movq %gs:THREAD_SELF_OFFSET, %rdi  // Pass Thread.
    call SYMBOL(artDeoptimize)         // (Thread*)
    UNREACHABLE
END_FUNCTION art_quick_deoptimize

    /*
     * Compiled code has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the interpreter bridge.
     */
DEFINE_FUNCTION art_quick_deoptimize_from_compiled_code
    SETUP_SAVE_EVERYTHING_FRAME
                                                // Stack should be aligned now.
    movq %gs:THREAD_SELF_OFFSET, %rsi           // Pass Thread.
    call SYMBOL(artDeoptimizeFromCompiledCode)  // (DeoptimizationKind, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_deoptimize_from_compiled_code

    /*
     * String's compareTo.
     *
     * On entry:
     *    rdi:   this string object (known non-null)
     *    rsi:   comp string object (known non-null)
     */
DEFINE_FUNCTION art_quick_string_compareto
    movl MIRROR_STRING_COUNT_OFFSET(%edi), %r8d
    movl MIRROR_STRING_COUNT_OFFSET(%esi), %r9d
    /* Build pointers to the start of string data */
    leal MIRROR_STRING_VALUE_OFFSET(%edi), %edi
    leal MIRROR_STRING_VALUE_OFFSET(%esi), %esi
#if (STRING_COMPRESSION_FEATURE)
    /* Differ cases */
    shrl    LITERAL(1), %r8d
    jnc     .Lstring_compareto_this_is_compressed
    shrl    LITERAL(1), %r9d
    jnc     .Lstring_compareto_that_is_compressed
    jmp     .Lstring_compareto_both_not_compressed
.Lstring_compareto_this_is_compressed:
    shrl    LITERAL(1), %r9d
    jnc     .Lstring_compareto_both_compressed
    /* Comparison this (8-bit) and that (16-bit) */
    mov     %r8d, %eax
    subl    %r9d, %eax
    mov     %r8d, %ecx
    cmovg   %r9d, %ecx
    /* Going into loop to compare each character */
    jecxz   .Lstring_compareto_keep_length1     // check loop counter (if 0 then stop)
.Lstring_compareto_loop_comparison_this_compressed:
    movzbl  (%edi), %r8d                        // move *(this_cur_char) byte to long
    movzwl  (%esi), %r9d                        // move *(that_cur_char) word to long
    addl    LITERAL(1), %edi                    // ++this_cur_char (8-bit)
    addl    LITERAL(2), %esi                    // ++that_cur_char (16-bit)
    subl    %r9d, %r8d
    loope   .Lstring_compareto_loop_comparison_this_compressed
    cmovne  %r8d, %eax                          // return eax = *(this_cur_char) - *(that_cur_char)
.Lstring_compareto_keep_length1:
    ret
.Lstring_compareto_that_is_compressed:
    movl    %r8d, %eax
    subl    %r9d, %eax
    mov     %r8d, %ecx
    cmovg   %r9d, %ecx
    /* Comparison this (8-bit) and that (16-bit) */
    jecxz   .Lstring_compareto_keep_length2     // check loop counter (if 0, don't compare)
.Lstring_compareto_loop_comparison_that_compressed:
    movzwl  (%edi), %r8d                        // move *(this_cur_char) word to long
    movzbl  (%esi), %r9d                        // move *(that_cur_chat) byte to long
    addl    LITERAL(2), %edi                    // ++this_cur_char (16-bit)
    addl    LITERAL(1), %esi                    // ++that_cur_char (8-bit)
    subl    %r9d, %r8d
    loope   .Lstring_compareto_loop_comparison_that_compressed
    cmovne  %r8d, %eax                          // return eax = *(this_cur_char) - *(that_cur_char)
.Lstring_compareto_keep_length2:
    ret
.Lstring_compareto_both_compressed:
    /* Calculate min length and count diff */
    movl    %r8d, %ecx
    movl    %r8d, %eax
    subl    %r9d, %eax
    cmovg   %r9d, %ecx
    jecxz   .Lstring_compareto_keep_length3
    repe    cmpsb
    je      .Lstring_compareto_keep_length3
    movzbl  -1(%edi), %eax        // get last compared char from this string (8-bit)
    movzbl  -1(%esi), %ecx        // get last compared char from comp string (8-bit)
    jmp     .Lstring_compareto_count_difference
#endif // STRING_COMPRESSION_FEATURE
.Lstring_compareto_both_not_compressed:
    /* Calculate min length and count diff */
    movl    %r8d, %ecx
    movl    %r8d, %eax
    subl    %r9d, %eax
    cmovg   %r9d, %ecx
    /*
     * At this point we have:
     *   eax: value to return if first part of strings are equal
     *   ecx: minimum among the lengths of the two strings
     *   esi: pointer to comp string data
     *   edi: pointer to this string data
     */
    jecxz .Lstring_compareto_keep_length3
    repe  cmpsw                   // find nonmatching chars in [%esi] and [%edi], up to length %ecx
    je    .Lstring_compareto_keep_length3
    movzwl  -2(%edi), %eax        // get last compared char from this string (16-bit)
    movzwl  -2(%esi), %ecx        // get last compared char from comp string (16-bit)
.Lstring_compareto_count_difference:
    subl  %ecx, %eax              // return the difference
.Lstring_compareto_keep_length3:
    ret
END_FUNCTION art_quick_string_compareto

UNIMPLEMENTED art_quick_memcmp16

DEFINE_FUNCTION art_quick_instance_of
    SETUP_FP_CALLEE_SAVE_FRAME
    subq LITERAL(8), %rsp                      // Alignment padding.
    CFI_ADJUST_CFA_OFFSET(8)
    call SYMBOL(artInstanceOfFromCode)         // (mirror::Object*, mirror::Class*)
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    RESTORE_FP_CALLEE_SAVE_FRAME
    ret
END_FUNCTION art_quick_instance_of

// Create a function `name` calling the ReadBarrier::Mark routine,
// getting its argument and returning its result through register
// `reg`, saving and restoring all caller-save registers.
//
// The generated function follows a non-standard runtime calling
// convention:
// - register `reg` (which may be different from RDI) is used to pass
//   the (sole) argument of this function;
// - register `reg` (which may be different from RAX) is used to return
//   the result of this function (instead of RAX);
// - if `reg` is different from `rdi`, RDI is treated like a normal
//   (non-argument) caller-save register;
// - if `reg` is different from `rax`, RAX is treated like a normal
//   (non-result) caller-save register;
// - everything else is the same as in the standard runtime calling
//   convention (e.g. standard callee-save registers are preserved).
MACRO2(READ_BARRIER_MARK_REG, name, reg)
    DEFINE_FUNCTION VAR(name)
    // Null check so that we can load the lock word.
    testq REG_VAR(reg), REG_VAR(reg)
    jz .Lret_rb_\name
.Lnot_null_\name:
    // Check the mark bit, if it is 1 return.
    testl LITERAL(LOCK_WORD_MARK_BIT_MASK_SHIFTED), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(reg))
    jz .Lslow_rb_\name
    ret
.Lslow_rb_\name:
    PUSH rax
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(reg)), %eax
    addl LITERAL(LOCK_WORD_STATE_FORWARDING_ADDRESS_OVERFLOW), %eax
    // Jump if the addl caused eax to unsigned overflow. The only case where it overflows is the
    // forwarding address one.
    // Taken ~25% of the time.
    jnae .Lret_forwarding_address\name

    // Save all potentially live caller-save core registers.
    movq 0(%rsp), %rax
    PUSH rcx
    PUSH rdx
    PUSH rsi
    PUSH rdi
    PUSH r8
    PUSH r9
    PUSH r10
    PUSH r11
    // Create space for caller-save floating-point registers.
    subq MACRO_LITERAL(12 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(12 * 8)
    // Save all potentially live caller-save floating-point registers.
    movq %xmm0, 0(%rsp)
    movq %xmm1, 8(%rsp)
    movq %xmm2, 16(%rsp)
    movq %xmm3, 24(%rsp)
    movq %xmm4, 32(%rsp)
    movq %xmm5, 40(%rsp)
    movq %xmm6, 48(%rsp)
    movq %xmm7, 56(%rsp)
    movq %xmm8, 64(%rsp)
    movq %xmm9, 72(%rsp)
    movq %xmm10, 80(%rsp)
    movq %xmm11, 88(%rsp)
    SETUP_FP_CALLEE_SAVE_FRAME

    .ifnc RAW_VAR(reg), rdi
      movq REG_VAR(reg), %rdi       // Pass arg1 - obj from `reg`.
    .endif
    call SYMBOL(artReadBarrierMark) // artReadBarrierMark(obj)
    .ifnc RAW_VAR(reg), rax
      movq %rax, REG_VAR(reg)       // Return result into `reg`.
    .endif

    RESTORE_FP_CALLEE_SAVE_FRAME
    // Restore floating-point registers.
    movq 0(%rsp), %xmm0
    movq 8(%rsp), %xmm1
    movq 16(%rsp), %xmm2
    movq 24(%rsp), %xmm3
    movq 32(%rsp), %xmm4
    movq 40(%rsp), %xmm5
    movq 48(%rsp), %xmm6
    movq 56(%rsp), %xmm7
    movq 64(%rsp), %xmm8
    movq 72(%rsp), %xmm9
    movq 80(%rsp), %xmm10
    movq 88(%rsp), %xmm11
    // Remove floating-point registers.
    addq MACRO_LITERAL(12 * 8), %rsp
    CFI_ADJUST_CFA_OFFSET(-(12 * 8))
    // Restore core regs, except `reg`, as it is used to return the
    // result of this function (simply remove it from the stack instead).
    POP_REG_NE r11, RAW_VAR(reg)
    POP_REG_NE r10, RAW_VAR(reg)
    POP_REG_NE r9, RAW_VAR(reg)
    POP_REG_NE r8, RAW_VAR(reg)
    POP_REG_NE rdi, RAW_VAR(reg)
    POP_REG_NE rsi, RAW_VAR(reg)
    POP_REG_NE rdx, RAW_VAR(reg)
    POP_REG_NE rcx, RAW_VAR(reg)
    POP_REG_NE rax, RAW_VAR(reg)
.Lret_rb_\name:
    ret
.Lret_forwarding_address\name:
    // The overflow cleared the top bits.
    sall LITERAL(LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT), %eax
    movq %rax, REG_VAR(reg)
    POP_REG_NE rax, RAW_VAR(reg)
    ret
    END_FUNCTION VAR(name)
END_MACRO

READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg00, rax
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg01, rcx
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg02, rdx
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg03, rbx
// Note: There is no art_quick_read_barrier_mark_reg04, as register 4 (RSP)
// cannot be used to pass arguments.
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg05, rbp
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg06, rsi
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg07, rdi
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg08, r8
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg09, r9
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg10, r10
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg11, r11
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg12, r12
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg13, r13
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg14, r14
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg15, r15

DEFINE_FUNCTION art_quick_read_barrier_slow
    SETUP_FP_CALLEE_SAVE_FRAME
    subq LITERAL(8), %rsp           // Alignment padding.
    CFI_ADJUST_CFA_OFFSET(8)
    call SYMBOL(artReadBarrierSlow) // artReadBarrierSlow(ref, obj, offset)
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    RESTORE_FP_CALLEE_SAVE_FRAME
    ret
END_FUNCTION art_quick_read_barrier_slow

DEFINE_FUNCTION art_quick_read_barrier_for_root_slow
    SETUP_FP_CALLEE_SAVE_FRAME
    subq LITERAL(8), %rsp                  // Alignment padding.
    CFI_ADJUST_CFA_OFFSET(8)
    call SYMBOL(artReadBarrierForRootSlow) // artReadBarrierForRootSlow(root)
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    RESTORE_FP_CALLEE_SAVE_FRAME
    ret
END_FUNCTION art_quick_read_barrier_for_root_slow

    /*
     * On stack replacement stub.
     * On entry:
     *   [sp] = return address
     *   rdi = stack to copy
     *   rsi = size of stack
     *   rdx = pc to call
     *   rcx = JValue* result
     *   r8 = shorty
     *   r9 = thread
     *
     * Note that the native C ABI already aligned the stack to 16-byte.
     */
DEFINE_FUNCTION art_quick_osr_stub
    // Save the non-volatiles.
    PUSH rbp                      // Save rbp.
    PUSH rcx                      // Save rcx/result*.
    PUSH r8                       // Save r8/shorty*.

    // Save callee saves.
    PUSH rbx
    PUSH r12
    PUSH r13
    PUSH r14
    PUSH r15

    pushq LITERAL(0)              // Push null for ArtMethod*.
    CFI_ADJUST_CFA_OFFSET(8)
    movl %esi, %ecx               // rcx := size of stack
    movq %rdi, %rsi               // rsi := stack to copy
    movq %rsp, %rbp               // Save stack pointer to RBP for CFI use in .Losr_entry.
    call .Losr_entry
    CFI_REMEMBER_STATE

    // Restore stack and callee-saves.
    addq LITERAL(8), %rsp
    CFI_ADJUST_CFA_OFFSET(-8)
    POP r15
    POP r14
    POP r13
    POP r12
    POP rbx
    POP r8
    POP rcx
    POP rbp
    cmpb LITERAL(68), (%r8)        // Test if result type char == 'D'.
    je .Losr_return_double_quick
    cmpb LITERAL(70), (%r8)        // Test if result type char == 'F'.
    je .Losr_return_float_quick
    movq %rax, (%rcx)              // Store the result assuming its a long, int or Object*
    ret
.Losr_return_double_quick:
    movsd %xmm0, (%rcx)            // Store the double floating point result.
    ret
.Losr_return_float_quick:
    movss %xmm0, (%rcx)            // Store the floating point result.
    ret
.Losr_entry:
    CFI_RESTORE_STATE             // Restore CFI state; however, since the call has pushed the
    CFI_DEF_CFA_REGISTER(rbp)     // return address we need to switch the CFA register to RBP.

    subl LITERAL(8), %ecx         // Given stack size contains pushed frame pointer, substract it.
    subq %rcx, %rsp
    movq %rsp, %rdi               // rdi := beginning of stack
    rep movsb                     // while (rcx--) { *rdi++ = *rsi++ }
    jmp *%rdx
END_FUNCTION art_quick_osr_stub

DEFINE_FUNCTION art_quick_invoke_polymorphic
    SETUP_SAVE_REFS_AND_ARGS_FRAME                 // save callee saves
    movq %gs:THREAD_SELF_OFFSET, %rdx              // pass Thread
    movq %rsp, %rcx                                // pass SP
    subq LITERAL(16), %rsp                         // make space for JValue result
    CFI_ADJUST_CFA_OFFSET(16)
    movq LITERAL(0), (%rsp)                        // initialize result
    movq %rsp, %rdi                                // store pointer to JValue result
    call SYMBOL(artInvokePolymorphic)              // artInvokePolymorphic(result, receiver, Thread*, SP)
                                                   // save the code pointer
    subq LITERAL('A'), %rax                        // Convert type descriptor character value to a zero based index.
    cmpb LITERAL('Z' - 'A'), %al                   // Eliminate out of bounds options
    ja .Lcleanup_and_return
    movzbq %al, %rax
    leaq .Lhandler_table(%rip), %rcx               // Get the address of the handler table
    movslq (%rcx, %rax, 4), %rax                   // Lookup handler offset relative to table
    addq %rcx, %rax                                // Add table address to yield handler address.
    jmpq *%rax                                     // Jump to handler.

.align 4
.Lhandler_table:                                   // Table of type descriptor to handlers.
MACRO1(HANDLER_TABLE_OFFSET, handle_label)
    // NB some tools require 32-bits for relocations. Shouldn't need adjusting.
    .long RAW_VAR(handle_label) - .Lhandler_table
END_MACRO
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // A
    HANDLER_TABLE_OFFSET(.Lstore_long_result)      // B (byte)
    HANDLER_TABLE_OFFSET(.Lstore_char_result)      // C (char)
    HANDLER_TABLE_OFFSET(.Lstore_double_result)    // D (double)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // E
    HANDLER_TABLE_OFFSET(.Lstore_float_result)     // F (float)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // G
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // H
    HANDLER_TABLE_OFFSET(.Lstore_long_result)      // I (int)
    HANDLER_TABLE_OFFSET(.Lstore_long_result)      // J (long)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // K
    HANDLER_TABLE_OFFSET(.Lstore_long_result)      // L (object - references are compressed and only 32-bits)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // M
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // N
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // O
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // P
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // Q
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // R
    HANDLER_TABLE_OFFSET(.Lstore_long_result)      // S (short)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // T
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // U
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // V (void)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // W
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // X
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)     // Y
    HANDLER_TABLE_OFFSET(.Lstore_boolean_result)   // Z (boolean)

.Lstore_boolean_result:
    movzbq (%rsp), %rax                            // Copy boolean result to the accumulator
    jmp .Lcleanup_and_return
.Lstore_char_result:
    movzwq (%rsp), %rax                            // Copy char result to the accumulator
    jmp .Lcleanup_and_return
.Lstore_float_result:
    movd (%rsp), %xmm0                             // Copy float result to the context restored by
    movd %xmm0, 32(%rsp)                           // RESTORE_SAVE_REFS_AND_ARGS_FRAME.
    jmp .Lcleanup_and_return
.Lstore_double_result:
    movsd (%rsp), %xmm0                            // Copy double result to the context restored by
    movsd %xmm0, 32(%rsp)                          // RESTORE_SAVE_REFS_AND_ARGS_FRAME.
    jmp .Lcleanup_and_return
.Lstore_long_result:
    movq (%rsp), %rax                              // Copy long result to the accumulator.
     // Fall-through
.Lcleanup_and_return:
    addq LITERAL(16), %rsp                         // Pop space for JValue result.
    CFI_ADJUST_CFA_OFFSET(16)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    RETURN_OR_DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_invoke_polymorphic

// Wrap ExecuteSwitchImpl in assembly method which specifies DEX PC for unwinding.
//  Argument 0: RDI: The context pointer for ExecuteSwitchImpl.
//  Argument 1: RSI: Pointer to the templated ExecuteSwitchImpl to call.
//  Argument 2: RDX: The value of DEX PC (memory address of the methods bytecode).
DEFINE_FUNCTION ExecuteSwitchImplAsm
    PUSH rbx                 // Spill RBX
    movq %rdx, %rbx          // RBX = DEX PC (callee save register)
    CFI_DEFINE_DEX_PC_WITH_OFFSET(0 /* RAX */, 3 /* RBX */, 0)

    call *%rsi               // Call the wrapped function

    POP rbx                  // Restore RBX
    ret
END_FUNCTION ExecuteSwitchImplAsm
