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

#include "jni.h"
#include "jvmti.h"

#include <vector>

#include "jvmti_helper.h"
#include "jni_helper.h"
#include "test_env.h"
#include "scoped_local_ref.h"

namespace art {
namespace common_monitors {

extern "C" JNIEXPORT jobject JNICALL Java_art_Monitors_getCurrentContendedMonitor(
    JNIEnv* env, jclass, jthread thr) {
  jobject out = nullptr;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetCurrentContendedMonitor(thr, &out));
  return out;
}

extern "C" JNIEXPORT jobject JNICALL Java_art_Monitors_getObjectMonitorUsage(
    JNIEnv* env, jclass, jobject obj) {
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Monitors$MonitorUsage"));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jmethodID constructor = env->GetMethodID(
      klass.get(),
      "<init>",
      "(Ljava/lang/Object;Ljava/lang/Thread;I[Ljava/lang/Thread;[Ljava/lang/Thread;)V");
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jvmtiMonitorUsage usage;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetObjectMonitorUsage(obj, &usage))) {
    return nullptr;
  }
  jobjectArray wait = CreateObjectArray(env, usage.waiter_count, "java/lang/Thread",
                                        [&](jint i) { return usage.waiters[i]; });
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.waiters));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.notify_waiters));
    return nullptr;
  }
  jobjectArray notify_wait = CreateObjectArray(env, usage.notify_waiter_count, "java/lang/Thread",
                                               [&](jint i) { return usage.notify_waiters[i]; });
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.waiters));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.notify_waiters));
    return nullptr;
  }
  return env->NewObject(klass.get(), constructor,
                        obj, usage.owner, usage.entry_count, wait, notify_wait);
}

struct MonitorsData {
  jclass test_klass;
  jmethodID monitor_enter;
  jmethodID monitor_entered;
  jmethodID monitor_wait;
  jmethodID monitor_waited;
  jclass monitor_klass;
};

static void monitorEnterCB(jvmtiEnv* jvmti,
                           JNIEnv* jnienv,
                           jthread thr,
                           jobject obj) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_enter, thr, obj);
}
static void monitorEnteredCB(jvmtiEnv* jvmti,
                             JNIEnv* jnienv,
                             jthread thr,
                             jobject obj) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_entered, thr, obj);
}
static void monitorWaitCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thr,
                          jobject obj,
                          jlong timeout) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_wait, thr, obj, timeout);
}
static void monitorWaitedCB(jvmtiEnv* jvmti,
                            JNIEnv* jnienv,
                            jthread thr,
                            jobject obj,
                            jboolean timed_out) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsInstanceOf(obj, data->monitor_klass)) {
    return;
  }
  jnienv->CallStaticVoidMethod(data->test_klass, data->monitor_waited, thr, obj, timed_out);
}

extern "C" JNIEXPORT void JNICALL Java_art_Monitors_setupMonitorEvents(
    JNIEnv* env,
    jclass,
    jclass test_klass,
    jobject monitor_enter,
    jobject monitor_entered,
    jobject monitor_wait,
    jobject monitor_waited,
    jclass monitor_klass,
    jthread thr) {
  MonitorsData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(MonitorsData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_generate_monitor_events = 1;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }

  memset(data, 0, sizeof(MonitorsData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(test_klass));
  data->monitor_enter = env->FromReflectedMethod(monitor_enter);
  data->monitor_entered = env->FromReflectedMethod(monitor_entered);
  data->monitor_wait = env->FromReflectedMethod(monitor_wait);
  data->monitor_waited = env->FromReflectedMethod(monitor_waited);
  data->monitor_klass = reinterpret_cast<jclass>(env->NewGlobalRef(monitor_klass));
  MonitorsData* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetEnvironmentLocalStorage(
                                reinterpret_cast<void**>(&old_data)))) {
    return;
  } else if (old_data != nullptr && old_data->test_klass != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data))) {
    return;
  }

  current_callbacks.MonitorContendedEnter = monitorEnterCB;
  current_callbacks.MonitorContendedEntered = monitorEnteredCB;
  current_callbacks.MonitorWait = monitorWaitCB;
  current_callbacks.MonitorWaited = monitorWaitedCB;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(&current_callbacks,
                                                         sizeof(current_callbacks)))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_WAIT, thr))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(
                                JVMTI_ENABLE, JVMTI_EVENT_MONITOR_WAITED, thr))) {
    return;
  }
}

}  // namespace common_monitors
}  // namespace art

