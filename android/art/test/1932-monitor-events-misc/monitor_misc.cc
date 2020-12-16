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
namespace Test1932MonitorEventsMisc {

extern "C" JNIEXPORT void JNICALL Java_art_Test1932_doNativeLockPrint(JNIEnv* env,
                                                                      jclass klass,
                                                                      jobject lock) {
                                                                      // jobject atomic_boolean) {
  // ScopedLocalRef<jclass> atomic_klass(env, env->FindClass("java/util/concurrent/AtomicBoolean"));
  // if (env->ExceptionCheck()) {
  //   return;
  // }
  // jmethodID atomic_set = env->GetMethodID(atomic_klass.get(), "set", "(z)V");
  jmethodID print_state = env->GetStaticMethodID(
      klass, "printLockState", "(Lart/Monitors$NamedLock;Ljava/lang/Object;I)V");
  if (env->ExceptionCheck()) {
    return;
  }
  jint res = env->MonitorEnter(lock);
  ScopedLocalRef<jobject> exc(env, env->ExceptionOccurred());
  env->ExceptionClear();
  env->CallStaticVoidMethod(klass, print_state, lock, exc.get(), res);
  env->MonitorExit(lock);
}

}  // namespace Test1932MonitorEventsMisc
}  // namespace art
