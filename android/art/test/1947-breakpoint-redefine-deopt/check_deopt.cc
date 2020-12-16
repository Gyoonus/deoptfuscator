/*
 * Copyright (C) 2018 The Android Open Source Project
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
#include "art_method-inl.h"
#include "jni_internal.h"
#include "instrumentation.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isMethodDeoptimized(JNIEnv*, jclass, jobject m) {
  ScopedObjectAccess soa(art::Thread::Current());
  ArtMethod* art_method = ArtMethod::FromReflectedMethod(soa, m);
  return Runtime::Current()->GetInstrumentation()->IsDeoptimized(art_method);
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isInterpretOnly(JNIEnv*, jclass) {
  return Runtime::Current()->GetInstrumentation()->IsForcedInterpretOnly();
}

}  // namespace art
