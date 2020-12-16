/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "loop_optimization.h"
#include "optimizing_unit_test.h"

namespace art {

/**
 * Fixture class for the loop optimization tests. These unit tests focus
 * constructing the loop hierarchy. Actual optimizations are tested
 * through the checker tests.
 */
class LoopOptimizationTest : public OptimizingUnitTest {
 public:
  LoopOptimizationTest()
      : graph_(CreateGraph()),
        iva_(new (GetAllocator()) HInductionVarAnalysis(graph_)),
        loop_opt_(new (GetAllocator()) HLoopOptimization(graph_, nullptr, iva_, nullptr)) {
    BuildGraph();
  }

  ~LoopOptimizationTest() { }

  /** Constructs bare minimum graph. */
  void BuildGraph() {
    graph_->SetNumberOfVRegs(1);
    entry_block_ = new (GetAllocator()) HBasicBlock(graph_);
    return_block_ = new (GetAllocator()) HBasicBlock(graph_);
    exit_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->AddBlock(return_block_);
    graph_->AddBlock(exit_block_);
    graph_->SetEntryBlock(entry_block_);
    graph_->SetExitBlock(exit_block_);
    parameter_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                      dex::TypeIndex(0),
                                                      0,
                                                      DataType::Type::kInt32);
    entry_block_->AddInstruction(parameter_);
    return_block_->AddInstruction(new (GetAllocator()) HReturnVoid());
    exit_block_->AddInstruction(new (GetAllocator()) HExit());
    entry_block_->AddSuccessor(return_block_);
    return_block_->AddSuccessor(exit_block_);
  }

  /** Adds a loop nest at given position before successor. */
  HBasicBlock* AddLoop(HBasicBlock* position, HBasicBlock* successor) {
    HBasicBlock* header = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* body = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(header);
    graph_->AddBlock(body);
    // Control flow.
    position->ReplaceSuccessor(successor, header);
    header->AddSuccessor(body);
    header->AddSuccessor(successor);
    header->AddInstruction(new (GetAllocator()) HIf(parameter_));
    body->AddSuccessor(header);
    body->AddInstruction(new (GetAllocator()) HGoto());
    return header;
  }

  /** Performs analysis. */
  void PerformAnalysis() {
    graph_->BuildDominatorTree();
    iva_->Run();
    // Do not release the loop hierarchy.
    ScopedArenaAllocator loop_allocator(GetArenaStack());
    loop_opt_->loop_allocator_ = &loop_allocator;
    loop_opt_->LocalRun();
  }

  /** Constructs string representation of computed loop hierarchy. */
  std::string LoopStructure() {
    return LoopStructureRecurse(loop_opt_->top_loop_);
  }

  // Helper method
  std::string LoopStructureRecurse(HLoopOptimization::LoopNode* node) {
    std::string s;
    for ( ; node != nullptr; node = node->next) {
      s.append("[");
      s.append(LoopStructureRecurse(node->inner));
      s.append("]");
    }
    return s;
  }

  // General building fields.
  HGraph* graph_;
  HInductionVarAnalysis* iva_;
  HLoopOptimization* loop_opt_;

  HBasicBlock* entry_block_;
  HBasicBlock* return_block_;
  HBasicBlock* exit_block_;

  HInstruction* parameter_;
};

//
// The actual tests.
//

TEST_F(LoopOptimizationTest, NoLoops) {
  PerformAnalysis();
  EXPECT_EQ("", LoopStructure());
}

TEST_F(LoopOptimizationTest, SingleLoop) {
  AddLoop(entry_block_, return_block_);
  PerformAnalysis();
  EXPECT_EQ("[]", LoopStructure());
}

TEST_F(LoopOptimizationTest, LoopNest10) {
  HBasicBlock* b = entry_block_;
  HBasicBlock* s = return_block_;
  for (int i = 0; i < 10; i++) {
    s = AddLoop(b, s);
    b = s->GetSuccessors()[0];
  }
  PerformAnalysis();
  EXPECT_EQ("[[[[[[[[[[]]]]]]]]]]", LoopStructure());
}

TEST_F(LoopOptimizationTest, LoopSequence10) {
  HBasicBlock* b = entry_block_;
  HBasicBlock* s = return_block_;
  for (int i = 0; i < 10; i++) {
    b = AddLoop(b, s);
    s = b->GetSuccessors()[1];
  }
  PerformAnalysis();
  EXPECT_EQ("[][][][][][][][][][]", LoopStructure());
}

TEST_F(LoopOptimizationTest, LoopSequenceOfNests) {
  HBasicBlock* b = entry_block_;
  HBasicBlock* s = return_block_;
  for (int i = 0; i < 10; i++) {
    b = AddLoop(b, s);
    s = b->GetSuccessors()[1];
    HBasicBlock* bi = b->GetSuccessors()[0];
    HBasicBlock* si = b;
    for (int j = 0; j < i; j++) {
      si = AddLoop(bi, si);
      bi = si->GetSuccessors()[0];
    }
  }
  PerformAnalysis();
  EXPECT_EQ("[]"
            "[[]]"
            "[[[]]]"
            "[[[[]]]]"
            "[[[[[]]]]]"
            "[[[[[[]]]]]]"
            "[[[[[[[]]]]]]]"
            "[[[[[[[[]]]]]]]]"
            "[[[[[[[[[]]]]]]]]]"
            "[[[[[[[[[[]]]]]]]]]]",
            LoopStructure());
}

TEST_F(LoopOptimizationTest, LoopNestWithSequence) {
  HBasicBlock* b = entry_block_;
  HBasicBlock* s = return_block_;
  for (int i = 0; i < 10; i++) {
    s = AddLoop(b, s);
    b = s->GetSuccessors()[0];
  }
  b = s;
  s = b->GetSuccessors()[1];
  for (int i = 0; i < 9; i++) {
    b = AddLoop(b, s);
    s = b->GetSuccessors()[1];
  }
  PerformAnalysis();
  EXPECT_EQ("[[[[[[[[[[][][][][][][][][][]]]]]]]]]]", LoopStructure());
}

// Check that SimplifyLoop() doesn't invalidate data flow when ordering loop headers'
// predecessors.
//
// This is a test for nodes.cc functionality - HGraph::SimplifyLoop.
TEST_F(LoopOptimizationTest, SimplifyLoopReoderPredecessors) {
  // Can't use AddLoop as we want special order for blocks predecessors.
  HBasicBlock* header = new (GetAllocator()) HBasicBlock(graph_);
  HBasicBlock* body = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(header);
  graph_->AddBlock(body);

  // Control flow: make a loop back edge first in the list of predecessors.
  entry_block_->RemoveSuccessor(return_block_);
  body->AddSuccessor(header);
  entry_block_->AddSuccessor(header);
  header->AddSuccessor(body);
  header->AddSuccessor(return_block_);
  DCHECK(header->GetSuccessors()[1] == return_block_);

  // Data flow.
  header->AddInstruction(new (GetAllocator()) HIf(parameter_));
  body->AddInstruction(new (GetAllocator()) HGoto());

  HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
  HInstruction* add = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi, parameter_);
  header->AddPhi(phi);
  body->AddInstruction(add);

  phi->AddInput(add);
  phi->AddInput(parameter_);

  graph_->ClearLoopInformation();
  graph_->ClearDominanceInformation();
  graph_->BuildDominatorTree();

  // Check that after optimizations in BuildDominatorTree()/SimplifyCFG() phi inputs
  // are still mapped correctly to the block predecessors.
  for (size_t i = 0, e = phi->InputCount(); i < e; i++) {
    HInstruction* input = phi->InputAt(i);
    ASSERT_TRUE(input->GetBlock()->Dominates(header->GetPredecessors()[i]));
  }
}

// Test that SimplifyLoop() processes the multiple-preheaders loops correctly.
//
// This is a test for nodes.cc functionality - HGraph::SimplifyLoop.
TEST_F(LoopOptimizationTest, SimplifyLoopSinglePreheader) {
  HBasicBlock* header = AddLoop(entry_block_, return_block_);

  header->InsertInstructionBefore(
      new (GetAllocator()) HSuspendCheck(), header->GetLastInstruction());

  // Insert an if construct before the loop so it will have two preheaders.
  HBasicBlock* if_block = new (GetAllocator()) HBasicBlock(graph_);
  HBasicBlock* preheader0 = new (GetAllocator()) HBasicBlock(graph_);
  HBasicBlock* preheader1 = new (GetAllocator()) HBasicBlock(graph_);

  graph_->AddBlock(if_block);
  graph_->AddBlock(preheader0);
  graph_->AddBlock(preheader1);

  // Fix successors/predecessors.
  entry_block_->ReplaceSuccessor(header, if_block);
  if_block->AddSuccessor(preheader0);
  if_block->AddSuccessor(preheader1);
  preheader0->AddSuccessor(header);
  preheader1->AddSuccessor(header);

  if_block->AddInstruction(new (GetAllocator()) HIf(parameter_));
  preheader0->AddInstruction(new (GetAllocator()) HGoto());
  preheader1->AddInstruction(new (GetAllocator()) HGoto());

  HBasicBlock* body = header->GetSuccessors()[0];
  DCHECK(body != return_block_);

  // Add some data flow.
  HIntConstant* const_0 = graph_->GetIntConstant(0);
  HIntConstant* const_1 = graph_->GetIntConstant(1);
  HIntConstant* const_2 = graph_->GetIntConstant(2);

  HAdd* preheader0_add = new (GetAllocator()) HAdd(DataType::Type::kInt32, parameter_, const_0);
  preheader0->AddInstruction(preheader0_add);
  HAdd* preheader1_add = new (GetAllocator()) HAdd(DataType::Type::kInt32, parameter_, const_1);
  preheader1->AddInstruction(preheader1_add);

  HPhi* header_phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
  header->AddPhi(header_phi);

  HAdd* body_add = new (GetAllocator()) HAdd(DataType::Type::kInt32, parameter_, const_2);
  body->AddInstruction(body_add);

  DCHECK(header->GetPredecessors()[0] == body);
  DCHECK(header->GetPredecessors()[1] == preheader0);
  DCHECK(header->GetPredecessors()[2] == preheader1);

  header_phi->AddInput(body_add);
  header_phi->AddInput(preheader0_add);
  header_phi->AddInput(preheader1_add);

  graph_->ClearLoopInformation();
  graph_->ClearDominanceInformation();
  graph_->BuildDominatorTree();

  EXPECT_EQ(header->GetPredecessors().size(), 2u);
  EXPECT_EQ(header->GetPredecessors()[1], body);

  HBasicBlock* new_preheader = header->GetLoopInformation()->GetPreHeader();
  EXPECT_EQ(preheader0->GetSingleSuccessor(), new_preheader);
  EXPECT_EQ(preheader1->GetSingleSuccessor(), new_preheader);

  EXPECT_EQ(new_preheader->GetPhis().CountSize(), 1u);
  HPhi* new_preheader_phi = new_preheader->GetFirstPhi()->AsPhi();
  EXPECT_EQ(new_preheader_phi->InputCount(), 2u);
  EXPECT_EQ(new_preheader_phi->InputAt(0), preheader0_add);
  EXPECT_EQ(new_preheader_phi->InputAt(1), preheader1_add);

  EXPECT_EQ(header_phi->InputCount(), 2u);
  EXPECT_EQ(header_phi->InputAt(0), new_preheader_phi);
  EXPECT_EQ(header_phi->InputAt(1), body_add);
}

}  // namespace art
