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
#include "android-base/macros.h"
#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test929Search {

extern "C" JNIEXPORT void JNICALL Java_Main_addToBootClassLoader(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jstring segment) {
  ScopedUtfChars utf(env, segment);
  if (utf.c_str() == nullptr) {
    return;
  }
  jvmtiError result = jvmti_env->AddToBootstrapClassLoaderSearch(utf.c_str());
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_Main_addToSystemClassLoader(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jstring segment) {
  ScopedUtfChars utf(env, segment);
  if (utf.c_str() == nullptr) {
    return;
  }
  jvmtiError result = jvmti_env->AddToSystemClassLoaderSearch(utf.c_str());
  JvmtiErrorToException(env, jvmti_env, result);
}

}  // namespace Test929Search
}  // namespace art
