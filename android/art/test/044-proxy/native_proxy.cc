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

#include <android-base/logging.h>

namespace art {

extern "C" JNIEXPORT void JNICALL Java_NativeProxy_nativeCall(
    JNIEnv* env, jclass clazz ATTRIBUTE_UNUSED, jobject inf_ref) {
  jclass native_inf_class = env->FindClass("NativeInterface");
  CHECK(native_inf_class != nullptr);
  jmethodID mid = env->GetMethodID(native_inf_class, "callback", "()V");
  CHECK(mid != nullptr);
  env->CallVoidMethod(inf_ref, mid);
}

}  // namespace art
