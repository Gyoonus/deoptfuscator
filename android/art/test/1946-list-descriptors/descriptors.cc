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
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test1946Descriptors {

typedef jvmtiError (*GetDescriptorList)(jvmtiEnv* env, jobject loader, jint* cnt, char*** descs);

struct DescriptorData {
  GetDescriptorList get_descriptor_list;
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

static void Cleanup(char** data, jint cnt) {
  for (jint i = 0; i < cnt; i++) {
    Dealloc(data[i]);
  }
  Dealloc(data);
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test1946_getClassloaderDescriptors(
    JNIEnv* env, jclass, jobject loader) {
  DescriptorData* data = nullptr;
  if (JvmtiErrorToException(
      env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)))) {
    return nullptr;
  }
  if (data == nullptr || data->get_descriptor_list == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Alloc tracking data not initialized.");
    return nullptr;
  }
  char** classes = nullptr;
  jint cnt = -1;
  if (JvmtiErrorToException(env, jvmti_env, data->get_descriptor_list(jvmti_env,
                                                                      loader,
                                                                      &cnt,
                                                                      &classes))) {
    return nullptr;
  }
  ScopedLocalRef<jobjectArray> arr(env, env->NewObjectArray(cnt,
                                                            env->FindClass("java/lang/String"),
                                                            nullptr));
  if (env->ExceptionCheck()) {
    Cleanup(classes, cnt);
    return nullptr;
  }

  for (jint i = 0; i < cnt; i++) {
    env->SetObjectArrayElement(arr.get(), i, env->NewStringUTF(classes[i]));
    if (env->ExceptionCheck()) {
      Cleanup(classes, cnt);
      return nullptr;
    }
  }
  Cleanup(classes, cnt);
  return arr.release();
}

static void DeallocParams(jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(params[i].name);
  }
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1946_initializeTest(JNIEnv* env, jclass) {
  void* old_data = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetEnvironmentLocalStorage(&old_data))) {
    return;
  } else if (old_data != nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Environment already has local storage set!");
    return;
  }
  DescriptorData* data = nullptr;
  if (JvmtiErrorToException(env,
                            jvmti_env,
                            jvmti_env->Allocate(sizeof(DescriptorData),
                                                reinterpret_cast<unsigned char**>(&data)))) {
    return;
  }
  memset(data, 0, sizeof(DescriptorData));
  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return;
  }
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.class.get_class_loader_class_descriptors", cur_info->id) == 0) {
      data->get_descriptor_list = reinterpret_cast<GetDescriptorList>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  if (data->get_descriptor_list == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), "Unable to find memory tracking extensions.");
    return;
  }
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEnvironmentLocalStorage(data));
  return;
}

}  // namespace Test1946Descriptors
}  // namespace art
