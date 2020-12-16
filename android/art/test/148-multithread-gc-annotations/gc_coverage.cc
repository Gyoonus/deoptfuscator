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

#include "gc/heap.h"
#include "jni.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {
namespace {

extern "C" JNIEXPORT jboolean JNICALL Java_MovingGCThread_performHomogeneousSpaceCompact(JNIEnv*, jclass) {
  return Runtime::Current()->GetHeap()->PerformHomogeneousSpaceCompact() == gc::kSuccess ?
      JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_MovingGCThread_supportHomogeneousSpaceCompact(JNIEnv*, jclass) {
  return Runtime::Current()->GetHeap()->SupportHomogeneousSpaceCompactAndCollectorTransitions() ?
      JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL Java_MovingGCThread_objectAddress(JNIEnv* env, jclass, jobject object) {
  ScopedObjectAccess soa(env);
  return reinterpret_cast<jlong>(soa.Decode<mirror::Object>(object).Ptr());
}

}  // namespace
}  // namespace art
