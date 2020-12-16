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

#include "common_helper.h"

#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace common_frame_pop {

struct FramePopData {
  jclass test_klass;
  jmethodID pop_method;
};

static void framePopCB(jvmtiEnv* jvmti,
                       JNIEnv* jnienv,
                       jthread thr,
                       jmethodID method ATTRIBUTE_UNUSED,
                       jboolean was_popped_by_exception) {
  FramePopData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  jlong location;
  jmethodID frame_method;
  if (JvmtiErrorToException(jnienv,
                            jvmti,
                            jvmti->GetFrameLocation(thr, 0, &frame_method, &location))) {
    return;
  }
  CHECK(data->pop_method != nullptr);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, frame_method);
  jnienv->CallStaticVoidMethod(data->test_klass,
                               data->pop_method,
                               method_arg,
                               was_popped_by_exception,
                               location);
  jnienv->DeleteLocalRef(method_arg);
}

extern "C" JNIEXPORT void JNICALL Java_art_FramePop_enableFramePopEvent(
    JNIEnv* env, jclass, jclass klass, jobject notify_method, jthread thr) {
  FramePopData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(FramePopData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(FramePopData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(klass));
  data->pop_method = env->FromReflectedMethod(notify_method);
  if (env->ExceptionCheck()) {
    return;
  }
  void* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(&old_data))) {
    return;
  } else if (old_data != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data))) {
    return;
  }
  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_generate_frame_pop_events = 1;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }
  current_callbacks.FramePop = framePopCB;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(&current_callbacks,
                                                         sizeof(current_callbacks)))) {
    return;
  }
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                            JVMTI_EVENT_FRAME_POP,
                                                            thr));
}

extern "C" JNIEXPORT jlong JNICALL Java_art_FramePop_makeJvmtiEnvForFramePop(JNIEnv* env, jclass) {
  JavaVM* vm;
  jvmtiEnv* out_jvmti_env = nullptr;
  if (env->GetJavaVM(&vm) != JNI_OK ||
      vm->GetEnv(reinterpret_cast<void**>(&out_jvmti_env), JVMTI_VERSION_1_0) != JNI_OK) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    if (rt_exception.get() == nullptr) {
      // CNFE should be pending.
      return 0L;
    }
    env->ThrowNew(rt_exception.get(), "Unable to create new jvmti_env");
    return 0L;
  }
  SetAllCapabilities(out_jvmti_env);
  return static_cast<jlong>(reinterpret_cast<intptr_t>(out_jvmti_env));
}

extern "C" JNIEXPORT void JNICALL Java_art_FramePop_notifyFramePop(
    JNIEnv* env, jclass, jthread thr, jint depth) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->NotifyFramePop(thr, depth));
}

}  // namespace common_frame_pop
}  // namespace art

