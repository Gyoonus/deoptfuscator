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

#include "android-base/macros.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jni_helper.h"
#include "test_env.h"

namespace art {
namespace Test907GetLoadedClasses {

static jstring GetClassName(JNIEnv* jni_env, jclass cls) {
  ScopedLocalRef<jclass> class_class(jni_env, jni_env->GetObjectClass(cls));
  jmethodID mid = jni_env->GetMethodID(class_class.get(), "getName", "()Ljava/lang/String;");
  return reinterpret_cast<jstring>(jni_env->CallObjectMethod(cls, mid));
}

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test907_getLoadedClasses(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  jint count = -1;
  jclass* classes = nullptr;
  jvmtiError result = jvmti_env->GetLoadedClasses(&count, &classes);
  if (result != JVMTI_ERROR_NONE) {
    char* err;
    jvmti_env->GetErrorName(result, &err);
    printf("Failure running GetLoadedClasses: %s\n", err);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(err));
    return nullptr;
  }

  auto callback = [&](jint i) {
    jstring class_name = GetClassName(env, classes[i]);
    env->DeleteLocalRef(classes[i]);
    return class_name;
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/String", callback);

  // Need to Deallocate.
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(classes));

  return ret;
}

}  // namespace Test907GetLoadedClasses
}  // namespace art
