/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <dlfcn.h>
#include <inttypes.h>

#include <cstdio>
#include <memory>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_binder.h"
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test986NativeBind {

static void doUpPrintCall(JNIEnv* env, const char* function) {
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Test986"));
  jmethodID targetMethod = env->GetStaticMethodID(klass.get(), function, "()V");
  env->CallStaticVoidMethod(klass.get(), targetMethod);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test986_00024Transform_sayHi__(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  doUpPrintCall(env, "doSayHi");
}

extern "C" JNIEXPORT void JNICALL Java_art_Test986_00024Transform_sayHi2(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  doUpPrintCall(env, "doSayHi2");
}

extern "C" JNIEXPORT void JNICALL NoReallySayGoodbye(JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  doUpPrintCall(env, "doSayBye");
}

static void doJvmtiMethodBind(jvmtiEnv* jvmtienv ATTRIBUTE_UNUSED,
                              JNIEnv* env,
                              jthread thread ATTRIBUTE_UNUSED,
                              jmethodID m,
                              void* address,
                              /*out*/void** out_address) {
  ScopedLocalRef<jclass> method_class(env, env->FindClass("java/lang/reflect/Method"));
  ScopedLocalRef<jobject> method_obj(env, env->ToReflectedMethod(method_class.get(), m, false));
  Dl_info addr_info;
  if (dladdr(address, &addr_info) == 0 || addr_info.dli_sname == nullptr) {
    ScopedLocalRef<jclass> exception_class(env, env->FindClass("java/lang/Exception"));
    env->ThrowNew(exception_class.get(), "dladdr failure!");
    return;
  }
  ScopedLocalRef<jstring> sym_name(env, env->NewStringUTF(addr_info.dli_sname));
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Test986"));
  jmethodID upcallMethod = env->GetStaticMethodID(
      klass.get(),
      "doNativeMethodBind",
      "(Ljava/lang/reflect/Method;Ljava/lang/String;)Ljava/lang/String;");
  if (env->ExceptionCheck()) {
    return;
  }
  ScopedLocalRef<jstring> new_symbol(env,
                                     reinterpret_cast<jstring>(
                                         env->CallStaticObjectMethod(klass.get(),
                                                                 upcallMethod,
                                                                 method_obj.get(),
                                                                 sym_name.get())));
  const char* new_symbol_chars = env->GetStringUTFChars(new_symbol.get(), nullptr);
  if (strcmp(new_symbol_chars, addr_info.dli_sname) != 0) {
    *out_address = dlsym(RTLD_DEFAULT, new_symbol_chars);
    if (*out_address == nullptr) {
      ScopedLocalRef<jclass> exception_class(env, env->FindClass("java/lang/Exception"));
      env->ThrowNew(exception_class.get(), "dlsym failure!");
      return;
    }
  }
  env->ReleaseStringUTFChars(new_symbol.get(), new_symbol_chars);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test986_setupNativeBindNotify(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass klass ATTRIBUTE_UNUSED) {
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.NativeMethodBind = doJvmtiMethodBind;
  jvmti_env->SetEventCallbacks(&cb, sizeof(cb));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test986_setNativeBindNotify(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jboolean enable) {
  jvmtiError res = jvmti_env->SetEventNotificationMode(enable ? JVMTI_ENABLE : JVMTI_DISABLE,
                                                       JVMTI_EVENT_NATIVE_METHOD_BIND,
                                                       nullptr);
  if (res != JVMTI_ERROR_NONE) {
    JvmtiErrorToException(env, jvmti_env, res);
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test986_rebindTransformClass(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jclass k) {
  JNINativeMethod m[2];
  m[0].name = "sayHi";
  m[0].signature = "()V";
  m[0].fnPtr = reinterpret_cast<void*>(Java_art_Test986_00024Transform_sayHi__);
  m[1].name = "sayHi2";
  m[1].signature = "()V";
  m[1].fnPtr = reinterpret_cast<void*>(Java_art_Test986_00024Transform_sayHi2);
  env->RegisterNatives(k, m, 2);
}

}  // namespace Test986NativeBind
}  // namespace art
