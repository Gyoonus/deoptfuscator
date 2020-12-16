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

#include <atomic>

#include "android-base/logging.h"
#include "jni.h"
#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1943SuspendRawMonitorExit {

jrawMonitorID mon;
std::atomic<bool> locked(false);

extern "C" JNIEXPORT void JNICALL Java_art_Test1943_nativeRun(JNIEnv* env, jclass) {
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->CreateRawMonitor("Test1943 monitor", &mon))) {
    return;
  }
  // Grab the monitor twice
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon))) {
    return;
  }
  locked.store(true);
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorWait(mon, 0))) {
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon))) {
    return;
  }
  locked.store(false);
}


extern "C" JNIEXPORT void JNICALL Java_art_Test1943_waitForPause(JNIEnv*, jclass) {
  while (!locked.load()) { }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1943_nativeNotify(JNIEnv* env, jclass) {
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon))) {
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorNotifyAll(mon))) {
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon))) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1943_grabRawMonitor(JNIEnv* env, jclass) {
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(mon))) {
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(mon))) {
    return;
  }
}

}  // namespace Test1943SuspendRawMonitorExit
}  // namespace art
