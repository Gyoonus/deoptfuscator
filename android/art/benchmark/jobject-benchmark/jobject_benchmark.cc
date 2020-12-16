/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "jni.h"

#include "java_vm_ext.h"
#include "mirror/class-inl.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace {

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeAddRemoveLocal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  for (jint i = 0; i < reps; ++i) {
    jobject ref = soa.Env()->AddLocalReference<jobject>(obj);
    soa.Env()->DeleteLocalRef(ref);
  }
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeDecodeLocal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  jobject ref = soa.Env()->AddLocalReference<jobject>(obj);
  for (jint i = 0; i < reps; ++i) {
    CHECK_EQ(soa.Decode<mirror::Object>(ref), obj);
  }
  soa.Env()->DeleteLocalRef(ref);
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeAddRemoveGlobal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  for (jint i = 0; i < reps; ++i) {
    jobject ref = soa.Vm()->AddGlobalRef(soa.Self(), obj);
    soa.Vm()->DeleteGlobalRef(soa.Self(), ref);
  }
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeDecodeGlobal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  jobject ref = soa.Vm()->AddGlobalRef(soa.Self(), obj);
  for (jint i = 0; i < reps; ++i) {
    CHECK_EQ(soa.Decode<mirror::Object>(ref), obj);
  }
  soa.Vm()->DeleteGlobalRef(soa.Self(), ref);
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeAddRemoveWeakGlobal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  for (jint i = 0; i < reps; ++i) {
    jobject ref = soa.Vm()->AddWeakGlobalRef(soa.Self(), obj);
    soa.Vm()->DeleteWeakGlobalRef(soa.Self(), ref);
  }
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeDecodeWeakGlobal(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Object> obj = soa.Decode<mirror::Object>(jobj);
  CHECK(obj != nullptr);
  jobject ref = soa.Vm()->AddWeakGlobalRef(soa.Self(), obj);
  for (jint i = 0; i < reps; ++i) {
    CHECK_EQ(soa.Decode<mirror::Object>(ref), obj);
  }
  soa.Vm()->DeleteWeakGlobalRef(soa.Self(), ref);
}

extern "C" JNIEXPORT void JNICALL Java_JObjectBenchmark_timeDecodeHandleScopeRef(
    JNIEnv* env, jobject jobj, jint reps) {
  ScopedObjectAccess soa(env);
  for (jint i = 0; i < reps; ++i) {
    soa.Decode<mirror::Object>(jobj);
  }
}

}  // namespace
}  // namespace art
