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

#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "android-base/logging.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"
#include "scoped_primitive_array.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test903HelloTagging {

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test903_getTaggedObjects(
    JNIEnv* env, jclass, jlongArray searchTags, jboolean returnObjects, jboolean returnTags) {
  ScopedLongArrayRO scoped_array(env);
  if (searchTags != nullptr) {
    scoped_array.reset(searchTags);
  }
  const jlong* tag_ptr = scoped_array.get();
  if (tag_ptr == nullptr) {
    // Can never pass null.
    tag_ptr = reinterpret_cast<const jlong*>(1);
  }

  jint result_count;
  jobject* result_object_array;
  jobject** result_object_array_ptr = returnObjects == JNI_TRUE ? &result_object_array : nullptr;
  jlong* result_tag_array;
  jlong** result_tag_array_ptr = returnTags == JNI_TRUE ? &result_tag_array : nullptr;

  jvmtiError ret = jvmti_env->GetObjectsWithTags(scoped_array.size(),
                                                 tag_ptr,
                                                 &result_count,
                                                 result_object_array_ptr,
                                                 result_tag_array_ptr);
  if (JvmtiErrorToException(env, jvmti_env, ret)) {
    return nullptr;
  }

  CHECK_GE(result_count, 0);

  ScopedLocalRef<jclass> obj_class(env, env->FindClass("java/lang/Object"));
  if (obj_class.get() == nullptr) {
    return nullptr;
  }

  jobjectArray resultObjectArray = nullptr;
  if (returnObjects == JNI_TRUE) {
    resultObjectArray = env->NewObjectArray(result_count, obj_class.get(), nullptr);
    if (resultObjectArray == nullptr) {
      return nullptr;
    }
    for (jint i = 0; i < result_count; ++i) {
      env->SetObjectArrayElement(resultObjectArray, i, result_object_array[i]);
    }
  }

  jlongArray resultTagArray = nullptr;
  if (returnTags == JNI_TRUE) {
    resultTagArray = env->NewLongArray(result_count);
    env->SetLongArrayRegion(resultTagArray, 0, result_count, result_tag_array);
  }

  jobject count_integer;
  {
    ScopedLocalRef<jclass> integer_class(env, env->FindClass("java/lang/Integer"));
    jmethodID methodID = env->GetMethodID(integer_class.get(), "<init>", "(I)V");
    count_integer = env->NewObject(integer_class.get(), methodID, result_count);
    if (count_integer == nullptr) {
      return nullptr;
    }
  }

  jobjectArray resultArray = env->NewObjectArray(3, obj_class.get(), nullptr);
  if (resultArray == nullptr) {
    return nullptr;
  }
  env->SetObjectArrayElement(resultArray, 0, resultObjectArray);
  env->SetObjectArrayElement(resultArray, 1, resultTagArray);
  env->SetObjectArrayElement(resultArray, 2, count_integer);

  return resultArray;
}

static jvmtiEnv* CreateJvmtiEnv(JNIEnv* env) {
  JavaVM* jvm;
  CHECK_EQ(0, env->GetJavaVM(&jvm));

  jvmtiEnv* new_jvmti_env;
  CHECK_EQ(0, jvm->GetEnv(reinterpret_cast<void**>(&new_jvmti_env), JVMTI_VERSION_1_0));

  jvmtiCapabilities capa;
  memset(&capa, 0, sizeof(jvmtiCapabilities));
  capa.can_tag_objects = 1;
  jvmtiError error = new_jvmti_env->AddCapabilities(&capa);
  CHECK_EQ(JVMTI_ERROR_NONE, error);

  return new_jvmti_env;
}

static void SetTag(jvmtiEnv* env, jobject obj, jlong tag) {
  jvmtiError ret = env->SetTag(obj, tag);
  CHECK_EQ(JVMTI_ERROR_NONE, ret);
}

static jlong GetTag(jvmtiEnv* env, jobject obj) {
  jlong tag;
  jvmtiError ret = env->GetTag(obj, &tag);
  CHECK_EQ(JVMTI_ERROR_NONE, ret);
  return tag;
}

extern "C" JNIEXPORT jlongArray JNICALL Java_art_Test903_testTagsInDifferentEnvs(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject obj, jlong base_tag, jint count) {
  std::unique_ptr<jvmtiEnv*[]> envs = std::unique_ptr<jvmtiEnv*[]>(new jvmtiEnv*[count]);
  envs[0] = jvmti_env;
  for (int32_t i = 1; i != count; ++i) {
    envs[i] = CreateJvmtiEnv(env);
  }

  for (int32_t i = 0; i != count; ++i) {
    SetTag(envs[i], obj, base_tag + i);
  }
  std::unique_ptr<jlong[]> vals = std::unique_ptr<jlong[]>(new jlong[count]);
  for (int32_t i = 0; i != count; ++i) {
    vals[i] = GetTag(envs[i], obj);
  }

  for (int32_t i = 1; i != count; ++i) {
    CHECK_EQ(JVMTI_ERROR_NONE, envs[i]->DisposeEnvironment());
  }

  jlongArray res = env->NewLongArray(count);
  if (res == nullptr) {
    return nullptr;
  }
  env->SetLongArrayRegion(res, 0, count, vals.get());
  return res;
}

}  // namespace Test903HelloTagging
}  // namespace art
