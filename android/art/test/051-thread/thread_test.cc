/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "base/macros.h"
#include "jni.h"
#include "thread-inl.h"

namespace art {

extern "C" JNIEXPORT jint JNICALL Java_Main_getNativePriority(JNIEnv* env,
                                                              jclass clazz ATTRIBUTE_UNUSED) {
  return ThreadForEnv(env)->GetNativePriority();
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_supportsThreadPriorities(
    JNIEnv* env ATTRIBUTE_UNUSED,
    jclass clazz ATTRIBUTE_UNUSED) {
#if defined(ART_TARGET_ANDROID)
  return JNI_TRUE;
#else
  return JNI_FALSE;
#endif
}

}  // namespace art
