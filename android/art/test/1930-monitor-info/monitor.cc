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
namespace Test1930MonitorInfo {

extern "C" JNIEXPORT void JNICALL Java_art_Test1930_executeLockedNative(JNIEnv* env,
                                                                        jclass klass,
                                                                        jobject run,
                                                                        jobject l) {
  ScopedLocalRef<jclass> runnable(env, env->FindClass("java/lang/Runnable"));
  if (env->ExceptionCheck()) {
    return;
  }
  jmethodID method = env->GetMethodID(runnable.get(), "run", "()V");

  if (env->ExceptionCheck()) {
    return;
  }
  jmethodID printMethod = env->GetStaticMethodID(klass, "printPreLock", "(Ljava/lang/Object;)V");
  if (env->ExceptionCheck()) {
    return;
  }

  env->CallStaticVoidMethod(klass, printMethod, l);
  if (env->ExceptionCheck()) {
    return;
  }
  if (env->MonitorEnter(l) != 0) {
    return;
  }
  env->CallVoidMethod(run, method);
  env->MonitorExit(l);
}

}  // namespace Test1930MonitorInfo
}  // namespace art
