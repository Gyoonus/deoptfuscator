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

#include <mutex>
#include <string>
#include <vector>

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
namespace Test924Threads {

struct WaiterStruct {
  std::atomic<bool> started;
  std::atomic<bool> finish;
};

extern "C" JNIEXPORT jlong JNICALL Java_art_Test924_nativeWaiterStructAlloc(
    JNIEnv* env, jclass TestClass ATTRIBUTE_UNUSED) {
  WaiterStruct* s = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(WaiterStruct),
                                                reinterpret_cast<unsigned char**>(&s)))) {
    return 0;
  }
  s->started = false;
  s->finish = false;
  return static_cast<jlong>(reinterpret_cast<intptr_t>(s));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test924_nativeWaiterStructWaitForNative(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass TestClass ATTRIBUTE_UNUSED, jlong waiter_struct) {
  WaiterStruct* s = reinterpret_cast<WaiterStruct*>(static_cast<intptr_t>(waiter_struct));
  while (!s->started) { }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test924_nativeWaiterStructFinish(
    JNIEnv* env ATTRIBUTE_UNUSED, jclass TestClass ATTRIBUTE_UNUSED, jlong waiter_struct) {
  WaiterStruct* s = reinterpret_cast<WaiterStruct*>(static_cast<intptr_t>(waiter_struct));
  s->finish = true;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test924_nativeLoop(JNIEnv* env,
                                                              jclass TestClass ATTRIBUTE_UNUSED,
                                                              jlong waiter_struct) {
  WaiterStruct* s = reinterpret_cast<WaiterStruct*>(static_cast<intptr_t>(waiter_struct));
  s->started = true;
  while (!s->finish) { }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(s)));
}

// private static native Thread getCurrentThread();
// private static native Object[] getThreadInfo(Thread t);

extern "C" JNIEXPORT jthread JNICALL Java_art_Test924_getCurrentThread(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jthread thread = nullptr;
  jvmtiError result = jvmti_env->GetCurrentThread(&thread);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }
  return thread;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test924_getThreadInfo(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthread thread) {
  jvmtiThreadInfo info;
  memset(&info, 0, sizeof(jvmtiThreadInfo));

  jvmtiError result = jvmti_env->GetThreadInfo(thread, &info);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint component_index) -> jobject {
    switch (component_index) {
      // The name.
      case 0:
        return (info.name == nullptr) ? nullptr : env->NewStringUTF(info.name);

      // The priority. Use a string for simplicity of construction.
      case 1:
        return env->NewStringUTF(android::base::StringPrintf("%d", info.priority).c_str());

      // Whether it's a daemon. Use a string for simplicity of construction.
      case 2:
        return env->NewStringUTF(info.is_daemon == JNI_TRUE ? "true" : "false");

      // The thread group;
      case 3:
        return env->NewLocalRef(info.thread_group);

      // The context classloader.
      case 4:
        return env->NewLocalRef(info.context_class_loader);
    }
    LOG(FATAL) << "Should not reach here";
    UNREACHABLE();
  };
  jobjectArray ret = CreateObjectArray(env, 5, "java/lang/Object", callback);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(info.name));
  if (info.thread_group != nullptr) {
    env->DeleteLocalRef(info.thread_group);
  }
  if (info.context_class_loader != nullptr) {
    env->DeleteLocalRef(info.context_class_loader);
  }

  return ret;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test924_getThreadState(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthread thread) {
  jint state;
  jvmtiError result = jvmti_env->GetThreadState(thread, &state);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return 0;
  }
  return state;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test924_getAllThreads(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jint thread_count;
  jthread* threads;

  jvmtiError result = jvmti_env->GetAllThreads(&thread_count, &threads);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint index) {
    return threads[index];
  };
  jobjectArray ret = CreateObjectArray(env, thread_count, "java/lang/Thread", callback);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(threads));

  return ret;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test924_getTLS(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthread thread) {
  void* tls;
  jvmtiError result = jvmti_env->GetThreadLocalStorage(thread, &tls);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return 0;
  }
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(tls));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test924_setTLS(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jthread thread, jlong val) {
  const void* tls = reinterpret_cast<void*>(static_cast<uintptr_t>(val));
  jvmtiError result = jvmti_env->SetThreadLocalStorage(thread, tls);
  JvmtiErrorToException(env, jvmti_env, result);
}

static std::mutex gEventsMutex;
static std::vector<std::string> gEvents;

static void JNICALL ThreadEvent(jvmtiEnv* jvmti_env,
                                JNIEnv* jni_env,
                                jthread thread,
                                bool is_start) {
  jvmtiThreadInfo info;
  {
    std::lock_guard<std::mutex> guard(gEventsMutex);

    jvmtiError result = jvmti_env->GetThreadInfo(thread, &info);
    if (result != JVMTI_ERROR_NONE) {
      gEvents.push_back("Error getting thread info");
      return;
    }

    gEvents.push_back(android::base::StringPrintf("Thread(%s): %s",
                                                  info.name,
                                                  is_start ? "start" : "end"));
  }

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(info.name));
  jni_env->DeleteLocalRef(info.thread_group);
  jni_env->DeleteLocalRef(info.context_class_loader);
}

static void JNICALL ThreadStart(jvmtiEnv* jvmti_env,
                                JNIEnv* jni_env,
                                jthread thread) {
  ThreadEvent(jvmti_env, jni_env, thread, true);
}

static void JNICALL ThreadEnd(jvmtiEnv* jvmti_env,
                              JNIEnv* jni_env,
                              jthread thread) {
  ThreadEvent(jvmti_env, jni_env, thread, false);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test924_enableThreadEvents(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jboolean b) {
  if (b == JNI_FALSE) {
    jvmtiError ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                         JVMTI_EVENT_THREAD_START,
                                                         nullptr);
    if (JvmtiErrorToException(env, jvmti_env, ret)) {
      return;
    }
    ret = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                              JVMTI_EVENT_THREAD_END,
                                              nullptr);
    JvmtiErrorToException(env, jvmti_env, ret);
    return;
  }

  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.ThreadStart = ThreadStart;
  callbacks.ThreadEnd = ThreadEnd;
  jvmtiError ret = jvmti_env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }

  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_THREAD_START,
                                            nullptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return;
  }
  ret = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                            JVMTI_EVENT_THREAD_END,
                                            nullptr);
  JvmtiErrorToException(env, jvmti_env, ret);
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test924_getThreadEventMessages(
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

}  // namespace Test924Threads
}  // namespace art
