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
#include <mutex>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test904ObjectAllocation {

static std::string GetClassName(JNIEnv* jni_env, jclass cls) {
  ScopedLocalRef<jclass> class_class(jni_env, jni_env->GetObjectClass(cls));
  jmethodID mid = jni_env->GetMethodID(class_class.get(), "getName", "()Ljava/lang/String;");
  ScopedLocalRef<jstring> str(
      jni_env, reinterpret_cast<jstring>(jni_env->CallObjectMethod(cls, mid)));
  ScopedUtfChars utf_chars(jni_env, str.get());
  return utf_chars.c_str();
}

static std::mutex gEventsMutex;
static std::vector<std::string> gEvents;

static void JNICALL ObjectAllocated(jvmtiEnv* ti_env ATTRIBUTE_UNUSED,
                                    JNIEnv* jni_env,
                                    jthread thread ATTRIBUTE_UNUSED,
                                    jobject object,
                                    jclass object_klass,
                                    jlong size) {
  std::string object_klass_descriptor = GetClassName(jni_env, object_klass);
  ScopedLocalRef<jclass> object_klass2(jni_env, jni_env->GetObjectClass(object));
  std::string object_klass_descriptor2 = GetClassName(jni_env, object_klass2.get());

  std::lock_guard<std::mutex> guard(gEventsMutex);
  gEvents.push_back(android::base::StringPrintf("ObjectAllocated type %s/%s size %zu",
                                                object_klass_descriptor.c_str(),
                                                object_klass_descriptor2.c_str(),
                                                static_cast<size_t>(size)));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test904_setupObjectAllocCallback(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jboolean enable) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.VMObjectAlloc = enable ? ObjectAllocated : nullptr;

  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test904_enableAllocationTracking(
    JNIEnv* env, jclass, jthread thread, jboolean enable) {
  jvmtiError ret = jvmti_env->SetEventNotificationMode(
      enable ? JVMTI_ENABLE : JVMTI_DISABLE,
      JVMTI_EVENT_VM_OBJECT_ALLOC,
      thread);
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test904_getTrackingEventMessages(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  std::lock_guard<std::mutex> guard(gEventsMutex);
  jobjectArray ret = CreateObjectArray(env,
                                       static_cast<jint>(gEvents.size()),
                                       "java/lang/String",
                                       [&](jint i) {
    return env->NewStringUTF(gEvents[i].c_str());
  });
  gEvents.clear();
  return ret;
}

}  // namespace Test904ObjectAllocation
}  // namespace art
