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

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test1909AgentTLS {

extern "C" JNIEXPORT void JNICALL Java_art_Test1909_setTLS(JNIEnv* env,
                                                           jclass,
                                                           jlong jvmti_env_ptr,
                                                           jthread thr,
                                                           jlong data) {
  JvmtiErrorToException(env,
                        reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr),
                        reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr)->SetThreadLocalStorage(
                            thr, reinterpret_cast<const void*>(static_cast<intptr_t>(data))));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test1909_getTLS(JNIEnv* env,
                                                            jclass,
                                                            jlong jvmti_env_ptr,
                                                            jthread thr) {
  void* res = nullptr;
  JvmtiErrorToException(
      env,
      reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr),
      reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr)->GetThreadLocalStorage(thr, &res));
  return static_cast<jlong>(reinterpret_cast<intptr_t>(res));
}

extern "C" JNIEXPORT void Java_art_Test1909_destroyJvmtiEnv(JNIEnv* env,
                                                            jclass,
                                                            jlong jvmti_env_ptr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        reinterpret_cast<jvmtiEnv*>(jvmti_env_ptr)->DisposeEnvironment());
}

extern "C" JNIEXPORT jlong Java_art_Test1909_newJvmtiEnv(JNIEnv* env, jclass) {
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

}  // namespace Test1909AgentTLS
}  // namespace art
