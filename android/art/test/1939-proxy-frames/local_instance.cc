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
namespace Test1939ProxyFrames {

extern "C" JNIEXPORT jobject Java_art_Test1939_GetFrameMethod(JNIEnv* env,
                                                              jclass,
                                                              jthread thr,
                                                              jint depth) {
  jmethodID m = nullptr;
  jlong loc = -1;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetFrameLocation(thr, depth, &m, &loc))) {
    return nullptr;
  }
  jclass klass = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetMethodDeclaringClass(m, &klass))) {
    return nullptr;
  }
  jobject res = env->ToReflectedMethod(klass, m, false);
  env->DeleteLocalRef(klass);
  return res;
}

extern "C" JNIEXPORT jlong Java_art_Test1939_GetFrameLocation(JNIEnv* env,
                                                              jclass,
                                                              jthread thr,
                                                              jint depth) {
  jmethodID m = nullptr;
  jlong loc = -1;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetFrameLocation(thr, depth, &m, &loc));
  return loc;
}

}  // namespace Test1939ProxyFrames
}  // namespace art

