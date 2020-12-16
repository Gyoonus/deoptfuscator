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
namespace Test996ObsoleteBreakpoints {

static constexpr jint kNumFrames = 10;

static jmethodID GetFirstObsoleteMethod(JNIEnv* env, jvmtiEnv* jvmti_env) {
  jint frame_count;
  jvmtiFrameInfo frames[kNumFrames];
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetStackTrace(nullptr,  // current thread
                                                     0,
                                                     kNumFrames,
                                                     frames,
                                                     &frame_count))) {
    return nullptr;
  }
  for (jint i = 0; i < frame_count; i++) {
    jboolean is_obsolete = false;
    if (JvmtiErrorToException(env, jvmti_env,
                              jvmti_env->IsMethodObsolete(frames[i].method, &is_obsolete))) {
      return nullptr;
    }
    if (is_obsolete) {
      return frames[i].method;
    }
  }
  ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
  env->ThrowNew(rt_exception.get(), "Unable to find obsolete method!");
  return nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test996_setBreakpointOnObsoleteMethod(
    JNIEnv* env, jclass k ATTRIBUTE_UNUSED, jlong loc) {
  jmethodID method = GetFirstObsoleteMethod(env, jvmti_env);
  if (method == nullptr) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetBreakpoint(method, loc));
}

}  // namespace Test996ObsoleteBreakpoints
}  // namespace art
