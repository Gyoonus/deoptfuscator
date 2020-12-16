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
  TestVisitor(Thread* thread, Context* context)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames) {}

  bool VisitFrame() REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare("mergeOk") == 0) {
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 1, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 2, kIntVReg, &value));
      CHECK_EQ(value, 2u);

      CHECK(GetVReg(m, 3, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 4, kIntVReg, &value));
      CHECK_EQ(value, 2u);
      did_check_ = true;
    } else if (m_name.compare("mergeNotOk") == 0) {
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 1, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      bool success = GetVReg(m, 2, kIntVReg, &value);
      if (!IsShadowFrame() && GetCurrentOatQuickMethodHeader()->IsOptimized()) {
        CHECK(!success);
      }

      CHECK(GetVReg(m, 3, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 4, kFloatVReg, &value));
      uint32_t cast = bit_cast<uint32_t, float>(4.0f);
      CHECK_EQ(value, cast);
      did_check_ = true;
    } else if (m_name.compare("phiEquivalent") == 0) {
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      // Quick doesn't like this one on x64.
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 1, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 2, kFloatVReg, &value));
      CHECK_EQ(value, 1u);

      did_check_ = true;
    } else if (m_name.compare("mergeReferences") == 0) {
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 1, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 2, kReferenceVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 3, kReferenceVReg, &value));
      CHECK_NE(value, 0u);

      did_check_ = true;
    } else if (m_name.compare("phiAllEquivalents") == 0) {
      uint32_t value = 0;

      CHECK(GetVReg(m, 0, kIntVReg, &value));
      CHECK_EQ(value, 0u);

      CHECK(GetVReg(m, 1, kIntVReg, &value));
      CHECK_EQ(value, 1u);

      CHECK(GetVReg(m, 2, kReferenceVReg, &value));
      CHECK_EQ(value, 0u);

      did_check_ = true;
    }

    return true;
  }

  bool did_check_ = false;
};

extern "C" JNIEXPORT void JNICALL Java_PhiLiveness_regsNativeCall(
    JNIEnv*, jclass value ATTRIBUTE_UNUSED) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  TestVisitor visitor(soa.Self(), context.get());
  visitor.WalkStack();
  CHECK(visitor.did_check_);
}

extern "C" JNIEXPORT void JNICALL Java_PhiLiveness_regsNativeCallWithParameters(
    JNIEnv*, jclass value ATTRIBUTE_UNUSED, jobject main, jint int_value, jfloat float_value) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<Context> context(Context::Create());
  CHECK(soa.Decode<mirror::Object>(main) == nullptr);
  CHECK_EQ(int_value, 0);
  int32_t cast = bit_cast<int32_t, float>(float_value);
  CHECK_EQ(cast, 0);
  TestVisitor visitor(soa.Self(), context.get());
  visitor.WalkStack();
  CHECK(visitor.did_check_);
}

}  // namespace

}  // namespace art
