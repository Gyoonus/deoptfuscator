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

#include "interpreter_switch_impl.h"

#include "base/enums.h"
#include "base/quasi_atomic.h"
#include "dex/dex_file_types.h"
#include "experimental_flags.h"
#include "interpreter_common.h"
#include "jit/jit.h"
#include "jvalue-inl.h"
#include "safe_math.h"

namespace art {
namespace interpreter {

#define HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(instr)                                    \
  do {                                                                                          \
    DCHECK(self->IsExceptionPending());                                                         \
    self->AllowThreadSuspension();                                                              \
    if (!MoveToExceptionHandler(self, shadow_frame, instr)) {                                   \
      /* Structured locking is to be enforced for abnormal termination, too. */                 \
      DoMonitorCheckOnExit<do_assignability_check>(self, &shadow_frame);                        \
      if (interpret_one_instruction) {                                                          \
        /* Signal mterp to return to caller */                                                  \
        shadow_frame.SetDexPC(dex::kDexNoIndex);                                                \
      }                                                                                         \
      ctx->result = JValue(); /* Handled in caller. */                                          \
      return;                                                                                   \
    } else {                                                                                    \
      int32_t displacement =                                                                    \
          static_cast<int32_t>(shadow_frame.GetDexPC()) - static_cast<int32_t>(dex_pc);         \
      inst = inst->RelativeAt(displacement);                                                    \
    }                                                                                           \
  } while (false)

#define HANDLE_PENDING_EXCEPTION() HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(instrumentation)

#define POSSIBLY_HANDLE_PENDING_EXCEPTION(_is_exception_pending, _next_function)  \
  do {                                                                            \
    if (UNLIKELY(_is_exception_pending)) {                                        \
      HANDLE_PENDING_EXCEPTION();                                                 \
    } else {                                                                      \
      inst = inst->_next_function();                                              \
    }                                                                             \
  } while (false)

#define HANDLE_MONITOR_CHECKS()                                                                   \
  if (!DoMonitorCheckOnExit<do_assignability_check>(self, &shadow_frame)) {                       \
    HANDLE_PENDING_EXCEPTION();                                                                   \
  }

// Code to run before each dex instruction.
#define PREAMBLE_SAVE(save_ref)                                                                      \
  {                                                                                             \
    if (UNLIKELY(instrumentation->HasDexPcListeners()) &&                                       \
        UNLIKELY(!DoDexPcMoveEvent(self,                                                        \
                                   accessor,                                                    \
                                   shadow_frame,                                                \
                                   dex_pc,                                                      \
                                   instrumentation,                                             \
                                   save_ref))) {                                                \
      HANDLE_PENDING_EXCEPTION();                                                               \
      break;                                                                                    \
    }                                                                                           \
  }                                                                                             \
  do {} while (false)

#define PREAMBLE() PREAMBLE_SAVE(nullptr)

#define BRANCH_INSTRUMENTATION(offset)                                                         \
  do {                                                                                         \
    if (UNLIKELY(instrumentation->HasBranchListeners())) {                                     \
      instrumentation->Branch(self, shadow_frame.GetMethod(), dex_pc, offset);                 \
    }                                                                                          \
    JValue result;                                                                             \
    if (jit::Jit::MaybeDoOnStackReplacement(self,                                              \
                                            shadow_frame.GetMethod(),                          \
                                            dex_pc,                                            \
                                            offset,                                            \
                                            &result)) {                                        \
      if (interpret_one_instruction) {                                                         \
        /* OSR has completed execution of the method.  Signal mterp to return to caller */     \
        shadow_frame.SetDexPC(dex::kDexNoIndex);                                               \
      }                                                                                        \
      ctx->result = result;                                                                    \
      return;                                                                                  \
    }                                                                                          \
  } while (false)

#define HOTNESS_UPDATE()                                                                       \
  do {                                                                                         \
    if (jit != nullptr) {                                                                      \
      jit->AddSamples(self, shadow_frame.GetMethod(), 1, /*with_backedges*/ true);             \
    }                                                                                          \
  } while (false)

#define HANDLE_ASYNC_EXCEPTION()                                                               \
  if (UNLIKELY(self->ObserveAsyncException())) {                                               \
    HANDLE_PENDING_EXCEPTION();                                                                \
    break;                                                                                     \
  }                                                                                            \
  do {} while (false)

#define HANDLE_BACKWARD_BRANCH(offset)                                                         \
  do {                                                                                         \
    if (IsBackwardBranch(offset)) {                                                            \
      HOTNESS_UPDATE();                                                                        \
      /* Record new dex pc early to have consistent suspend point at loop header. */           \
      shadow_frame.SetDexPC(inst->GetDexPc(insns));                                            \
      self->AllowThreadSuspension();                                                           \
    }                                                                                          \
  } while (false)

// Unlike most other events the DexPcMovedEvent can be sent when there is a pending exception (if
// the next instruction is MOVE_EXCEPTION). This means it needs to be handled carefully to be able
// to detect exceptions thrown by the DexPcMovedEvent itself. These exceptions could be thrown by
// jvmti-agents while handling breakpoint or single step events. We had to move this into its own
// function because it was making ExecuteSwitchImpl have too large a stack.
NO_INLINE static bool DoDexPcMoveEvent(Thread* self,
                                       const CodeItemDataAccessor& accessor,
                                       const ShadowFrame& shadow_frame,
                                       uint32_t dex_pc,
                                       const instrumentation::Instrumentation* instrumentation,
                                       JValue* save_ref)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(instrumentation->HasDexPcListeners());
  StackHandleScope<2> hs(self);
  Handle<mirror::Throwable> thr(hs.NewHandle(self->GetException()));
  mirror::Object* null_obj = nullptr;
  HandleWrapper<mirror::Object> h(
      hs.NewHandleWrapper(LIKELY(save_ref == nullptr) ? &null_obj : save_ref->GetGCRoot()));
  self->ClearException();
  instrumentation->DexPcMovedEvent(self,
                                   shadow_frame.GetThisObject(accessor.InsSize()),
                                   shadow_frame.GetMethod(),
                                   dex_pc);
  if (UNLIKELY(self->IsExceptionPending())) {
    // We got a new exception in the dex-pc-moved event. We just let this exception replace the old
    // one.
    // TODO It would be good to add the old exception to the suppressed exceptions of the new one if
    // possible.
    return false;
  } else {
    if (UNLIKELY(!thr.IsNull())) {
      self->SetException(thr.Get());
    }
    return true;
  }
}

static bool NeedsMethodExitEvent(const instrumentation::Instrumentation* ins)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return ins->HasMethodExitListeners() || ins->HasWatchedFramePopListeners();
}

// Sends the normal method exit event. Returns true if the events succeeded and false if there is a
// pending exception.
NO_INLINE static bool SendMethodExitEvents(Thread* self,
                                           const instrumentation::Instrumentation* instrumentation,
                                           const ShadowFrame& frame,
                                           ObjPtr<mirror::Object> thiz,
                                           ArtMethod* method,
                                           uint32_t dex_pc,
                                           const JValue& result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  bool had_event = false;
  if (UNLIKELY(instrumentation->HasMethodExitListeners())) {
    had_event = true;
    instrumentation->MethodExitEvent(self, thiz.Ptr(), method, dex_pc, result);
  }
  if (UNLIKELY(frame.NeedsNotifyPop() && instrumentation->HasWatchedFramePopListeners())) {
    had_event = true;
    instrumentation->WatchedFramePopped(self, frame);
  }
  if (UNLIKELY(had_event)) {
    return !self->IsExceptionPending();
  } else {
    return true;
  }
}

template<bool do_access_check, bool transaction_active>
void ExecuteSwitchImplCpp(SwitchImplContext* ctx) {
  Thread* self = ctx->self;
  const CodeItemDataAccessor& accessor = ctx->accessor;
  ShadowFrame& shadow_frame = ctx->shadow_frame;
  JValue result_register = ctx->result_register;
  bool interpret_one_instruction = ctx->interpret_one_instruction;
  constexpr bool do_assignability_check = do_access_check;
  if (UNLIKELY(!shadow_frame.HasReferenceArray())) {
    LOG(FATAL) << "Invalid shadow frame for interpreter use";
    ctx->result = JValue();
    return;
  }
  self->VerifyStack();

  uint32_t dex_pc = shadow_frame.GetDexPC();
  const auto* const instrumentation = Runtime::Current()->GetInstrumentation();
  const uint16_t* const insns = accessor.Insns();
  const Instruction* inst = Instruction::At(insns + dex_pc);
  uint16_t inst_data;
  jit::Jit* jit = Runtime::Current()->GetJit();

  do {
    dex_pc = inst->GetDexPc(insns);
    shadow_frame.SetDexPC(dex_pc);
    TraceExecution(shadow_frame, inst, dex_pc);
    inst_data = inst->Fetch16(0);
    switch (inst->Opcode(inst_data)) {
      case Instruction::NOP:
        PREAMBLE();
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_FROM16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_32x(),
                             shadow_frame.GetVReg(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_WIDE:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_WIDE_FROM16:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_22x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_WIDE_16:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_32x(),
                                 shadow_frame.GetVRegLong(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_OBJECT:
        PREAMBLE();
        shadow_frame.SetVRegReference(inst->VRegA_12x(inst_data),
                                      shadow_frame.GetVRegReference(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_OBJECT_FROM16:
        PREAMBLE();
        shadow_frame.SetVRegReference(inst->VRegA_22x(inst_data),
                                      shadow_frame.GetVRegReference(inst->VRegB_22x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MOVE_OBJECT_16:
        PREAMBLE();
        shadow_frame.SetVRegReference(inst->VRegA_32x(),
                                      shadow_frame.GetVRegReference(inst->VRegB_32x()));
        inst = inst->Next_3xx();
        break;
      case Instruction::MOVE_RESULT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_11x(inst_data), result_register.GetI());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_RESULT_WIDE:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_11x(inst_data), result_register.GetJ());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_RESULT_OBJECT:
        PREAMBLE_SAVE(&result_register);
        shadow_frame.SetVRegReference(inst->VRegA_11x(inst_data), result_register.GetL());
        inst = inst->Next_1xx();
        break;
      case Instruction::MOVE_EXCEPTION: {
        PREAMBLE();
        ObjPtr<mirror::Throwable> exception = self->GetException();
        DCHECK(exception != nullptr) << "No pending exception on MOVE_EXCEPTION instruction";
        shadow_frame.SetVRegReference(inst->VRegA_11x(inst_data), exception.Ptr());
        self->ClearException();
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::RETURN_VOID_NO_BARRIER: {
        PREAMBLE();
        JValue result;
        self->AllowThreadSuspension();
        HANDLE_MONITOR_CHECKS();
        if (UNLIKELY(NeedsMethodExitEvent(instrumentation) &&
                     !SendMethodExitEvents(self,
                                           instrumentation,
                                           shadow_frame,
                                           shadow_frame.GetThisObject(accessor.InsSize()),
                                           shadow_frame.GetMethod(),
                                           inst->GetDexPc(insns),
                                           result))) {
          HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(nullptr);
        }
        if (interpret_one_instruction) {
          /* Signal mterp to return to caller */
          shadow_frame.SetDexPC(dex::kDexNoIndex);
        }
        ctx->result = result;
        return;
      }
      case Instruction::RETURN_VOID: {
        PREAMBLE();
        QuasiAtomic::ThreadFenceForConstructor();
        JValue result;
        self->AllowThreadSuspension();
        HANDLE_MONITOR_CHECKS();
        if (UNLIKELY(NeedsMethodExitEvent(instrumentation) &&
                     !SendMethodExitEvents(self,
                                           instrumentation,
                                           shadow_frame,
                                           shadow_frame.GetThisObject(accessor.InsSize()),
                                           shadow_frame.GetMethod(),
                                           inst->GetDexPc(insns),
                                           result))) {
          HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(nullptr);
        }
        if (interpret_one_instruction) {
          /* Signal mterp to return to caller */
          shadow_frame.SetDexPC(dex::kDexNoIndex);
        }
        ctx->result = result;
        return;
      }
      case Instruction::RETURN: {
        PREAMBLE();
        JValue result;
        result.SetJ(0);
        result.SetI(shadow_frame.GetVReg(inst->VRegA_11x(inst_data)));
        self->AllowThreadSuspension();
        HANDLE_MONITOR_CHECKS();
        if (UNLIKELY(NeedsMethodExitEvent(instrumentation) &&
                     !SendMethodExitEvents(self,
                                           instrumentation,
                                           shadow_frame,
                                           shadow_frame.GetThisObject(accessor.InsSize()),
                                           shadow_frame.GetMethod(),
                                           inst->GetDexPc(insns),
                                           result))) {
          HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(nullptr);
        }
        if (interpret_one_instruction) {
          /* Signal mterp to return to caller */
          shadow_frame.SetDexPC(dex::kDexNoIndex);
        }
        ctx->result = result;
        return;
      }
      case Instruction::RETURN_WIDE: {
        PREAMBLE();
        JValue result;
        result.SetJ(shadow_frame.GetVRegLong(inst->VRegA_11x(inst_data)));
        self->AllowThreadSuspension();
        HANDLE_MONITOR_CHECKS();
        if (UNLIKELY(NeedsMethodExitEvent(instrumentation) &&
                     !SendMethodExitEvents(self,
                                           instrumentation,
                                           shadow_frame,
                                           shadow_frame.GetThisObject(accessor.InsSize()),
                                           shadow_frame.GetMethod(),
                                           inst->GetDexPc(insns),
                                           result))) {
          HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(nullptr);
        }
        if (interpret_one_instruction) {
          /* Signal mterp to return to caller */
          shadow_frame.SetDexPC(dex::kDexNoIndex);
        }
        ctx->result = result;
        return;
      }
      case Instruction::RETURN_OBJECT: {
        PREAMBLE();
        JValue result;
        self->AllowThreadSuspension();
        HANDLE_MONITOR_CHECKS();
        const size_t ref_idx = inst->VRegA_11x(inst_data);
        ObjPtr<mirror::Object> obj_result = shadow_frame.GetVRegReference(ref_idx);
        if (do_assignability_check && obj_result != nullptr) {
          ObjPtr<mirror::Class> return_type = shadow_frame.GetMethod()->ResolveReturnType();
          // Re-load since it might have moved.
          obj_result = shadow_frame.GetVRegReference(ref_idx);
          if (return_type == nullptr) {
            // Return the pending exception.
            HANDLE_PENDING_EXCEPTION();
          }
          if (!obj_result->VerifierInstanceOf(return_type)) {
            // This should never happen.
            std::string temp1, temp2;
            self->ThrowNewExceptionF("Ljava/lang/InternalError;",
                                     "Returning '%s' that is not instance of return type '%s'",
                                     obj_result->GetClass()->GetDescriptor(&temp1),
                                     return_type->GetDescriptor(&temp2));
            HANDLE_PENDING_EXCEPTION();
          }
        }
        result.SetL(obj_result);
        if (UNLIKELY(NeedsMethodExitEvent(instrumentation) &&
                     !SendMethodExitEvents(self,
                                           instrumentation,
                                           shadow_frame,
                                           shadow_frame.GetThisObject(accessor.InsSize()),
                                           shadow_frame.GetMethod(),
                                           inst->GetDexPc(insns),
                                           result))) {
          HANDLE_PENDING_EXCEPTION_WITH_INSTRUMENTATION(nullptr);
        }
        // Re-load since it might have moved during the MethodExitEvent.
        result.SetL(shadow_frame.GetVRegReference(ref_idx));
        if (interpret_one_instruction) {
          /* Signal mterp to return to caller */
          shadow_frame.SetDexPC(dex::kDexNoIndex);
        }
        ctx->result = result;
        return;
      }
      case Instruction::CONST_4: {
        PREAMBLE();
        uint4_t dst = inst->VRegA_11n(inst_data);
        int4_t val = inst->VRegB_11n(inst_data);
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::CONST_16: {
        PREAMBLE();
        uint8_t dst = inst->VRegA_21s(inst_data);
        int16_t val = inst->VRegB_21s();
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CONST: {
        PREAMBLE();
        uint8_t dst = inst->VRegA_31i(inst_data);
        int32_t val = inst->VRegB_31i();
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_3xx();
        break;
      }
      case Instruction::CONST_HIGH16: {
        PREAMBLE();
        uint8_t dst = inst->VRegA_21h(inst_data);
        int32_t val = static_cast<int32_t>(inst->VRegB_21h() << 16);
        shadow_frame.SetVReg(dst, val);
        if (val == 0) {
          shadow_frame.SetVRegReference(dst, nullptr);
        }
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CONST_WIDE_16:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_21s(inst_data), inst->VRegB_21s());
        inst = inst->Next_2xx();
        break;
      case Instruction::CONST_WIDE_32:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_31i(inst_data), inst->VRegB_31i());
        inst = inst->Next_3xx();
        break;
      case Instruction::CONST_WIDE:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_51l(inst_data), inst->VRegB_51l());
        inst = inst->Next_51l();
        break;
      case Instruction::CONST_WIDE_HIGH16:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_21h(inst_data),
                                 static_cast<uint64_t>(inst->VRegB_21h()) << 48);
        inst = inst->Next_2xx();
        break;
      case Instruction::CONST_STRING: {
        PREAMBLE();
        ObjPtr<mirror::String> s = ResolveString(self,
                                                 shadow_frame,
                                                 dex::StringIndex(inst->VRegB_21c()));
        if (UNLIKELY(s == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), s.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::CONST_STRING_JUMBO: {
        PREAMBLE();
        ObjPtr<mirror::String> s = ResolveString(self,
                                                 shadow_frame,
                                                 dex::StringIndex(inst->VRegB_31c()));
        if (UNLIKELY(s == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_31c(inst_data), s.Ptr());
          inst = inst->Next_3xx();
        }
        break;
      }
      case Instruction::CONST_CLASS: {
        PREAMBLE();
        ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegB_21c()),
                                                         shadow_frame.GetMethod(),
                                                         self,
                                                         false,
                                                         do_access_check);
        if (UNLIKELY(c == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), c.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::CONST_METHOD_HANDLE: {
        PREAMBLE();
        ClassLinker* cl = Runtime::Current()->GetClassLinker();
        ObjPtr<mirror::MethodHandle> mh = cl->ResolveMethodHandle(self,
                                                                  inst->VRegB_21c(),
                                                                  shadow_frame.GetMethod());
        if (UNLIKELY(mh == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), mh.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::CONST_METHOD_TYPE: {
        PREAMBLE();
        ClassLinker* cl = Runtime::Current()->GetClassLinker();
        ObjPtr<mirror::MethodType> mt = cl->ResolveMethodType(self,
                                                              inst->VRegB_21c(),
                                                              shadow_frame.GetMethod());
        if (UNLIKELY(mt == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), mt.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::MONITOR_ENTER: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        if (UNLIKELY(obj == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          DoMonitorEnter<do_assignability_check>(self, &shadow_frame, obj);
          POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        }
        break;
      }
      case Instruction::MONITOR_EXIT: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        if (UNLIKELY(obj == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          DoMonitorExit<do_assignability_check>(self, &shadow_frame, obj);
          POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        }
        break;
      }
      case Instruction::CHECK_CAST: {
        PREAMBLE();
        ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegB_21c()),
                                                         shadow_frame.GetMethod(),
                                                         self,
                                                         false,
                                                         do_access_check);
        if (UNLIKELY(c == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(inst->VRegA_21c(inst_data));
          if (UNLIKELY(obj != nullptr && !obj->InstanceOf(c))) {
            ThrowClassCastException(c, obj->GetClass());
            HANDLE_PENDING_EXCEPTION();
          } else {
            inst = inst->Next_2xx();
          }
        }
        break;
      }
      case Instruction::INSTANCE_OF: {
        PREAMBLE();
        ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegC_22c()),
                                                         shadow_frame.GetMethod(),
                                                         self,
                                                         false,
                                                         do_access_check);
        if (UNLIKELY(c == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(inst->VRegB_22c(inst_data));
          shadow_frame.SetVReg(inst->VRegA_22c(inst_data),
                               (obj != nullptr && obj->InstanceOf(c)) ? 1 : 0);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::ARRAY_LENGTH:  {
        PREAMBLE();
        ObjPtr<mirror::Object> array = shadow_frame.GetVRegReference(inst->VRegB_12x(inst_data));
        if (UNLIKELY(array == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVReg(inst->VRegA_12x(inst_data), array->AsArray()->GetLength());
          inst = inst->Next_1xx();
        }
        break;
      }
      case Instruction::NEW_INSTANCE: {
        PREAMBLE();
        ObjPtr<mirror::Object> obj = nullptr;
        ObjPtr<mirror::Class> c = ResolveVerifyAndClinit(dex::TypeIndex(inst->VRegB_21c()),
                                                         shadow_frame.GetMethod(),
                                                         self,
                                                         false,
                                                         do_access_check);
        if (LIKELY(c != nullptr)) {
          if (UNLIKELY(c->IsStringClass())) {
            gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
            obj = mirror::String::AllocEmptyString<true>(self, allocator_type);
          } else {
            obj = AllocObjectFromCode<true>(
                c.Ptr(),
                self,
                Runtime::Current()->GetHeap()->GetCurrentAllocator());
          }
        }
        if (UNLIKELY(obj == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          obj->GetClass()->AssertInitializedOrInitializingInThread(self);
          // Don't allow finalizable objects to be allocated during a transaction since these can't
          // be finalized without a started runtime.
          if (transaction_active && obj->GetClass()->IsFinalizable()) {
            AbortTransactionF(self, "Allocating finalizable object in transaction: %s",
                              obj->PrettyTypeOf().c_str());
            HANDLE_PENDING_EXCEPTION();
            break;
          }
          shadow_frame.SetVRegReference(inst->VRegA_21c(inst_data), obj.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::NEW_ARRAY: {
        PREAMBLE();
        int32_t length = shadow_frame.GetVReg(inst->VRegB_22c(inst_data));
        ObjPtr<mirror::Object> obj = AllocArrayFromCode<do_access_check, true>(
            dex::TypeIndex(inst->VRegC_22c()),
            length,
            shadow_frame.GetMethod(),
            self,
            Runtime::Current()->GetHeap()->GetCurrentAllocator());
        if (UNLIKELY(obj == nullptr)) {
          HANDLE_PENDING_EXCEPTION();
        } else {
          shadow_frame.SetVRegReference(inst->VRegA_22c(inst_data), obj.Ptr());
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::FILLED_NEW_ARRAY: {
        PREAMBLE();
        bool success =
            DoFilledNewArray<false, do_access_check, transaction_active>(inst, shadow_frame, self,
                                                                         &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::FILLED_NEW_ARRAY_RANGE: {
        PREAMBLE();
        bool success =
            DoFilledNewArray<true, do_access_check, transaction_active>(inst, shadow_frame,
                                                                        self, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::FILL_ARRAY_DATA: {
        PREAMBLE();
        const uint16_t* payload_addr = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
        const Instruction::ArrayDataPayload* payload =
            reinterpret_cast<const Instruction::ArrayDataPayload*>(payload_addr);
        ObjPtr<mirror::Object> obj = shadow_frame.GetVRegReference(inst->VRegA_31t(inst_data));
        bool success = FillArrayData(obj, payload);
        if (!success) {
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        if (transaction_active) {
          RecordArrayElementsInTransaction(obj->AsArray(), payload->element_count);
        }
        inst = inst->Next_3xx();
        break;
      }
      case Instruction::THROW: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        ObjPtr<mirror::Object> exception =
            shadow_frame.GetVRegReference(inst->VRegA_11x(inst_data));
        if (UNLIKELY(exception == nullptr)) {
          ThrowNullPointerException("throw with null exception");
        } else if (do_assignability_check && !exception->GetClass()->IsThrowableClass()) {
          // This should never happen.
          std::string temp;
          self->ThrowNewExceptionF("Ljava/lang/InternalError;",
                                   "Throwing '%s' that is not instance of Throwable",
                                   exception->GetClass()->GetDescriptor(&temp));
        } else {
          self->SetException(exception->AsThrowable());
        }
        HANDLE_PENDING_EXCEPTION();
        break;
      }
      case Instruction::GOTO: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        int8_t offset = inst->VRegA_10t(inst_data);
        BRANCH_INSTRUMENTATION(offset);
        inst = inst->RelativeAt(offset);
        HANDLE_BACKWARD_BRANCH(offset);
        break;
      }
      case Instruction::GOTO_16: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        int16_t offset = inst->VRegA_20t();
        BRANCH_INSTRUMENTATION(offset);
        inst = inst->RelativeAt(offset);
        HANDLE_BACKWARD_BRANCH(offset);
        break;
      }
      case Instruction::GOTO_32: {
        PREAMBLE();
        HANDLE_ASYNC_EXCEPTION();
        int32_t offset = inst->VRegA_30t();
        BRANCH_INSTRUMENTATION(offset);
        inst = inst->RelativeAt(offset);
        HANDLE_BACKWARD_BRANCH(offset);
        break;
      }
      case Instruction::PACKED_SWITCH: {
        PREAMBLE();
        int32_t offset = DoPackedSwitch(inst, shadow_frame, inst_data);
        BRANCH_INSTRUMENTATION(offset);
        inst = inst->RelativeAt(offset);
        HANDLE_BACKWARD_BRANCH(offset);
        break;
      }
      case Instruction::SPARSE_SWITCH: {
        PREAMBLE();
        int32_t offset = DoSparseSwitch(inst, shadow_frame, inst_data);
        BRANCH_INSTRUMENTATION(offset);
        inst = inst->RelativeAt(offset);
        HANDLE_BACKWARD_BRANCH(offset);
        break;
      }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"

      case Instruction::CMPL_FLOAT: {
        PREAMBLE();
        float val1 = shadow_frame.GetVRegFloat(inst->VRegB_23x());
        float val2 = shadow_frame.GetVRegFloat(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CMPG_FLOAT: {
        PREAMBLE();
        float val1 = shadow_frame.GetVRegFloat(inst->VRegB_23x());
        float val2 = shadow_frame.GetVRegFloat(inst->VRegC_23x());
        int32_t result;
        if (val1 < val2) {
          result = -1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = 1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::CMPL_DOUBLE: {
        PREAMBLE();
        double val1 = shadow_frame.GetVRegDouble(inst->VRegB_23x());
        double val2 = shadow_frame.GetVRegDouble(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }

      case Instruction::CMPG_DOUBLE: {
        PREAMBLE();
        double val1 = shadow_frame.GetVRegDouble(inst->VRegB_23x());
        double val2 = shadow_frame.GetVRegDouble(inst->VRegC_23x());
        int32_t result;
        if (val1 < val2) {
          result = -1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = 1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }

#pragma clang diagnostic pop

      case Instruction::CMP_LONG: {
        PREAMBLE();
        int64_t val1 = shadow_frame.GetVRegLong(inst->VRegB_23x());
        int64_t val2 = shadow_frame.GetVRegLong(inst->VRegC_23x());
        int32_t result;
        if (val1 > val2) {
          result = 1;
        } else if (val1 == val2) {
          result = 0;
        } else {
          result = -1;
        }
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data), result);
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::IF_EQ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) ==
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_NE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) !=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LT: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) <
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) >=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GT: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) >
        shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LE: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_22t(inst_data)) <=
            shadow_frame.GetVReg(inst->VRegB_22t(inst_data))) {
          int16_t offset = inst->VRegC_22t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_EQZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) == 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_NEZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) != 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LTZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) < 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GEZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) >= 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_GTZ: {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) > 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::IF_LEZ:  {
        PREAMBLE();
        if (shadow_frame.GetVReg(inst->VRegA_21t(inst_data)) <= 0) {
          int16_t offset = inst->VRegB_21t();
          BRANCH_INSTRUMENTATION(offset);
          inst = inst->RelativeAt(offset);
          HANDLE_BACKWARD_BRANCH(offset);
        } else {
          BRANCH_INSTRUMENTATION(2);
          inst = inst->Next_2xx();
        }
        break;
      }
      case Instruction::AGET_BOOLEAN: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::BooleanArray> array = a->AsBooleanArray();
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET_BYTE: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::ByteArray> array = a->AsByteArray();
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET_CHAR: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::CharArray> array = a->AsCharArray();
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET_SHORT: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::ShortArray> array = a->AsShortArray();
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        DCHECK(a->IsIntArray() || a->IsFloatArray()) << a->PrettyTypeOf();
        ObjPtr<mirror::IntArray> array = ObjPtr<mirror::IntArray>::DownCast(a);
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVReg(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET_WIDE:  {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        DCHECK(a->IsLongArray() || a->IsDoubleArray()) << a->PrettyTypeOf();
        ObjPtr<mirror::LongArray> array = ObjPtr<mirror::LongArray>::DownCast(a);
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::AGET_OBJECT: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::ObjectArray<mirror::Object>> array = a->AsObjectArray<mirror::Object>();
        if (array->CheckIsValidIndex(index)) {
          shadow_frame.SetVRegReference(inst->VRegA_23x(inst_data), array->GetWithoutChecks(index));
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_BOOLEAN: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        uint8_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::BooleanArray> array = a->AsBooleanArray();
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_BYTE: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int8_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::ByteArray> array = a->AsByteArray();
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_CHAR: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        uint16_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::CharArray> array = a->AsCharArray();
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_SHORT: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int16_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::ShortArray> array = a->AsShortArray();
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t val = shadow_frame.GetVReg(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        DCHECK(a->IsIntArray() || a->IsFloatArray()) << a->PrettyTypeOf();
        ObjPtr<mirror::IntArray> array = ObjPtr<mirror::IntArray>::DownCast(a);
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_WIDE: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int64_t val = shadow_frame.GetVRegLong(inst->VRegA_23x(inst_data));
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        DCHECK(a->IsLongArray() || a->IsDoubleArray()) << a->PrettyTypeOf();
        ObjPtr<mirror::LongArray> array = ObjPtr<mirror::LongArray>::DownCast(a);
        if (array->CheckIsValidIndex(index)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::APUT_OBJECT: {
        PREAMBLE();
        ObjPtr<mirror::Object> a = shadow_frame.GetVRegReference(inst->VRegB_23x());
        if (UNLIKELY(a == nullptr)) {
          ThrowNullPointerExceptionFromInterpreter();
          HANDLE_PENDING_EXCEPTION();
          break;
        }
        int32_t index = shadow_frame.GetVReg(inst->VRegC_23x());
        ObjPtr<mirror::Object> val = shadow_frame.GetVRegReference(inst->VRegA_23x(inst_data));
        ObjPtr<mirror::ObjectArray<mirror::Object>> array = a->AsObjectArray<mirror::Object>();
        if (array->CheckIsValidIndex(index) && array->CheckAssignable(val)) {
          array->SetWithoutChecks<transaction_active>(index, val);
          inst = inst->Next_2xx();
        } else {
          HANDLE_PENDING_EXCEPTION();
        }
        break;
      }
      case Instruction::IGET_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimBoolean, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BYTE: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimByte, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_CHAR: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimChar, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_SHORT: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimShort, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimInt, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_WIDE: {
        PREAMBLE();
        bool success = DoFieldGet<InstancePrimitiveRead, Primitive::kPrimLong, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_OBJECT: {
        PREAMBLE();
        bool success = DoFieldGet<InstanceObjectRead, Primitive::kPrimNot, do_access_check>(
            self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimInt>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_WIDE_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimLong>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_OBJECT_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimNot>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BOOLEAN_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimBoolean>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_BYTE_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimByte>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_CHAR_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimChar>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IGET_SHORT_QUICK: {
        PREAMBLE();
        bool success = DoIGetQuick<Primitive::kPrimShort>(shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimBoolean, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_BYTE: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimByte, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_CHAR: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimChar, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_SHORT: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimShort, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimInt, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_WIDE: {
        PREAMBLE();
        bool success = DoFieldGet<StaticPrimitiveRead, Primitive::kPrimLong, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SGET_OBJECT: {
        PREAMBLE();
        bool success = DoFieldGet<StaticObjectRead, Primitive::kPrimNot, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimBoolean, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BYTE: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimByte, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_CHAR: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimChar, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_SHORT: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimShort, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimInt, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_WIDE: {
        PREAMBLE();
        bool success = DoFieldPut<InstancePrimitiveWrite, Primitive::kPrimLong, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_OBJECT: {
        PREAMBLE();
        bool success = DoFieldPut<InstanceObjectWrite, Primitive::kPrimNot, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimInt, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BOOLEAN_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimBoolean, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_BYTE_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimByte, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_CHAR_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimChar, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_SHORT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimShort, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_WIDE_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimLong, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::IPUT_OBJECT_QUICK: {
        PREAMBLE();
        bool success = DoIPutQuick<Primitive::kPrimNot, transaction_active>(
            shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_BOOLEAN: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimBoolean, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_BYTE: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimByte, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_CHAR: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimChar, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_SHORT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimShort, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimInt, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_WIDE: {
        PREAMBLE();
        bool success = DoFieldPut<StaticPrimitiveWrite, Primitive::kPrimLong, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SPUT_OBJECT: {
        PREAMBLE();
        bool success = DoFieldPut<StaticObjectWrite, Primitive::kPrimNot, do_access_check,
            transaction_active>(self, shadow_frame, inst, inst_data);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL: {
        PREAMBLE();
        bool success = DoInvoke<kVirtual, false, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kVirtual, true, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_SUPER: {
        PREAMBLE();
        bool success = DoInvoke<kSuper, false, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_SUPER_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kSuper, true, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_DIRECT: {
        PREAMBLE();
        bool success = DoInvoke<kDirect, false, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_DIRECT_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kDirect, true, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_INTERFACE: {
        PREAMBLE();
        bool success = DoInvoke<kInterface, false, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_INTERFACE_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kInterface, true, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_STATIC: {
        PREAMBLE();
        bool success = DoInvoke<kStatic, false, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_STATIC_RANGE: {
        PREAMBLE();
        bool success = DoInvoke<kStatic, true, do_access_check>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_QUICK: {
        PREAMBLE();
        bool success = DoInvokeVirtualQuick<false>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_VIRTUAL_RANGE_QUICK: {
        PREAMBLE();
        bool success = DoInvokeVirtualQuick<true>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_POLYMORPHIC: {
        PREAMBLE();
        DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
        bool success = DoInvokePolymorphic<false /* is_range */>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_4xx);
        break;
      }
      case Instruction::INVOKE_POLYMORPHIC_RANGE: {
        PREAMBLE();
        DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
        bool success = DoInvokePolymorphic<true /* is_range */>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_4xx);
        break;
      }
      case Instruction::INVOKE_CUSTOM: {
        PREAMBLE();
        DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
        bool success = DoInvokeCustom<false /* is_range */>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::INVOKE_CUSTOM_RANGE: {
        PREAMBLE();
        DCHECK(Runtime::Current()->IsMethodHandlesEnabled());
        bool success = DoInvokeCustom<true /* is_range */>(
            self, shadow_frame, inst, inst_data, &result_register);
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_3xx);
        break;
      }
      case Instruction::NEG_INT:
        PREAMBLE();
        shadow_frame.SetVReg(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NOT_INT:
        PREAMBLE();
        shadow_frame.SetVReg(
            inst->VRegA_12x(inst_data), ~shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NOT_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(
            inst->VRegA_12x(inst_data), ~shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::NEG_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(
            inst->VRegA_12x(inst_data), -shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data),
                                 shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data),
                             shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::LONG_TO_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::FLOAT_TO_INT: {
        PREAMBLE();
        float val = shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data));
        int32_t result = art_float_to_integral<int32_t, float>(val);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::FLOAT_TO_LONG: {
        PREAMBLE();
        float val = shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data));
        int64_t result = art_float_to_integral<int64_t, float>(val);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::FLOAT_TO_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_12x(inst_data),
                                   shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::DOUBLE_TO_INT: {
        PREAMBLE();
        double val = shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data));
        int32_t result = art_float_to_integral<int32_t, double>(val);
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DOUBLE_TO_LONG: {
        PREAMBLE();
        double val = shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data));
        int64_t result = art_float_to_integral<int64_t, double>(val);
        shadow_frame.SetVRegLong(inst->VRegA_12x(inst_data), result);
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DOUBLE_TO_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_12x(inst_data),
                                  shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_BYTE:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<int8_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_CHAR:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<uint16_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::INT_TO_SHORT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_12x(inst_data), static_cast<int16_t>(
            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      case Instruction::ADD_INT: {
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      }
      case Instruction::SUB_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeSub(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_23x()),
                                     shadow_frame.GetVReg(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT: {
        PREAMBLE();
        bool success = DoIntDivide(shadow_frame, inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_23x()),
                                   shadow_frame.GetVReg(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT: {
        PREAMBLE();
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_23x(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_23x()),
                                      shadow_frame.GetVReg(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::SHL_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) <<
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) >>
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             static_cast<uint32_t>(shadow_frame.GetVReg(inst->VRegB_23x())) >>
                             (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::AND_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) &
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) |
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_23x(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_23x()) ^
                             shadow_frame.GetVReg(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeAdd(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeSub(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 SafeMul(shadow_frame.GetVRegLong(inst->VRegB_23x()),
                                         shadow_frame.GetVRegLong(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_LONG:
        PREAMBLE();
        DoLongDivide(shadow_frame, inst->VRegA_23x(inst_data),
                     shadow_frame.GetVRegLong(inst->VRegB_23x()),
                     shadow_frame.GetVRegLong(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_2xx);
        break;
      case Instruction::REM_LONG:
        PREAMBLE();
        DoLongRemainder(shadow_frame, inst->VRegA_23x(inst_data),
                        shadow_frame.GetVRegLong(inst->VRegB_23x()),
                        shadow_frame.GetVRegLong(inst->VRegC_23x()));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_2xx);
        break;
      case Instruction::AND_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) &
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) |
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) ^
                                 shadow_frame.GetVRegLong(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHL_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) <<
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 shadow_frame.GetVRegLong(inst->VRegB_23x()) >>
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_LONG:
        PREAMBLE();
        shadow_frame.SetVRegLong(inst->VRegA_23x(inst_data),
                                 static_cast<uint64_t>(shadow_frame.GetVRegLong(inst->VRegB_23x())) >>
                                 (shadow_frame.GetVReg(inst->VRegC_23x()) & 0x3f));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) +
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) -
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) *
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  shadow_frame.GetVRegFloat(inst->VRegB_23x()) /
                                  shadow_frame.GetVRegFloat(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::REM_FLOAT:
        PREAMBLE();
        shadow_frame.SetVRegFloat(inst->VRegA_23x(inst_data),
                                  fmodf(shadow_frame.GetVRegFloat(inst->VRegB_23x()),
                                        shadow_frame.GetVRegFloat(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) +
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::SUB_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) -
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) *
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   shadow_frame.GetVRegDouble(inst->VRegB_23x()) /
                                   shadow_frame.GetVRegDouble(inst->VRegC_23x()));
        inst = inst->Next_2xx();
        break;
      case Instruction::REM_DOUBLE:
        PREAMBLE();
        shadow_frame.SetVRegDouble(inst->VRegA_23x(inst_data),
                                   fmod(shadow_frame.GetVRegDouble(inst->VRegB_23x()),
                                        shadow_frame.GetVRegDouble(inst->VRegC_23x())));
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA, SafeAdd(shadow_frame.GetVReg(vregA),
                                            shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             SafeSub(shadow_frame.GetVReg(vregA),
                                     shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             SafeMul(shadow_frame.GetVReg(vregA),
                                     shadow_frame.GetVReg(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        bool success = DoIntDivide(shadow_frame, vregA, shadow_frame.GetVReg(vregA),
                                   shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_1xx);
        break;
      }
      case Instruction::REM_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        bool success = DoIntRemainder(shadow_frame, vregA, shadow_frame.GetVReg(vregA),
                                      shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_1xx);
        break;
      }
      case Instruction::SHL_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) <<
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHR_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) >>
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::USHR_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             static_cast<uint32_t>(shadow_frame.GetVReg(vregA)) >>
                             (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x1f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::AND_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) &
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::OR_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) |
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::XOR_INT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVReg(vregA,
                             shadow_frame.GetVReg(vregA) ^
                             shadow_frame.GetVReg(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeAdd(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeSub(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 SafeMul(shadow_frame.GetVRegLong(vregA),
                                         shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        DoLongDivide(shadow_frame, vregA, shadow_frame.GetVRegLong(vregA),
                    shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        break;
      }
      case Instruction::REM_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        DoLongRemainder(shadow_frame, vregA, shadow_frame.GetVRegLong(vregA),
                        shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        POSSIBLY_HANDLE_PENDING_EXCEPTION(self->IsExceptionPending(), Next_1xx);
        break;
      }
      case Instruction::AND_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) &
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::OR_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) |
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::XOR_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) ^
                                 shadow_frame.GetVRegLong(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHL_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) <<
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SHR_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 shadow_frame.GetVRegLong(vregA) >>
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::USHR_LONG_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegLong(vregA,
                                 static_cast<uint64_t>(shadow_frame.GetVRegLong(vregA)) >>
                                 (shadow_frame.GetVReg(inst->VRegB_12x(inst_data)) & 0x3f));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_FLOAT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) +
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_FLOAT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) -
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_FLOAT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) *
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_FLOAT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  shadow_frame.GetVRegFloat(vregA) /
                                  shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::REM_FLOAT_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegFloat(vregA,
                                  fmodf(shadow_frame.GetVRegFloat(vregA),
                                        shadow_frame.GetVRegFloat(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_DOUBLE_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) +
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::SUB_DOUBLE_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) -
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::MUL_DOUBLE_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) *
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::DIV_DOUBLE_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   shadow_frame.GetVRegDouble(vregA) /
                                   shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data)));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::REM_DOUBLE_2ADDR: {
        PREAMBLE();
        uint4_t vregA = inst->VRegA_12x(inst_data);
        shadow_frame.SetVRegDouble(vregA,
                                   fmod(shadow_frame.GetVRegDouble(vregA),
                                        shadow_frame.GetVRegDouble(inst->VRegB_12x(inst_data))));
        inst = inst->Next_1xx();
        break;
      }
      case Instruction::ADD_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                     inst->VRegC_22s()));
        inst = inst->Next_2xx();
        break;
      case Instruction::RSUB_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeSub(inst->VRegC_22s(),
                                     shadow_frame.GetVReg(inst->VRegB_22s(inst_data))));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                     inst->VRegC_22s()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT_LIT16: {
        PREAMBLE();
        bool success = DoIntDivide(shadow_frame, inst->VRegA_22s(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                   inst->VRegC_22s());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT_LIT16: {
        PREAMBLE();
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_22s(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_22s(inst_data)),
                                      inst->VRegC_22s());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::AND_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) &
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) |
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT_LIT16:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22s(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22s(inst_data)) ^
                             inst->VRegC_22s());
        inst = inst->Next_2xx();
        break;
      case Instruction::ADD_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeAdd(shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b()));
        inst = inst->Next_2xx();
        break;
      case Instruction::RSUB_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeSub(inst->VRegC_22b(), shadow_frame.GetVReg(inst->VRegB_22b())));
        inst = inst->Next_2xx();
        break;
      case Instruction::MUL_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             SafeMul(shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b()));
        inst = inst->Next_2xx();
        break;
      case Instruction::DIV_INT_LIT8: {
        PREAMBLE();
        bool success = DoIntDivide(shadow_frame, inst->VRegA_22b(inst_data),
                                   shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::REM_INT_LIT8: {
        PREAMBLE();
        bool success = DoIntRemainder(shadow_frame, inst->VRegA_22b(inst_data),
                                      shadow_frame.GetVReg(inst->VRegB_22b()), inst->VRegC_22b());
        POSSIBLY_HANDLE_PENDING_EXCEPTION(!success, Next_2xx);
        break;
      }
      case Instruction::AND_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) &
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::OR_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) |
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::XOR_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) ^
                             inst->VRegC_22b());
        inst = inst->Next_2xx();
        break;
      case Instruction::SHL_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) <<
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::SHR_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             shadow_frame.GetVReg(inst->VRegB_22b()) >>
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::USHR_INT_LIT8:
        PREAMBLE();
        shadow_frame.SetVReg(inst->VRegA_22b(inst_data),
                             static_cast<uint32_t>(shadow_frame.GetVReg(inst->VRegB_22b())) >>
                             (inst->VRegC_22b() & 0x1f));
        inst = inst->Next_2xx();
        break;
      case Instruction::UNUSED_3E ... Instruction::UNUSED_43:
      case Instruction::UNUSED_79 ... Instruction::UNUSED_7A:
      case Instruction::UNUSED_F3 ... Instruction::UNUSED_F9:
        UnexpectedOpcode(inst, shadow_frame);
    }
  } while (!interpret_one_instruction);
  // Record where we stopped.
  shadow_frame.SetDexPC(inst->GetDexPc(insns));
  ctx->result = result_register;
  return;
}  // NOLINT(readability/fn_size)

// Explicit definitions of ExecuteSwitchImplCpp.
template HOT_ATTR
void ExecuteSwitchImplCpp<true, false>(SwitchImplContext* ctx);
template HOT_ATTR
void ExecuteSwitchImplCpp<false, false>(SwitchImplContext* ctx);
template
void ExecuteSwitchImplCpp<true, true>(SwitchImplContext* ctx);
template
void ExecuteSwitchImplCpp<false, true>(SwitchImplContext* ctx);

}  // namespace interpreter
}  // namespace art
