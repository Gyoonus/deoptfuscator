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
  TestVisitor(Thread* thread, Context* context, mirror::Object* this_value)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        this_value_(this_value),
        found_method_index_(0) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare("testThisWithInstanceCall") == 0) {
      found_method_index_ = 1;
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);
      CHECK_EQ(GetThisObject(), this_value_);
    } else if (m_name.compare("testThisWithStaticCall") == 0) {
      found_method_index_ = 2;
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
    } else if (m_name.compare("testParameter") == 0) {
      found_method_index_ = 3;
      uint32_t value = 0;
      CHECK(GetVReg(m, 1, kReferenceVReg, &value));
    } else if (m_name.compare("testObjectInScope") == 0) {
      found_method_index_ = 4;
      uint32_t value = 0;
      CHECK(GetVReg(m, 0, kReferenceVReg, &value));
    }

    return true;
  }

  mirror::Object* this_value_;

  // Value returned to Java to ensure the methods testSimpleVReg and testPairVReg
  // have been found and tested.
  jint found_method_index_;
};

extern "C" JNIEXPORT jint JNICALL Java_Main_doNativeCallRef(JNIEnv*, jobject value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get(), soa.Decode<mirror::Object>(value).Ptr());
  visitor.WalkStack();
  return visitor.found_method_index_;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_doStaticNativeCallRef(JNIEnv*, jclass) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get(), nullptr);
  visitor.WalkStack();
  return visitor.found_method_index_;
}

}  // namespace

}  // namespace art
