/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <inttypes.h>

#include <cstdio>
#include <memory>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_binder.h"
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test989StackTraceThrow {

extern "C" JNIEXPORT
jfloat JNICALL Java_art_Test989_returnFloatNative(JNIEnv* env, jclass klass) {
  jmethodID targetMethod = env->GetStaticMethodID(klass, "doGetFloat", "()F");
  return env->CallStaticFloatMethod(klass, targetMethod);
}
extern "C" JNIEXPORT
jdouble JNICALL Java_art_Test989_returnDoubleNative(JNIEnv* env, jclass klass) {
  jmethodID targetMethod = env->GetStaticMethodID(klass, "doGetDouble", "()D");
  return env->CallStaticDoubleMethod(klass, targetMethod);
}

extern "C" JNIEXPORT jobject JNICALL Java_art_Test989_returnValueNative(JNIEnv* env, jclass klass) {
  jmethodID targetMethod = env->GetStaticMethodID(klass, "mkTestObject", "()Ljava/lang/Object;");
  return env->CallStaticObjectMethod(klass, targetMethod);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test989_doNothingNative(JNIEnv* env ATTRIBUTE_UNUSED,
                                                                   jclass klass ATTRIBUTE_UNUSED) {
  return;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test989_throwANative(JNIEnv* env,
                                                                jclass klass) {
  jmethodID targetMethod = env->GetStaticMethodID(klass, "doThrowA", "()V");
  env->CallStaticVoidMethod(klass, targetMethod);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test989_acceptValueNative(JNIEnv* env,
                                                                     jclass klass,
                                                                     jobject arg) {
  jmethodID targetMethod = env->GetStaticMethodID(klass, "printObject", "(Ljava/lang/Object;)V");
  env->CallStaticVoidMethod(klass, targetMethod, arg);
}

}  // namespace Test989StackTraceThrow
}  // namespace art

