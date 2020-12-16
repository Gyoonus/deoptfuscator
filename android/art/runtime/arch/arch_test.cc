/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <stdint.h>

#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "common_runtime_test.h"
#include "quick/quick_method_frame_info.h"

// asm_support.h declares tests next to the #defines. We use asm_support_check.h to (safely)
// generate CheckAsmSupportOffsetsAndSizes using gtest's EXPECT for the tests. We also use the
// RETURN_TYPE, HEADER and FOOTER defines from asm_support_check.h to try to ensure that any
// tests are actually generated.

// Let CheckAsmSupportOffsetsAndSizes return a size_t (the count).
#define ASM_SUPPORT_CHECK_RETURN_TYPE size_t

// Declare the counter that will be updated per test.
#define ASM_SUPPORT_CHECK_HEADER size_t count = 0;

// Use EXPECT_EQ for tests, and increment the counter.
#define ADD_TEST_EQ(x, y) EXPECT_EQ(x, y); count++;

// Return the counter at the end of CheckAsmSupportOffsetsAndSizes.
#define ASM_SUPPORT_CHECK_FOOTER return count;

// Generate CheckAsmSupportOffsetsAndSizes().
#include "asm_support_check.h"

namespace art {

class ArchTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions *options) OVERRIDE {
    // Use 64-bit ISA for runtime setup to make method size potentially larger
    // than necessary (rather than smaller) during CreateCalleeSaveMethod
    options->push_back(std::make_pair("imageinstructionset", "x86_64"));
  }

  // Do not do any of the finalization. We don't want to run any code, we don't need the heap
  // prepared, it actually will be a problem with setting the instruction set to x86_64 in
  // SetUpRuntimeOptions.
  void FinalizeSetup() OVERRIDE {
    ASSERT_EQ(InstructionSet::kX86_64, Runtime::Current()->GetInstructionSet());
  }

  static void CheckFrameSize(InstructionSet isa, CalleeSaveType type, uint32_t save_size)
      NO_THREAD_SAFETY_ANALYSIS {
    Runtime* const runtime = Runtime::Current();
    Thread* const self = Thread::Current();
    ScopedObjectAccess soa(self);  // So we can create callee-save methods.

    runtime->SetInstructionSet(isa);
    ArtMethod* save_method = runtime->CreateCalleeSaveMethod();
    runtime->SetCalleeSaveMethod(save_method, type);
    QuickMethodFrameInfo frame_info =  runtime->GetRuntimeMethodFrameInfo(save_method);
    EXPECT_EQ(frame_info.FrameSizeInBytes(), save_size) << "Expected and real size differs for "
        << type << " core spills=" << std::hex << frame_info.CoreSpillMask() << " fp spills="
        << frame_info.FpSpillMask() << std::dec;
  }
};

TEST_F(ArchTest, CheckCommonOffsetsAndSizes) {
  size_t test_count = CheckAsmSupportOffsetsAndSizes();
  EXPECT_GT(test_count, 0u);
}

// Grab architecture specific constants.
namespace arm {
#include "arch/arm/asm_support_arm.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_OFFSET
}  // namespace arm

namespace arm64 {
#include "arch/arm64/asm_support_arm64.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_OFFSET
}  // namespace arm64

namespace mips {
#include "arch/mips/asm_support_mips.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_REGISTER_COUNT
#undef BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE
}  // namespace mips

namespace mips64 {
#include "arch/mips64/asm_support_mips64.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_REGISTER_COUNT
#undef BAKER_MARK_INTROSPECTION_FIELD_ARRAY_ENTRY_SIZE
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRIES_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRY_SIZE
}  // namespace mips64

namespace x86 {
#include "arch/x86/asm_support_x86.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
}  // namespace x86

namespace x86_64 {
#include "arch/x86_64/asm_support_x86_64.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
}  // namespace x86_64

// Check architecture specific constants are sound.
#define TEST_ARCH(Arch, arch)                                       \
  TEST_F(ArchTest, Arch) {                                          \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveAllCalleeSaves,             \
                   arch::kFrameSizeSaveAllCalleeSaves);             \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveRefsOnly,                   \
                   arch::kFrameSizeSaveRefsOnly);                   \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveRefsAndArgs,                \
                   arch::kFrameSizeSaveRefsAndArgs);                \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveEverything,                 \
                   arch::kFrameSizeSaveEverything);                 \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveEverythingForClinit,        \
                   arch::kFrameSizeSaveEverythingForClinit);        \
    CheckFrameSize(InstructionSet::k##Arch,                         \
                   CalleeSaveType::kSaveEverythingForSuspendCheck,  \
                   arch::kFrameSizeSaveEverythingForSuspendCheck);  \
  }
TEST_ARCH(Arm, arm)
TEST_ARCH(Arm64, arm64)
TEST_ARCH(Mips, mips)
TEST_ARCH(Mips64, mips64)
TEST_ARCH(X86, x86)
TEST_ARCH(X86_64, x86_64)

}  // namespace art
