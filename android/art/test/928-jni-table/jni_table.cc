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

#include <stdio.h>

#include "jni.h"
#include "jvmti.h"

#include "android-base/logging.h"
#include "android-base/macros.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test927JNITable {

// This test is equivalent to the jni_internal_test JNIEnvExtTableOverride.

static size_t gGlobalRefCount = 0;
static JNINativeInterface* gOriginalEnv = nullptr;

static jobject CountNewGlobalRef(JNIEnv* env, jobject o) {
  ++gGlobalRefCount;
  return gOriginalEnv->NewGlobalRef(env, o);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test928_doJNITableTest(
    JNIEnv* env, jclass klass) {
  // Get the current table, as the delegate.
  jvmtiError getorig_result = jvmti_env->GetJNIFunctionTable(&gOriginalEnv);
  if (JvmtiErrorToException(env, jvmti_env, getorig_result)) {
    return;
  }

  // Get the current table, as the override we'll install.
  JNINativeInterface* env_override;
  jvmtiError getoverride_result = jvmti_env->GetJNIFunctionTable(&env_override);
  if (JvmtiErrorToException(env, jvmti_env, getoverride_result)) {
    return;
  }

  env_override->NewGlobalRef = CountNewGlobalRef;
  gGlobalRefCount = 0;

  // Install the override.
  jvmtiError setoverride_result = jvmti_env->SetJNIFunctionTable(env_override);
  if (JvmtiErrorToException(env, jvmti_env, setoverride_result)) {
    return;
  }

  jobject global = env->NewGlobalRef(klass);
  CHECK_EQ(1u, gGlobalRefCount);
  env->DeleteGlobalRef(global);

  // Install the "original." There is no real reset.
  jvmtiError setoverride2_result = jvmti_env->SetJNIFunctionTable(gOriginalEnv);
  if (JvmtiErrorToException(env, jvmti_env, setoverride2_result)) {
    return;
  }

  jobject global2 = env->NewGlobalRef(klass);
  CHECK_EQ(1u, gGlobalRefCount);
  env->DeleteGlobalRef(global2);

  // Try to install null. Should return NULL_POINTER error.
  jvmtiError setoverride3_result = jvmti_env->SetJNIFunctionTable(nullptr);
  if (setoverride3_result != JVMTI_ERROR_NULL_POINTER) {
    LOG(FATAL) << "Didn't receive NULL_POINTER";
  }

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(env_override));
}

}  // namespace Test927JNITable
}  // namespace art
