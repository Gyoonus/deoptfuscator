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

#include <signal.h>
#include <sys/types.h>

#include <atomic>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test933MiscEvents {

static std::atomic<bool> saw_dump_request(false);

static void DumpRequestCallback(jvmtiEnv* jenv ATTRIBUTE_UNUSED) {
  printf("Received dump request.\n");
  saw_dump_request.store(true, std::memory_order::memory_order_relaxed);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test933_testSigQuit(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.DataDumpRequest = DumpRequestCallback;
  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }

  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_DATA_DUMP_REQUEST,
                                            nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }

  // Send sigquit to self.
  kill(getpid(), SIGQUIT);

  // Busy-wait for request.
  for (;;) {
    sleep(1);
    if (saw_dump_request.load(std::memory_order::memory_order_relaxed)) {
      break;
    }
  }

  ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_DATA_DUMP_REQUEST, nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
}

}  // namespace Test933MiscEvents
}  // namespace art
