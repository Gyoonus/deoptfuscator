/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "art_method-inl.h"
#include "jni.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

extern "C" JNIEXPORT jint JNICALL Java_Main_vmArrayBaseOffset(JNIEnv* env, jclass, jobject classObj) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(classObj);
  return mirror::Array::DataOffset(
      Primitive::ComponentSize(klass->GetComponentType()->GetPrimitiveType())).Int32Value();
}

extern "C" JNIEXPORT jint JNICALL Java_Main_vmArrayIndexScale(JNIEnv* env, jclass, jobject classObj) {
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(classObj);
  return Primitive::ComponentSize(klass->GetComponentType()->GetPrimitiveType());
}

}  // namespace art
