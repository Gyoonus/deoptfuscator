/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "jni.h"

namespace art {

// Simple empty method. We will check for correct registration with UnsatisfiedLinkError.
static void foo(JNIEnv*, jclass) {
}

static JNINativeMethod gMethods[] = {
    { "foo", "()V", reinterpret_cast<void*>(foo) }
};

extern "C" JNIEXPORT jint JNICALL Java_Main_registerNatives(JNIEnv* env, jclass, jclass trg) {
  return env->RegisterNatives(trg, gMethods, 1);
}

}  // namespace art
