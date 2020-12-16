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

#include <cstdio>
#include <memory>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"

// Test infrastructure
#include "jni_binder.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test945ObsoleteNative {

extern "C" JNIEXPORT void JNICALL Java_art_Test945_00024Transform_doExecute(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject runnable) {
  jclass runnable_klass = env->FindClass("java/lang/Runnable");
  jmethodID run_method = env->GetMethodID(runnable_klass, "run", "()V");
  env->CallVoidMethod(runnable, run_method);
}


}  // namespace Test945ObsoleteNative
}  // namespace art
