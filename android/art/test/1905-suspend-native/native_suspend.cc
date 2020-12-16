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
namespace Test1905NativeSuspend {

std::atomic<bool> done(false);
std::atomic<bool> started(false);

extern "C" JNIEXPORT void JNICALL Java_art_Test1905_nativeSpin(JNIEnv*, jclass) {
  while (!done.load()) {
    started.store(true);
  }
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test1905_isNativeThreadSpinning(JNIEnv*, jclass) {
  return started.load();
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1905_nativeResume(JNIEnv*, jclass) {
  done.store(true);
}

}  // namespace Test1905NativeSuspend
}  // namespace art
