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

#include "graph_checker.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "superblock_cloner.h"

#include "gtest/gtest.h"

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;

// This class provides methods and helpers for testing various cloning and copying routines:
// individual instruction cloning and cloning of the more coarse-grain structures.
class SuperblockClonerTest : public OptimizingUnitTest {
 public:
  SuperblockClonerTest()
      : graph_(CreateGraph()), entry_block_(nullptr), exit_block_(nullptr), parameter_(nullptr) {}

  void CreateBasicLoopControlFlow(/* out */ HBasicBlock** header_p,
                                  /* out */ HBasicBlock** body_p) {
    entry_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->SetEntryBlock(entry_block_);

    HBasicBlock* loop_preheader = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_header = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_body = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_exit = new (GetAllocator()) HBasicBlock(graph_);

    graph_->AddBlock(loop_preheader);
    graph_->AddBlock(loop_header);
    graph_->AddBlock(loop_body);
    graph_->AddBlock(loop_exit);

    exit_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(exit_block_);
    graph_->SetExitBlock(exit_block_);

    entry_block_->AddSuccessor(loop_preheader);
    loop_preheader->AddSuccessor(loop_header);
    // Loop exit first to have a proper exit condition/target for HIf.
    loop_header->AddSuccessor(loop_exit);
    loop_header->AddSuccessor(loop_body);
    loop_body->AddSuccessor(loop_header);
    loop_exit->AddSuccessor(exit_block_);

    *header_p = loop_header;
    *body_p = loop_body;

    parameter_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                      dex::TypeIndex(0),
                                                      0,
                                                      DataType::Type::kInt32);
    entry_block_->AddInstruction(parameter_);
    loop_exit->AddInstruction(new (GetAllocator()) HReturnVoid());
    exit_block_->AddInstruction(new (GetAllocator()) HExit());
  }

  void CreateBasicLoopDataFlow(HBasicBlock* loop_header, HBasicBlock* loop_body) {
    uint32_t dex_pc = 0;

    // Entry block.
    HIntConstant* const_0 = graph_->GetIntConstant(0);
    HIntConstant* const_1 = graph_->GetIntConstant(1);
    HIntConstant* const_128 = graph_->GetIntConstant(128);

    // Header block.
    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    HInstruction* suspend_check = new (GetAllocator()) HSuspendCheck();

    loop_header->AddPhi(phi);
    loop_header->AddInstruction(suspend_check);
    loop_header->AddInstruction(new (GetAllocator()) HGreaterThanOrEqual(phi, const_128));
    loop_header->AddInstruction(new (GetAllocator()) HIf(parameter_));

    // Loop body block.
    HInstruction* null_check = new (GetAllocator()) HNullCheck(parameter_, dex_pc);
    HInstruction* array_length = new (GetAllocator()) HArrayLength(null_check, dex_pc);
    HInstruction* bounds_check = new (GetAllocator()) HBoundsCheck(phi, array_length, dex_pc);
    HInstruction* array_get =
        new (GetAllocator()) HArrayGet(null_check, bounds_check, DataType::Type::kInt32, dex_pc);
    HInstruction* add =  new (GetAllocator()) HAdd(DataType::Type::kInt32, array_get, const_1);
    HInstruction* array_set =
        new (GetAllocator()) HArraySet(null_check, bounds_check, add, DataType::Type::kInt32, dex_pc);
    HInstruction* induction_inc = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi, const_1);

    loop_body->AddInstruction(null_check);
    loop_body->AddInstruction(array_length);
    loop_body->AddInstruction(bounds_check);
    loop_body->AddInstruction(array_get);
    loop_body->AddInstruction(add);
    loop_body->AddInstruction(array_set);
    loop_body->AddInstruction(induction_inc);
    loop_body->AddInstruction(new (GetAllocator()) HGoto());

    phi->AddInput(const_0);
    phi->AddInput(induction_inc);

    graph_->SetHasBoundsChecks(true);

    // Adjust HEnvironment for each instruction which require that.
    ArenaVector<HInstruction*> current_locals({phi, const_128, parameter_},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));

    HEnvironment* env = ManuallyBuildEnvFor(suspend_check, &current_locals);
    null_check->CopyEnvironmentFrom(env);
    bounds_check->CopyEnvironmentFrom(env);
  }

  HEnvironment* ManuallyBuildEnvFor(HInstruction* instruction,
                                    ArenaVector<HInstruction*>* current_locals) {
    HEnvironment* environment = new (GetAllocator()) HEnvironment(
        (GetAllocator()),
        current_locals->size(),
        graph_->GetArtMethod(),
        instruction->GetDexPc(),
        instruction);

    environment->CopyFrom(ArrayRef<HInstruction* const>(*current_locals));
    instruction->SetRawEnvironment(environment);
    return environment;
  }

  bool CheckGraph() {
    GraphChecker checker(graph_);
    checker.Run();
    if (!checker.IsValid()) {
      for (const std::string& error : checker.GetErrors()) {
        std::cout << error << std::endl;
      }
      return false;
    }
    return true;
  }

  HGraph* graph_;

  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;

  HInstruction* parameter_;
};

TEST_F(SuperblockClonerTest, IndividualInstrCloner) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  CreateBasicLoopControlFlow(&header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  HSuspendCheck* old_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  CloneAndReplaceInstructionVisitor visitor(graph_);
  // Do instruction cloning and replacement twice with different visiting order.

  visitor.VisitInsertionOrder();
  size_t instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 12u);
  EXPECT_TRUE(CheckGraph());

  visitor.VisitReversePostOrder();
  instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 24u);
  EXPECT_TRUE(CheckGraph());

  HSuspendCheck* new_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  EXPECT_NE(new_suspend_check, old_suspend_check);
  EXPECT_NE(new_suspend_check, nullptr);
}

// Tests SuperblockCloner::CloneBasicBlocks - check instruction cloning and initial remapping of
// instructions' inputs.
TEST_F(SuperblockClonerTest, CloneBasicBlocks) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  CreateBasicLoopControlFlow(&header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  HBasicBlockMap bb_map(std::less<HBasicBlock*>(), arena->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(std::less<HInstruction*>(), arena->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  orig_bb_set.Union(&loop_info->GetBlocks());

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          &bb_map,
                          &hir_map);
  EXPECT_TRUE(cloner.IsSubgraphClonable());

  cloner.CloneBasicBlocks();

  EXPECT_EQ(bb_map.size(), 2u);
  EXPECT_EQ(hir_map.size(), 12u);

  for (auto it : hir_map) {
    HInstruction* orig_instr = it.first;
    HInstruction* copy_instr = it.second;

    EXPECT_EQ(cloner.GetBlockCopy(orig_instr->GetBlock()), copy_instr->GetBlock());
    EXPECT_EQ(orig_instr->GetKind(), copy_instr->GetKind());
    EXPECT_EQ(orig_instr->GetType(), copy_instr->GetType());

    if (orig_instr->IsPhi()) {
      continue;
    }

    EXPECT_EQ(orig_instr->InputCount(), copy_instr->InputCount());

    // Check that inputs match.
    for (size_t i = 0, e = orig_instr->InputCount(); i < e; i++) {
      HInstruction* orig_input = orig_instr->InputAt(i);
      HInstruction* copy_input = copy_instr->InputAt(i);
      if (cloner.IsInOrigBBSet(orig_input->GetBlock())) {
        EXPECT_EQ(cloner.GetInstrCopy(orig_input), copy_input);
      } else {
        EXPECT_EQ(orig_input, copy_input);
      }
    }

    EXPECT_EQ(orig_instr->HasEnvironment(), copy_instr->HasEnvironment());

    // Check that environments match.
    if (orig_instr->HasEnvironment()) {
      HEnvironment* orig_env = orig_instr->GetEnvironment();
      HEnvironment* copy_env = copy_instr->GetEnvironment();

      EXPECT_EQ(copy_env->GetParent(), nullptr);
      EXPECT_EQ(orig_env->Size(), copy_env->Size());

      for (size_t i = 0, e = orig_env->Size(); i < e; i++) {
        HInstruction* orig_input = orig_env->GetInstructionAt(i);
        HInstruction* copy_input = copy_env->GetInstructionAt(i);
        if (cloner.IsInOrigBBSet(orig_input->GetBlock())) {
          EXPECT_EQ(cloner.GetInstrCopy(orig_input), copy_input);
        } else {
          EXPECT_EQ(orig_input, copy_input);
        }
      }
    }
  }
}

// SuperblockCloner::CleanUpControlFlow - checks algorithms of local adjustments of the control
// flow.
TEST_F(SuperblockClonerTest, AdjustControlFlowInfo) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  CreateBasicLoopControlFlow(&header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);

  HLoopInformation* loop_info = header->GetLoopInformation();
  orig_bb_set.Union(&loop_info->GetBlocks());

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          nullptr,
                          nullptr);
  EXPECT_TRUE(cloner.IsSubgraphClonable());

  cloner.FindAndSetLocalAreaForAdjustments();
  cloner.CleanUpControlFlow();

  EXPECT_TRUE(CheckGraph());

  EXPECT_TRUE(entry_block_->Dominates(header));
  EXPECT_TRUE(entry_block_->Dominates(exit_block_));

  EXPECT_EQ(header->GetLoopInformation(), loop_info);
  EXPECT_EQ(loop_info->GetHeader(), header);
  EXPECT_TRUE(loop_info->Contains(*loop_body));
  EXPECT_TRUE(loop_info->IsBackEdge(*loop_body));
}

}  // namespace art
