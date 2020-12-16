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

#include <stdio.h>

#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test923Monitors {


static jlong MonitorToLong(jrawMonitorID id) {
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(id));
}

static jrawMonitorID LongToMonitor(jlong l) {
  return reinterpret_cast<jrawMonitorID>(static_cast<uintptr_t>(l));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test923_createRawMonitor(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jrawMonitorID id;
  jvmtiError result = jvmti_env->CreateRawMonitor("dummy", &id);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return 0;
  }
  return MonitorToLong(id);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_destroyRawMonitor(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l) {
  jvmtiError result = jvmti_env->DestroyRawMonitor(LongToMonitor(l));
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_rawMonitorEnter(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l) {
  jvmtiError result = jvmti_env->RawMonitorEnter(LongToMonitor(l));
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_rawMonitorExit(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l) {
  jvmtiError result = jvmti_env->RawMonitorExit(LongToMonitor(l));
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_rawMonitorWait(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l, jlong millis) {
  jvmtiError result = jvmti_env->RawMonitorWait(LongToMonitor(l), millis);
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_rawMonitorNotify(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l) {
  jvmtiError result = jvmti_env->RawMonitorNotify(LongToMonitor(l));
  JvmtiErrorToException(env, jvmti_env, result);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test923_rawMonitorNotifyAll(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jlong l) {
  jvmtiError result = jvmti_env->RawMonitorNotifyAll(LongToMonitor(l));
  JvmtiErrorToException(env, jvmti_env, result);
}

}  // namespace Test923Monitors
}  // namespace art
