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

#include <iostream>

#include "art_method.h"
#include "jit/jit.h"
#include "linear_alloc.h"
#include "nativehelper/ScopedUtfChars.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {
namespace {

extern "C" JNIEXPORT jlong JNICALL Java_Main_getArtMethod(JNIEnv* env,
                                                          jclass,
                                                          jobject java_method) {
  ScopedObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, java_method);
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(method));
}

extern "C" JNIEXPORT void JNICALL Java_Main_reuseArenaOfMethod(JNIEnv*,
                                                               jclass,
                                                               jlong art_method) {
  // Create a new allocation and use it to request a specified amount of arenas.
  // Hopefully one of them is a reused one, the one that covers the art_method pointer.
  std::unique_ptr<LinearAlloc> alloc(Runtime::Current()->CreateLinearAlloc());
  do {
    // Ask for a byte - it's sufficient to get an arena and not have issues with size.
    alloc->Alloc(Thread::Current(), 1);
  } while (!alloc->Contains(reinterpret_cast<void*>(static_cast<uintptr_t>(art_method))));
}

}  // namespace
}  // namespace art
