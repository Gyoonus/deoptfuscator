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

#include "asm_support_x86.S"
#include "interpreter/cfi_asm_support.h"

#include "arch/quick_alloc_entrypoints.S"

// For x86, the CFA is esp+4, the address above the pushed return address on the stack.

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     */
MACRO2(SETUP_SAVE_ALL_CALLEE_SAVES_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)), REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 3*4 + 16 + 4)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly)
     */
MACRO2(SETUP_SAVE_REFS_ONLY_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl MACRO_LITERAL(12), %esp  // Grow stack by 3 words.
    CFI_ADJUST_CFA_OFFSET(12)
    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)), REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 3*4 + 16 + 4)
#error "FRAME_SIZE_SAVE_REFS_ONLY(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly)
     * and preserves the value of got_reg at entry.
     */
MACRO2(SETUP_SAVE_REFS_ONLY_FRAME_PRESERVE_GOT_REG, got_reg, temp_reg)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    PUSH RAW_VAR(got_reg)  // Save got_reg
    subl MACRO_LITERAL(8), %esp  // Grow stack by 2 words.
    CFI_ADJUST_CFA_OFFSET(8)

    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)), REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the top quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    // Restore got_reg.
    movl 12(%esp), REG_VAR(got_reg)
    CFI_RESTORE(RAW_VAR(got_reg))

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 3*4 + 16 + 4)
#error "FRAME_SIZE_SAVE_REFS_ONLY(X86) size not as expected."
#endif
END_MACRO

MACRO0(RESTORE_SAVE_REFS_ONLY_FRAME)
    addl MACRO_LITERAL(16), %esp  // Unwind stack up to saved values
    CFI_ADJUST_CFA_OFFSET(-16)
    POP ebp  // Restore callee saves (ebx is saved/restored by the upcall)
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs)
     */
MACRO2(SETUP_SAVE_REFS_AND_ARGS_FRAME, got_reg, temp_reg)
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx
    // Create space for FPR args.
    subl MACRO_LITERAL(4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(4 * 8)
    // Save FPRs.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)), REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    // Push save all callee-save method.
    pushl RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the stop quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 7*4 + 4*8 + 4)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs) where the method is passed in EAX.
     */
MACRO0(SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_EAX)
    // Save callee and GPR args, mixed together to agree with core spills bitmap.
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx

    // Create space for FPR args.
    subl MACRO_LITERAL(32), %esp
    CFI_ADJUST_CFA_OFFSET(32)

    // Save FPRs.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    PUSH eax  // Store the ArtMethod reference at the bottom of the stack.
    // Store esp as the stop quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET
END_MACRO

MACRO0(RESTORE_SAVE_REFS_AND_ARGS_FRAME)
    // Restore FPRs. EAX is still on the stack.
    movsd 4(%esp), %xmm0
    movsd 12(%esp), %xmm1
    movsd 20(%esp), %xmm2
    movsd 28(%esp), %xmm3

    addl MACRO_LITERAL(36), %esp  // Remove FPRs and EAX.
    CFI_ADJUST_CFA_OFFSET(-36)

    POP ecx                       // Restore args except eax
    POP edx
    POP ebx
    POP ebp                       // Restore callee saves
    POP esi
    POP edi
END_MACRO

// Restore register and jump to routine
// Inputs:  EDI contains pointer to code.
// Notes: Need to pop EAX too (restores Method*)
MACRO0(RESTORE_SAVE_REFS_AND_ARGS_FRAME_AND_JUMP)
    POP eax  // Restore Method*

    // Restore FPRs.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3

    addl MACRO_LITERAL(32), %esp  // Remove FPRs.
    CFI_ADJUST_CFA_OFFSET(-32)

    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    xchgl 0(%esp),%edi // restore EDI and place code pointer as only value on stack
    ret
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when EDI and ESI are already saved.
     */
MACRO3(SETUP_SAVE_EVERYTHING_FRAME_EDI_ESI_SAVED, got_reg, temp_reg, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    // Save core registers from highest to lowest to agree with core spills bitmap.
    // EDI and ESI, or at least placeholders for them, are already on the stack.
    PUSH ebp
    PUSH ebx
    PUSH edx
    PUSH ecx
    PUSH eax
    // Create space for FPR registers and stack alignment padding.
    subl MACRO_LITERAL(12 + 8 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(12 + 8 * 8)
    // Save FPRs.
    movsd %xmm0, 12(%esp)
    movsd %xmm1, 20(%esp)
    movsd %xmm2, 28(%esp)
    movsd %xmm3, 36(%esp)
    movsd %xmm4, 44(%esp)
    movsd %xmm5, 52(%esp)
    movsd %xmm6, 60(%esp)
    movsd %xmm7, 68(%esp)

    SETUP_GOT_NOSAVE RAW_VAR(got_reg)
    // Load Runtime::instance_ from GOT.
    movl SYMBOL(_ZN3art7Runtime9instance_E)@GOT(REG_VAR(got_reg)), REG_VAR(temp_reg)
    movl (REG_VAR(temp_reg)), REG_VAR(temp_reg)
    // Push save everything callee-save method.
    pushl \runtime_method_offset(REG_VAR(temp_reg))
    CFI_ADJUST_CFA_OFFSET(4)
    // Store esp as the stop quick frame.
    movl %esp, %fs:THREAD_TOP_QUICK_FRAME_OFFSET

    // Ugly compile-time check, but we only have the preprocessor.
    // Last +4: implicit return address pushed on stack when caller made call.
#if (FRAME_SIZE_SAVE_EVERYTHING != 7*4 + 8*8 + 12 + 4 + 4)
#error "FRAME_SIZE_SAVE_EVERYTHING(X86) size not as expected."
#endif
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when EDI is already saved.
     */
MACRO3(SETUP_SAVE_EVERYTHING_FRAME_EDI_SAVED, got_reg, temp_reg, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    // Save core registers from highest to lowest to agree with core spills bitmap.
    // EDI, or at least a placeholder for it, is already on the stack.
    PUSH esi
    SETUP_SAVE_EVERYTHING_FRAME_EDI_ESI_SAVED RAW_VAR(got_reg), RAW_VAR(temp_reg), \runtime_method_offset
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     */
MACRO3(SETUP_SAVE_EVERYTHING_FRAME, got_reg, temp_reg, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    PUSH edi
    SETUP_SAVE_EVERYTHING_FRAME_EDI_SAVED RAW_VAR(got_reg), RAW_VAR(temp_reg), \runtime_method_offset
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_FRPS)
    // Restore FPRs. Method and padding is still on the stack.
    movsd 16(%esp), %xmm0
    movsd 24(%esp), %xmm1
    movsd 32(%esp), %xmm2
    movsd 40(%esp), %xmm3
    movsd 48(%esp), %xmm4
    movsd 56(%esp), %xmm5
    movsd 64(%esp), %xmm6
    movsd 72(%esp), %xmm7
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_EAX)
    // Restore core registers (except eax).
    POP ecx
    POP edx
    POP ebx
    POP ebp
    POP esi
    POP edi
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME)
    RESTORE_SAVE_EVERYTHING_FRAME_FRPS

    // Remove save everything callee save method, stack alignment padding and FPRs.
    addl MACRO_LITERAL(16 + 8 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(-(16 + 8 * 8))

    POP eax
    RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_EAX
END_MACRO

MACRO0(RESTORE_SAVE_EVERYTHING_FRAME_KEEP_EAX)
    RESTORE_SAVE_EVERYTHING_FRAME_FRPS

    // Remove save everything callee save method, stack alignment padding and FPRs, skip EAX.
    addl MACRO_LITERAL(16 + 8 * 8 + 4), %esp
    CFI_ADJUST_CFA_OFFSET(-(16 + 8 * 8 + 4))

    RESTORE_SAVE_EVERYTHING_FRAME_GPRS_EXCEPT_EAX
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     */
MACRO0(DELIVER_PENDING_EXCEPTION_FRAME_READY)
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp               // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*)
    UNREACHABLE
END_MACRO

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx, ebx // save callee saves for throw
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx, ebx // save all registers as basis for long jump context
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp               // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call CALLVAR(cxx_name)                     // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx       // save all registers as basis for long jump context
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp               // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call CALLVAR(cxx_name)                     // cxx_name(Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx, ebx // save all registers as basis for long jump context
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp                // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                                   // pass arg1
    call CALLVAR(cxx_name)                     // cxx_name(arg1, Thread*)
    UNREACHABLE
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx       // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                                   // alignment padding
    pushl %fs:THREAD_SELF_OFFSET               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                                   // pass arg2
    PUSH eax                                   // pass arg1
    call CALLVAR(cxx_name)                     // cxx_name(arg1, arg2, Thread*)
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
    // Save all registers as basis for long jump context; EDI will replace fault address later.
    SETUP_SAVE_EVERYTHING_FRAME_EDI_SAVED ebx, ebx
    // Retrieve fault address and save EDI.
    movl (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__)(%esp), %eax
    movl %edi, (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__)(%esp)
    CFI_REL_OFFSET(%edi, (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__))
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp                           // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                                              // pass arg1
    call SYMBOL(artThrowNullPointerExceptionFromSignal)   // (addr, self)
    UNREACHABLE
END_FUNCTION art_quick_throw_null_pointer_exception

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
     * as usual - except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
     * the method_idx.  This wrapper will save arg1-arg3 and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/r1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in r0/r1 consisting
     * of the target Method* in r0 and method->code_ in r1.
     *
     * If unsuccessful, the helper will return null/null and there will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
MACRO1(INVOKE_TRAMPOLINE_BODY, cxx_name)
    SETUP_SAVE_REFS_AND_ARGS_FRAME ebx, ebx
    movl %esp, %edx  // remember SP

    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call CALLVAR(cxx_name)        // cxx_name(arg1, arg2, Thread*, SP)
    movl %edx, %edi               // save code pointer in EDI
    addl MACRO_LITERAL(20), %esp  // Pop arguments skip eax
    CFI_ADJUST_CFA_OFFSET(-20)

    // Restore FPRs.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3

    // Remove space for FPR args.
    addl MACRO_LITERAL(4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(-4 * 8)

    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    // Swap EDI callee save with code pointer.
    xchgl %edi, (%esp)
    testl %eax, %eax              // Branch forward if exception pending.
    jz    1f
    // Tail call to intended method.
    ret
1:
    addl MACRO_LITERAL(4), %esp   // Pop code pointer off stack
    CFI_ADJUST_CFA_OFFSET(-4)
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
     * Helper for quick invocation stub to set up XMM registers.
     * Increments shorty and arg_array and clobbers temp_char.
     * Branches to finished if it encounters the end of the shorty.
     */
MACRO5(LOOP_OVER_SHORTY_LOADING_XMMS, xmm_reg, shorty, arg_array, temp_char, finished)
1: // LOOP
    movb (REG_VAR(shorty)), REG_VAR(temp_char)     // temp_char := *shorty
    addl MACRO_LITERAL(1), REG_VAR(shorty)         // shorty++
    cmpb MACRO_LITERAL(0), REG_VAR(temp_char)      // if (temp_char == '\0')
    je VAR(finished)                               //   goto finished
    cmpb MACRO_LITERAL(68), REG_VAR(temp_char)     // if (temp_char == 'D')
    je 2f                                          //   goto FOUND_DOUBLE
    cmpb MACRO_LITERAL(70), REG_VAR(temp_char)     // if (temp_char == 'F')
    je 3f                                          //   goto FOUND_FLOAT
    addl MACRO_LITERAL(4), REG_VAR(arg_array)      // arg_array++
    //  Handle extra space in arg array taken by a long.
    cmpb MACRO_LITERAL(74), REG_VAR(temp_char)     // if (temp_char != 'J')
    jne 1b                                         //   goto LOOP
    addl MACRO_LITERAL(4), REG_VAR(arg_array)      // arg_array++
    jmp 1b                                         // goto LOOP
2:  // FOUND_DOUBLE
    movsd (REG_VAR(arg_array)), REG_VAR(xmm_reg)
    addl MACRO_LITERAL(8), REG_VAR(arg_array)      // arg_array+=2
    jmp 4f
3:  // FOUND_FLOAT
    movss (REG_VAR(arg_array)), REG_VAR(xmm_reg)
    addl MACRO_LITERAL(4), REG_VAR(arg_array)      // arg_array++
4:
END_MACRO

    /*
     * Helper for quick invocation stub to set up GPR registers.
     * Increments shorty and arg_array, and returns the current short character in
     * temp_char. Branches to finished if it encounters the end of the shorty.
     */
MACRO4(SKIP_OVER_FLOATS, shorty, arg_array, temp_char, finished)
1: // LOOP:
    movb (REG_VAR(shorty)), REG_VAR(temp_char)     // temp_char := *shorty
    addl MACRO_LITERAL(1), REG_VAR(shorty)         // shorty++
    cmpb MACRO_LITERAL(0), REG_VAR(temp_char)      // if (temp_char == '\0')
    je VAR(finished)                               //   goto finished
    cmpb MACRO_LITERAL(70), REG_VAR(temp_char)     // if (temp_char == 'F')
    je 3f                                          //   goto SKIP_FLOAT
    cmpb MACRO_LITERAL(68), REG_VAR(temp_char)     // if (temp_char == 'D')
    je 4f                                          //   goto SKIP_DOUBLE
    jmp 5f                                         // goto end
3:  // SKIP_FLOAT
    addl MACRO_LITERAL(4), REG_VAR(arg_array)      // arg_array++
    jmp 1b                                         // goto LOOP
4:  // SKIP_DOUBLE
    addl MACRO_LITERAL(8), REG_VAR(arg_array)      // arg_array+=2
    jmp 1b                                         // goto LOOP
5:
END_MACRO

  /*
     * Quick invocation stub (non-static).
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or null for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = shorty
     */
DEFINE_FUNCTION art_quick_invoke_stub
    // Save the non-volatiles.
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    PUSH esi                      // save esi
    PUSH edi                      // save edi
    // Set up argument XMM registers.
    mov 24+16(%esp), %esi         // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 8+16(%esp), %edi          // EDI := arg_array + 4 ; ie skip this pointer.
    addl LITERAL(4), %edi
    // Clobbers ESI, EDI, EAX.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, esi, edi, al, .Lxmm_setup_finished
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, esi, edi, al, .Lxmm_setup_finished
    .balign 16
.Lxmm_setup_finished:
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    CFI_DEF_CFA_REGISTER(ebp)
    mov 28(%ebp), %ebx            // get arg array size
    // reserve space for return addr, method*, ebx, ebp, esi, and edi in frame
    addl LITERAL(36), %ebx
    // align frame size to 16 bytes
    andl LITERAL(0xFFFFFFF0), %ebx
    subl LITERAL(20), %ebx        // remove space for return address, ebx, ebp, esi and edi
    subl %ebx, %esp               // reserve stack space for argument array

    movl LITERAL(0), (%esp)       // store null for method*

    // Copy arg array into stack.
    movl 28(%ebp), %ecx           // ECX = size of args
    movl 24(%ebp), %esi           // ESI = argument array
    leal 4(%esp), %edi            // EDI = just after Method* in stack arguments
    rep movsb                     // while (ecx--) { *edi++ = *esi++ }

    mov 40(%ebp), %esi            // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 24(%ebp), %edi            // EDI := arg_array
    mov 0(%edi), %ecx             // ECX := this pointer
    addl LITERAL(4), %edi         // EDI := arg_array + 4 ; ie skip this pointer.

    // Enumerate the possible cases for loading GPRS.
    // edx (and maybe ebx):
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LfirstLong
    // Must be an integer value.
    movl (%edi), %edx
    addl LITERAL(4), %edi         // arg_array++

    // Now check ebx
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished
.LfirstLong:
    movl (%edi), %edx
    movl 4(%edi), %ebx
    // Nothing left to load.
.Lgpr_setup_finished:
    mov 20(%ebp), %eax            // move method pointer into eax
    call *ART_METHOD_QUICK_CODE_OFFSET_32(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    CFI_DEF_CFA_REGISTER(esp)
    POP edi                       // pop edi
    POP esi                       // pop esi
    POP ebx                       // pop ebx
    POP ebp                       // pop ebp
    mov 20(%esp), %ecx            // get result pointer
    mov %eax, (%ecx)              // store the result assuming its a long, int or Object*
    mov %edx, 4(%ecx)             // store the other half of the result
    mov 24(%esp), %edx            // get the shorty
    cmpb LITERAL(68), (%edx)      // test if result type char == 'D'
    je .Lreturn_double_quick
    cmpb LITERAL(70), (%edx)      // test if result type char == 'F'
    je .Lreturn_float_quick
    ret
.Lreturn_double_quick:
    movsd %xmm0, (%ecx)           // store the floating point result
    ret
.Lreturn_float_quick:
    movss %xmm0, (%ecx)           // store the floating point result
    ret
END_FUNCTION art_quick_invoke_stub

  /*
     * Quick invocation stub (static).
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or null for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = shorty
     */
DEFINE_FUNCTION art_quick_invoke_static_stub
    // Save the non-volatiles.
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    PUSH esi                      // save esi
    PUSH edi                      // save edi
    // Set up argument XMM registers.
    mov 24+16(%esp), %esi         // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 8+16(%esp), %edi          // EDI := arg_array
    // Clobbers ESI, EDI, EAX.
    LOOP_OVER_SHORTY_LOADING_XMMS xmm0, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm1, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm2, esi, edi, al, .Lxmm_setup_finished2
    LOOP_OVER_SHORTY_LOADING_XMMS xmm3, esi, edi, al, .Lxmm_setup_finished2
    .balign 16
.Lxmm_setup_finished2:
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    CFI_DEF_CFA_REGISTER(ebp)
    mov 28(%ebp), %ebx            // get arg array size
    // reserve space for return addr, method*, ebx, ebp, esi, and edi in frame
    addl LITERAL(36), %ebx
    // align frame size to 16 bytes
    andl LITERAL(0xFFFFFFF0), %ebx
    subl LITERAL(20), %ebx        // remove space for return address, ebx, ebp, esi and edi
    subl %ebx, %esp               // reserve stack space for argument array

    movl LITERAL(0), (%esp)       // store null for method*

    // Copy arg array into stack.
    movl 28(%ebp), %ecx           // ECX = size of args
    movl 24(%ebp), %esi           // ESI = argument array
    leal 4(%esp), %edi            // EDI = just after Method* in stack arguments
    rep movsb                     // while (ecx--) { *edi++ = *esi++ }

    mov 40(%ebp), %esi            // ESI := shorty + 1  ; ie skip return arg character.
    addl LITERAL(1), %esi
    mov 24(%ebp), %edi            // EDI := arg_array

    // Enumerate the possible cases for loading GPRS.
    // ecx (and maybe edx)
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LfirstLong2
    // Must be an integer value.  Load into ECX.
    movl (%edi), %ecx
    addl LITERAL(4), %edi         // arg_array++

    // Now check edx (and maybe ebx).
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    cmpb LITERAL(74), %al         // if (al == 'J') goto FOUND_LONG
    je .LSecondLong2
    // Must be an integer.  Load into EDX.
    movl (%edi), %edx
    addl LITERAL(4), %edi         // arg_array++

    // Is there anything for ebx?
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished2
.LSecondLong2:
    // EDX:EBX is long.  That is all.
    movl (%edi), %edx
    movl 4(%edi), %ebx
    jmp .Lgpr_setup_finished2
.LfirstLong2:
    // ECX:EDX is a long
    movl (%edi), %ecx
    movl 4(%edi), %edx
    addl LITERAL(8), %edi         // arg_array += 2

    // Anything for EBX?
    SKIP_OVER_FLOATS esi, edi, al, .Lgpr_setup_finished2
    // Must be first word of a long, or an integer. First word of long doesn't
    // go into EBX, but can be loaded there anyways, as it is harmless.
    movl (%edi), %ebx
    jmp .Lgpr_setup_finished2
    // Nothing left to load.
.Lgpr_setup_finished2:
    mov 20(%ebp), %eax            // move method pointer into eax
    call *ART_METHOD_QUICK_CODE_OFFSET_32(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    CFI_DEF_CFA_REGISTER(esp)
    POP edi                       // pop edi
    POP esi                       // pop esi
    POP ebx                       // pop ebx
    POP ebp                       // pop ebp
    mov 20(%esp), %ecx            // get result pointer
    mov %eax, (%ecx)              // store the result assuming its a long, int or Object*
    mov %edx, 4(%ecx)             // store the other half of the result
    mov 24(%esp), %edx            // get the shorty
    cmpb LITERAL(68), (%edx)      // test if result type char == 'D'
    je .Lreturn_double_quick2
    cmpb LITERAL(70), (%edx)      // test if result type char == 'F'
    je .Lreturn_float_quick2
    ret
.Lreturn_double_quick2:
    movsd %xmm0, (%ecx)           // store the floating point result
    ret
.Lreturn_float_quick2:
    movss %xmm0, (%ecx)           // store the floating point result
    ret
END_FUNCTION art_quick_invoke_static_stub

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx         // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp                  // push padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                 // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                                     // pass arg1
    call CALLVAR(cxx_name)                       // cxx_name(arg1, Thread*)
    addl MACRO_LITERAL(16), %esp                 // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                 // restore frame up to return address
    CALL_MACRO(return_macro)                     // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx         // save ref containing registers for GC
    // Outgoing argument set up
    PUSH eax                                     // push padding
    pushl %fs:THREAD_SELF_OFFSET                 // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                                     // pass arg2
    PUSH eax                                     // pass arg1
    call CALLVAR(cxx_name)                       // cxx_name(arg1, arg2, Thread*)
    addl MACRO_LITERAL(16), %esp                 // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                 // restore frame up to return address
    CALL_MACRO(return_macro)                     // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx         // save ref containing registers for GC
    // Outgoing argument set up
    pushl %fs:THREAD_SELF_OFFSET                 // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                                     // pass arg3
    PUSH ecx                                     // pass arg2
    PUSH eax                                     // pass arg1
    call CALLVAR(cxx_name)                       // cxx_name(arg1, arg2, arg3, Thread*)
    addl MACRO_LITERAL(16), %esp                 // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                 // restore frame up to return address
    CALL_MACRO(return_macro)                     // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(FOUR_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME_PRESERVE_GOT_REG ebx, ebx  // save ref containing registers for GC

    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp                 // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET                 // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                                     // pass arg4
    PUSH edx                                     // pass arg3
    PUSH ecx                                     // pass arg2
    PUSH eax                                     // pass arg1
    call CALLVAR(cxx_name)                       // cxx_name(arg1, arg2, arg3, arg4, Thread*)
    addl MACRO_LITERAL(32), %esp                 // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_SAVE_REFS_ONLY_FRAME                 // restore frame up to return address
    CALL_MACRO(return_macro)                     // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(ONE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx               // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp                       // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                      // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                                          // pass arg1
    call CALLVAR(cxx_name)                            // cxx_name(arg1, Thread*)
    addl MACRO_LITERAL(16), %esp                      // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                      // restore frame up to return address
    CALL_MACRO(return_macro)                          // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(TWO_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx               // save ref containing registers for GC
    // Outgoing argument set up
    PUSH eax                                          // alignment padding
    pushl %fs:THREAD_SELF_OFFSET                      // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                                          // pass arg2
    PUSH eax                                          // pass arg1
    call CALLVAR(cxx_name)                            // cxx_name(arg1, arg2, referrer, Thread*)
    addl MACRO_LITERAL(16), %esp                      // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                      // restore frame up to return address
    CALL_MACRO(return_macro)                          // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO3(THREE_ARG_REF_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx               // save ref containing registers for GC
    // Outgoing argument set up
    pushl %fs:THREAD_SELF_OFFSET                      // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                                          // pass arg3
    PUSH ecx                                          // pass arg2
    PUSH eax                                          // pass arg1
    call CALLVAR(cxx_name)                            // cxx_name(arg1, arg2, arg3, Thread*)
    addl LITERAL(16), %esp                            // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_SAVE_REFS_ONLY_FRAME                      // restore frame up to return address
    CALL_MACRO(return_macro)                          // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

// Macro for string and type resolution and initialization.
MACRO3(ONE_ARG_SAVE_EVERYTHING_DOWNCALL, c_name, cxx_name, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET)
    DEFINE_FUNCTION VAR(c_name)
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx, \runtime_method_offset  // save ref containing registers for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp                       // push padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                      // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                                          // pass arg1
    call CALLVAR(cxx_name)                            // cxx_name(arg1, Thread*)
    addl MACRO_LITERAL(16), %esp                      // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    testl %eax, %eax                                  // If result is null, deliver the OOME.
    jz 1f
    CFI_REMEMBER_STATE
    RESTORE_SAVE_EVERYTHING_FRAME_KEEP_EAX            // restore frame up to return address
    ret                                               // return
    CFI_RESTORE_STATE
    CFI_DEF_CFA(esp, FRAME_SIZE_SAVE_EVERYTHING)      // workaround for clang bug: 31975598
1:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
    END_FUNCTION VAR(c_name)
END_MACRO

MACRO2(ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT, c_name, cxx_name)
    ONE_ARG_SAVE_EVERYTHING_DOWNCALL \c_name, \cxx_name, RUNTIME_SAVE_EVERYTHING_FOR_CLINIT_METHOD_OFFSET
END_MACRO

MACRO0(RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER)
    testl %eax, %eax               // eax == 0 ?
    jz  1f                         // if eax == 0 goto 1
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
    cmpl MACRO_LITERAL(0),%fs:THREAD_EXCEPTION_OFFSET // exception field == 0 ?
    jne 1f                                            // if exception field != 0 goto 1
    ret                                               // return
1:                                                    // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

// Generate the allocation entrypoints for each allocator.
GENERATE_ALLOC_ENTRYPOINTS_FOR_NON_TLAB_ALLOCATORS

// Comment out allocators that have x86 specific asm.
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
    // eax: type/return value
    // ecx, ebx, edx: free
    movl %fs:THREAD_SELF_OFFSET, %ebx                   // ebx = thread
                                                        // Check if the thread local allocation
                                                        // stack has room
    movl THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET(%ebx), %ecx
    cmpl THREAD_LOCAL_ALLOC_STACK_END_OFFSET(%ebx), %ecx
    jae  .Lslow_path\c_name

    movl MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET(%eax), %ecx  // Load the object size (ecx)
                                                        // Check if the size is for a thread
                                                        // local allocation. Also does the
                                                        // finalizable and initialization check.
    cmpl LITERAL(ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE), %ecx
    ja   .Lslow_path\c_name
    shrl LITERAL(ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT), %ecx // Calculate the rosalloc bracket index
                                                            // from object size.
                                                        // Load thread local rosalloc run (ebx)
                                                        // Subtract __SIZEOF_POINTER__ to subtract
                                                        // one from edi as there is no 0 byte run
                                                        // and the size is already aligned.
    movl (THREAD_ROSALLOC_RUNS_OFFSET - __SIZEOF_POINTER__)(%ebx, %ecx, __SIZEOF_POINTER__), %ebx
                                                        // Load free_list head (edi),
                                                        // this will be the return value.
    movl (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)(%ebx), %ecx
    jecxz   .Lslow_path\c_name
                                                        // Point of no slow path. Won't go to
                                                        // the slow path from here on.
                                                        // Load the next pointer of the head
                                                        // and update head of free list with
                                                        // next pointer
    movl ROSALLOC_SLOT_NEXT_OFFSET(%ecx), %edx
    movl %edx, (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)(%ebx)
                                                        // Decrement size of free list by 1
    decl (ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)(%ebx)
                                                        // Store the class pointer in the
                                                        // header. This also overwrites the
                                                        // next pointer. The offsets are
                                                        // asserted to match.
#if ROSALLOC_SLOT_NEXT_OFFSET != MIRROR_OBJECT_CLASS_OFFSET
#error "Class pointer needs to overwrite next pointer."
#endif
    POISON_HEAP_REF eax
    movl %eax, MIRROR_OBJECT_CLASS_OFFSET(%ecx)
    movl %fs:THREAD_SELF_OFFSET, %ebx                   // ebx = thread
                                                        // Push the new object onto the thread
                                                        // local allocation stack and
                                                        // increment the thread local
                                                        // allocation stack top.
    movl THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET(%ebx), %eax
    movl %ecx, (%eax)
    addl LITERAL(COMPRESSED_REFERENCE_SIZE), %eax
    movl %eax, THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET(%ebx)
                                                        // No fence needed for x86.
    movl %ecx, %eax                                     // Move object to return register
    ret
.Lslow_path\c_name:
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx          // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                       // alignment padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax
    call SYMBOL(artAllocObjectFromCodeResolvedRosAlloc)  // cxx_name(arg0, Thread*)
    addl LITERAL(16), %esp                       // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                 // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER      // return or deliver exception
    END_FUNCTION VAR(c_name)
END_MACRO

ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_resolved_rosalloc, artAllocObjectFromCodeResolvedRosAlloc
ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_initialized_rosalloc, artAllocObjectFromCodeInitializedRosAlloc

// The common fast path code for art_quick_alloc_object_resolved/initialized_tlab
// and art_quick_alloc_object_resolved/initialized_region_tlab.
//
// EAX: type/return_value
MACRO1(ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH, slowPathLabel)
    movl %fs:THREAD_SELF_OFFSET, %ebx                   // ebx = thread
    movl THREAD_LOCAL_END_OFFSET(%ebx), %edi            // Load thread_local_end.
    subl THREAD_LOCAL_POS_OFFSET(%ebx), %edi            // Compute the remaining buffer size.
    movl MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET(%eax), %ecx  // Load the object size.
    cmpl %edi, %ecx                                     // Check if it fits.
    ja   VAR(slowPathLabel)
    movl THREAD_LOCAL_POS_OFFSET(%ebx), %edx            // Load thread_local_pos
                                                        // as allocated object.
    addl %edx, %ecx                                     // Add the object size.
    movl %ecx, THREAD_LOCAL_POS_OFFSET(%ebx)            // Update thread_local_pos.
    incl THREAD_LOCAL_OBJECTS_OFFSET(%ebx)              // Increase thread_local_objects.
                                                        // Store the class pointer in the header.
                                                        // No fence needed for x86.
    POISON_HEAP_REF eax
    movl %eax, MIRROR_OBJECT_CLASS_OFFSET(%edx)
    movl %edx, %eax
    POP edi
    ret                                                 // Fast path succeeded.
END_MACRO

// The common slow path code for art_quick_alloc_object_resolved/initialized_tlab
// and art_quick_alloc_object_resolved/initialized_region_tlab.
MACRO1(ALLOC_OBJECT_RESOLVED_TLAB_SLOW_PATH, cxx_name)
    POP edi
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx                 // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                               // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                        // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax
    call CALLVAR(cxx_name)                              // cxx_name(arg0, Thread*)
    addl LITERAL(16), %esp
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME                        // restore frame up to return address
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER             // return or deliver exception
END_MACRO

MACRO2(ART_QUICK_ALLOC_OBJECT_TLAB, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name)
    // Fast path tlab allocation.
    // EAX: type
    // EBX, ECX, EDX: free.
    PUSH edi
    ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH .Lslow_path\c_name
.Lslow_path\c_name:
    ALLOC_OBJECT_RESOLVED_TLAB_SLOW_PATH RAW_VAR(cxx_name)
    END_FUNCTION VAR(c_name)
END_MACRO

ART_QUICK_ALLOC_OBJECT_TLAB art_quick_alloc_object_resolved_tlab, artAllocObjectFromCodeResolvedTLAB
ART_QUICK_ALLOC_OBJECT_TLAB art_quick_alloc_object_initialized_tlab, artAllocObjectFromCodeInitializedTLAB
ART_QUICK_ALLOC_OBJECT_TLAB art_quick_alloc_object_resolved_region_tlab, artAllocObjectFromCodeResolvedRegionTLAB
ART_QUICK_ALLOC_OBJECT_TLAB art_quick_alloc_object_initialized_region_tlab, artAllocObjectFromCodeInitializedRegionTLAB

// The fast path code for art_quick_alloc_array_region_tlab.
// Inputs: EAX: the class, ECX: int32_t component_count, EDX: total_size
// Free temp: EBX
// Output: EAX: return value.
MACRO1(ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE, slowPathLabel)
    mov %fs:THREAD_SELF_OFFSET, %ebx                          // ebx = thread
    // Mask out the unaligned part to make sure we are 8 byte aligned.
    andl LITERAL(OBJECT_ALIGNMENT_MASK_TOGGLED), %edx
    movl THREAD_LOCAL_END_OFFSET(%ebx), %edi
    subl THREAD_LOCAL_POS_OFFSET(%ebx), %edi
    cmpl %edi, %edx                                           // Check if it fits.
    ja   RAW_VAR(slowPathLabel)
    movl THREAD_LOCAL_POS_OFFSET(%ebx), %edi
    addl %edi, %edx                                            // Add the object size.
    movl %edx, THREAD_LOCAL_POS_OFFSET(%ebx)                   // Update thread_local_pos_
    addl LITERAL(1), THREAD_LOCAL_OBJECTS_OFFSET(%ebx)         // Increase thread_local_objects.
                                                               // Store the class pointer in the
                                                               // header.
                                                               // No fence needed for x86.
    POISON_HEAP_REF eax
    movl %eax, MIRROR_OBJECT_CLASS_OFFSET(%edi)
    movl %ecx, MIRROR_ARRAY_LENGTH_OFFSET(%edi)
    movl %edi, %eax
    POP edi
    ret                                                        // Fast path succeeded.
END_MACRO

MACRO1(COMPUTE_ARRAY_SIZE_UNKNOWN, slow_path)
    // We should never enter here. Code is provided for reference.
    int3
    // Possibly a large object, go slow.
    // Also does negative array size check.
    cmpl LITERAL((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_WIDE_ARRAY_DATA_OFFSET) / 8), %ecx
    ja RAW_VAR(slow_path)
    PUSH ecx
    movl %ecx, %edx
    movl MIRROR_CLASS_COMPONENT_TYPE_OFFSET(%eax), %ecx        // Load component type.
    UNPOISON_HEAP_REF ecx
    movl MIRROR_CLASS_OBJECT_PRIMITIVE_TYPE_OFFSET(%ecx), %ecx // Load primitive type.
    shr MACRO_LITERAL(PRIMITIVE_TYPE_SIZE_SHIFT_SHIFT), %ecx        // Get component size shift.
    sall %cl, %edx                                              // Calculate array count shifted.
    // Add array header + alignment rounding.
    add MACRO_LITERAL(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK), %edx
    // Add 4 extra bytes if we are doing a long array.
    add MACRO_LITERAL(1), %ecx
    and MACRO_LITERAL(4), %ecx
#if MIRROR_WIDE_ARRAY_DATA_OFFSET != MIRROR_INT_ARRAY_DATA_OFFSET + 4
#error Long array data offset must be 4 greater than int array data offset.
#endif
    addl %ecx, %edx
    POP ecx
END_MACRO

MACRO1(COMPUTE_ARRAY_SIZE_8, slow_path)
    // EAX: mirror::Class* klass, ECX: int32_t component_count
    // Possibly a large object, go slow.
    // Also does negative array size check.
    cmpl LITERAL(MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET), %ecx
    ja RAW_VAR(slow_path)
    // Add array header + alignment rounding.
    leal (MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)(%ecx), %edx
END_MACRO

MACRO1(COMPUTE_ARRAY_SIZE_16, slow_path)
    // EAX: mirror::Class* klass, ECX: int32_t component_count
    // Possibly a large object, go slow.
    // Also does negative array size check.
    cmpl LITERAL((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET) / 2), %ecx
    ja RAW_VAR(slow_path)
    // Add array header + alignment rounding.
    leal ((MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK) / 2)(%ecx), %edx
    sall MACRO_LITERAL(1), %edx
END_MACRO

MACRO1(COMPUTE_ARRAY_SIZE_32, slow_path)
    // EAX: mirror::Class* klass, ECX: int32_t component_count
    // Possibly a large object, go slow.
    // Also does negative array size check.
    cmpl LITERAL((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET) / 4), %ecx
    ja RAW_VAR(slow_path)
    // Add array header + alignment rounding.
    leal ((MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK) / 4)(%ecx), %edx
    sall MACRO_LITERAL(2), %edx
END_MACRO

MACRO1(COMPUTE_ARRAY_SIZE_64, slow_path)
    // EAX: mirror::Class* klass, ECX: int32_t component_count
    // Possibly a large object, go slow.
    // Also does negative array size check.
    cmpl LITERAL((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_WIDE_ARRAY_DATA_OFFSET) / 8), %ecx
    ja RAW_VAR(slow_path)
    // Add array header + alignment rounding.
    leal ((MIRROR_WIDE_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK) / 8)(%ecx), %edx
    sall MACRO_LITERAL(3), %edx
END_MACRO

MACRO3(GENERATE_ALLOC_ARRAY_TLAB, c_entrypoint, cxx_name, size_setup)
    DEFINE_FUNCTION VAR(c_entrypoint)
    // EAX: mirror::Class* klass, ECX: int32_t component_count
    PUSH edi
    CALL_MACRO(size_setup) .Lslow_path\c_entrypoint
    ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE .Lslow_path\c_entrypoint
.Lslow_path\c_entrypoint:
    POP edi
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx                        // save ref containing registers for GC
    // Outgoing argument set up
    PUSH eax                                                   // alignment padding
    pushl %fs:THREAD_SELF_OFFSET                               // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx
    PUSH eax
    call CALLVAR(cxx_name)                                     // cxx_name(arg0, arg1, Thread*)
    addl LITERAL(16), %esp                                     // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
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

ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_static_storage, artInitializeStaticStorageFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_type, artInitializeTypeFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_resolve_string, artResolveStringFromCode

TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_EAX_ZERO

DEFINE_FUNCTION art_quick_lock_object
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_lock
.Lretry_lock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%eax), %ecx  // ecx := lock word
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx         // test the 2 high bits.
    jne  .Lslow_lock                      // slow path if either of the two high bits are set.
    movl %ecx, %edx                       // save lock word (edx) to keep read barrier bits.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %ecx  // zero the gc bits.
    test %ecx, %ecx
    jnz  .Lalready_thin                   // lock word contains a thin lock
    // unlocked case - edx: original lock word, eax: obj.
    movl %eax, %ecx                       // remember object in case of retry
    movl %edx, %eax                       // eax: lock word zero except for read barrier bits.
    movl %fs:THREAD_ID_OFFSET, %edx       // load thread id.
    or   %eax, %edx                       // edx: thread id with count of 0 + read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%ecx)  // eax: old val, edx: new val.
    jnz  .Llock_cmpxchg_fail              // cmpxchg failed retry
    ret
.Lalready_thin:  // edx: lock word (with high 2 bits zero and original rb bits), eax: obj.
    movl %fs:THREAD_ID_OFFSET, %ecx       // ecx := thread id
    cmpw %cx, %dx                         // do we hold the lock already?
    jne  .Lslow_lock
    movl %edx, %ecx                       // copy the lock word to check count overflow.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %ecx  // zero the read barrier bits.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // increment recursion count for overflow check.
    test LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED), %ecx  // overflowed if the first gc state bit is set.
    jne  .Lslow_lock                      // count overflowed so go slow
    movl %eax, %ecx                       // save obj to use eax for cmpxchg.
    movl %edx, %eax                       // copy the lock word as the old val for cmpxchg.
    addl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx  // increment recursion count again for real.
    // update lockword, cmpxchg necessary for read barrier bits.
    lock cmpxchg  %edx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%ecx)  // eax: old val, edx: new val.
    jnz  .Llock_cmpxchg_fail              // cmpxchg failed retry
    ret
.Llock_cmpxchg_fail:
    movl  %ecx, %eax                      // restore eax
    jmp  .Lretry_lock
.Lslow_lock:
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                 // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                              // pass object
    call SYMBOL(artLockObjectFromCode)    // artLockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object

DEFINE_FUNCTION art_quick_lock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                 // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                              // pass object
    call SYMBOL(artLockObjectFromCode)    // artLockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_lock_object_no_inline


DEFINE_FUNCTION art_quick_unlock_object
    testl %eax, %eax                      // null check object/eax
    jz   .Lslow_unlock
.Lretry_unlock:
    movl MIRROR_OBJECT_LOCK_WORD_OFFSET(%eax), %ecx  // ecx := lock word
    movl %fs:THREAD_ID_OFFSET, %edx       // edx := thread id
    test LITERAL(LOCK_WORD_STATE_MASK), %ecx
    jnz  .Lslow_unlock                    // lock word contains a monitor
    cmpw %cx, %dx                         // does the thread id match?
    jne  .Lslow_unlock
    movl %ecx, %edx                       // copy the lock word to detect new count of 0.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED), %edx  // zero the gc bits.
    cmpl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %edx
    jae  .Lrecursive_thin_unlock
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %eax, %edx                       // edx: obj
    movl %ecx, %eax                       // eax: old lock word.
    andl LITERAL(LOCK_WORD_GC_STATE_MASK_SHIFTED), %ecx  // ecx: new lock word zero except original rb bits.
#ifndef USE_READ_BARRIER
    movl %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)  // eax: old val, ecx: new val.
    jnz  .Lunlock_cmpxchg_fail            // cmpxchg failed retry
#endif
    ret
.Lrecursive_thin_unlock:  // ecx: original lock word, eax: obj
    // update lockword, cmpxchg necessary for read barrier bits.
    movl %eax, %edx                       // edx: obj
    movl %ecx, %eax                       // eax: old lock word.
    subl LITERAL(LOCK_WORD_THIN_LOCK_COUNT_ONE), %ecx  // ecx: new lock word with decremented count.
#ifndef USE_READ_BARRIER
    mov  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)
#else
    lock cmpxchg  %ecx, MIRROR_OBJECT_LOCK_WORD_OFFSET(%edx)  // eax: old val, ecx: new val.
    jnz  .Lunlock_cmpxchg_fail            // cmpxchg failed retry
#endif
    ret
.Lunlock_cmpxchg_fail:  // edx: obj
    movl %edx, %eax                       // restore eax
    jmp  .Lretry_unlock
.Lslow_unlock:
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                 // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                              // pass object
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object

DEFINE_FUNCTION art_quick_unlock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME  ebx, ebx  // save ref containing registers for GC
    // Outgoing argument set up
    subl LITERAL(8), %esp                 // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                              // pass object
    call SYMBOL(artUnlockObjectFromCode)  // artUnlockObjectFromCode(object, Thread*)
    addl LITERAL(16), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_REFS_ONLY_FRAME          // restore frame up to return address
    RETURN_IF_EAX_ZERO
END_FUNCTION art_quick_unlock_object_no_inline

DEFINE_FUNCTION art_quick_instance_of
    PUSH eax                              // alignment padding
    PUSH ecx                              // pass arg2 - obj->klass
    PUSH eax                              // pass arg1 - checked class
    call SYMBOL(artInstanceOfFromCode)    // (Object* obj, Class* ref_klass)
    addl LITERAL(12), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_instance_of

DEFINE_FUNCTION art_quick_check_instance_of
    PUSH eax                              // alignment padding
    PUSH ecx                              // pass arg2 - checked class
    PUSH eax                              // pass arg1 - obj
    call SYMBOL(artInstanceOfFromCode)    // (Object* obj, Class* ref_klass)
    testl %eax, %eax
    jz .Lthrow_class_cast_exception       // jump forward if not assignable
    addl LITERAL(12), %esp                // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
    CFI_ADJUST_CFA_OFFSET(12)             // Reset unwind info so following code unwinds.

.Lthrow_class_cast_exception:
    POP eax                               // pop arguments
    POP ecx
    addl LITERAL(4), %esp
    CFI_ADJUST_CFA_OFFSET(-4)

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx, ebx // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                              // alignment padding
    pushl %fs:THREAD_SELF_OFFSET          // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                              // pass arg2
    PUSH eax                              // pass arg1
    call SYMBOL(artThrowClassCastExceptionForObject)  // (Object* src, Class* dest, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_check_instance_of

// Restore reg's value if reg is not the same as exclude_reg, otherwise just adjust stack.
MACRO2(POP_REG_NE, reg, exclude_reg)
    .ifc RAW_VAR(reg), RAW_VAR(exclude_reg)
      addl MACRO_LITERAL(4), %esp
      CFI_ADJUST_CFA_OFFSET(-4)
    .else
      POP RAW_VAR(reg)
    .endif
END_MACRO

    /*
     * Macro to insert read barrier, only used in art_quick_aput_obj.
     * obj_reg and dest_reg are registers, offset is a defined literal such as
     * MIRROR_OBJECT_CLASS_OFFSET.
     * pop_eax is a boolean flag, indicating if eax is popped after the call.
     * TODO: When read barrier has a fast path, add heap unpoisoning support for the fast path.
     */
MACRO4(READ_BARRIER, obj_reg, offset, dest_reg, pop_eax)
#ifdef USE_READ_BARRIER
    PUSH eax                        // save registers used in art_quick_aput_obj
    PUSH ebx
    PUSH edx
    PUSH ecx
    // Outgoing argument set up
    pushl MACRO_LITERAL((RAW_VAR(offset)))  // pass offset, double parentheses are necessary
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH RAW_VAR(obj_reg)           // pass obj_reg
    PUSH eax                        // pass ref, just pass eax for now since parameter ref is unused
    call SYMBOL(artReadBarrierSlow) // artReadBarrierSlow(ref, obj_reg, offset)
    // No need to unpoison return value in eax, artReadBarrierSlow() would do the unpoisoning.
    .ifnc RAW_VAR(dest_reg), eax
      movl %eax, REG_VAR(dest_reg)  // save loaded ref in dest_reg
    .endif
    addl MACRO_LITERAL(12), %esp    // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    POP_REG_NE ecx, RAW_VAR(dest_reg) // Restore args except dest_reg
    POP_REG_NE edx, RAW_VAR(dest_reg)
    POP_REG_NE ebx, RAW_VAR(dest_reg)
    .ifc RAW_VAR(pop_eax), true
      POP_REG_NE eax, RAW_VAR(dest_reg)
    .endif
#else
    movl RAW_VAR(offset)(REG_VAR(obj_reg)), REG_VAR(dest_reg)
    UNPOISON_HEAP_REF RAW_VAR(dest_reg)
#endif  // USE_READ_BARRIER
END_MACRO

DEFINE_FUNCTION art_quick_aput_obj
    test %edx, %edx              // store of null
    jz .Ldo_aput_null
    READ_BARRIER eax, MIRROR_OBJECT_CLASS_OFFSET, ebx, true
    READ_BARRIER ebx, MIRROR_CLASS_COMPONENT_TYPE_OFFSET, ebx, true
    // value's type == array's component type - trivial assignability
#if defined(USE_READ_BARRIER)
    READ_BARRIER edx, MIRROR_OBJECT_CLASS_OFFSET, eax, false
    cmpl %eax, %ebx
    POP eax                      // restore eax from the push in the beginning of READ_BARRIER macro
    // This asymmetric push/pop saves a push of eax and maintains stack alignment.
#elif defined(USE_HEAP_POISONING)
    PUSH eax                     // save eax
    movl MIRROR_OBJECT_CLASS_OFFSET(%edx), %eax
    UNPOISON_HEAP_REF eax
    cmpl %eax, %ebx
    POP eax                      // restore eax
#else
    cmpl MIRROR_OBJECT_CLASS_OFFSET(%edx), %ebx
#endif
    jne .Lcheck_assignability
.Ldo_aput:
    POISON_HEAP_REF edx
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(CARD_TABLE_CARD_SHIFT), %eax
    movb %dl, (%edx, %eax)
    ret
.Ldo_aput_null:
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)
    ret
.Lcheck_assignability:
    PUSH eax                      // save arguments
    PUSH ecx
    PUSH edx
#if defined(USE_READ_BARRIER)
    subl LITERAL(4), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(4)
    READ_BARRIER edx, MIRROR_OBJECT_CLASS_OFFSET, eax, true
    subl LITERAL(4), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass arg2 - type of the value to be stored
#elif defined(USE_HEAP_POISONING)
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    movl MIRROR_OBJECT_CLASS_OFFSET(%edx), %eax
    UNPOISON_HEAP_REF eax
    PUSH eax                      // pass arg2 - type of the value to be stored
#else
    subl LITERAL(8), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    pushl MIRROR_OBJECT_CLASS_OFFSET(%edx)  // pass arg2 - type of the value to be stored
    CFI_ADJUST_CFA_OFFSET(4)
#endif
    PUSH ebx                      // pass arg1 - component type of the array
    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    testl %eax, %eax
    jz   .Lthrow_array_store_exception
    POP  edx
    POP  ecx
    POP  eax
    POISON_HEAP_REF edx
    movl %edx, MIRROR_OBJECT_ARRAY_DATA_OFFSET(%eax, %ecx, 4)  // do the aput
    movl %fs:THREAD_CARD_TABLE_OFFSET, %edx
    shrl LITERAL(CARD_TABLE_CARD_SHIFT), %eax
    movb %dl, (%edx, %eax)
    ret
    CFI_ADJUST_CFA_OFFSET(12)     // 3 POP after the jz for unwinding.
.Lthrow_array_store_exception:
    POP  edx
    POP  ecx
    POP  eax
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME ebx, ebx // save all registers as basis for long jump context
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH edx                      // pass arg2 - value
    PUSH eax                      // pass arg1 - array
    call SYMBOL(artThrowArrayStoreException) // (array, value, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_aput_obj

DEFINE_FUNCTION art_quick_memcpy
    SETUP_GOT_NOSAVE ebx          // clobbers EBX
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call PLT_SYMBOL(memcpy)       // (void*, const void*, size_t)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_memcpy

DEFINE_FUNCTION art_quick_test_suspend
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx, RUNTIME_SAVE_EVERYTHING_FOR_SUSPEND_CHECK_METHOD_OFFSET  // save everything for GC
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp                      // push padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET                      // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artTestSuspendFromCode)               // (Thread*)
    addl MACRO_LITERAL(16), %esp                      // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    RESTORE_SAVE_EVERYTHING_FRAME                     // restore frame up to return address
    ret                                               // return
END_FUNCTION art_quick_test_suspend

DEFINE_FUNCTION art_quick_d2l
    subl LITERAL(12), %esp        // alignment padding, room for argument
    CFI_ADJUST_CFA_OFFSET(12)
    movsd %xmm0, 0(%esp)          // arg a
    call SYMBOL(art_d2l)          // (jdouble a)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_d2l

DEFINE_FUNCTION art_quick_f2l
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    movss %xmm0, 0(%esp)          // arg a
    call SYMBOL(art_f2l)          // (jfloat a)
    addl LITERAL(12), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_f2l

DEFINE_FUNCTION art_quick_ldiv
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass arg4 b.hi
    PUSH edx                      // pass arg3 b.lo
    PUSH ecx                      // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(artLdiv)          // (jlong a, jlong b)
    addl LITERAL(28), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-28)
    ret
END_FUNCTION art_quick_ldiv

DEFINE_FUNCTION art_quick_lmod
    subl LITERAL(12), %esp        // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    PUSH ebx                      // pass arg4 b.hi
    PUSH edx                      // pass arg3 b.lo
    PUSH ecx                      // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(artLmod)          // (jlong a, jlong b)
    addl LITERAL(28), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-28)
    ret
END_FUNCTION art_quick_lmod

DEFINE_FUNCTION art_quick_lmul
    imul %eax, %ebx               // ebx = a.lo(eax) * b.hi(ebx)
    imul %edx, %ecx               // ecx = b.lo(edx) * a.hi(ecx)
    mul  %edx                     // edx:eax = a.lo(eax) * b.lo(edx)
    add  %ebx, %ecx
    add  %ecx, %edx               // edx += (a.lo * b.hi) + (b.lo * a.hi)
    ret
END_FUNCTION art_quick_lmul

DEFINE_FUNCTION art_quick_lshl
    // ecx:eax << edx
    xchg %edx, %ecx
    shld %cl,%eax,%edx
    shl  %cl,%eax
    test LITERAL(32), %cl
    jz  1f
    mov %eax, %edx
    xor %eax, %eax
1:
    ret
END_FUNCTION art_quick_lshl

DEFINE_FUNCTION art_quick_lshr
    // ecx:eax >> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    sar  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    sar LITERAL(31), %edx
1:
    ret
END_FUNCTION art_quick_lshr

DEFINE_FUNCTION art_quick_lushr
    // ecx:eax >>> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    shr  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    xor %edx, %edx
1:
    ret
END_FUNCTION art_quick_lushr

// Note: Functions `art{Get,Set}<Kind>{Static,Instance}FromCompiledCode` are
// defined with a macro in runtime/entrypoints/quick/quick_field_entrypoints.cc.

ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION

TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCompiledCode, RETURN_IF_EAX_ZERO
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCompiledCode, RETURN_IF_EAX_ZERO

THREE_ARG_REF_DOWNCALL art_quick_set64_static, artSet64StaticFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCompiledCode, RETURN_IF_EAX_ZERO
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCompiledCode, RETURN_IF_EAX_ZERO

// Call artSet64InstanceFromCode with 4 word size arguments.
DEFINE_FUNCTION art_quick_set64_instance
    movd %ebx, %xmm0
    SETUP_SAVE_REFS_ONLY_FRAME ebx, ebx  // save ref containing registers for GC
    movd %xmm0, %ebx
    // Outgoing argument set up
    subl LITERAL(12), %esp         // alignment padding
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ebx                      // pass high half of new_val
    PUSH edx                      // pass low half of new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet64InstanceFromCompiledCode)  // (field_idx, Object*, new_val, Thread*)
    addl LITERAL(32), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_SAVE_REFS_ONLY_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_instance

DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_EAX
    PUSH esp                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass proxy method
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(16 + FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY), %esp
    CFI_ADJUST_CFA_OFFSET(-(16 + FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY))
    RESTORE_SAVE_REFS_ONLY_FRAME
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * eax is the conflict ArtMethod.
     * xmm7 is a hidden argument that holds the target interface method's dex method index.
     *
     * Note that this stub writes to eax.
     * Because of lack of free registers, it also saves and restores edi.
     */
DEFINE_FUNCTION art_quick_imt_conflict_trampoline
    PUSH EDI
    PUSH ESI
    PUSH EDX
    movl 16(%esp), %edi         // Load referrer.
    // If the method is obsolete, just go through the dex cache miss slow path.
    // The obsolete flag is set with suspended threads, so we do not need an acquire operation here.
    testl LITERAL(ACC_OBSOLETE_METHOD), ART_METHOD_ACCESS_FLAGS_OFFSET(%edi)
    jnz .Limt_conflict_trampoline_dex_cache_miss
    movl ART_METHOD_DECLARING_CLASS_OFFSET(%edi), %edi // Load declaring class (no read barrier).
    movl MIRROR_CLASS_DEX_CACHE_OFFSET(%edi), %edi     // Load the DexCache (without read barrier).
    UNPOISON_HEAP_REF edi
    movl MIRROR_DEX_CACHE_RESOLVED_METHODS_OFFSET(%edi), %edi  // Load the resolved methods.
    pushl ART_METHOD_JNI_OFFSET_32(%eax)  // Push ImtConflictTable.
    CFI_ADJUST_CFA_OFFSET(4)
    movd %xmm7, %eax            // Get target method index stored in xmm7.
    movl %eax, %esi             // Remember method index in ESI.
    andl LITERAL(METHOD_DEX_CACHE_SIZE_MINUS_ONE), %eax  // Calculate DexCache method slot index.
    leal 0(%edi, %eax, 2 * __SIZEOF_POINTER__), %edi  // Load DexCache method slot address.
    mov %ecx, %edx              // Make EDX:EAX == ECX:EBX so that LOCK CMPXCHG8B makes no changes.
    mov %ebx, %eax              // (The actual value does not matter.)
    lock cmpxchg8b (%edi)       // Relaxed atomic load EDX:EAX from the dex cache slot.
    popl %edi                   // Pop ImtConflictTable.
    CFI_ADJUST_CFA_OFFSET(-4)
    cmp %edx, %esi              // Compare method index to see if we had a DexCache method hit.
    jne .Limt_conflict_trampoline_dex_cache_miss
.Limt_table_iterate:
    cmpl %eax, 0(%edi)
    jne .Limt_table_next_entry
    // We successfully hit an entry in the table. Load the target method
    // and jump to it.
    movl __SIZEOF_POINTER__(%edi), %eax
    CFI_REMEMBER_STATE
    POP EDX
    POP ESI
    POP EDI
    jmp *ART_METHOD_QUICK_CODE_OFFSET_32(%eax)
    CFI_RESTORE_STATE
.Limt_table_next_entry:
    // If the entry is null, the interface method is not in the ImtConflictTable.
    cmpl LITERAL(0), 0(%edi)
    jz .Lconflict_trampoline
    // Iterate over the entries of the ImtConflictTable.
    addl LITERAL(2 * __SIZEOF_POINTER__), %edi
    jmp .Limt_table_iterate
.Lconflict_trampoline:
    // Call the runtime stub to populate the ImtConflictTable and jump to the
    // resolved method.
    CFI_REMEMBER_STATE
    POP EDX
    POP ESI
    POP EDI
    INVOKE_TRAMPOLINE_BODY artInvokeInterfaceTrampoline
    CFI_RESTORE_STATE
.Limt_conflict_trampoline_dex_cache_miss:
    // We're not creating a proper runtime method frame here,
    // artLookupResolvedMethod() is not allowed to walk the stack.

    // Save core register args; EDX is already saved.
    PUSH ebx
    PUSH ecx

    // Save FPR args.
    subl MACRO_LITERAL(32), %esp
    CFI_ADJUST_CFA_OFFSET(32)
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)

    pushl 32+8+16(%esp)         // Pass referrer.
    CFI_ADJUST_CFA_OFFSET(4)
    pushl %esi                  // Pass method index.
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artLookupResolvedMethod)  // (uint32_t method_index, ArtMethod* referrer)
    addl LITERAL(8), %esp       // Pop arguments.
    CFI_ADJUST_CFA_OFFSET(-8)

    // Restore FPR args.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3
    addl MACRO_LITERAL(32), %esp
    CFI_ADJUST_CFA_OFFSET(-32)

    // Restore core register args.
    POP ecx
    POP ebx

    cmp LITERAL(0), %eax        // If the method wasn't resolved,
    je .Lconflict_trampoline    //   skip the lookup and go to artInvokeInterfaceTrampoline().
    jmp .Limt_table_iterate
END_FUNCTION art_quick_imt_conflict_trampoline

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME ebx, ebx
    movl %esp, %edi
    PUSH EDI                      // pass SP. do not just PUSH ESP; that messes up unwinding
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass method
    call SYMBOL(artQuickResolutionTrampoline) // (Method* called, receiver, Thread*, SP)
    movl %eax, %edi               // remember code pointer in EDI
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)
    test %eax, %eax               // if code pointer is null goto deliver pending exception
    jz 1f
    RESTORE_SAVE_REFS_AND_ARGS_FRAME_AND_JUMP
1:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

DEFINE_FUNCTION art_quick_generic_jni_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_EAX
    movl %esp, %ebp               // save SP at callee-save frame
    CFI_DEF_CFA_REGISTER(ebp)
    subl LITERAL(5120), %esp
    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //  (esp)    4(esp)   <= C calling convention
    //  fs:...    ebp     <= where they are

    subl LITERAL(8), %esp         // Padding for 16B alignment.
    pushl %ebp                    // Pass SP (to ArtMethod).
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    call SYMBOL(artQuickGenericJniTrampoline)  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // eax: pointer to native code, 0 on error.
    // edx: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    test %eax, %eax
    jz .Lexception_in_native

    // Release part of the alloca.
    movl %edx, %esp

    // On x86 there are no registers passed, so nothing to pop here.
    // Native call.
    call *%eax

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*, result, result_f)
    //  (esp)    4(esp)  12(esp)    <= C calling convention
    //  fs:...  eax:edx   fp0      <= where they are

    subl LITERAL(20), %esp        // Padding & pass float result.
    fstpl (%esp)
    pushl %edx                    // Pass int result.
    pushl %eax
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    call SYMBOL(artQuickGenericJniEndTrampoline)

    // Pending exceptions possible.
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx
    testl %ebx, %ebx
    jnz .Lexception_in_native

    // Tear down the alloca.
    movl %ebp, %esp
    CFI_DEF_CFA_REGISTER(esp)


    // Tear down the callee-save frame.
    // Remove space for FPR args and EAX
    addl LITERAL(4 + 4 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(-(4 + 4 * 8))

    POP ecx
    addl LITERAL(4), %esp         // Avoid edx, as it may be part of the result.
    CFI_ADJUST_CFA_OFFSET(-4)
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    POP edi
    // Quick expects the return value to be in xmm0.
    movd %eax, %xmm0
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    ret
.Lexception_in_native:
    pushl %fs:THREAD_TOP_QUICK_FRAME_OFFSET
    addl LITERAL(-1), (%esp)  // Remove the GenericJNI tag.
    movl (%esp), %esp
    // Do a call to push a new save-all frame required by the runtime.
    call .Lexception_call
.Lexception_call:
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_generic_jni_trampoline

DEFINE_FUNCTION art_quick_to_interpreter_bridge
    SETUP_SAVE_REFS_AND_ARGS_FRAME  ebx, ebx  // save frame
    mov %esp, %edx                // remember SP
    PUSH eax                      // alignment padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax                      // pass  method
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    CFI_ADJUST_CFA_OFFSET(-16)

    // Return eax:edx in xmm0 also.
    movd %eax, %xmm0
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0

    addl LITERAL(48), %esp        // Remove FPRs and EAX, ECX, EDX, EBX.
    CFI_ADJUST_CFA_OFFSET(-48)

    POP ebp                       // Restore callee saves
    POP esi
    POP edi

    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Called by managed code, saves callee saves and then calls artInvokeObsoleteMethod
     */
ONE_ARG_RUNTIME_EXCEPTION art_invoke_obsolete_method_stub, artInvokeObsoleteMethod

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
    SETUP_SAVE_REFS_AND_ARGS_FRAME ebx, edx
    PUSH eax                      // Save eax which will be clobbered by the callee-save method.
    subl LITERAL(16), %esp        // Align stack (12 bytes) and reserve space for the SP argument
    CFI_ADJUST_CFA_OFFSET(16)     // (4 bytes). We lack the scratch registers to calculate the SP
                                  // right now, so we will just fill it in later.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                      // Pass receiver.
    PUSH eax                      // Pass Method*.
    leal 32(%esp), %eax           // Put original SP into eax
    movl %eax, 12(%esp)           // set SP
    call SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, SP)

    addl LITERAL(28), %esp        // Pop arguments upto saved Method*.
    CFI_ADJUST_CFA_OFFSET(-28)

    testl %eax, %eax
    jz 1f                         // Test for null return (indicating exception) and handle it.

    movl 60(%esp), %edi           // Restore edi.
    movl %eax, 60(%esp)           // Place code* over edi, just under return pc.
    movl SYMBOL(art_quick_instrumentation_exit)@GOT(%ebx), %ebx
    // Place instrumentation exit as return pc. ebx holds the GOT computed on entry.
    movl %ebx, 64(%esp)
    movl 0(%esp), %eax           // Restore eax.
    // Restore FPRs (extra 4 bytes of offset due to EAX push at top).
    movsd 8(%esp), %xmm0
    movsd 16(%esp), %xmm1
    movsd 24(%esp), %xmm2
    movsd 32(%esp), %xmm3

    // Restore GPRs.
    movl 40(%esp), %ecx           // Restore ecx.
    movl 44(%esp), %edx           // Restore edx.
    movl 48(%esp), %ebx           // Restore ebx.
    movl 52(%esp), %ebp           // Restore ebp.
    movl 56(%esp), %esi           // Restore esi.
    addl LITERAL(60), %esp        // Wind stack back upto code*.
    CFI_ADJUST_CFA_OFFSET(-60)
    ret                           // Call method (and pop).
1:
    // Make caller handle exception
    addl LITERAL(4), %esp
    CFI_ADJUST_CFA_OFFSET(-4)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION_CUSTOM_CFA art_quick_instrumentation_exit, 0
    pushl LITERAL(0)              // Push a fake return PC as there will be none on the stack.
    CFI_ADJUST_CFA_OFFSET(4)
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx

    movl %esp, %ecx               // Remember SP
    subl LITERAL(8), %esp         // Align stack.
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH edx                      // Save gpr return value. edx and eax need to be together,
                                  // which isn't the case in kSaveEverything frame.
    PUSH eax
    leal 32(%esp), %eax           // Get pointer to fpr_result, in kSaveEverything frame
    movl %esp, %edx               // Get pointer to gpr_result
    PUSH eax                      // Pass fpr_result
    PUSH edx                      // Pass gpr_result
    PUSH ecx                      // Pass SP
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current.
    CFI_ADJUST_CFA_OFFSET(4)

    call SYMBOL(artInstrumentationMethodExitFromCode)  // (Thread*, SP, gpr_result*, fpr_result*)
    // Return result could have been changed if it's a reference.
    movl 16(%esp), %ecx
    movl %ecx, (80+32)(%esp)
    addl LITERAL(32), %esp        // Pop arguments and grp_result.
    CFI_ADJUST_CFA_OFFSET(-32)

    testl %eax, %eax              // Check if we returned error.
    jz .Ldo_deliver_instrumentation_exception
    testl %edx, %edx
    jnz .Ldeoptimize
    // Normal return.
    movl %eax, FRAME_SIZE_SAVE_EVERYTHING-4(%esp)   // Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    ret
.Ldeoptimize:
    mov %edx, (FRAME_SIZE_SAVE_EVERYTHING-4)(%esp)  // Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    jmp SYMBOL(art_quick_deoptimize)
.Ldo_deliver_instrumentation_exception:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx
    subl LITERAL(12), %esp        // Align stack.
    CFI_ADJUST_CFA_OFFSET(12)
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    call SYMBOL(artDeoptimize)    // (Thread*)
    UNREACHABLE
END_FUNCTION art_quick_deoptimize

    /*
     * Compiled code has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the interpreter bridge.
     */
DEFINE_FUNCTION art_quick_deoptimize_from_compiled_code
    SETUP_SAVE_EVERYTHING_FRAME ebx, ebx
    subl LITERAL(8), %esp                      // Align stack.
    CFI_ADJUST_CFA_OFFSET(8)
    pushl %fs:THREAD_SELF_OFFSET                // Pass Thread::Current().
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH eax
    call SYMBOL(artDeoptimizeFromCompiledCode)  // (DeoptimizationKind, Thread*)
    UNREACHABLE
END_FUNCTION art_quick_deoptimize_from_compiled_code

    /*
     * String's compareTo.
     *
     * On entry:
     *    eax:   this string object (known non-null)
     *    ecx:   comp string object (known non-null)
     */
DEFINE_FUNCTION art_quick_string_compareto
    PUSH esi                      // push callee save reg
    PUSH edi                      // push callee save reg
    mov MIRROR_STRING_COUNT_OFFSET(%eax), %edx
    mov MIRROR_STRING_COUNT_OFFSET(%ecx), %ebx
    lea MIRROR_STRING_VALUE_OFFSET(%eax), %esi
    lea MIRROR_STRING_VALUE_OFFSET(%ecx), %edi
#if (STRING_COMPRESSION_FEATURE)
    /* Differ cases */
    shrl    LITERAL(1), %edx
    jnc     .Lstring_compareto_this_is_compressed
    shrl    LITERAL(1), %ebx
    jnc     .Lstring_compareto_that_is_compressed
    jmp     .Lstring_compareto_both_not_compressed
.Lstring_compareto_this_is_compressed:
    shrl    LITERAL(1), %ebx
    jnc     .Lstring_compareto_both_compressed
    /* If (this->IsCompressed() && that->IsCompressed() == false) */
    mov     %edx, %eax
    subl    %ebx, %eax
    mov     %edx, %ecx
    cmovg   %ebx, %ecx
    /* Going into loop to compare each character */
    jecxz   .Lstring_compareto_keep_length            // check loop counter (if 0, don't compare)
.Lstring_compareto_loop_comparison_this_compressed:
    movzbl  (%esi), %edx                              // move *(this_cur_char) byte to long
    movzwl  (%edi), %ebx                              // move *(that_cur_char) word to long
    addl    LITERAL(1), %esi                          // ++this_cur_char (8-bit)
    addl    LITERAL(2), %edi                          // ++that_cur_char (16-bit)
    subl    %ebx, %edx
    loope   .Lstring_compareto_loop_comparison_this_compressed
    cmovne  %edx, %eax                        // return eax = *(this_cur_char) - *(that_cur_char)
    jmp     .Lstring_compareto_return
.Lstring_compareto_that_is_compressed:
    mov     %edx, %eax
    subl    %ebx, %eax
    mov     %edx, %ecx
    cmovg   %ebx, %ecx
    /* If (this->IsCompressed() == false && that->IsCompressed()) */
    jecxz   .Lstring_compareto_keep_length            // check loop counter, if 0, don't compare
.Lstring_compareto_loop_comparison_that_compressed:
    movzwl  (%esi), %edx                              // move *(this_cur_char) word to long
    movzbl  (%edi), %ebx                              // move *(that_cur_char) byte to long
    addl    LITERAL(2), %esi                          // ++this_cur_char (16-bit)
    addl    LITERAL(1), %edi                          // ++that_cur_char (8-bit)
    subl    %ebx, %edx
    loope   .Lstring_compareto_loop_comparison_that_compressed
    cmovne  %edx, %eax
    jmp     .Lstring_compareto_return         // return eax = *(this_cur_char) - *(that_cur_char)
.Lstring_compareto_both_compressed:
    /* Calculate min length and count diff */
    mov     %edx, %ecx
    mov     %edx, %eax
    subl    %ebx, %eax
    cmovg   %ebx, %ecx
    jecxz   .Lstring_compareto_keep_length
    repe    cmpsb
    je      .Lstring_compareto_keep_length
    movzbl  -1(%esi), %eax        // get last compared char from this string (8-bit)
    movzbl  -1(%edi), %ecx        // get last compared char from comp string (8-bit)
    jmp     .Lstring_compareto_count_difference
#endif // STRING_COMPRESSION_FEATURE
.Lstring_compareto_both_not_compressed:
    /* Calculate min length and count diff */
    mov     %edx, %ecx
    mov     %edx, %eax
    subl    %ebx, %eax
    cmovg   %ebx, %ecx
    /*
     * At this point we have:
     *   eax: value to return if first part of strings are equal
     *   ecx: minimum among the lengths of the two strings
     *   esi: pointer to this string data
     *   edi: pointer to comp string data
     */
    jecxz .Lstring_compareto_keep_length
    repe  cmpsw                   // find nonmatching chars in [%esi] and [%edi], up to length %ecx
    je    .Lstring_compareto_keep_length
    movzwl  -2(%esi), %eax        // get last compared char from this string (16-bit)
    movzwl  -2(%edi), %ecx        // get last compared char from comp string (16-bit)
.Lstring_compareto_count_difference:
    subl    %ecx, %eax
.Lstring_compareto_keep_length:
.Lstring_compareto_return:
    POP edi                       // pop callee save reg
    POP esi                       // pop callee save reg
    ret
END_FUNCTION art_quick_string_compareto

// Create a function `name` calling the ReadBarrier::Mark routine,
// getting its argument and returning its result through register
// `reg`, saving and restoring all caller-save registers.
//
// If `reg` is different from `eax`, the generated function follows a
// non-standard runtime calling convention:
// - register `reg` is used to pass the (sole) argument of this function
//   (instead of EAX);
// - register `reg` is used to return the result of this function
//   (instead of EAX);
// - EAX is treated like a normal (non-argument) caller-save register;
// - everything else is the same as in the standard runtime calling
//   convention (e.g. standard callee-save registers are preserved).
MACRO2(READ_BARRIER_MARK_REG, name, reg)
    DEFINE_FUNCTION VAR(name)
    // Null check so that we can load the lock word.
    test REG_VAR(reg), REG_VAR(reg)
    jz .Lret_rb_\name
.Lnot_null_\name:
    // Check the mark bit, if it is 1 return.
    testl LITERAL(LOCK_WORD_MARK_BIT_MASK_SHIFTED), MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(reg))
    jz .Lslow_rb_\name
    ret
.Lslow_rb_\name:
    PUSH eax
    mov MIRROR_OBJECT_LOCK_WORD_OFFSET(REG_VAR(reg)), %eax
    add LITERAL(LOCK_WORD_STATE_FORWARDING_ADDRESS_OVERFLOW), %eax
    // Jump if overflow, the only case where it overflows should be the forwarding address one.
    // Taken ~25% of the time.
    jnae .Lret_forwarding_address\name

    // Save all potentially live caller-save core registers.
    mov 0(%esp), %eax
    PUSH ecx
    PUSH edx
    PUSH ebx
    // 8-byte align the stack to improve (8-byte) XMM register saving and restoring.
    // and create space for caller-save floating-point registers.
    subl MACRO_LITERAL(4 + 8 * 8), %esp
    CFI_ADJUST_CFA_OFFSET(4 + 8 * 8)
    // Save all potentially live caller-save floating-point registers.
    movsd %xmm0, 0(%esp)
    movsd %xmm1, 8(%esp)
    movsd %xmm2, 16(%esp)
    movsd %xmm3, 24(%esp)
    movsd %xmm4, 32(%esp)
    movsd %xmm5, 40(%esp)
    movsd %xmm6, 48(%esp)
    movsd %xmm7, 56(%esp)

    subl LITERAL(4), %esp            // alignment padding
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH RAW_VAR(reg)                // pass arg1 - obj from `reg`
    call SYMBOL(artReadBarrierMark)  // artReadBarrierMark(obj)
    .ifnc RAW_VAR(reg), eax
      movl %eax, REG_VAR(reg)        // return result into `reg`
    .endif
    addl LITERAL(8), %esp            // pop argument and remove padding
    CFI_ADJUST_CFA_OFFSET(-8)

    // Restore floating-point registers.
    movsd 0(%esp), %xmm0
    movsd 8(%esp), %xmm1
    movsd 16(%esp), %xmm2
    movsd 24(%esp), %xmm3
    movsd 32(%esp), %xmm4
    movsd 40(%esp), %xmm5
    movsd 48(%esp), %xmm6
    movsd 56(%esp), %xmm7
    // Remove floating-point registers and padding.
    addl MACRO_LITERAL(8 * 8 + 4), %esp
    CFI_ADJUST_CFA_OFFSET(-(8 * 8 + 4))
    // Restore core regs, except `reg`, as it is used to return the
    // result of this function (simply remove it from the stack instead).
    POP_REG_NE ebx, RAW_VAR(reg)
    POP_REG_NE edx, RAW_VAR(reg)
    POP_REG_NE ecx, RAW_VAR(reg)
    POP_REG_NE eax, RAW_VAR(reg)
.Lret_rb_\name:
    ret
.Lret_forwarding_address\name:
    // The overflow cleared the top bits.
    sall LITERAL(LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT), %eax
    mov %eax, REG_VAR(reg)
    POP_REG_NE eax, RAW_VAR(reg)
    ret
    END_FUNCTION VAR(name)
END_MACRO

READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg00, eax
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg01, ecx
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg02, edx
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg03, ebx
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg05, ebp
// Note: There is no art_quick_read_barrier_mark_reg04, as register 4 (ESP)
// cannot be used to pass arguments.
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg06, esi
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg07, edi

DEFINE_FUNCTION art_quick_read_barrier_slow
    PUSH edx                         // pass arg3 - offset
    PUSH ecx                         // pass arg2 - obj
    PUSH eax                         // pass arg1 - ref
    call SYMBOL(artReadBarrierSlow)  // artReadBarrierSlow(ref, obj, offset)
    addl LITERAL(12), %esp           // pop arguments
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_read_barrier_slow

DEFINE_FUNCTION art_quick_read_barrier_for_root_slow
    subl LITERAL(8), %esp                   // alignment padding
    CFI_ADJUST_CFA_OFFSET(8)
    PUSH eax                                // pass arg1 - root
    call SYMBOL(artReadBarrierForRootSlow)  // artReadBarrierForRootSlow(root)
    addl LITERAL(12), %esp                  // pop argument and remove padding
    CFI_ADJUST_CFA_OFFSET(-12)
    ret
END_FUNCTION art_quick_read_barrier_for_root_slow

  /*
     * On stack replacement stub.
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = stack to copy
     *   [sp + 8] = size of stack
     *   [sp + 12] = pc to call
     *   [sp + 16] = JValue* result
     *   [sp + 20] = shorty
     *   [sp + 24] = thread
     */
DEFINE_FUNCTION art_quick_osr_stub
    // Save native callee saves.
    PUSH ebp
    PUSH ebx
    PUSH esi
    PUSH edi
    SAVE_SIZE=20                   // 4 registers and the return address
    mov 4+16(%esp), %esi           // ESI = argument array
    mov 8+16(%esp), %ecx           // ECX = size of args
    mov 12+16(%esp), %ebx          // EBX = pc to call
    mov %esp, %ebp                 // Save stack pointer
    CFI_DEF_CFA(ebp, SAVE_SIZE)    // CFA = ebp + SAVE_SIZE
    CFI_REMEMBER_STATE
    andl LITERAL(0xFFFFFFF0), %esp // Align stack
    pushl %ebp                     // Save old stack pointer
    subl LITERAL(12), %esp         // Align stack
    movl LITERAL(0), (%esp)        // Store null for ArtMethod* slot
    // ebp isn't properly spilled in the osr method, so we need use DWARF expression.
    // NB: the CFI must be before the call since this is the address gdb will lookup.
    // NB: gdb expects that cfa_expression returns the CFA value (not address to it).
    CFI_ESCAPE(                    /* cfa = [sp + 12] + SAVE_SIZE */ \
      0x0f, 6,                     /* DW_CFA_def_cfa_expression(len) */ \
      0x92, 4, 12,                 /* DW_OP_bregx(reg,offset) */ \
      0x06,                        /* DW_OP_deref */ \
      0x23, SAVE_SIZE)             /* DW_OP_plus_uconst(val) */
    call .Losr_entry
    mov 12(%esp), %esp             // Restore stack pointer.
    CFI_DEF_CFA(esp, SAVE_SIZE)    // CFA = esp + SAVE_SIZE

    // Restore callee saves.
    POP edi
    POP esi
    POP ebx
    POP ebp
    mov 16(%esp), %ecx            // Get JValue result
    mov %eax, (%ecx)              // Store the result assuming it is a long, int or Object*
    mov %edx, 4(%ecx)             // Store the other half of the result
    mov 20(%esp), %edx            // Get the shorty
    cmpb LITERAL(68), (%edx)      // Test if result type char == 'D'
    je .Losr_return_double_quick
    cmpb LITERAL(70), (%edx)      // Test if result type char == 'F'
    je .Losr_return_float_quick
    ret
.Losr_return_double_quick:
    movsd %xmm0, (%ecx)           // Store the floating point result
    ret
.Losr_return_float_quick:
    movss %xmm0, (%ecx)           // Store the floating point result
    ret
.Losr_entry:
    CFI_RESTORE_STATE
    CFI_DEF_CFA(ebp, SAVE_SIZE)   // CFA = ebp + SAVE_SIZE
    subl LITERAL(4), %ecx         // Given stack size contains pushed frame pointer, substract it.
    subl %ecx, %esp
    mov %esp, %edi                // EDI = beginning of stack
    rep movsb                     // while (ecx--) { *edi++ = *esi++ }
    jmp *%ebx
END_FUNCTION art_quick_osr_stub

DEFINE_FUNCTION art_quick_invoke_polymorphic
    SETUP_SAVE_REFS_AND_ARGS_FRAME  ebx, ebx       // Save frame.
    mov %esp, %edx                                 // Remember SP.
    subl LITERAL(16), %esp                         // Make space for JValue result.
    CFI_ADJUST_CFA_OFFSET(16)
    movl LITERAL(0), (%esp)                        // Initialize result to zero.
    movl LITERAL(0), 4(%esp)
    mov %esp, %eax                                 // Store pointer to JValue result in eax.
    PUSH edx                                       // pass SP
    pushl %fs:THREAD_SELF_OFFSET                   // pass Thread::Current()
    CFI_ADJUST_CFA_OFFSET(4)
    PUSH ecx                                       // pass receiver (method handle)
    PUSH eax                                       // pass JResult
    call SYMBOL(artInvokePolymorphic)              // artInvokePolymorphic(result, receiver, Thread*, SP)
    subl LITERAL('A'), %eax                        // Eliminate out of bounds options
    cmpb LITERAL('Z' - 'A'), %al
    ja .Lcleanup_and_return
    movzbl %al, %eax
    call .Lput_eip_in_ecx
.Lbranch_start:
    movl %ecx, %edx
    add $(.Lhandler_table - .Lbranch_start), %edx  // Make EDX point to handler_table.
    leal (%edx, %eax, 2), %eax                     // Calculate address of entry in table.
    movzwl (%eax), %eax                            // Lookup relative branch in table.
    addl %ecx, %eax                                // Add EIP relative offset.
    jmp *%eax                                      // Branch to handler.

    // Handlers for different return types.
.Lstore_boolean_result:
    movzbl 16(%esp), %eax                          // Copy boolean result to the accumulator.
    jmp .Lcleanup_and_return
.Lstore_char_result:
    movzwl 16(%esp), %eax                          // Copy char result to the accumulator.
    jmp .Lcleanup_and_return
.Lstore_float_result:
    movd 16(%esp), %xmm0                           // Copy float result to the context restored by
    movd %xmm0, 36(%esp)                           // RESTORE_SAVE_REFS_ONLY_FRAME.
    jmp .Lcleanup_and_return
.Lstore_double_result:
    movsd 16(%esp), %xmm0                          // Copy double result to the context restored by
    movsd %xmm0, 36(%esp)                          // RESTORE_SAVE_REFS_ONLY_FRAME.
    jmp .Lcleanup_and_return
.Lstore_long_result:
    movl 20(%esp), %edx                            // Copy upper-word of result to the context restored by
    movl %edx, 72(%esp)                            // RESTORE_SAVE_REFS_ONLY_FRAME.
    // Fall-through for lower bits.
.Lstore_int_result:
    movl 16(%esp), %eax                            // Copy int result to the accumulator.
    // Fall-through to clean up and return.
.Lcleanup_and_return:
    addl LITERAL(32), %esp                         // Pop arguments and stack allocated JValue result.
    CFI_ADJUST_CFA_OFFSET(-32)
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    RETURN_OR_DELIVER_PENDING_EXCEPTION

.Lput_eip_in_ecx:                                  // Internal function that puts address of
    movl 0(%esp), %ecx                             // next instruction into ECX when CALL
    ret

    // Handler table to handlers for given type.
.Lhandler_table:
MACRO1(HANDLER_TABLE_ENTRY, handler_label)
    // NB some tools require 16-bits for relocations. Shouldn't need adjusting.
    .word RAW_VAR(handler_label) - .Lbranch_start
END_MACRO
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // A
    HANDLER_TABLE_ENTRY(.Lstore_int_result)        // B (byte)
    HANDLER_TABLE_ENTRY(.Lstore_char_result)       // C (char)
    HANDLER_TABLE_ENTRY(.Lstore_double_result)     // D (double)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // E
    HANDLER_TABLE_ENTRY(.Lstore_float_result)      // F (float)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // G
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // H
    HANDLER_TABLE_ENTRY(.Lstore_int_result)        // I (int)
    HANDLER_TABLE_ENTRY(.Lstore_long_result)       // J (long)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // K
    HANDLER_TABLE_ENTRY(.Lstore_int_result)        // L (object)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // M
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // N
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // O
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // P
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // Q
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // R
    HANDLER_TABLE_ENTRY(.Lstore_int_result)        // S (short)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // T
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // U
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // V (void)
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // W
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // X
    HANDLER_TABLE_ENTRY(.Lcleanup_and_return)      // Y
    HANDLER_TABLE_ENTRY(.Lstore_boolean_result)    // Z (boolean)

END_FUNCTION art_quick_invoke_polymorphic

// Wrap ExecuteSwitchImpl in assembly method which specifies DEX PC for unwinding.
//  Argument 0: ESP+4: The context pointer for ExecuteSwitchImpl.
//  Argument 1: ESP+8: Pointer to the templated ExecuteSwitchImpl to call.
//  Argument 2: ESP+12: The value of DEX PC (memory address of the methods bytecode).
DEFINE_FUNCTION ExecuteSwitchImplAsm
    PUSH ebx                 // Spill EBX; Increments ESP, so arg0 is at ESP+8 now.
    mov 12(%esp), %eax       // EAX = C++ templated interpreter function
    mov 16(%esp), %ebx       // EBX = DEX PC (callee save register)
    mov 8(%esp), %ecx        // ECX = Context argument for the function
    CFI_DEFINE_DEX_PC_WITH_OFFSET(0 /* EAX */, 3 /* EBX */, 0)

    sub LITERAL(4), %esp     // Alignment padding
    CFI_ADJUST_CFA_OFFSET(4)
    push %ecx                // Push argument
    CFI_ADJUST_CFA_OFFSET(4)
    call *%eax               // Call the wrapped function
    addl LITERAL(8), %esp
    CFI_ADJUST_CFA_OFFSET(-8)

    POP ebx                  // Restore EBX
    ret
END_FUNCTION ExecuteSwitchImplAsm

    // TODO: implement these!
UNIMPLEMENTED art_quick_memcmp16
