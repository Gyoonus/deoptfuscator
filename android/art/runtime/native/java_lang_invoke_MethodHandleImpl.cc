/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "java_lang_invoke_MethodHandleImpl.h"

#include "nativehelper/jni_macros.h"

#include "art_method.h"
#include "handle_scope-inl.h"
#include "jni_internal.h"
#include "mirror/field.h"
#include "mirror/method.h"
#include "mirror/method_handle_impl.h"
#include "native_util.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

static jobject MethodHandleImpl_getMemberInternal(JNIEnv* env, jobject thiz) {
  ScopedObjectAccess soa(env);
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::MethodHandleImpl> handle = hs.NewHandle(
      soa.Decode<mirror::MethodHandleImpl>(thiz));

  // Check the handle kind, we need to materialize a Field for field accessors,
  // a Method for method invokers and a Constructor for constructors.
  const mirror::MethodHandle::Kind handle_kind = handle->GetHandleKind();

  // We check this here because we pass false to CreateFromArtField and
  // CreateFromArtMethod.
  DCHECK(!Runtime::Current()->IsActiveTransaction());

  MutableHandle<mirror::Object> h_object(hs.NewHandle<mirror::Object>(nullptr));
  if (handle_kind >= mirror::MethodHandle::kFirstAccessorKind) {
    ArtField* const field = handle->GetTargetField();
    h_object.Assign(mirror::Field::CreateFromArtField<kRuntimePointerSize, false>(
        soa.Self(), field, false /* force_resolve */));
  } else {
    ArtMethod* const method = handle->GetTargetMethod();
    if (method->IsConstructor()) {
      h_object.Assign(mirror::Constructor::CreateFromArtMethod<kRuntimePointerSize, false>(
          soa.Self(), method));
    } else {
      h_object.Assign(mirror::Method::CreateFromArtMethod<kRuntimePointerSize, false>(
          soa.Self(), method));
    }
  }

  if (UNLIKELY(h_object == nullptr)) {
    soa.Self()->AssertPendingOOMException();
    return nullptr;
  }

  return soa.AddLocalReference<jobject>(h_object.Get());
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(MethodHandleImpl, getMemberInternal, "()Ljava/lang/reflect/Member;"),
};

void register_java_lang_invoke_MethodHandleImpl(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/invoke/MethodHandleImpl");
}

}  // namespace art
