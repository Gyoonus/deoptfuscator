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

#include "android-base/stringprintf.h"

#include "base/arena_allocator.h"
#include "builder.h"
#include "dex/dex_file.h"
#include "dex/dex_instruction.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"
#include "ssa_builder.h"

#include "gtest/gtest.h"

namespace art {

class SsaTest : public OptimizingUnitTest {
 protected:
  void TestCode(const std::vector<uint16_t>& data, const char* expected);
};

class SsaPrettyPrinter : public HPrettyPrinter {
 public:
  explicit SsaPrettyPrinter(HGraph* graph) : HPrettyPrinter(graph), str_("") {}

  void PrintInt(int value) OVERRIDE {
    str_ += android::base::StringPrintf("%d", value);
  }

  void PrintString(const char* value) OVERRIDE {
    str_ += value;
  }

  void PrintNewLine() OVERRIDE {
    str_ += '\n';
  }

  void Clear() { str_.clear(); }

  std::string str() const { return str_; }

  void VisitIntConstant(HIntConstant* constant) OVERRIDE {
    PrintPreInstruction(constant);
    str_ += constant->DebugName();
    str_ += " ";
    PrintInt(constant->GetValue());
    PrintPostInstruction(constant);
  }

 private:
  std::string str_;

  DISALLOW_COPY_AND_ASSIGN(SsaPrettyPrinter);
};

static void ReNumberInstructions(HGraph* graph) {
  int id = 0;
  for (HBasicBlock* block : graph->GetBlocks()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      it.Current()->SetId(id++);
    }
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      it.Current()->SetId(id++);
    }
  }
}

void SsaTest::TestCode(const std::vector<uint16_t>& data, const char* expected) {
  HGraph* graph = CreateCFG(data);
  // Suspend checks implementation may change in the future, and this test relies
  // on how instructions are ordered.
  RemoveSuspendChecks(graph);
  ReNumberInstructions(graph);

  // Test that phis had their type set.
  for (HBasicBlock* block : graph->GetBlocks()) {
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      ASSERT_NE(it.Current()->GetType(), DataType::Type::kVoid);
    }
  }

  SsaPrettyPrinter printer(graph);
  printer.VisitInsertionOrder();

  ASSERT_STREQ(expected, printer.str().c_str());
}

TEST_F(SsaTest, CFG1) {
  // Test that we get rid of loads and stores.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [2, 2]\n"
    "  1: Goto\n"
    "BasicBlock 1, pred: 0, succ: 5, 2\n"
    "  2: Equal(0, 0) [3]\n"
    "  3: If(2)\n"
    "BasicBlock 2, pred: 1, succ: 3\n"
    "  4: Goto\n"
    "BasicBlock 3, pred: 5, 2, succ: 4\n"
    "  5: ReturnVoid\n"
    "BasicBlock 4, pred: 3\n"
    "  6: Exit\n"
    // Synthesized block to avoid critical edge.
    "BasicBlock 5, pred: 1, succ: 3\n"
    "  7: Goto\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(SsaTest, CFG2) {
  // Test that we create a phi for the join block of an if control flow instruction
  // when there is only code in the else branch.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [6, 3, 3]\n"
    "  1: IntConstant 4 [6]\n"
    "  2: Goto\n"
    "BasicBlock 1, pred: 0, succ: 5, 2\n"
    "  3: Equal(0, 0) [4]\n"
    "  4: If(3)\n"
    "BasicBlock 2, pred: 1, succ: 3\n"
    "  5: Goto\n"
    "BasicBlock 3, pred: 5, 2, succ: 4\n"
    "  6: Phi(0, 1) [7]\n"
    "  7: Return(6)\n"
    "BasicBlock 4, pred: 3\n"
    "  8: Exit\n"
    // Synthesized block to avoid critical edge.
    "BasicBlock 5, pred: 1, succ: 3\n"
    "  9: Goto\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, CFG3) {
  // Test that we create a phi for the join block of an if control flow instruction
  // when both branches update a local.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [4, 4]\n"
    "  1: IntConstant 5 [8]\n"
    "  2: IntConstant 4 [8]\n"
    "  3: Goto\n"
    "BasicBlock 1, pred: 0, succ: 3, 2\n"
    "  4: Equal(0, 0) [5]\n"
    "  5: If(4)\n"
    "BasicBlock 2, pred: 1, succ: 4\n"
    "  6: Goto\n"
    "BasicBlock 3, pred: 1, succ: 4\n"
    "  7: Goto\n"
    "BasicBlock 4, pred: 2, 3, succ: 5\n"
    "  8: Phi(2, 1) [9]\n"
    "  9: Return(8)\n"
    "BasicBlock 5, pred: 4\n"
    "  10: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0x200,
    Instruction::CONST_4 | 5 << 12 | 0,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop1) {
  // Test that we create a phi for an initialized local at entry of a loop.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [6, 3, 3]\n"
    "  1: IntConstant 4 [6]\n"
    "  2: Goto\n"
    "BasicBlock 1, pred: 0, succ: 4, 2\n"
    "  3: Equal(0, 0) [4]\n"
    "  4: If(3)\n"
    "BasicBlock 2, pred: 1, succ: 3\n"
    "  5: Goto\n"
    "BasicBlock 3, pred: 2, 4, succ: 5\n"
    "  6: Phi(1, 0) [9]\n"
    "  7: Goto\n"
    "BasicBlock 4, pred: 1, succ: 3\n"
    "  8: Goto\n"
    "BasicBlock 5, pred: 3, succ: 6\n"
    "  9: Return(6)\n"
    "BasicBlock 6, pred: 5\n"
    "  10: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0x200,
    Instruction::GOTO | 0xFF00,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop2) {
  // Simple loop with one preheader and one back edge.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [4]\n"
    "  1: IntConstant 4 [4]\n"
    "  2: Goto\n"
    "BasicBlock 1, pred: 0, succ: 2\n"
    "  3: Goto\n"
    "BasicBlock 2, pred: 1, 3, succ: 4, 3\n"
    "  4: Phi(0, 1) [5, 5]\n"
    "  5: Equal(4, 4) [6]\n"
    "  6: If(5)\n"
    "BasicBlock 3, pred: 2, succ: 2\n"
    "  7: Goto\n"
    "BasicBlock 4, pred: 2, succ: 5\n"
    "  8: ReturnVoid\n"
    "BasicBlock 5, pred: 4\n"
    "  9: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0xFD00,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop3) {
  // Test that a local not yet defined at the entry of a loop is handled properly.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [5]\n"
    "  1: IntConstant 5 [9]\n"
    "  2: IntConstant 4 [5]\n"
    "  3: Goto\n"
    "BasicBlock 1, pred: 0, succ: 2\n"
    "  4: Goto\n"
    "BasicBlock 2, pred: 1, 3, succ: 4, 3\n"
    "  5: Phi(0, 2) [6, 6]\n"
    "  6: Equal(5, 5) [7]\n"
    "  7: If(6)\n"
    "BasicBlock 3, pred: 2, succ: 2\n"
    "  8: Goto\n"
    "BasicBlock 4, pred: 2, succ: 5\n"
    "  9: Return(1)\n"
    "BasicBlock 5, pred: 4\n"
    "  10: Exit\n";

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0xFD00,
    Instruction::CONST_4 | 5 << 12 | 1 << 8,
    Instruction::RETURN | 1 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop4) {
  // Make sure we support a preheader of a loop not being the first predecessor
  // in the predecessor list of the header.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [4]\n"
    "  1: IntConstant 4 [4]\n"
    "  2: Goto\n"
    "BasicBlock 1, pred: 0, succ: 4\n"
    "  3: Goto\n"
    "BasicBlock 2, pred: 4, 3, succ: 5, 3\n"
    "  4: Phi(0, 1) [9, 5, 5]\n"
    "  5: Equal(4, 4) [6]\n"
    "  6: If(5)\n"
    "BasicBlock 3, pred: 2, succ: 2\n"
    "  7: Goto\n"
    "BasicBlock 4, pred: 1, succ: 2\n"
    "  8: Goto\n"
    "BasicBlock 5, pred: 2, succ: 6\n"
    "  9: Return(4)\n"
    "BasicBlock 6, pred: 5\n"
    "  10: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::GOTO | 0x500,
    Instruction::IF_EQ, 5,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0xFD00,
    Instruction::GOTO | 0xFC00,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop5) {
  // Make sure we create a preheader of a loop when a header originally has two
  // incoming blocks and one back edge.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [4, 4]\n"
    "  1: IntConstant 5 [13]\n"
    "  2: IntConstant 4 [13]\n"
    "  3: Goto\n"
    "BasicBlock 1, pred: 0, succ: 3, 2\n"
    "  4: Equal(0, 0) [5]\n"
    "  5: If(4)\n"
    "BasicBlock 2, pred: 1, succ: 8\n"
    "  6: Goto\n"
    "BasicBlock 3, pred: 1, succ: 8\n"
    "  7: Goto\n"
    "BasicBlock 4, pred: 8, 5, succ: 6, 5\n"
    "  8: Equal(13, 13) [9]\n"
    "  9: If(8)\n"
    "BasicBlock 5, pred: 4, succ: 4\n"
    "  10: Goto\n"
    "BasicBlock 6, pred: 4, succ: 7\n"
    "  11: Return(13)\n"
    "BasicBlock 7, pred: 6\n"
    "  12: Exit\n"
    "BasicBlock 8, pred: 2, 3, succ: 4\n"
    "  13: Phi(2, 1) [11, 8, 8]\n"
    "  14: Goto\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0x200,
    Instruction::CONST_4 | 5 << 12 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0xFE00,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop6) {
  // Test a loop with one preheader and two back edges (e.g. continue).
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [5]\n"
    "  1: IntConstant 4 [5, 8, 8]\n"
    "  2: IntConstant 5 [5]\n"
    "  3: Goto\n"
    "BasicBlock 1, pred: 0, succ: 2\n"
    "  4: Goto\n"
    "BasicBlock 2, pred: 1, 4, 5, succ: 6, 3\n"
    "  5: Phi(0, 2, 1) [12, 6, 6]\n"
    "  6: Equal(5, 5) [7]\n"
    "  7: If(6)\n"
    "BasicBlock 3, pred: 2, succ: 5, 4\n"
    "  8: Equal(1, 1) [9]\n"
    "  9: If(8)\n"
    "BasicBlock 4, pred: 3, succ: 2\n"
    "  10: Goto\n"
    "BasicBlock 5, pred: 3, succ: 2\n"
    "  11: Goto\n"
    "BasicBlock 6, pred: 2, succ: 7\n"
    "  12: Return(5)\n"
    "BasicBlock 7, pred: 6\n"
    "  13: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 8,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 5 << 12 | 0,
    Instruction::GOTO | 0xFA00,
    Instruction::GOTO | 0xF900,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, Loop7) {
  // Test a loop with one preheader, one back edge, and two exit edges (e.g. break).
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [5]\n"
    "  1: IntConstant 4 [5, 8, 8]\n"
    "  2: IntConstant 5 [12]\n"
    "  3: Goto\n"
    "BasicBlock 1, pred: 0, succ: 2\n"
    "  4: Goto\n"
    "BasicBlock 2, pred: 1, 5, succ: 8, 3\n"
    "  5: Phi(0, 1) [12, 6, 6]\n"
    "  6: Equal(5, 5) [7]\n"
    "  7: If(6)\n"
    "BasicBlock 3, pred: 2, succ: 5, 4\n"
    "  8: Equal(1, 1) [9]\n"
    "  9: If(8)\n"
    "BasicBlock 4, pred: 3, succ: 6\n"
    "  10: Goto\n"
    "BasicBlock 5, pred: 3, succ: 2\n"
    "  11: Goto\n"
    "BasicBlock 6, pred: 8, 4, succ: 7\n"
    "  12: Phi(5, 2) [13]\n"
    "  13: Return(12)\n"
    "BasicBlock 7, pred: 6\n"
    "  14: Exit\n"
    "BasicBlock 8, pred: 2, succ: 6\n"
    "  15: Goto\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 8,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 5 << 12 | 0,
    Instruction::GOTO | 0x0200,
    Instruction::GOTO | 0xF900,
    Instruction::RETURN | 0 << 8);

  TestCode(data, expected);
}

TEST_F(SsaTest, DeadLocal) {
  // Test that we correctly handle a local not being used.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0\n"
    "  1: Goto\n"
    "BasicBlock 1, pred: 0, succ: 2\n"
    "  2: ReturnVoid\n"
    "BasicBlock 2, pred: 1\n"
    "  3: Exit\n";

  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(SsaTest, LocalInIf) {
  // Test that we do not create a phi in the join block when one predecessor
  // does not update the local.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [3, 3]\n"
    "  1: IntConstant 4\n"
    "  2: Goto\n"
    "BasicBlock 1, pred: 0, succ: 5, 2\n"
    "  3: Equal(0, 0) [4]\n"
    "  4: If(3)\n"
    "BasicBlock 2, pred: 1, succ: 3\n"
    "  5: Goto\n"
    "BasicBlock 3, pred: 5, 2, succ: 4\n"
    "  6: ReturnVoid\n"
    "BasicBlock 4, pred: 3\n"
    "  7: Exit\n"
    // Synthesized block to avoid critical edge.
    "BasicBlock 5, pred: 1, succ: 3\n"
    "  8: Goto\n";

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::CONST_4 | 4 << 12 | 1 << 8,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

TEST_F(SsaTest, MultiplePredecessors) {
  // Test that we do not create a phi when one predecessor
  // does not update the local.
  const char* expected =
    "BasicBlock 0, succ: 1\n"
    "  0: IntConstant 0 [4, 4, 8, 8, 6, 6, 2, 2]\n"
    "  1: Goto\n"
    "BasicBlock 1, pred: 0, succ: 3, 2\n"
    "  2: Equal(0, 0) [3]\n"
    "  3: If(2)\n"
    "BasicBlock 2, pred: 1, succ: 5\n"
    "  4: Add(0, 0)\n"
    "  5: Goto\n"
    "BasicBlock 3, pred: 1, succ: 7, 4\n"
    "  6: Equal(0, 0) [7]\n"
    "  7: If(6)\n"
    "BasicBlock 4, pred: 3, succ: 5\n"
    "  8: Add(0, 0)\n"
    "  9: Goto\n"
    // This block should not get a phi for local 1.
    "BasicBlock 5, pred: 2, 7, 4, succ: 6\n"
    "  10: ReturnVoid\n"
    "BasicBlock 6, pred: 5\n"
    "  11: Exit\n"
    "BasicBlock 7, pred: 3, succ: 5\n"
    "  12: Goto\n";

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 5,
    Instruction::ADD_INT_LIT8 | 1 << 8, 0 << 8,
    Instruction::GOTO | 0x0500,
    Instruction::IF_EQ, 4,
    Instruction::ADD_INT_LIT8 | 1 << 8, 0 << 8,
    Instruction::RETURN_VOID);

  TestCode(data, expected);
}

}  // namespace art
