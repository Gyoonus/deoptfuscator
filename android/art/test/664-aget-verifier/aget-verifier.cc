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

#include "dex/dex_file.h"

#include "art_method-inl.h"
#include "dex/method_reference.h"
#include "jni.h"
#include "mirror/class-inl.h"
#include "mirror/executable.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace {

extern "C" JNIEXPORT jboolean JNICALL Java_Main_testCompiled(JNIEnv* env,
                                                             jclass,
                                                             jobject method) {
  CHECK(method != nullptr);
  ScopedObjectAccess soa(env);
  ObjPtr<mirror::Executable> exec = soa.Decode<mirror::Executable>(method);
  ArtMethod* art_method = exec->GetArtMethod();
  return art_method->HasAnyCompiledCode();
}

}  // namespace
}  // namespace art
