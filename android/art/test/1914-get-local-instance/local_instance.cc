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

#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <vector>

#include "android-base/logging.h"
#include "jni.h"
#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1914LocalInstance {

extern "C" JNIEXPORT void Java_art_Test1914_00024TargetClass_NativeInstanceMethod(
    JNIEnv* env, jobject thiz, jobject run) {
  ScopedLocalRef<jclass> runnable(env, env->FindClass("java/lang/Runnable"));
  if (env->ExceptionCheck()) { return; }
  jmethodID method = env->GetMethodID(runnable.get(), "run", "()V");
  if (env->ExceptionCheck()) { return; }
  env->CallVoidMethod(run, method);
  if (env->ExceptionCheck()) { return; }
  ScopedLocalRef<jclass> Test1914(env, env->FindClass("art/Test1914"));
  if (env->ExceptionCheck()) { return; }
  jmethodID report = env->GetStaticMethodID(Test1914.get(), "reportValue", "(Ljava/lang/Object;)V");
  if (env->ExceptionCheck()) { return; }
  env->CallStaticVoidMethod(Test1914.get(), report, thiz);
}

extern "C" JNIEXPORT void Java_art_Test1914_NativeStaticMethod(
    JNIEnv* env, jclass, jobject run) {
  ScopedLocalRef<jclass> runnable(env, env->FindClass("java/lang/Runnable"));
  if (env->ExceptionCheck()) { return; }
  jmethodID method = env->GetMethodID(runnable.get(), "run", "()V");
  if (env->ExceptionCheck()) { return; }
  env->CallVoidMethod(run, method);
  if (env->ExceptionCheck()) { return; }
  ScopedLocalRef<jclass> Test1914(env, env->FindClass("art/Test1914"));
  if (env->ExceptionCheck()) { return; }
  jmethodID report = env->GetStaticMethodID(Test1914.get(), "reportValue", "(Ljava/lang/Object;)V");
  if (env->ExceptionCheck()) { return; }
  env->CallStaticVoidMethod(Test1914.get(), report, nullptr);
}

}  // namespace Test1914LocalInstance
}  // namespace art

