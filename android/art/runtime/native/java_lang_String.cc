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

#include "java_lang_String.h"

#include "nativehelper/jni_macros.h"

#include "common_throws.h"
#include "jni_internal.h"
#include "mirror/array.h"
#include "mirror/object-inl.h"
#include "mirror/string-inl.h"
#include "mirror/string.h"
#include "native_util.h"
#include "nativehelper/scoped_local_ref.h"
#include "scoped_fast_native_object_access-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "verify_object.h"

namespace art {

static jchar String_charAt(JNIEnv* env, jobject java_this, jint index) {
  ScopedFastNativeObjectAccess soa(env);
  return soa.Decode<mirror::String>(java_this)->CharAt(index);
}

static jint String_compareTo(JNIEnv* env, jobject java_this, jstring java_rhs) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(java_rhs == nullptr)) {
    ThrowNullPointerException("rhs == null");
    return -1;
  } else {
    return soa.Decode<mirror::String>(java_this)->CompareTo(
        soa.Decode<mirror::String>(java_rhs).Ptr());
  }
}

static jstring String_concat(JNIEnv* env, jobject java_this, jstring java_string_arg) {
  ScopedFastNativeObjectAccess soa(env);
  if (UNLIKELY(java_string_arg == nullptr)) {
    ThrowNullPointerException("string arg == null");
    return nullptr;
  }
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::String> string_this(hs.NewHandle(soa.Decode<mirror::String>(java_this)));
  Handle<mirror::String> string_arg(hs.NewHandle(soa.Decode<mirror::String>(java_string_arg)));
  int32_t length_this = string_this->GetLength();
  int32_t length_arg = string_arg->GetLength();
  if (length_arg > 0 && length_this > 0) {
    ObjPtr<mirror::String> result =
        mirror::String::AllocFromStrings(soa.Self(), string_this, string_arg);
    return soa.AddLocalReference<jstring>(result);
  }
  jobject string_original = (length_this == 0) ? java_string_arg : java_this;
  return reinterpret_cast<jstring>(string_original);
}

static jstring String_fastSubstring(JNIEnv* env, jobject java_this, jint start, jint length) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::String> string_this(hs.NewHandle(soa.Decode<mirror::String>(java_this)));
  gc::AllocatorType allocator_type = Runtime::Current()->GetHeap()->GetCurrentAllocator();
  ObjPtr<mirror::String> result = mirror::String::AllocFromString<true>(soa.Self(),
                                                                        length,
                                                                        string_this,
                                                                        start,
                                                                        allocator_type);
  return soa.AddLocalReference<jstring>(result);
}

static void String_getCharsNoCheck(JNIEnv* env, jobject java_this, jint start, jint end,
                                   jcharArray buffer, jint index) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::CharArray> char_array(hs.NewHandle(soa.Decode<mirror::CharArray>(buffer)));
  soa.Decode<mirror::String>(java_this)->GetChars(start, end, char_array, index);
}

static jstring String_intern(JNIEnv* env, jobject java_this) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::String> result = soa.Decode<mirror::String>(java_this)->Intern();
  return soa.AddLocalReference<jstring>(result);
}

static jstring String_doReplace(JNIEnv* env, jobject java_this, jchar old_c, jchar new_c) {
  ScopedFastNativeObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::String> string = hs.NewHandle(soa.Decode<mirror::String>(java_this));
  ObjPtr<mirror::String> result = mirror::String::DoReplace(soa.Self(), string, old_c, new_c);
  return soa.AddLocalReference<jstring>(result);
}

static jcharArray String_toCharArray(JNIEnv* env, jobject java_this) {
  ScopedFastNativeObjectAccess soa(env);
  ObjPtr<mirror::String> s = soa.Decode<mirror::String>(java_this);
  return soa.AddLocalReference<jcharArray>(s->ToCharArray(soa.Self()));
}

static JNINativeMethod gMethods[] = {
  FAST_NATIVE_METHOD(String, charAt, "(I)C"),
  FAST_NATIVE_METHOD(String, compareTo, "(Ljava/lang/String;)I"),
  FAST_NATIVE_METHOD(String, concat, "(Ljava/lang/String;)Ljava/lang/String;"),
  FAST_NATIVE_METHOD(String, doReplace, "(CC)Ljava/lang/String;"),
  FAST_NATIVE_METHOD(String, fastSubstring, "(II)Ljava/lang/String;"),
  FAST_NATIVE_METHOD(String, getCharsNoCheck, "(II[CI)V"),
  FAST_NATIVE_METHOD(String, intern, "()Ljava/lang/String;"),
  FAST_NATIVE_METHOD(String, toCharArray, "()[C"),
};

void register_java_lang_String(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/String");
}

}  // namespace art
