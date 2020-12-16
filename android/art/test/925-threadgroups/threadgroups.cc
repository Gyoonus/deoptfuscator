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

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test925ThreadGroups {

//   private static native Object[] getThreadGroupInfo();
//   // Returns an array where element 0 is an array of threads and element 1 is an array of groups.
//   private static native Object[] getThreadGroupChildren();

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test925_getTopThreadGroups(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jthreadGroup* groups;
  jint group_count;
  jvmtiError result = jvmti_env->GetTopThreadGroups(&group_count, &groups);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint index) -> jobject {
    return groups[index];
  };
  jobjectArray ret = CreateObjectArray(env, group_count, "java/lang/ThreadGroup", callback);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(groups));

  return ret;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test925_getThreadGroupInfo(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthreadGroup group) {
  jvmtiThreadGroupInfo info;
  jvmtiError result = jvmti_env->GetThreadGroupInfo(group, &info);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint index) -> jobject {
    switch (index) {
      // The parent.
      case 0:
        return info.parent;

      // The name.
      case 1:
        return (info.name == nullptr) ? nullptr : env->NewStringUTF(info.name);

      // The priority. Use a string for simplicity of construction.
      case 2:
        return env->NewStringUTF(android::base::StringPrintf("%d", info.max_priority).c_str());

      // Whether it's a daemon. Use a string for simplicity of construction.
      case 3:
        return env->NewStringUTF(info.is_daemon == JNI_TRUE ? "true" : "false");
    }
    LOG(FATAL) << "Should not reach here";
    UNREACHABLE();
  };
  return CreateObjectArray(env, 4, "java/lang/Object", callback);
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test925_getThreadGroupChildren(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthreadGroup group) {
  jint thread_count;
  jthread* threads;
  jint threadgroup_count;
  jthreadGroup* groups;

  jvmtiError result = jvmti_env->GetThreadGroupChildren(group,
                                                        &thread_count,
                                                        &threads,
                                                        &threadgroup_count,
                                                        &groups);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint component_index) -> jobject {
    if (component_index == 0) {
      // Threads.
      auto inner_callback = [&](jint index) {
        return threads[index];
      };
      return CreateObjectArray(env, thread_count, "java/lang/Thread", inner_callback);
    } else {
      // Groups.
      auto inner_callback = [&](jint index) {
        return groups[index];
      };
      return CreateObjectArray(env, threadgroup_count, "java/lang/ThreadGroup", inner_callback);
    }
  };
  jobjectArray ret = CreateObjectArray(env, 2, "java/lang/Object", callback);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(threads));
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(groups));

  return ret;
}

}  // namespace Test925ThreadGroups
}  // namespace art
