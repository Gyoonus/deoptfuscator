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

#include <memory>
#include <vector>

#include "arch/instruction_set.h"
#include "base/arena_allocator.h"
#include "base/enums.h"
#include "cfi_test.h"
#include "gtest/gtest.h"
#include "jni/quick/calling_convention.h"
#include "read_barrier_config.h"
#include "utils/assembler.h"
#include "utils/jni_macro_assembler.h"

#include "jni/jni_cfi_test_expected.inc"

namespace art {

// Run the tests only on host.
#ifndef ART_TARGET_ANDROID

class JNICFITest : public CFITest {
 public:
  // Enable this flag to generate the expected outputs.
  static constexpr bool kGenerateExpected = false;

  void TestImpl(InstructionSet isa,
                const char* isa_str,
                const std::vector<uint8_t>& expected_asm,
                const std::vector<uint8_t>& expected_cfi) {
    if (Is64BitInstructionSet(isa)) {
      TestImplSized<PointerSize::k64>(isa, isa_str, expected_asm, expected_cfi);
    } else {
      TestImplSized<PointerSize::k32>(isa, isa_str, expected_asm, expected_cfi);
    }
  }

 private:
  template <PointerSize kPointerSize>
  void TestImplSized(InstructionSet isa,
                     const char* isa_str,
                     const std::vector<uint8_t>& expected_asm,
                     const std::vector<uint8_t>& expected_cfi) {
    // Description of simple method.
    const bool is_static = true;
    const bool is_synchronized = false;
    const char* shorty = "IIFII";

    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    std::unique_ptr<JniCallingConvention> jni_conv(
        JniCallingConvention::Create(&allocator,
                                     is_static,
                                     is_synchronized,
                                     /*is_critical_native*/false,
                                     shorty,
                                     isa));
    std::unique_ptr<ManagedRuntimeCallingConvention> mr_conv(
        ManagedRuntimeCallingConvention::Create(
            &allocator, is_static, is_synchronized, shorty, isa));
    const int frame_size(jni_conv->FrameSize());
    ArrayRef<const ManagedRegister> callee_save_regs = jni_conv->CalleeSaveRegisters();

    // Assemble the method.
    std::unique_ptr<JNIMacroAssembler<kPointerSize>> jni_asm(
        JNIMacroAssembler<kPointerSize>::Create(&allocator, isa));
    jni_asm->cfi().SetEnabled(true);
    jni_asm->BuildFrame(frame_size, mr_conv->MethodRegister(),
                        callee_save_regs, mr_conv->EntrySpills());
    jni_asm->IncreaseFrameSize(32);
    jni_asm->DecreaseFrameSize(32);
    jni_asm->RemoveFrame(frame_size, callee_save_regs, /* may_suspend */ true);
    jni_asm->FinalizeCode();
    std::vector<uint8_t> actual_asm(jni_asm->CodeSize());
    MemoryRegion code(&actual_asm[0], actual_asm.size());
    jni_asm->FinalizeInstructions(code);
    ASSERT_EQ(jni_asm->cfi().GetCurrentCFAOffset(), frame_size);
    const std::vector<uint8_t>& actual_cfi = *(jni_asm->cfi().data());

    if (kGenerateExpected) {
      GenerateExpected(stdout, isa, isa_str, actual_asm, actual_cfi);
    } else {
      EXPECT_EQ(expected_asm, actual_asm);
      EXPECT_EQ(expected_cfi, actual_cfi);
    }
  }
};

#define TEST_ISA(isa)                                                 \
  TEST_F(JNICFITest, isa) {                                           \
    std::vector<uint8_t> expected_asm(expected_asm_##isa,             \
        expected_asm_##isa + arraysize(expected_asm_##isa));          \
    std::vector<uint8_t> expected_cfi(expected_cfi_##isa,             \
        expected_cfi_##isa + arraysize(expected_cfi_##isa));          \
    TestImpl(InstructionSet::isa, #isa, expected_asm, expected_cfi);  \
  }

#ifdef ART_ENABLE_CODEGEN_arm
// Run the tests for ARM only with Baker read barriers, as the
// expected generated code contains a Marking Register refresh
// instruction.
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
TEST_ISA(kThumb2)
#endif
#endif

#ifdef ART_ENABLE_CODEGEN_arm64
// Run the tests for ARM64 only with Baker read barriers, as the
// expected generated code contains a Marking Register refresh
// instruction.
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
TEST_ISA(kArm64)
#endif
#endif

#ifdef ART_ENABLE_CODEGEN_x86
TEST_ISA(kX86)
#endif

#ifdef ART_ENABLE_CODEGEN_x86_64
TEST_ISA(kX86_64)
#endif

#ifdef ART_ENABLE_CODEGEN_mips
TEST_ISA(kMips)
#endif

#ifdef ART_ENABLE_CODEGEN_mips64
TEST_ISA(kMips64)
#endif

#endif  // ART_TARGET_ANDROID

}  // namespace art
