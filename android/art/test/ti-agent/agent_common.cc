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

#include "android-base/logging.h"
#include "android-base/macros.h"

#include "jvmti_helper.h"
#include "test_env.h"

namespace art {

// Common JNI functions.

extern "C" JNIEXPORT void JNICALL Java_art_Main_setTag(
    JNIEnv* env, jclass, jobject obj, jlong tag) {
  jvmtiError ret = jvmti_env->SetTag(obj, tag);
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Main_getTag(JNIEnv* env, jclass, jobject obj) {
  jlong tag = 0;
  jvmtiError ret = jvmti_env->GetTag(obj, &tag);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return 0;
  }
  return tag;
}

}  // namespace art
