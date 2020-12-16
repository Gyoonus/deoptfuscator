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

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test930AgentThread {

struct AgentData {
  AgentData() : main_thread(nullptr),
                jvmti_env(nullptr),
                priority(0) {
  }

  jthread main_thread;
  jvmtiEnv* jvmti_env;
  pthread_barrier_t b;
  jint priority;
};

static void AgentMain(jvmtiEnv* jenv, JNIEnv* env, void* arg) {
  AgentData* data = reinterpret_cast<AgentData*>(arg);

  // Check some basics.
  // This thread is not the main thread.
  jthread this_thread;
  jvmtiError this_thread_result = jenv->GetCurrentThread(&this_thread);
  CheckJvmtiError(jenv, this_thread_result);
  CHECK(!env->IsSameObject(this_thread, data->main_thread));

  // The thread is a daemon.
  jvmtiThreadInfo info;
  jvmtiError info_result = jenv->GetThreadInfo(this_thread, &info);
  CheckJvmtiError(jenv, info_result);
  CHECK(info.is_daemon);
  CheckJvmtiError(jenv, jenv->Deallocate(reinterpret_cast<unsigned char*>(info.name)));
  if (info.thread_group != nullptr) {
    env->DeleteLocalRef(info.thread_group);
  }
  if (info.context_class_loader != nullptr) {
    env->DeleteLocalRef(info.context_class_loader);
  }

  // The thread has the requested priority.
  // TODO: Our thread priorities do not work on the host.
  // CHECK_EQ(info.priority, data->priority);

  // Check further parts of the thread:
  jint thread_count;
  jthread* threads;
  jvmtiError threads_result = jenv->GetAllThreads(&thread_count, &threads);
  CheckJvmtiError(jenv, threads_result);
  bool found = false;
  for (jint i = 0; i != thread_count; ++i) {
    if (env->IsSameObject(threads[i], this_thread)) {
      found = true;
      break;
    }
  }
  CHECK(found);

  // Done, let the main thread progress.
  int wait_result = pthread_barrier_wait(&data->b);
  CHECK(wait_result == PTHREAD_BARRIER_SERIAL_THREAD || wait_result == 0);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test931_testAgentThread(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  // Create a Thread object.
  ScopedLocalRef<jobject> thread_name(env, env->NewStringUTF("Agent Thread"));
  if (thread_name.get() == nullptr) {
    return;
  }

  ScopedLocalRef<jclass> thread_klass(env, env->FindClass("java/lang/Thread"));
  if (thread_klass.get() == nullptr) {
    return;
  }
  ScopedLocalRef<jobject> thread(env, env->AllocObject(thread_klass.get()));
  if (thread.get() == nullptr) {
    return;
  }

  // Get a ThreadGroup from the current thread. We need a non-null one as we're gonna call a
  // runtime-only constructor (so we can set priority and daemon state).
  jvmtiThreadInfo cur_thread_info;
  jvmtiError info_result = jvmti_env->GetThreadInfo(nullptr, &cur_thread_info);
  if (JvmtiErrorToException(env, jvmti_env, info_result)) {
    return;
  }
  CheckJvmtiError(jvmti_env,
                  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(cur_thread_info.name)));
  ScopedLocalRef<jobject> thread_group(env, cur_thread_info.thread_group);
  if (cur_thread_info.context_class_loader != nullptr) {
    env->DeleteLocalRef(cur_thread_info.context_class_loader);
  }

  jmethodID initID = env->GetMethodID(thread_klass.get(),
                                      "<init>",
                                      "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V");
  if (initID == nullptr) {
    return;
  }
  env->CallNonvirtualVoidMethod(thread.get(),
                                thread_klass.get(),
                                initID,
                                thread_group.get(),
                                thread_name.get(),
                                0,
                                JNI_FALSE);
  if (env->ExceptionCheck()) {
    return;
  }

  jthread main_thread;
  jvmtiError main_thread_result = jvmti_env->GetCurrentThread(&main_thread);
  if (JvmtiErrorToException(env, jvmti_env, main_thread_result)) {
    return;
  }

  AgentData data;
  data.main_thread = env->NewGlobalRef(main_thread);
  data.jvmti_env = jvmti_env;
  data.priority = JVMTI_THREAD_MIN_PRIORITY;
  CHECK_EQ(0, pthread_barrier_init(&data.b, nullptr, 2));

  jvmtiError result = jvmti_env->RunAgentThread(thread.get(), AgentMain, &data, data.priority);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return;
  }

  int wait_result = pthread_barrier_wait(&data.b);
  CHECK(wait_result == PTHREAD_BARRIER_SERIAL_THREAD || wait_result == 0);

  // Scheduling may mean that the agent thread is put to sleep. Wait until it's dead in an effort
  // to not unload the plugin and crash.
  for (;;) {
    sleep(1);
    jint thread_state;
    jvmtiError state_result = jvmti_env->GetThreadState(thread.get(), &thread_state);
    if (JvmtiErrorToException(env, jvmti_env, state_result)) {
      return;
    }
    if (thread_state == 0 ||                                    // Was never alive.
        (thread_state & JVMTI_THREAD_STATE_TERMINATED) != 0) {  // Was alive and died.
      break;
    }
  }
  // Yield and sleep a bit more, to give the plugin time to tear down the native thread structure.
  sched_yield();
  sleep(1);

  env->DeleteGlobalRef(data.main_thread);

  pthread_barrier_destroy(&data.b);
}

}  // namespace Test930AgentThread
}  // namespace art
