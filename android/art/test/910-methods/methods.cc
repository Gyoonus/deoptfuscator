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
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test910Methods {

extern "C" JNIEXPORT jobjectArray JNICALL Java_art_Test910_getMethodName(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  char* name;
  char* sig;
  char* gen;
  jvmtiError result = jvmti_env->GetMethodName(id, &name, &sig, &gen);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
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
  jvmtiError result2 = jvmti_env->GetMethodName(id, nullptr, nullptr, nullptr);
  if (JvmtiErrorToException(env, jvmti_env, result2)) {
    return nullptr;
  }

  return ret;
}

extern "C" JNIEXPORT jclass JNICALL Java_art_Test910_getMethodDeclaringClass(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jclass declaring_class;
  jvmtiError result = jvmti_env->GetMethodDeclaringClass(id, &declaring_class);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return nullptr;
  }

  return declaring_class;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test910_getMethodModifiers(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jint modifiers;
  jvmtiError result = jvmti_env->GetMethodModifiers(id, &modifiers);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return 0;
  }

  return modifiers;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test910_getMaxLocals(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jint max_locals;
  jvmtiError result = jvmti_env->GetMaxLocals(id, &max_locals);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }

  return max_locals;
}

extern "C" JNIEXPORT jint JNICALL Java_art_Test910_getArgumentsSize(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jint arguments;
  jvmtiError result = jvmti_env->GetArgumentsSize(id, &arguments);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }

  return arguments;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test910_getMethodLocationStart(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jlong start;
  jlong end;
  jvmtiError result = jvmti_env->GetMethodLocation(id, &start, &end);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }

  return start;
}

extern "C" JNIEXPORT jlong JNICALL Java_art_Test910_getMethodLocationEnd(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jlong start;
  jlong end;
  jvmtiError result = jvmti_env->GetMethodLocation(id, &start, &end);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return -1;
  }

  return end;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test910_isMethodNative(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jboolean is_native;
  jvmtiError result = jvmti_env->IsMethodNative(id, &is_native);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return JNI_FALSE;
  }

  return is_native;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test910_isMethodObsolete(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jboolean is_obsolete;
  jvmtiError result = jvmti_env->IsMethodObsolete(id, &is_obsolete);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return JNI_FALSE;
  }

  return is_obsolete;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test910_isMethodSynthetic(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jobject method) {
  jmethodID id = env->FromReflectedMethod(method);

  jboolean is_synthetic;
  jvmtiError result = jvmti_env->IsMethodSynthetic(id, &is_synthetic);
  if (JvmtiErrorToException(env, jvmti_env, result)) {
    return JNI_FALSE;
  }

  return is_synthetic;
}

}  // namespace Test910Methods
}  // namespace art
