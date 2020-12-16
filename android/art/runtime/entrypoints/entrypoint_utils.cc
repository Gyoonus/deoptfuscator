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

#include "entrypoints/entrypoint_utils.h"

#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/mutex.h"
#include "class_linker-inl.h"
#include "dex/dex_file-inl.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "entrypoints/quick/callee_save_frame.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc/accounting/card_table-inl.h"
#include "java_vm_ext.h"
#include "mirror/class-inl.h"
#include "mirror/method.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "nth_caller_visitor.h"
#include "oat_quick_method_header.h"
#include "reflection.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

namespace art {

void CheckReferenceResult(Handle<mirror::Object> o, Thread* self) {
  if (o == nullptr) {
    return;
  }
  // Make sure that the result is an instance of the type this method was expected to return.
  ArtMethod* method = self->GetCurrentMethod(nullptr);
  ObjPtr<mirror::Class> return_type = method->ResolveReturnType();

  if (!o->InstanceOf(return_type)) {
    Runtime::Current()->GetJavaVM()->JniAbortF(nullptr,
                                               "attempt to return an instance of %s from %s",
                                               o->PrettyTypeOf().c_str(),
                                               method->PrettyMethod().c_str());
  }
}

JValue InvokeProxyInvocationHandler(ScopedObjectAccessAlreadyRunnable& soa, const char* shorty,
                                    jobject rcvr_jobj, jobject interface_method_jobj,
                                    std::vector<jvalue>& args) {
  DCHECK(soa.Env()->IsInstanceOf(rcvr_jobj, WellKnownClasses::java_lang_reflect_Proxy));

  // Build argument array possibly triggering GC.
  soa.Self()->AssertThreadSuspensionIsAllowable();
  jobjectArray args_jobj = nullptr;
  const JValue zero;
  int32_t target_sdk_version = Runtime::Current()->GetTargetSdkVersion();
  // Do not create empty arrays unless needed to maintain Dalvik bug compatibility.
  if (args.size() > 0 || (target_sdk_version > 0 && target_sdk_version <= 21)) {
    args_jobj = soa.Env()->NewObjectArray(args.size(), WellKnownClasses::java_lang_Object, nullptr);
    if (args_jobj == nullptr) {
      CHECK(soa.Self()->IsExceptionPending());
      return zero;
    }
    for (size_t i = 0; i < args.size(); ++i) {
      if (shorty[i + 1] == 'L') {
        jobject val = args.at(i).l;
        soa.Env()->SetObjectArrayElement(args_jobj, i, val);
      } else {
        JValue jv;
        jv.SetJ(args.at(i).j);
        mirror::Object* val = BoxPrimitive(Primitive::GetType(shorty[i + 1]), jv).Ptr();
        if (val == nullptr) {
          CHECK(soa.Self()->IsExceptionPending());
          return zero;
        }
        soa.Decode<mirror::ObjectArray<mirror::Object>>(args_jobj)->Set<false>(i, val);
      }
    }
  }

  // Call Proxy.invoke(Proxy proxy, Method method, Object[] args).
  jvalue invocation_args[3];
  invocation_args[0].l = rcvr_jobj;
  invocation_args[1].l = interface_method_jobj;
  invocation_args[2].l = args_jobj;
  jobject result =
      soa.Env()->CallStaticObjectMethodA(WellKnownClasses::java_lang_reflect_Proxy,
                                         WellKnownClasses::java_lang_reflect_Proxy_invoke,
                                         invocation_args);

  // Unbox result and handle error conditions.
  if (LIKELY(!soa.Self()->IsExceptionPending())) {
    if (shorty[0] == 'V' || (shorty[0] == 'L' && result == nullptr)) {
      // Do nothing.
      return zero;
    } else {
      ArtMethod* interface_method =
          soa.Decode<mirror::Method>(interface_method_jobj)->GetArtMethod();
      // This can cause thread suspension.
      ObjPtr<mirror::Class> result_type = interface_method->ResolveReturnType();
      ObjPtr<mirror::Object> result_ref = soa.Decode<mirror::Object>(result);
      JValue result_unboxed;
      if (!UnboxPrimitiveForResult(result_ref.Ptr(), result_type, &result_unboxed)) {
        DCHECK(soa.Self()->IsExceptionPending());
        return zero;
      }
      return result_unboxed;
    }
  } else {
    // In the case of checked exceptions that aren't declared, the exception must be wrapped by
    // a UndeclaredThrowableException.
    mirror::Throwable* exception = soa.Self()->GetException();
    if (exception->IsCheckedException()) {
      bool declares_exception = false;
      {
        ScopedAssertNoThreadSuspension ants(__FUNCTION__);
        ObjPtr<mirror::Object> rcvr = soa.Decode<mirror::Object>(rcvr_jobj);
        mirror::Class* proxy_class = rcvr->GetClass();
        ObjPtr<mirror::Method> interface_method = soa.Decode<mirror::Method>(interface_method_jobj);
        ArtMethod* proxy_method = rcvr->GetClass()->FindVirtualMethodForInterface(
            interface_method->GetArtMethod(), kRuntimePointerSize);
        auto virtual_methods = proxy_class->GetVirtualMethodsSlice(kRuntimePointerSize);
        size_t num_virtuals = proxy_class->NumVirtualMethods();
        size_t method_size = ArtMethod::Size(kRuntimePointerSize);
        // Rely on the fact that the methods are contiguous to determine the index of the method in
        // the slice.
        int throws_index = (reinterpret_cast<uintptr_t>(proxy_method) -
            reinterpret_cast<uintptr_t>(&virtual_methods[0])) / method_size;
        CHECK_LT(throws_index, static_cast<int>(num_virtuals));
        mirror::ObjectArray<mirror::Class>* declared_exceptions =
            proxy_class->GetProxyThrows()->Get(throws_index);
        mirror::Class* exception_class = exception->GetClass();
        for (int32_t i = 0; i < declared_exceptions->GetLength() && !declares_exception; i++) {
          mirror::Class* declared_exception = declared_exceptions->Get(i);
          declares_exception = declared_exception->IsAssignableFrom(exception_class);
        }
      }
      if (!declares_exception) {
        soa.Self()->ThrowNewWrappedException("Ljava/lang/reflect/UndeclaredThrowableException;",
                                             nullptr);
      }
    }
    return zero;
  }
}

bool FillArrayData(ObjPtr<mirror::Object> obj, const Instruction::ArrayDataPayload* payload) {
  DCHECK_EQ(payload->ident, static_cast<uint16_t>(Instruction::kArrayDataSignature));
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerException("null array in FILL_ARRAY_DATA");
    return false;
  }
  mirror::Array* array = obj->AsArray();
  DCHECK(!array->IsObjectArray());
  if (UNLIKELY(static_cast<int32_t>(payload->element_count) > array->GetLength())) {
    Thread* self = Thread::Current();
    self->ThrowNewExceptionF("Ljava/lang/ArrayIndexOutOfBoundsException;",
                             "failed FILL_ARRAY_DATA; length=%d, index=%d",
                             array->GetLength(), payload->element_count);
    return false;
  }
  // Copy data from dex file to memory assuming both are little endian.
  uint32_t size_in_bytes = payload->element_count * payload->element_width;
  memcpy(array->GetRawData(payload->element_width, 0), payload->data, size_in_bytes);
  return true;
}

static inline std::pair<ArtMethod*, uintptr_t> DoGetCalleeSaveMethodOuterCallerAndPc(
    ArtMethod** sp, CalleeSaveType type) REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(*sp, Runtime::Current()->GetCalleeSaveMethod(type));

  const size_t callee_frame_size = GetCalleeSaveFrameSize(kRuntimeISA, type);
  auto** caller_sp = reinterpret_cast<ArtMethod**>(
      reinterpret_cast<uintptr_t>(sp) + callee_frame_size);
  const size_t callee_return_pc_offset = GetCalleeSaveReturnPcOffset(kRuntimeISA, type);
  uintptr_t caller_pc = *reinterpret_cast<uintptr_t*>(
      (reinterpret_cast<uint8_t*>(sp) + callee_return_pc_offset));
  ArtMethod* outer_method = *caller_sp;
  return std::make_pair(outer_method, caller_pc);
}

static inline ArtMethod* DoGetCalleeSaveMethodCaller(ArtMethod* outer_method,
                                                     uintptr_t caller_pc,
                                                     bool do_caller_check)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* caller = outer_method;
  if (LIKELY(caller_pc != reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc()))) {
    if (outer_method != nullptr) {
      const OatQuickMethodHeader* current_code = outer_method->GetOatQuickMethodHeader(caller_pc);
      DCHECK(current_code != nullptr);
      DCHECK(current_code->IsOptimized());
      uintptr_t native_pc_offset = current_code->NativeQuickPcOffset(caller_pc);
      CodeInfo code_info = current_code->GetOptimizedCodeInfo();
      MethodInfo method_info = current_code->GetOptimizedMethodInfo();
      CodeInfoEncoding encoding = code_info.ExtractEncoding();
      StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset, encoding);
      DCHECK(stack_map.IsValid());
      if (stack_map.HasInlineInfo(encoding.stack_map.encoding)) {
        InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map, encoding);
        caller = GetResolvedMethod(outer_method,
                                   method_info,
                                   inline_info,
                                   encoding.inline_info.encoding,
                                   inline_info.GetDepth(encoding.inline_info.encoding) - 1);
      }
    }
    if (kIsDebugBuild && do_caller_check) {
      // Note that do_caller_check is optional, as this method can be called by
      // stubs, and tests without a proper call stack.
      NthCallerVisitor visitor(Thread::Current(), 1, true);
      visitor.WalkStack();
      CHECK_EQ(caller, visitor.caller);
    }
  } else {
    // We're instrumenting, just use the StackVisitor which knows how to
    // handle instrumented frames.
    NthCallerVisitor visitor(Thread::Current(), 1, true);
    visitor.WalkStack();
    caller = visitor.caller;
  }
  return caller;
}

ArtMethod* GetCalleeSaveMethodCaller(ArtMethod** sp, CalleeSaveType type, bool do_caller_check)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  auto outer_caller_and_pc = DoGetCalleeSaveMethodOuterCallerAndPc(sp, type);
  ArtMethod* outer_method = outer_caller_and_pc.first;
  uintptr_t caller_pc = outer_caller_and_pc.second;
  ArtMethod* caller = DoGetCalleeSaveMethodCaller(outer_method, caller_pc, do_caller_check);
  return caller;
}

CallerAndOuterMethod GetCalleeSaveMethodCallerAndOuterMethod(Thread* self, CalleeSaveType type) {
  CallerAndOuterMethod result;
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ArtMethod** sp = self->GetManagedStack()->GetTopQuickFrameKnownNotTagged();
  auto outer_caller_and_pc = DoGetCalleeSaveMethodOuterCallerAndPc(sp, type);
  result.outer_method = outer_caller_and_pc.first;
  uintptr_t caller_pc = outer_caller_and_pc.second;
  result.caller =
      DoGetCalleeSaveMethodCaller(result.outer_method, caller_pc, /* do_caller_check */ true);
  return result;
}

ArtMethod* GetCalleeSaveOuterMethod(Thread* self, CalleeSaveType type) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ArtMethod** sp = self->GetManagedStack()->GetTopQuickFrameKnownNotTagged();
  return DoGetCalleeSaveMethodOuterCallerAndPc(sp, type).first;
}

}  // namespace art
