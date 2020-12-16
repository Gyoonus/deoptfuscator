/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_

#include "arch/instruction_set.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/mutex.h"
#include "thread-inl.h"

// Specific frame size code is in architecture-specific files. We include this to compile-time
// specialize the code.
#include "arch/arm/quick_method_frame_info_arm.h"
#include "arch/arm64/quick_method_frame_info_arm64.h"
#include "arch/mips/quick_method_frame_info_mips.h"
#include "arch/mips64/quick_method_frame_info_mips64.h"
#include "arch/x86/quick_method_frame_info_x86.h"
#include "arch/x86_64/quick_method_frame_info_x86_64.h"

namespace art {
class ArtMethod;

class ScopedQuickEntrypointChecks {
 public:
  explicit ScopedQuickEntrypointChecks(Thread *self,
                                       bool entry_check = kIsDebugBuild,
                                       bool exit_check = kIsDebugBuild)
      REQUIRES_SHARED(Locks::mutator_lock_) : self_(self), exit_check_(exit_check) {
    if (entry_check) {
      TestsOnEntry();
    }
  }

  ~ScopedQuickEntrypointChecks() REQUIRES_SHARED(Locks::mutator_lock_) {
    if (exit_check_) {
      TestsOnExit();
    }
  }

 private:
  void TestsOnEntry() REQUIRES_SHARED(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  void TestsOnExit() REQUIRES_SHARED(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  Thread* const self_;
  bool exit_check_;
};

static constexpr size_t GetCalleeSaveFrameSize(InstructionSet isa, CalleeSaveType type) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return arm::ArmCalleeSaveFrameSize(type);
    case InstructionSet::kArm64:
      return arm64::Arm64CalleeSaveFrameSize(type);
    case InstructionSet::kMips:
      return mips::MipsCalleeSaveFrameSize(type);
    case InstructionSet::kMips64:
      return mips64::Mips64CalleeSaveFrameSize(type);
    case InstructionSet::kX86:
      return x86::X86CalleeSaveFrameSize(type);
    case InstructionSet::kX86_64:
      return x86_64::X86_64CalleeSaveFrameSize(type);
    case InstructionSet::kNone:
      LOG(FATAL) << "kNone has no frame size";
      UNREACHABLE();
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}

// Note: this specialized statement is sanity-checked in the quick-trampoline gtest.
static constexpr PointerSize GetConstExprPointerSize(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return kArmPointerSize;
    case InstructionSet::kArm64:
      return kArm64PointerSize;
    case InstructionSet::kMips:
      return kMipsPointerSize;
    case InstructionSet::kMips64:
      return kMips64PointerSize;
    case InstructionSet::kX86:
      return kX86PointerSize;
    case InstructionSet::kX86_64:
      return kX86_64PointerSize;
    case InstructionSet::kNone:
      LOG(FATAL) << "kNone has no pointer size";
      UNREACHABLE();
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}

// Note: this specialized statement is sanity-checked in the quick-trampoline gtest.
static constexpr size_t GetCalleeSaveReturnPcOffset(InstructionSet isa, CalleeSaveType type) {
  return GetCalleeSaveFrameSize(isa, type) - static_cast<size_t>(GetConstExprPointerSize(isa));
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
