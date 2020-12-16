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

#include "jni_binder.h"
#include "jvmti_helper.h"
#include "scoped_utf_chars.h"
#include "test_env.h"

namespace art {

// Utility functions for binding jni methods.
extern "C" JNIEXPORT void JNICALL Java_art_Main_bindAgentJNI(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jstring className, jobject classLoader) {
  ScopedUtfChars name(env, className);
  BindFunctions(jvmti_env, env, name.c_str(), classLoader);
}

extern "C" JNIEXPORT void JNICALL Java_art_Main_bindAgentJNIForClass(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jclass bindClass) {
  BindFunctionsOnClass(jvmti_env, env, bindClass);
}

}  // namespace art
