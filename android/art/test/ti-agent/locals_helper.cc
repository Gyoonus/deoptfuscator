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
namespace common_locals {

static void DeallocateContents(jvmtiLocalVariableEntry* vars, jint nvars) {
  for (jint i = 0; i < nvars; i++) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars[i].name));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars[i].signature));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars[i].generic_signature));
  }
}

extern "C" JNIEXPORT void Java_art_Locals_EnableLocalVariableAccess(JNIEnv* env, jclass) {
  jvmtiCapabilities caps;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetCapabilities(&caps))) {
    return;
  }
  caps.can_access_local_variables = 1;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps));
}

extern "C" JNIEXPORT void Java_art_Locals_SetLocalVariableObject(JNIEnv* env,
                                                                 jclass,
                                                                 jthread t,
                                                                 jint depth,
                                                                 jint slot,
                                                                 jobject val) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetLocalObject(t, depth, slot, val));
}

extern "C" JNIEXPORT void Java_art_Locals_SetLocalVariableDouble(JNIEnv* env,
                                                                 jclass,
                                                                 jthread t,
                                                                 jint depth,
                                                                 jint slot,
                                                                 jdouble val) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetLocalDouble(t, depth, slot, val));
}

extern "C" JNIEXPORT void Java_art_Locals_SetLocalVariableFloat(JNIEnv* env,
                                                                jclass,
                                                                jthread t,
                                                                jint depth,
                                                                jint slot,
                                                                jfloat val) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetLocalFloat(t, depth, slot, val));
}

extern "C" JNIEXPORT void Java_art_Locals_SetLocalVariableLong(JNIEnv* env,
                                                               jclass,
                                                               jthread t,
                                                               jint depth,
                                                               jint slot,
                                                               jlong val) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetLocalLong(t, depth, slot, val));
}

extern "C" JNIEXPORT void Java_art_Locals_SetLocalVariableInt(JNIEnv* env,
                                                              jclass,
                                                              jthread t,
                                                              jint depth,
                                                              jint slot,
                                                              jint val) {
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetLocalInt(t, depth, slot, val));
}

extern "C" JNIEXPORT jdouble Java_art_Locals_GetLocalVariableDouble(JNIEnv* env,
                                                                    jclass,
                                                                    jthread t,
                                                                    jint depth,
                                                                    jint slot) {
  jdouble ret = 0;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalDouble(t, depth, slot, &ret));
  return ret;
}

extern "C" JNIEXPORT jfloat Java_art_Locals_GetLocalVariableFloat(JNIEnv* env,
                                                                  jclass,
                                                                  jthread t,
                                                                  jint depth,
                                                                  jint slot) {
  jfloat ret = 0;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalFloat(t, depth, slot, &ret));
  return ret;
}

extern "C" JNIEXPORT jlong Java_art_Locals_GetLocalVariableLong(JNIEnv* env,
                                                                jclass,
                                                                jthread t,
                                                                jint depth,
                                                                jint slot) {
  jlong ret = 0;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalLong(t, depth, slot, &ret));
  return ret;
}

extern "C" JNIEXPORT jint Java_art_Locals_GetLocalVariableInt(JNIEnv* env,
                                                              jclass,
                                                              jthread t,
                                                              jint depth,
                                                              jint slot) {
  jint ret = 0;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalInt(t, depth, slot, &ret));
  return ret;
}

extern "C" JNIEXPORT jobject Java_art_Locals_GetLocalInstance(JNIEnv* env,
                                                              jclass,
                                                              jthread t,
                                                              jint depth) {
  jobject ret = nullptr;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalInstance(t, depth, &ret));
  return ret;
}

extern "C" JNIEXPORT jobject Java_art_Locals_GetLocalVariableObject(JNIEnv* env,
                                                                    jclass,
                                                                    jthread t,
                                                                    jint depth,
                                                                    jint slot) {
  jobject ret = nullptr;
  JvmtiErrorToException(env, jvmti_env, jvmti_env->GetLocalObject(t, depth, slot, &ret));
  return ret;
}

extern "C" JNIEXPORT jobjectArray Java_art_Locals_GetLocalVariableTable(JNIEnv* env,
                                                                        jclass,
                                                                        jobject m) {
  jmethodID method = env->FromReflectedMethod(m);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Locals$VariableDescription"));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jint nvars;
  jvmtiLocalVariableEntry* vars = nullptr;
  if (JvmtiErrorToException(env, jvmti_env,
                            jvmti_env->GetLocalVariableTable(method, &nvars, &vars))) {
    return nullptr;
  }
  jobjectArray vars_array = env->NewObjectArray(nvars, klass.get(), nullptr);
  if (env->ExceptionCheck()) {
    DeallocateContents(vars, nvars);
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars));
    return nullptr;
  }

  jmethodID constructor = env->GetMethodID(
      klass.get(), "<init>", "(JILjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  for (jint i = 0; i < nvars; i++) {
    ScopedLocalRef<jstring> name_string(env, env->NewStringUTF(vars[i].name));
    ScopedLocalRef<jstring> sig_string(env, env->NewStringUTF(vars[i].signature));
    ScopedLocalRef<jstring> generic_sig_string(env, env->NewStringUTF(vars[i].generic_signature));
    jobject var_obj = env->NewObject(klass.get(),
                                     constructor,
                                     vars[i].start_location,
                                     vars[i].length,
                                     name_string.get(),
                                     sig_string.get(),
                                     generic_sig_string.get(),
                                     vars[i].slot);
    if (env->ExceptionCheck()) {
      DeallocateContents(vars, nvars);
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars));
      return nullptr;
    }
    env->SetObjectArrayElement(vars_array, i, var_obj);
    if (env->ExceptionCheck()) {
      DeallocateContents(vars, nvars);
      jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars));
      return nullptr;
    }
  }

  DeallocateContents(vars, nvars);
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(vars));
  return vars_array;
}

}  // namespace common_locals
}  // namespace art
