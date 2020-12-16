/*
 * Copyright (C) 2018 The Android Open Source Project
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


#include <cstdio>
#include <iostream>
#include <mutex>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1950UnpreparedTransform {

jclass kMainClass = nullptr;
jmethodID kPrepareFunc = nullptr;

extern "C" JNIEXPORT void ClassLoadCallback(jvmtiEnv* jvmti ATTRIBUTE_UNUSED,
                                               JNIEnv* env,
                                               jthread thr ATTRIBUTE_UNUSED,
                                               jclass klass) {
  env->CallStaticVoidMethod(kMainClass, kPrepareFunc, klass);
}

extern "C" JNIEXPORT void JNICALL Java_Main_clearClassLoadHook(
    JNIEnv* env, jclass main ATTRIBUTE_UNUSED, jthread thr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                            JVMTI_EVENT_CLASS_LOAD,
                                                            thr));
}
extern "C" JNIEXPORT void JNICALL Java_Main_setupClassLoadHook(
    JNIEnv* env, jclass main, jthread thr) {
  kMainClass = reinterpret_cast<jclass>(env->NewGlobalRef(main));
  kPrepareFunc = env->GetStaticMethodID(main, "doClassLoad", "(Ljava/lang/Class;)V");
  if (env->ExceptionCheck()) {
    return;
  }
  current_callbacks.ClassLoad = ClassLoadCallback;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(
                                &current_callbacks, sizeof(current_callbacks)))) {
    return;
  }
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                            JVMTI_EVENT_CLASS_LOAD,
                                                            thr));
}

}  // namespace Test1950UnpreparedTransform
}  // namespace art
