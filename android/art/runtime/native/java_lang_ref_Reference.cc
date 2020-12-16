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

#include "java_lang_ref_Reference.h"

#include "nativehelper/jni_macros.h"

#include "gc/heap.h"
#include "gc/reference_processor.h"
#include "jni_internal.h"
#include "mirror/object-inl.h"
#include "mirror/reference-inl.h"
#include "native_util.h"
#include "scoped_fast_native_object_access-inl.h"

namespace art {

static jobject Reference_getReferent(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::Reference> ref = soa.Decode<mirror::Reference>(javaThis);
  ObjPtr<mirror::Object> const referent =
      Runtime::Current()->GetHeap()->GetReferenceProcessor()->GetReferent(soa.Self(), ref);
  return soa.AddLocalReference<jobject>(referent);
}

static void Reference_clearReferent(JNIEnv* env, jobject javaThis) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::Reference> ref = soa.Decode<mirror::Reference>(javaThis);
  Runtime::Current()->GetHeap()->GetReferenceProcessor()->ClearReferent(ref);
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(Reference, getReferent, "()Ljava/lang/Object;"),
  FAST_NATIVE_METHOD(Reference, clearReferent, "()V"),
};

void register_java_lang_ref_Reference(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/ref/Reference");
}

}  // namespace art
