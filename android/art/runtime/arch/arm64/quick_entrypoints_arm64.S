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

#include "asm_support_arm64.S"
#include "interpreter/cfi_asm_support.h"

#include "arch/quick_alloc_entrypoints.S"


.macro INCREASE_FRAME frame_adjustment
    sub sp, sp, #(\frame_adjustment)
    .cfi_adjust_cfa_offset (\frame_adjustment)
.endm

.macro DECREASE_FRAME frame_adjustment
    add sp, sp, #(\frame_adjustment)
    .cfi_adjust_cfa_offset -(\frame_adjustment)
.endm

.macro SAVE_REG reg, offset
    str \reg, [sp, #(\offset)]
    .cfi_rel_offset \reg, (\offset)
.endm

.macro RESTORE_REG reg, offset
    ldr \reg, [sp, #(\offset)]
    .cfi_restore \reg
.endm

.macro SAVE_REG_INCREASE_FRAME reg, frame_adjustment
    str \reg, [sp, #-(\frame_adjustment)]!
    .cfi_adjust_cfa_offset (\frame_adjustment)
    .cfi_rel_offset \reg, 0
.endm

.macro RESTORE_REG_DECREASE_FRAME reg, frame_adjustment
    ldr \reg, [sp], #(\frame_adjustment)
    .cfi_restore \reg
    .cfi_adjust_cfa_offset -(\frame_adjustment)
.endm

.macro SAVE_TWO_REGS reg1, reg2, offset
    stp \reg1, \reg2, [sp, #(\offset)]
    .cfi_rel_offset \reg1, (\offset)
    .cfi_rel_offset \reg2, (\offset) + 8
.endm

.macro RESTORE_TWO_REGS reg1, reg2, offset
    ldp \reg1, \reg2, [sp, #(\offset)]
    .cfi_restore \reg1
    .cfi_restore \reg2
.endm

.macro SAVE_TWO_REGS_INCREASE_FRAME reg1, reg2, frame_adjustment
    stp \reg1, \reg2, [sp, #-(\frame_adjustment)]!
    .cfi_adjust_cfa_offset (\frame_adjustment)
    .cfi_rel_offset \reg1, 0
    .cfi_rel_offset \reg2, 8
.endm

.macro RESTORE_TWO_REGS_DECREASE_FRAME reg1, reg2, frame_adjustment
    ldp \reg1, \reg2, [sp], #(\frame_adjustment)
    .cfi_restore \reg1
    .cfi_restore \reg2
    .cfi_adjust_cfa_offset -(\frame_adjustment)
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAllCalleeSaves)
     */
.macro SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    // art::Runtime** xIP0 = &art::Runtime::instance_
    adrp xIP0, :got:_ZN3art7Runtime9instance_E
    ldr xIP0, [xIP0, #:got_lo12:_ZN3art7Runtime9instance_E]

    // Our registers aren't intermixed - just spill in order.
    ldr xIP0, [xIP0]  // art::Runtime* xIP0 = art::Runtime::instance_;

    // ArtMethod* xIP0 = Runtime::instance_->callee_save_methods_[kSaveAllCalleeSaves];
    ldr xIP0, [xIP0, RUNTIME_SAVE_ALL_CALLEE_SAVES_METHOD_OFFSET]

    INCREASE_FRAME 176

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_ALL_CALLEE_SAVES != 176)
#error "FRAME_SIZE_SAVE_ALL_CALLEE_SAVES(ARM64) size not as expected."
#endif

    // Stack alignment filler [sp, #8].
    // FP callee-saves.
    stp d8, d9,   [sp, #16]
    stp d10, d11, [sp, #32]
    stp d12, d13, [sp, #48]
    stp d14, d15, [sp, #64]

    // GP callee-saves
    SAVE_TWO_REGS x19, x20, 80
    SAVE_TWO_REGS x21, x22, 96
    SAVE_TWO_REGS x23, x24, 112
    SAVE_TWO_REGS x25, x26, 128
    SAVE_TWO_REGS x27, x28, 144
    SAVE_TWO_REGS x29, xLR, 160

    // Store ArtMethod* Runtime::callee_save_methods_[kSaveAllCalleeSaves].
    str xIP0, [sp]
    // Place sp in Thread::Current()->top_quick_frame.
    mov xIP0, sp
    str xIP0, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsOnly).
     */
.macro SETUP_SAVE_REFS_ONLY_FRAME
    // art::Runtime** xIP0 = &art::Runtime::instance_
    adrp xIP0, :got:_ZN3art7Runtime9instance_E
    ldr xIP0, [xIP0, #:got_lo12:_ZN3art7Runtime9instance_E]

    // Our registers aren't intermixed - just spill in order.
    ldr xIP0, [xIP0]  // art::Runtime* xIP0 = art::Runtime::instance_;

    // ArtMethod* xIP0 = Runtime::instance_->callee_save_methods_[kSaveRefOnly];
    ldr xIP0, [xIP0, RUNTIME_SAVE_REFS_ONLY_METHOD_OFFSET]

    INCREASE_FRAME 96

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_ONLY != 96)
#error "FRAME_SIZE_SAVE_REFS_ONLY(ARM64) size not as expected."
#endif

    // GP callee-saves.
    // x20 paired with ArtMethod* - see below.
    SAVE_TWO_REGS x21, x22, 16
    SAVE_TWO_REGS x23, x24, 32
    SAVE_TWO_REGS x25, x26, 48
    SAVE_TWO_REGS x27, x28, 64
    SAVE_TWO_REGS x29, xLR, 80

    // Store ArtMethod* Runtime::callee_save_methods_[kSaveRefsOnly].
    // Note: We could avoid saving X20 in the case of Baker read
    // barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    stp xIP0, x20, [sp]
    .cfi_rel_offset x20, 8

    // Place sp in Thread::Current()->top_quick_frame.
    mov xIP0, sp
    str xIP0, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
.endm

// TODO: Probably no need to restore registers preserved by aapcs64.
.macro RESTORE_SAVE_REFS_ONLY_FRAME
    // Callee-saves.
    // Note: Likewise, we could avoid restoring X20 in the case of Baker
    // read barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    RESTORE_REG x20, 8
    RESTORE_TWO_REGS x21, x22, 16
    RESTORE_TWO_REGS x23, x24, 32
    RESTORE_TWO_REGS x25, x26, 48
    RESTORE_TWO_REGS x27, x28, 64
    RESTORE_TWO_REGS x29, xLR, 80

    DECREASE_FRAME 96
.endm

.macro POP_SAVE_REFS_ONLY_FRAME
    DECREASE_FRAME 96
.endm


.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
    INCREASE_FRAME 224

    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_REFS_AND_ARGS != 224)
#error "FRAME_SIZE_SAVE_REFS_AND_ARGS(ARM64) size not as expected."
#endif

    // Stack alignment filler [sp, #8].
    // FP args.
    stp d0, d1, [sp, #16]
    stp d2, d3, [sp, #32]
    stp d4, d5, [sp, #48]
    stp d6, d7, [sp, #64]

    // Core args.
    SAVE_TWO_REGS x1, x2, 80
    SAVE_TWO_REGS x3, x4, 96
    SAVE_TWO_REGS x5, x6, 112

    // x7, Callee-saves.
    // Note: We could avoid saving X20 in the case of Baker read
    // barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    SAVE_TWO_REGS x7, x20, 128
    SAVE_TWO_REGS x21, x22, 144
    SAVE_TWO_REGS x23, x24, 160
    SAVE_TWO_REGS x25, x26, 176
    SAVE_TWO_REGS x27, x28, 192

    // x29(callee-save) and LR.
    SAVE_TWO_REGS x29, xLR, 208

.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveRefsAndArgs).
     *
     * TODO This is probably too conservative - saving FP & LR.
     */
.macro SETUP_SAVE_REFS_AND_ARGS_FRAME
    // art::Runtime** xIP0 = &art::Runtime::instance_
    adrp xIP0, :got:_ZN3art7Runtime9instance_E
    ldr xIP0, [xIP0, #:got_lo12:_ZN3art7Runtime9instance_E]

    // Our registers aren't intermixed - just spill in order.
    ldr xIP0, [xIP0]  // art::Runtime* xIP0 = art::Runtime::instance_;

    // ArtMethod* xIP0 = Runtime::instance_->callee_save_methods_[kSaveRefAndArgs];
    ldr xIP0, [xIP0, RUNTIME_SAVE_REFS_AND_ARGS_METHOD_OFFSET]

    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL

    str xIP0, [sp]    // Store ArtMethod* Runtime::callee_save_methods_[kSaveRefsAndArgs].
    // Place sp in Thread::Current()->top_quick_frame.
    mov xIP0, sp
    str xIP0, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
.endm

.macro SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_X0
    SETUP_SAVE_REFS_AND_ARGS_FRAME_INTERNAL
    str x0, [sp, #0]  // Store ArtMethod* to bottom of stack.
    // Place sp in Thread::Current()->top_quick_frame.
    mov xIP0, sp
    str xIP0, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
.endm

// TODO: Probably no need to restore registers preserved by aapcs64.
.macro RESTORE_SAVE_REFS_AND_ARGS_FRAME
    // FP args.
    ldp d0, d1, [sp, #16]
    ldp d2, d3, [sp, #32]
    ldp d4, d5, [sp, #48]
    ldp d6, d7, [sp, #64]

    // Core args.
    RESTORE_TWO_REGS x1, x2, 80
    RESTORE_TWO_REGS x3, x4, 96
    RESTORE_TWO_REGS x5, x6, 112

    // x7, Callee-saves.
    // Note: Likewise, we could avoid restoring X20 in the case of Baker
    // read barriers, as it is overwritten by REFRESH_MARKING_REGISTER
    // later; but it's not worth handling this special case.
    RESTORE_TWO_REGS x7, x20, 128
    RESTORE_TWO_REGS x21, x22, 144
    RESTORE_TWO_REGS x23, x24, 160
    RESTORE_TWO_REGS x25, x26, 176
    RESTORE_TWO_REGS x27, x28, 192

    // x29(callee-save) and LR.
    RESTORE_TWO_REGS x29, xLR, 208

    DECREASE_FRAME 224
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     * when the SP has already been decremented by FRAME_SIZE_SAVE_EVERYTHING
     * and saving registers x29 and LR is handled elsewhere.
     */
.macro SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP_SKIP_X29_LR runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    // Ugly compile-time check, but we only have the preprocessor.
#if (FRAME_SIZE_SAVE_EVERYTHING != 512)
#error "FRAME_SIZE_SAVE_EVERYTHING(ARM64) size not as expected."
#endif

    // Save FP registers.
    // For better performance, store d0 and d31 separately, so that all STPs are 16-byte aligned.
    str d0,       [sp, #8]
    stp d1, d2,   [sp, #16]
    stp d3, d4,   [sp, #32]
    stp d5, d6,   [sp, #48]
    stp d7, d8,   [sp, #64]
    stp d9, d10,  [sp, #80]
    stp d11, d12, [sp, #96]
    stp d13, d14, [sp, #112]
    stp d15, d16, [sp, #128]
    stp d17, d18, [sp, #144]
    stp d19, d20, [sp, #160]
    stp d21, d22, [sp, #176]
    stp d23, d24, [sp, #192]
    stp d25, d26, [sp, #208]
    stp d27, d28, [sp, #224]
    stp d29, d30, [sp, #240]
    str d31,      [sp, #256]

    // Save core registers.
    SAVE_REG            x0, 264
    SAVE_TWO_REGS  x1,  x2, 272
    SAVE_TWO_REGS  x3,  x4, 288
    SAVE_TWO_REGS  x5,  x6, 304
    SAVE_TWO_REGS  x7,  x8, 320
    SAVE_TWO_REGS  x9, x10, 336
    SAVE_TWO_REGS x11, x12, 352
    SAVE_TWO_REGS x13, x14, 368
    SAVE_TWO_REGS x15, x16, 384
    SAVE_TWO_REGS x17, x18, 400
    SAVE_TWO_REGS x19, x20, 416
    SAVE_TWO_REGS x21, x22, 432
    SAVE_TWO_REGS x23, x24, 448
    SAVE_TWO_REGS x25, x26, 464
    SAVE_TWO_REGS x27, x28, 480

    // art::Runtime** xIP0 = &art::Runtime::instance_
    adrp xIP0, :got:_ZN3art7Runtime9instance_E
    ldr xIP0, [xIP0, #:got_lo12:_ZN3art7Runtime9instance_E]

    ldr xIP0, [xIP0]  // art::Runtime* xIP0 = art::Runtime::instance_;

    // ArtMethod* xIP0 = Runtime::instance_->callee_save_methods_[kSaveEverything];
    ldr xIP0, [xIP0, \runtime_method_offset]

    // Store ArtMethod* Runtime::callee_save_methods_[kSaveEverything].
    str xIP0, [sp]
    // Place sp in Thread::Current()->top_quick_frame.
    mov xIP0, sp
    str xIP0, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
.endm

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveEverything)
     */
.macro SETUP_SAVE_EVERYTHING_FRAME runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    INCREASE_FRAME 512
    SAVE_TWO_REGS x29, xLR, 496
    SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP_SKIP_X29_LR \runtime_method_offset
.endm

.macro RESTORE_SAVE_EVERYTHING_FRAME_KEEP_X0
    // Restore FP registers.
    // For better performance, load d0 and d31 separately, so that all LDPs are 16-byte aligned.
    ldr d0,       [sp, #8]
    ldp d1, d2,   [sp, #16]
    ldp d3, d4,   [sp, #32]
    ldp d5, d6,   [sp, #48]
    ldp d7, d8,   [sp, #64]
    ldp d9, d10,  [sp, #80]
    ldp d11, d12, [sp, #96]
    ldp d13, d14, [sp, #112]
    ldp d15, d16, [sp, #128]
    ldp d17, d18, [sp, #144]
    ldp d19, d20, [sp, #160]
    ldp d21, d22, [sp, #176]
    ldp d23, d24, [sp, #192]
    ldp d25, d26, [sp, #208]
    ldp d27, d28, [sp, #224]
    ldp d29, d30, [sp, #240]
    ldr d31,      [sp, #256]

    // Restore core registers, except x0.
    RESTORE_TWO_REGS  x1,  x2, 272
    RESTORE_TWO_REGS  x3,  x4, 288
    RESTORE_TWO_REGS  x5,  x6, 304
    RESTORE_TWO_REGS  x7,  x8, 320
    RESTORE_TWO_REGS  x9, x10, 336
    RESTORE_TWO_REGS x11, x12, 352
    RESTORE_TWO_REGS x13, x14, 368
    RESTORE_TWO_REGS x15, x16, 384
    RESTORE_TWO_REGS x17, x18, 400
    RESTORE_TWO_REGS x19, x20, 416
    RESTORE_TWO_REGS x21, x22, 432
    RESTORE_TWO_REGS x23, x24, 448
    RESTORE_TWO_REGS x25, x26, 464
    RESTORE_TWO_REGS x27, x28, 480
    RESTORE_TWO_REGS x29, xLR, 496

    DECREASE_FRAME 512
.endm

.macro RESTORE_SAVE_EVERYTHING_FRAME
    RESTORE_REG  x0, 264
    RESTORE_SAVE_EVERYTHING_FRAME_KEEP_X0
.endm

// Macro to refresh the Marking Register (W20).
//
// This macro must be called at the end of functions implementing
// entrypoints that possibly (directly or indirectly) perform a
// suspend check (before they return).
.macro REFRESH_MARKING_REGISTER
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
    ldr wMR, [xSELF, #THREAD_IS_GC_MARKING_OFFSET]
#endif
.endm

.macro RETURN_IF_RESULT_IS_ZERO
    cbnz x0, 1f                // result non-zero branch over
    ret                        // return
1:
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO
    cbz x0, 1f                 // result zero branch over
    ret                        // return
1:
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_ when the runtime method frame is ready.
     */
.macro DELIVER_PENDING_EXCEPTION_FRAME_READY
    mov x0, xSELF

    // Point of no return.
    bl artDeliverPendingExceptionFromCode  // artDeliverPendingExceptionFromCode(Thread*)
    brk 0  // Unreached
.endm

    /*
     * Macro that calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
.macro DELIVER_PENDING_EXCEPTION
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.endm

.macro RETURN_OR_DELIVER_PENDING_EXCEPTION_REG reg
    ldr \reg, [xSELF, # THREAD_EXCEPTION_OFFSET]   // Get exception field.
    cbnz \reg, 1f
    ret
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro RETURN_OR_DELIVER_PENDING_EXCEPTION
    RETURN_OR_DELIVER_PENDING_EXCEPTION_REG xIP0
.endm

// Same as above with x1. This is helpful in stubs that want to avoid clobbering another register.
.macro RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
    RETURN_OR_DELIVER_PENDING_EXCEPTION_REG x1
.endm

.macro RETURN_IF_W0_IS_ZERO_OR_DELIVER
    cbnz w0, 1f                // result non-zero branch over
    ret                        // return
1:
    DELIVER_PENDING_EXCEPTION
.endm

.macro NO_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME // save all registers as basis for long jump context
    mov x0, xSELF                     // pass Thread::Current
    bl  \cxx_name                     // \cxx_name(Thread*)
    brk 0
END \c_name
.endm

.macro NO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_EVERYTHING_FRAME       // save all registers as basis for long jump context
    mov x0, xSELF                     // pass Thread::Current
    bl  \cxx_name                     // \cxx_name(Thread*)
    brk 0
END \c_name
.endm

.macro ONE_ARG_RUNTIME_EXCEPTION c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME // save all registers as basis for long jump context.
    mov x1, xSELF                     // pass Thread::Current.
    bl  \cxx_name                     // \cxx_name(arg, Thread*).
    brk 0
END \c_name
.endm

.macro TWO_ARG_RUNTIME_EXCEPTION_SAVE_EVERYTHING c_name, cxx_name
    .extern \cxx_name
ENTRY \c_name
    SETUP_SAVE_EVERYTHING_FRAME       // save all registers as basis for long jump context
    mov x2, xSELF                     // pass Thread::Current
    bl  \cxx_name                     // \cxx_name(arg1, arg2, Thread*)
    brk 0
END \c_name
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
    // Save all registers as basis for long jump context.
    INCREASE_FRAME (FRAME_SIZE_SAVE_EVERYTHING - __SIZEOF_POINTER__)
    SAVE_REG x29, (FRAME_SIZE_SAVE_EVERYTHING - 2 * __SIZEOF_POINTER__)  // LR already saved.
    SETUP_SAVE_EVERYTHING_FRAME_DECREMENTED_SP_SKIP_X29_LR
    mov x0, lr                        // pass the fault address stored in LR by the fault handler.
    mov x1, xSELF                     // pass Thread::Current.
    bl  artThrowNullPointerExceptionFromSignal  // (arg, Thread*).
    brk 0
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
     * as usual - except instead of loading arg0/x0 with the target Method*, arg0/x0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/x1.
     *
     * The helper will attempt to locate the target and return a 128-bit result in x0/x1 consisting
     * of the target Method* in x0 and method->code_ in x1.
     *
     * If unsuccessful, the helper will return null/????. There will be a pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     *
     * Adapted from ARM32 code.
     *
     * Clobbers xIP0.
     */
.macro INVOKE_TRAMPOLINE_BODY cxx_name
    .extern \cxx_name
    SETUP_SAVE_REFS_AND_ARGS_FRAME        // save callee saves in case allocation triggers GC
    // Helper signature is always
    // (method_idx, *this_object, *caller_method, *self, sp)

    mov    x2, xSELF                      // pass Thread::Current
    mov    x3, sp
    bl     \cxx_name                      // (method_idx, this, Thread*, SP)
    mov    xIP0, x1                       // save Method*->code_
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    cbz    x0, 1f                         // did we find the target? if not go to exception delivery
    br     xIP0                           // tail call to target
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


.macro INVOKE_STUB_CREATE_FRAME

SAVE_SIZE=15*8   // x4, x5, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, SP, LR, FP saved.
SAVE_SIZE_AND_METHOD=SAVE_SIZE+8


    mov x9, sp                             // Save stack pointer.
    .cfi_register sp,x9

    add x10, x2, # SAVE_SIZE_AND_METHOD    // calculate size of frame.
    sub x10, sp, x10                       // Calculate SP position - saves + ArtMethod* + args
    and x10, x10, # ~0xf                   // Enforce 16 byte stack alignment.
    mov sp, x10                            // Set new SP.

    sub x10, x9, #SAVE_SIZE                // Calculate new FP (later). Done here as we must move SP
    .cfi_def_cfa_register x10              // before this.
    .cfi_adjust_cfa_offset SAVE_SIZE

    str x28, [x10, #112]
    .cfi_rel_offset x28, 112

    stp x26, x27, [x10, #96]
    .cfi_rel_offset x26, 96
    .cfi_rel_offset x27, 104

    stp x24, x25, [x10, #80]
    .cfi_rel_offset x24, 80
    .cfi_rel_offset x25, 88

    stp x22, x23, [x10, #64]
    .cfi_rel_offset x22, 64
    .cfi_rel_offset x23, 72

    stp x20, x21, [x10, #48]
    .cfi_rel_offset x20, 48
    .cfi_rel_offset x21, 56

    stp x9, x19, [x10, #32]                // Save old stack pointer and x19.
    .cfi_rel_offset sp, 32
    .cfi_rel_offset x19, 40

    stp x4, x5, [x10, #16]                 // Save result and shorty addresses.
    .cfi_rel_offset x4, 16
    .cfi_rel_offset x5, 24

    stp xFP, xLR, [x10]                    // Store LR & FP.
    .cfi_rel_offset x29, 0
    .cfi_rel_offset x30, 8

    mov xFP, x10                           // Use xFP now, as it's callee-saved.
    .cfi_def_cfa_register x29
    mov xSELF, x3                          // Move thread pointer into SELF register.

    // Copy arguments into stack frame.
    // Use simple copy routine for now.
    // 4 bytes per slot.
    // X1 - source address
    // W2 - args length
    // X9 - destination address.
    // W10 - temporary
    add x9, sp, #8                         // Destination address is bottom of stack + null.

    // Copy parameters into the stack. Use numeric label as this is a macro and Clang's assembler
    // does not have unique-id variables.
1:
    cmp w2, #0
    beq 2f
    sub w2, w2, #4      // Need 65536 bytes of range.
    ldr w10, [x1, x2]
    str w10, [x9, x2]

    b 1b

2:
    // Store null into ArtMethod* at bottom of frame.
    str xzr, [sp]
.endm

.macro INVOKE_STUB_CALL_AND_RETURN

    REFRESH_MARKING_REGISTER

    // load method-> METHOD_QUICK_CODE_OFFSET
    ldr x9, [x0, #ART_METHOD_QUICK_CODE_OFFSET_64]
    // Branch to method.
    blr x9

    // Restore return value address and shorty address.
    ldp x4, x5, [xFP, #16]
    .cfi_restore x4
    .cfi_restore x5

    ldr x28, [xFP, #112]
    .cfi_restore x28

    ldp x26, x27, [xFP, #96]
    .cfi_restore x26
    .cfi_restore x27

    ldp x24, x25, [xFP, #80]
    .cfi_restore x24
    .cfi_restore x25

    ldp x22, x23, [xFP, #64]
    .cfi_restore x22
    .cfi_restore x23

    ldp x20, x21, [xFP, #48]
    .cfi_restore x20
    .cfi_restore x21

    // Store result (w0/x0/s0/d0) appropriately, depending on resultType.
    ldrb w10, [x5]

    // Check the return type and store the correct register into the jvalue in memory.
    // Use numeric label as this is a macro and Clang's assembler does not have unique-id variables.

    // Don't set anything for a void type.
    cmp w10, #'V'
    beq 3f

    // Is it a double?
    cmp w10, #'D'
    bne 1f
    str d0, [x4]
    b 3f

1:  // Is it a float?
    cmp w10, #'F'
    bne 2f
    str s0, [x4]
    b 3f

2:  // Just store x0. Doesn't matter if it is 64 or 32 bits.
    str x0, [x4]

3:  // Finish up.
    ldp x2, x19, [xFP, #32]   // Restore stack pointer and x19.
    .cfi_restore x19
    mov sp, x2
    .cfi_restore sp

    ldp xFP, xLR, [xFP]    // Restore old frame pointer and link register.
    .cfi_restore x29
    .cfi_restore x30

    ret

.endm


/*
 *  extern"C" void art_quick_invoke_stub(ArtMethod *method,   x0
 *                                       uint32_t  *args,     x1
 *                                       uint32_t argsize,    w2
 *                                       Thread *self,        x3
 *                                       JValue *result,      x4
 *                                       char   *shorty);     x5
 *  +----------------------+
 *  |                      |
 *  |  C/C++ frame         |
 *  |       LR''           |
 *  |       FP''           | <- SP'
 *  +----------------------+
 *  +----------------------+
 *  |        x28           | <- TODO: Remove callee-saves.
 *  |         :            |
 *  |        x19           |
 *  |        SP'           |
 *  |        X5            |
 *  |        X4            |        Saved registers
 *  |        LR'           |
 *  |        FP'           | <- FP
 *  +----------------------+
 *  | uint32_t out[n-1]    |
 *  |    :      :          |        Outs
 *  | uint32_t out[0]      |
 *  | ArtMethod*           | <- SP  value=null
 *  +----------------------+
 *
 * Outgoing registers:
 *  x0    - Method*
 *  x1-x7 - integer parameters.
 *  d0-d7 - Floating point parameters.
 *  xSELF = self
 *  SP = & of ArtMethod*
 *  x1 = "this" pointer.
 *
 */
ENTRY art_quick_invoke_stub
    // Spill registers as per AACPS64 calling convention.
    INVOKE_STUB_CREATE_FRAME

    // Fill registers x/w1 to x/w7 and s/d0 to s/d7 with parameters.
    // Parse the passed shorty to determine which register to load.
    // Load addresses for routines that load WXSD registers.
    adr  x11, .LstoreW2
    adr  x12, .LstoreX2
    adr  x13, .LstoreS0
    adr  x14, .LstoreD0

    // Initialize routine offsets to 0 for integers and floats.
    // x8 for integers, x15 for floating point.
    mov x8, #0
    mov x15, #0

    add x10, x5, #1         // Load shorty address, plus one to skip return value.
    ldr w1, [x9],#4         // Load "this" parameter, and increment arg pointer.

    // Loop to fill registers.
.LfillRegisters:
    ldrb w17, [x10], #1       // Load next character in signature, and increment.
    cbz w17, .LcallFunction   // Exit at end of signature. Shorty 0 terminated.

    cmp  w17, #'F' // is this a float?
    bne .LisDouble

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .Ladvance4

    add x17, x13, x15       // Calculate subroutine to jump to.
    br  x17

.LisDouble:
    cmp w17, #'D'           // is this a double?
    bne .LisLong

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .Ladvance8

    add x17, x14, x15       // Calculate subroutine to jump to.
    br x17

.LisLong:
    cmp w17, #'J'           // is this a long?
    bne .LisOther

    cmp x8, # 6*12          // Skip this load if all registers full.
    beq .Ladvance8

    add x17, x12, x8        // Calculate subroutine to jump to.
    br x17

.LisOther:                  // Everything else takes one vReg.
    cmp x8, # 6*12          // Skip this load if all registers full.
    beq .Ladvance4

    add x17, x11, x8        // Calculate subroutine to jump to.
    br x17

.Ladvance4:
    add x9, x9, #4
    b .LfillRegisters

.Ladvance8:
    add x9, x9, #8
    b .LfillRegisters

// Macro for loading a parameter into a register.
//  counter - the register with offset into these tables
//  size - the size of the register - 4 or 8 bytes.
//  register - the name of the register to be loaded.
.macro LOADREG counter size register return
    ldr \register , [x9], #\size
    add \counter, \counter, 12
    b \return
.endm

// Store ints.
.LstoreW2:
    LOADREG x8 4 w2 .LfillRegisters
    LOADREG x8 4 w3 .LfillRegisters
    LOADREG x8 4 w4 .LfillRegisters
    LOADREG x8 4 w5 .LfillRegisters
    LOADREG x8 4 w6 .LfillRegisters
    LOADREG x8 4 w7 .LfillRegisters

// Store longs.
.LstoreX2:
    LOADREG x8 8 x2 .LfillRegisters
    LOADREG x8 8 x3 .LfillRegisters
    LOADREG x8 8 x4 .LfillRegisters
    LOADREG x8 8 x5 .LfillRegisters
    LOADREG x8 8 x6 .LfillRegisters
    LOADREG x8 8 x7 .LfillRegisters

// Store singles.
.LstoreS0:
    LOADREG x15 4 s0 .LfillRegisters
    LOADREG x15 4 s1 .LfillRegisters
    LOADREG x15 4 s2 .LfillRegisters
    LOADREG x15 4 s3 .LfillRegisters
    LOADREG x15 4 s4 .LfillRegisters
    LOADREG x15 4 s5 .LfillRegisters
    LOADREG x15 4 s6 .LfillRegisters
    LOADREG x15 4 s7 .LfillRegisters

// Store doubles.
.LstoreD0:
    LOADREG x15 8 d0 .LfillRegisters
    LOADREG x15 8 d1 .LfillRegisters
    LOADREG x15 8 d2 .LfillRegisters
    LOADREG x15 8 d3 .LfillRegisters
    LOADREG x15 8 d4 .LfillRegisters
    LOADREG x15 8 d5 .LfillRegisters
    LOADREG x15 8 d6 .LfillRegisters
    LOADREG x15 8 d7 .LfillRegisters


.LcallFunction:

    INVOKE_STUB_CALL_AND_RETURN

END art_quick_invoke_stub

/*  extern"C"
 *     void art_quick_invoke_static_stub(ArtMethod *method,   x0
 *                                       uint32_t  *args,     x1
 *                                       uint32_t argsize,    w2
 *                                       Thread *self,        x3
 *                                       JValue *result,      x4
 *                                       char   *shorty);     x5
 */
ENTRY art_quick_invoke_static_stub
    // Spill registers as per AACPS64 calling convention.
    INVOKE_STUB_CREATE_FRAME

    // Fill registers x/w1 to x/w7 and s/d0 to s/d7 with parameters.
    // Parse the passed shorty to determine which register to load.
    // Load addresses for routines that load WXSD registers.
    adr  x11, .LstoreW1_2
    adr  x12, .LstoreX1_2
    adr  x13, .LstoreS0_2
    adr  x14, .LstoreD0_2

    // Initialize routine offsets to 0 for integers and floats.
    // x8 for integers, x15 for floating point.
    mov x8, #0
    mov x15, #0

    add x10, x5, #1     // Load shorty address, plus one to skip return value.

    // Loop to fill registers.
.LfillRegisters2:
    ldrb w17, [x10], #1         // Load next character in signature, and increment.
    cbz w17, .LcallFunction2    // Exit at end of signature. Shorty 0 terminated.

    cmp  w17, #'F'          // is this a float?
    bne .LisDouble2

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .Ladvance4_2

    add x17, x13, x15       // Calculate subroutine to jump to.
    br  x17

.LisDouble2:
    cmp w17, #'D'           // is this a double?
    bne .LisLong2

    cmp x15, # 8*12         // Skip this load if all registers full.
    beq .Ladvance8_2

    add x17, x14, x15       // Calculate subroutine to jump to.
    br x17

.LisLong2:
    cmp w17, #'J'           // is this a long?
    bne .LisOther2

    cmp x8, # 7*12          // Skip this load if all registers full.
    beq .Ladvance8_2

    add x17, x12, x8        // Calculate subroutine to jump to.
    br x17

.LisOther2:                 // Everything else takes one vReg.
    cmp x8, # 7*12          // Skip this load if all registers full.
    beq .Ladvance4_2

    add x17, x11, x8        // Calculate subroutine to jump to.
    br x17

.Ladvance4_2:
    add x9, x9, #4
    b .LfillRegisters2

.Ladvance8_2:
    add x9, x9, #8
    b .LfillRegisters2

// Store ints.
.LstoreW1_2:
    LOADREG x8 4 w1 .LfillRegisters2
    LOADREG x8 4 w2 .LfillRegisters2
    LOADREG x8 4 w3 .LfillRegisters2
    LOADREG x8 4 w4 .LfillRegisters2
    LOADREG x8 4 w5 .LfillRegisters2
    LOADREG x8 4 w6 .LfillRegisters2
    LOADREG x8 4 w7 .LfillRegisters2

// Store longs.
.LstoreX1_2:
    LOADREG x8 8 x1 .LfillRegisters2
    LOADREG x8 8 x2 .LfillRegisters2
    LOADREG x8 8 x3 .LfillRegisters2
    LOADREG x8 8 x4 .LfillRegisters2
    LOADREG x8 8 x5 .LfillRegisters2
    LOADREG x8 8 x6 .LfillRegisters2
    LOADREG x8 8 x7 .LfillRegisters2

// Store singles.
.LstoreS0_2:
    LOADREG x15 4 s0 .LfillRegisters2
    LOADREG x15 4 s1 .LfillRegisters2
    LOADREG x15 4 s2 .LfillRegisters2
    LOADREG x15 4 s3 .LfillRegisters2
    LOADREG x15 4 s4 .LfillRegisters2
    LOADREG x15 4 s5 .LfillRegisters2
    LOADREG x15 4 s6 .LfillRegisters2
    LOADREG x15 4 s7 .LfillRegisters2

// Store doubles.
.LstoreD0_2:
    LOADREG x15 8 d0 .LfillRegisters2
    LOADREG x15 8 d1 .LfillRegisters2
    LOADREG x15 8 d2 .LfillRegisters2
    LOADREG x15 8 d3 .LfillRegisters2
    LOADREG x15 8 d4 .LfillRegisters2
    LOADREG x15 8 d5 .LfillRegisters2
    LOADREG x15 8 d6 .LfillRegisters2
    LOADREG x15 8 d7 .LfillRegisters2


.LcallFunction2:

    INVOKE_STUB_CALL_AND_RETURN

END art_quick_invoke_static_stub



/*  extern"C" void art_quick_osr_stub(void** stack,                x0
 *                                    size_t stack_size_in_bytes,  x1
 *                                    const uin8_t* native_pc,     x2
 *                                    JValue *result,              x3
 *                                    char   *shorty,              x4
 *                                    Thread *self)                x5
 */
ENTRY art_quick_osr_stub
SAVE_SIZE=14*8   // x3, x4, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, FP, LR saved.
    SAVE_TWO_REGS_INCREASE_FRAME x3, x4, SAVE_SIZE
    SAVE_TWO_REGS x19, x20, 16
    SAVE_TWO_REGS x21, x22, 32
    SAVE_TWO_REGS x23, x24, 48
    SAVE_TWO_REGS x25, x26, 64
    SAVE_TWO_REGS x27, x28, 80
    SAVE_TWO_REGS xFP, xLR, 96

    mov xSELF, x5                         // Move thread pointer into SELF register.
    REFRESH_MARKING_REGISTER

    INCREASE_FRAME 16
    str xzr, [sp]                         // Store null for ArtMethod* slot
    // Branch to stub.
    bl .Losr_entry
    .cfi_remember_state
    DECREASE_FRAME 16

    // Restore saved registers including value address and shorty address.
    RESTORE_TWO_REGS x19, x20, 16
    RESTORE_TWO_REGS x21, x22, 32
    RESTORE_TWO_REGS x23, x24, 48
    RESTORE_TWO_REGS x25, x26, 64
    RESTORE_TWO_REGS x27, x28, 80
    RESTORE_TWO_REGS xFP, xLR, 96
    RESTORE_TWO_REGS_DECREASE_FRAME x3, x4, SAVE_SIZE

    // Store result (w0/x0/s0/d0) appropriately, depending on resultType.
    ldrb w10, [x4]

    // Check the return type and store the correct register into the jvalue in memory.

    // Don't set anything for a void type.
    cmp w10, #'V'
    beq .Losr_exit
    // Is it a double?
    cmp w10, #'D'
    beq .Losr_return_double
    // Is it a float?
    cmp w10, #'F'
    beq .Losr_return_float
    // Just store x0. Doesn't matter if it is 64 or 32 bits.
    str x0, [x3]
.Losr_exit:
    ret
.Losr_return_double:
    str d0, [x3]
    ret
.Losr_return_float:
    str s0, [x3]
    ret

.Losr_entry:
    .cfi_restore_state                     // Reset unwind info so following code unwinds.
    .cfi_def_cfa_offset (SAVE_SIZE+16)     // workaround for clang bug: 31975598

    mov x9, sp                             // Save stack pointer.
    .cfi_def_cfa_register x9

    // Update stack pointer for the callee
    sub sp, sp, x1

    // Update link register slot expected by the callee.
    sub w1, w1, #8
    str lr, [sp, x1]

    // Copy arguments into stack frame.
    // Use simple copy routine for now.
    // 4 bytes per slot.
    // X0 - source address
    // W1 - args length
    // SP - destination address.
    // W10 - temporary
.Losr_loop_entry:
    cbz w1, .Losr_loop_exit
    sub w1, w1, #4
    ldr w10, [x0, x1]
    str w10, [sp, x1]
    b .Losr_loop_entry

.Losr_loop_exit:
    // Branch to the OSR entry point.
    br x2

END art_quick_osr_stub

    /*
     * On entry x0 is uintptr_t* gprs_ and x1 is uint64_t* fprs_
     */

ENTRY art_quick_do_long_jump
    // Load FPRs
    ldp d0, d1, [x1], #16
    ldp d2, d3, [x1], #16
    ldp d4, d5, [x1], #16
    ldp d6, d7, [x1], #16
    ldp d8, d9, [x1], #16
    ldp d10, d11, [x1], #16
    ldp d12, d13, [x1], #16
    ldp d14, d15, [x1], #16
    ldp d16, d17, [x1], #16
    ldp d18, d19, [x1], #16
    ldp d20, d21, [x1], #16
    ldp d22, d23, [x1], #16
    ldp d24, d25, [x1], #16
    ldp d26, d27, [x1], #16
    ldp d28, d29, [x1], #16
    ldp d30, d31, [x1]

    // Load GPRs
    // TODO: lots of those are smashed, could optimize.
    add x0, x0, #30*8
    ldp x30, x1, [x0], #-16          // LR & SP
    ldp x28, x29, [x0], #-16
    ldp x26, x27, [x0], #-16
    ldp x24, x25, [x0], #-16
    ldp x22, x23, [x0], #-16
    ldp x20, x21, [x0], #-16
    ldp x18, x19, [x0], #-16         // X18 & xSELF
    ldp x16, x17, [x0], #-16
    ldp x14, x15, [x0], #-16
    ldp x12, x13, [x0], #-16
    ldp x10, x11, [x0], #-16
    ldp x8, x9, [x0], #-16
    ldp x6, x7, [x0], #-16
    ldp x4, x5, [x0], #-16
    ldp x2, x3, [x0], #-16
    mov sp, x1

    REFRESH_MARKING_REGISTER

    // Need to load PC, it's at the end (after the space for the unused XZR). Use x1.
    ldr x1, [x0, #33*8]
    // And the value of x0.
    ldr x0, [x0]

    br  x1
END art_quick_do_long_jump

    /*
     * Entry from managed code that calls artLockObjectFromCode, may block for GC. x0 holds the
     * possibly null object to lock.
     *
     * Derived from arm32 code.
     */
    .extern artLockObjectFromCode
ENTRY art_quick_lock_object
    cbz    w0, .Lslow_lock
    add    x4, x0, #MIRROR_OBJECT_LOCK_WORD_OFFSET  // exclusive load/store has no immediate anymore
.Lretry_lock:
    ldr    w2, [xSELF, #THREAD_ID_OFFSET] // TODO: Can the thread ID really change during the loop?
    ldaxr  w1, [x4]                   // acquire needed only in most common case
    and    w3, w1, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  // zero the gc bits
    cbnz   w3, .Lnot_unlocked         // already thin locked
    // unlocked case - x1: original lock word that's zero except for the read barrier bits.
    orr    x2, x1, x2                 // x2 holds thread id with count of 0 with preserved read barrier bits
    stxr   w3, w2, [x4]
    cbnz   w3, .Llock_stxr_fail       // store failed, retry
    ret
.Lnot_unlocked:  // x1: original lock word
    lsr    w3, w1, LOCK_WORD_STATE_SHIFT
    cbnz   w3, .Lslow_lock            // if either of the top two bits are set, go slow path
    eor    w2, w1, w2                 // lock_word.ThreadId() ^ self->ThreadId()
    uxth   w2, w2                     // zero top 16 bits
    cbnz   w2, .Lslow_lock            // lock word and self thread id's match -> recursive lock
                                      // else contention, go to slow path
    and    w3, w1, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  // zero the gc bits.
    add    w2, w3, #LOCK_WORD_THIN_LOCK_COUNT_ONE  // increment count in lock word placing in w2 to check overflow
    lsr    w3, w2, #LOCK_WORD_GC_STATE_SHIFT     // if the first gc state bit is set, we overflowed.
    cbnz   w3, .Lslow_lock            // if we overflow the count go slow path
    add    w2, w1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  // increment count for real
    stxr   w3, w2, [x4]
    cbnz   w3, .Llock_stxr_fail       // store failed, retry
    ret
.Llock_stxr_fail:
    b      .Lretry_lock               // retry
.Lslow_lock:
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case we block
    mov    x1, xSELF                  // pass Thread::Current
    bl     artLockObjectFromCode      // (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_W0_IS_ZERO_OR_DELIVER
END art_quick_lock_object

ENTRY art_quick_lock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case we block
    mov    x1, xSELF                  // pass Thread::Current
    bl     artLockObjectFromCode      // (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_W0_IS_ZERO_OR_DELIVER
END art_quick_lock_object_no_inline

    /*
     * Entry from managed code that calls artUnlockObjectFromCode and delivers exception on failure.
     * x0 holds the possibly null object to lock.
     *
     * Derived from arm32 code.
     */
    .extern artUnlockObjectFromCode
ENTRY art_quick_unlock_object
    cbz    x0, .Lslow_unlock
    add    x4, x0, #MIRROR_OBJECT_LOCK_WORD_OFFSET  // exclusive load/store has no immediate anymore
.Lretry_unlock:
#ifndef USE_READ_BARRIER
    ldr    w1, [x4]
#else
    ldxr   w1, [x4]                   // Need to use atomic instructions for read barrier
#endif
    lsr    w2, w1, LOCK_WORD_STATE_SHIFT
    cbnz   w2, .Lslow_unlock          // if either of the top two bits are set, go slow path
    ldr    w2, [xSELF, #THREAD_ID_OFFSET]
    and    w3, w1, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  // zero the gc bits
    eor    w3, w3, w2                 // lock_word.ThreadId() ^ self->ThreadId()
    uxth   w3, w3                     // zero top 16 bits
    cbnz   w3, .Lslow_unlock          // do lock word and self thread id's match?
    and    w3, w1, #LOCK_WORD_GC_STATE_MASK_SHIFTED_TOGGLED  // zero the gc bits
    cmp    w3, #LOCK_WORD_THIN_LOCK_COUNT_ONE
    bpl    .Lrecursive_thin_unlock
    // transition to unlocked
    and    w3, w1, #LOCK_WORD_GC_STATE_MASK_SHIFTED  // w3: zero except for the preserved read barrier bits
#ifndef USE_READ_BARRIER
    stlr   w3, [x4]
#else
    stlxr  w2, w3, [x4]               // Need to use atomic instructions for read barrier
    cbnz   w2, .Lunlock_stxr_fail     // store failed, retry
#endif
    ret
.Lrecursive_thin_unlock:  // w1: original lock word
    sub    w1, w1, #LOCK_WORD_THIN_LOCK_COUNT_ONE  // decrement count
#ifndef USE_READ_BARRIER
    str    w1, [x4]
#else
    stxr   w2, w1, [x4]               // Need to use atomic instructions for read barrier
    cbnz   w2, .Lunlock_stxr_fail     // store failed, retry
#endif
    ret
.Lunlock_stxr_fail:
    b      .Lretry_unlock             // retry
.Lslow_unlock:
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case exception allocation triggers GC
    mov    x1, xSELF                  // pass Thread::Current
    bl     artUnlockObjectFromCode    // (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_W0_IS_ZERO_OR_DELIVER
END art_quick_unlock_object

ENTRY art_quick_unlock_object_no_inline
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case exception allocation triggers GC
    mov    x1, xSELF                  // pass Thread::Current
    bl     artUnlockObjectFromCode    // (Object* obj, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_W0_IS_ZERO_OR_DELIVER
END art_quick_unlock_object_no_inline

    /*
     * Entry from managed code that calls artInstanceOfFromCode and on failure calls
     * artThrowClassCastExceptionForObject.
     */
    .extern artInstanceOfFromCode
    .extern artThrowClassCastExceptionForObject
ENTRY art_quick_check_instance_of
    // Store arguments and link register
    // Stack needs to be 16B aligned on calls.
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, 32
    SAVE_REG xLR, 24

    // Call runtime code
    bl artInstanceOfFromCode

    // Restore LR.
    RESTORE_REG xLR, 24

    // Check for exception
    cbz x0, .Lthrow_class_cast_exception

    // Restore and return
    .cfi_remember_state
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, 32
    ret
    .cfi_restore_state                // Reset unwind info so following code unwinds.
    .cfi_def_cfa_offset 32            // workaround for clang bug: 31975598

.Lthrow_class_cast_exception:
    // Restore
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, 32

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME // save all registers as basis for long jump context
    mov x2, xSELF                     // pass Thread::Current
    bl artThrowClassCastExceptionForObject     // (Object*, Class*, Thread*)
    brk 0                             // We should not return here...
END art_quick_check_instance_of

// Restore xReg's value from [sp, #offset] if xReg is not the same as xExclude.
.macro POP_REG_NE xReg, offset, xExclude
    .ifnc \xReg, \xExclude
        ldr \xReg, [sp, #\offset]     // restore xReg
        .cfi_restore \xReg
    .endif
.endm

// Restore xReg1's value from [sp, #offset] if xReg1 is not the same as xExclude.
// Restore xReg2's value from [sp, #(offset + 8)] if xReg2 is not the same as xExclude.
.macro POP_REGS_NE xReg1, xReg2, offset, xExclude
    .ifc \xReg1, \xExclude
        ldr \xReg2, [sp, #(\offset + 8)]        // restore xReg2
    .else
        .ifc \xReg2, \xExclude
            ldr \xReg1, [sp, #\offset]          // restore xReg1
        .else
            ldp \xReg1, \xReg2, [sp, #\offset]  // restore xReg1 and xReg2
        .endif
    .endif
    .cfi_restore \xReg1
    .cfi_restore \xReg2
.endm

    /*
     * Macro to insert read barrier, only used in art_quick_aput_obj.
     * xDest, wDest and xObj are registers, offset is a defined literal such as
     * MIRROR_OBJECT_CLASS_OFFSET. Dest needs both x and w versions of the same register to handle
     * name mismatch between instructions. This macro uses the lower 32b of register when possible.
     * TODO: When read barrier has a fast path, add heap unpoisoning support for the fast path.
     */
.macro READ_BARRIER xDest, wDest, xObj, xTemp, wTemp, offset, number
#ifdef USE_READ_BARRIER
# ifdef USE_BAKER_READ_BARRIER
    ldr \wTemp, [\xObj, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    tbnz \wTemp, #LOCK_WORD_READ_BARRIER_STATE_SHIFT, .Lrb_slowpath\number
    // False dependency to avoid needing load/load fence.
    add \xObj, \xObj, \xTemp, lsr #32
    ldr \wDest, [\xObj, #\offset]   // Heap reference = 32b. This also zero-extends to \xDest.
    UNPOISON_HEAP_REF \wDest
    b .Lrb_exit\number
# endif  // USE_BAKER_READ_BARRIER
.Lrb_slowpath\number:
    // Store registers used in art_quick_aput_obj (x0-x4, LR), stack is 16B aligned.
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, 48
    SAVE_TWO_REGS x2, x3, 16
    SAVE_TWO_REGS x4, xLR, 32

    // mov x0, \xRef                // pass ref in x0 (no-op for now since parameter ref is unused)
    .ifnc \xObj, x1
        mov x1, \xObj               // pass xObj
    .endif
    mov w2, #\offset                // pass offset
    bl artReadBarrierSlow           // artReadBarrierSlow(ref, xObj, offset)
    // No need to unpoison return value in w0, artReadBarrierSlow() would do the unpoisoning.
    .ifnc \wDest, w0
        mov \wDest, w0              // save return value in wDest
    .endif

    // Conditionally restore saved registers
    POP_REG_NE x0, 0, \xDest
    POP_REG_NE x1, 8, \xDest
    POP_REG_NE x2, 16, \xDest
    POP_REG_NE x3, 24, \xDest
    POP_REG_NE x4, 32, \xDest
    RESTORE_REG xLR, 40
    DECREASE_FRAME 48
.Lrb_exit\number:
#else
    ldr \wDest, [\xObj, #\offset]   // Heap reference = 32b. This also zero-extends to \xDest.
    UNPOISON_HEAP_REF \wDest
#endif  // USE_READ_BARRIER
.endm

#ifdef USE_READ_BARRIER
    .extern artReadBarrierSlow
#endif
ENTRY art_quick_aput_obj
    cbz x2, .Ldo_aput_null
    READ_BARRIER x3, w3, x0, x3, w3, MIRROR_OBJECT_CLASS_OFFSET, 0  // Heap reference = 32b
                                                                    // This also zero-extends to x3
    READ_BARRIER x3, w3, x3, x4, w4, MIRROR_CLASS_COMPONENT_TYPE_OFFSET, 1 // Heap reference = 32b
    // This also zero-extends to x3
    READ_BARRIER x4, w4, x2, x4, w4, MIRROR_OBJECT_CLASS_OFFSET, 2  // Heap reference = 32b
                                                                    // This also zero-extends to x4
    cmp w3, w4  // value's type == array's component type - trivial assignability
    bne .Lcheck_assignability
.Ldo_aput:
    add x3, x0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
                                                         // "Compress" = do nothing
    POISON_HEAP_REF w2
    str w2, [x3, x1, lsl #2]                             // Heap reference = 32b
    ldr x3, [xSELF, #THREAD_CARD_TABLE_OFFSET]
    lsr x0, x0, #CARD_TABLE_CARD_SHIFT
    strb w3, [x3, x0]
    ret
.Ldo_aput_null:
    add x3, x0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
                                                         // "Compress" = do nothing
    str w2, [x3, x1, lsl #2]                             // Heap reference = 32b
    ret
.Lcheck_assignability:
    // Store arguments and link register
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, 32
    SAVE_TWO_REGS x2, xLR, 16

    // Call runtime code
    mov x0, x3              // Heap reference, 32b, "uncompress" = do nothing, already zero-extended
    mov x1, x4              // Heap reference, 32b, "uncompress" = do nothing, already zero-extended
    bl artIsAssignableFromCode

    // Check for exception
    cbz x0, .Lthrow_array_store_exception

    // Restore
    .cfi_remember_state
    RESTORE_TWO_REGS x2, xLR, 16
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, 32

    add x3, x0, #MIRROR_OBJECT_ARRAY_DATA_OFFSET
                                                          // "Compress" = do nothing
    POISON_HEAP_REF w2
    str w2, [x3, x1, lsl #2]                              // Heap reference = 32b
    ldr x3, [xSELF, #THREAD_CARD_TABLE_OFFSET]
    lsr x0, x0, #CARD_TABLE_CARD_SHIFT
    strb w3, [x3, x0]
    ret
    .cfi_restore_state            // Reset unwind info so following code unwinds.
    .cfi_def_cfa_offset 32        // workaround for clang bug: 31975598
.Lthrow_array_store_exception:
    RESTORE_TWO_REGS x2, xLR, 16
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, 32

    SETUP_SAVE_ALL_CALLEE_SAVES_FRAME
    mov x1, x2                      // Pass value.
    mov x2, xSELF                   // Pass Thread::Current.
    bl artThrowArrayStoreException  // (Object*, Object*, Thread*).
    brk 0                           // Unreached.
END art_quick_aput_obj

// Macro to facilitate adding new allocation entrypoints.
.macro ONE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x1, xSELF                  // pass Thread::Current
    bl     \entrypoint                // (uint32_t type_idx, Method* method, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro TWO_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x2, xSELF                  // pass Thread::Current
    bl     \entrypoint                // (uint32_t type_idx, Method* method, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro to facilitate adding new allocation entrypoints.
.macro THREE_ARG_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x3, xSELF                  // pass Thread::Current
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
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x4, xSELF                  // pass Thread::Current
    bl     \entrypoint                //
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macros taking opportunity of code similarities for downcalls.
.macro ONE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x1, xSELF                  // pass Thread::Current
    bl     \entrypoint                // (uint32_t type_idx, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

.macro TWO_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x2, xSELF                  // pass Thread::Current
    bl     \entrypoint
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

.macro THREE_ARG_REF_DOWNCALL name, entrypoint, return
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x3, xSELF                  // pass Thread::Current
    bl     \entrypoint
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    \return
END \name
.endm

// Macro for string and type resolution and initialization.
.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL name, entrypoint, runtime_method_offset = RUNTIME_SAVE_EVERYTHING_METHOD_OFFSET
    .extern \entrypoint
ENTRY \name
    SETUP_SAVE_EVERYTHING_FRAME \runtime_method_offset       // save everything for stack crawl
    mov   x1, xSELF                   // pass Thread::Current
    bl    \entrypoint                 // (int32_t index, Thread* self)
    cbz   w0, 1f                      // If result is null, deliver the OOME.
    .cfi_remember_state
    RESTORE_SAVE_EVERYTHING_FRAME_KEEP_X0
    REFRESH_MARKING_REGISTER
    ret                        // return
    .cfi_restore_state
    .cfi_def_cfa_offset FRAME_SIZE_SAVE_EVERYTHING  // workaround for clang bug: 31975598
1:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
END \name
.endm

.macro ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT name, entrypoint
    ONE_ARG_SAVE_EVERYTHING_DOWNCALL \name, \entrypoint, RUNTIME_SAVE_EVERYTHING_FOR_CLINIT_METHOD_OFFSET
.endm

.macro RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
    cbz w0, 1f                 // result zero branch over
    ret                        // return
1:
    DELIVER_PENDING_EXCEPTION
.endm

    /*
     * Entry from managed code that calls artHandleFillArrayDataFromCode and delivers exception on
     * failure.
     */
TWO_ARG_REF_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER

    /*
     * Entry from managed code when uninitialized static storage, this stub will run the class
     * initializer and deliver the exception on error. On success the static storage base is
     * returned.
     */
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_static_storage, artInitializeStaticStorageFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL_FOR_CLINIT art_quick_initialize_type, artInitializeTypeFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode
ONE_ARG_SAVE_EVERYTHING_DOWNCALL art_quick_resolve_string, artResolveStringFromCode

// Note: Functions `art{Get,Set}<Kind>{Static,Instance}FromCompiledCode` are
// defined with a macro in runtime/entrypoints/quick/quick_field_entrypoints.cc.

ONE_ARG_REF_DOWNCALL art_quick_get_boolean_static, artGetBooleanStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get_byte_static, artGetByteStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get_char_static, artGetCharStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get_short_static, artGetShortStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get32_static, artGet32StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get64_static, artGet64StaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
ONE_ARG_REF_DOWNCALL art_quick_get_obj_static, artGetObjStaticFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1

TWO_ARG_REF_DOWNCALL art_quick_get_boolean_instance, artGetBooleanInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get_byte_instance, artGetByteInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get_char_instance, artGetCharInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get_short_instance, artGetShortInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get32_instance, artGet32InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get64_instance, artGet64InstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1
TWO_ARG_REF_DOWNCALL art_quick_get_obj_instance, artGetObjInstanceFromCompiledCode, RETURN_OR_DELIVER_PENDING_EXCEPTION_X1

TWO_ARG_REF_DOWNCALL art_quick_set8_static, artSet8StaticFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set16_static, artSet16StaticFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set32_static, artSet32StaticFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set64_static, artSet64StaticFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
TWO_ARG_REF_DOWNCALL art_quick_set_obj_static, artSetObjStaticFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER

THREE_ARG_REF_DOWNCALL art_quick_set8_instance, artSet8InstanceFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set16_instance, artSet16InstanceFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set32_instance, artSet32InstanceFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set64_instance, artSet64InstanceFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER
THREE_ARG_REF_DOWNCALL art_quick_set_obj_instance, artSetObjInstanceFromCompiledCode, RETURN_IF_W0_IS_ZERO_OR_DELIVER

// Generate the allocation entrypoints for each allocator.
GENERATE_ALLOC_ENTRYPOINTS_FOR_NON_TLAB_ALLOCATORS
// Comment out allocators that have arm64 specific asm.
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

// If isInitialized=1 then the compiler assumes the object's class has already been initialized.
// If isInitialized=0 the compiler can only assume it's been at least resolved.
.macro ART_QUICK_ALLOC_OBJECT_ROSALLOC c_name, cxx_name, isInitialized
ENTRY \c_name
    // Fast path rosalloc allocation.
    // x0: type, xSELF(x19): Thread::Current
    // x1-x7: free.
    ldr    x3, [xSELF, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]  // Check if the thread local
                                                              // allocation stack has room.
                                                              // ldp won't work due to large offset.
    ldr    x4, [xSELF, #THREAD_LOCAL_ALLOC_STACK_END_OFFSET]
    cmp    x3, x4
    bhs    .Lslow_path\c_name
    ldr    w3, [x0, #MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET]  // Load the object size (x3)
    cmp    x3, #ROSALLOC_MAX_THREAD_LOCAL_BRACKET_SIZE        // Check if the size is for a thread
                                                              // local allocation. Also does the
                                                              // finalizable and initialization
                                                              // checks.
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
    add    x4, xSELF, x3, lsr #(ROSALLOC_BRACKET_QUANTUM_SIZE_SHIFT - POINTER_SIZE_SHIFT)
                                                              // Subtract pointer size since ther
                                                              // are no runs for 0 byte allocations
                                                              // and the size is already aligned.
    ldr    x4, [x4, #(THREAD_ROSALLOC_RUNS_OFFSET - __SIZEOF_POINTER__)]
                                                              // Load the free list head (x3). This
                                                              // will be the return val.
    ldr    x3, [x4, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)]
    cbz    x3, .Lslow_path\c_name
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber x0 and x1.
    ldr    x1, [x3, #ROSALLOC_SLOT_NEXT_OFFSET]               // Load the next pointer of the head
                                                              // and update the list head with the
                                                              // next pointer.
    str    x1, [x4, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_HEAD_OFFSET)]
                                                              // Store the class pointer in the
                                                              // header. This also overwrites the
                                                              // next pointer. The offsets are
                                                              // asserted to match.

#if ROSALLOC_SLOT_NEXT_OFFSET != MIRROR_OBJECT_CLASS_OFFSET
#error "Class pointer needs to overwrite next pointer."
#endif
    POISON_HEAP_REF w0
    str    w0, [x3, #MIRROR_OBJECT_CLASS_OFFSET]
                                                              // Push the new object onto the thread
                                                              // local allocation stack and
                                                              // increment the thread local
                                                              // allocation stack top.
    ldr    x1, [xSELF, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]
    str    w3, [x1], #COMPRESSED_REFERENCE_SIZE               // (Increment x1 as a side effect.)
    str    x1, [xSELF, #THREAD_LOCAL_ALLOC_STACK_TOP_OFFSET]
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
    ldr    w1, [x4, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)]
    sub    x1, x1, #1
                                                              // TODO: consider combining this store
                                                              // and the list head store above using
                                                              // strd.
    str    w1, [x4, #(ROSALLOC_RUN_FREE_LIST_OFFSET + ROSALLOC_RUN_FREE_LIST_SIZE_OFFSET)]

    mov    x0, x3                                             // Set the return value and return.
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
    ret
.Lslow_path\c_name:
    SETUP_SAVE_REFS_ONLY_FRAME                      // save callee saves in case of GC
    mov    x1, xSELF                                // pass Thread::Current
    bl     \cxx_name
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \c_name
.endm

ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_resolved_rosalloc, artAllocObjectFromCodeResolvedRosAlloc, /* isInitialized */ 0
ART_QUICK_ALLOC_OBJECT_ROSALLOC art_quick_alloc_object_initialized_rosalloc, artAllocObjectFromCodeInitializedRosAlloc, /* isInitialized */ 1

// If isInitialized=1 then the compiler assumes the object's class has already been initialized.
// If isInitialized=0 the compiler can only assume it's been at least resolved.
.macro ALLOC_OBJECT_TLAB_FAST_PATH_RESOLVED slowPathLabel isInitialized
    ldr    x4, [xSELF, #THREAD_LOCAL_POS_OFFSET]
    ldr    x5, [xSELF, #THREAD_LOCAL_END_OFFSET]
    ldr    w7, [x0, #MIRROR_CLASS_OBJECT_SIZE_ALLOC_FAST_PATH_OFFSET]  // Load the object size (x7).
    add    x6, x4, x7                                         // Add object size to tlab pos.
    cmp    x6, x5                                             // Check if it fits, overflow works
                                                              // since the tlab pos and end are 32
                                                              // bit values.

    // When isInitialized == 0, then the class is potentially not yet initialized.
    // If the class is not yet initialized, the object size will be very large to force the branch
    // below to be taken.
    //
    // See InitializeClassVisitors in class-inl.h for more details.
    bhi    \slowPathLabel
    str    x6, [xSELF, #THREAD_LOCAL_POS_OFFSET]              // Store new thread_local_pos.
    ldr    x5, [xSELF, #THREAD_LOCAL_OBJECTS_OFFSET]          // Increment thread_local_objects.
    add    x5, x5, #1
    str    x5, [xSELF, #THREAD_LOCAL_OBJECTS_OFFSET]
    POISON_HEAP_REF w0
    str    w0, [x4, #MIRROR_OBJECT_CLASS_OFFSET]              // Store the class pointer.
                                                              // Fence. This is "ish" not "ishst" so
                                                              // that the code after this allocation
                                                              // site will see the right values in
                                                              // the fields of the class.
    mov    x0, x4
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
    ret
.endm

// The common code for art_quick_alloc_object_*region_tlab
.macro GENERATE_ALLOC_OBJECT_RESOLVED_TLAB name, entrypoint, isInitialized
ENTRY \name
    // Fast path region tlab allocation.
    // x0: type, xSELF(x19): Thread::Current
    // x1-x7: free.
    ALLOC_OBJECT_TLAB_FAST_PATH_RESOLVED .Lslow_path\name, \isInitialized
.Lslow_path\name:
    SETUP_SAVE_REFS_ONLY_FRAME                 // Save callee saves in case of GC.
    mov    x1, xSELF                           // Pass Thread::Current.
    bl     \entrypoint                         // (mirror::Class*, Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_resolved_region_tlab, artAllocObjectFromCodeResolvedRegionTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_initialized_region_tlab, artAllocObjectFromCodeInitializedRegionTLAB, /* isInitialized */ 1
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_resolved_tlab, artAllocObjectFromCodeResolvedTLAB, /* isInitialized */ 0
GENERATE_ALLOC_OBJECT_RESOLVED_TLAB art_quick_alloc_object_initialized_tlab, artAllocObjectFromCodeInitializedTLAB, /* isInitialized */ 1

.macro ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE slowPathLabel, xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    and    \xTemp1, \xTemp1, #OBJECT_ALIGNMENT_MASK_TOGGLED64 // Apply alignment mask
                                                              // (addr + 7) & ~7. The mask must
                                                              // be 64 bits to keep high bits in
                                                              // case of overflow.
    // Negative sized arrays are handled here since xCount holds a zero extended 32 bit value.
    // Negative ints become large 64 bit unsigned ints which will always be larger than max signed
    // 32 bit int. Since the max shift for arrays is 3, it can not become a negative 64 bit int.
    cmp    \xTemp1, #MIN_LARGE_OBJECT_THRESHOLD               // Possibly a large object, go slow
    bhs    \slowPathLabel                                     // path.

    ldr    \xTemp0, [xSELF, #THREAD_LOCAL_POS_OFFSET]         // Check tlab for space, note that
                                                              // we use (end - begin) to handle
                                                              // negative size arrays. It is
                                                              // assumed that a negative size will
                                                              // always be greater unsigned than
                                                              // region size.
    ldr    \xTemp2, [xSELF, #THREAD_LOCAL_END_OFFSET]
    sub    \xTemp2, \xTemp2, \xTemp0
    cmp    \xTemp1, \xTemp2

    // The array class is always initialized here. Unlike new-instance,
    // this does not act as a double test.
    bhi    \slowPathLabel
    // "Point of no slow path". Won't go to the slow path from here on. OK to clobber x0 and x1.
                                                              // Move old thread_local_pos to x0
                                                              // for the return value.
    mov    x0, \xTemp0
    add    \xTemp0, \xTemp0, \xTemp1
    str    \xTemp0, [xSELF, #THREAD_LOCAL_POS_OFFSET]         // Store new thread_local_pos.
    ldr    \xTemp0, [xSELF, #THREAD_LOCAL_OBJECTS_OFFSET]     // Increment thread_local_objects.
    add    \xTemp0, \xTemp0, #1
    str    \xTemp0, [xSELF, #THREAD_LOCAL_OBJECTS_OFFSET]
    POISON_HEAP_REF \wClass
    str    \wClass, [x0, #MIRROR_OBJECT_CLASS_OFFSET]         // Store the class pointer.
    str    \wCount, [x0, #MIRROR_ARRAY_LENGTH_OFFSET]         // Store the array length.
                                                              // Fence.
// new-array is special. The class is loaded and immediately goes to the Initialized state
// before it is published. Therefore the only fence needed is for the publication of the object.
// See ClassLinker::CreateArrayClass() for more details.

// For publication of the new array, we don't need a 'dmb ishst' here.
// The compiler generates 'dmb ishst' for all new-array insts.
    ret
.endm

.macro GENERATE_ALLOC_ARRAY_TLAB name, entrypoint, size_setup
ENTRY \name
    // Fast path array allocation for region tlab allocation.
    // x0: mirror::Class* type
    // x1: int32_t component_count
    // x2-x7: free.
    mov    x3, x0
    \size_setup x3, w3, x1, w1, x4, w4, x5, w5, x6, w6
    ALLOC_ARRAY_TLAB_FAST_PATH_RESOLVED_WITH_SIZE .Lslow_path\name, x3, w3, x1, w1, x4, w4, x5, w5, x6, w6
.Lslow_path\name:
    // x0: mirror::Class* klass
    // x1: int32_t component_count
    // x2: Thread* self
    SETUP_SAVE_REFS_ONLY_FRAME        // save callee saves in case of GC
    mov    x2, xSELF                  // pass Thread::Current
    bl     \entrypoint
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_IF_RESULT_IS_NON_ZERO_OR_DELIVER
END \name
.endm

.macro COMPUTE_ARRAY_SIZE_UNKNOWN xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    // Array classes are never finalizable or uninitialized, no need to check.
    ldr    \wTemp0, [\xClass, #MIRROR_CLASS_COMPONENT_TYPE_OFFSET] // Load component type
    UNPOISON_HEAP_REF \wTemp0
    ldr    \wTemp0, [\xTemp0, #MIRROR_CLASS_OBJECT_PRIMITIVE_TYPE_OFFSET]
    lsr    \xTemp0, \xTemp0, #PRIMITIVE_TYPE_SIZE_SHIFT_SHIFT // Component size shift is in high 16
                                                              // bits.
                                                              // xCount is holding a 32 bit value,
                                                              // it can not overflow.
    lsl    \xTemp1, \xCount, \xTemp0                          // Calculate data size
    // Add array data offset and alignment.
    add    \xTemp1, \xTemp1, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
#if MIRROR_LONG_ARRAY_DATA_OFFSET != MIRROR_INT_ARRAY_DATA_OFFSET + 4
#error Long array data offset must be 4 greater than int array data offset.
#endif

    add    \xTemp0, \xTemp0, #1                               // Add 4 to the length only if the
                                                              // component size shift is 3
                                                              // (for 64 bit alignment).
    and    \xTemp0, \xTemp0, #4
    add    \xTemp1, \xTemp1, \xTemp0
.endm

.macro COMPUTE_ARRAY_SIZE_8 xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    // Add array data offset and alignment.
    add    \xTemp1, \xCount, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_16 xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    lsl    \xTemp1, \xCount, #1
    // Add array data offset and alignment.
    add    \xTemp1, \xTemp1, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_32 xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    lsl    \xTemp1, \xCount, #2
    // Add array data offset and alignment.
    add    \xTemp1, \xTemp1, #(MIRROR_INT_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

.macro COMPUTE_ARRAY_SIZE_64 xClass, wClass, xCount, wCount, xTemp0, wTemp0, xTemp1, wTemp1, xTemp2, wTemp2
    lsl    \xTemp1, \xCount, #3
    // Add array data offset and alignment.
    add    \xTemp1, \xTemp1, #(MIRROR_WIDE_ARRAY_DATA_OFFSET + OBJECT_ALIGNMENT_MASK)
.endm

// TODO(ngeoffray): art_quick_alloc_array_resolved_region_tlab is not used for arm64, remove
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
     * Called by managed code when the thread has been asked to suspend.
     */
    .extern artTestSuspendFromCode
ENTRY art_quick_test_suspend
    SETUP_SAVE_EVERYTHING_FRAME RUNTIME_SAVE_EVERYTHING_FOR_SUSPEND_CHECK_METHOD_OFFSET  // save callee saves for stack crawl
    mov    x0, xSELF
    bl     artTestSuspendFromCode             // (Thread*)
    RESTORE_SAVE_EVERYTHING_FRAME
    REFRESH_MARKING_REGISTER
    ret
END art_quick_test_suspend

ENTRY art_quick_implicit_suspend
    mov    x0, xSELF
    SETUP_SAVE_REFS_ONLY_FRAME                // save callee saves for stack crawl
    bl     artTestSuspendFromCode             // (Thread*)
    RESTORE_SAVE_REFS_ONLY_FRAME
    REFRESH_MARKING_REGISTER
    ret
END art_quick_implicit_suspend

     /*
     * Called by managed code that is attempting to call a method on a proxy class. On entry
     * x0 holds the proxy method and x1 holds the receiver; The frame size of the invoked proxy
     * method agrees with a ref and args callee save frame.
     */
     .extern artQuickProxyInvokeHandler
ENTRY art_quick_proxy_invoke_handler
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_X0
    mov     x2, xSELF                   // pass Thread::Current
    mov     x3, sp                      // pass SP
    bl      artQuickProxyInvokeHandler  // (Method* proxy method, receiver, Thread*, SP)
    ldr     x2, [xSELF, THREAD_EXCEPTION_OFFSET]
    cbnz    x2, .Lexception_in_proxy    // success if no exception is pending
    RESTORE_SAVE_REFS_AND_ARGS_FRAME    // Restore frame
    REFRESH_MARKING_REGISTER
    fmov    d0, x0                      // Store result in d0 in case it was float or double
    ret                                 // return on success
.Lexception_in_proxy:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_proxy_invoke_handler

    /*
     * Called to resolve an imt conflict.
     * x0 is the conflict ArtMethod.
     * xIP1 is a hidden argument that holds the target interface method's dex method index.
     *
     * Note that this stub writes to xIP0, xIP1, x13-x15, and x0.
     */
    .extern artLookupResolvedMethod
ENTRY art_quick_imt_conflict_trampoline
    ldr xIP0, [sp, #0]  // Load referrer
    // Load the declaring class (without read barrier) and access flags (for obsolete method check).
    // The obsolete flag is set with suspended threads, so we do not need an acquire operation here.
#if ART_METHOD_ACCESS_FLAGS_OFFSET != ART_METHOD_DECLARING_CLASS_OFFSET + 4
#error "Expecting declaring class and access flags to be consecutive for LDP."
#endif
    ldp wIP0, w15, [xIP0, #ART_METHOD_DECLARING_CLASS_OFFSET]
    // If the method is obsolete, just go through the dex cache miss slow path.
    tbnz x15, #ACC_OBSOLETE_METHOD_SHIFT, .Limt_conflict_trampoline_dex_cache_miss
    ldr wIP0, [xIP0, #MIRROR_CLASS_DEX_CACHE_OFFSET]  // Load the DexCache (without read barrier).
    UNPOISON_HEAP_REF wIP0
    ubfx x15, xIP1, #0, #METHOD_DEX_CACHE_HASH_BITS  // Calculate DexCache method slot index.
    ldr xIP0, [xIP0, #MIRROR_DEX_CACHE_RESOLVED_METHODS_OFFSET]  // Load the resolved methods.
    add xIP0, xIP0, x15, lsl #(POINTER_SIZE_SHIFT + 1)  // Load DexCache method slot address.

    // Relaxed atomic load x14:x15 from the dex cache slot.
.Limt_conflict_trampoline_retry_load:
    ldxp x14, x15, [xIP0]
    stxp w13, x14, x15, [xIP0]
    cbnz w13, .Limt_conflict_trampoline_retry_load

    cmp x15, xIP1       // Compare method index to see if we had a DexCache method hit.
    bne .Limt_conflict_trampoline_dex_cache_miss
.Limt_conflict_trampoline_have_interface_method:
    ldr xIP1, [x0, #ART_METHOD_JNI_OFFSET_64]  // Load ImtConflictTable
    ldr x0, [xIP1]  // Load first entry in ImtConflictTable.
.Limt_table_iterate:
    cmp x0, x14
    // Branch if found. Benchmarks have shown doing a branch here is better.
    beq .Limt_table_found
    // If the entry is null, the interface method is not in the ImtConflictTable.
    cbz x0, .Lconflict_trampoline
    // Iterate over the entries of the ImtConflictTable.
    ldr x0, [xIP1, #(2 * __SIZEOF_POINTER__)]!
    b .Limt_table_iterate
.Limt_table_found:
    // We successfully hit an entry in the table. Load the target method
    // and jump to it.
    ldr x0, [xIP1, #__SIZEOF_POINTER__]
    ldr xIP0, [x0, #ART_METHOD_QUICK_CODE_OFFSET_64]
    br xIP0
.Lconflict_trampoline:
    // Call the runtime stub to populate the ImtConflictTable and jump to the
    // resolved method.
    mov x0, x14  // Load interface method
    INVOKE_TRAMPOLINE_BODY artInvokeInterfaceTrampoline
.Limt_conflict_trampoline_dex_cache_miss:
    // We're not creating a proper runtime method frame here,
    // artLookupResolvedMethod() is not allowed to walk the stack.

    // Save GPR args and return address, allocate space for FPR args, align stack.
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, (8 * 8 + 8 * 8 + 8 + 8)
    SAVE_TWO_REGS x2, x3, 16
    SAVE_TWO_REGS x4, x5, 32
    SAVE_TWO_REGS x6, x7, 48
    SAVE_REG      xLR, (8 * 8 + 8 * 8 + 8)

    // Save FPR args.
    stp d0, d1, [sp, #64]
    stp d2, d3, [sp, #80]
    stp d4, d5, [sp, #96]
    stp d6, d7, [sp, #112]

    mov x0, xIP1                            // Pass method index.
    ldr x1, [sp, #(8 * 8 + 8 * 8 + 8 + 8)]  // Pass referrer.
    bl artLookupResolvedMethod              // (uint32_t method_index, ArtMethod* referrer)
    mov x14, x0   // Move the interface method to x14 where the loop above expects it.

    // Restore FPR args.
    ldp d0, d1, [sp, #64]
    ldp d2, d3, [sp, #80]
    ldp d4, d5, [sp, #96]
    ldp d6, d7, [sp, #112]

    // Restore GPR args and return address.
    RESTORE_REG      xLR, (8 * 8 + 8 * 8 + 8)
    RESTORE_TWO_REGS x2, x3, 16
    RESTORE_TWO_REGS x4, x5, 32
    RESTORE_TWO_REGS x6, x7, 48
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, (8 * 8 + 8 * 8 + 8 + 8)

    // If the method wasn't resolved, skip the lookup and go to artInvokeInterfaceTrampoline().
    cbz x14, .Lconflict_trampoline
    b .Limt_conflict_trampoline_have_interface_method
END art_quick_imt_conflict_trampoline

ENTRY art_quick_resolution_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME
    mov x2, xSELF
    mov x3, sp
    bl artQuickResolutionTrampoline  // (called, receiver, Thread*, SP)
    cbz x0, 1f
    mov xIP0, x0            // Remember returned code pointer in xIP0.
    ldr x0, [sp, #0]        // artQuickResolutionTrampoline puts called method in *SP.
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    br xIP0
1:
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    DELIVER_PENDING_EXCEPTION
END art_quick_resolution_trampoline

/*
 * Generic JNI frame layout:
 *
 * #-------------------#
 * |                   |
 * | caller method...  |
 * #-------------------#    <--- SP on entry
 * | Return X30/LR     |
 * | X29/FP            |    callee save
 * | X28               |    callee save
 * | X27               |    callee save
 * | X26               |    callee save
 * | X25               |    callee save
 * | X24               |    callee save
 * | X23               |    callee save
 * | X22               |    callee save
 * | X21               |    callee save
 * | X20               |    callee save
 * | X19               |    callee save
 * | X7                |    arg7
 * | X6                |    arg6
 * | X5                |    arg5
 * | X4                |    arg4
 * | X3                |    arg3
 * | X2                |    arg2
 * | X1                |    arg1
 * | D7                |    float arg 8
 * | D6                |    float arg 7
 * | D5                |    float arg 6
 * | D4                |    float arg 5
 * | D3                |    float arg 4
 * | D2                |    float arg 3
 * | D1                |    float arg 2
 * | D0                |    float arg 1
 * | Method*           | <- X0
 * #-------------------#
 * | local ref cookie  | // 4B
 * | handle scope size | // 4B
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
 * | Ptr to (1)        |    <--- SP
 * #-------------------#
 */
    /*
     * Called to do a generic JNI down-call
     */
ENTRY art_quick_generic_jni_trampoline
    SETUP_SAVE_REFS_AND_ARGS_FRAME_WITH_METHOD_IN_X0

    // Save SP , so we can have static CFI info.
    mov x28, sp
    .cfi_def_cfa_register x28

    // This looks the same, but is different: this will be updated to point to the bottom
    // of the frame when the handle scope is inserted.
    mov xFP, sp

    mov xIP0, #5120
    sub sp, sp, xIP0

    // prepare for artQuickGenericJniTrampoline call
    // (Thread*,  SP)
    //    x0      x1   <= C calling convention
    //   xSELF    xFP  <= where they are

    mov x0, xSELF   // Thread*
    mov x1, xFP
    bl artQuickGenericJniTrampoline  // (Thread*, sp)

    // The C call will have registered the complete save-frame on success.
    // The result of the call is:
    // x0: pointer to native code, 0 on error.
    // x1: pointer to the bottom of the used area of the alloca, can restore stack till there.

    // Check for error = 0.
    cbz x0, .Lexception_in_native

    // Release part of the alloca.
    mov sp, x1

    // Save the code pointer
    mov xIP0, x0

    // Load parameters from frame into registers.
    // TODO Check with artQuickGenericJniTrampoline.
    //      Also, check again APPCS64 - the stack arguments are interleaved.
    ldp x0, x1, [sp]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]

    ldp d0, d1, [sp, #64]
    ldp d2, d3, [sp, #80]
    ldp d4, d5, [sp, #96]
    ldp d6, d7, [sp, #112]

    add sp, sp, #128

    blr xIP0        // native call.

    // result sign extension is handled in C code
    // prepare for artQuickGenericJniEndTrampoline call
    // (Thread*, result, result_f)
    //    x0       x1       x2        <= C calling convention
    mov x1, x0      // Result (from saved).
    mov x0, xSELF   // Thread register.
    fmov x2, d0     // d0 will contain floating point result, but needs to go into x2

    bl artQuickGenericJniEndTrampoline

    // Pending exceptions possible.
    ldr x2, [xSELF, THREAD_EXCEPTION_OFFSET]
    cbnz x2, .Lexception_in_native

    // Tear down the alloca.
    mov sp, x28
    .cfi_def_cfa_register sp

    // Tear down the callee-save frame.
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER

    // store into fpr, for when it's a fpr return...
    fmov d0, x0
    ret

.Lexception_in_native:
    // Move to x1 then sp to please assembler.
    ldr x1, [xSELF, # THREAD_TOP_QUICK_FRAME_OFFSET]
    add sp, x1, #-1  // Remove the GenericJNI tag.
    .cfi_def_cfa_register sp
    # This will create a new save-all frame, required by the runtime.
    DELIVER_PENDING_EXCEPTION
END art_quick_generic_jni_trampoline

/*
 * Called to bridge from the quick to interpreter ABI. On entry the arguments match those
 * of a quick call:
 * x0 = method being called/to bridge to.
 * x1..x7, d0..d7 = arguments to that method.
 */
ENTRY art_quick_to_interpreter_bridge
    SETUP_SAVE_REFS_AND_ARGS_FRAME         // Set up frame and save arguments.

    //  x0 will contain mirror::ArtMethod* method.
    mov x1, xSELF                          // How to get Thread::Current() ???
    mov x2, sp

    // uint64_t artQuickToInterpreterBridge(mirror::ArtMethod* method, Thread* self,
    //                                      mirror::ArtMethod** sp)
    bl   artQuickToInterpreterBridge

    RESTORE_SAVE_REFS_AND_ARGS_FRAME       // TODO: no need to restore arguments in this case.
    REFRESH_MARKING_REGISTER

    fmov d0, x0

    RETURN_OR_DELIVER_PENDING_EXCEPTION
END art_quick_to_interpreter_bridge

/*
 * Called to attempt to execute an obsolete method.
 */
ONE_ARG_RUNTIME_EXCEPTION art_invoke_obsolete_method_stub, artInvokeObsoleteMethod


//
// Instrumentation-related stubs
//
    .extern artInstrumentationMethodEntryFromCode
ENTRY art_quick_instrumentation_entry
    SETUP_SAVE_REFS_AND_ARGS_FRAME

    mov   x20, x0             // Preserve method reference in a callee-save.

    mov   x2, xSELF
    mov   x3, sp  // Pass SP
    bl    artInstrumentationMethodEntryFromCode  // (Method*, Object*, Thread*, SP)

    mov   xIP0, x0            // x0 = result of call.
    mov   x0, x20             // Reload method reference.

    RESTORE_SAVE_REFS_AND_ARGS_FRAME  // Note: will restore xSELF
    REFRESH_MARKING_REGISTER
    cbz   xIP0, 1f            // Deliver the pending exception if method is null.
    adr   xLR, art_quick_instrumentation_exit
    br    xIP0                // Tail-call method with lr set to art_quick_instrumentation_exit.

1:
    DELIVER_PENDING_EXCEPTION
END art_quick_instrumentation_entry

    .extern artInstrumentationMethodExitFromCode
ENTRY art_quick_instrumentation_exit
    mov   xLR, #0             // Clobber LR for later checks.
    SETUP_SAVE_EVERYTHING_FRAME

    add   x3, sp, #8          // Pass floating-point result pointer, in kSaveEverything frame.
    add   x2, sp, #264        // Pass integer result pointer, in kSaveEverything frame.
    mov   x1, sp              // Pass SP.
    mov   x0, xSELF           // Pass Thread.
    bl   artInstrumentationMethodExitFromCode    // (Thread*, SP, gpr_res*, fpr_res*)

    cbz   x0, .Ldo_deliver_instrumentation_exception
                              // Handle error
    cbnz  x1, .Ldeoptimize
    // Normal return.
    str   x0, [sp, #FRAME_SIZE_SAVE_EVERYTHING - 8]
                              // Set return pc.
    RESTORE_SAVE_EVERYTHING_FRAME
    REFRESH_MARKING_REGISTER
    br    lr
.Ldo_deliver_instrumentation_exception:
    DELIVER_PENDING_EXCEPTION_FRAME_READY
.Ldeoptimize:
    str   x1, [sp, #FRAME_SIZE_SAVE_EVERYTHING - 8]
                              // Set return pc.
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
    SETUP_SAVE_EVERYTHING_FRAME
    mov    x0, xSELF          // Pass thread.
    bl     artDeoptimize      // (Thread*)
    brk 0
END art_quick_deoptimize

    /*
     * Compiled code has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
    .extern artDeoptimizeFromCompiledCode
ENTRY art_quick_deoptimize_from_compiled_code
    SETUP_SAVE_EVERYTHING_FRAME
    mov    x1, xSELF                      // Pass thread.
    bl     artDeoptimizeFromCompiledCode  // (DeoptimizationKind, Thread*)
    brk 0
END art_quick_deoptimize_from_compiled_code


    /*
     * String's indexOf.
     *
     * TODO: Not very optimized.
     * On entry:
     *    x0:   string object (known non-null)
     *    w1:   char to match (known <= 0xFFFF)
     *    w2:   Starting offset in string data
     */
ENTRY art_quick_indexof
#if (STRING_COMPRESSION_FEATURE)
    ldr   w4, [x0, #MIRROR_STRING_COUNT_OFFSET]
#else
    ldr   w3, [x0, #MIRROR_STRING_COUNT_OFFSET]
#endif
    add   x0, x0, #MIRROR_STRING_VALUE_OFFSET
#if (STRING_COMPRESSION_FEATURE)
    /* w4 holds count (with flag) and w3 holds actual length */
    lsr   w3, w4, #1
#endif
    /* Clamp start to [0..count] */
    cmp   w2, #0
    csel  w2, wzr, w2, lt
    cmp   w2, w3
    csel  w2, w3, w2, gt

    /* Save a copy to compute result */
    mov   x5, x0

#if (STRING_COMPRESSION_FEATURE)
    tbz   w4, #0, .Lstring_indexof_compressed
#endif
    /* Build pointer to start of data to compare and pre-bias */
    add   x0, x0, x2, lsl #1
    sub   x0, x0, #2
    /* Compute iteration count */
    sub   w2, w3, w2

    /*
     * At this point we have:
     *  x0: start of the data to test
     *  w1: char to compare
     *  w2: iteration count
     *  x5: original start of string data
     */

    subs  w2, w2, #4
    b.lt  .Lindexof_remainder

.Lindexof_loop4:
    ldrh  w6, [x0, #2]!
    ldrh  w7, [x0, #2]!
    ldrh  wIP0, [x0, #2]!
    ldrh  wIP1, [x0, #2]!
    cmp   w6, w1
    b.eq  .Lmatch_0
    cmp   w7, w1
    b.eq  .Lmatch_1
    cmp   wIP0, w1
    b.eq  .Lmatch_2
    cmp   wIP1, w1
    b.eq  .Lmatch_3
    subs  w2, w2, #4
    b.ge  .Lindexof_loop4

.Lindexof_remainder:
    adds  w2, w2, #4
    b.eq  .Lindexof_nomatch

.Lindexof_loop1:
    ldrh  w6, [x0, #2]!
    cmp   w6, w1
    b.eq  .Lmatch_3
    subs  w2, w2, #1
    b.ne  .Lindexof_loop1

.Lindexof_nomatch:
    mov   x0, #-1
    ret

.Lmatch_0:
    sub   x0, x0, #6
    sub   x0, x0, x5
    asr   x0, x0, #1
    ret
.Lmatch_1:
    sub   x0, x0, #4
    sub   x0, x0, x5
    asr   x0, x0, #1
    ret
.Lmatch_2:
    sub   x0, x0, #2
    sub   x0, x0, x5
    asr   x0, x0, #1
    ret
.Lmatch_3:
    sub   x0, x0, x5
    asr   x0, x0, #1
    ret
#if (STRING_COMPRESSION_FEATURE)
   /*
    * Comparing compressed string character-per-character with
    * input character
    */
.Lstring_indexof_compressed:
    add   x0, x0, x2
    sub   x0, x0, #1
    sub   w2, w3, w2
.Lstring_indexof_compressed_loop:
    subs  w2, w2, #1
    b.lt  .Lindexof_nomatch
    ldrb  w6, [x0, #1]!
    cmp   w6, w1
    b.eq  .Lstring_indexof_compressed_matched
    b     .Lstring_indexof_compressed_loop
.Lstring_indexof_compressed_matched:
    sub   x0, x0, x5
    ret
#endif
END art_quick_indexof

    /*
     * Create a function `name` calling the ReadBarrier::Mark routine,
     * getting its argument and returning its result through W register
     * `wreg` (corresponding to X register `xreg`), saving and restoring
     * all caller-save registers.
     *
     * If `wreg` is different from `w0`, the generated function follows a
     * non-standard runtime calling convention:
     * - register `wreg` is used to pass the (sole) argument of this
     *   function (instead of W0);
     * - register `wreg` is used to return the result of this function
     *   (instead of W0);
     * - W0 is treated like a normal (non-argument) caller-save register;
     * - everything else is the same as in the standard runtime calling
     *   convention (e.g. standard callee-save registers are preserved).
     */
.macro READ_BARRIER_MARK_REG name, wreg, xreg
ENTRY \name
    // Reference is null, no work to do at all.
    cbz \wreg, .Lret_rb_\name
    // Use wIP0 as temp and check the mark bit of the reference. wIP0 is not used by the compiler.
    ldr   wIP0, [\xreg, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    tbz   wIP0, #LOCK_WORD_MARK_BIT_SHIFT, .Lnot_marked_rb_\name
.Lret_rb_\name:
    ret
.Lnot_marked_rb_\name:
    // Check if the top two bits are one, if this is the case it is a forwarding address.
    tst   wIP0, wIP0, lsl #1
    bmi   .Lret_forwarding_address\name
.Lslow_rb_\name:
    /*
     * Allocate 44 stack slots * 8 = 352 bytes:
     * - 20 slots for core registers X0-15, X17-X19, LR
     * - 24 slots for floating-point registers D0-D7 and D16-D31
     */
    // We must not clobber IP1 since code emitted for HLoadClass and HLoadString
    // relies on IP1 being preserved.
    // Save all potentially live caller-save core registers.
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, 352
    SAVE_TWO_REGS  x2,  x3, 16
    SAVE_TWO_REGS  x4,  x5, 32
    SAVE_TWO_REGS  x6,  x7, 48
    SAVE_TWO_REGS  x8,  x9, 64
    SAVE_TWO_REGS x10, x11, 80
    SAVE_TWO_REGS x12, x13, 96
    SAVE_TWO_REGS x14, x15, 112
    SAVE_TWO_REGS x17, x18, 128  // Skip x16, i.e. IP0.
    SAVE_TWO_REGS x19, xLR, 144  // Save also return address.
    // Save all potentially live caller-save floating-point registers.
    stp   d0, d1,   [sp, #160]
    stp   d2, d3,   [sp, #176]
    stp   d4, d5,   [sp, #192]
    stp   d6, d7,   [sp, #208]
    stp   d16, d17, [sp, #224]
    stp   d18, d19, [sp, #240]
    stp   d20, d21, [sp, #256]
    stp   d22, d23, [sp, #272]
    stp   d24, d25, [sp, #288]
    stp   d26, d27, [sp, #304]
    stp   d28, d29, [sp, #320]
    stp   d30, d31, [sp, #336]

    .ifnc \wreg, w0
      mov   w0, \wreg                   // Pass arg1 - obj from `wreg`
    .endif
    bl    artReadBarrierMark            // artReadBarrierMark(obj)
    .ifnc \wreg, w0
      mov   \wreg, w0                   // Return result into `wreg`
    .endif

    // Restore core regs, except `xreg`, as `wreg` is used to return the
    // result of this function (simply remove it from the stack instead).
    POP_REGS_NE x0, x1,   0,   \xreg
    POP_REGS_NE x2, x3,   16,  \xreg
    POP_REGS_NE x4, x5,   32,  \xreg
    POP_REGS_NE x6, x7,   48,  \xreg
    POP_REGS_NE x8, x9,   64,  \xreg
    POP_REGS_NE x10, x11, 80,  \xreg
    POP_REGS_NE x12, x13, 96,  \xreg
    POP_REGS_NE x14, x15, 112, \xreg
    POP_REGS_NE x17, x18, 128, \xreg
    POP_REGS_NE x19, xLR, 144, \xreg  // Restore also return address.
    // Restore floating-point registers.
    ldp   d0, d1,   [sp, #160]
    ldp   d2, d3,   [sp, #176]
    ldp   d4, d5,   [sp, #192]
    ldp   d6, d7,   [sp, #208]
    ldp   d16, d17, [sp, #224]
    ldp   d18, d19, [sp, #240]
    ldp   d20, d21, [sp, #256]
    ldp   d22, d23, [sp, #272]
    ldp   d24, d25, [sp, #288]
    ldp   d26, d27, [sp, #304]
    ldp   d28, d29, [sp, #320]
    ldp   d30, d31, [sp, #336]
    // Remove frame and return.
    DECREASE_FRAME 352
    ret
.Lret_forwarding_address\name:
    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    lsl   \wreg, wIP0, #LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    ret
END \name
.endm

READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg00, w0,  x0
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg01, w1,  x1
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg02, w2,  x2
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg03, w3,  x3
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg04, w4,  x4
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg05, w5,  x5
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg06, w6,  x6
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg07, w7,  x7
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg08, w8,  x8
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg09, w9,  x9
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg10, w10, x10
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg11, w11, x11
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg12, w12, x12
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg13, w13, x13
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg14, w14, x14
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg15, w15, x15
// READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg16, w16, x16 ip0 is blocked
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg17, w17, x17
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg18, w18, x18
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg19, w19, x19
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg20, w20, x20
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg21, w21, x21
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg22, w22, x22
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg23, w23, x23
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg24, w24, x24
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg25, w25, x25
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg26, w26, x26
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg27, w27, x27
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg28, w28, x28
READ_BARRIER_MARK_REG art_quick_read_barrier_mark_reg29, w29, x29


.macro SELECT_X_OR_W_FOR_MACRO macro_to_use, x, w, xreg
    .if \xreg
      \macro_to_use \x
    .else
      \macro_to_use \w
    .endif
.endm

.macro FOR_REGISTERS macro_for_register, macro_for_reserved_register, xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x0, w0, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x1, w1, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x2, w2, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x3, w3, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x4, w4, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x5, w5, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x6, w6, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x7, w7, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x8, w8, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x9, w9, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x10, w10, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x11, w11, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x12, w12, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x13, w13, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x14, w14, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x15, w15, \xreg
    \macro_for_reserved_register  // IP0 is reserved
    \macro_for_reserved_register  // IP1 is reserved
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x18, w18, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x19, w19, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x20, w20, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x21, w21, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x22, w22, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x23, w23, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x24, w24, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x25, w25, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x26, w26, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x27, w27, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x28, w28, \xreg
    SELECT_X_OR_W_FOR_MACRO \macro_for_register, x29, w29, \xreg
    \macro_for_reserved_register  // lr is reserved
    \macro_for_reserved_register  // sp is reserved
.endm

.macro FOR_XREGISTERS macro_for_register, macro_for_reserved_register
    FOR_REGISTERS \macro_for_register, \macro_for_reserved_register, /* xreg */ 1
.endm

.macro FOR_WREGISTERS macro_for_register, macro_for_reserved_register
    FOR_REGISTERS \macro_for_register, \macro_for_reserved_register, /* xreg */ 0
.endm

.macro BRK0_BRK0
    brk 0
    brk 0
.endm

#if BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET != BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#error "Array and field introspection code sharing requires same LDR offset."
#endif
.macro INTROSPECTION_ARRAY_LOAD index_reg
    ldr   wIP0, [xIP0, \index_reg, lsl #2]
    b     art_quick_read_barrier_mark_introspection
.endm

.macro MOV_WIP0_TO_WREG_AND_BL_LR reg
    mov   \reg, wIP0
    br    lr  // Do not use RET as we do not enter the entrypoint with "BL".
.endm

.macro READ_BARRIER_MARK_INTROSPECTION_SLOW_PATH ldr_offset
    /*
     * Allocate 44 stack slots * 8 = 352 bytes:
     * - 19 slots for core registers X0-15, X18-X19, LR
     * - 1 slot padding
     * - 24 slots for floating-point registers D0-D7 and D16-D31
     */
    // Save all potentially live caller-save core registers.
    SAVE_TWO_REGS_INCREASE_FRAME x0, x1, 352
    SAVE_TWO_REGS  x2,  x3, 16
    SAVE_TWO_REGS  x4,  x5, 32
    SAVE_TWO_REGS  x6,  x7, 48
    SAVE_TWO_REGS  x8,  x9, 64
    SAVE_TWO_REGS x10, x11, 80
    SAVE_TWO_REGS x12, x13, 96
    SAVE_TWO_REGS x14, x15, 112
    SAVE_TWO_REGS x18, x19, 128       // Skip x16, x17, i.e. IP0, IP1.
    SAVE_REG      xLR,      144       // Save return address, skip padding at 152.
    // Save all potentially live caller-save floating-point registers.
    stp   d0, d1,   [sp, #160]
    stp   d2, d3,   [sp, #176]
    stp   d4, d5,   [sp, #192]
    stp   d6, d7,   [sp, #208]
    stp   d16, d17, [sp, #224]
    stp   d18, d19, [sp, #240]
    stp   d20, d21, [sp, #256]
    stp   d22, d23, [sp, #272]
    stp   d24, d25, [sp, #288]
    stp   d26, d27, [sp, #304]
    stp   d28, d29, [sp, #320]
    stp   d30, d31, [sp, #336]

    mov   x0, xIP0
    bl    artReadBarrierMark          // artReadBarrierMark(obj)
    mov   xIP0, x0

    // Restore core regs, except x0 and x1 as the return register switch case
    // address calculation is smoother with an extra register.
    RESTORE_TWO_REGS  x2,  x3, 16
    RESTORE_TWO_REGS  x4,  x5, 32
    RESTORE_TWO_REGS  x6,  x7, 48
    RESTORE_TWO_REGS  x8,  x9, 64
    RESTORE_TWO_REGS x10, x11, 80
    RESTORE_TWO_REGS x12, x13, 96
    RESTORE_TWO_REGS x14, x15, 112
    RESTORE_TWO_REGS x18, x19, 128    // Skip x16, x17, i.e. IP0, IP1.
    RESTORE_REG      xLR,      144    // Restore return address.
    // Restore caller-save floating-point registers.
    ldp   d0, d1,   [sp, #160]
    ldp   d2, d3,   [sp, #176]
    ldp   d4, d5,   [sp, #192]
    ldp   d6, d7,   [sp, #208]
    ldp   d16, d17, [sp, #224]
    ldp   d18, d19, [sp, #240]
    ldp   d20, d21, [sp, #256]
    ldp   d22, d23, [sp, #272]
    ldp   d24, d25, [sp, #288]
    ldp   d26, d27, [sp, #304]
    ldp   d28, d29, [sp, #320]
    ldp   d30, d31, [sp, #336]

    ldr   x0, [lr, #\ldr_offset]      // Load the instruction.
    adr   xIP1, .Lmark_introspection_return_switch
    bfi   xIP1, x0, #3, #5            // Calculate switch case address.
    RESTORE_TWO_REGS_DECREASE_FRAME x0, x1, 352
    br    xIP1
.endm

    /*
     * Use introspection to load a reference from the same address as the LDR
     * instruction in generated code would load (unless loaded by the thunk,
     * see below), call ReadBarrier::Mark() with that reference if needed
     * and return it in the same register as the LDR instruction would load.
     *
     * The entrypoint is called through a thunk that differs across load kinds.
     * For field and array loads the LDR instruction in generated code follows
     * the branch to the thunk, i.e. the LDR is at [LR, #-4], and the thunk
     * knows the holder and performs the gray bit check, returning to the LDR
     * instruction if the object is not gray, so this entrypoint no longer
     * needs to know anything about the holder. For GC root loads, the LDR
     * instruction in generated code precedes the branch to the thunk (i.e.
     * the LDR is at [LR, #-8]) and the thunk does not do the gray bit check.
     *
     * For field accesses and array loads with a constant index the thunk loads
     * the reference into IP0 using introspection and calls the main entrypoint,
     * art_quick_read_barrier_mark_introspection. With heap poisoning enabled,
     * the passed reference is poisoned.
     *
     * For array accesses with non-constant index, the thunk inserts the bits
     * 16-21 of the LDR instruction to the entrypoint address, effectively
     * calculating a switch case label based on the index register (bits 16-20)
     * and adding an extra offset (bit 21 is set) to differentiate from the
     * main entrypoint, then moves the base register to IP0 and jumps to the
     * switch case. Therefore we need to align the main entrypoint to 512 bytes,
     * accounting for a 256-byte offset followed by 32 array entrypoints
     * starting at art_quick_read_barrier_mark_introspection_arrays, each
     * containing an LDR (register) and a branch to the main entrypoint.
     *
     * For GC root accesses we cannot use the main entrypoint because of the
     * different offset where the LDR instruction in generated code is located.
     * (And even with heap poisoning enabled, GC roots are not poisoned.)
     * To re-use the same entrypoint pointer in generated code, we make sure
     * that the gc root entrypoint (a copy of the entrypoint with a different
     * offset for introspection loads) is located at a known offset (768 bytes,
     * or BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET) from the main
     * entrypoint and the GC root thunk adjusts the entrypoint pointer, moves
     * the root register to IP0 and jumps to the customized entrypoint,
     * art_quick_read_barrier_mark_introspection_gc_roots. The thunk also
     * performs all the fast-path checks, so we need just the slow path.
     *
     * The code structure is
     *   art_quick_read_barrier_mark_introspection:
     *     Up to 256 bytes for the main entrypoint code.
     *     Padding to 256 bytes if needed.
     *   art_quick_read_barrier_mark_introspection_arrays:
     *     Exactly 256 bytes for array load switch cases (32x2 instructions).
     *   .Lmark_introspection_return_switch:
     *     Exactly 256 bytes for return switch cases (32x2 instructions).
     *   art_quick_read_barrier_mark_introspection_gc_roots:
     *     GC root entrypoint code.
     */
    .balign 512
ENTRY art_quick_read_barrier_mark_introspection
    // At this point, IP0 contains the reference, IP1 can be freely used.
    // For heap poisoning, the reference is poisoned, so unpoison it first.
    UNPOISON_HEAP_REF wIP0
    // If reference is null, just return it in the right register.
    cbz   wIP0, .Lmark_introspection_return
    // Use wIP1 as temp and check the mark bit of the reference.
    ldr   wIP1, [xIP0, #MIRROR_OBJECT_LOCK_WORD_OFFSET]
    tbz   wIP1, #LOCK_WORD_MARK_BIT_SHIFT, .Lmark_introspection_unmarked
.Lmark_introspection_return:
    // Without an extra register for the return switch case address calculation,
    // we exploit the high word of the xIP0 to temporarily store the ref_reg*8,
    // so the return switch below must move wIP0 instead of xIP0 to the register.
    ldr   wIP1, [lr, #BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET]  // Load the instruction.
    bfi   xIP0, xIP1, #(32 + 3), #5   // Extract ref_reg*8 to high word in xIP0.
    adr   xIP1, .Lmark_introspection_return_switch
    bfxil xIP1, xIP0, #32, #8         // Calculate return switch case address.
    br    xIP1
.Lmark_introspection_unmarked:
    // Check if the top two bits are one, if this is the case it is a forwarding address.
    tst   wIP1, wIP1, lsl #1
    bmi   .Lmark_introspection_forwarding_address
    READ_BARRIER_MARK_INTROSPECTION_SLOW_PATH BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET

.Lmark_introspection_forwarding_address:
    // Shift left by the forwarding address shift. This clears out the state bits since they are
    // in the top 2 bits of the lock word.
    lsl   wIP0, wIP1, #LOCK_WORD_STATE_FORWARDING_ADDRESS_SHIFT
    b .Lmark_introspection_return

    // We're very close to the alloted 256B for the entrypoint code before the
    // array switch cases. Should we go a little bit over the limit, we can
    // move some code after the array switch cases and return switch cases.
    .balign 256
    .hidden art_quick_read_barrier_mark_introspection_arrays
    .global art_quick_read_barrier_mark_introspection_arrays
art_quick_read_barrier_mark_introspection_arrays:
    FOR_XREGISTERS INTROSPECTION_ARRAY_LOAD, BRK0_BRK0
.Lmark_introspection_return_switch:
    FOR_WREGISTERS MOV_WIP0_TO_WREG_AND_BL_LR, BRK0_BRK0
    .hidden art_quick_read_barrier_mark_introspection_gc_roots
    .global art_quick_read_barrier_mark_introspection_gc_roots
art_quick_read_barrier_mark_introspection_gc_roots:
    READ_BARRIER_MARK_INTROSPECTION_SLOW_PATH BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_OFFSET
END art_quick_read_barrier_mark_introspection

.extern artInvokePolymorphic
ENTRY art_quick_invoke_polymorphic
    SETUP_SAVE_REFS_AND_ARGS_FRAME                // Save callee saves in case allocation triggers GC.
    mov     x2, xSELF
    mov     x3, sp
    INCREASE_FRAME 16                             // Reserve space for JValue result.
    str     xzr, [sp, #0]                         // Initialize result to zero.
    mov     x0, sp                                // Set r0 to point to result.
    bl      artInvokePolymorphic                  // artInvokePolymorphic(result, receiver, thread, save_area)
    uxtb    w0, w0                                // Result is the return type descriptor as a char.
    sub     w0, w0, 'A'                           // Convert to zero based index.
    cmp     w0, 'Z' - 'A'
    bhi     .Lcleanup_and_return                  // Clean-up if out-of-bounds.
    adrp    x1, .Lhandler_table                   // Compute address of handler table.
    add     x1, x1, :lo12:.Lhandler_table
    ldrb    w0, [x1, w0, uxtw]                    // Lookup handler offset in handler table.
    adr     x1, .Lstart_of_handlers
    add     x0, x1, w0, sxtb #2                   // Convert relative offset to absolute address.
    br      x0                                    // Branch to handler.

.Lstart_of_handlers:
.Lstore_boolean_result:
    ldrb    w0, [sp]
    b       .Lcleanup_and_return
.Lstore_char_result:
    ldrh    w0, [sp]
    b       .Lcleanup_and_return
.Lstore_float_result:
    ldr     s0, [sp]
    str     s0, [sp, #32]
    b       .Lcleanup_and_return
.Lstore_double_result:
    ldr     d0, [sp]
    str     d0, [sp, #32]
    b       .Lcleanup_and_return
.Lstore_long_result:
    ldr     x0, [sp]
    // Fall-through
.Lcleanup_and_return:
    DECREASE_FRAME 16
    RESTORE_SAVE_REFS_AND_ARGS_FRAME
    REFRESH_MARKING_REGISTER
    RETURN_OR_DELIVER_PENDING_EXCEPTION_X1

    .section    .rodata                           // Place handler table in read-only section away from text.
    .align  2
.macro HANDLER_TABLE_OFFSET handler_label
    .byte (\handler_label - .Lstart_of_handlers) / 4
.endm
.Lhandler_table:
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // A
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // B (byte)
    HANDLER_TABLE_OFFSET(.Lstore_char_result)     // C (char)
    HANDLER_TABLE_OFFSET(.Lstore_double_result)   // D (double)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // E
    HANDLER_TABLE_OFFSET(.Lstore_float_result)    // F (float)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // G
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // H
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // I (int)
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // J (long)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // K
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // L (object - references are compressed and only 32-bits)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // M
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // N
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // O
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // P
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // Q
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // R
    HANDLER_TABLE_OFFSET(.Lstore_long_result)     // S (short)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // T
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // U
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // V (void)
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // W
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // X
    HANDLER_TABLE_OFFSET(.Lcleanup_and_return)    // Y
    HANDLER_TABLE_OFFSET(.Lstore_boolean_result)  // Z (boolean)
    .text

END  art_quick_invoke_polymorphic

// Wrap ExecuteSwitchImpl in assembly method which specifies DEX PC for unwinding.
//  Argument 0: x0: The context pointer for ExecuteSwitchImpl.
//  Argument 1: x1: Pointer to the templated ExecuteSwitchImpl to call.
//  Argument 2: x2: The value of DEX PC (memory address of the methods bytecode).
ENTRY ExecuteSwitchImplAsm
    SAVE_TWO_REGS_INCREASE_FRAME x19, xLR, 16
    mov x19, x2                                   // x19 = DEX PC
    CFI_DEFINE_DEX_PC_WITH_OFFSET(0 /* x0 */, 19 /* x19 */, 0)
    blr x1                                        // Call the wrapped method.
    RESTORE_TWO_REGS_DECREASE_FRAME x19, xLR, 16
    ret
END ExecuteSwitchImplAsm
