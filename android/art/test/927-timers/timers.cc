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

#include <inttypes.h>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test926Timers {

extern "C" JNIEXPORT jint JNICALL Java_art_Test927_getAvailableProcessors(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jint count;
  jvmtiError result = jvmti_env->GetAvailableProcessors(&count);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }
  return count;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test927_getTime(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jlong time;
  jvmtiError result = jvmti_env->GetTime(&time);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }
  return time;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test927_getTimerInfo(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jvmtiTimerInfo info;
  jvmtiError result = jvmti_env->GetTimerInfo(&info);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint index) -> jobject {
    switch (index) {
      // Max value.
      case 0:
        return env->NewStringUTF(android::base::StringPrintf("%" PRId64, info.max_value).c_str());

      // Skip forward.
      case 1:
        return env->NewStringUTF(info.may_skip_forward == JNI_TRUE ? "true" : "false");
      // Skip backward.
      case 2:
        return env->NewStringUTF(info.may_skip_forward == JNI_TRUE ? "true" : "false");

      // The kind.
      case 3:
        return env->NewStringUTF(
            android::base::StringPrintf("%d", static_cast<jint>(info.kind)).c_str());
    }
    LOG(FATAL) << "Should not reach here";
    UNREACHABLE();
  };
  return CreateObjectArray(env, 4, "java/lang/Object", callback);
}

}  // namespace Test926Timers
}  // namespace art
