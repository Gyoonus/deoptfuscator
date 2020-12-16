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

#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "test_env.h"

#include "jvmti_helper.h"
#include "scoped_local_ref.h"

namespace art {
namespace Test984ObsoleteInvoke {

static constexpr size_t kNumFrames = 30;

extern "C" JNIEXPORT jobject JNICALL Java_art_Test984_getFirstObsoleteMethod984(JNIEnv* env,
                                                                                jclass) {
  jthread cur;
  jint frame_count;
  jvmtiFrameInfo frames[kNumFrames];
  // jint cur_start = 0;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetCurrentThread(&cur))) {
    // ERROR
    return nullptr;
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetStackTrace(cur,
                                                     0,
                                                     kNumFrames,
                                                     frames,
                                                     &frame_count))) {
    // ERROR
    return nullptr;
  }
  for (jint i = 0; i < frame_count; i++) {
    jmethodID method = frames[i].method;
    jboolean is_obsolete = false;
    if (JvmtiErrorToException(env, jvmti_env, jvmti_env->IsMethodObsolete(method, &is_obsolete))) {
      // ERROR
      return nullptr;
    }
    if (is_obsolete) {
      return env->ToReflectedMethod(env->FindClass("java/lang/reflect/Method"),
                                    method,
                                    JNI_TRUE);
    }
  }
  ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
  env->ThrowNew(rt_exception.get(), "Unable to find obsolete method!");
  return nullptr;
}

}  // namespace Test984ObsoleteInvoke
}  // namespace art
