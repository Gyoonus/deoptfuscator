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

#include "builder.h"
#include "dex/dex_instruction.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"

#include "gtest/gtest.h"

namespace art {

/**
 * Check that the HGraphBuilder adds suspend checks to backward branches.
 */

class SuspendCheckTest : public OptimizingUnitTest {
 protected:
  void TestCode(const std::vector<uint16_t>& data);
};

void SuspendCheckTest::TestCode(const std::vector<uint16_t>& data) {
  HGraph* graph = CreateCFG(data);
  HBasicBlock* first_block = graph->GetEntryBlock()->GetSingleSuccessor();
  HBasicBlock* loop_header = first_block->GetSingleSuccessor();
  ASSERT_TRUE(loop_header->IsLoopHeader());
  ASSERT_EQ(loop_header->GetLoopInformation()->GetPreHeader(), first_block);
  ASSERT_TRUE(loop_header->GetFirstInstruction()->IsSuspendCheck());
}

TEST_F(SuspendCheckTest, CFG1) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
    Instruction::NOP,
    Instruction::GOTO | 0xFF00);

  TestCode(data);
}

TEST_F(SuspendCheckTest, CFG2) {
  const std::vector<uint16_t> data = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 0, 0);

  TestCode(data);
}

TEST_F(SuspendCheckTest, CFG3) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 0xFFFF,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(SuspendCheckTest, CFG4) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_NE, 0xFFFF,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(SuspendCheckTest, CFG5) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQZ, 0xFFFF,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST_F(SuspendCheckTest, CFG6) {
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_NEZ, 0xFFFF,
    Instruction::RETURN_VOID);

  TestCode(data);
}
}  // namespace art
