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

#include <stdio.h>

#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_helper.h"
#include "test_env.h"

namespace art {
namespace Test918Fields {

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test918_getFieldName(
    JNIEnv* env, jclass klass, jobject field) {
  jfieldID id = env->FromReflectedField(field);

  char* name;
  char* sig;
  char* gen;
  // Note: technically putting the caller class here is wrong, but we don't need it, anyways.
  jvmtiError result = jvmti_env->GetFieldName(klass, id, &name, &sig, &gen);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetFieldName: %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return nullptr;
  }

  auto callback = [&](jint i) {
    if (i == 0) {
      return name == nullptr ? nullptr : env->NewStringUTF(name);
    } else if (i == 1) {
      return sig == nullptr ? nullptr : env->NewStringUTF(sig);
    } else {
      return gen == nullptr ? nullptr : env->NewStringUTF(gen);
    }
  };
  jobjectArray ret = CreateObjectArray(env, 3, "java/lang/String", callback);

  // Need to deallocate the strings.
  if (name != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(name));
  }
  if (sig != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(sig));
  }
  if (gen != nullptr) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(gen));
  }

  // Also run GetMethodName with all parameter pointers null to check for segfaults.
  jvmtiError result2 = jvmti_env->GetFieldName(klass, id, nullptr, nullptr, nullptr);
  if (result2 != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result2, &err);
    printf("Failure running GetFieldName(null, null, null): %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return nullptr;
  }

  return ret;
}

extern "C" JNIEXPORT jclass JNICALL Java_art_Test918_getFieldDeclaringClass(
    JNIEnv* env, jclass klass, jobject field) {
  jfieldID id = env->FromReflectedField(field);

  jclass declaring_class;
  jvmtiError result = jvmti_env->GetFieldDeclaringClass(klass, id, &declaring_class);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetFieldDeclaringClass: %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return nullptr;
  }

  return declaring_class;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test918_getFieldModifiers(
    JNIEnv* env, jclass klass, jobject field) {
  jfieldID id = env->FromReflectedField(field);

  jint modifiers;
  jvmtiError result = jvmti_env->GetFieldModifiers(klass, id, &modifiers);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetFieldModifiers: %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return 0;
  }

  return modifiers;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test918_isFieldSynthetic(
    JNIEnv* env, jclass klass, jobject field) {
  jfieldID id = env->FromReflectedField(field);

  jboolean synth;
  jvmtiError result = jvmti_env->IsFieldSynthetic(klass, id, &synth);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running IsFieldSynthetic: %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return 0;
  }

  return synth;
}

}  // namespace Test918Fields
}  // namespace art
