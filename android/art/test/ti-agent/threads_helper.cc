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

#include "common_helper.h"

#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace common_threads {

extern "C" JNIEXPORT void Java_art_Threads_interruptThread(JNIEnv* env, jclass, jthread thr) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->InterruptThread(thr));
}

extern "C" JNIEXPORT void Java_art_Threads_stopThread(JNIEnv* env,
                                                      jclass,
                                                      jthread thr,
                                                      jobject exception) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->StopThread(thr, exception));
}

}  // namespace common_threads
}  // namespace art
