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

#include <atomic>

#include "android-base/logging.h"
#include "jni.h"
#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1941DisposeStress {

extern "C" JNIEXPORT void JNICALL Java_art_Test1941_setTracingOn(JNIEnv* env,
                                                                 jclass,
                                                                 jthread thr,
                                                                 jboolean enable) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(enable ? JVMTI_ENABLE : JVMTI_DISABLE,
                                                            JVMTI_EVENT_SINGLE_STEP,
                                                            thr));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1941_AllocEnv(JNIEnv* env, jclass) {
  JavaVM* vm = nullptr;
  if (env->GetJavaVM(&vm) != 0) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to get JavaVM");
    return -1;
  }
  jvmtiEnv* new_env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&new_env), JVMTI_VERSION_1_0) != 0) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to create new jvmtiEnv");
    return -1;
  }
  return static_cast<jlong>(reinterpret_cast<intptr_t>(new_env));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1941_FreeEnv(JNIEnv* env,
                                                            jclass,
                                                            jlong jvmti_env_ptr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr)->DisposeEnvironment());
}

}  // namespace Test1941DisposeStress
}  // namespace art

