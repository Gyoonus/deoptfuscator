/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "jni_compiler.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <memory>
#include <vector>

#include "art_method.h"
#include "base/arena_allocator.h"
#include "base/enums.h"
#include "base/logging.h"  // For VLOG.
#include "base/macros.h"
#include "base/utils.h"
#include "calling_convention.h"
#include "class_linker.h"
#include "debug/dwarf/debug_frame_opcode_writer.h"
#include "dex/dex_file-inl.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "jni_env_ext.h"
#include "memory_region.h"
#include "thread.h"
#include "utils/arm/managed_register_arm.h"
#include "utils/arm64/managed_register_arm64.h"
#include "utils/assembler.h"
#include "utils/jni_macro_assembler.h"
#include "utils/managed_register.h"
#include "utils/mips/managed_register_mips.h"
#include "utils/mips64/managed_register_mips64.h"
#include "utils/x86/managed_register_x86.h"

#define __ jni_asm->

namespace art {

template <PointerSize kPointerSize>
static void CopyParameter(JNIMacroAssembler<kPointerSize>* jni_asm,
                          ManagedRuntimeCallingConvention* mr_conv,
                          JniCallingConvention* jni_conv,
                          size_t frame_size, size_t out_arg_size);
template <PointerSize kPointerSize>
static void SetNativeParameter(JNIMacroAssembler<kPointerSize>* jni_asm,
                               JniCallingConvention* jni_conv,
                               ManagedRegister in_reg);

template <PointerSize kPointerSize>
static std::unique_ptr<JNIMacroAssembler<kPointerSize>> GetMacroAssembler(
    ArenaAllocator* allocator, InstructionSet isa, const InstructionSetFeatures* features) {
  return JNIMacroAssembler<kPointerSize>::Create(allocator, isa, features);
}

enum class JniEntrypoint {
  kStart,
  kEnd
};

template <PointerSize kPointerSize>
static ThreadOffset<kPointerSize> GetJniEntrypointThreadOffset(JniEntrypoint which,
                                                               bool reference_return,
                                                               bool is_synchronized,
                                                               bool is_fast_native) {
  if (which == JniEntrypoint::kStart) {  // JniMethodStart
    ThreadOffset<kPointerSize> jni_start =
        is_synchronized
            ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodStartSynchronized)
            : (is_fast_native
                   ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodFastStart)
                   : QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodStart));

    return jni_start;
  } else {  // JniMethodEnd
    ThreadOffset<kPointerSize> jni_end(-1);
    if (reference_return) {
      // Pass result.
      jni_end = is_synchronized
                    ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodEndWithReferenceSynchronized)
                    : (is_fast_native
                           ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodFastEndWithReference)
                           : QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodEndWithReference));
    } else {
      jni_end = is_synchronized
                    ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodEndSynchronized)
                    : (is_fast_native
                           ? QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodFastEnd)
                           : QUICK_ENTRYPOINT_OFFSET(kPointerSize, pJniMethodEnd));
    }

    return jni_end;
  }
}


// Generate the JNI bridge for the given method, general contract:
// - Arguments are in the managed runtime format, either on stack or in
//   registers, a reference to the method object is supplied as part of this
//   convention.
//
template <PointerSize kPointerSize>
static JniCompiledMethod ArtJniCompileMethodInternal(CompilerDriver* driver,
                                                     uint32_t access_flags,
                                                     uint32_t method_idx,
                                                     const DexFile& dex_file) {
  const bool is_native = (access_flags & kAccNative) != 0;
  CHECK(is_native);
  const bool is_static = (access_flags & kAccStatic) != 0;
  const bool is_synchronized = (access_flags & kAccSynchronized) != 0;
  const char* shorty = dex_file.GetMethodShorty(dex_file.GetMethodId(method_idx));
  InstructionSet instruction_set = driver->GetInstructionSet();
  const InstructionSetFeatures* instruction_set_features = driver->GetInstructionSetFeatures();

  // i.e. if the method was annotated with @FastNative
  const bool is_fast_native = (access_flags & kAccFastNative) != 0u;

  // i.e. if the method was annotated with @CriticalNative
  bool is_critical_native = (access_flags & kAccCriticalNative) != 0u;

  VLOG(jni) << "JniCompile: Method :: "
              << dex_file.PrettyMethod(method_idx, /* with signature */ true)
              << " :: access_flags = " << std::hex << access_flags << std::dec;

  if (UNLIKELY(is_fast_native)) {
    VLOG(jni) << "JniCompile: Fast native method detected :: "
              << dex_file.PrettyMethod(method_idx, /* with signature */ true);
  }

  if (UNLIKELY(is_critical_native)) {
    VLOG(jni) << "JniCompile: Critical native method detected :: "
              << dex_file.PrettyMethod(method_idx, /* with signature */ true);
  }

  if (kIsDebugBuild) {
    // Don't allow both @FastNative and @CriticalNative. They are mutually exclusive.
    if (UNLIKELY(is_fast_native && is_critical_native)) {
      LOG(FATAL) << "JniCompile: Method cannot be both @CriticalNative and @FastNative"
                 << dex_file.PrettyMethod(method_idx, /* with_signature */ true);
    }

    // @CriticalNative - extra checks:
    // -- Don't allow virtual criticals
    // -- Don't allow synchronized criticals
    // -- Don't allow any objects as parameter or return value
    if (UNLIKELY(is_critical_native)) {
      CHECK(is_static)
          << "@CriticalNative functions cannot be virtual since that would"
          << "require passing a reference parameter (this), which is illegal "
          << dex_file.PrettyMethod(method_idx, /* with_signature */ true);
      CHECK(!is_synchronized)
          << "@CriticalNative functions cannot be synchronized since that would"
          << "require passing a (class and/or this) reference parameter, which is illegal "
          << dex_file.PrettyMethod(method_idx, /* with_signature */ true);
      for (size_t i = 0; i < strlen(shorty); ++i) {
        CHECK_NE(Primitive::kPrimNot, Primitive::GetType(shorty[i]))
            << "@CriticalNative methods' shorty types must not have illegal references "
            << dex_file.PrettyMethod(method_idx, /* with_signature */ true);
      }
    }
  }

  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  // Calling conventions used to iterate over parameters to method
  std::unique_ptr<JniCallingConvention> main_jni_conv =
      JniCallingConvention::Create(&allocator,
                                   is_static,
                                   is_synchronized,
                                   is_critical_native,
                                   shorty,
                                   instruction_set);
  bool reference_return = main_jni_conv->IsReturnAReference();

  std::unique_ptr<ManagedRuntimeCallingConvention> mr_conv(
      ManagedRuntimeCallingConvention::Create(
          &allocator, is_static, is_synchronized, shorty, instruction_set));

  // Calling conventions to call into JNI method "end" possibly passing a returned reference, the
  //     method and the current thread.
  const char* jni_end_shorty;
  if (reference_return && is_synchronized) {
    jni_end_shorty = "ILL";
  } else if (reference_return) {
    jni_end_shorty = "IL";
  } else if (is_synchronized) {
    jni_end_shorty = "VL";
  } else {
    jni_end_shorty = "V";
  }

  std::unique_ptr<JniCallingConvention> end_jni_conv(
      JniCallingConvention::Create(&allocator,
                                   is_static,
                                   is_synchronized,
                                   is_critical_native,
                                   jni_end_shorty,
                                   instruction_set));

  // Assembler that holds generated instructions
  std::unique_ptr<JNIMacroAssembler<kPointerSize>> jni_asm =
      GetMacroAssembler<kPointerSize>(&allocator, instruction_set, instruction_set_features);
  const CompilerOptions& compiler_options = driver->GetCompilerOptions();
  jni_asm->cfi().SetEnabled(compiler_options.GenerateAnyDebugInfo());
  jni_asm->SetEmitRunTimeChecksInDebugMode(compiler_options.EmitRunTimeChecksInDebugMode());

  // Offsets into data structures
  // TODO: if cross compiling these offsets are for the host not the target
  const Offset functions(OFFSETOF_MEMBER(JNIEnvExt, functions));
  const Offset monitor_enter(OFFSETOF_MEMBER(JNINativeInterface, MonitorEnter));
  const Offset monitor_exit(OFFSETOF_MEMBER(JNINativeInterface, MonitorExit));

  // 1. Build the frame saving all callee saves, Method*, and PC return address.
  const size_t frame_size(main_jni_conv->FrameSize());  // Excludes outgoing args.
  ArrayRef<const ManagedRegister> callee_save_regs = main_jni_conv->CalleeSaveRegisters();
  __ BuildFrame(frame_size, mr_conv->MethodRegister(), callee_save_regs, mr_conv->EntrySpills());
  DCHECK_EQ(jni_asm->cfi().GetCurrentCFAOffset(), static_cast<int>(frame_size));

  if (LIKELY(!is_critical_native)) {
    // NOTE: @CriticalNative methods don't have a HandleScope
    //       because they can't have any reference parameters or return values.

    // 2. Set up the HandleScope
    mr_conv->ResetIterator(FrameOffset(frame_size));
    main_jni_conv->ResetIterator(FrameOffset(0));
    __ StoreImmediateToFrame(main_jni_conv->HandleScopeNumRefsOffset(),
                             main_jni_conv->ReferenceCount(),
                             mr_conv->InterproceduralScratchRegister());

    __ CopyRawPtrFromThread(main_jni_conv->HandleScopeLinkOffset(),
                            Thread::TopHandleScopeOffset<kPointerSize>(),
                            mr_conv->InterproceduralScratchRegister());
    __ StoreStackOffsetToThread(Thread::TopHandleScopeOffset<kPointerSize>(),
                                main_jni_conv->HandleScopeOffset(),
                                mr_conv->InterproceduralScratchRegister());

    // 3. Place incoming reference arguments into handle scope
    main_jni_conv->Next();  // Skip JNIEnv*
    // 3.5. Create Class argument for static methods out of passed method
    if (is_static) {
      FrameOffset handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
      // Check handle scope offset is within frame
      CHECK_LT(handle_scope_offset.Uint32Value(), frame_size);
      // Note this LoadRef() doesn't need heap unpoisoning since it's from the ArtMethod.
      // Note this LoadRef() does not include read barrier. It will be handled below.
      //
      // scratchRegister = *method[DeclaringClassOffset()];
      __ LoadRef(main_jni_conv->InterproceduralScratchRegister(),
                 mr_conv->MethodRegister(), ArtMethod::DeclaringClassOffset(), false);
      __ VerifyObject(main_jni_conv->InterproceduralScratchRegister(), false);
      // *handleScopeOffset = scratchRegister
      __ StoreRef(handle_scope_offset, main_jni_conv->InterproceduralScratchRegister());
      main_jni_conv->Next();  // in handle scope so move to next argument
    }
    // Place every reference into the handle scope (ignore other parameters).
    while (mr_conv->HasNext()) {
      CHECK(main_jni_conv->HasNext());
      bool ref_param = main_jni_conv->IsCurrentParamAReference();
      CHECK(!ref_param || mr_conv->IsCurrentParamAReference());
      // References need placing in handle scope and the entry value passing
      if (ref_param) {
        // Compute handle scope entry, note null is placed in the handle scope but its boxed value
        // must be null.
        FrameOffset handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
        // Check handle scope offset is within frame and doesn't run into the saved segment state.
        CHECK_LT(handle_scope_offset.Uint32Value(), frame_size);
        CHECK_NE(handle_scope_offset.Uint32Value(),
                 main_jni_conv->SavedLocalReferenceCookieOffset().Uint32Value());
        bool input_in_reg = mr_conv->IsCurrentParamInRegister();
        bool input_on_stack = mr_conv->IsCurrentParamOnStack();
        CHECK(input_in_reg || input_on_stack);

        if (input_in_reg) {
          ManagedRegister in_reg  =  mr_conv->CurrentParamRegister();
          __ VerifyObject(in_reg, mr_conv->IsCurrentArgPossiblyNull());
          __ StoreRef(handle_scope_offset, in_reg);
        } else if (input_on_stack) {
          FrameOffset in_off  = mr_conv->CurrentParamStackOffset();
          __ VerifyObject(in_off, mr_conv->IsCurrentArgPossiblyNull());
          __ CopyRef(handle_scope_offset, in_off,
                     mr_conv->InterproceduralScratchRegister());
        }
      }
      mr_conv->Next();
      main_jni_conv->Next();
    }

    // 4. Write out the end of the quick frames.
    __ StoreStackPointerToThread(Thread::TopOfManagedStackOffset<kPointerSize>());

    // NOTE: @CriticalNative does not need to store the stack pointer to the thread
    //       because garbage collections are disabled within the execution of a
    //       @CriticalNative method.
    //       (TODO: We could probably disable it for @FastNative too).
  }  // if (!is_critical_native)

  // 5. Move frame down to allow space for out going args.
  const size_t main_out_arg_size = main_jni_conv->OutArgSize();
  size_t current_out_arg_size = main_out_arg_size;
  __ IncreaseFrameSize(main_out_arg_size);

  // Call the read barrier for the declaring class loaded from the method for a static call.
  // Skip this for @CriticalNative because we didn't build a HandleScope to begin with.
  // Note that we always have outgoing param space available for at least two params.
  if (kUseReadBarrier && is_static && !is_critical_native) {
    const bool kReadBarrierFastPath =
        (instruction_set != InstructionSet::kMips) && (instruction_set != InstructionSet::kMips64);
    std::unique_ptr<JNIMacroLabel> skip_cold_path_label;
    if (kReadBarrierFastPath) {
      skip_cold_path_label = __ CreateLabel();
      // Fast path for supported targets.
      //
      // Check if gc_is_marking is set -- if it's not, we don't need
      // a read barrier so skip it.
      __ LoadFromThread(main_jni_conv->InterproceduralScratchRegister(),
                        Thread::IsGcMarkingOffset<kPointerSize>(),
                        Thread::IsGcMarkingSize());
      // Jump over the slow path if gc is marking is false.
      __ Jump(skip_cold_path_label.get(),
              JNIMacroUnaryCondition::kZero,
              main_jni_conv->InterproceduralScratchRegister());
    }

    // Construct slow path for read barrier:
    //
    // Call into the runtime's ReadBarrierJni and have it fix up
    // the object address if it was moved.

    ThreadOffset<kPointerSize> read_barrier = QUICK_ENTRYPOINT_OFFSET(kPointerSize,
                                                                      pReadBarrierJni);
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
    main_jni_conv->Next();  // Skip JNIEnv.
    FrameOffset class_handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
    // Pass the handle for the class as the first argument.
    if (main_jni_conv->IsCurrentParamOnStack()) {
      FrameOffset out_off = main_jni_conv->CurrentParamStackOffset();
      __ CreateHandleScopeEntry(out_off, class_handle_scope_offset,
                         mr_conv->InterproceduralScratchRegister(),
                         false);
    } else {
      ManagedRegister out_reg = main_jni_conv->CurrentParamRegister();
      __ CreateHandleScopeEntry(out_reg, class_handle_scope_offset,
                         ManagedRegister::NoRegister(), false);
    }
    main_jni_conv->Next();
    // Pass the current thread as the second argument and call.
    if (main_jni_conv->IsCurrentParamInRegister()) {
      __ GetCurrentThread(main_jni_conv->CurrentParamRegister());
      __ Call(main_jni_conv->CurrentParamRegister(),
              Offset(read_barrier),
              main_jni_conv->InterproceduralScratchRegister());
    } else {
      __ GetCurrentThread(main_jni_conv->CurrentParamStackOffset(),
                          main_jni_conv->InterproceduralScratchRegister());
      __ CallFromThread(read_barrier, main_jni_conv->InterproceduralScratchRegister());
    }
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));  // Reset.

    if (kReadBarrierFastPath) {
      __ Bind(skip_cold_path_label.get());
    }
  }

  // 6. Call into appropriate JniMethodStart passing Thread* so that transition out of Runnable
  //    can occur. The result is the saved JNI local state that is restored by the exit call. We
  //    abuse the JNI calling convention here, that is guaranteed to support passing 2 pointer
  //    arguments.
  FrameOffset locked_object_handle_scope_offset(0xBEEFDEAD);
  if (LIKELY(!is_critical_native)) {
    // Skip this for @CriticalNative methods. They do not call JniMethodStart.
    ThreadOffset<kPointerSize> jni_start(
        GetJniEntrypointThreadOffset<kPointerSize>(JniEntrypoint::kStart,
                                                   reference_return,
                                                   is_synchronized,
                                                   is_fast_native).SizeValue());
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
    locked_object_handle_scope_offset = FrameOffset(0);
    if (is_synchronized) {
      // Pass object for locking.
      main_jni_conv->Next();  // Skip JNIEnv.
      locked_object_handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
      main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
      if (main_jni_conv->IsCurrentParamOnStack()) {
        FrameOffset out_off = main_jni_conv->CurrentParamStackOffset();
        __ CreateHandleScopeEntry(out_off, locked_object_handle_scope_offset,
                                  mr_conv->InterproceduralScratchRegister(), false);
      } else {
        ManagedRegister out_reg = main_jni_conv->CurrentParamRegister();
        __ CreateHandleScopeEntry(out_reg, locked_object_handle_scope_offset,
                                  ManagedRegister::NoRegister(), false);
      }
      main_jni_conv->Next();
    }
    if (main_jni_conv->IsCurrentParamInRegister()) {
      __ GetCurrentThread(main_jni_conv->CurrentParamRegister());
      __ Call(main_jni_conv->CurrentParamRegister(),
              Offset(jni_start),
              main_jni_conv->InterproceduralScratchRegister());
    } else {
      __ GetCurrentThread(main_jni_conv->CurrentParamStackOffset(),
                          main_jni_conv->InterproceduralScratchRegister());
      __ CallFromThread(jni_start, main_jni_conv->InterproceduralScratchRegister());
    }
    if (is_synchronized) {  // Check for exceptions from monitor enter.
      __ ExceptionPoll(main_jni_conv->InterproceduralScratchRegister(), main_out_arg_size);
    }
  }

  // Store into stack_frame[saved_cookie_offset] the return value of JniMethodStart.
  FrameOffset saved_cookie_offset(
      FrameOffset(0xDEADBEEFu));  // @CriticalNative - use obviously bad value for debugging
  if (LIKELY(!is_critical_native)) {
    saved_cookie_offset = main_jni_conv->SavedLocalReferenceCookieOffset();
    __ Store(saved_cookie_offset, main_jni_conv->IntReturnRegister(), 4 /* sizeof cookie */);
  }

  // 7. Iterate over arguments placing values from managed calling convention in
  //    to the convention required for a native call (shuffling). For references
  //    place an index/pointer to the reference after checking whether it is
  //    null (which must be encoded as null).
  //    Note: we do this prior to materializing the JNIEnv* and static's jclass to
  //    give as many free registers for the shuffle as possible.
  mr_conv->ResetIterator(FrameOffset(frame_size + main_out_arg_size));
  uint32_t args_count = 0;
  while (mr_conv->HasNext()) {
    args_count++;
    mr_conv->Next();
  }

  // Do a backward pass over arguments, so that the generated code will be "mov
  // R2, R3; mov R1, R2" instead of "mov R1, R2; mov R2, R3."
  // TODO: A reverse iterator to improve readability.
  for (uint32_t i = 0; i < args_count; ++i) {
    mr_conv->ResetIterator(FrameOffset(frame_size + main_out_arg_size));
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));

    // Skip the extra JNI parameters for now.
    if (LIKELY(!is_critical_native)) {
      main_jni_conv->Next();    // Skip JNIEnv*.
      if (is_static) {
        main_jni_conv->Next();  // Skip Class for now.
      }
    }
    // Skip to the argument we're interested in.
    for (uint32_t j = 0; j < args_count - i - 1; ++j) {
      mr_conv->Next();
      main_jni_conv->Next();
    }
    CopyParameter(jni_asm.get(), mr_conv.get(), main_jni_conv.get(), frame_size, main_out_arg_size);
  }
  if (is_static && !is_critical_native) {
    // Create argument for Class
    mr_conv->ResetIterator(FrameOffset(frame_size + main_out_arg_size));
    main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
    main_jni_conv->Next();  // Skip JNIEnv*
    FrameOffset handle_scope_offset = main_jni_conv->CurrentParamHandleScopeEntryOffset();
    if (main_jni_conv->IsCurrentParamOnStack()) {
      FrameOffset out_off = main_jni_conv->CurrentParamStackOffset();
      __ CreateHandleScopeEntry(out_off, handle_scope_offset,
                         mr_conv->InterproceduralScratchRegister(),
                         false);
    } else {
      ManagedRegister out_reg = main_jni_conv->CurrentParamRegister();
      __ CreateHandleScopeEntry(out_reg, handle_scope_offset,
                         ManagedRegister::NoRegister(), false);
    }
  }

  // Set the iterator back to the incoming Method*.
  main_jni_conv->ResetIterator(FrameOffset(main_out_arg_size));
  if (LIKELY(!is_critical_native)) {
    // 8. Create 1st argument, the JNI environment ptr.
    // Register that will hold local indirect reference table
    if (main_jni_conv->IsCurrentParamInRegister()) {
      ManagedRegister jni_env = main_jni_conv->CurrentParamRegister();
      DCHECK(!jni_env.Equals(main_jni_conv->InterproceduralScratchRegister()));
      __ LoadRawPtrFromThread(jni_env, Thread::JniEnvOffset<kPointerSize>());
    } else {
      FrameOffset jni_env = main_jni_conv->CurrentParamStackOffset();
      __ CopyRawPtrFromThread(jni_env,
                              Thread::JniEnvOffset<kPointerSize>(),
                              main_jni_conv->InterproceduralScratchRegister());
    }
  }

  // 9. Plant call to native code associated with method.
  MemberOffset jni_entrypoint_offset =
      ArtMethod::EntryPointFromJniOffset(InstructionSetPointerSize(instruction_set));
  // FIXME: Not sure if MethodStackOffset will work here. What does it even do?
  __ Call(main_jni_conv->MethodStackOffset(),
          jni_entrypoint_offset,
          // XX: Why not the jni conv scratch register?
          mr_conv->InterproceduralScratchRegister());

  // 10. Fix differences in result widths.
  if (main_jni_conv->RequiresSmallResultTypeExtension()) {
    if (main_jni_conv->GetReturnType() == Primitive::kPrimByte ||
        main_jni_conv->GetReturnType() == Primitive::kPrimShort) {
      __ SignExtend(main_jni_conv->ReturnRegister(),
                    Primitive::ComponentSize(main_jni_conv->GetReturnType()));
    } else if (main_jni_conv->GetReturnType() == Primitive::kPrimBoolean ||
               main_jni_conv->GetReturnType() == Primitive::kPrimChar) {
      __ ZeroExtend(main_jni_conv->ReturnRegister(),
                    Primitive::ComponentSize(main_jni_conv->GetReturnType()));
    }
  }

  // 11. Process return value
  FrameOffset return_save_location = main_jni_conv->ReturnValueSaveLocation();
  if (main_jni_conv->SizeOfReturnValue() != 0 && !reference_return) {
    if (LIKELY(!is_critical_native)) {
      // For normal JNI, store the return value on the stack because the call to
      // JniMethodEnd will clobber the return value. It will be restored in (13).
      if ((instruction_set == InstructionSet::kMips ||
           instruction_set == InstructionSet::kMips64) &&
          main_jni_conv->GetReturnType() == Primitive::kPrimDouble &&
          return_save_location.Uint32Value() % 8 != 0) {
        // Ensure doubles are 8-byte aligned for MIPS
        return_save_location = FrameOffset(return_save_location.Uint32Value()
                                               + static_cast<size_t>(kMipsPointerSize));
        // TODO: refactor this into the JniCallingConvention code
        // as a return value alignment requirement.
      }
      CHECK_LT(return_save_location.Uint32Value(), frame_size + main_out_arg_size);
      __ Store(return_save_location,
               main_jni_conv->ReturnRegister(),
               main_jni_conv->SizeOfReturnValue());
    } else {
      // For @CriticalNative only,
      // move the JNI return register into the managed return register (if they don't match).
      ManagedRegister jni_return_reg = main_jni_conv->ReturnRegister();
      ManagedRegister mr_return_reg = mr_conv->ReturnRegister();

      // Check if the JNI return register matches the managed return register.
      // If they differ, only then do we have to do anything about it.
      // Otherwise the return value is already in the right place when we return.
      if (!jni_return_reg.Equals(mr_return_reg)) {
        // This is typically only necessary on ARM32 due to native being softfloat
        // while managed is hardfloat.
        // -- For example VMOV {r0, r1} -> D0; VMOV r0 -> S0.
        __ Move(mr_return_reg, jni_return_reg, main_jni_conv->SizeOfReturnValue());
      } else if (jni_return_reg.IsNoRegister() && mr_return_reg.IsNoRegister()) {
        // Sanity check: If the return value is passed on the stack for some reason,
        // then make sure the size matches.
        CHECK_EQ(main_jni_conv->SizeOfReturnValue(), mr_conv->SizeOfReturnValue());
      }
    }
  }

  // Increase frame size for out args if needed by the end_jni_conv.
  const size_t end_out_arg_size = end_jni_conv->OutArgSize();
  if (end_out_arg_size > current_out_arg_size) {
    size_t out_arg_size_diff = end_out_arg_size - current_out_arg_size;
    current_out_arg_size = end_out_arg_size;
    // TODO: This is redundant for @CriticalNative but we need to
    // conditionally do __DecreaseFrameSize below.
    __ IncreaseFrameSize(out_arg_size_diff);
    saved_cookie_offset = FrameOffset(saved_cookie_offset.SizeValue() + out_arg_size_diff);
    locked_object_handle_scope_offset =
        FrameOffset(locked_object_handle_scope_offset.SizeValue() + out_arg_size_diff);
    return_save_location = FrameOffset(return_save_location.SizeValue() + out_arg_size_diff);
  }
  //     thread.
  end_jni_conv->ResetIterator(FrameOffset(end_out_arg_size));

  if (LIKELY(!is_critical_native)) {
    // 12. Call JniMethodEnd
    ThreadOffset<kPointerSize> jni_end(
        GetJniEntrypointThreadOffset<kPointerSize>(JniEntrypoint::kEnd,
                                                   reference_return,
                                                   is_synchronized,
                                                   is_fast_native).SizeValue());
    if (reference_return) {
      // Pass result.
      SetNativeParameter(jni_asm.get(), end_jni_conv.get(), end_jni_conv->ReturnRegister());
      end_jni_conv->Next();
    }
    // Pass saved local reference state.
    if (end_jni_conv->IsCurrentParamOnStack()) {
      FrameOffset out_off = end_jni_conv->CurrentParamStackOffset();
      __ Copy(out_off, saved_cookie_offset, end_jni_conv->InterproceduralScratchRegister(), 4);
    } else {
      ManagedRegister out_reg = end_jni_conv->CurrentParamRegister();
      __ Load(out_reg, saved_cookie_offset, 4);
    }
    end_jni_conv->Next();
    if (is_synchronized) {
      // Pass object for unlocking.
      if (end_jni_conv->IsCurrentParamOnStack()) {
        FrameOffset out_off = end_jni_conv->CurrentParamStackOffset();
        __ CreateHandleScopeEntry(out_off, locked_object_handle_scope_offset,
                           end_jni_conv->InterproceduralScratchRegister(),
                           false);
      } else {
        ManagedRegister out_reg = end_jni_conv->CurrentParamRegister();
        __ CreateHandleScopeEntry(out_reg, locked_object_handle_scope_offset,
                           ManagedRegister::NoRegister(), false);
      }
      end_jni_conv->Next();
    }
    if (end_jni_conv->IsCurrentParamInRegister()) {
      __ GetCurrentThread(end_jni_conv->CurrentParamRegister());
      __ Call(end_jni_conv->CurrentParamRegister(),
              Offset(jni_end),
              end_jni_conv->InterproceduralScratchRegister());
    } else {
      __ GetCurrentThread(end_jni_conv->CurrentParamStackOffset(),
                          end_jni_conv->InterproceduralScratchRegister());
      __ CallFromThread(jni_end, end_jni_conv->InterproceduralScratchRegister());
    }

    // 13. Reload return value
    if (main_jni_conv->SizeOfReturnValue() != 0 && !reference_return) {
      __ Load(mr_conv->ReturnRegister(), return_save_location, mr_conv->SizeOfReturnValue());
      // NIT: If it's @CriticalNative then we actually only need to do this IF
      // the calling convention's native return register doesn't match the managed convention's
      // return register.
    }
  }  // if (!is_critical_native)

  // 14. Move frame up now we're done with the out arg space.
  __ DecreaseFrameSize(current_out_arg_size);

  // 15. Process pending exceptions from JNI call or monitor exit.
  __ ExceptionPoll(main_jni_conv->InterproceduralScratchRegister(), 0 /* stack_adjust */);

  // 16. Remove activation - need to restore callee save registers since the GC may have changed
  //     them.
  DCHECK_EQ(jni_asm->cfi().GetCurrentCFAOffset(), static_cast<int>(frame_size));
  // We expect the compiled method to possibly be suspended during its
  // execution, except in the case of a CriticalNative method.
  bool may_suspend = !is_critical_native;
  __ RemoveFrame(frame_size, callee_save_regs, may_suspend);
  DCHECK_EQ(jni_asm->cfi().GetCurrentCFAOffset(), static_cast<int>(frame_size));

  // 17. Finalize code generation
  __ FinalizeCode();
  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);

  return JniCompiledMethod(instruction_set,
                           std::move(managed_code),
                           frame_size,
                           main_jni_conv->CoreSpillMask(),
                           main_jni_conv->FpSpillMask(),
                           ArrayRef<const uint8_t>(*jni_asm->cfi().data()));
}

// Copy a single parameter from the managed to the JNI calling convention.
template <PointerSize kPointerSize>
static void CopyParameter(JNIMacroAssembler<kPointerSize>* jni_asm,
                          ManagedRuntimeCallingConvention* mr_conv,
                          JniCallingConvention* jni_conv,
                          size_t frame_size,
                          size_t out_arg_size) {
  bool input_in_reg = mr_conv->IsCurrentParamInRegister();
  bool output_in_reg = jni_conv->IsCurrentParamInRegister();
  FrameOffset handle_scope_offset(0);
  bool null_allowed = false;
  bool ref_param = jni_conv->IsCurrentParamAReference();
  CHECK(!ref_param || mr_conv->IsCurrentParamAReference());
  // input may be in register, on stack or both - but not none!
  CHECK(input_in_reg || mr_conv->IsCurrentParamOnStack());
  if (output_in_reg) {  // output shouldn't straddle registers and stack
    CHECK(!jni_conv->IsCurrentParamOnStack());
  } else {
    CHECK(jni_conv->IsCurrentParamOnStack());
  }
  // References need placing in handle scope and the entry address passing.
  if (ref_param) {
    null_allowed = mr_conv->IsCurrentArgPossiblyNull();
    // Compute handle scope offset. Note null is placed in the handle scope but the jobject
    // passed to the native code must be null (not a pointer into the handle scope
    // as with regular references).
    handle_scope_offset = jni_conv->CurrentParamHandleScopeEntryOffset();
    // Check handle scope offset is within frame.
    CHECK_LT(handle_scope_offset.Uint32Value(), (frame_size + out_arg_size));
  }
  if (input_in_reg && output_in_reg) {
    ManagedRegister in_reg = mr_conv->CurrentParamRegister();
    ManagedRegister out_reg = jni_conv->CurrentParamRegister();
    if (ref_param) {
      __ CreateHandleScopeEntry(out_reg, handle_scope_offset, in_reg, null_allowed);
    } else {
      if (!mr_conv->IsCurrentParamOnStack()) {
        // regular non-straddling move
        __ Move(out_reg, in_reg, mr_conv->CurrentParamSize());
      } else {
        UNIMPLEMENTED(FATAL);  // we currently don't expect to see this case
      }
    }
  } else if (!input_in_reg && !output_in_reg) {
    FrameOffset out_off = jni_conv->CurrentParamStackOffset();
    if (ref_param) {
      __ CreateHandleScopeEntry(out_off, handle_scope_offset, mr_conv->InterproceduralScratchRegister(),
                         null_allowed);
    } else {
      FrameOffset in_off = mr_conv->CurrentParamStackOffset();
      size_t param_size = mr_conv->CurrentParamSize();
      CHECK_EQ(param_size, jni_conv->CurrentParamSize());
      __ Copy(out_off, in_off, mr_conv->InterproceduralScratchRegister(), param_size);
    }
  } else if (!input_in_reg && output_in_reg) {
    FrameOffset in_off = mr_conv->CurrentParamStackOffset();
    ManagedRegister out_reg = jni_conv->CurrentParamRegister();
    // Check that incoming stack arguments are above the current stack frame.
    CHECK_GT(in_off.Uint32Value(), frame_size);
    if (ref_param) {
      __ CreateHandleScopeEntry(out_reg, handle_scope_offset, ManagedRegister::NoRegister(), null_allowed);
    } else {
      size_t param_size = mr_conv->CurrentParamSize();
      CHECK_EQ(param_size, jni_conv->CurrentParamSize());
      __ Load(out_reg, in_off, param_size);
    }
  } else {
    CHECK(input_in_reg && !output_in_reg);
    ManagedRegister in_reg = mr_conv->CurrentParamRegister();
    FrameOffset out_off = jni_conv->CurrentParamStackOffset();
    // Check outgoing argument is within frame
    CHECK_LT(out_off.Uint32Value(), frame_size);
    if (ref_param) {
      // TODO: recycle value in in_reg rather than reload from handle scope
      __ CreateHandleScopeEntry(out_off, handle_scope_offset, mr_conv->InterproceduralScratchRegister(),
                         null_allowed);
    } else {
      size_t param_size = mr_conv->CurrentParamSize();
      CHECK_EQ(param_size, jni_conv->CurrentParamSize());
      if (!mr_conv->IsCurrentParamOnStack()) {
        // regular non-straddling store
        __ Store(out_off, in_reg, param_size);
      } else {
        // store where input straddles registers and stack
        CHECK_EQ(param_size, 8u);
        FrameOffset in_off = mr_conv->CurrentParamStackOffset();
        __ StoreSpanning(out_off, in_reg, in_off, mr_conv->InterproceduralScratchRegister());
      }
    }
  }
}

template <PointerSize kPointerSize>
static void SetNativeParameter(JNIMacroAssembler<kPointerSize>* jni_asm,
                               JniCallingConvention* jni_conv,
                               ManagedRegister in_reg) {
  if (jni_conv->IsCurrentParamOnStack()) {
    FrameOffset dest = jni_conv->CurrentParamStackOffset();
    __ StoreRawPtr(dest, in_reg);
  } else {
    if (!jni_conv->CurrentParamRegister().Equals(in_reg)) {
      __ Move(jni_conv->CurrentParamRegister(), in_reg, jni_conv->CurrentParamSize());
    }
  }
}

JniCompiledMethod ArtQuickJniCompileMethod(CompilerDriver* compiler,
                                           uint32_t access_flags,
                                           uint32_t method_idx,
                                           const DexFile& dex_file) {
  if (Is64BitInstructionSet(compiler->GetInstructionSet())) {
    return ArtJniCompileMethodInternal<PointerSize::k64>(
        compiler, access_flags, method_idx, dex_file);
  } else {
    return ArtJniCompileMethodInternal<PointerSize::k32>(
        compiler, access_flags, method_idx, dex_file);
  }
}

}  // namespace art
