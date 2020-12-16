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
#include "oat_quick_method_header.h"
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

    if (m_name.compare("testSimpleVReg") == 0) {
      found_method_index_ = 1;
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      CHECK_EQ(value, 42u);

      bool success = GetVReg(m, 1, kIntVReg, &value);
      if (!IsShadowFrame() && GetCurrentOatQuickMethodHeader()->IsOptimized()) {
        CHECK(!success);
      }

      success = GetVReg(m, 2, kIntVReg, &value);
      if (!IsShadowFrame() && GetCurrentOatQuickMethodHeader()->IsOptimized()) {
        CHECK(!success);
      }

      CHECK(GetVReg(m, 3, kReferenceVReg, &value));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value), this_value_);

      CHECK(GetVReg(m, 4, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 5, kFloatVReg, &value));
      uint32_t cast = bit_cast<uint32_t, float>(1.0f);
      CHECK_EQ(value, cast);

      CHECK(GetVReg(m, 6, kIntVReg, &value));
      CHECK_EQ(value, 2u);

      CHECK(GetVReg(m, 7, kIntVReg, &value));
      CHECK_EQ(value, true);

      CHECK(GetVReg(m, 8, kIntVReg, &value));
      CHECK_EQ(value, 3u);

      CHECK(GetVReg(m, 9, kIntVReg, &value));
      CHECK_EQ(value, static_cast<uint32_t>('c'));
    } else if (m_name.compare("testPairVReg") == 0) {
      found_method_index_ = 2;
      uint64_t value = 0;
      CHECK(GetVRegPair(m, 0, kLongLoVReg, kLongHiVReg, &value));
      CHECK_EQ(value, 42u);

      bool success = GetVRegPair(m, 2, kLongLoVReg, kLongHiVReg, &value);
      if (!IsShadowFrame() && GetCurrentOatQuickMethodHeader()->IsOptimized()) {
        CHECK(!success);
      }

      success = GetVRegPair(m, 4, kLongLoVReg, kLongHiVReg, &value);
      if (!IsShadowFrame() && GetCurrentOatQuickMethodHeader()->IsOptimized()) {
        CHECK(!success);
      }

      uint32_t value32 = 0;
      CHECK(GetVReg(m, 6, kReferenceVReg, &value32));
      CHECK_EQ(reinterpret_cast<mirror::Object*>(value32), this_value_);

      CHECK(GetVRegPair(m, 7, kLongLoVReg, kLongHiVReg, &value));
      CHECK_EQ(static_cast<int64_t>(value), std::numeric_limits<int64_t>::min());

      CHECK(GetVRegPair(m, 9, kLongLoVReg, kLongHiVReg, &value));
      CHECK_EQ(static_cast<int64_t>(value), std::numeric_limits<int64_t>::max());

      CHECK(GetVRegPair(m, 11, kLongLoVReg, kLongHiVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVRegPair(m, 13, kDoubleLoVReg, kDoubleHiVReg, &value));
      uint64_t cast = bit_cast<uint64_t, double>(2.0);
      CHECK_EQ(value, cast);
    }

    return true;
  }

  mirror::Object* this_value_;

  // Value returned to Java to ensure the methods testSimpleVReg and testPairVReg
  // have been found and tested.
  jint found_method_index_;
};

extern "C" JNIEXPORT jint JNICALL Java_Main_doNativeCall(JNIEnv*, jobject value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get(), soa.Decode<mirror::Object>(value).Ptr());
  visitor.WalkStack();
  return visitor.found_method_index_;
}

}  // namespace

}  // namespace art
