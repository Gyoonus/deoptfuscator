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

#include "asm_support_arm.S"
#include "interpreter/cfi_asm_support.h"

#include "arch/quick_alloc_entrypoints.S"

    /* Deliver the given exception */
    .extern artDeliverExceptionFromCode
    /* Deliver an exception pending on a thread */
    .extern artDeliverPendingException

    /*
     * Macro to spill the GPRs.
     */
.macro SPILL_ALL_CALLEE_SAVE_GPRS
    push {r4-r11, lr}                             @ 9 words (36 bytes) of callee saves.
    .cfi_adjust_cfa_offset 36
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r5, 4
    .cfi_rel_offset r6, 8
    .cfi_rel_offset r7, 12
    .cfi_rel_offset r8, 16
    .cfi_rel_offset r9, 20
    .cfi_rel_offset r10, 24
    .cfi_rel_offset r11, 28
    .cfi_rel_offset lr, 32
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVES_FRAME rTemp
    SPILL_ALL_CALLEE_SAVE_GPRS                    @ 9 words (36 bytes) of callee saves.
    vpush {s16-s31}                               @ 16 words (64 bytes) of floats.
    .cfi_adjust_cfa_offset 64
    sub sp, #12                                   @ 3 words of space, bottom word will hold Method*
    .cfi_adjust_cfa_offset 12
    RUNTIME_CURRENT1 \rTemp                       @ Load Runtime::Current into rTemp.
    @ Load kSaveAllCalleeSaves Method* into rTemp.
    ldr \rTemp, [\rTemp, #RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET]
    str \rTemp, [sp, #0]                          @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.

     // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 36 + 64 + 12)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(ARM) size not as expected."
#endif
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly).
     */
.macro SETUP_SAVE_REFS_ONLY_FRAME rTemp
    // Note: We could avoid saving R8 in the case of Baker read
    // barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    push {r5-r8, r10-r11, lr}                     @ 7 words of callee saves
    .cfi_adjust_cfa_offset 28
    .cfi_rel_offset r5, 0
    .cfi_rel_offset r6, 4
    .cfi_rel_offset r7, 8
    .cfi_rel_offset r8, 12
    .cfi_rel_offset r10, 16
    .cfi_rel_offset r11, 20
    .cfi_rel_offset lr, 24
    sub sp, #4                                    @ bottom word will hold Method*
    .cfi_adjust_cfa_offset 4
    RUNTIME_CURRENT2 \rTemp                       @ Load Runtime::Current into rTemp.
    @ Load kSaveRefsOnly Method* into rTemp.
    ldr \rTemp, [\rTemp, #RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET]
    str \rTemp, [sp, #0]                          @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 28 + 4)
#error "FRAME_SIZE_SAVE_REFS_ONLY(ARM) size not as expected."
#endif
.endm

.macro RESTORE_SAVE_REFS_ONLY_FRAME
    add sp, #4               @ bottom word holds Method*
    .cfi_adjust_cfa_offset -4
    // Note: Likewise, we could avoid restoring R8 in the case of Baker
    // read barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    pop {r5-r8, r10-r11, lr} @ 7 words of callee saves
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore lr
    .cfi_adjust_cfa_offset -28
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs).
     */
.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_REGISTERS_ONLY
    // Note: We could avoid saving R8 in the case of Baker read
    // barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    push {r1-r3, r5-r8, r10-r11, lr}   @ 10 words of callee saves and args.
    .cfi_adjust_cfa_offset 40
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    .cfi_rel_offset r3, 8
    .cfi_rel_offset r5, 12
    .cfi_rel_offset r6, 16
    .cfi_rel_offset r7, 20
    .cfi_rel_offset r8, 24
    .cfi_rel_offset r10, 28
    .cfi_rel_offset r11, 32
    .cfi_rel_offset lr, 36
    vpush {s0-s15}                     @ 16 words of float args.
    .cfi_adjust_cfa_offset 64
    sub sp, #8                         @ 2 words of space, alignment padding and Method*
    .cfi_adjust_cfa_offset 8
    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 40 + 64 + 8)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(ARM) size not as expected."
#endif
.endm

.macro SETUP_SAVE_REFS_AND_ARGS_FRAME rTemp
    SETUP_SAVE_REFS_AND_ARGS_FRAME_REGISTERS_ONLY
    RUNTIME_CURRENT3 \rTemp                       @ Load Runtime::Current into rTemp.
    @ Load kSaveRefsAndArgs Method* into rTemp.
    ldr \rTemp, [\rTemp, #RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET]
    str \rTemp, [sp, #0]                          @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.
.endm

.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_R0
    SETUP_SAVE_REFS_AND_ARGS_FRAME_REGISTERS_ONLY
    str r0, [sp, #0]                              @ Store ArtMethod* to bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.
.endm

.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME
    add  sp, #8                      @ rewind sp
    .cfi_adjust_cfa_offset -8
    vpop {s0-s15}
    .cfi_adjust_cfa_offset -64
    // Note: Likewise, we could avoid restoring X20 in the case of Baker
    // read barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    pop {r1-r3, r5-r8, r10-r11, lr}  @ 10 words of callee saves
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore lr
    .cfi_adjust_cfa_offset -40
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when core registers are already saved.
     */
.macro SETUP_SAVE_EVERYTHING_FRAME_CORE_REGS_SAVED rTemp, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
                                        @ 14 words of callee saves and args already saved.
    vpush {d0-d15}                      @ 32 words, 2 for each of the 16 saved doubles.
    .cfi_adjust_cfa_offset 128
    sub sp, #8                          @ 2 words of space, alignment padding and Method*
    .cfi_adjust_cfa_offset 8
    RUNTIME_CURRENT1 \rTemp             @ Load Runtime::Current into rTemp.
    @ Load kSaveEverything Method* into rTemp.
    ldr \rTemp, [\rTemp, #\runtime_method_offset]
    str \rTemp, [sp, #0]                @ Place Method* at bottom of stack.
    str sp, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]  @ Place sp in Thread::Current()->top_quick_frame.

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_EVERYTHING != 56 + 128 + 8)
#error "FRAME_SIZE_SAVE_EVERYTHING(ARM) size not as expected."
#endif
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     */
.macro SETUP_SAVE_EVERYTHING_FRAME rTemp, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    push {r0-r12, lr}                   @ 14 words of callee saves and args.
    .cfi_adjust_cfa_offset 56
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset r4, 16
    .cfi_rel_offset r5, 20
    .cfi_rel_offset r6, 24
    .cfi_rel_offset r7, 28
    .cfi_rel_offset r8, 32
    .cfi_rel_offset r9, 36
    .cfi_rel_offset r10, 40
    .cfi_rel_offset r11, 44
    .cfi_rel_offset ip, 48
    .cfi_rel_offset lr, 52
    SETUP_SAVE_EVERYTHING_FRAME_CORE_REGS_SAVED \rTemp, \runtime_method_offset
.endm

.macro RESTORE_SAVE_EVERYTHING_FRAME
    add  sp, #8                         @ rewind sp
    .cfi_adjust_cfa_offset -8
    vpop {d0-d15}
    .cfi_adjust_cfa_offset -128
    pop {r0-r12, lr}                    @ 14 words of callee saves
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r4
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r9
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore r12
    .cfi_restore lr
    .cfi_adjust_cfa_offset -56
.endm

.macro RESTORE_SAVE_EVERYTHING_FRAME_KEEP_R0
    add  sp, #8                         @ rewind sp
    .cfi_adjust_cfa_offset -8
    vpop {d0-d15}
    .cfi_adjust_cfa_offset -128
    add  sp, #4                         @ skip r0
    .cfi_adjust_cfa_offset -4
    .cfi_restore r0                     @ debugger can no longer restore caller's r0
    pop {r1-r12, lr}                    @ 13 words of callee saves
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r4
    .cfi_restore r5
    .cfi_restore r6
    .cfi_restore r7
    .cfi_restore r8
    .cfi_restore r9
    .cfi_restore r10
    .cfi_restore r11
    .cfi_restore r12
    .cfi_restore lr
    .cfi_adjust_cfa_offset -52
.endm

// Macro to refresh the Marking Register (R8).
//
// This macro must be called at the end of functions implementing
// entrypoints that possibly (directly or indirectly) perform a
// suspend check (before they return).
.macro REFRESH_MARKING_REGISTER
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
    ldr rMR, [rSELF, #THREAD_IS_GC_MARKING_OFFSET]
#endif
.endm

.macro RETURN_IF_RESULT_IS_ZERO
    cbnz   r0, 1f              @ result non-zero branch over
    bx     lr                  @ return
1:
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO
    cbz    r0, 1f              @ result zero branch over
    bx     lr                  @ return
1:
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     */
.macro DELIVER_PENDING_EXCEPTION_FRAME_READY
    mov    r0, r9                              @ pass Thread::Current
    bl     artDeliverPendingExceptionFromCode  @ artDeliverPendingExceptionFromCode(Thread*)
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
.macro DELIVER_PENDING_EXCEPTION
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME r0       @ save callee saves for throw
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.endm

.macro NO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME r0       @ save all registers as basis for long jump context
    mov r0, r9                      @ pass Thread::Current
    bl  \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_EVERYTHING_FRAME r0  @ save all registers as basis for long jump context
    mov r0, r9                      @ pass Thread::Current
    bl  \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro ONE_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME r1       @ save all registers as basis for long jump context
    mov r1, r9                      @ pass Thread::Current
    bl  \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_EVERYTHING_FRAME r2  @ save all registers as basis for long jump context
    mov r2, r9                      @ pass Thread::Current
    bl  \cxx_name                   @ \cxx_name(Thread*)
END \c_name
.endm

.macro  RETURN_OR_DELIVER_PENDING_EXCEPTION_REG reg
    ldr \reg, [r9, #THREAD_EXCEPTION_OFFSET]   // Get exception field.
    cbnz \reg, 1f
    bx lr
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro  RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    RETURN_OR_DELIVER_PENDING_EXCEPTION_REG r1
.endm

.macro RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
    RETURN_IF_RESULT_IS_NON_ZERO
    DELIVER_PENDING_EXCEPTION
.endm

// Macros taking opportunity of code similarities for downcalls.
.macro  ONE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r1        @ save callee saves in case of GC
    mov    r1, r9                        @ pass Thread::Current
    bl     \entrypoint                   @ (uint32_t field_idx, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

.macro  TWO_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r2        @ save callee saves in case of GC
    mov    r2, r9                        @ pass Thread::Current
    bl     \entrypoint                   @ (field_idx, Object*, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

.macro THREE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r3        @ save callee saves in case of GC
    mov    r3, r9                        @ pass Thread::Current
    bl     \entrypoint                   @ (field_idx, Object*, new_val, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME         @ TODO: we can clearly save an add here
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Call installed by a signal handler to create and deliver a NullPointerException.
     */
    .extern art_quick_throw_null_pointer_exception_from_signal
ENTRY art_quick_throw_null_pointer_exception_from_signal
    // The fault handler pushes the gc map address, i.e. "return address", to stack
    // and passes the fault address in LR. So we need to set up the CFI info accordingly.
    .cfi_def_cfa_offset __SIZEOF_POINTER__
    .cfi_rel_offset lr, 0
    push {r0-r12}                   @ 13 words of callee saves and args; LR already saved.
    .cfi_adjust_cfa_offset 52
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset r4, 16
    .cfi_rel_offset r5, 20
    .cfi_rel_offset r6, 24
    .cfi_rel_offset r7, 28
    .cfi_rel_offset r8, 32
    .cfi_rel_offset r9, 36
    .cfi_rel_offset r10, 40
    .cfi_rel_offset r11, 44
    .cfi_rel_offset ip, 48

    @ save all registers as basis for long jump context
    SETUP_SAVE_EVERYTHING_FRAME_CORE_REGS_SAVED r1
    mov r0, lr                      @ pass the fault address stored in LR by the fault handler.
    mov r1, r9                      @ pass Thread::Current
    bl  artThrowNullPointerExceptionFromSignal  @ (Thread*)
END art_quick_throw_null_pointer_exception_from_signal

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING art_quick_throw_div_zero, artThrowDivZeroFromCode

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
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/r1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in r0/r1 consisting
     * of the target Method* in r0 and method->code_ in r1.
     *
     * If unsuccessful, the helper will return null/null. There will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     *
     * Clobbers IP (R12).
     */
.macro INVOKE_TRAMPOLINE_BODY cxx_name
    .extern \cxx_name
    SETUP_SAVE_REFS_AND_ARGS_FRAME r2     @ save callee saves in case allocation triggers GC
    mov    r2, r9                         @ pass Thread::Current
    mov    r3, sp
    bl     \cxx_name                      @ (method_idx, this, Thread*, SP)
    mov    r12, r1                        @ save Method*->code_
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    cbz    r0, 1f                         @ did we find the target? if not go to exception delivery
    bx     r12                            @ tail call to target
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

    /*
     * Quick invocation stub internal.
     * On entry:
     *   r0 = method pointer
     *   r1 = argument array or null for no argument methods
     *   r2 = size of argument array in bytes
     *   r3 = (managed) thread pointer
     *   [sp] = JValue* result
     *   [sp + 4] = result_in_float
     *   [sp + 8] = core register argument array
     *   [sp + 12] = fp register argument array
     *  +-------------------------+
     *  | uint32_t* fp_reg_args   |
     *  | uint32_t* core_reg_args |
     *  |   result_in_float       | <- Caller frame
     *  |   Jvalue* result        |
     *  +-------------------------+
     *  |          lr             |
     *  |          r11            |
     *  |          r9             |
     *  |          r4             | <- r11
     *  +-------------------------+
     *  | uint32_t out[n-1]       |
     *  |    :      :             |        Outs
     *  | uint32_t out[0]         |
     *  | StackRef<ArtMethod>     | <- SP  value=null
     *  +-------------------------+
     */
ENTRY art_quick_invoke_stub_internal
    SPILL_ALL_CALLEE_SAVE_GPRS             @ spill regs (9)
    mov    r11, sp                         @ save the stack pointer
    .cfi_def_cfa_register r11

    mov    r9, r3                          @ move managed thread pointer into r9

    add    r4, r2, #4                      @ create space for method pointer in frame
    sub    r4, sp, r4                      @ reserve & align *stack* to 16 bytes: native calling
    and    r4, #0xFFFFFFF0                 @ convention only aligns to 8B, so we have to ensure ART
    mov    sp, r4                          @ 16B alignment ourselves.

    mov    r4, r0                          @ save method*
    add    r0, sp, #4                      @ pass stack pointer + method ptr as dest for memcpy
    bl     memcpy                          @ memcpy (dest, src, bytes)
    mov    ip, #0                          @ set ip to 0
    str    ip, [sp]                        @ store null for method* at bottom of frame

    ldr    ip, [r11, #48]                  @ load fp register argument array pointer
    vldm   ip, {s0-s15}                    @ copy s0 - s15

    ldr    ip, [r11, #44]                  @ load core register argument array pointer
    mov    r0, r4                          @ restore method*
    add    ip, ip, #4                      @ skip r0
    ldm    ip, {r1-r3}                     @ copy r1 - r3

    REFRESH_MARKING_REGISTER

    ldr    ip, [r0, #ART_METHOD_QUICK_CODE_OFFSET_32]  @ get pointer to the code
    blx    ip                              @ call the method

    mov    sp, r11                         @ restore the stack pointer
    .cfi_def_cfa_register sp

    ldr    r4, [sp, #40]                   @ load result_is_float
    ldr    r9, [sp, #36]                   @ load the result pointer
    cmp    r4, #0
    ite    eq
    strdeq r0, [r9]                        @ store r0/r1 into result pointer
    vstrne d0, [r9]                        @ store s0-s1/d0 into result pointer

    pop    {r4, r5, r6, r7, r8, r9, r10, r11, pc}               @ restore spill regs
END art_quick_invoke_stub_internal

    /*
     * On stack replacement stub.
     * On entry:
     *   r0 = stack to copy
     *   r1 = size of stack
     *   r2 = pc to call
     *   r3 = JValue* result
     *   [sp] = shorty
     *   [sp + 4] = thread
     */
ENTRY art_quick_osr_stub
    SPILL_ALL_CALLEE_SAVE_GPRS             @ Spill regs (9)
    SAVE_SIZE=9*4
    mov    r11, sp                         @ Save the stack pointer
    .cfi_def_cfa r11, SAVE_SIZE            @ CFA = r11 + SAVE_SIZE
    .cfi_remember_state
    mov    r10, r1                         @ Save size of stack
    ldr    r9, [r11, #40]                  @ Move managed thread pointer into r9
    REFRESH_MARKING_REGISTER
    mov    r6, r2                          @ Save the pc to call
    sub    r7, sp, #12                     @ Reserve space for stack pointer,
                                           @    JValue* result, and ArtMethod* slot.
    and    r7, #0xFFFFFFF0                 @ Align stack pointer
    mov    sp, r7                          @ Update stack pointer
    str    r11, [sp, #4]                   @ Save old stack pointer
    str    r3, [sp, #8]                    @ Save JValue* result
    mov    ip, #0
    str    ip, [sp]                        @ Store null for ArtMethod* at bottom of frame
    // r11 isn't properly spilled in the osr method, so we need use DWARF expression.
    // NB: the CFI must be before the call since this is the address gdb will lookup.
    // NB: gdb expects that cfa_expression returns the CFA value (not address to it).
    .cfi_escape                            /* CFA = [sp + 4] + SAVE_SIZE */ \
      0x0f, 6,                             /* DW_CFA_def_cfa_expression(len) */ \
      0x92, 13, 4,                         /* DW_OP_bregx(reg,offset) */ \
      0x06,                                /* DW_OP_deref */ \
      0x23, SAVE_SIZE                      /* DW_OP_plus_uconst(val) */
    bl     .Losr_entry                     @ Call the method
    ldr    r10, [sp, #8]                   @ Restore JValue* result
    ldr    sp, [sp, #4]                    @ Restore saved stack pointer
    .cfi_def_cfa sp, SAVE_SIZE             @ CFA = sp + SAVE_SIZE
    ldr    r4, [sp, #36]                   @ load shorty
    ldrb   r4, [r4, #0]                    @ load return type
    cmp    r4, #68                         @ Test if result type char == 'D'.
    beq    .Losr_fp_result
    cmp    r4, #70                         @ Test if result type char == 'F'.
    beq    .Losr_fp_result
    strd r0, [r10]                         @ Store r0/r1 into result pointer
    b    .Losr_exit
.Losr_fp_result:
    vstr d0, [r10]                         @ Store s0-s1/d0 into result pointer
.Losr_exit:
    pop    {r4, r5, r6, r7, r8, r9, r10, r11, pc}
.Losr_entry:
    .cfi_restore_state
    .cfi_def_cfa r11, SAVE_SIZE            @ CFA = r11 + SAVE_SIZE
    sub sp, sp, r10                        @ Reserve space for callee stack
    sub r10, r10, #4
    str lr, [sp, r10]                      @ Store link register per the compiler ABI
    mov r2, r10
    mov r1, r0
    mov r0, sp
    bl  memcpy                             @ memcpy (dest r0, src r1, bytes r2)
    bx r6
END art_quick_osr_stub

    /*
     * On entry r0 is uint32_t* gprs_ and r1 is uint32_t* fprs_
     */
ARM_ENTRY art_quick_do_long_jump
    vldm r1, {s0-s31}     @ load all fprs from argument fprs_
    ldr  r2, [r0, #60]    @ r2 = r15 (PC from gprs_ 60=4*15)
    ldr  r14, [r0, #56]   @ (LR from gprs_ 56=4*14)
    add  r0, r0, #12      @ increment r0 to skip gprs_[0..2] 12=4*3
    ldm  r0, {r3-r13}     @ load remaining gprs from argument gprs_
    REFRESH_MARKING_REGISTER
    ldr  r0, [r0, #-12]   @ load r0 value
    mov  r1, #0           @ clear result register r1
    bx   r2               @ do long jump
END art_quick_do_long_jump

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and delivers exception on
     * failure.
     */
TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC. r0 holds the
     * possibly null object to lock.
     */
    .extern artLockObjectFromCode
ENTRY art_quick_lock_object
    cbz    r0, .Lslow_lock
.Lretry_lock:
    ldr    r2, [r9, #THREAD_ID_OFFSET]
    ldrex  r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    mov    r3, r1
    and    r3, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  @ zero the gc bits
    cbnz   r3, .Lnot_unlocked         @ already thin locked
    @ unlocked case - r1: original lock word that's zero except for the read barrier bits.
    orr    r2, r1, r2                 @ r2 holds thread id with count of 0 with preserved read barrier bits
    strex  r3, r2, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    cbnz   r3, .Llock_strex_fail      @ store failed, retry
    dmb    ish                        @ full (LoadLoad|LoadStore) memory barrier
    bx lr
.Lnot_unlocked:  @ r1: original lock word, r2: thread_id with count of 0 and zero read barrier bits
    lsr    r3, r1, LOCK_WORD_STATE_SHIFT
    cbnz   r3, .Lslow_lock            @ if either of the top two bits are set, go slow path
    eor    r2, r1, r2                 @ lock_word.ThreadId() ^ self->ThreadId()
    uxth   r2, r2                     @ zero top 16 bits
    cbnz   r2, .Lslow_lock            @ lock word and self thread id's match -> recursive lock
                                      @ else contention, go to slow path
    mov    r3, r1                     @ copy the lock word to check count overflow.
    and    r3, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  @ zero the gc bits.
    add    r2, r3, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ increment count in lock word placing in r2 to check overflow
    lsr    r3, r2, #LOCK_WORD_GC_STATE_SHIFT    @ if the first gc state bit is set, we overflowed.
    cbnz   r3, .Lslow_lock            @ if we overflow the count go slow path
    add    r2, r1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ increment count for real
    strex  r3, r2, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET] @ strex necessary for read barrier bits
    cbnz   r3, .Llock_strex_fail      @ strex failed, retry
    bx lr
.Llock_strex_fail:
    b      .Lretry_lock               @ retry
.Lslow_lock:
    SETUP_SAVE_REFS_ONLY_FRAME r1     @ save callee saves in case we block
    mov    r1, r9                     @ pass Thread::Current
    bl     artLockObjectFromCode      @ (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_lock_object

ENTRY art_quick_lock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME r1     @ save callee saves in case we block
    mov    r1, r9                     @ pass Thread::Current
    bl     artLockObjectFromCode      @ (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_lock_object_no_inline

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     * r0 holds the possibly null object to lock.
     */
    .extern artUnlockObjectFromCode
ENTRY art_quick_unlock_object
    cbz    r0, .Lslow_unlock
.Lretry_unlock:
#ifndef USE_READ_BARRIER
    ldr    r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    ldrex  r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ Need to use atomic instructions for read barrier
#endif
    lsr    r2, r1, #LOCK_WORD_STATE_SHIFT
    cbnz   r2, .Lslow_unlock          @ if either of the top two bits are set, go slow path
    ldr    r2, [r9, #THREAD_ID_OFFSET]
    mov    r3, r1                     @ copy lock word to check thread id equality
    and    r3, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  @ zero the gc bits
    eor    r3, r3, r2                 @ lock_word.ThreadId() ^ self->ThreadId()
    uxth   r3, r3                     @ zero top 16 bits
    cbnz   r3, .Lslow_unlock          @ do lock word and self thread id's match?
    mov    r3, r1                     @ copy lock word to detect transition to unlocked
    and    r3, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  @ zero the gc bits
    cmp    r3, #LOCK_WORD_THIN_LOCK_COUNT_ONE
    bpl    .Lrecursive_thin_unlock
    @ transition to unlocked
    mov    r3, r1
    and    r3, #LOCK_WORD_GC_STATE_MASK_SHIFTED  @ r3: zero except for the preserved gc bits
    dmb    ish                        @ full (LoadStore|StoreStore) memory barrier
#ifndef USE_READ_BARRIER
    str    r3, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    strex  r2, r3, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ strex necessary for read barrier bits
    cbnz   r2, .Lunlock_strex_fail    @ store failed, retry
#endif
    bx     lr
.Lrecursive_thin_unlock:  @ r1: original lock word
    sub    r1, r1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  @ decrement count
#ifndef USE_READ_BARRIER
    str    r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
#else
    strex  r2, r1, [r0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]  @ strex necessary for read barrier bits
    cbnz   r2, .Lunlock_strex_fail    @ store failed, retry
#endif
    bx     lr
.Lunlock_strex_fail:
    b      .Lretry_unlock             @ retry
.Lslow_unlock:
    @ save callee saves in case exception allocation triggers GC
    SETUP_SAVE_REFS_ONLY_FRAME r1
    mov    r1, r9                     @ pass Thread::Current
    bl     artUnlockObjectFromCode    @ (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_unlock_object

ENTRY art_quick_unlock_object_no_inline
    @ save callee saves in case exception allocation triggers GC
    SETUP_SAVE_REFS_ONLY_FRAME r1
    mov    r1, r9                     @ pass Thread::Current
    bl     artUnlockObjectFromCode    @ (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_unlock_object_no_inline

    /*
     * Entry from managed code that calls artInstanceOfFromCode and on failure calls
     * artThrowClassCastExceptionForObject.
     */
    .extern artInstanceOfFromCode
    .extern artThrowClassCastExceptionForObject
ENTRY art_quick_check_instance_of
    push {r0-r2, lr}                    @ save arguments, padding (r2) and link register
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset lr, 12
    bl artInstanceOfFromCode
    cbz    r0, .Lthrow_class_cast_exception
    pop {r0-r2, pc}

.Lthrow_class_cast_exception:
    pop {r0-r2, lr}
    .cfi_adjust_cfa_offset -16
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore lr

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME r2       @ save all registers as basis for long jump context
    mov r2, r9                      @ pass Thread::Current
    bl  artThrowClassCastExceptionForObject  @ (Object*, Class*, Thread*)
    bkpt
END art_quick_check_instance_of

// Restore rReg's value from [sp, #offset] if rReg is not the same as rExclude.
.macro POP_REG_NE rReg, offset, rExclude
    .ifnc \rReg, \rExclude
        ldr \rReg, [sp, #\offset]   @ restore rReg
        .cfi_restore \rReg
    .endif
.endm

// Save rReg's value to [sp, #offset].
.macro PUSH_REG rReg, offset
    str \rReg, [sp, #\offset]       @ save rReg
    .cfi_rel_offset \rReg, \offset
.endm

    /*
     * Macro to insert read barrier, only used in art_quick_aput_obj.
     * rObj and rDest are registers, offset is a defined literal such as MIRROR_OBJECT_CLASS_OFFSET.
     * TODO: When read barrier has a fast path, add heap unpoisoning support for the fast path.
     */
.macro READ_BARRIER rDest, rObj, offset
#ifdef USE_READ_BARRIER
    push {r0-r3, ip, lr}            @ 6 words for saved registers (used in art_quick_aput_obj)
    .cfi_adjust_cfa_offset 24
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset ip, 16
    .cfi_rel_offset lr, 20
    sub sp, #8                      @ push padding
    .cfi_adjust_cfa_offset 8
    @ mov r0, \rRef                 @ pass ref in r0 (no-op for now since parameter ref is unused)
    .ifnc \rObj, r1
        mov r1, \rObj               @ pass rObj
    .endif
    mov r2, #\offset                @ pass offset
    bl artReadBarrierSlow           @ artReadBarrierSlow(ref, rObj, offset)
    @ No need to unpoison return value in r0, artReadBarrierSlow() would do the unpoisoning.
    .ifnc \rDest, r0
        mov \rDest, r0              @ save return value in rDest
    .endif
    add sp, #8                      @ pop padding
    .cfi_adjust_cfa_offset -8
    POP_REG_NE r0, 0, \rDest        @ conditionally restore saved registers
    POP_REG_NE r1, 4, \rDest
    POP_REG_NE r2, 8, \rDest
    POP_REG_NE r3, 12, \rDest
    POP_REG_NE ip, 16, \rDest
    add sp, #20
    .cfi_adjust_cfa_offset -20
    pop {lr}                        @ restore lr
    .cfi_adjust_cfa_offset -4
    .cfi_restore lr
#else
    ldr \rDest, [\rObj, #\offset]
    UNPOISON_HEAP_REF \rDest
#endif  // USE_READ_BARRIER
.endm

#ifdef USE_READ_BARRIER
    .extern artReadBarrierSlow
#endif
    .hidden art_quick_aput_obj
ENTRY art_quick_aput_obj
#ifdef USE_READ_BARRIER
    @ The offset to .Ldo_aput_null is too large to use cbz due to expansion from READ_BARRIER macro.
    tst r2, r2
    beq .Ldo_aput_null
#else
    cbz r2, .Ldo_aput_null
#endif  // USE_READ_BARRIER
    READ_BARRIER r3, r0, MIRROR_OBJECT_CLASS_OFFSET
    READ_BARRIER ip, r2, MIRROR_OBJECT_CLASS_OFFSET
    READ_BARRIER r3, r3, MIRROR_CLASS_COMPONENT_TYPE_OFFSET
    cmp r3, ip  @ value's type == array's component type - trivial assignability
    bne .Lcheck_assignability
.Ldo_aput:
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    POISON_HEAP_REF r2
    str r2, [r3, r1, lsl #2]
    ldr r3, [r9, #THREAD_CARD_TABLE_OFFSET]
    lsr r0, r0, #CARD_TABLE_CARD_SHIFT
    strb r3, [r3, r0]
    blx lr
.Ldo_aput_null:
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    str r2, [r3, r1, lsl #2]
    blx lr
.Lcheck_assignability:
    push {r0-r2, lr}             @ save arguments
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset lr, 12
    mov r1, ip
    mov r0, r3
    bl artIsAssignableFromCode
    cbz r0, .Lthrow_array_store_exception
    pop {r0-r2, lr}
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore lr
    .cfi_adjust_cfa_offset -16
    add r3, r0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
    POISON_HEAP_REF r2
    str r2, [r3, r1, lsl #2]
    ldr r3, [r9, #THREAD_CARD_TABLE_OFFSET]
    lsr r0, r0, #CARD_TABLE_CARD_SHIFT
    strb r3, [r3, r0]
    blx lr
.Lthrow_array_store_exception:
    pop {r0-r2, lr}
    /* No need to repeat restore cfi directives, the ones above apply here. */
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME r3
    mov r1, r2
    mov r2, r9                     @ pass Thread::Current
    bl artThrowArrayStoreException @ (Class*, Class*, Thread*)
    bkpt                           @ unreached
END art_quick_aput_obj

// Macro to facilitate adding new allocation entrypoints.
.macro ONE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r1     @ save callee saves in case of GC
    mov    r1, r9                     @ pass Thread::Current
    bl     \entrypoint     @ (uint32_t type_idx, Method* method, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r2     @ save callee saves in case of GC
    mov    r2, r9                     @ pass Thread::Current
    bl     \entrypoint     @ (uint32_t type_idx, Method* method, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro to facilitate adding new array allocation entrypoints.
.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r3     @ save callee saves in case of GC
    mov    r3, r9                     @ pass Thread::Current
    @ (uint32_t type_idx, Method* method, int32_t component_count, Thread*)
    bl     \entrypoint
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro FOUR_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME r12    @ save callee saves in case of GC
    str    r9, [sp, #-16]!            @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 16
    bl     \entrypoint
    add    sp, #16                    @ strip the extra frame
    .cfi_adjust_cfa_offset -16
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro for string and type resolution and initialization.
.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL name, entrypoint, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_EVERYTHING_FRAME r1, \runtime_method_offset    @ save everything in case of GC
    mov    r1, r9                     @ pass Thread::Current
    bl     \entrypoint                @ (uint32_t index, Thread*)
    cbz    r0, 1f                     @ If result is null, deliver the OOME.
    .cfi_remember_state
    RESTORE_SAVE_EVERYTHING_FRAME_KEEP_R0
    REFRESH_MARKING_REGISTER
    bx     lr
    .cfi_restore_state
1:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END \name
.endm

.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT name, entrypoint
    ONE_ARG_SAVE_EVERYTHING_DOWNCALL \name, \entrypoint, RUNTIME_SAVE_EVERYTHING_FOR_CLINIT_METHOD_OFFSET
.endm

ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_static_storage, artInitializeStaticStorageFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_type, artInitializeTypeFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_resolve_string, artResolveStringFromCode

// Note: Functions `art{Get,Set}<Kind>{Static,Instance}FromCompiledCode` are
// defined with a macro in runtime/entrypoints/quick/quick_field_entrypoints.cc.

    /*
     * Called by managed code to resolve a static field and load a non-wide value.
     */
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    /*
     * Called by managed code to resolve a static field and load a 64-bit primitive value.
     */
    .extern artGet64StaticFromCompiledCode
ENTRY art_quick_get64_static
    SETUP_SAVE_REFS_ONLY_FRAME r2        @ save callee saves in case of GC
    mov    r1, r9                        @ pass Thread::Current
    bl     artGet64StaticFromCompiledCode        @ (uint32_t field_idx, Thread*)
    ldr    r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    cbnz   r2, 1f                        @ success if no exception pending
    bx     lr                            @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_get64_static

    /*
     * Called by managed code to resolve an instance field and load a non-wide value.
     */
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_R1
    /*
     * Called by managed code to resolve an instance field and load a 64-bit primitive value.
     */
    .extern artGet64InstanceFromCompiledCode
ENTRY art_quick_get64_instance
    SETUP_SAVE_REFS_ONLY_FRAME r2        @ save callee saves in case of GC
    mov    r2, r9                        @ pass Thread::Current
    bl     artGet64InstanceFromCompiledCode      @ (field_idx, Object*, Thread*)
    ldr    r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    cbnz   r2, 1f                        @ success if no exception pending
    bx     lr                            @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_get64_instance

    /*
     * Called by managed code to resolve a static field and store a value.
     */
TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER

    /*
     * Called by managed code to resolve an instance field and store a non-wide value.
     */
THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCompiledCode, RETURN_IF_RESULT_IS_ZERO_OR_DELIVER

    /*
     * Called by managed code to resolve an instance field and store a wide value.
     */
    .extern artSet64InstanceFromCompiledCode
ENTRY art_quick_set64_instance
    SETUP_SAVE_REFS_ONLY_FRAME r12       @ save callee saves in case of GC
                                         @ r2:r3 contain the wide argument
    str    r9, [sp, #-16]!               @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 16
    bl     artSet64InstanceFromCompiledCode      @ (field_idx, Object*, new_val, Thread*)
    add    sp, #16                       @ release out args
    .cfi_adjust_cfa_offset -16
    RESTORE_SAVE_REFS_ONLY_FRAME         @ TODO: we can clearly save an add here
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_set64_instance

    .extern artSet64StaticFromCompiledCode
ENTRY art_quick_set64_static
    SETUP_SAVE_REFS_ONLY_FRAME r12        @ save callee saves in case of GC
                                          @ r2:r3 contain the wide argument
    str    r9, [sp, #-16]!                @ expand the frame and pass Thread::Current
    .cfi_adjust_cfa_offset 16
    bl     artSet64StaticFromCompiledCode @ (field_idx, new_val, Thread*)
    add    sp, #16                        @ release out args
    .cfi_adjust_cfa_offset -16
    RESTORE_SAVE_REFS_ONLY_FRAME          @ TODO: we can clearly save an add here
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_ZERO
    DELIVER_PENDING_EXCEPTION
END art_quick_set64_static

// Generate the allocation entrypoints for each allocator.
GENERATE_ALLOC_ENTRYPOINTS_FOR_NON_TLAB_ALLOCATORS
// Comment out allocators that have arm specific asm.
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

// A hand-written override for GENERATE_ALLOC_ENTRYPOINTS_ALLOC_RESOLVED_OBJECT(_rosalloc, RosAlloc).
//
// If isInitialized=1 then the compiler assumes the object's class has already been initialized.
// If isInitialized=0 the compiler can only assume it's been at least resolved.
.macro ART_QUICK_ALLOC_OBJECT_ROSALLOC c_name, cxx_name, isInitialized
ENTRY \c_name
    // Fast path rosalloc allocation.
    // r0: type/return value, r9: Thread::Current
    // r1, r2, r3, r12: free.
    ldr    r3, [r9, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]     // Check if the thread local
                                                              // allocation stack has room.
                                                              // TODO: consider using ldrd.
    ldr    r12, [r9, #THREAD_LOCAL_ALLOC_STACK_END_OFFSET]
    cmp    r3, r12
    bhs    .Lslow_path\c_name

    ldr    r3, [r0, #MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET]  // Load the object size (r3)
    cmp    r3, #ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE        // Check if the size is for a thread
                                                              // local allocation. Also does the
                                                              // initialized and finalizable checks.
    // When isInitialized == 0, then the class is potentially not yet initialized.
    // If the class is not yet initialized, the object size will be very large to force the branch
    // below to be taken.
    //
    // See InitializeClassVisitors in class-inl.h for more details.
    bhs    .Lslow_path\c_name
                                                              // Compute the rosalloc bracket index
                                                              // from the size. Since the size is
                                                              // already aligned we can combine the
                                                              // two shifts together.
    add    r12, r9, r3, lsr #(ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT - POINTER_SIZE_SHIFT)
                                                              // Subtract pointer size since ther
                                                              // are no runs for 0 byte allocations
                                                              // and the size is already aligned.
                                                              // Load the rosalloc run (r12)
    ldr    r12, [r12, #(THREAD_ROSALLOC_RUNS_OFFSET - __SIZEOF_POINTER__)]
                                                              // Load the free list head (r3). This
                                                              // will be the return val.
    ldr    r3, [r12, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)]
    cbz    r3, .Lslow_path\c_name
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber r0 and r1.
    ldr    r1, [r3, #ROSALLOC_SLOT_NEXT_OFFSET]               // Load the next pointer of the head
                                                              // and update the list head with the
                                                              // next pointer.
    str    r1, [r12, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)]
                                                              // Store the class pointer in the
                                                              // header. This also overwrites the
                                                              // next pointer. The offsets are
                                                              // asserted to match.
#if ROSALLOC_SLOT_NEXT_OFFSET != MIRROR_OBJECT_CLASS_OFFSET
#error "Class pointer needs to overwrite next pointer."
#endif
    POISON_HEAP_REF r0
    str    r0, [r3, #MIRROR_OBJECT_CLASS_OFFSET]
                                                              // Push the new object onto the thread
                                                              // local allocation stack and
                                                              // increment the thread local
                                                              // allocation stack top.
    ldr    r1, [r9, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]
    str    r3, [r1], #COMPRESSED_REFERENCE_SIZE               // (Increment r1 as a side effect.)
    str    r1, [r9, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]
                                                              // Decrement the size of the free list

    // After this "STR" the object is published to the thread local allocation stack,
    // and it will be observable from a runtime internal (eg. Heap::VisitObjects) point of view.
    // It is not yet visible to the running (user) compiled code until after the return.
    //
    // To avoid the memory barrier prior to the "STR", a trick is employed, by differentiating
    // the state of the allocation stack slot. It can be a pointer to one of:
    // 0) Null entry, because the stack was bumped but the new pointer wasn't written yet.
    //       (The stack initial state is "null" pointers).
    // 1) A partially valid object, with an invalid class pointer to the next free rosalloc slot.
    // 2) A fully valid object, with a valid class pointer pointing to a real class.
    // Other states are not allowed.
    //
    // An object that is invalid only temporarily, and will eventually become valid.
    // The internal runtime code simply checks if the object is not null or is partial and then
    // ignores it.
    //
    // (Note: The actual check is done by seeing if a non-null object has a class pointer pointing
    // to ClassClass, and that the ClassClass's class pointer is self-cyclic. A rosalloc free slot
    // "next" pointer is not-cyclic.)
    //
    // See also b/28790624 for a listing of CLs dealing with this race.
    ldr    r1, [r12, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)]
    sub    r1, #1
                                                              // TODO: consider combining this store
                                                              // and the list head store above using
                                                              // strd.
    str    r1, [r12, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)]

    mov    r0, r3                                             // Set the return value and return.
.if \isInitialized == 0
    // This barrier is only necessary when the allocation also requires
    // a class initialization check.
    //
    // If the class is already observably initialized, then new-instance allocations are protected
    // from publishing by the compiler which inserts its own StoreStore barrier.
    dmb    ish
    // Use a "dmb ish" fence here because if there are later loads of statics (e.g. class size),
    // they should happen-after the implicit initialization check.
    //
    // TODO: Remove this dmb for class initialization checks (b/36692143) by introducing
    // a new observably-initialized class state.
.endif
    bx     lr

.Lslow_path\c_name:
    SETUP_SAVE_REFS_ONLY_FRAME r2     @ save callee saves in case of GC
    mov    r1, r9                     @ pass Thread::Current
    bl     \cxx_name                  @ (mirror::Class* cls, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \c_name
.endm

ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_resolved_rosalloc, artAllocObjectFromCodeResolvedRosAlloc, /* isInitialized */ 0
ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_initialized_rosalloc, artAllocObjectFromCodeInitializedRosAlloc, /* isInitialized */ 1

// The common fast path code for art_quick_alloc_object_resolved/initialized_tlab
// and art_quick_alloc_object_resolved/initialized_region_tlab.
//
// r0: type r9: Thread::Current, r1, r2, r3, r12: free.
// Need to preserve r0 to the slow path.
//
// If isInitialized=1 then the compiler assumes the object's class has already been initialized.
// If isInitialized=0 the compiler can only assume it's been at least resolved.
.macro ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH slowPathLabel isInitialized
                                                             // Load thread_local_pos (r12) and
                                                             // thread_local_end (r3) with ldrd.
                                                             // Check constraints for ldrd.
#if !((THREAD_LOCAL_POS_OFFSET + 4 == THREAD_LOCAL_END_OFFSET) && (THREAD_LOCAL_POS_OFFSET % 8 == 0))
#error "Thread::thread_local_pos/end must be consecutive and are 8 byte aligned for performance"
#endif
    ldrd   r12, r3, [r9, #THREAD_LOCAL_POS_OFFSET]
    sub    r12, r3, r12                                       // Compute the remaining buf size.
    ldr    r3, [r0, #MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET]  // Load the object size (r3).
    cmp    r3, r12                                            // Check if it fits.
    // When isInitialized == 0, then the class is potentially not yet initialized.
    // If the class is not yet initialized, the object size will be very large to force the branch
    // below to be taken.
    //
    // See InitializeClassVisitors in class-inl.h for more details.
    bhi    \slowPathLabel
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber r0 and r1.
                                                              // Reload old thread_local_pos (r0)
                                                              // for the return value.
    ldr    r2, [r9, #THREAD_LOCAL_POS_OFFSET]
    add    r1, r2, r3
    str    r1, [r9, #THREAD_LOCAL_POS_OFFSET]                 // Store new thread_local_pos.
    // After this "STR" the object is published to the thread local allocation stack,
    // and it will be observable from a runtime internal (eg. Heap::VisitObjects) point of view.
    // It is not yet visible to the running (user) compiled code until after the return.
    //
    // To avoid the memory barrier prior to the "STR", a trick is employed, by differentiating
    // the state of the object. It can be either:
    // 1) A partially valid object, with a null class pointer
    //       (because the initial state of TLAB buffers is all 0s/nulls).
    // 2) A fully valid object, with a valid class pointer pointing to a real class.
    // Other states are not allowed.
    //
    // An object that is invalid only temporarily, and will eventually become valid.
    // The internal runtime code simply checks if the object is not null or is partial and then
    // ignores it.
    //
    // (Note: The actual check is done by checking that the object's class pointer is non-null.
    // Also, unlike rosalloc, the object can never be observed as null).
    ldr    r1, [r9, #THREAD_LOCAL_OBJECTS_OFFSET]             // Increment thread_local_objects.
    add    r1, r1, #1
    str    r1, [r9, #THREAD_LOCAL_OBJECTS_OFFSET]
    POISON_HEAP_REF r0
    str    r0, [r2, #MIRROR_OBJECT_CLASS_OFFSET]              // Store the class pointer.
                                                              // Fence. This is "ish" not "ishst" so
                                                              // that the code after this allocation
                                                              // site will see the right values in
                                                              // the fields of the class.
    mov    r0, r2
.if \isInitialized == 0
    // This barrier is only necessary when the allocation also requires
    // a class initialization check.
    //
    // If the class is already observably initialized, then new-instance allocations are protected
    // from publishing by the compiler which inserts its own StoreStore barrier.
    dmb    ish
    // Use a "dmb ish" fence here because if there are later loads of statics (e.g. class size),
    // they should happen-after the implicit initialization check.
    //
    // TODO: Remove dmb for class initialization checks (b/36692143)
.endif
    bx     lr
.endm

// The common code for art_quick_alloc_object_*region_tlab
.macro GENERATE_ALLOC_OBJECT_RESOLVED_TLAB name, entrypoint, isInitialized
ENTRY \name
    // Fast path tlab allocation.
    // r0: type, r9: Thread::Current
    // r1, r2, r3, r12: free.
    ALLOC_OBJECT_RESOLVED_TLAB_FAST_PATH .Lslow_path\name, \isInitialized
.Lslow_path\name:
    SETUP_SAVE_REFS_ONLY_FRAME r2                             // Save callee saves in case of GC.
    mov    r1, r9                                             // Pass Thread::Current.
    bl     \entrypoint                                        // (mirror::Class* klass, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_resolved_region_tlab, artAllocObjectFromCodeResolvedRegionTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_initialized_region_tlab, artAllocObjectFromCodeInitializedRegionTLAB, /* isInitialized */ 1
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_resolved_tlab, artAllocObjectFromCodeResolvedTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_initialized_tlab, artAllocObjectFromCodeInitializedTLAB, /* isInitialized */ 1


// The common fast path code for art_quick_alloc_array_resolved/initialized_tlab
// and art_quick_alloc_array_resolved/initialized_region_tlab.
//
// r0: type r1: component_count r2: total_size r9: Thread::Current, r3, r12: free.
// Need to preserve r0 and r1 to the slow path.
.macro ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE slowPathLabel
    and    r2, r2, #OBJECT_ALIGNMENT_MASK_TOGGLED             // Apply alignment mask
                                                              // (addr + 7) & ~7.

                                                              // Load thread_local_pos (r3) and
                                                              // thread_local_end (r12) with ldrd.
                                                              // Check constraints for ldrd.
#if !((THREAD_LOCAL_POS_OFFSET + 4 == THREAD_LOCAL_END_OFFSET) && (THREAD_LOCAL_POS_OFFSET % 8 == 0))
#error "Thread::thread_local_pos/end must be consecutive and are 8 byte aligned for performance"
#endif
    ldrd   r3, r12, [r9, #THREAD_LOCAL_POS_OFFSET]
    sub    r12, r12, r3                                       // Compute the remaining buf size.
    cmp    r2, r12                                            // Check if the total_size fits.
    // The array class is always initialized here. Unlike new-instance,
    // this does not act as a double test.
    bhi    \slowPathLabel
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber r0 and r1.
    add    r2, r2, r3
    str    r2, [r9, #THREAD_LOCAL_POS_OFFSET]                 // Store new thread_local_pos.
    ldr    r2, [r9, #THREAD_LOCAL_OBJECTS_OFFSET]             // Increment thread_local_objects.
    add    r2, r2, #1
    str    r2, [r9, #THREAD_LOCAL_OBJECTS_OFFSET]
    POISON_HEAP_REF r0
    str    r0, [r3, #MIRROR_OBJECT_CLASS_OFFSET]              // Store the class pointer.
    str    r1, [r3, #MIRROR_ARRAY_LENGTH_OFFSET]              // Store the array length.
                                                              // Fence. This is "ish" not "ishst" so
                                                              // that the code after this allocation
                                                              // site will see the right values in
                                                              // the fields of the class.
    mov    r0, r3
// new-array is special. The class is loaded and immediately goes to the Initialized state
// before it is published. Therefore the only fence needed is for the publication of the object.
// See ClassLinker::CreateArrayClass() for more details.

// For publication of the new array, we don't need a 'dmb ishst' here.
// The compiler generates 'dmb ishst' for all new-array insts.
    bx     lr
.endm

.macro GENERATE_ALLOC_ARRAY_TLAB name, entrypoint, size_setup
ENTRY \name
    // Fast path array allocation for region tlab allocation.
    // r0: mirror::Class* type
    // r1: int32_t component_count
    // r9: thread
    // r2, r3, r12: free.
    \size_setup .Lslow_path\name
    ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE .Lslow_path\name
.Lslow_path\name:
    // r0: mirror::Class* klass
    // r1: int32_t component_count
    // r2: Thread* self
    SETUP_SAVE_REFS_ONLY_FRAME r2  // save callee saves in case of GC
    mov    r2, r9                  // pass Thread::Current
    bl     \entrypoint
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

.macro COMPUTE_ARRAY_SIZE_UNKNOWN slow_path
    bkpt                                                    // We should never enter here.
                                                            // Code below is for reference.
                                                            // Possibly a large object, go slow.
                                                            // Also does negative array size check.
    movw r2, #((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_WIDE_ARRAY_DATA_OFFSET) / 8)
    cmp r1, r2
    bhi \slow_path
                                                            // Array classes are never finalizable
                                                            // or uninitialized, no need to check.
    ldr    r3, [r0, #MIRROR_CLASS_COMPONENT_TYPE_OFFSET]    // Load component type
    UNPOISON_HEAP_REF r3
    ldr    r3, [r3, #MIRROR_CLASS_OBJECT_PRIMITIVE_TYPE_OFFSET]
    lsr    r3, r3, #PRIMITIVE_TYPE_SIZE_SHIFT_SHIFT         // Component size shift is in high 16
                                                            // bits.
    lsl    r2, r1, r3                                       // Calculate data size
                                                            // Add array data offset and alignment.
    add    r2, r2, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
#if MIRROR_WIDE_ARRAY_DATA_OFFSET != MIRROR_INT_ARRAY_DATA_OFFSET + 4
#error Long array data offset must be 4 greater than int array data offset.
#endif

    add    r3, r3, #1                                       // Add 4 to the length only if the
                                                            // component size shift is 3
                                                            // (for 64 bit alignment).
    and    r3, r3, #4
    add    r2, r2, r3
.endm

.macro COMPUTE_ARRAY_SIZE_8 slow_path
    // Possibly a large object, go slow.
    // Also does negative array size check.
    movw r2, #(MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET)
    cmp r1, r2
    bhi \slow_path
    // Add array data offset and alignment.
    add    r2, r1, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_16 slow_path
    // Possibly a large object, go slow.
    // Also does negative array size check.
    movw r2, #((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET) / 2)
    cmp r1, r2
    bhi \slow_path
    lsl    r2, r1, #1
    // Add array data offset and alignment.
    add    r2, r2, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_32 slow_path
    // Possibly a large object, go slow.
    // Also does negative array size check.
    movw r2, #((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_INT_ARRAY_DATA_OFFSET) / 4)
    cmp r1, r2
    bhi \slow_path
    lsl    r2, r1, #2
    // Add array data offset and alignment.
    add    r2, r2, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_64 slow_path
    // Possibly a large object, go slow.
    // Also does negative array size check.
    movw r2, #((MIN_LARGE_OBJECT_THRESHOLD - MIRROR_LONG_ARRAY_DATA_OFFSET) / 8)
    cmp r1, r2
    bhi \slow_path
    lsl    r2, r1, #3
    // Add array data offset and alignment.
    add    r2, r2, #(MIRROR_WIDE_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

// TODO(ngeoffray): art_quick_alloc_array_resolved_region_tlab is not used for arm, remove
// the entrypoint once all backends have been updated to use the size variants.
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

    /*
     * Called by managed code when the value in rSUSPEND has been decremented to 0.
     */
    .extern artTestSuspendFromCode
ENTRY art_quick_test_suspend
    SETUP_SAVE_EVERYTHING_FRAME r0, RUNTIME_SAVE_EVERYTHING_FOR_SUSPEND_CHECK_METHOD_OFFSET @ save everything for GC stack crawl
    mov    r0, rSELF
    bl     artTestSuspendFromCode               @ (Thread*)
    RESTORE_SAVE_EVERYTHING_FRAME
    REFRESH_MARKING_REGISTER
    bx     lr
END art_quick_test_suspend

ENTRY art_quick_implicit_suspend
    mov    r0, rSELF
    SETUP_SAVE_REFS_ONLY_FRAME r1             @ save callee saves for stack crawl
    bl     artTestSuspendFromCode             @ (Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    bx     lr
END art_quick_implicit_suspend

    /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * r0 holds the proxy method and r1 holds the receiver; r2 and r3 may contain arguments. The
     * frame size of the invoked proxy method agrees with a ref and args callee save frame.
     */
     .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_R0
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artQuickProxyInvokeHandler  @ (Method* proxy method, receiver, Thread*, SP)
    ldr     r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #(FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY)
    .cfi_adjust_cfa_offset -(FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    cbnz    r2, 1f                 @ success if no exception is pending
    vmov    d0, r0, r1             @ store into fpr, for when it's a fpr return...
    bx      lr                     @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * r0 is the conflict ArtMethod.
     * r12 is a hidden argument that holds the target interface method's dex method index.
     *
     * Note that this stub writes to r0, r4, and r12.
     */
    .extern artLookupResolvedMethod
ENTRY art_quick_imt_conflict_trampoline
    push    {r1-r2}
    .cfi_adjust_cfa_offset (2 * 4)
    .cfi_rel_offset r1, 0
    .cfi_rel_offset r2, 4
    ldr     r4, [sp, #(2 * 4)]  // Load referrer.
    ldr     r2, [r0, #ART_METHOD_JNI_OFFSET_32]  // Load ImtConflictTable
    // Load the declaring class (without read barrier) and access flags (for obsolete method check).
    // The obsolete flag is set with suspended threads, so we do not need an acquire operation here.
#if ART_METHOD_ACCESS_FLAGS_OFFSET != ART_METHOD_DECLARING_CLASS_OFFSET + 4
#error "Expecting declaring class and access flags to be consecutive for LDRD."
#endif
    ldrd    r0, r1, [r4, #ART_METHOD_DECLARING_CLASS_OFFSET]
    // If the method is obsolete, just go through the dex cache miss slow path.
    lsrs    r1, #(ACC_OBSOLETE_METHOD_SHIFT + 1)
    bcs     .Limt_conflict_trampoline_dex_cache_miss
    ldr     r4, [r0, #MIRROR_CLASS_DEX_CACHE_OFFSET]  // Load the DexCache (without read barrier).
    UNPOISON_HEAP_REF r4
    ubfx    r1, r12, #0, #METHOD_DEX_CACHE_HASH_BITS  // Calculate DexCache method slot index.
    ldr     r4, [r4, #MIRROR_DEX_CACHE_RESOLVED_METHODS_OFFSET]  // Load the resolved methods.
    add     r4, r4, r1, lsl #(POINTER_SIZE_SHIFT + 1)  // Load DexCache method slot address.

// FIXME: Configure the build to use the faster code when appropriate.
//        Currently we fall back to the slower version.
#if HAS_ATOMIC_LDRD
    ldrd    r0, r1, [r4]
#else
    push    {r3}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset r3, 0
.Limt_conflict_trampoline_retry_load:
    ldrexd  r0, r1, [r4]
    strexd  r3, r0, r1, [r4]
    cmp     r3, #0
    bne     .Limt_conflict_trampoline_retry_load
    pop     {r3}
    .cfi_adjust_cfa_offset -4
    .cfi_restore r3
#endif

    ldr     r4, [r2]  // Load first entry in ImtConflictTable.
    cmp     r1, r12   // Compare method index to see if we had a DexCache method hit.
    bne     .Limt_conflict_trampoline_dex_cache_miss
.Limt_table_iterate:
    cmp     r4, r0
    // Branch if found. Benchmarks have shown doing a branch here is better.
    beq     .Limt_table_found
    // If the entry is null, the interface method is not in the ImtConflictTable.
    cbz     r4, .Lconflict_trampoline
    // Iterate over the entries of the ImtConflictTable.
    ldr     r4, [r2, #(2 * __SIZEOF_POINTER__)]!
    b .Limt_table_iterate
.Limt_table_found:
    // We successfully hit an entry in the table. Load the target method
    // and jump to it.
    ldr     r0, [r2, #__SIZEOF_POINTER__]
    .cfi_remember_state
    pop     {r1-r2}
    .cfi_adjust_cfa_offset -(2 * 4)
    .cfi_restore r1
    .cfi_restore r2
    ldr     pc, [r0, #ART_METHOD_QUICK_CODE_OFFSET_32]
    .cfi_restore_state
.Lconflict_trampoline:
    // Call the runtime stub to populate the ImtConflictTable and jump to the
    // resolved method.
    .cfi_remember_state
    pop     {r1-r2}
    .cfi_adjust_cfa_offset -(2 * 4)
    .cfi_restore r1
    .cfi_restore r2
    INVOKE_TRAMPOLINE_BODY artInvokeInterfaceTrampoline
    .cfi_restore_state
.Limt_conflict_trampoline_dex_cache_miss:
    // We're not creating a proper runtime method frame here,
    // artLookupResolvedMethod() is not allowed to walk the stack.

    // Save ImtConflictTable (r2), remaining arg (r3), first entry (r4), return address (lr).
    push    {r2-r4, lr}
    .cfi_adjust_cfa_offset (4 * 4)
    .cfi_rel_offset r3, 4
    .cfi_rel_offset lr, 12
    // Save FPR args.
    vpush   {d0-d7}
    .cfi_adjust_cfa_offset (8 * 8)

    mov     r0, ip                      // Pass method index.
    ldr     r1, [sp, #(8 * 8 + 6 * 4)]  // Pass referrer.
    bl      artLookupResolvedMethod     // (uint32_t method_index, ArtMethod* referrer)

    // Restore FPR args.
    vpop    {d0-d7}
    .cfi_adjust_cfa_offset -(8 * 8)
    // Restore ImtConflictTable (r2), remaining arg (r3), first entry (r4), return address (lr).
    pop     {r2-r4, lr}
    .cfi_adjust_cfa_offset -(4 * 4)
    .cfi_restore r3
    .cfi_restore lr

    cmp     r0, #0                  // If the method wasn't resolved,
    beq     .Lconflict_trampoline   //   skip the lookup and go to artInvokeInterfaceTrampoline().
    b       .Limt_table_iterate
END art_quick_imt_conflict_trampoline

    .extern artQuickResolutionTrampoline
ENTRY art_quick_resolution_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME r2
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    blx     artQuickResolutionTrampoline  @ (Method* called, receiver, Thread*, SP)
    cbz     r0, 1f                 @ is code pointer null? goto exception
    mov     r12, r0
    ldr     r0, [sp, #0]           @ load resolved method in r0
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    bx      r12                    @ tail-call into actual code
1:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

    /*
     * Called to do a generic JNI down-call
     */
ENTRY art_quick_generic_jni_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_R0

    // Save rSELF
    mov r11, rSELF
    // Save SP , so we can have static CFI info. r10 is saved in ref_and_args.
    mov r10, sp
    .cfi_def_cfa_register r10

    sub sp, sp, #5120

    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //    r0      r1   <= C calling convention
    //  rSELF     r10  <= where they are

    mov r0, rSELF   // Thread*
    mov r1, r10
    blx artQuickGenericJniTrampoline  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // r0: pointer to native code, 0 on error.
    // r1: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    cbz r0, .Lexception_in_native

    // Release part of the alloca.
    mov sp, r1

    // Save the code pointer
    mov r12, r0

    // Load parameters from frame into registers.
    pop {r0-r3}

    // Softfloat.
    // TODO: Change to hardfloat when supported.

    blx r12           // native call.

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*, result, result_f)
    //    r0      r2,r3    stack       <= C calling convention
    //    r11     r0,r1    r0,r1          <= where they are
    sub sp, sp, #8 // Stack alignment.

    push {r0-r1}
    mov r3, r1
    mov r2, r0
    mov r0, r11

    blx artQuickGenericJniEndTrampoline

    // Restore self pointer.
    mov r9, r11

    // Pending exceptions possible.
    ldr r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    cbnz r2, .Lexception_in_native

    // Tear down the alloca.
    mov sp, r10
    .cfi_def_cfa_register sp

    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #FRAME_SIZE_SAVE_REFS_AND_ARGS-FRAME_SIZE_SAVE_REFS_ONLY
    .cfi_adjust_cfa_offset -(FRAME_SIZE_SAVE_REFS_AND_ARGS-FRAME_SIZE_SAVE_REFS_ONLY)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER

    // store into fpr, for when it's a fpr return...
    vmov d0, r0, r1
    bx lr      // ret
    // Undo the unwinding information from above since it doesn't apply below.
    .cfi_def_cfa_register r10
    .cfi_adjust_cfa_offset FRAME_SIZE_SAVE_REFS_AND_ARGS-FRAME_SIZE_SAVE_REFS_ONLY

.Lexception_in_native:
    ldr ip, [r9, #THREAD_TOP_QUICK_FRAME_OFFSET]
    add ip, ip, #-1  // Remove the GenericJNI tag. ADD/SUB writing directly to SP is UNPREDICTABLE.
    mov sp, ip
    .cfi_def_cfa_register sp
    # This will create a new save-all frame, required by the runtime.
    DELIVER_PENDING_EXCEPTION
END art_quick_generic_jni_trampoline

    .extern artQuickToInterpreterBridge
ENTRY art_quick_to_interpreter_bridge
    SETUP_SAVE_REFS_AND_ARGS_FRAME r1
    mov     r1, r9                 @ pass Thread::Current
    mov     r2, sp                 @ pass SP
    blx     artQuickToInterpreterBridge    @ (Method* method, Thread*, SP)
    ldr     r2, [r9, #THREAD_EXCEPTION_OFFSET]  @ load Thread::Current()->exception_
    // Tear down the callee-save frame. Skip arg registers.
    add     sp, #(FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY)
    .cfi_adjust_cfa_offset -(FRAME_SIZE_SAVE_REFS_AND_ARGS - FRAME_SIZE_SAVE_REFS_ONLY)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    cbnz    r2, 1f                 @ success if no exception is pending
    vmov    d0, r0, r1             @ store into fpr, for when it's a fpr return...
    bx      lr                     @ return on success
1:
    DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

/*
 * Called to attempt to execute an obsolete method.
 */
ONE_ARG_RUNTIME_EXCEPTION art_invoke_obsolete_method_stub, artInvokeObsoleteMethod

    /*
     * Routine that intercepts method calls and returns.
     */
    .extern artInstrumentationMethodEntryFromCode
    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_entry
    @ Make stack crawlable and clobber r2 and r3 (post saving)
    SETUP_SAVE_REFS_AND_ARGS_FRAME r2
    @ preserve r0 (not normally an arg) knowing there is a spare slot in kSaveRefsAndArgs.
    str   r0, [sp, #4]
    mov   r2, r9         @ pass Thread::Current
    mov   r3, sp         @ pass SP
    blx   artInstrumentationMethodEntryFromCode  @ (Method*, Object*, Thread*, SP)
    cbz   r0, .Ldeliver_instrumentation_entry_exception
                         @ Deliver exception if we got nullptr as function.
    mov   r12, r0        @ r12 holds reference to code
    ldr   r0, [sp, #4]   @ restore r0
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    adr   lr, art_quick_instrumentation_exit + /* thumb mode */ 1
                         @ load art_quick_instrumentation_exit into lr in thumb mode
    REFRESH_MARKING_REGISTER
    bx    r12            @ call method with lr set to art_quick_instrumentation_exit
.Ldeliver_instrumentation_entry_exception:
    @ Deliver exception for art_quick_instrumentation_entry placed after
    @ art_quick_instrumentation_exit so that the fallthrough works.
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_instrumentation_entry

ENTRY art_quick_instrumentation_exit
    mov   lr, #0         @ link register is to here, so clobber with 0 for later checks
    SETUP_SAVE_EVERYTHING_FRAME r2

    add   r3, sp, #8     @ store fpr_res pointer, in kSaveEverything frame
    add   r2, sp, #136   @ store gpr_res pointer, in kSaveEverything frame
    mov   r1, sp         @ pass SP
    mov   r0, r9         @ pass Thread::Current
    blx   artInstrumentationMethodExitFromCode  @ (Thread*, SP, gpr_res*, fpr_res*)

    cbz   r0, .Ldo_deliver_instrumentation_exception
                         @ Deliver exception if we got nullptr as function.
    cbnz  r1, .Ldeoptimize
    // Normal return.
    str   r0, [sp, #FRAME_SIZE_SAVE_EVERYTHING - 4]
                         @ Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    REFRESH_MARKING_REGISTER
    bx lr
.Ldo_deliver_instrumentation_exception:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.Ldeoptimize:
    str   r1, [sp, #FRAME_SIZE_SAVE_EVERYTHING - 4]
                         @ Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    // Jump to art_quick_deoptimize.
    b     art_quick_deoptimize
END art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimize
ENTRY art_quick_deoptimize
    SETUP_SAVE_EVERYTHING_FRAME r0
    mov    r0, r9         @ pass Thread::Current
    blx    artDeoptimize  @ (Thread*)
END art_quick_deoptimize

    /*
     * Compiled code has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the interpreter bridge.
     */
    .extern artDeoptimizeFromCompiledCode
ENTRY art_quick_deoptimize_from_compiled_code
    SETUP_SAVE_EVERYTHING_FRAME r1
    mov    r1, r9                         @ pass Thread::Current
    blx    artDeoptimizeFromCompiledCode  @ (DeoptimizationKind, Thread*)
END art_quick_deoptimize_from_compiled_code

    /*
     * Signed 64-bit integer multiply.
     *
     * Consider WXxYZ (r1r0 x r3r2) with a long multiply:
     *        WX
     *      x YZ
     *  --------
     *     ZW ZX
     *  YW YX
     *
     * The low word of the result holds ZX, the high word holds
     * (ZW+YX) + (the high overflow from ZX).  YW doesn't matter because
     * it doesn't fit in the low 64 bits.
     *
     * Unlike most ARM math operations, multiply instructions have
     * restrictions on using the same register more than once (Rd and Rm
     * cannot be the same).
     */
    /* mul-long vAA, vBB, vCC */
ENTRY art_quick_mul_long
    push    {r9-r10}
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset r9, 0
    .cfi_rel_offset r10, 4
    mul     ip, r2, r1                  @  ip<- ZxW
    umull   r9, r10, r2, r0             @  r9/r10 <- ZxX
    mla     r2, r0, r3, ip              @  r2<- YxX + (ZxW)
    add     r10, r2, r10                @  r10<- r10 + low(ZxW + (YxX))
    mov     r0,r9
    mov     r1,r10
    pop     {r9-r10}
    .cfi_adjust_cfa_offset -8
    .cfi_restore r9
    .cfi_restore r10
    bx      lr
END art_quick_mul_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* shl-long vAA, vBB, vCC */
ARM_ENTRY art_quick_shl_long            @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r2<- r2 & 0x3f
    mov     r1, r1, asl r2              @  r1<- r1 << r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r1, r1, r0, lsr r3          @  r1<- r1 | (r0 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r1, r0, asl ip              @  if r2 >= 32, r1<- r0 << (r2-32)
    mov     r0, r0, asl r2              @  r0<- r0 << r2
    bx      lr
END art_quick_shl_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* shr-long vAA, vBB, vCC */
ARM_ENTRY art_quick_shr_long            @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, asr ip              @  if r2 >= 32, r0<-r1 >> (r2-32)
    mov     r1, r1, asr r2              @  r1<- r1 >> r2
    bx      lr
END art_quick_shr_long

    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     * On entry:
     *   r0: low word
     *   r1: high word
     *   r2: shift count
     */
    /* ushr-long vAA, vBB, vCC */
ARM_ENTRY art_quick_ushr_long           @ ARM code as thumb code requires spills
    and     r2, r2, #63                 @ r0<- r0 & 0x3f
    mov     r0, r0, lsr r2              @  r0<- r2 >> r2
    rsb     r3, r2, #32                 @  r3<- 32 - r2
    orr     r0, r0, r1, asl r3          @  r0<- r0 | (r1 << (32-r2))
    subs    ip, r2, #32                 @  ip<- r2 - 32
    movpl   r0, r1, lsr ip              @  if r2 >= 32, r0<-r1 >>> (r2-32)
    mov     r1, r1, lsr r2              @  r1<- r1 >>> r2
    bx      lr
END art_quick_ushr_long

    /*
     * String's indexOf.
     *
     * On entry:
     *    r0:   string object (known non-null)
     *    r1:   char to match (known <= 0xFFFF)
     *    r2:   Starting offset in string data
     */
ENTRY art_quick_indexof
    push {r4, r10-r11, lr} @ 4 words of callee saves
    .cfi_adjust_cfa_offset 16
    .cfi_rel_offset r4, 0
    .cfi_rel_offset r10, 4
    .cfi_rel_offset r11, 8
    .cfi_rel_offset lr, 12
#if (STRING_COMPRESSION_FEATURE)
    ldr   r4, [r0, #MIRROR_STRING_COUNT_OFFSET]
#else
    ldr   r3, [r0, #MIRROR_STRING_COUNT_OFFSET]
#endif
    add   r0, #MIRROR_STRING_VALUE_OFFSET
#if (STRING_COMPRESSION_FEATURE)
    /* r4 count (with flag) and r3 holds actual length */
    lsr   r3, r4, #1
#endif
    /* Clamp start to [0..count] */
    cmp   r2, #0
    it    lt
    movlt r2, #0
    cmp   r2, r3
    it    gt
    movgt r2, r3

    /* Save a copy in r12 to later compute result */
    mov   r12, r0

    /* Build pointer to start of data to compare and pre-bias */
#if (STRING_COMPRESSION_FEATURE)
    lsrs  r4, r4, #1
    bcc   .Lstring_indexof_compressed
#endif
    add   r0, r0, r2, lsl #1
    sub   r0, #2

    /* Compute iteration count */
    sub   r2, r3, r2

    /*
     * At this point we have:
     *   r0: start of data to test
     *   r1: char to compare
     *   r2: iteration count
     *   r4: compression style (used temporarily)
     *   r12: original start of string data
     *   r3, r4, r10, r11 available for loading string data
     */

    subs  r2, #4
    blt   .Lindexof_remainder

.Lindexof_loop4:
    ldrh  r3, [r0, #2]!
    ldrh  r4, [r0, #2]!
    ldrh  r10, [r0, #2]!
    ldrh  r11, [r0, #2]!
    cmp   r3, r1
    beq   .Lmatch_0
    cmp   r4, r1
    beq   .Lmatch_1
    cmp   r10, r1
    beq   .Lmatch_2
    cmp   r11, r1
    beq   .Lmatch_3
    subs  r2, #4
    bge   .Lindexof_loop4

.Lindexof_remainder:
    adds  r2, #4
    beq   .Lindexof_nomatch

.Lindexof_loop1:
    ldrh  r3, [r0, #2]!
    cmp   r3, r1
    beq   .Lmatch_3
    subs  r2, #1
    bne   .Lindexof_loop1

.Lindexof_nomatch:
    mov   r0, #-1
    pop {r4, r10-r11, pc}

.Lmatch_0:
    sub   r0, #6
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_1:
    sub   r0, #4
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_2:
    sub   r0, #2
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
.Lmatch_3:
    sub   r0, r12
    asr   r0, r0, #1
    pop {r4, r10-r11, pc}
#if (STRING_COMPRESSION_FEATURE)
.Lstring_indexof_compressed:
    add   r0, r0, r2
    sub   r0, #1
    sub   r2, r3, r2
.Lstring_indexof_compressed_loop:
    subs  r2, #1
    blt   .Lindexof_nomatch
    ldrb  r3, [r0, #1]!
    cmp   r3, r1
    beq   .Lstring_indexof_compressed_matched
    b     .Lstring_indexof_compressed_loop
.Lstring_indexof_compressed_matched:
    sub   r0, r12
    pop {r4, r10-r11, pc}
#endif
END art_quick_indexof

    /* Assembly routines used to handle ABI differences. */

    /* double fmod(double a, double b) */
    .extern fmod
ENTRY art_quick_fmod
    push  {lr}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset lr, 0
    sub   sp, #4
    .cfi_adjust_cfa_offset 4
    vmov  r0, r1, d0
    vmov  r2, r3, d1
    bl    fmod
    vmov  d0, r0, r1
    add   sp, #4
    .cfi_adjust_cfa_offset -4
    pop   {pc}
END art_quick_fmod

    /* float fmodf(float a, float b) */
     .extern fmodf
ENTRY art_quick_fmodf
    push  {lr}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset lr, 0
    sub   sp, #4
    .cfi_adjust_cfa_offset 4
    vmov  r0, r1, d0
    bl    fmodf
    vmov  s0, r0
    add   sp, #4
    .cfi_adjust_cfa_offset -4
    pop   {pc}
END art_quick_fmodf

    /* int64_t art_d2l(double d) */
    .extern art_d2l
ENTRY art_quick_d2l
    vmov  r0, r1, d0
    b     art_d2l
END art_quick_d2l

    /* int64_t art_f2l(float f) */
    .extern art_f2l
ENTRY art_quick_f2l
    vmov  r0, s0
    b     art_f2l
END art_quick_f2l

    /* float art_l2f(int64_t l) */
    .extern art_l2f
ENTRY art_quick_l2f
    push  {lr}
    .cfi_adjust_cfa_offset 4
    .cfi_rel_offset lr, 0
    sub   sp, #4
    .cfi_adjust_cfa_offset 4
    bl    art_l2f
    vmov  s0, r0
    add   sp, #4
    .cfi_adjust_cfa_offset -4
    pop   {pc}
END art_quick_l2f

.macro CONDITIONAL_CBZ reg, reg_if, dest
.ifc \reg, \reg_if
    cbz \reg, \dest
.endif
.endm

.macro CONDITIONAL_CMPBZ reg, reg_if, dest
.ifc \reg, \reg_if
    cmp \reg, #0
    beq \dest
.endif
.endm

// Use CBZ if the register is in {r0, r7} otherwise compare and branch.
.macro SMART_CBZ reg, dest
    CONDITIONAL_CBZ \reg, r0, \dest
    CONDITIONAL_CBZ \reg, r1, \dest
    CONDITIONAL_CBZ \reg, r2, \dest
    CONDITIONAL_CBZ \reg, r3, \dest
    CONDITIONAL_CBZ \reg, r4, \dest
    CONDITIONAL_CBZ \reg, r5, \dest
    CONDITIONAL_CBZ \reg, r6, \dest
    CONDITIONAL_CBZ \reg, r7, \dest
    CONDITIONAL_CMPBZ \reg, r8, \dest
    CONDITIONAL_CMPBZ \reg, r9, \dest
    CONDITIONAL_CMPBZ \reg, r10, \dest
    CONDITIONAL_CMPBZ \reg, r11, \dest
    CONDITIONAL_CMPBZ \reg, r12, \dest
    CONDITIONAL_CMPBZ \reg, r13, \dest
    CONDITIONAL_CMPBZ \reg, r14, \dest
    CONDITIONAL_CMPBZ \reg, r15, \dest
.endm

    /*
     * Create a function `name` calling the ReadBarrier::Mark routine,
     * getting its argument and returning its result through register
     * `reg`, saving and restoring all caller-save registers.
     *
     * IP is clobbered; `reg` must not be IP.
     *
     * If `reg` is different from `r0`, the generated function follows a
     * non-standard runtime calling convention:
     * - register `reg` is used to pass the (sole) argument of this
     *   function (instead of R0);
     * - register `reg` is used to return the result of this function
     *   (instead of R0);
     * - R0 is treated like a normal (non-argument) caller-save register;
     * - everything else is the same as in the standard runtime calling
     *   convention (e.g. standard callee-save registers are preserved).
     */
.macro READ_BARRIER_MARK_REG name, reg
ENTRY \name
    // Null check so that we can load the lock word.
    SMART_CBZ \reg, .Lret_rb_\name
    // Check lock word for mark bit, if marked return. Use IP for scratch since it is blocked.
    ldr ip, [\reg, MIRROR_OBJECT_LOCK_WORD_OFFSET]
    tst ip, #LOCK_WORD_MARK_BIT_MASK_SHIFTED
    beq .Lnot_marked_rb_\name
    // Already marked, return right away.
.Lret_rb_\name:
    bx lr

.Lnot_marked_rb_\name:
    // Test that both the forwarding state bits are 1.
#if (LOCK_WORD_STATE_SHIFT != 30) || (LOCK_WORD_STATE_FORWARDING_ADDRESS != 3)
    // To use "CMP ip, #modified-immediate; BHS", we need the lock word state in
    // the highest bits and the "forwarding address" state to have all bits set.
#error "Unexpected lock word state shift or forwarding address state value."
#endif
    cmp ip, #(LOCK_WORD_STATE_FORWARDING_ADDRESS << LOCK_WORD_STATE_SHIFT)
    bhs .Lret_forwarding_address\name

.Lslow_rb_\name:
    // Save IP: The kSaveEverything entrypoint art_quick_resolve_string used to
    // make a tail call here. Currently, it serves only for stack alignment but
    // we may reintroduce kSaveEverything calls here in the future.
    push  {r0-r4, r9, ip, lr}           @ save return address, core caller-save registers and ip
    .cfi_adjust_cfa_offset 32
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset r4, 16
    .cfi_rel_offset r9, 20
    .cfi_rel_offset ip, 24
    .cfi_rel_offset lr, 28

    .ifnc \reg, r0
      mov   r0, \reg                    @ pass arg1 - obj from `reg`
    .endif

    vpush {s0-s15}                      @ save floating-point caller-save registers
    .cfi_adjust_cfa_offset 64
    bl    artReadBarrierMark            @ r0 <- artReadBarrierMark(obj)
    vpop {s0-s15}                       @ restore floating-point registers
    .cfi_adjust_cfa_offset -64

    .ifc \reg, r0                       @ Save result to the stack slot or destination register.
      str r0, [sp, #0]
    .else
      .ifc \reg, r1
        str r0, [sp, #4]
      .else
        .ifc \reg, r2
          str r0, [sp, #8]
        .else
          .ifc \reg, r3
            str r0, [sp, #12]
          .else
            .ifc \reg, r4
              str r0, [sp, #16]
            .else
              .ifc \reg, r9
                str r0, [sp, #20]
              .else
                mov \reg, r0
              .endif
            .endif
          .endif
        .endif
      .endif
    .endif

    pop   {r0-r4, r9, ip, lr}           @ restore caller-save registers
    .cfi_adjust_cfa_offset -32
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r4
    .cfi_restore r9
    .cfi_restore ip
    .cfi_restore lr
    bx lr
.Lret_forwarding_address\name:
    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    lsl \reg, ip, #LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    bx lr
END \name
.endm

READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg00, r0
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg01, r1
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg02, r2
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg03, r3
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg04, r4
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg05, r5
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg06, r6
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg07, r7
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg08, r8
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg09, r9
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg10, r10
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg11, r11

// Helper macros for Baker CC read barrier mark introspection (BRBMI).
.macro BRBMI_FOR_12_REGISTERS macro_for_register, macro_for_reserved_register
    \macro_for_register r0
    \macro_for_register r1
    \macro_for_register r2
    \macro_for_register r3
    \macro_for_reserved_register  // R4 is reserved for the entrypoint address.
    \macro_for_register r5
    \macro_for_register r6
    \macro_for_register r7
    \macro_for_register r8
    \macro_for_register r9
    \macro_for_register r10
    \macro_for_register r11
.endm

.macro BRBMI_FOR_REGISTERS macro_for_register, macro_for_reserved_register
    BRBMI_FOR_12_REGISTERS \macro_for_register, \macro_for_reserved_register
    \macro_for_reserved_register  // IP is reserved.
    \macro_for_reserved_register  // SP is reserved.
    \macro_for_reserved_register  // LR is reserved.
    \macro_for_reserved_register  // PC is reserved.
.endm

.macro BRBMI_RETURN_SWITCH_CASE reg
.Lmark_introspection_return_switch_case_\reg:
    mov     \reg, ip
    bx      lr
.endm

.macro BRBMI_BAD_RETURN_SWITCH_CASE
.Lmark_introspection_return_switch_case_bad:
    BRBMI_BKPT_FILL_4B
.endm

.macro BRBMI_RETURN_SWITCH_CASE_OFFSET reg
    .byte   (.Lmark_introspection_return_switch_case_\reg - .Lmark_introspection_return_table) / 2
.endm

.macro BRBMI_BAD_RETURN_SWITCH_CASE_OFFSET
    .byte   (.Lmark_introspection_return_switch_case_bad - .Lmark_introspection_return_table) / 2
.endm

#if BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET != BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#error "Array and field introspection code sharing requires same LDR offset."
#endif
.macro BRBMI_ARRAY_LOAD index_reg
    ldr     ip, [ip, \index_reg, lsl #2]                // 4 bytes.
    b       art_quick_read_barrier_mark_introspection   // Should be 2 bytes, encoding T2.
    .balign 8                                           // Add padding to 8 bytes.
.endm

.macro BRBMI_BKPT_FILL_4B
    bkpt    0
    bkpt    0
.endm

.macro BRBMI_BKPT_FILL_8B
    BRBMI_BKPT_FILL_4B
    BRBMI_BKPT_FILL_4B
.endm

.macro BRBMI_RUNTIME_CALL
    // Note: This macro generates exactly 22 bytes of code. The core register
    // PUSH and the MOVs are 16-bit instructions, the rest is 32-bit instructions.

    push   {r0-r3, r7, lr}            // Save return address and caller-save registers.
    .cfi_adjust_cfa_offset 24
    .cfi_rel_offset r0, 0
    .cfi_rel_offset r1, 4
    .cfi_rel_offset r2, 8
    .cfi_rel_offset r3, 12
    .cfi_rel_offset r7, 16
    .cfi_rel_offset lr, 20

    mov     r0, ip                    // Pass the reference.
    vpush {s0-s15}                    // save floating-point caller-save registers
    .cfi_adjust_cfa_offset 64
    bl      artReadBarrierMark        // r0 <- artReadBarrierMark(obj)
    vpop    {s0-s15}                  // restore floating-point registers
    .cfi_adjust_cfa_offset -64
    mov     ip, r0                    // Move reference to ip in preparation for return switch.

    pop     {r0-r3, r7, lr}           // Restore registers.
    .cfi_adjust_cfa_offset -24
    .cfi_restore r0
    .cfi_restore r1
    .cfi_restore r2
    .cfi_restore r3
    .cfi_restore r7
    .cfi_restore lr
.endm

.macro BRBMI_CHECK_NULL_AND_MARKED label_suffix
    // If reference is null, just return it in the right register.
    cmp     ip, #0
    beq     .Lmark_introspection_return\label_suffix
    // Use R4 as temp and check the mark bit of the reference.
    ldr     r4, [ip, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    tst     r4, #LOCK_WORD_MARK_BIT_MASK_SHIFTED
    beq     .Lmark_introspection_unmarked\label_suffix
.Lmark_introspection_return\label_suffix:
.endm

.macro BRBMI_UNMARKED_FORWARDING_ADDRESS_CHECK label_suffix
.Lmark_introspection_unmarked\label_suffix:
    // Check if the top two bits are one, if this is the case it is a forwarding address.
#if (LOCK_WORD_STATE_SHIFT != 30) || (LOCK_WORD_STATE_FORWARDING_ADDRESS != 3)
    // To use "CMP ip, #modified-immediate; BHS", we need the lock word state in
    // the highest bits and the "forwarding address" state to have all bits set.
#error "Unexpected lock word state shift or forwarding address state value."
#endif
    cmp     r4, #(LOCK_WORD_STATE_FORWARDING_ADDRESS << LOCK_WORD_STATE_SHIFT)
    bhs     .Lmark_introspection_forwarding_address\label_suffix
.endm

.macro BRBMI_EXTRACT_FORWARDING_ADDRESS label_suffix
.Lmark_introspection_forwarding_address\label_suffix:
    // Note: This macro generates exactly 22 bytes of code, the branch is near.

    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    lsl     ip, r4, #LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    b       .Lmark_introspection_return\label_suffix
.endm

.macro BRBMI_LOAD_RETURN_REG_FROM_CODE_wide ldr_offset
    // Load the half of the instruction that contains Rt. Adjust for the thumb state in LR.
    ldrh    r4, [lr, #(-1 + \ldr_offset + 2)]
.endm

.macro BRBMI_LOAD_RETURN_REG_FROM_CODE_narrow ldr_offset
    // Load the 16-bit instruction. Adjust for the thumb state in LR.
    ldrh    r4, [lr, #(-1 + \ldr_offset)]
.endm

.macro BRBMI_GC_ROOT_AND_FIELD_SLOW_PATH gc_root_ldr_offset, label_suffix
    .balign 64
    .thumb_func
    .type art_quick_read_barrier_mark_introspection_gc_roots\label_suffix, #function
    .hidden art_quick_read_barrier_mark_introspection_gc_roots\label_suffix
    .global art_quick_read_barrier_mark_introspection_gc_roots\label_suffix
art_quick_read_barrier_mark_introspection_gc_roots\label_suffix:
    BRBMI_RUNTIME_CALL
    // Load the LDR (or the half of it) that contains Rt.
    BRBMI_LOAD_RETURN_REG_FROM_CODE\label_suffix \gc_root_ldr_offset
    b       .Lmark_introspection_extract_register_and_return\label_suffix
    // We've used 28 bytes since the "gc_roots" entrypoint (22 bytes for
    // BRBMI_RUNTIME_CALL, 4 bytes for LDRH and 2 bytes for the branch). Squeeze
    // the 6 byte forwarding address extraction here across the 32-byte boundary.
    BRBMI_EXTRACT_FORWARDING_ADDRESS \label_suffix
    // And the slow path taking exactly 30 bytes (6 bytes for the forwarding
    // address check, 22 bytes for BRBMI_RUNTIME_CALL and 2 bytes for the near
    // branch) shall take the rest of the 32-byte section (within a cache line).
    BRBMI_UNMARKED_FORWARDING_ADDRESS_CHECK \label_suffix
    BRBMI_RUNTIME_CALL
    b       .Lmark_introspection_return\label_suffix
.endm

    /*
     * Use introspection to load a reference from the same address as the LDR
     * instruction in generated code would load (unless loaded by the thunk,
     * see below), call ReadBarrier::Mark() with that reference if needed
     * and return it in the same register as the LDR instruction would load.
     *
     * The entrypoint is called through a thunk that differs across load kinds.
     * For field and array loads the LDR instruction in generated code follows
     * the branch to the thunk, i.e. the LDR is (ignoring the heap poisoning)
     * at [LR, #(-4 - 1)] (encoding T3) or [LR, #(-2 - 1)] (encoding T1) where
     * the -1 is an adjustment for the Thumb mode bit in LR, and the thunk
     * knows the holder and performs the gray bit check, returning to the LDR
     * instruction if the object is not gray, so this entrypoint no longer
     * needs to know anything about the holder. For GC root loads, the LDR
     * instruction in generated code precedes the branch to the thunk, i.e. the
     * LDR is at [LR, #(-8 - 1)] (encoding T3) or [LR, #(-6 - 1)] (encoding T1)
     * where the -1 is again the Thumb mode bit adjustment, and the thunk does
     * not do the gray bit check.
     *
     * For field accesses and array loads with a constant index the thunk loads
     * the reference into IP using introspection and calls the main entrypoint,
     * art_quick_read_barrier_mark_introspection. With heap poisoning enabled,
     * the passed reference is poisoned.
     *
     * For array accesses with non-constant index, the thunk inserts the bits
     * 0-5 of the LDR instruction to the entrypoint address, effectively
     * calculating a switch case label based on the index register (bits 0-3)
     * and adding an extra offset (bits 4-5 hold the shift which is always 2
     * for reference loads) to differentiate from the main entrypoint, then
     * moves the base register to IP and jumps to the switch case. Therefore
     * we need to align the main entrypoint to 512 bytes, accounting for
     * a 256-byte offset followed by 16 array entrypoints starting at
     * art_quick_read_barrier_mark_introspection_arrays, each containing an LDR
     * (register) and a branch to the main entrypoint.
     *
     * For GC root accesses we cannot use the main entrypoint because of the
     * different offset where the LDR instruction in generated code is located.
     * (And even with heap poisoning enabled, GC roots are not poisoned.)
     * To re-use the same entrypoint pointer in generated code, we make sure
     * that the gc root entrypoint (a copy of the entrypoint with a different
     * offset for introspection loads) is located at a known offset (128 bytes,
     * or BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET) from the main
     * entrypoint and the GC root thunk adjusts the entrypoint pointer, moves
     * the root register to IP and jumps to the customized entrypoint,
     * art_quick_read_barrier_mark_introspection_gc_roots. The thunk also
     * performs all the fast-path checks, so we need just the slow path.
     *
     * The code structure is
     *   art_quick_read_barrier_mark_introspection:
     *     Up to 32 bytes code for main entrypoint fast-path code for fields
     *     (and array elements with constant offset) with LDR encoding T3;
     *     jumps to the switch in the "narrow" entrypoint.
     *     Padding to 32 bytes if needed.
     *   art_quick_read_barrier_mark_introspection_narrow:
     *     Up to 48 bytes code for fast path code for fields (and array
     *     elements with constant offset) with LDR encoding T1, ending in the
     *     return switch instruction TBB and the table with switch offsets.
     *     Padding to 80 bytes if needed.
     *   .Lmark_introspection_return_switch_case_r0:
     *     Exactly 48 bytes of code for the return switch cases (12 cases,
     *     including BKPT for the reserved registers).
     *     Ends at 128 bytes total.
     *   art_quick_read_barrier_mark_introspection_gc_roots_wide:
     *     GC root entrypoint code for LDR encoding T3 (28 bytes).
     *     Forwarding address extraction for LDR encoding T3 (6 bytes).
     *     Slow path for main entrypoint for LDR encoding T3 (30 bytes).
     *     Ends at 192 bytes total.
     *   art_quick_read_barrier_mark_introspection_gc_roots_narrow:
     *     GC root entrypoint code for LDR encoding T1 (28 bytes).
     *     Forwarding address extraction for LDR encoding T1 (6 bytes).
     *     Slow path for main entrypoint for LDR encoding T1 (30 bytes).
     *     Ends at 256 bytes total.
     *   art_quick_read_barrier_mark_introspection_arrays:
     *     Exactly 128 bytes for array load switch cases (16x2 instructions).
     */
    .balign 512
ENTRY art_quick_read_barrier_mark_introspection
    // At this point, IP contains the reference, R4 can be freely used.
    // (R4 is reserved for the entrypoint address.)
    // For heap poisoning, the reference is poisoned, so unpoison it first.
    UNPOISON_HEAP_REF ip
    // Check for null or marked, lock word is loaded into IP.
    BRBMI_CHECK_NULL_AND_MARKED _wide
    // Load the half of the instruction that contains Rt.
    BRBMI_LOAD_RETURN_REG_FROM_CODE_wide BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET
.Lmark_introspection_extract_register_and_return_wide:
    lsr     r4, r4, #12               // Extract `ref_reg`.
    b       .Lmark_introspection_return_switch

    .balign 32
    .thumb_func
    .type art_quick_read_barrier_mark_introspection_narrow, #function
    .hidden art_quick_read_barrier_mark_introspection_narrow
    .global art_quick_read_barrier_mark_introspection_narrow
art_quick_read_barrier_mark_introspection_narrow:
    // At this point, IP contains the reference, R4 can be freely used.
    // (R4 is reserved for the entrypoint address.)
    // For heap poisoning, the reference is poisoned, so unpoison it first.
    UNPOISON_HEAP_REF ip
    // Check for null or marked, lock word is loaded into R4.
    BRBMI_CHECK_NULL_AND_MARKED _narrow
    // Load the 16-bit instruction.
    BRBMI_LOAD_RETURN_REG_FROM_CODE_narrow BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET
.Lmark_introspection_extract_register_and_return_narrow:
    and     r4, r4, #7                // Extract `ref_reg`.
.Lmark_introspection_return_switch:
    tbb     [pc, r4]                  // Jump to the switch case.
.Lmark_introspection_return_table:
    BRBMI_FOR_REGISTERS BRBMI_RETURN_SWITCH_CASE_OFFSET, BRBMI_BAD_RETURN_SWITCH_CASE_OFFSET
    .balign 16
    BRBMI_FOR_12_REGISTERS BRBMI_RETURN_SWITCH_CASE, BRBMI_BAD_RETURN_SWITCH_CASE

    BRBMI_GC_ROOT_AND_FIELD_SLOW_PATH BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_OFFSET, _wide
    BRBMI_GC_ROOT_AND_FIELD_SLOW_PATH BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_OFFSET, _narrow

    .balign 256
    .thumb_func
    .type art_quick_read_barrier_mark_introspection_arrays, #function
    .hidden art_quick_read_barrier_mark_introspection_arrays
    .global art_quick_read_barrier_mark_introspection_arrays
art_quick_read_barrier_mark_introspection_arrays:
    BRBMI_FOR_REGISTERS BRBMI_ARRAY_LOAD, BRBMI_BKPT_FILL_8B
END art_quick_read_barrier_mark_introspection

.extern artInvokePolymorphic
ENTRY art_quick_invoke_polymorphic
    SETUP_SAVE_REFS_AND_ARGS_FRAME r2
    mov     r2, r9                 @ pass Thread::Current
    mov     r3, sp                 @ pass SP
    mov     r0, #0                 @ initialize 64-bit JValue as zero.
    str     r0, [sp, #-4]!
    .cfi_adjust_cfa_offset 4
    str     r0, [sp, #-4]!
    .cfi_adjust_cfa_offset 4
    mov     r0, sp                 @ pass JValue for return result as first argument.
    bl      artInvokePolymorphic   @ artInvokePolymorphic(JValue, receiver, Thread*, SP)
    sub     r0, 'A'                @ return value is descriptor of handle's return type.
    cmp     r0, 'Z' - 'A'          @ check if value is in bounds of handler table
    bgt     .Lcleanup_and_return   @ and clean-up if not.
    adr     r1, .Lhandler_table
    tbb     [r0, r1]               @ branch to handler for return value based on return type.

.Lstart_of_handlers:
.Lstore_boolean_result:
    ldrb    r0, [sp]               @ Copy boolean value to return value of this function.
    b       .Lcleanup_and_return
.Lstore_char_result:
    ldrh    r0, [sp]               @ Copy char value to return value of this function.
    b       .Lcleanup_and_return
.Lstore_float_result:
    vldr    s0, [sp]               @ Copy float value from JValue result to the context restored by
    vstr    s0, [sp, #16]          @ RESTORE_SAVE_REFS_AND_ARGS_FRAME.
    b       .Lcleanup_and_return
.Lstore_double_result:
    vldr    d0, [sp]               @ Copy double value from JValue result to the context restored by
    vstr    d0, [sp, #16]          @ RESTORE_SAVE_REFS_AND_ARGS_FRAME.
    b       .Lcleanup_and_return
.Lstore_long_result:
    ldr     r1, [sp, #4]           @ Copy the upper bits from JValue result to the context restored by
    str     r1, [sp, #80]          @ RESTORE_SAVE_REFS_AND_ARGS_FRAME.
    // Fall-through for lower bits.
.Lstore_int_result:
    ldr     r0, [sp]               @ Copy int value to return value of this function.
    // Fall-through to clean up and return.
.Lcleanup_and_return:
    add     sp, #8
    .cfi_adjust_cfa_offset -8
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_OR_DELIVER_PENDING_EXCEPTION_REG r2

.macro HANDLER_TABLE_OFFSET handler_label
    .byte (\handler_label - .Lstart_of_handlers) / 2
.endm

.Lhandler_table:
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // A
    HANDLER_TABLE_OFFSET(.Lstore_int_result)      // B (byte)
    HANDLER_TABLE_OFFSET(.Lstore_char_result)     // C (char)
    HANDLER_TABLE_OFFSET(.Lstore_double_result)   // D (double)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // E
    HANDLER_TABLE_OFFSET(.Lstore_float_result)    // F (float)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // G
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // H
    HANDLER_TABLE_OFFSET(.Lstore_int_result)      // I (int)
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // J (long)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // K
    HANDLER_TABLE_OFFSET(.Lstore_int_result)      // L (object)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // M
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // N
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // O
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // P
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // Q
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // R
    HANDLER_TABLE_OFFSET(.Lstore_int_result)      // S (short)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // T
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // U
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // V (void)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // W
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // X
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // Y
    HANDLER_TABLE_OFFSET(.Lstore_boolean_result)  // Z (boolean)
.purgem HANDLER_TABLE_OFFSET
END art_quick_invoke_polymorphic

// Wrap ExecuteSwitchImpl in assembly method which specifies DEX PC for unwinding.
//  Argument 0: r0: The context pointer for ExecuteSwitchImpl.
//  Argument 1: r1: Pointer to the templated ExecuteSwitchImpl to call.
//  Argument 2: r2: The value of DEX PC (memory address of the methods bytecode).
ENTRY ExecuteSwitchImplAsm
    push {r4, lr}                                 // 2 words of callee saves.
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset r4, 0
    .cfi_rel_offset lr, 4
    mov r4, r2                                    // r4 = DEX PC
    CFI_DEFINE_DEX_PC_WITH_OFFSET(0 /* r0 */, 4 /* r4 */, 0)
    blx r1                                        // Call the wrapped method.
    pop {r4, pc}
END ExecuteSwitchImplAsm
