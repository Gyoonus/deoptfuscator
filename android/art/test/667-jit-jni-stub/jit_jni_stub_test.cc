/*
 * Copyright 2017 The Android Open Source Project
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

#include <jni.h>

#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class.h"
#include "mirror/string.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

// Local class declared as a friend of JitCodeCache so that we can access its internals.
class JitJniStubTestHelper {
 public:
  static bool isNextJitGcFull(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(Runtime::Current()->GetJit() != nullptr);
    jit::JitCodeCache* cache = Runtime::Current()->GetJit()->GetCodeCache();
    MutexLock mu(self, cache->lock_);
    return cache->ShouldDoFullCollection();
  }
};

// Calls through to a static method with signature "()V".
extern "C" JNIEXPORT
void Java_Main_callThrough(JNIEnv* env, jclass, jclass klass, jstring methodName) {
  ScopedObjectAccess soa(Thread::Current());
  std::string name = soa.Decode<mirror::String>(methodName)->ToModifiedUtf8();
  jmethodID method = env->GetStaticMethodID(klass, name.c_str(), "()V");
  CHECK(method != nullptr) << soa.Decode<mirror::Class>(klass)->PrettyDescriptor() << "." << name;
  env->CallStaticVoidMethod(klass, method);
}

extern "C" JNIEXPORT
void Java_Main_jitGc(JNIEnv*, jclass) {
  CHECK(Runtime::Current()->GetJit() != nullptr);
  jit::JitCodeCache* cache = Runtime::Current()->GetJit()->GetCodeCache();
  ScopedObjectAccess soa(Thread::Current());
  cache->GarbageCollectCache(Thread::Current());
}

extern "C" JNIEXPORT
jboolean Java_Main_isNextJitGcFull(JNIEnv*, jclass) {
  ScopedObjectAccess soa(Thread::Current());
  return JitJniStubTestHelper::isNextJitGcFull(soa.Self());
}

}  // namespace art
