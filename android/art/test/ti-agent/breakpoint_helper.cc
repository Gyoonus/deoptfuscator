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

namespace common_breakpoint {

struct BreakpointData {
  jclass test_klass;
  jmethodID breakpoint_method;
  bool in_callback;
  bool allow_recursive;
};

extern "C" void breakpointCB(jvmtiEnv* jvmti,
                             JNIEnv* jnienv,
                             jthread thread,
                             jmethodID method,
                             jlocation location) {
  BreakpointData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (data->in_callback && !data->allow_recursive) {
    return;
  }
  data->in_callback = true;
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jnienv->CallStaticVoidMethod(data->test_klass,
                               data->breakpoint_method,
                               thread,
                               method_arg,
                               static_cast<jlong>(location));
  jnienv->DeleteLocalRef(method_arg);
  data->in_callback = false;
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Breakpoint_getLineNumberTableNative(
    JNIEnv* env,
    jclass k ATTRIBUTE_UNUSED,
    jobject target) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jint nlines;
  jvmtiLineNumberEntry* lines = nullptr;
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetLineNumberTable(method, &nlines, &lines))) {
    return nullptr;
  }
  jintArray lines_array = env->NewIntArray(nlines);
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(lines));
    return nullptr;
  }
  jlongArray locs_array = env->NewLongArray(nlines);
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(lines));
    return nullptr;
  }
  ScopedLocalRef<jclass> object_class(env, env->FindClass("java/lang/Object"));
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(lines));
    return nullptr;
  }
  jobjectArray ret = env->NewObjectArray(2, object_class.get(), nullptr);
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(lines));
    return nullptr;
  }
  jint* temp_lines = env->GetIntArrayElements(lines_array, /*isCopy*/nullptr);
  jlong* temp_locs = env->GetLongArrayElements(locs_array, /*isCopy*/nullptr);
  for (jint i = 0; i < nlines; i++) {
    temp_lines[i] = lines[i].line_number;
    temp_locs[i] = lines[i].start_location;
  }
  env->ReleaseIntArrayElements(lines_array, temp_lines, 0);
  env->ReleaseLongArrayElements(locs_array, temp_locs, 0);
  env->SetObjectArrayElement(ret, 0, locs_array);
  env->SetObjectArrayElement(ret, 1, lines_array);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(lines));
  return ret;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Breakpoint_getStartLocation(JNIEnv* env,
                                                                        jclass k ATTRIBUTE_UNUSED,
                                                                        jobject target) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return 0;
  }
  jlong start = 0;
  jlong end = end;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetMethodLocation(method, &start, &end));
  return start;
}

extern "C" JNIEXPORT void JNICALL Java_art_Breakpoint_clearBreakpoint(JNIEnv* env,
                                                                      jclass k ATTRIBUTE_UNUSED,
                                                                      jobject target,
                                                                      jlocation location) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->ClearBreakpoint(method, location));
}

extern "C" JNIEXPORT void JNICALL Java_art_Breakpoint_setBreakpoint(JNIEnv* env,
                                                                    jclass k ATTRIBUTE_UNUSED,
                                                                    jobject target,
                                                                    jlocation location) {
  jmethodID method = env->FromReflectedMethod(target);
  if (env->ExceptionCheck()) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetBreakpoint(method, location));
}

extern "C" JNIEXPORT void JNICALL Java_art_Breakpoint_startBreakpointWatch(
    JNIEnv* env,
    jclass k ATTRIBUTE_UNUSED,
    jclass method_klass,
    jobject method,
    jboolean allow_recursive,
    jthread thr) {
  BreakpointData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(BreakpointData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(BreakpointData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(method_klass));
  data->breakpoint_method = env->FromReflectedMethod(method);
  data->in_callback = false;
  data->allow_recursive = allow_recursive;

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
  current_callbacks.Breakpoint = breakpointCB;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(&current_callbacks,
                                                         sizeof(current_callbacks)))) {
    return;
  }
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_BREAKPOINT,
                                                                thr))) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Breakpoint_stopBreakpointWatch(
    JNIEnv* env,
    jclass k ATTRIBUTE_UNUSED,
    jthread thr) {
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_BREAKPOINT,
                                                                thr))) {
    return;
  }
}

}  // namespace common_breakpoint

}  // namespace art
