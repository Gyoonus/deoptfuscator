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
#include "base/runtime_debug.h"
#include "cfi_test.h"
#include "driver/compiler_options.h"
#include "gtest/gtest.h"
#include "optimizing/code_generator.h"
#include "optimizing/optimizing_unit_test.h"
#include "read_barrier_config.h"
#include "utils/arm/assembler_arm_vixl.h"
#include "utils/assembler.h"
#include "utils/mips/assembler_mips.h"
#include "utils/mips64/assembler_mips64.h"

#include "optimizing/optimizing_cfi_test_expected.inc"

namespace vixl32 = vixl::aarch32;

using vixl32::r0;

namespace art {

// Run the tests only on host.
#ifndef ART_TARGET_ANDROID

class OptimizingCFITest : public CFITest, public OptimizingUnitTestHelper {
 public:
  // Enable this flag to generate the expected outputs.
  static constexpr bool kGenerateExpected = false;

  OptimizingCFITest()
      : pool_and_allocator_(),
        opts_(),
        isa_features_(),
        graph_(nullptr),
        code_gen_(),
        blocks_(GetAllocator()->Adapter()) {}

  ArenaAllocator* GetAllocator() { return pool_and_allocator_.GetAllocator(); }

  void SetUpFrame(InstructionSet isa) {
    // Ensure that slow-debug is off, so that there is no unexpected read-barrier check emitted.
    SetRuntimeDebugFlagsEnabled(false);

    // Setup simple context.
    std::string error;
    isa_features_ = InstructionSetFeatures::FromVariant(isa, "default", &error);
    graph_ = CreateGraph();
    // Generate simple frame with some spills.
    code_gen_ = CodeGenerator::Create(graph_, isa, *isa_features_, opts_);
    code_gen_->GetAssembler()->cfi().SetEnabled(true);
    code_gen_->InitializeCodeGenerationData();
    const int frame_size = 64;
    int core_reg = 0;
    int fp_reg = 0;
    for (int i = 0; i < 2; i++) {  // Two registers of each kind.
      for (; core_reg < 32; core_reg++) {
        if (code_gen_->IsCoreCalleeSaveRegister(core_reg)) {
          auto location = Location::RegisterLocation(core_reg);
          code_gen_->AddAllocatedRegister(location);
          core_reg++;
          break;
        }
      }
      for (; fp_reg < 32; fp_reg++) {
        if (code_gen_->IsFloatingPointCalleeSaveRegister(fp_reg)) {
          auto location = Location::FpuRegisterLocation(fp_reg);
          code_gen_->AddAllocatedRegister(location);
          fp_reg++;
          break;
        }
      }
    }
    code_gen_->block_order_ = &blocks_;
    code_gen_->ComputeSpillMask();
    code_gen_->SetFrameSize(frame_size);
    code_gen_->GenerateFrameEntry();
  }

  void Finish() {
    code_gen_->GenerateFrameExit();
    code_gen_->Finalize(&code_allocator_);
  }

  void Check(InstructionSet isa,
             const char* isa_str,
             const std::vector<uint8_t>& expected_asm,
             const std::vector<uint8_t>& expected_cfi) {
    // Get the outputs.
    const std::vector<uint8_t>& actual_asm = code_allocator_.GetMemory();
    Assembler* opt_asm = code_gen_->GetAssembler();
    const std::vector<uint8_t>& actual_cfi = *(opt_asm->cfi().data());

    if (kGenerateExpected) {
      GenerateExpected(stdout, isa, isa_str, actual_asm, actual_cfi);
    } else {
      EXPECT_EQ(expected_asm, actual_asm);
      EXPECT_EQ(expected_cfi, actual_cfi);
    }
  }

  void TestImpl(InstructionSet isa, const char*
                isa_str,
                const std::vector<uint8_t>& expected_asm,
                const std::vector<uint8_t>& expected_cfi) {
    SetUpFrame(isa);
    Finish();
    Check(isa, isa_str, expected_asm, expected_cfi);
  }

  CodeGenerator* GetCodeGenerator() {
    return code_gen_.get();
  }

 private:
  class InternalCodeAllocator : public CodeAllocator {
   public:
    InternalCodeAllocator() {}

    virtual uint8_t* Allocate(size_t size) {
      memory_.resize(size);
      return memory_.data();
    }

    const std::vector<uint8_t>& GetMemory() { return memory_; }

   private:
    std::vector<uint8_t> memory_;

    DISALLOW_COPY_AND_ASSIGN(InternalCodeAllocator);
  };

  ArenaPoolAndAllocator pool_and_allocator_;
  CompilerOptions opts_;
  std::unique_ptr<const InstructionSetFeatures> isa_features_;
  HGraph* graph_;
  std::unique_ptr<CodeGenerator> code_gen_;
  ArenaVector<HBasicBlock*> blocks_;
  InternalCodeAllocator code_allocator_;
};

#define TEST_ISA(isa)                                                 \
  TEST_F(OptimizingCFITest, isa) {                                    \
    std::vector<uint8_t> expected_asm(                                \
        expected_asm_##isa,                                           \
        expected_asm_##isa + arraysize(expected_asm_##isa));          \
    std::vector<uint8_t> expected_cfi(                                \
        expected_cfi_##isa,                                           \
        expected_cfi_##isa + arraysize(expected_cfi_##isa));          \
    TestImpl(InstructionSet::isa, #isa, expected_asm, expected_cfi);  \
  }

#ifdef ART_ENABLE_CODEGEN_arm
TEST_ISA(kThumb2)
#endif

#ifdef ART_ENABLE_CODEGEN_arm64
// Run the tests for ARM64 only with Baker read barriers, as the
// expected generated code saves and restore X21 and X22 (instead of
// X20 and X21), as X20 is used as Marking Register in the Baker read
// barrier configuration, and as such is removed from the set of
// callee-save registers in the ARM64 code generator of the Optimizing
// compiler.
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

#ifdef ART_ENABLE_CODEGEN_arm
TEST_F(OptimizingCFITest, kThumb2Adjust) {
  std::vector<uint8_t> expected_asm(
      expected_asm_kThumb2_adjust,
      expected_asm_kThumb2_adjust + arraysize(expected_asm_kThumb2_adjust));
  std::vector<uint8_t> expected_cfi(
      expected_cfi_kThumb2_adjust,
      expected_cfi_kThumb2_adjust + arraysize(expected_cfi_kThumb2_adjust));
  SetUpFrame(InstructionSet::kThumb2);
#define __ down_cast<arm::ArmVIXLAssembler*>(GetCodeGenerator() \
    ->GetAssembler())->GetVIXLAssembler()->
  vixl32::Label target;
  __ CompareAndBranchIfZero(r0, &target);
  // Push the target out of range of CBZ.
  for (size_t i = 0; i != 65; ++i) {
    __ Ldr(r0, vixl32::MemOperand(r0));
  }
  __ Bind(&target);
#undef __
  Finish();
  Check(InstructionSet::kThumb2, "kThumb2_adjust", expected_asm, expected_cfi);
}
#endif

#ifdef ART_ENABLE_CODEGEN_mips
TEST_F(OptimizingCFITest, kMipsAdjust) {
  // One NOP in delay slot, 1 << 15 NOPS have size 1 << 17 which exceeds 18-bit signed maximum.
  static constexpr size_t kNumNops = 1u + (1u << 15);
  std::vector<uint8_t> expected_asm(
      expected_asm_kMips_adjust_head,
      expected_asm_kMips_adjust_head + arraysize(expected_asm_kMips_adjust_head));
  expected_asm.resize(expected_asm.size() + kNumNops * 4u, 0u);
  expected_asm.insert(
      expected_asm.end(),
      expected_asm_kMips_adjust_tail,
      expected_asm_kMips_adjust_tail + arraysize(expected_asm_kMips_adjust_tail));
  std::vector<uint8_t> expected_cfi(
      expected_cfi_kMips_adjust,
      expected_cfi_kMips_adjust + arraysize(expected_cfi_kMips_adjust));
  SetUpFrame(InstructionSet::kMips);
#define __ down_cast<mips::MipsAssembler*>(GetCodeGenerator()->GetAssembler())->
  mips::MipsLabel target;
  __ Beqz(mips::A0, &target);
  // Push the target out of range of BEQZ.
  for (size_t i = 0; i != kNumNops; ++i) {
    __ Nop();
  }
  __ Bind(&target);
#undef __
  Finish();
  Check(InstructionSet::kMips, "kMips_adjust", expected_asm, expected_cfi);
}
#endif

#ifdef ART_ENABLE_CODEGEN_mips64
TEST_F(OptimizingCFITest, kMips64Adjust) {
  // One NOP in forbidden slot, 1 << 15 NOPS have size 1 << 17 which exceeds 18-bit signed maximum.
  static constexpr size_t kNumNops = 1u + (1u << 15);
  std::vector<uint8_t> expected_asm(
      expected_asm_kMips64_adjust_head,
      expected_asm_kMips64_adjust_head + arraysize(expected_asm_kMips64_adjust_head));
  expected_asm.resize(expected_asm.size() + kNumNops * 4u, 0u);
  expected_asm.insert(
      expected_asm.end(),
      expected_asm_kMips64_adjust_tail,
      expected_asm_kMips64_adjust_tail + arraysize(expected_asm_kMips64_adjust_tail));
  std::vector<uint8_t> expected_cfi(
      expected_cfi_kMips64_adjust,
      expected_cfi_kMips64_adjust + arraysize(expected_cfi_kMips64_adjust));
  SetUpFrame(InstructionSet::kMips64);
#define __ down_cast<mips64::Mips64Assembler*>(GetCodeGenerator()->GetAssembler())->
  mips64::Mips64Label target;
  __ Beqc(mips64::A1, mips64::A2, &target);
  // Push the target out of range of BEQC.
  for (size_t i = 0; i != kNumNops; ++i) {
    __ Nop();
  }
  __ Bind(&target);
#undef __
  Finish();
  Check(InstructionSet::kMips64, "kMips64_adjust", expected_asm, expected_cfi);
}
#endif

#endif  // ART_TARGET_ANDROID

}  // namespace art
