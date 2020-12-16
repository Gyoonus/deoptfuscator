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
namespace Test987AgentBind {

static void doUpPrintCall(JNIEnv* env, const char* function) {
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Test987"));
  jmethodID targetMethod = env->GetStaticMethodID(klass.get(), function, "()V");
  env->CallStaticVoidMethod(klass.get(), targetMethod);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test987_00024Transform_sayHi__(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  doUpPrintCall(env, "doSayHi");
}

extern "C" JNIEXPORT void JNICALL Java_art_Test987_00024Transform_sayHi2(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  doUpPrintCall(env, "doSayHi2");
}

}  // namespace Test987AgentBind
}  // namespace art
