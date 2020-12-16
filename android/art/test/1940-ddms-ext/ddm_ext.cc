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

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "nativehelper/scoped_local_ref.h"
#include "nativehelper/scoped_primitive_array.h"
#include "test_env.h"

namespace art {
namespace Test1940DdmExt {

typedef jvmtiError (*DdmHandleChunk)(jvmtiEnv* env,
                                     jint type_in,
                                     jint len_in,
                                     const jbyte* data_in,
                                     jint* type_out,
                                     jint* len_data_out,
                                     jbyte** data_out);

struct DdmsTrackingData {
  DdmHandleChunk send_ddm_chunk;
  jclass test_klass;
  jmethodID publish_method;
};

template <typename T>
static void Dealloc(T* t) {
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(t));
}

template <typename T, typename ...Rest>
static void Dealloc(T* t, Rest... rs) {
  Dealloc(t);
  Dealloc(rs...);
}

extern "C" JNIEXPORT jobject JNICALL Java_art_Test1940_processChunk(JNIEnv* env,
                                                                    jclass,
                                                                    jobject chunk) {
  DdmsTrackingData* data = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return nullptr;
  }
  CHECK(chunk != nullptr);
  CHECK(data != nullptr);
  CHECK(data->send_ddm_chunk != nullptr);
  ScopedLocalRef<jclass> chunk_class(env, env->FindClass("org/apache/harmony/dalvik/ddmc/Chunk"));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jfieldID type_field_id = env->GetFieldID(chunk_class.get(), "type", "I");
  jfieldID offset_field_id = env->GetFieldID(chunk_class.get(), "offset", "I");
  jfieldID length_field_id = env->GetFieldID(chunk_class.get(), "length", "I");
  jfieldID data_field_id = env->GetFieldID(chunk_class.get(), "data", "[B");
  jint type = env->GetIntField(chunk, type_field_id);
  jint off = env->GetIntField(chunk, offset_field_id);
  jint len = env->GetIntField(chunk, length_field_id);
  ScopedLocalRef<jbyteArray> chunk_buf(
      env, reinterpret_cast<jbyteArray>(env->GetObjectField(chunk, data_field_id)));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  ScopedByteArrayRO byte_data(env, chunk_buf.get());
  jint out_type;
  jint out_size;
  jbyte* out_data;
  if (JvmtiErrorToException(env, jvmti_env, data->send_ddm_chunk(jvmti_env,
                                                                 type,
                                                                 len,
                                                                 &byte_data[off],
                                                                 /*out*/&out_type,
                                                                 /*out*/&out_size,
                                                                 /*out*/&out_data))) {
    return nullptr;
  } else {
    ScopedLocalRef<jbyteArray> chunk_data(env, env->NewByteArray(out_size));
    env->SetByteArrayRegion(chunk_data.get(), 0, out_size, out_data);
    Dealloc(out_data);
    ScopedLocalRef<jobject> res(env, env->NewObject(chunk_class.get(),
                                                    env->GetMethodID(chunk_class.get(),
                                                                     "<init>",
                                                                     "(I[BII)V"),
                                                    out_type,
                                                    chunk_data.get(),
                                                    0,
                                                    out_size));
    return res.release();
  }
}

static void DeallocParams(jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(params[i].name);
  }
}

static void JNICALL PublishCB(jvmtiEnv* jvmti, JNIEnv* jnienv, jint type, jint size, jbyte* bytes) {
  DdmsTrackingData* data = nullptr;
  if (JvmtiErrorToException(jnienv, jvmti,
                            jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return;
  }
  ScopedLocalRef<jbyteArray> res(jnienv, jnienv->NewByteArray(size));
  jnienv->SetByteArrayRegion(res.get(), 0, size, bytes);
  jnienv->CallStaticVoidMethod(data->test_klass, data->publish_method, type, res.get());
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1940_initializeTest(JNIEnv* env,
                                                                   jclass,
                                                                   jclass method_klass,
                                                                   jobject publish_method) {
  void* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(&old_data))) {
    return;
  } else if (old_data != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  DdmsTrackingData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(DdmsTrackingData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(DdmsTrackingData));
  data->test_klass = reinterpret_cast<jclass>(env->NewGlobalRef(method_klass));
  data->publish_method = env->FromReflectedMethod(publish_method);
  if (env->ExceptionCheck()) {
    return;
  }
  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return;
  }
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.internal.ddm.process_chunk", cur_info->id) == 0) {
      data->send_ddm_chunk = reinterpret_cast<DdmHandleChunk>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  if (data->send_ddm_chunk == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to find memory tracking extensions.");
    return;
  }
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data))) {
    return;
  }

  jint event_index = -1;
  bool found_event = false;
  jvmtiExtensionEventInfo* events = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionEvents(&n_ext, &events))) {
    return;
  }
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionEventInfo* cur_info = &events[i];
    if (strcmp("com.android.art.internal.ddm.publish_chunk", cur_info->id) == 0) {
      found_event = true;
      event_index = cur_info->extension_event_index;
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params);
  }
  // Cleanup the array.
  Dealloc(events);
  if (!found_event) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to find ddms extension event.");
    return;
  }
  JvmtiErrorToException(env,
                        jvmti_env,
                        jvmti_env->SetExtensionEventCallback(
                            event_index, reinterpret_cast<jvmtiExtensionEvent>(PublishCB)));
  return;
}

}  // namespace Test1940DdmExt
}  // namespace art
