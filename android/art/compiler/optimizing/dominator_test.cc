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
#include "dex/dex_instruction.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art {

class OptimizerTest : public OptimizingUnitTest {
 protected:
  void TestCode(const std::vector<uint16_t>& data, const uint32_t* blocks, size_t blocks_length);
};

void OptimizerTest::TestCode(const std::vector<uint16_t>& data,
                             const uint32_t* blocks,
                             size_t blocks_length) {
  HGraph* graph = CreateCFG(data);
  ASSERT_EQ(graph->GetBlocks().size(), blocks_length);
  for (size_t i = 0, e = blocks_length; i < e; ++i) {
    if (blocks[i] == kInvalidBlockId) {
      if (graph->GetBlocks()[i] == nullptr) {
        // Dead block.
      } else {
        // Only the entry block has no dominator.
        ASSERT_EQ(nullptr, graph->GetBlocks()[i]->GetDominator());
        ASSERT_TRUE(graph->GetBlocks()[i]->IsEntryBlock());
      }
    } else {
      ASSERT_NE(nullptr, graph->GetBlocks()[i]->GetDominator());
      ASSERT_EQ(blocks[i], graph->GetBlocks()[i]->GetDominator()->GetBlockId());
    }
  }
}

TEST_F(OptimizerTest, ReturnVoid) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
      Instruction::RETURN_VOID);  // Block number 1

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG1) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x100,  // Block number 1
    Instruction::RETURN_VOID);  // Block number 2

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      2
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG2) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x100,  // Block number 1
    Instruction::GOTO | 0x100,  // Block number 2
    Instruction::RETURN_VOID);  // Block number 3

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      2,
      3
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG3) {
  const std::vector<uint16_t> data1 = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x200,    // Block number 1
    Instruction::RETURN_VOID,     // Block number 2
    Instruction::GOTO | 0xFF00);  // Block number 3

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      3,
      1,
      2
  };

  TestCode(data1, dominators, sizeof(dominators) / sizeof(int));

  const std::vector<uint16_t> data2 = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_16, 3,
    Instruction::RETURN_VOID,
    Instruction::GOTO_16, 0xFFFF);

  TestCode(data2, dominators, sizeof(dominators) / sizeof(int));

  const std::vector<uint16_t> data3 = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 4, 0,
    Instruction::RETURN_VOID,
    Instruction::GOTO_32, 0xFFFF, 0xFFFF);

  TestCode(data3, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG4) {
  const std::vector<uint16_t> data1 = ZERO_REGISTER_CODE_ITEM(
    Instruction::NOP,
    Instruction::GOTO | 0xFF00);

  const uint32_t dominators[] = {
      kInvalidBlockId,
      3,
      kInvalidBlockId,
      0
  };

  TestCode(data1, dominators, sizeof(dominators) / sizeof(int));

  const std::vector<uint16_t> data2 = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 0, 0);

  TestCode(data2, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG5) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
    Instruction::RETURN_VOID,     // Block number 1
    Instruction::GOTO | 0x100,    // Dead block
    Instruction::GOTO | 0xFE00);  // Block number 2


  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      kInvalidBlockId,
      1
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG6) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      1,
      3,
      1,  // Synthesized block to avoid critical edge.
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG7) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x100,    // Block number 2
    Instruction::GOTO | 0xFF00);  // Block number 3

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      1,
      kInvalidBlockId,  // exit block is not dominated by any block due to the spin loop.
      1,   // block to avoid critical edge.
      1    // block to avoid critical edge.
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG8) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x200,    // Block number 2
    Instruction::GOTO | 0x100,    // Block number 3
    Instruction::GOTO | 0xFF00);  // Block number 4

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      1,
      1,
      kInvalidBlockId,  // exit block is not dominated by any block due to the spin loop.
      1    // block to avoid critical edge.
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG9) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,        // Block number 1
    Instruction::GOTO | 0x200,    // Block number 2
    Instruction::GOTO | 0x100,    // Block number 3
    Instruction::GOTO | 0xFE00);  // Block number 4

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      1,
      1,
      kInvalidBlockId,  // exit block is not dominated by any block due to the spin loop.
      1    // block to avoid critical edge.
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

TEST_F(OptimizerTest, CFG10) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 6,  // Block number 1
    Instruction::IF_EQ, 3,  // Block number 2
    Instruction::GOTO | 0x100,  // Block number 3
    Instruction::GOTO | 0x100,  // Block number 4
    Instruction::RETURN_VOID);  // Block number 5

  const uint32_t dominators[] = {
      kInvalidBlockId,
      0,
      1,
      2,
      2,
      1,
      5,    // Block number 5 dominates exit block
      1,    // block to avoid critical edge.
      2     // block to avoid critical edge.
  };

  TestCode(data, dominators, sizeof(dominators) / sizeof(int));
}

}  // namespace art
