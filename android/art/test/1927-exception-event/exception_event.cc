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

#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "android-base/logging.h"
#include "jni.h"
#include "jvmti.h"

#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1927ExceptionEvent {

static void ThrowNative(JNIEnv* env) {
  ScopedLocalRef<jclass> exception(env, env->FindClass("art/Test1927$TestException"));
  env->ThrowNew(exception.get(), "from native");
}

static void CallMethod(JNIEnv* env, jclass test, const char* name) {
  jmethodID m = env->GetStaticMethodID(test, name, "()V");
  env->CallStaticVoidMethod(test, m);
}

static void ClearAndPrintException(JNIEnv* env, jclass test) {
  jthrowable e = env->ExceptionOccurred();
  env->ExceptionClear();
  jmethodID m = env->GetStaticMethodID(test, "printException", "(Ljava/lang/Throwable;)V");
  env->CallStaticVoidMethod(test, m, e);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_terminal_1N(JNIEnv* env, jclass) {
  ThrowNative(env);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N(JNIEnv* env, jclass test) {
  ThrowNative(env);
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1J(JNIEnv* env, jclass test) {
  CallMethod(env, test, "terminal_J");
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1N(JNIEnv* env, jclass test) {
  CallMethod(env, test, "terminal_N");
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_intermediate_1N_1J(JNIEnv* env, jclass test) {
  CallMethod(env, test, "terminal_J");
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_intermediate_1N_1N(JNIEnv* env, jclass test) {
  CallMethod(env, test, "terminal_N");
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1J_1J(JNIEnv* env, jclass test) {
  CallMethod(env, test, "intermediate_J_J");
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1J_1N(JNIEnv* env, jclass test) {
  CallMethod(env, test, "intermediate_J_N");
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1N_1J(JNIEnv* env, jclass test) {
  CallMethod(env, test, "intermediate_N_J");
  ClearAndPrintException(env, test);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1927_test_1N_1N_1N(JNIEnv* env, jclass test) {
  CallMethod(env, test, "intermediate_N_N");
  ClearAndPrintException(env, test);
}

}  // namespace Test1927ExceptionEvent
}  // namespace art
