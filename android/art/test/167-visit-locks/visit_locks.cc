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

#include <iostream>

#include "android-base/logging.h"

#include "arch/context.h"
#include "art_method.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "monitor.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread-current-inl.h"

namespace art {

extern "C" JNIEXPORT void JNICALL Java_Main_testVisitLocks(JNIEnv*, jclass) {
  ScopedObjectAccess soa(Thread::Current());

  class VisitLocks : public StackVisitor {
   public:
    VisitLocks(Thread* thread, Context* context)
        : StackVisitor(thread, context, StackWalkKind::kIncludeInlinedFrames) {
    }

    bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
      ArtMethod* m = GetMethod();

      // Ignore runtime methods.
      if (m == nullptr || m->IsRuntimeMethod()) {
        return true;
      }

      if (m->PrettyMethod() == "void TestSync.run()") {
        // Interesting frame.
        Monitor::VisitLocks(this, Callback, nullptr);
        return false;
      }

      return true;
    }

    static void Callback(mirror::Object* obj, void*) REQUIRES_SHARED(Locks::mutator_lock_) {
      CHECK(obj != nullptr);
      CHECK(obj->IsString());
      std::cerr << obj->AsString()->ToModifiedUtf8() << std::endl;
    }
  };
  Context* context = Context::Create();
  VisitLocks vl(soa.Self(), context);
  vl.WalkStack();
  delete context;
}

}  // namespace art
