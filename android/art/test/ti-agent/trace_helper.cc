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

namespace common_trace {

static bool IsInCallback(JNIEnv* env, jvmtiEnv *jvmti, jthread thr) {
  void* data;
  ScopedLocalRef<jthrowable> exc(env, env->ExceptionOccurred());
  env->ExceptionClear();
  jvmti->GetThreadLocalStorage(thr, &data);
  if (exc.get() != nullptr) {
    env->Throw(exc.get());
  }
  if (data == nullptr) {
    return false;
  } else {
    return true;
  }
}

static void SetInCallback(JNIEnv* env, jvmtiEnv *jvmti, jthread thr, bool val) {
  ScopedLocalRef<jthrowable> exc(env, env->ExceptionOccurred());
  env->ExceptionClear();
  jvmti->SetThreadLocalStorage(thr, (val ? reinterpret_cast<void*>(0x1)
                                         : reinterpret_cast<void*>(0x0)));
  if (exc.get() != nullptr) {
    env->Throw(exc.get());
  }
}

class ScopedCallbackState {
 public:
  ScopedCallbackState(JNIEnv* jnienv, jvmtiEnv* env, jthread thr)
      : jnienv_(jnienv), env_(env), thr_(thr) {
    CHECK(!IsInCallback(jnienv_, env_, thr_));
    SetInCallback(jnienv_, env_, thr_, true);
  }
  ~ScopedCallbackState() {
    CHECK(IsInCallback(jnienv_, env_, thr_));
    SetInCallback(jnienv_, env_, thr_, false);
  }

 private:
  JNIEnv* jnienv_;
  jvmtiEnv* env_;
  jthread thr_;
};

struct TraceData {
  jclass test_klass;
  jmethodID enter_method;
  jmethodID exit_method;
  jmethodID field_access;
  jmethodID field_modify;
  jmethodID single_step;
  jmethodID thread_start;
  jmethodID thread_end;
  bool access_watch_on_load;
  bool modify_watch_on_load;
  jrawMonitorID trace_mon;

  jclass GetTestClass(jvmtiEnv* jvmti, JNIEnv* env) {
    if (JvmtiErrorToException(env, jvmti, jvmti->RawMonitorEnter(trace_mon))) {
      return nullptr;
    }
    jclass out = reinterpret_cast<jclass>(env->NewLocalRef(test_klass));
    if (JvmtiErrorToException(env, jvmti, jvmti->RawMonitorExit(trace_mon))) {
      return nullptr;
    }
    return out;
  }
};

static void threadStartCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thread) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->thread_start != nullptr);
  jnienv->CallStaticVoidMethod(klass.get(), data->thread_start, thread);
}
static void threadEndCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thread) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->thread_end != nullptr);
  jnienv->CallStaticVoidMethod(klass.get(), data->thread_end, thread);
}

static void singleStepCB(jvmtiEnv* jvmti,
                         JNIEnv* jnienv,
                         jthread thread,
                         jmethodID method,
                         jlocation location) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (IsInCallback(jnienv, jvmti, thread)) {
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->single_step != nullptr);
  ScopedCallbackState st(jnienv, jvmti, thread);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jnienv->CallStaticVoidMethod(klass.get(),
                               data->single_step,
                               thread,
                               method_arg,
                               static_cast<jlong>(location));
  jnienv->DeleteLocalRef(method_arg);
}

static void fieldAccessCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thr,
                          jmethodID method,
                          jlocation location,
                          jclass field_klass,
                          jobject object,
                          jfieldID field) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (IsInCallback(jnienv, jvmti, thr)) {
    // Don't do callback for either of these to prevent an infinite loop.
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->field_access != nullptr);
  ScopedCallbackState st(jnienv, jvmti, thr);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jobject field_arg = GetJavaField(jvmti, jnienv, field_klass, field);
  jnienv->CallStaticVoidMethod(klass.get(),
                               data->field_access,
                               method_arg,
                               static_cast<jlong>(location),
                               field_klass,
                               object,
                               field_arg);
  jnienv->DeleteLocalRef(method_arg);
  jnienv->DeleteLocalRef(field_arg);
}

static void fieldModificationCB(jvmtiEnv* jvmti,
                                JNIEnv* jnienv,
                                jthread thr,
                                jmethodID method,
                                jlocation location,
                                jclass field_klass,
                                jobject object,
                                jfieldID field,
                                char type_char,
                                jvalue new_value) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (IsInCallback(jnienv, jvmti, thr)) {
    // Don't do callback recursively to prevent an infinite loop.
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->field_modify != nullptr);
  ScopedCallbackState st(jnienv, jvmti, thr);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jobject field_arg = GetJavaField(jvmti, jnienv, field_klass, field);
  jobject value = GetJavaValueByType(jnienv, type_char, new_value);
  if (jnienv->ExceptionCheck()) {
    jnienv->DeleteLocalRef(method_arg);
    jnienv->DeleteLocalRef(field_arg);
    return;
  }
  jnienv->CallStaticVoidMethod(klass.get(),
                               data->field_modify,
                               method_arg,
                               static_cast<jlong>(location),
                               field_klass,
                               object,
                               field_arg,
                               value);
  jnienv->DeleteLocalRef(method_arg);
  jnienv->DeleteLocalRef(field_arg);
}

static void methodExitCB(jvmtiEnv* jvmti,
                         JNIEnv* jnienv,
                         jthread thr,
                         jmethodID method,
                         jboolean was_popped_by_exception,
                         jvalue return_value) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (method == data->exit_method ||
      method == data->enter_method ||
      IsInCallback(jnienv, jvmti, thr)) {
    // Don't do callback for either of these to prevent an infinite loop.
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  CHECK(data->exit_method != nullptr);
  ScopedCallbackState st(jnienv, jvmti, thr);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  jobject result =
      was_popped_by_exception ? nullptr : GetJavaValue(jvmti, jnienv, method, return_value);
  if (jnienv->ExceptionCheck()) {
    return;
  }
  jnienv->CallStaticVoidMethod(klass.get(),
                               data->exit_method,
                               method_arg,
                               was_popped_by_exception,
                               result);
  jnienv->DeleteLocalRef(method_arg);
}

static void methodEntryCB(jvmtiEnv* jvmti,
                          JNIEnv* jnienv,
                          jthread thr,
                          jmethodID method) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  CHECK(data->enter_method != nullptr);
  if (method == data->exit_method ||
      method == data->enter_method ||
      IsInCallback(jnienv, jvmti, thr)) {
    // Don't do callback for either of these to prevent an infinite loop.
    return;
  }
  ScopedLocalRef<jclass> klass(jnienv, data->GetTestClass(jvmti, jnienv));
  if (klass.get() == nullptr) {
    return;
  }
  ScopedCallbackState st(jnienv, jvmti, thr);
  jobject method_arg = GetJavaMethod(jvmti, jnienv, method);
  if (jnienv->ExceptionCheck()) {
    return;
  }
  jnienv->CallStaticVoidMethod(klass.get(), data->enter_method, method_arg);
  jnienv->DeleteLocalRef(method_arg);
}

static void classPrepareCB(jvmtiEnv* jvmti,
                           JNIEnv* jnienv,
                           jthread thr ATTRIBUTE_UNUSED,
                           jclass klass) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (data->access_watch_on_load || data->modify_watch_on_load) {
    jint nfields;
    jfieldID* fields;
    if (JvmtiErrorToException(jnienv, jvmti, jvmti->GetClassFields(klass, &nfields, &fields))) {
      return;
    }
    for (jint i = 0; i < nfields; i++) {
      jfieldID f = fields[i];
      // Ignore errors
      if (data->access_watch_on_load) {
        jvmti->SetFieldAccessWatch(klass, f);
      }

      if (data->modify_watch_on_load) {
        jvmti->SetFieldModificationWatch(klass, f);
      }
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(fields));
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_watchAllFieldAccesses(JNIEnv* env) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  data->access_watch_on_load = true;
  // We need the classPrepareCB to watch new fields as the classes are loaded/prepared.
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_CLASS_PREPARE,
                                                                nullptr))) {
    return;
  }
  jint nklasses;
  jclass* klasses;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLoadedClasses(&nklasses, &klasses))) {
    return;
  }
  for (jint i = 0; i < nklasses; i++) {
    jclass k = klasses[i];

    jint nfields;
    jfieldID* fields;
    jvmtiError err = jvmti_env->GetClassFields(k, &nfields, &fields);
    if (err == JVMTI_ERROR_CLASS_NOT_PREPARED) {
      continue;
    } else if (JvmtiErrorToException(env, jvmti_env, err)) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(klasses));
      return;
    }
    for (jint j = 0; j < nfields; j++) {
      jvmti_env->SetFieldAccessWatch(k, fields[j]);
    }
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(fields));
  }
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(klasses));
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_watchAllFieldModifications(JNIEnv* env) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  data->modify_watch_on_load = true;
  // We need the classPrepareCB to watch new fields as the classes are loaded/prepared.
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_CLASS_PREPARE,
                                                                nullptr))) {
    return;
  }
  jint nklasses;
  jclass* klasses;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLoadedClasses(&nklasses, &klasses))) {
    return;
  }
  for (jint i = 0; i < nklasses; i++) {
    jclass k = klasses[i];

    jint nfields;
    jfieldID* fields;
    jvmtiError err = jvmti_env->GetClassFields(k, &nfields, &fields);
    if (err == JVMTI_ERROR_CLASS_NOT_PREPARED) {
      continue;
    } else if (JvmtiErrorToException(env, jvmti_env, err)) {
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(klasses));
      return;
    }
    for (jint j = 0; j < nfields; j++) {
      jvmti_env->SetFieldModificationWatch(k, fields[j]);
    }
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(fields));
  }
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(klasses));
}

static bool GetFieldAndClass(JNIEnv* env,
                             jobject ref_field,
                             jclass* out_klass,
                             jfieldID* out_field) {
  *out_field = env->FromReflectedField(ref_field);
  if (env->ExceptionCheck()) {
    return false;
  }
  jclass field_klass = env->FindClass("java/lang/reflect/Field");
  if (env->ExceptionCheck()) {
    return false;
  }
  jmethodID get_declaring_class_method =
      env->GetMethodID(field_klass, "getDeclaringClass", "()Ljava/lang/Class;");
  if (env->ExceptionCheck()) {
    env->DeleteLocalRef(field_klass);
    return false;
  }
  *out_klass = static_cast<jclass>(env->CallObjectMethod(ref_field, get_declaring_class_method));
  if (env->ExceptionCheck()) {
    *out_klass = nullptr;
    env->DeleteLocalRef(field_klass);
    return false;
  }
  env->DeleteLocalRef(field_klass);
  return true;
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_watchFieldModification(
    JNIEnv* env,
    jclass trace ATTRIBUTE_UNUSED,
    jobject field_obj) {
  jfieldID field;
  jclass klass;
  if (!GetFieldAndClass(env, field_obj, &klass, &field)) {
    return;
  }

  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetFieldModificationWatch(klass, field));
  env->DeleteLocalRef(klass);
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_watchFieldAccess(
    JNIEnv* env,
    jclass trace ATTRIBUTE_UNUSED,
    jobject field_obj) {
  jfieldID field;
  jclass klass;
  if (!GetFieldAndClass(env, field_obj, &klass, &field)) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetFieldAccessWatch(klass, field));
  env->DeleteLocalRef(klass);
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_enableTracing2(
    JNIEnv* env,
    jclass trace ATTRIBUTE_UNUSED,
    jclass klass,
    jobject enter,
    jobject exit,
    jobject field_access,
    jobject field_modify,
    jobject single_step,
    jobject thread_start,
    jobject thread_end,
    jthread thr) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(TraceData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(TraceData));
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->CreateRawMonitor("Trace monitor", &data->trace_mon))) {
    return;
  }
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(klass));
  data->enter_method = enter != nullptr ? env->FromReflectedMethod(enter) : nullptr;
  data->exit_method = exit != nullptr ? env->FromReflectedMethod(exit) : nullptr;
  data->field_access = field_access != nullptr ? env->FromReflectedMethod(field_access) : nullptr;
  data->field_modify = field_modify != nullptr ? env->FromReflectedMethod(field_modify) : nullptr;
  data->single_step = single_step != nullptr ? env->FromReflectedMethod(single_step) : nullptr;
  data->thread_start = thread_start != nullptr ? env->FromReflectedMethod(thread_start) : nullptr;
  data->thread_end = thread_end != nullptr ? env->FromReflectedMethod(thread_end) : nullptr;

  TraceData* old_data = nullptr;
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

  current_callbacks.MethodEntry = methodEntryCB;
  current_callbacks.MethodExit = methodExitCB;
  current_callbacks.FieldAccess = fieldAccessCB;
  current_callbacks.FieldModification = fieldModificationCB;
  current_callbacks.ClassPrepare = classPrepareCB;
  current_callbacks.SingleStep = singleStepCB;
  current_callbacks.ThreadStart = threadStartCB;
  current_callbacks.ThreadEnd = threadEndCB;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventCallbacks(&current_callbacks,
                                                         sizeof(current_callbacks)))) {
    return;
  }
  if (enter != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_METHOD_ENTRY,
                                                                thr))) {
    return;
  }
  if (exit != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_METHOD_EXIT,
                                                                thr))) {
    return;
  }
  if (field_access != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_FIELD_ACCESS,
                                                                thr))) {
    return;
  }
  if (field_modify != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_FIELD_MODIFICATION,
                                                                thr))) {
    return;
  }
  if (single_step != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_SINGLE_STEP,
                                                                thr))) {
    return;
  }
  if (thread_start != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_THREAD_START,
                                                                thr))) {
    return;
  }
  if (thread_end != nullptr &&
      JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_ENABLE,
                                                                JVMTI_EVENT_THREAD_END,
                                                                thr))) {
    return;
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_enableTracing(
    JNIEnv* env,
    jclass trace,
    jclass klass,
    jobject enter,
    jobject exit,
    jobject field_access,
    jobject field_modify,
    jobject single_step,
    jthread thr) {
  Java_art_Trace_enableTracing2(env,
                                trace,
                                klass,
                                enter,
                                exit,
                                field_access,
                                field_modify,
                                single_step,
                                /* thread_start */ nullptr,
                                /* thread_end */ nullptr,
                                thr);
  return;
}

extern "C" JNIEXPORT void JNICALL Java_art_Trace_disableTracing(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr) {
  TraceData* data = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  // If data is null then we haven't ever enabled tracing so we don't need to do anything.
  if (data == nullptr || data->test_klass == nullptr) {
    return;
  }
  ScopedLocalRef<jthrowable> err(env, nullptr);
  // First disable all the events.
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_FIELD_ACCESS,
                                                                thr))) {
    env->ExceptionDescribe();
    err.reset(env->ExceptionOccurred());
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_FIELD_MODIFICATION,
                                                                thr))) {
    env->ExceptionDescribe();
    err.reset(env->ExceptionOccurred());
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_METHOD_ENTRY,
                                                                thr))) {
    env->ExceptionDescribe();
    err.reset(env->ExceptionOccurred());
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_METHOD_EXIT,
                                                                thr))) {
    env->ExceptionDescribe();
    err.reset(env->ExceptionOccurred());
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->SetEventNotificationMode(JVMTI_DISABLE,
                                                                JVMTI_EVENT_SINGLE_STEP,
                                                                thr))) {
    env->ExceptionDescribe();
    err.reset(env->ExceptionOccurred());
    env->ExceptionClear();
  }
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->RawMonitorEnter(data->trace_mon))) {
    return;
  }
  // Clear test_klass so we know this isn't being used
  env->DeleteGlobalRef(data->test_klass);
  data->test_klass = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->RawMonitorExit(data->trace_mon))) {
    return;
  }
  if (err.get() != nullptr) {
    env->Throw(err.get());
  }
}

}  // namespace common_trace


}  // namespace art
