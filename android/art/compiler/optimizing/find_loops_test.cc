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
#include "dex/dex_file.h"
#include "dex/dex_instruction.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"
#include "ssa_liveness_analysis.h"

#include "gtest/gtest.h"

namespace art {

class FindLoopsTest : public OptimizingUnitTest {};

TEST_F(FindLoopsTest, CFG1) {
  // Constant is not used.
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN_VOID);

  HGraph* graph = CreateCFG(data);
  for (HBasicBlock* block : graph->GetBlocks()) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  }
}

TEST_F(FindLoopsTest, CFG2) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN);

  HGraph* graph = CreateCFG(data);
  for (HBasicBlock* block : graph->GetBlocks()) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  }
}

TEST_F(FindLoopsTest, CFG3) {
  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 3 << 12 | 0,
    Instruction::CONST_4 | 4 << 12 | 1 << 8,
    Instruction::ADD_INT_2ADDR | 1 << 12,
    Instruction::GOTO | 0x100,
    Instruction::RETURN);

  HGraph* graph = CreateCFG(data);
  for (HBasicBlock* block : graph->GetBlocks()) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  }
}

TEST_F(FindLoopsTest, CFG4) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0x200,
    Instruction::CONST_4 | 5 << 12 | 0,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);
  for (HBasicBlock* block : graph->GetBlocks()) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  }
}

TEST_F(FindLoopsTest, CFG5) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);
  for (HBasicBlock* block : graph->GetBlocks()) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  }
}

static void TestBlock(HGraph* graph,
                      uint32_t block_id,
                      bool is_loop_header,
                      uint32_t parent_loop_header_id,
                      const int* blocks_in_loop = nullptr,
                      size_t number_of_blocks = 0) {
  HBasicBlock* block = graph->GetBlocks()[block_id];
  ASSERT_EQ(block->IsLoopHeader(), is_loop_header);
  if (parent_loop_header_id == kInvalidBlockId) {
    ASSERT_EQ(block->GetLoopInformation(), nullptr);
  } else {
    ASSERT_EQ(block->GetLoopInformation()->GetHeader()->GetBlockId(), parent_loop_header_id);
  }

  if (blocks_in_loop != nullptr) {
    HLoopInformation* info = block->GetLoopInformation();
    const BitVector& blocks = info->GetBlocks();
    ASSERT_EQ(blocks.NumSetBits(), number_of_blocks);
    for (size_t i = 0; i < number_of_blocks; ++i) {
      ASSERT_TRUE(blocks.IsBitSet(blocks_in_loop[i]));
    }
  } else {
    ASSERT_FALSE(block->IsLoopHeader());
  }
}

TEST_F(FindLoopsTest, Loop1) {
  // Simple loop with one preheader and one back edge.
  // var a = 0;
  // while (a == a) {
  // }
  // return;
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,
    Instruction::RETURN_VOID);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header
  const int blocks2[] = {2, 3};
  TestBlock(graph, 2, true, 2, blocks2, 2);     // loop header
  TestBlock(graph, 3, false, 2);                // block in loop
  TestBlock(graph, 4, false, kInvalidBlockId);  // return block
  TestBlock(graph, 5, false, kInvalidBlockId);  // exit block
}

TEST_F(FindLoopsTest, Loop2) {
  // Make sure we support a preheader of a loop not being the first predecessor
  // in the predecessor list of the header.
  // var a = 0;
  // while (a == a) {
  // }
  // return a;
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::GOTO | 0x400,
    Instruction::IF_EQ, 4,
    Instruction::GOTO | 0xFE00,
    Instruction::GOTO | 0xFD00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // goto block
  const int blocks2[] = {2, 3};
  TestBlock(graph, 2, true, 2, blocks2, 2);     // loop header
  TestBlock(graph, 3, false, 2);                // block in loop
  TestBlock(graph, 4, false, kInvalidBlockId);  // pre header
  TestBlock(graph, 5, false, kInvalidBlockId);  // return block
  TestBlock(graph, 6, false, kInvalidBlockId);  // exit block
}

TEST_F(FindLoopsTest, Loop3) {
  // Make sure we create a preheader of a loop when a header originally has two
  // incoming blocks and one back edge.
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // goto block
  TestBlock(graph, 2, false, kInvalidBlockId);
  const int blocks2[] = {3, 4};
  TestBlock(graph, 3, true, 3, blocks2, 2);     // loop header
  TestBlock(graph, 4, false, 3);                // block in loop
  TestBlock(graph, 5, false, kInvalidBlockId);  // pre header
  TestBlock(graph, 6, false, kInvalidBlockId);  // return block
  TestBlock(graph, 7, false, kInvalidBlockId);  // exit block
  TestBlock(graph, 8, false, kInvalidBlockId);  // synthesized pre header
}

TEST_F(FindLoopsTest, Loop4) {
  // Test loop with originally two back edges.
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 6,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFC00,
    Instruction::GOTO | 0xFB00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header
  const int blocks2[] = {2, 3, 4, 5};
  TestBlock(graph, 2, true, 2, blocks2, arraysize(blocks2));  // loop header
  TestBlock(graph, 3, false, 2);                // block in loop
  TestBlock(graph, 4, false, 2);                // back edge
  TestBlock(graph, 5, false, 2);                // back edge
  TestBlock(graph, 6, false, kInvalidBlockId);  // return block
  TestBlock(graph, 7, false, kInvalidBlockId);  // exit block
}


TEST_F(FindLoopsTest, Loop5) {
  // Test loop with two exit edges.
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 6,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x0200,
    Instruction::GOTO | 0xFB00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header
  const int blocks2[] = {2, 3, 5};
  TestBlock(graph, 2, true, 2, blocks2, 3);     // loop header
  TestBlock(graph, 3, false, 2);                // block in loop
  TestBlock(graph, 4, false, kInvalidBlockId);  // loop exit
  TestBlock(graph, 5, false, 2);                // back edge
  TestBlock(graph, 6, false, kInvalidBlockId);  // return block
  TestBlock(graph, 7, false, kInvalidBlockId);  // exit block
  TestBlock(graph, 8, false, kInvalidBlockId);  // synthesized block at the loop exit
}

TEST_F(FindLoopsTest, InnerLoop) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 6,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,  // inner loop
    Instruction::GOTO | 0xFB00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header of outer loop
  const int blocks2[] = {2, 3, 4, 5, 8};
  TestBlock(graph, 2, true, 2, blocks2, 5);     // outer loop header
  const int blocks3[] = {3, 4};
  TestBlock(graph, 3, true, 3, blocks3, 2);     // inner loop header
  TestBlock(graph, 4, false, 3);                // back edge on inner loop
  TestBlock(graph, 5, false, 2);                // back edge on outer loop
  TestBlock(graph, 6, false, kInvalidBlockId);  // return block
  TestBlock(graph, 7, false, kInvalidBlockId);  // exit block
  TestBlock(graph, 8, false, 2);                // synthesized block as pre header of inner loop

  ASSERT_TRUE(graph->GetBlocks()[3]->GetLoopInformation()->IsIn(
                    *graph->GetBlocks()[2]->GetLoopInformation()));
  ASSERT_FALSE(graph->GetBlocks()[2]->GetLoopInformation()->IsIn(
                    *graph->GetBlocks()[3]->GetLoopInformation()));
}

TEST_F(FindLoopsTest, TwoLoops) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,  // first loop
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,  // second loop
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header of first loop
  const int blocks2[] = {2, 3};
  TestBlock(graph, 2, true, 2, blocks2, 2);     // first loop header
  TestBlock(graph, 3, false, 2);                // back edge of first loop
  const int blocks4[] = {4, 5};
  TestBlock(graph, 4, true, 4, blocks4, 2);     // second loop header
  TestBlock(graph, 5, false, 4);                // back edge of second loop
  TestBlock(graph, 6, false, kInvalidBlockId);  // return block
  TestBlock(graph, 7, false, kInvalidBlockId);  // exit block

  ASSERT_FALSE(graph->GetBlocks()[4]->GetLoopInformation()->IsIn(
                    *graph->GetBlocks()[2]->GetLoopInformation()));
  ASSERT_FALSE(graph->GetBlocks()[2]->GetLoopInformation()->IsIn(
                    *graph->GetBlocks()[4]->GetLoopInformation()));
}

TEST_F(FindLoopsTest, NonNaturalLoop) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x0100,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFD00,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);
  ASSERT_TRUE(graph->GetBlocks()[3]->IsLoopHeader());
  HLoopInformation* info = graph->GetBlocks()[3]->GetLoopInformation();
  ASSERT_EQ(1u, info->NumberOfBackEdges());
  ASSERT_FALSE(info->GetHeader()->Dominates(info->GetBackEdges()[0]));
}

TEST_F(FindLoopsTest, DoWhileLoop) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::GOTO | 0x0100,
    Instruction::IF_EQ, 0xFFFF,
    Instruction::RETURN | 0 << 8);

  HGraph* graph = CreateCFG(data);

  TestBlock(graph, 0, false, kInvalidBlockId);  // entry block
  TestBlock(graph, 1, false, kInvalidBlockId);  // pre header of first loop
  const int blocks2[] = {2, 3, 6};
  TestBlock(graph, 2, true, 2, blocks2, 3);     // loop header
  TestBlock(graph, 3, false, 2);                // back edge of first loop
  TestBlock(graph, 4, false, kInvalidBlockId);  // return block
  TestBlock(graph, 5, false, kInvalidBlockId);  // exit block
  TestBlock(graph, 6, false, 2);                // synthesized block to avoid a critical edge
}

}  // namespace art
