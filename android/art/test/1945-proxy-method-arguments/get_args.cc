/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Visit a proxy method Quick frame at a given depth.
class GetProxyQuickFrameVisitor FINAL : public StackVisitor {
 public:
  GetProxyQuickFrameVisitor(art::Thread* target, art::Context* ctx, size_t frame_depth)
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      : art::StackVisitor(target, ctx, art::StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        cur_depth_(0u),
        frame_depth_(frame_depth),
        quick_frame_(nullptr) {}

  bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    if (GetMethod()->IsRuntimeMethod()) {
      return true;
    }
    if (cur_depth_ == frame_depth_) {
      // Found frame.
      ShadowFrame* shadow_frame = GetCurrentShadowFrame();
      if (shadow_frame != nullptr) {
        // Nothing to do.
      } else {
        VisitQuickFrameAtSearchedDepth();
      }
      return false;
    } else {
      ++cur_depth_;
      return true;
    }
  }

  void VisitQuickFrameAtSearchedDepth() REQUIRES_SHARED(Locks::mutator_lock_) {
    quick_frame_ = GetCurrentQuickFrame();
    CHECK(quick_frame_ != nullptr);
    ArtMethod* method = *quick_frame_;
    CHECK(method != nullptr);
    CHECK(method->IsProxyMethod()) << method->PrettyMethod();
  }

  // Return the found Quick frame.
  ArtMethod** GetQuickFrame() {
    return quick_frame_;
  }

 private:
  // The depth of the currently visited frame.
  size_t cur_depth_;
  // The depth of the currently searched frame.
  size_t frame_depth_;
  // The quick frame, if found.
  ArtMethod** quick_frame_;
  // Method name

  DISALLOW_COPY_AND_ASSIGN(GetProxyQuickFrameVisitor);
};

extern "C" StackReference<mirror::Object>* artQuickGetProxyReferenceArgumentAt(size_t arg_pos,
                                                                               ArtMethod** sp)
    REQUIRES_SHARED(Locks::mutator_lock_);

jobject GetProxyReferenceArgument(size_t arg_pos, size_t proxy_method_frame_depth) {
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  std::unique_ptr<Context> context(Context::Create());

  GetProxyQuickFrameVisitor visitor(self, context.get(), proxy_method_frame_depth);
  visitor.WalkStack();
  ArtMethod** quick_frame = visitor.GetQuickFrame();
  CHECK(quick_frame != nullptr);

  // Find reference argument in frame.
  StackReference<mirror::Object>* ref_arg =
      artQuickGetProxyReferenceArgumentAt(arg_pos, quick_frame);
  CHECK(ref_arg != nullptr);
  art::ObjPtr<mirror::Object> obj = ref_arg->AsMirrorPtr();

  return obj.IsNull() ? nullptr : soa.AddLocalReference<jobject>(obj);
}

extern "C" JNIEXPORT jobject JNICALL Java_TestInvocationHandler_getArgument(
    JNIEnv* env ATTRIBUTE_UNUSED, jobject thiz ATTRIBUTE_UNUSED, int arg_pos, int frame_depth) {
  return GetProxyReferenceArgument(arg_pos, frame_depth);
}

}  // namespace

}  // namespace art
