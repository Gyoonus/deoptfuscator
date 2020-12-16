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

#include "graph_checker.h"
#include "optimizing_unit_test.h"

namespace art {

class GraphCheckerTest : public OptimizingUnitTest {
 protected:
  HGraph* CreateSimpleCFG();
  void TestCode(const std::vector<uint16_t>& data);
};

/**
 * Create a simple control-flow graph composed of two blocks:
 *
 *   BasicBlock 0, succ: 1
 *     0: ReturnVoid 1
 *   BasicBlock 1, pred: 0
 *     1: Exit
 */
HGraph* GraphCheckerTest::CreateSimpleCFG() {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = new (GetAllocator()) HBasicBlock(graph);
  entry_block->AddInstruction(new (GetAllocator()) HReturnVoid());
  graph->AddBlock(entry_block);
  graph->SetEntryBlock(entry_block);
  HBasicBlock* exit_block = new (GetAllocator()) HBasicBlock(graph);
  exit_block->AddInstruction(new (GetAllocator()) HExit());
  graph->AddBlock(exit_block);
  graph->SetExitBlock(exit_block);
  entry_block->AddSuccessor(exit_block);
  graph->BuildDominatorTree();
  return graph;
}

void GraphCheckerTest::TestCode(const std::vector<uint16_t>& data) {
  HGraph* graph = CreateCFG(data);
  ASSERT_NE(graph, nullptr);

  GraphChecker graph_checker(graph);
  graph_checker.Run();
  ASSERT_TRUE(graph_checker.IsValid());
}

TEST_F(GraphCheckerTest, ReturnVoid) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
      Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(GraphCheckerTest, CFG1) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
      Instruction::GOTO | 0x100,
      Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(GraphCheckerTest, CFG2) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(GraphCheckerTest, CFG3) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0xFF00);

  TestCode(data);
}

// Test case with an invalid graph containing inconsistent
// predecessor/successor arcs in CFG.
TEST_F(GraphCheckerTest, InconsistentPredecessorsAndSuccessors) {
  HGraph* graph = CreateSimpleCFG();
  GraphChecker graph_checker(graph);
  graph_checker.Run();
  ASSERT_TRUE(graph_checker.IsValid());

  // Remove the entry block from the exit block's predecessors, to create an
  // inconsistent successor/predecessor relation.
  graph->GetExitBlock()->RemovePredecessor(graph->GetEntryBlock());
  graph_checker.Run();
  ASSERT_FALSE(graph_checker.IsValid());
}

// Test case with an invalid graph containing a non-branch last
// instruction in a block.
TEST_F(GraphCheckerTest, BlockEndingWithNonBranchInstruction) {
  HGraph* graph = CreateSimpleCFG();
  GraphChecker graph_checker(graph);
  graph_checker.Run();
  ASSERT_TRUE(graph_checker.IsValid());

  // Remove the sole instruction of the exit block (composed of a
  // single Exit instruction) to make it invalid (i.e. not ending by a
  // branch instruction).
  HBasicBlock* exit_block = graph->GetExitBlock();
  HInstruction* last_inst = exit_block->GetLastInstruction();
  exit_block->RemoveInstruction(last_inst);

  graph_checker.Run();
  ASSERT_FALSE(graph_checker.IsValid());
}

TEST_F(GraphCheckerTest, SSAPhi) {
  // This code creates one Phi function during the conversion to SSA form.
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::RETURN | 0 << 8);

  TestCode(data);
}

}  // namespace art
