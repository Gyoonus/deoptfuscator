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
namespace Test1908NativeSuspendResume {

std::atomic<bool> done(false);
std::atomic<bool> started(false);
std::atomic<bool> resumed(false);
std::atomic<bool> resuming(false);

extern "C" JNIEXPORT jint JNICALL Java_art_Test1908_nativeSpinAndResume(JNIEnv*,
                                                                        jclass,
                                                                        jthread thr) {
  while (!done.load()) {
    started.store(true);
  }
  resuming.store(true);
  jint ret = jvmti_env->ResumeThread(thr);
  resumed.store(true);
  return ret;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test1908_isNativeThreadSpinning(JNIEnv*, jclass) {
  return started.load();
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1908_waitForNativeResumeStarted(JNIEnv*, jclass) {
  while (!resuming.load()) {}
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1908_waitForNativeResumeFinished(JNIEnv*, jclass) {
  while (!resumed.load()) {}
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1908_nativeResume(JNIEnv*, jclass) {
  done.store(true);
}

}  // namespace Test1908NativeSuspendResume
}  // namespace art
