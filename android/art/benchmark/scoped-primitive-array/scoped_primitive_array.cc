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
#include "nativehelper/ScopedPrimitiveArray.h"

extern "C" JNIEXPORT jlong JNICALL Java_ScopedPrimitiveArrayBenchmark_measureByteArray(
    JNIEnv* env, jclass, int reps, jbyteArray arr) {
  jlong ret = 0;
  for (jint i = 0; i < reps; ++i) {
    ScopedByteArrayRO sc(env, arr);
    ret += sc[0] + sc[sc.size() - 1];
  }
  return ret;
}

extern "C" JNIEXPORT jlong JNICALL Java_ScopedPrimitiveArrayBenchmark_measureShortArray(
    JNIEnv* env, jclass, int reps, jshortArray arr) {
  jlong ret = 0;
  for (jint i = 0; i < reps; ++i) {
    ScopedShortArrayRO sc(env, arr);
    ret += sc[0] + sc[sc.size() - 1];
  }
  return ret;
}

extern "C" JNIEXPORT jlong JNICALL Java_ScopedPrimitiveArrayBenchmark_measureIntArray(
    JNIEnv* env, jclass, int reps, jintArray arr) {
  jlong ret = 0;
  for (jint i = 0; i < reps; ++i) {
    ScopedIntArrayRO sc(env, arr);
    ret += sc[0] + sc[sc.size() - 1];
  }
  return ret;
}

extern "C" JNIEXPORT jlong JNICALL Java_ScopedPrimitiveArrayBenchmark_measureLongArray(
    JNIEnv* env, jclass, int reps, jlongArray arr) {
  jlong ret = 0;
  for (jint i = 0; i < reps; ++i) {
    ScopedLongArrayRO sc(env, arr);
    ret += sc[0] + sc[sc.size() - 1];
  }
  return ret;
}
