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
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test905ObjectFree {

static std::vector<jlong> collected_tags1;
static std::vector<jlong> collected_tags2;

jvmtiEnv* jvmti_env2;

static void JNICALL ObjectFree1(jvmtiEnv* ti_env, jlong tag) {
  CHECK_EQ(ti_env, jvmti_env);
  collected_tags1.push_back(tag);
}

static void JNICALL ObjectFree2(jvmtiEnv* ti_env, jlong tag) {
  CHECK_EQ(ti_env, jvmti_env2);
  collected_tags2.push_back(tag);
}

static void setupObjectFreeCallback(JNIEnv* env, jvmtiEnv* jenv, jvmtiEventObjectFree callback) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.ObjectFree = callback;
  jvmtiError ret = jenv->SetEventCallbacks(&callbacks, sizeof(callbacks));
  JvmtiErrorToException(env, jenv, ret);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test905_setupObjectFreeCallback(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  setupObjectFreeCallback(env, jvmti_env, ObjectFree1);
  JavaVM* jvm = nullptr;
  env->GetJavaVM(&jvm);
  CHECK_EQ(jvm->GetEnv(reinterpret_cast<void**>(&jvmti_env2), JVMTI_VERSION_1_2), 0);
  SetStandardCapabilities(jvmti_env2);
  setupObjectFreeCallback(env, jvmti_env2, ObjectFree2);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test905_enableFreeTracking(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jboolean enable) {
  jvmtiError ret = jvmti_env->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE,
      JVMTI_EVENT_OBJECT_FREE,
      nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
  ret = jvmti_env2->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE,
      JVMTI_EVENT_OBJECT_FREE,
      nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT jlongArray JNICALL Java_art_Test905_getCollectedTags(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jint index) {
  std::vector<jlong>& tags = (index == 0) ? collected_tags1 : collected_tags2;
  jlongArray ret = env->NewLongArray(tags.size());
  if (ret == nullptr) {
    return ret;
  }

  env->SetLongArrayRegion(ret, 0, tags.size(), tags.data());
  tags.clear();

  return ret;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test905_setTag2(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject obj, jlong tag) {
  jvmtiError ret = jvmti_env2->SetTag(obj, tag);
  JvmtiErrorToException(env, jvmti_env, ret);
}

}  // namespace Test905ObjectFree
}  // namespace art
