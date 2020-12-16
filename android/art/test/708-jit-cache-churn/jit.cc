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

#include "jni.h"

#include "art_method.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread_list.h"

namespace art {

extern "C" JNIEXPORT
jboolean
Java_JitCacheChurnTest_removeJitCompiledMethod(JNIEnv* env,
                                               jclass,
                                               jobject javaMethod,
                                               jboolean releaseMemory) {
  if (!Runtime::Current()->UseJitCompilation()) {
    return JNI_FALSE;
  }

  jit::Jit* jit = Runtime::Current()->GetJit();
  jit->WaitForCompilationToFinish(Thread::Current());

  ScopedObjectAccess soa(env);
  ArtMethod* method = ArtMethod::FromReflectedMethod(soa, javaMethod);

  jit::JitCodeCache* code_cache = jit->GetCodeCache();

  // Drop the shared mutator lock
  ScopedThreadSuspension selfSuspension(Thread::Current(), art::ThreadState::kNative);
  // Get exclusive mutator lock with suspend all.
  ScopedSuspendAll suspend("Removing JIT compiled method", /*long_suspend*/true);
  bool removed = code_cache->RemoveMethod(method, static_cast<bool>(releaseMemory));
  return removed ? JNI_TRUE : JNI_FALSE;
}

}  // namespace art
