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

#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "android-base/logging.h"
#include "jni.h"
#include "jvmti.h"

#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1922OwnedMonitors {

static bool doMonitorEnter(JNIEnv* env, jobject target) {
  return env->MonitorEnter(target) != 0;
}
static bool doMonitorExit(JNIEnv* env, jobject target) {
  return env->MonitorExit(target) != 0;
}

static bool doCallRunnable(JNIEnv* env, jobject next) {
  ScopedLocalRef<jclass> run_class(env, env->FindClass("java/lang/Runnable"));
  if (run_class.get() == nullptr) {
    return true;
  }
  jmethodID run = env->GetMethodID(run_class.get(), "run", "()V");
  if (env->ExceptionCheck()) {
    return true;
  }
  env->CallVoidMethod(next, run);
  return env->ExceptionCheck();
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1922_00024Target_lockThisNative(
    JNIEnv* env, jobject thiz, jobject next) {
  if (doMonitorEnter(env, thiz)) {
    return;
  }
  if (doCallRunnable(env, next)) {
    return;
  }
  if (doMonitorExit(env, thiz)) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1922_00024Target_lockNative(
    JNIEnv* env, jobject thiz ATTRIBUTE_UNUSED, jobject mon, jobject next) {
  if (doMonitorEnter(env, mon)) {
    return;
  }
  if (doCallRunnable(env, next)) {
    return;
  }
  if (doMonitorExit(env, mon)) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1922_setupTest(JNIEnv* env, jclass) {
  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_get_owned_monitor_info = 1;
  caps.can_get_owned_monitor_stack_depth_info = 1;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps));
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test1922_getOwnedMonitorStackDepthInfo(
    JNIEnv* env, jclass, jthread thread) {
  jint len = 0;
  jvmtiMonitorStackDepthInfo* monitors = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetOwnedMonitorStackDepthInfo(thread, &len, &monitors))) {
    return nullptr;
  }
  ScopedLocalRef<jclass> ret_class(env, env->FindClass("art/Test1922$MonitorStackDepthInfo"));
  if (ret_class.get() == nullptr) {
    // CNFE should be pending.
    return nullptr;
  }
  jmethodID constructor = env->GetMethodID(ret_class.get(), "<init>", "(ILjava/lang/Object;)V");
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  return CreateObjectArray(env, len, "art/Test1922$MonitorStackDepthInfo",
                           [&](jint i) {
                             return env->NewObject(ret_class.get(),
                                                   constructor,
                                                   monitors[i].stack_depth,
                                                   monitors[i].monitor);
                           });
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test1922_getOwnedMonitors(JNIEnv* env,
                                                                             jclass,
                                                                             jthread thread) {
  jint len = 0;
  jobject* arr = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetOwnedMonitorInfo(thread, &len, &arr))) {
    return nullptr;
  }
  ScopedLocalRef<jclass> obj_class(env, env->FindClass("java/lang/Object"));
  if (obj_class.get() == nullptr) {
    // CNFE should be pending.
    return nullptr;
  }
  jobjectArray ret = env->NewObjectArray(len, obj_class.get(), nullptr);
  if (ret == nullptr) {
    return nullptr;
  }
  for (jint i = 0; i < len; i++) {
    env->SetObjectArrayElement(ret, i, arr[i]);
    if (env->ExceptionCheck()) {
      return nullptr;
    }
  }
  return ret;
}

}  // namespace Test1922OwnedMonitors
}  // namespace art
