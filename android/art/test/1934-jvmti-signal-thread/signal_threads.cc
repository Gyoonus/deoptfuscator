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
namespace Test1934SignalThreads {

struct NativeMonitor {
  jrawMonitorID continue_monitor;
  bool should_continue;
  jrawMonitorID start_monitor;
  bool should_start;
};

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1934_allocNativeMonitor(JNIEnv* env, jclass) {
  NativeMonitor* mon;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(NativeMonitor),
                                                reinterpret_cast<unsigned char**>(&mon)))) {
    return -1l;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->CreateRawMonitor("test-1934 start",
                                                        &mon->start_monitor))) {
    return -1l;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->CreateRawMonitor("test-1934 continue",
                                                        &mon->continue_monitor))) {
    return -1l;
  }
  mon->should_continue = false;
  mon->should_start = false;
  return static_cast<jlong>(reinterpret_cast<intptr_t>(mon));
}

extern "C" JNIEXPORT void Java_art_Test1934_nativeWaitForOtherThread(JNIEnv* env,
                                                                     jclass,
                                                                     jlong id) {
  NativeMonitor* mon = reinterpret_cast<NativeMonitor*>(static_cast<intptr_t>(id));
  // Start
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon->start_monitor))) {
    return;
  }
  mon->should_start = true;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->RawMonitorNotifyAll(mon->start_monitor))) {
    JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->start_monitor));
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->start_monitor))) {
    return;
  }

  // Finish
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon->continue_monitor))) {
    return;
  }
  while (!mon->should_continue) {
    if (JvmtiErrorToException(env,
                              jvmti_env,
                              jvmti_env->RawMonitorWait(mon->continue_monitor, -1l))) {
      JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->continue_monitor));
      return;
    }
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->continue_monitor));
}

extern "C" JNIEXPORT void Java_art_Test1934_nativeDoInterleaved(JNIEnv* env,
                                                                jclass,
                                                                jlong id,
                                                                jobject closure) {
  NativeMonitor* mon = reinterpret_cast<NativeMonitor*>(static_cast<intptr_t>(id));
  // Wait for start.
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon->start_monitor))) {
    return;
  }
  while (!mon->should_start) {
    if (JvmtiErrorToException(env,
                              jvmti_env,
                              jvmti_env->RawMonitorWait(mon->start_monitor, -1l))) {
      return;
    }
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->start_monitor))) {
    return;
  }

  // Call closure.
  ScopedLocalRef<jclass> runnable_klass(env, env->FindClass("java/lang/Runnable"));
  if (env->ExceptionCheck()) {
    return;
  }
  jmethodID doRun = env->GetMethodID(runnable_klass.get(), "run", "()V");
  if (env->ExceptionCheck()) {
    return;
  }
  env->CallVoidMethod(closure, doRun);

  // Tell other thread to finish.
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon->continue_monitor))) {
    return;
  }
  mon->should_continue = true;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->RawMonitorNotifyAll(mon->continue_monitor))) {
    JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->continue_monitor));
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon->continue_monitor));
}

extern "C" JNIEXPORT void Java_art_Test1934_destroyNativeMonitor(JNIEnv*, jclass, jlong id) {
  NativeMonitor* mon = reinterpret_cast<NativeMonitor*>(static_cast<intptr_t>(id));
  jvmti_env->DestroyRawMonitor(mon->start_monitor);
  jvmti_env->DestroyRawMonitor(mon->continue_monitor);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(mon));
}

}  // namespace Test1934SignalThreads
}  // namespace art

