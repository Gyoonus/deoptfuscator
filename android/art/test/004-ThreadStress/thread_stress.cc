/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <iostream>

#include "jni.h"
#include "mirror/string.h"
#include "mirror/throwable.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

extern "C" JNIEXPORT void JNICALL Java_Main_printString(JNIEnv*, jclass, jstring s) {
  ScopedObjectAccess soa(Thread::Current());
  std::cout << soa.Decode<mirror::String>(s)->ToModifiedUtf8();
}

extern "C" JNIEXPORT void JNICALL Java_Main_printThrowable(JNIEnv*, jclass, jthrowable t) {
  ScopedObjectAccess soa(Thread::Current());
  std::cout << soa.Decode<mirror::Throwable>(t)->Dump();
}

}  // namespace art
