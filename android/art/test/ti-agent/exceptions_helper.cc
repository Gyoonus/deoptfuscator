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

namespace common_exceptions {

struct ExceptionsData {
  jclass test_klass;
  jclass exception_klass;
  jmethodID exception_event;
  jmethodID exception_catch_event;
};

static void exceptionCB(jvmtiEnv* jvmti,
                        JNIEnv* jnienv,
                        jthread thread,
                        jmethodID throw_method,
                        jlocation throw_location,
                        jobject throwable,
                        jmethodID catch_method,
                        jlocation catch_location) {
  ExceptionsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  DCHECK(throwable != nullptr);
  if (!jnienv->IsInstanceOf(throwable, data->exception_klass)) {
    return;
  }
  jthrowable e = jnienv->ExceptionOccurred();
  jnienv->ExceptionClear();
  CHECK(data->exception_event != nullptr);
  jobject throw_method_arg = GetJavaMethod(jvmti, jnienv, throw_method);
  jobject catch_method_arg =
      catch_method != nullptr ? GetJavaMethod(jvmti, jnienv, catch_method) : nullptr;
  jnienv->CallStaticVoidMethod(data->test_klass,
                               data->exception_event,
                               thread,
                               throw_method_arg,
                               static_cast<jlong>(throw_location),
                               throwable,
                               catch_method_arg,
                               static_cast<jlong>(catch_location));
  jnienv->DeleteLocalRef(throw_method_arg);
  if (catch_method_arg != nullptr) {
    jnienv->DeleteLocalRef(catch_method_arg);
  }
  if (e != nullptr) {
    jnienv->Throw(e);
  }
}


static void exceptionCatchCB(jvmtiEnv* jvmti,
                             JNIEnv* jnienv,
                             jthread thread,
                             jmethodID catch_method,
                             jlocation catch_location,
                             jobject throwable) {
  ExceptionsData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (!jnienv->IsSameObject(data->exception_klass, jnienv->GetObjectClass(throwable))) {
    return;
  }
  jthrowable e = jnienv->ExceptionOccurred();
  jnienv->ExceptionClear();
  CHECK(data->exception_catch_event != nullptr);
  jobject catch_method_arg = GetJavaMethod(jvmti, jnienv, catch_method);
  jnienv->CallStaticVoidMethod(data->test_klass,
                               data->exception_catch_event,
                               thread,
                               catch_method_arg,
                               static_cast<jlong>(catch_location),
                               throwable);
  jnienv->DeleteLocalRef(catch_method_arg);
  if (e != nullptr) {
    jnienv->Throw(e);
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Exceptions_setupExceptionTracing(
    JNIEnv* env,
    jclass exception ATTRIBUTE_UNUSED,
    jclass klass,
    jclass except,
    jobject exception_event,
    jobject exception_catch_event) {
  ExceptionsData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(ExceptionsData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_generate_exception_events = 1;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }
  memset(data, 0, sizeof(ExceptionsData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(klass));
  data->exception_klass = reinterpret_cast<jclass>(env->NewGlobalRef(except));
  data->exception_event =
      exception_event != nullptr ?  env->FromReflectedMethod(exception_event) : nullptr;
  data->exception_catch_event =
      exception_catch_event != nullptr ? env->FromReflectedMethod(exception_catch_event) : nullptr;

  ExceptionsData* old_data = nullptr;
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

  current_callbacks.Exception = exceptionCB;
  current_callbacks.ExceptionCatch = exceptionCatchCB;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(&current_callbacks,
                                                         sizeof(current_callbacks)))) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Exceptions_enableExceptionCatchEvent(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                            JVMTI_EVENT_EXCEPTION_CATCH,
                                                            thr));
}

extern "C" JNIEXPORT void JNICALL Java_art_Exceptions_enableExceptionEvent(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                            JVMTI_EVENT_EXCEPTION,
                                                            thr));
}

extern "C" JNIEXPORT void JNICALL Java_art_Exceptions_disableExceptionCatchEvent(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                            JVMTI_EVENT_EXCEPTION_CATCH,
                                                            thr));
}

extern "C" JNIEXPORT void JNICALL Java_art_Exceptions_disableExceptionEvent(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr) {
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                            JVMTI_EVENT_EXCEPTION,
                                                            thr));
}

}  // namespace common_exceptions


}  // namespace art
