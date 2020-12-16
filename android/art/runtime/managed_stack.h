/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_MANAGED_STACK_H_
#define ART_RUNTIME_MANAGED_STACK_H_

#include <cstdint>
#include <cstring>
#include <string>

#include <android-base/logging.h>

#include "base/macros.h"
#include "base/mutex.h"
#include "base/bit_utils.h"

namespace art {

namespace mirror {
class Object;
}  // namespace mirror

class ArtMethod;
class ShadowFrame;
template <typename T> class StackReference;

// The managed stack is used to record fragments of managed code stacks. Managed code stacks
// may either be shadow frames or lists of frames using fixed frame sizes. Transition records are
// necessary for transitions between code using different frame layouts and transitions into native
// code.
class PACKED(4) ManagedStack {
 public:
  ManagedStack()
      : tagged_top_quick_frame_(TaggedTopQuickFrame::CreateNotTagged(nullptr)),
        link_(nullptr),
        top_shadow_frame_(nullptr) {}

  void PushManagedStackFragment(ManagedStack* fragment) {
    // Copy this top fragment into given fragment.
    memcpy(fragment, this, sizeof(ManagedStack));
    // Clear this fragment, which has become the top.
    memset(this, 0, sizeof(ManagedStack));
    // Link our top fragment onto the given fragment.
    link_ = fragment;
  }

  void PopManagedStackFragment(const ManagedStack& fragment) {
    DCHECK(&fragment == link_);
    // Copy this given fragment back to the top.
    memcpy(this, &fragment, sizeof(ManagedStack));
  }

  ManagedStack* GetLink() const {
    return link_;
  }

  ArtMethod** GetTopQuickFrameKnownNotTagged() const {
    return tagged_top_quick_frame_.GetSpKnownNotTagged();
  }

  ArtMethod** GetTopQuickFrame() const {
    return tagged_top_quick_frame_.GetSp();
  }

  bool GetTopQuickFrameTag() const {
    return tagged_top_quick_frame_.GetTag();
  }

  bool HasTopQuickFrame() const {
    return tagged_top_quick_frame_.GetTaggedSp() != 0u;
  }

  void SetTopQuickFrame(ArtMethod** top) {
    DCHECK(top_shadow_frame_ == nullptr);
    DCHECK_ALIGNED(top, 4u);
    tagged_top_quick_frame_ = TaggedTopQuickFrame::CreateNotTagged(top);
  }

  void SetTopQuickFrameTagged(ArtMethod** top) {
    DCHECK(top_shadow_frame_ == nullptr);
    DCHECK_ALIGNED(top, 4u);
    tagged_top_quick_frame_ = TaggedTopQuickFrame::CreateTagged(top);
  }

  static size_t TaggedTopQuickFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, tagged_top_quick_frame_);
  }

  ALWAYS_INLINE ShadowFrame* PushShadowFrame(ShadowFrame* new_top_frame);
  ALWAYS_INLINE ShadowFrame* PopShadowFrame();

  ShadowFrame* GetTopShadowFrame() const {
    return top_shadow_frame_;
  }

  bool HasTopShadowFrame() const {
    return GetTopShadowFrame() != nullptr;
  }

  void SetTopShadowFrame(ShadowFrame* top) {
    DCHECK_EQ(tagged_top_quick_frame_.GetTaggedSp(), 0u);
    top_shadow_frame_ = top;
  }

  static size_t TopShadowFrameOffset() {
    return OFFSETOF_MEMBER(ManagedStack, top_shadow_frame_);
  }

  size_t NumJniShadowFrameReferences() const REQUIRES_SHARED(Locks::mutator_lock_);

  bool ShadowFramesContain(StackReference<mirror::Object>* shadow_frame_entry) const;

 private:
  // Encodes the top quick frame (which must be at least 4-byte aligned)
  // and a flag that marks the GenericJNI trampoline.
  class TaggedTopQuickFrame {
   public:
    static TaggedTopQuickFrame CreateNotTagged(ArtMethod** sp) {
      DCHECK_ALIGNED(sp, 4u);
      return TaggedTopQuickFrame(reinterpret_cast<uintptr_t>(sp));
    }

    static TaggedTopQuickFrame CreateTagged(ArtMethod** sp) {
      DCHECK_ALIGNED(sp, 4u);
      return TaggedTopQuickFrame(reinterpret_cast<uintptr_t>(sp) | 1u);
    }

    // Get SP known to be not tagged and non-null.
    ArtMethod** GetSpKnownNotTagged() const {
      DCHECK(!GetTag());
      DCHECK_NE(tagged_sp_, 0u);
      return reinterpret_cast<ArtMethod**>(tagged_sp_);
    }

    ArtMethod** GetSp() const {
      return reinterpret_cast<ArtMethod**>(tagged_sp_ & ~static_cast<uintptr_t>(1u));
    }

    bool GetTag() const {
      return (tagged_sp_ & 1u) != 0u;
    }

    uintptr_t GetTaggedSp() const {
      return tagged_sp_;
    }

   private:
    explicit TaggedTopQuickFrame(uintptr_t tagged_sp) : tagged_sp_(tagged_sp) { }

    uintptr_t tagged_sp_;
  };
  static_assert(sizeof(TaggedTopQuickFrame) == sizeof(uintptr_t), "TaggedTopQuickFrame size check");

  TaggedTopQuickFrame tagged_top_quick_frame_;
  ManagedStack* link_;
  ShadowFrame* top_shadow_frame_;
};

}  // namespace art

#endif  // ART_RUNTIME_MANAGED_STACK_H_
