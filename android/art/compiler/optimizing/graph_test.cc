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

#include "base/arena_allocator.h"
#include "builder.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"

#include "gtest/gtest.h"

namespace art {

class GraphTest : public OptimizingUnitTest {
 protected:
  HBasicBlock* CreateIfBlock(HGraph* graph);
  HBasicBlock* CreateGotoBlock(HGraph* graph);
  HBasicBlock* CreateEntryBlock(HGraph* graph);
  HBasicBlock* CreateReturnBlock(HGraph* graph);
  HBasicBlock* CreateExitBlock(HGraph* graph);
};

HBasicBlock* GraphTest::CreateIfBlock(HGraph* graph) {
  HBasicBlock* if_block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(if_block);
  HInstruction* instr = graph->GetIntConstant(4);
  HInstruction* equal = new (GetAllocator()) HEqual(instr, instr);
  if_block->AddInstruction(equal);
  instr = new (GetAllocator()) HIf(equal);
  if_block->AddInstruction(instr);
  return if_block;
}

HBasicBlock* GraphTest::CreateGotoBlock(HGraph* graph) {
  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  HInstruction* got = new (GetAllocator()) HGoto();
  block->AddInstruction(got);
  return block;
}

HBasicBlock* GraphTest::CreateEntryBlock(HGraph* graph) {
  HBasicBlock* block = CreateGotoBlock(graph);
  graph->SetEntryBlock(block);
  return block;
}

HBasicBlock* GraphTest::CreateReturnBlock(HGraph* graph) {
  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  HInstruction* return_instr = new (GetAllocator()) HReturnVoid();
  block->AddInstruction(return_instr);
  return block;
}

HBasicBlock* GraphTest::CreateExitBlock(HGraph* graph) {
  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  HInstruction* exit_instr = new (GetAllocator()) HExit();
  block->AddInstruction(exit_instr);
  return block;
}


// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the false block to be the return block.
TEST_F(GraphTest, IfSuccessorSimpleJoinBlock1) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* if_true = CreateGotoBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);
  HBasicBlock* exit_block = CreateExitBlock(graph);

  entry_block->AddSuccessor(if_block);
  if_block->AddSuccessor(if_true);
  if_true->AddSuccessor(return_block);
  if_block->AddSuccessor(return_block);
  return_block->AddSuccessor(exit_block);

  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), if_true);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), return_block);

  graph->SimplifyCFG();

  // Ensure we still have the same if true block.
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), if_true);

  // Ensure the critical edge has been removed.
  HBasicBlock* false_block = if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor();
  ASSERT_NE(false_block, return_block);

  // Ensure the new block branches to the join block.
  ASSERT_EQ(false_block->GetSuccessors()[0], return_block);
}

// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the true block to be the return block.
TEST_F(GraphTest, IfSuccessorSimpleJoinBlock2) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* if_false = CreateGotoBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);
  HBasicBlock* exit_block = CreateExitBlock(graph);

  entry_block->AddSuccessor(if_block);
  if_block->AddSuccessor(return_block);
  if_false->AddSuccessor(return_block);
  if_block->AddSuccessor(if_false);
  return_block->AddSuccessor(exit_block);

  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), return_block);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), if_false);

  graph->SimplifyCFG();

  // Ensure we still have the same if true block.
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), if_false);

  // Ensure the critical edge has been removed.
  HBasicBlock* true_block = if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor();
  ASSERT_NE(true_block, return_block);

  // Ensure the new block branches to the join block.
  ASSERT_EQ(true_block->GetSuccessors()[0], return_block);
}

// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the true block to be the loop header.
TEST_F(GraphTest, IfSuccessorMultipleBackEdges1) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);
  HBasicBlock* exit_block = CreateExitBlock(graph);

  entry_block->AddSuccessor(if_block);
  if_block->AddSuccessor(if_block);
  if_block->AddSuccessor(return_block);
  return_block->AddSuccessor(exit_block);

  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), if_block);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), return_block);

  graph->BuildDominatorTree();

  // Ensure we still have the same if false block.
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), return_block);

  // Ensure there is only one back edge.
  ASSERT_EQ(if_block->GetPredecessors().size(), 2u);
  ASSERT_EQ(if_block->GetPredecessors()[0], entry_block->GetSingleSuccessor());
  ASSERT_NE(if_block->GetPredecessors()[1], if_block);

  // Ensure the new block is the back edge.
  ASSERT_EQ(if_block->GetPredecessors()[1],
            if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor());
}

// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the false block to be the loop header.
TEST_F(GraphTest, IfSuccessorMultipleBackEdges2) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);
  HBasicBlock* exit_block = CreateExitBlock(graph);

  entry_block->AddSuccessor(if_block);
  if_block->AddSuccessor(return_block);
  if_block->AddSuccessor(if_block);
  return_block->AddSuccessor(exit_block);

  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), return_block);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), if_block);

  graph->BuildDominatorTree();

  // Ensure we still have the same if true block.
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), return_block);

  // Ensure there is only one back edge.
  ASSERT_EQ(if_block->GetPredecessors().size(), 2u);
  ASSERT_EQ(if_block->GetPredecessors()[0], entry_block->GetSingleSuccessor());
  ASSERT_NE(if_block->GetPredecessors()[1], if_block);

  // Ensure the new block is the back edge.
  ASSERT_EQ(if_block->GetPredecessors()[1],
            if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor());
}

// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the true block to be a loop header with multiple pre headers.
TEST_F(GraphTest, IfSuccessorMultiplePreHeaders1) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* first_if_block = CreateIfBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* loop_block = CreateGotoBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);

  entry_block->AddSuccessor(first_if_block);
  first_if_block->AddSuccessor(if_block);
  first_if_block->AddSuccessor(loop_block);
  loop_block->AddSuccessor(loop_block);
  if_block->AddSuccessor(loop_block);
  if_block->AddSuccessor(return_block);


  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), loop_block);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), return_block);

  graph->BuildDominatorTree();

  HIf* if_instr = if_block->GetLastInstruction()->AsIf();
  // Ensure we still have the same if false block.
  ASSERT_EQ(if_instr->IfFalseSuccessor(), return_block);

  // Ensure there is only one pre header..
  ASSERT_EQ(loop_block->GetPredecessors().size(), 2u);

  // Ensure the new block is the successor of the true block.
  ASSERT_EQ(if_instr->IfTrueSuccessor()->GetSuccessors().size(), 1u);
  ASSERT_EQ(if_instr->IfTrueSuccessor()->GetSuccessors()[0],
            loop_block->GetLoopInformation()->GetPreHeader());
}

// Test that the successors of an if block stay consistent after a SimplifyCFG.
// This test sets the false block to be a loop header with multiple pre headers.
TEST_F(GraphTest, IfSuccessorMultiplePreHeaders2) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry_block = CreateEntryBlock(graph);
  HBasicBlock* first_if_block = CreateIfBlock(graph);
  HBasicBlock* if_block = CreateIfBlock(graph);
  HBasicBlock* loop_block = CreateGotoBlock(graph);
  HBasicBlock* return_block = CreateReturnBlock(graph);

  entry_block->AddSuccessor(first_if_block);
  first_if_block->AddSuccessor(if_block);
  first_if_block->AddSuccessor(loop_block);
  loop_block->AddSuccessor(loop_block);
  if_block->AddSuccessor(return_block);
  if_block->AddSuccessor(loop_block);

  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfTrueSuccessor(), return_block);
  ASSERT_EQ(if_block->GetLastInstruction()->AsIf()->IfFalseSuccessor(), loop_block);

  graph->BuildDominatorTree();

  HIf* if_instr = if_block->GetLastInstruction()->AsIf();
  // Ensure we still have the same if true block.
  ASSERT_EQ(if_instr->IfTrueSuccessor(), return_block);

  // Ensure there is only one pre header..
  ASSERT_EQ(loop_block->GetPredecessors().size(), 2u);

  // Ensure the new block is the successor of the false block.
  ASSERT_EQ(if_instr->IfFalseSuccessor()->GetSuccessors().size(), 1u);
  ASSERT_EQ(if_instr->IfFalseSuccessor()->GetSuccessors()[0],
            loop_block->GetLoopInformation()->GetPreHeader());
}

TEST_F(GraphTest, InsertInstructionBefore) {
  HGraph* graph = CreateGraph();
  HBasicBlock* block = CreateGotoBlock(graph);
  HInstruction* got = block->GetLastInstruction();
  ASSERT_TRUE(got->IsControlFlow());

  // Test at the beginning of the block.
  HInstruction* first_instruction = new (GetAllocator()) HIntConstant(4);
  block->InsertInstructionBefore(first_instruction, got);

  ASSERT_NE(first_instruction->GetId(), -1);
  ASSERT_EQ(first_instruction->GetBlock(), block);
  ASSERT_EQ(block->GetFirstInstruction(), first_instruction);
  ASSERT_EQ(block->GetLastInstruction(), got);
  ASSERT_EQ(first_instruction->GetNext(), got);
  ASSERT_EQ(first_instruction->GetPrevious(), nullptr);
  ASSERT_EQ(got->GetNext(), nullptr);
  ASSERT_EQ(got->GetPrevious(), first_instruction);

  // Test in the middle of the block.
  HInstruction* second_instruction = new (GetAllocator()) HIntConstant(4);
  block->InsertInstructionBefore(second_instruction, got);

  ASSERT_NE(second_instruction->GetId(), -1);
  ASSERT_EQ(second_instruction->GetBlock(), block);
  ASSERT_EQ(block->GetFirstInstruction(), first_instruction);
  ASSERT_EQ(block->GetLastInstruction(), got);
  ASSERT_EQ(first_instruction->GetNext(), second_instruction);
  ASSERT_EQ(first_instruction->GetPrevious(), nullptr);
  ASSERT_EQ(second_instruction->GetNext(), got);
  ASSERT_EQ(second_instruction->GetPrevious(), first_instruction);
  ASSERT_EQ(got->GetNext(), nullptr);
  ASSERT_EQ(got->GetPrevious(), second_instruction);
}

}  // namespace art
