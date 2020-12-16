/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "ssa_liveness_analysis.h"

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "base/arena_allocator.h"
#include "base/arena_containers.h"
#include "code_generator.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

class SsaLivenessAnalysisTest : public OptimizingUnitTest {
 public:
  SsaLivenessAnalysisTest()
      : graph_(CreateGraph()),
        compiler_options_(),
        instruction_set_(kRuntimeISA) {
    std::string error_msg;
    instruction_set_features_ =
        InstructionSetFeatures::FromVariant(instruction_set_, "default", &error_msg);
    codegen_ = CodeGenerator::Create(graph_,
                                     instruction_set_,
                                     *instruction_set_features_,
                                     compiler_options_);
    CHECK(codegen_ != nullptr) << instruction_set_ << " is not a supported target architecture.";
    // Create entry block.
    entry_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_);
    graph_->SetEntryBlock(entry_);
  }

 protected:
  HBasicBlock* CreateSuccessor(HBasicBlock* block) {
    HGraph* graph = block->GetGraph();
    HBasicBlock* successor = new (GetAllocator()) HBasicBlock(graph);
    graph->AddBlock(successor);
    block->AddSuccessor(successor);
    return successor;
  }

  HGraph* graph_;
  CompilerOptions compiler_options_;
  InstructionSet instruction_set_;
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;
  std::unique_ptr<CodeGenerator> codegen_;
  HBasicBlock* entry_;
};

TEST_F(SsaLivenessAnalysisTest, TestReturnArg) {
  HInstruction* arg = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry_->AddInstruction(arg);

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* ret = new (GetAllocator()) HReturn(arg);
  block->AddInstruction(ret);
  block->AddInstruction(new (GetAllocator()) HExit());

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  std::ostringstream arg_dump;
  arg->GetLiveInterval()->Dump(arg_dump);
  EXPECT_STREQ("ranges: { [2,6) }, uses: { 6 }, { } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
               arg_dump.str().c_str());
}

TEST_F(SsaLivenessAnalysisTest, TestAput) {
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* value = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(2), 2, DataType::Type::kInt32);
  HInstruction* extra_arg1 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(3), 3, DataType::Type::kInt32);
  HInstruction* extra_arg2 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(4), 4, DataType::Type::kReference);
  HInstruction* const args[] = { array, index, value, extra_arg1, extra_arg2 };
  for (HInstruction* insn : args) {
    entry_->AddInstruction(insn);
  }

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* null_check = new (GetAllocator()) HNullCheck(array, 0);
  block->AddInstruction(null_check);
  HEnvironment* null_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs */ 5,
                                                                   /* method */ nullptr,
                                                                   /* dex_pc */ 0u,
                                                                   null_check);
  null_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  null_check->SetRawEnvironment(null_check_env);
  HInstruction* length = new (GetAllocator()) HArrayLength(array, 0);
  block->AddInstruction(length);
  HInstruction* bounds_check = new (GetAllocator()) HBoundsCheck(index, length, /* dex_pc */ 0u);
  block->AddInstruction(bounds_check);
  HEnvironment* bounds_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                     /* number_of_vregs */ 5,
                                                                     /* method */ nullptr,
                                                                     /* dex_pc */ 0u,
                                                                     bounds_check);
  bounds_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  bounds_check->SetRawEnvironment(bounds_check_env);
  HInstruction* array_set =
      new (GetAllocator()) HArraySet(array, index, value, DataType::Type::kInt32, /* dex_pc */ 0);
  block->AddInstruction(array_set);

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  EXPECT_FALSE(graph_->IsDebuggable());
  EXPECT_EQ(18u, bounds_check->GetLifetimePosition());
  static const char* const expected[] = {
      "ranges: { [2,21) }, uses: { 15 17 21 }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [4,21) }, uses: { 19 21 }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [6,21) }, uses: { 21 }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      // Environment uses do not keep the non-reference argument alive.
      "ranges: { [8,10) }, uses: { }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment uses keep the reference argument alive.
      "ranges: { [10,19) }, uses: { }, { 15 19 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
  };
  static_assert(arraysize(expected) == arraysize(args), "Array size check.");
  size_t arg_index = 0u;
  for (HInstruction* arg : args) {
    std::ostringstream arg_dump;
    arg->GetLiveInterval()->Dump(arg_dump);
    EXPECT_STREQ(expected[arg_index], arg_dump.str().c_str()) << arg_index;
    ++arg_index;
  }
}

TEST_F(SsaLivenessAnalysisTest, TestDeoptimize) {
  HInstruction* array = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  HInstruction* index = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(1), 1, DataType::Type::kInt32);
  HInstruction* value = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(2), 2, DataType::Type::kInt32);
  HInstruction* extra_arg1 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(3), 3, DataType::Type::kInt32);
  HInstruction* extra_arg2 = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(4), 4, DataType::Type::kReference);
  HInstruction* const args[] = { array, index, value, extra_arg1, extra_arg2 };
  for (HInstruction* insn : args) {
    entry_->AddInstruction(insn);
  }

  HBasicBlock* block = CreateSuccessor(entry_);
  HInstruction* null_check = new (GetAllocator()) HNullCheck(array, 0);
  block->AddInstruction(null_check);
  HEnvironment* null_check_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs */ 5,
                                                                   /* method */ nullptr,
                                                                   /* dex_pc */ 0u,
                                                                   null_check);
  null_check_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  null_check->SetRawEnvironment(null_check_env);
  HInstruction* length = new (GetAllocator()) HArrayLength(array, 0);
  block->AddInstruction(length);
  // Use HAboveOrEqual+HDeoptimize as the bounds check.
  HInstruction* ae = new (GetAllocator()) HAboveOrEqual(index, length);
  block->AddInstruction(ae);
  HInstruction* deoptimize = new(GetAllocator()) HDeoptimize(
      GetAllocator(), ae, DeoptimizationKind::kBlockBCE, /* dex_pc */ 0u);
  block->AddInstruction(deoptimize);
  HEnvironment* deoptimize_env = new (GetAllocator()) HEnvironment(GetAllocator(),
                                                                   /* number_of_vregs */ 5,
                                                                   /* method */ nullptr,
                                                                   /* dex_pc */ 0u,
                                                                   deoptimize);
  deoptimize_env->CopyFrom(ArrayRef<HInstruction* const>(args));
  deoptimize->SetRawEnvironment(deoptimize_env);
  HInstruction* array_set =
      new (GetAllocator()) HArraySet(array, index, value, DataType::Type::kInt32, /* dex_pc */ 0);
  block->AddInstruction(array_set);

  graph_->BuildDominatorTree();
  SsaLivenessAnalysis ssa_analysis(graph_, codegen_.get(), GetScopedAllocator());
  ssa_analysis.Analyze();

  EXPECT_FALSE(graph_->IsDebuggable());
  EXPECT_EQ(20u, deoptimize->GetLifetimePosition());
  static const char* const expected[] = {
      "ranges: { [2,23) }, uses: { 15 17 23 }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [4,23) }, uses: { 19 23 }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 "
          "is_high: 0",
      "ranges: { [6,23) }, uses: { 23 }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment use in HDeoptimize keeps even the non-reference argument alive.
      "ranges: { [8,21) }, uses: { }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
      // Environment uses keep the reference argument alive.
      "ranges: { [10,21) }, uses: { }, { 15 21 } is_fixed: 0, is_split: 0 is_low: 0 is_high: 0",
  };
  static_assert(arraysize(expected) == arraysize(args), "Array size check.");
  size_t arg_index = 0u;
  for (HInstruction* arg : args) {
    std::ostringstream arg_dump;
    arg->GetLiveInterval()->Dump(arg_dump);
    EXPECT_STREQ(expected[arg_index], arg_dump.str().c_str()) << arg_index;
    ++arg_index;
  }
}

}  // namespace art
