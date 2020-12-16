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

#include <inttypes.h>

#include <cstdio>
#include <memory>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_binder.h"
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test993Breakpoints {

extern "C" JNIEXPORT
jobject JNICALL Java_art_Test993_constructNative(JNIEnv* env,
                                                 jclass klass ATTRIBUTE_UNUSED,
                                                 jobject target,
                                                 jclass clazz) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  return env->NewObject(clazz, method);
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test993_invokeNativeObject(JNIEnv* env,
                                                 jclass klass ATTRIBUTE_UNUSED,
                                                 jobject target,
                                                 jclass clazz,
                                                 jobject thizz) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  if (thizz == nullptr) {
    env->CallStaticObjectMethod(clazz, method);
  } else {
    env->CallObjectMethod(thizz, method);
  }
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test993_invokeNativeBool(JNIEnv* env,
                                               jclass klass ATTRIBUTE_UNUSED,
                                               jobject target,
                                               jclass clazz,
                                               jobject thizz) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  if (thizz == nullptr) {
    env->CallStaticBooleanMethod(clazz, method);
  } else {
    env->CallBooleanMethod(thizz, method);
  }
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test993_invokeNativeLong(JNIEnv* env,
                                               jclass klass ATTRIBUTE_UNUSED,
                                               jobject target,
                                               jclass clazz,
                                               jobject thizz) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  if (thizz == nullptr) {
    env->CallStaticLongMethod(clazz, method);
  } else {
    env->CallLongMethod(thizz, method);
  }
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test993_invokeNative(JNIEnv* env,
                                           jclass klass ATTRIBUTE_UNUSED,
                                           jobject target,
                                           jclass clazz,
                                           jobject thizz) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  if (thizz == nullptr) {
    env->CallStaticVoidMethod(clazz, method);
  } else {
    env->CallVoidMethod(thizz, method);
  }
}

}  // namespace Test993Breakpoints
}  // namespace art

