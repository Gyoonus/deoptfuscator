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

#include <stdio.h>

#include "android-base/macros.h"
#include "jni.h"
#include "jvmti.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test922Properties {

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test922_getSystemProperties(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED) {
  jint count;
  char** properties;
  jvmtiError result = jvmti_env->GetSystemProperties(&count, &properties);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  auto callback = [&](jint i) -> jstring {
    char* data = properties[i];
    if (data == nullptr) {
      return nullptr;
    }
    jstring ret = env->NewStringUTF(data);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(data));
    return ret;
  };
  jobjectArray ret = CreateObjectArray(env, count, "java/lang/String", callback);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(properties));

  return ret;
}

extern "C" JNIEXPORT jstring JNICALL Java_art_Test922_getSystemProperty(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jstring key) {
  ScopedUtfChars string(env, key);
  if (string.c_str() == nullptr) {
    return nullptr;
  }

  char* value = nullptr;
  jvmtiError result = jvmti_env->GetSystemProperty(string.c_str(), &value);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  jstring ret = (value == nullptr) ? nullptr : env->NewStringUTF(value);

  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(value));

  return ret;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test922_setSystemProperty(
    JNIEnv* env, jclass Main_klass ATTRIBUTE_UNUSED, jstring key, jstring value) {
  ScopedUtfChars key_string(env, key);
  if (key_string.c_str() == nullptr) {
    return;
  }
  ScopedUtfChars value_string(env, value);
  if (value_string.c_str() == nullptr) {
    return;
  }

  jvmtiError result = jvmti_env->SetSystemProperty(key_string.c_str(), value_string.c_str());
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return;
  }
}

}  // namespace Test922Properties
}  // namespace art
