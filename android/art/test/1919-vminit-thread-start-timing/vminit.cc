/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "1919-vminit-thread-start-timing/vminit.h"

#include <mutex>
#include <thread>
#include <vector>

#include <jni.h>
#include <stdio.h>
#include <string.h>
#include "android-base/macros.h"
#include "jvmti.h"

// Test infrastructure
#include "scoped_local_ref.h"
#include "jvmti_helper.h"
#include "jni_helper.h"
#include "test_env.h"

namespace art {
namespace Test1919VMInitThreadStart {

struct EventData {
  std::string event;
  jobject data;
};

struct EventList {
  jrawMonitorID events_mutex;
  std::vector<EventData> events;
};


static void EnableEvent(jvmtiEnv* env, jvmtiEvent evt) {
  jvmtiError error = env->SetEventNotificationMode(JVMTI_ENABLE, evt, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    printf("Failed to enable event");
  }
}

static void JNICALL ThreadStartCallback(jvmtiEnv *jvmti, JNIEnv* env, jthread thread) {
  EventList* list = nullptr;
  CheckJvmtiError(jvmti, jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&list)));
  CheckJvmtiError(jvmti, jvmti->RawMonitorEnter(list->events_mutex));
  list->events.push_back({ "ThreadStart", env->NewGlobalRef(thread) });
  CheckJvmtiError(jvmti, jvmti->RawMonitorExit(list->events_mutex));
}

static void JNICALL Test1919AgentThread(jvmtiEnv* jvmti,
                                        JNIEnv* env,
                                        void* arg ATTRIBUTE_UNUSED) {
  EventList* list = nullptr;
  CheckJvmtiError(jvmti, jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&list)));
  CheckJvmtiError(jvmti, jvmti->RawMonitorEnter(list->events_mutex));
  jthread cur;
  CheckJvmtiError(jvmti, jvmti->GetCurrentThread(&cur));
  list->events.push_back({ "Test1919AgentThread", env->NewGlobalRef(cur) });
  env->DeleteLocalRef(cur);
  // Wake up VMInit
  CheckJvmtiError(jvmti, jvmti->RawMonitorNotify(list->events_mutex));
  CheckJvmtiError(jvmti, jvmti->RawMonitorExit(list->events_mutex));
}

static void CreateAgentThread(jvmtiEnv* jvmti, JNIEnv* env) {
  // Create a Thread object.
  ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF("JVMTI_THREAD-Test1919"));
  CHECK(thread_name.get() != nullptr);

  ScopedLocalRef<jclass> thread_klass(env, env->FindClass("java/lang/Thread"));
  CHECK(thread_klass.get() != nullptr);

  ScopedLocalRef<jobject> thread(env, env->AllocObject(thread_klass.get()));
  CHECK(thread.get() != nullptr);

  jmethodID initID = env->GetMethodID(thread_klass.get(), "<init>", "(Ljava/lang/String;)V");
  CHECK(initID != nullptr);

  env->CallNonvirtualVoidMethod(thread.get(), thread_klass.get(), initID, thread_name.get());
  CHECK(!env->ExceptionCheck());

  // Run agent thread.
  CheckJvmtiError(jvmti, jvmti->RunAgentThread(thread.get(),
                                               Test1919AgentThread,
                                               nullptr,
                                               JVMTI_THREAD_NORM_PRIORITY));
}

static void JNICALL VMInitCallback(jvmtiEnv *jvmti, JNIEnv* env, jthread thread) {
  EventList* list = nullptr;
  CheckJvmtiError(jvmti, jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&list)));
  CheckJvmtiError(jvmti, jvmti->RawMonitorEnter(list->events_mutex));
  list->events.push_back({ "VMInit", env->NewGlobalRef(thread) });
  // Create a new thread.
  CreateAgentThread(jvmti, env);
  // Wait for new thread to run.
  CheckJvmtiError(jvmti, jvmti->RawMonitorWait(list->events_mutex, 0));
  CheckJvmtiError(jvmti, jvmti->RawMonitorExit(list->events_mutex));
}

static void InstallVMEvents(jvmtiEnv* env) {
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));
  callbacks.VMInit = VMInitCallback;
  callbacks.ThreadStart = ThreadStartCallback;
  jvmtiError ret = env->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (ret != JVMTI_ERROR_NONE) {
    printf("Failed to install callbacks");
  }

  EnableEvent(env, JVMTI_EVENT_VM_INIT);
  EnableEvent(env, JVMTI_EVENT_THREAD_START);
}

static void InstallEventList(jvmtiEnv* env) {
  EventList* list = nullptr;
  CheckJvmtiError(env, env->Allocate(sizeof(EventList), reinterpret_cast<unsigned char**>(&list)));
  memset(list, 0, sizeof(EventList));
  CheckJvmtiError(env, env->CreateRawMonitor("Test1919 Monitor", &list->events_mutex));
  CheckJvmtiError(env, env->SetEnvironmentLocalStorage(list));
}

jint OnLoad(JavaVM* vm,
            char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0) != 0) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  InstallVMEvents(jvmti_env);
  InstallEventList(jvmti_env);
  return 0;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test1919_getEventNames(JNIEnv* env, jclass) {
  EventList* list = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(
                                reinterpret_cast<void**>(&list)))) {
    return nullptr;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(list->events_mutex))) {
    return nullptr;
  }
  jobjectArray ret = CreateObjectArray(env, list->events.size(), "java/lang/String",
                                       [&](jint i) {
                                         return env->NewStringUTF(list->events[i].event.c_str());
                                       });
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(list->events_mutex))) {
    return nullptr;
  }
  return ret;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test1919_getEventThreads(JNIEnv* env, jclass) {
  EventList* list = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(
                                reinterpret_cast<void**>(&list)))) {
    return nullptr;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorEnter(list->events_mutex))) {
    return nullptr;
  }
  jobjectArray ret = CreateObjectArray(env, list->events.size(), "java/lang/Thread",
                                       [&](jint i) {
                                         return env->NewLocalRef(list->events[i].data);
                                       });
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->RawMonitorExit(list->events_mutex))) {
    return nullptr;
  }
  return ret;
}

}  // namespace Test1919VMInitThreadStart
}  // namespace art
