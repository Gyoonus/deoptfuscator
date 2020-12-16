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

#include "arch/context.h"
#include "art_method-inl.h"
#include "jni.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"

namespace art {

namespace {

class TestVisitor : public StackVisitor {
 public:
  TestVisitor(const ScopedObjectAccess& soa, Context* context, jobject expected_value)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(soa.Self(), context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        expected_value_(expected_value),
        found_(false),
        soa_(soa) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name == "testCase") {
      found_ = true;
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value),
               soa_.Decode<mirror::Object>(expected_value_).Ptr());
    }
    return true;
  }

  jobject expected_value_;
  bool found_;
  const ScopedObjectAccess& soa_;
};

}  // namespace

extern "C" JNIEXPORT void JNICALL Java_Main_lookForMyRegisters(JNIEnv*, jclass, jobject value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa, context.get(), value);
  visitor.WalkStack();
  CHECK(visitor.found_);
}

}  // namespace art
