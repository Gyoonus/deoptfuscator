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

#include <functional>
#include <stdio.h>

#include "arch/x86/instruction_set_features_x86.h"
#include "code_generator_x86.h"
#include "constant_folding.h"
#include "dead_code_elimination.h"
#include "driver/compiler_options.h"
#include "graph_checker.h"
#include "optimizing_unit_test.h"
#include "pretty_printer.h"

#include "gtest/gtest.h"

namespace art {

/**
 * Fixture class for the constant folding and dce tests.
 */
class ConstantFoldingTest : public OptimizingUnitTest {
 public:
  ConstantFoldingTest() : graph_(nullptr) { }

  void TestCode(const std::vector<uint16_t>& data,
                const std::string& expected_before,
                const std::string& expected_after_cf,
                const std::string& expected_after_dce,
                const std::function<void(HGraph*)>& check_after_cf,
                DataType::Type return_type = DataType::Type::kInt32) {
    graph_ = CreateCFG(data, return_type);
    TestCodeOnReadyGraph(expected_before,
                         expected_after_cf,
                         expected_after_dce,
                         check_after_cf);
  }

  void TestCodeOnReadyGraph(const std::string& expected_before,
                            const std::string& expected_after_cf,
                            const std::string& expected_after_dce,
                            const std::function<void(HGraph*)>& check_after_cf) {
    ASSERT_NE(graph_, nullptr);
    
    StringPrettyPrinter printer_before(graph_);
    printer_before.VisitInsertionOrder();
    std::string actual_before = printer_before.str();
    EXPECT_EQ(expected_before, actual_before);
    std::cout << "before==\n" << actual_before << std::endl;

    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegenX86(graph_, *features_x86.get(), CompilerOptions());
    HConstantFolding(graph_, "constant_folding").Run();
    GraphChecker graph_checker_cf(graph_);
    graph_checker_cf.Run();
    ASSERT_TRUE(graph_checker_cf.IsValid());

    StringPrettyPrinter printer_after_cf(graph_);
    printer_after_cf.VisitInsertionOrder();
    std::string actual_after_cf = printer_after_cf.str();
    EXPECT_EQ(expected_after_cf, actual_after_cf);

    check_after_cf(graph_);

    HDeadCodeElimination(graph_, nullptr /* stats */, "dead_code_elimination").Run();
    GraphChecker graph_checker_dce(graph_);
    graph_checker_dce.Run();
    ASSERT_TRUE(graph_checker_dce.IsValid());

    StringPrettyPrinter printer_after_dce(graph_);
    printer_after_dce.VisitInsertionOrder();
    std::string actual_after_dce = printer_after_dce.str();
    EXPECT_EQ(expected_after_dce, actual_after_dce);
    
    std::cout << "after==\n" << actual_after_dce << std::endl;
  }

  HGraph* graph_;
};

/**
 * Tiny three-register program exercising int constant folding on negation.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v0 <- 1                  0.      const/4 v0, #+1
 *     v1 <- -v0                1.      neg-int v1, v0
 *     return v1                2.      return v1
 */
TEST_F(ConstantFoldingTest, IntConstantFoldingNegation) {
  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 8 | 1 << 12,
    Instruction::NEG_INT | 1 << 8 | 0 << 12,
    Instruction::RETURN | 1 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: IntConstant [3]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  3: Neg(2) [4]\n"
      "  4: Return(3)\n"
      "BasicBlock 2, pred: 1\n"
      "  5: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: IntConstant [3]\n", "  2: IntConstant\n"
                                "  6: IntConstant [4]\n" },
    { "  3: Neg(2) [4]\n",      removed },
    { "  4: Return(3)\n",       "  4: Return(6)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsIntConstant());
    ASSERT_EQ(inst->AsIntConstant()->GetValue(), -1);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: IntConstant\n", removed },
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Tiny three-register program exercising long constant folding on negation.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     (v0, v1) <- 4294967296   0.      const-wide v0 #+4294967296
 *     (v2, v3) <- -(v0, v1)    1.      neg-long v2, v0
 *     return (v2, v3)          2.      return-wide v2
 */
TEST_F(ConstantFoldingTest, LongConstantFoldingNegation) {
  const int64_t input = INT64_C(4294967296);             // 2^32
  const uint16_t word0 = Low16Bits(Low32Bits(input));    // LSW.
  const uint16_t word1 = High16Bits(Low32Bits(input));
  const uint16_t word2 = Low16Bits(High32Bits(input));
  const uint16_t word3 = High16Bits(High32Bits(input));  // MSW.
  const std::vector<uint16_t> data = FOUR_REGISTERS_CODE_ITEM(
    Instruction::CONST_WIDE | 0 << 8, word0, word1, word2, word3,
    Instruction::NEG_LONG | 2 << 8 | 0 << 12,
    Instruction::RETURN_WIDE | 2 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: LongConstant [3]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  3: Neg(2) [4]\n"
      "  4: Return(3)\n"
      "BasicBlock 2, pred: 1\n"
      "  5: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: LongConstant [3]\n", "  2: LongConstant\n"
                                 "  6: LongConstant [4]\n" },
    { "  3: Neg(2) [4]\n",       removed },
    { "  4: Return(3)\n",        "  4: Return(6)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsLongConstant());
    ASSERT_EQ(inst->AsLongConstant()->GetValue(), INT64_C(-4294967296));
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: LongConstant\n", removed },
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf,
           DataType::Type::kInt64);
}

/**
 * Tiny three-register program exercising int constant folding on addition.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v0 <- 1                  0.      const/4 v0, #+1
 *     v1 <- 2                  1.      const/4 v1, #+2
 *     v2 <- v0 + v1            2.      add-int v2, v0, v1
 *     return v2                4.      return v2
 */
TEST_F(ConstantFoldingTest, IntConstantFoldingOnAddition1) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 8 | 1 << 12,
    Instruction::CONST_4 | 1 << 8 | 2 << 12,
    Instruction::ADD_INT | 2 << 8, 0 | 1 << 8,
    Instruction::RETURN | 2 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: IntConstant [4]\n"
      "  3: IntConstant [4]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Add(2, 3) [5]\n"
      "  5: Return(4)\n"
      "BasicBlock 2, pred: 1\n"
      "  6: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: IntConstant [4]\n", "  2: IntConstant\n" },
    { "  3: IntConstant [4]\n", "  3: IntConstant\n"
                                "  7: IntConstant [5]\n" },
    { "  4: Add(2, 3) [5]\n",   removed },
    { "  5: Return(4)\n",       "  5: Return(7)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsIntConstant());
    ASSERT_EQ(inst->AsIntConstant()->GetValue(), 3);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: IntConstant\n", removed },
    { "  3: IntConstant\n", removed }
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Small three-register program exercising int constant folding on addition.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v0 <- 1                  0.      const/4 v0, #+1
 *     v1 <- 2                  1.      const/4 v1, #+2
 *     v0 <- v0 + v1            2.      add-int/2addr v0, v1
 *     v1 <- 4                  3.      const/4 v1, #+4
 *     v2 <- 5                  4.      const/4 v2, #+5
 *     v1 <- v1 + v2            5.      add-int/2addr v1, v2
 *     v2 <- v0 + v1            6.      add-int v2, v0, v1
 *     return v2                8.      return v2
 */
TEST_F(ConstantFoldingTest, IntConstantFoldingOnAddition2) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 8 | 1 << 12,
    Instruction::CONST_4 | 1 << 8 | 2 << 12,
    Instruction::ADD_INT_2ADDR | 0 << 8 | 1 << 12,
    Instruction::CONST_4 | 1 << 8 | 4 << 12,
    Instruction::CONST_4 | 2 << 8 | 5 << 12,
    Instruction::ADD_INT_2ADDR | 1 << 8 | 2 << 12,
    Instruction::ADD_INT | 2 << 8, 0 | 1 << 8,
    Instruction::RETURN | 2 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: IntConstant [4]\n"
      "  3: IntConstant [4]\n"
      "  5: IntConstant [7]\n"
      "  6: IntConstant [7]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Add(2, 3) [8]\n"
      "  7: Add(5, 6) [8]\n"
      "  8: Add(4, 7) [9]\n"
      "  9: Return(8)\n"
      "BasicBlock 2, pred: 1\n"
      "  10: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: IntConstant [4]\n",  "  2: IntConstant\n" },
    { "  3: IntConstant [4]\n",  "  3: IntConstant\n" },
    { "  5: IntConstant [7]\n",  "  5: IntConstant\n" },
    { "  6: IntConstant [7]\n",  "  6: IntConstant\n"
                                 "  11: IntConstant\n"
                                 "  12: IntConstant\n"
                                 "  13: IntConstant [9]\n" },
    { "  4: Add(2, 3) [8]\n",    removed },
    { "  7: Add(5, 6) [8]\n",    removed },
    { "  8: Add(4, 7) [9]\n",    removed  },
    { "  9: Return(8)\n",        "  9: Return(13)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the values of the computed constants.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst1 = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst1->IsIntConstant());
    ASSERT_EQ(inst1->AsIntConstant()->GetValue(), 12);
    HInstruction* inst2 = inst1->GetPrevious();
    ASSERT_TRUE(inst2->IsIntConstant());
    ASSERT_EQ(inst2->AsIntConstant()->GetValue(), 9);
    HInstruction* inst3 = inst2->GetPrevious();
    ASSERT_TRUE(inst3->IsIntConstant());
    ASSERT_EQ(inst3->AsIntConstant()->GetValue(), 3);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: IntConstant\n",  removed },
    { "  3: IntConstant\n",  removed },
    { "  5: IntConstant\n",  removed },
    { "  6: IntConstant\n",  removed },
    { "  11: IntConstant\n", removed },
    { "  12: IntConstant\n", removed }
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Tiny three-register program exercising int constant folding on subtraction.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v0 <- 3                  0.      const/4 v0, #+3
 *     v1 <- 2                  1.      const/4 v1, #+2
 *     v2 <- v0 - v1            2.      sub-int v2, v0, v1
 *     return v2                4.      return v2
 */
TEST_F(ConstantFoldingTest, IntConstantFoldingOnSubtraction) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 8 | 3 << 12,
    Instruction::CONST_4 | 1 << 8 | 2 << 12,
    Instruction::SUB_INT | 2 << 8, 0 | 1 << 8,
    Instruction::RETURN | 2 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: IntConstant [4]\n"
      "  3: IntConstant [4]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Sub(2, 3) [5]\n"
      "  5: Return(4)\n"
      "BasicBlock 2, pred: 1\n"
      "  6: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: IntConstant [4]\n",  "  2: IntConstant\n" },
    { "  3: IntConstant [4]\n",  "  3: IntConstant\n"
                                 "  7: IntConstant [5]\n" },
    { "  4: Sub(2, 3) [5]\n",    removed },
    { "  5: Return(4)\n",        "  5: Return(7)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsIntConstant());
    ASSERT_EQ(inst->AsIntConstant()->GetValue(), 1);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: IntConstant\n", removed },
    { "  3: IntConstant\n", removed }
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Tiny three-register-pair program exercising long constant folding
 * on addition.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     (v0, v1) <- 1            0.      const-wide/16 v0, #+1
 *     (v2, v3) <- 2            2.      const-wide/16 v2, #+2
 *     (v4, v5) <-
 *       (v0, v1) + (v1, v2)    4.      add-long v4, v0, v2
 *     return (v4, v5)          6.      return-wide v4
 */
TEST_F(ConstantFoldingTest, LongConstantFoldingOnAddition) {
  const std::vector<uint16_t> data = SIX_REGISTERS_CODE_ITEM(
    Instruction::CONST_WIDE_16 | 0 << 8, 1,
    Instruction::CONST_WIDE_16 | 2 << 8, 2,
    Instruction::ADD_LONG | 4 << 8, 0 | 2 << 8,
    Instruction::RETURN_WIDE | 4 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: LongConstant [4]\n"
      "  3: LongConstant [4]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Add(2, 3) [5]\n"
      "  5: Return(4)\n"
      "BasicBlock 2, pred: 1\n"
      "  6: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: LongConstant [4]\n",  "  2: LongConstant\n" },
    { "  3: LongConstant [4]\n",  "  3: LongConstant\n"
                                  "  7: LongConstant [5]\n" },
    { "  4: Add(2, 3) [5]\n",     removed },
    { "  5: Return(4)\n",         "  5: Return(7)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsLongConstant());
    ASSERT_EQ(inst->AsLongConstant()->GetValue(), 3);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: LongConstant\n", removed },
    { "  3: LongConstant\n", removed }
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf,
           DataType::Type::kInt64);
}

/**
 * Tiny three-register-pair program exercising long constant folding
 * on subtraction.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     (v0, v1) <- 3            0.      const-wide/16 v0, #+3
 *     (v2, v3) <- 2            2.      const-wide/16 v2, #+2
 *     (v4, v5) <-
 *       (v0, v1) - (v1, v2)    4.      sub-long v4, v0, v2
 *     return (v4, v5)          6.      return-wide v4
 */
TEST_F(ConstantFoldingTest, LongConstantFoldingOnSubtraction) {
  const std::vector<uint16_t> data = SIX_REGISTERS_CODE_ITEM(
    Instruction::CONST_WIDE_16 | 0 << 8, 3,
    Instruction::CONST_WIDE_16 | 2 << 8, 2,
    Instruction::SUB_LONG | 4 << 8, 0 | 2 << 8,
    Instruction::RETURN_WIDE | 4 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: LongConstant [4]\n"
      "  3: LongConstant [4]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Sub(2, 3) [5]\n"
      "  5: Return(4)\n"
      "BasicBlock 2, pred: 1\n"
      "  6: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: LongConstant [4]\n",  "  2: LongConstant\n" },
    { "  3: LongConstant [4]\n",  "  3: LongConstant\n"
                                  "  7: LongConstant [5]\n" },
    { "  4: Sub(2, 3) [5]\n",     removed },
    { "  5: Return(4)\n",         "  5: Return(7)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the value of the computed constant.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsLongConstant());
    ASSERT_EQ(inst->AsLongConstant()->GetValue(), 1);
  };

  // Expected difference after dead code elimination.
  diff_t expected_dce_diff = {
    { "  2: LongConstant\n", removed },
    { "  3: LongConstant\n", removed }
  };
  std::string expected_after_dce = Patch(expected_after_cf, expected_dce_diff);

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf,
           DataType::Type::kInt64);
}

/**
 * Three-register program with jumps leading to the creation of many
 * blocks.
 *
 * The intent of this test is to ensure that all constant expressions
 * are actually evaluated at compile-time, thanks to the reverse
 * (forward) post-order traversal of the the dominator tree.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v0 <- 1                   0.     const/4 v0, #+1
 *     v1 <- 2                   1.     const/4 v1, #+2
 *     v2 <- v0 + v1             2.     add-int v2, v0, v1
 *     goto L2                   4.     goto +4
 * L1: v1 <- v0 + 5              5.     add-int/lit16 v1, v0, #+5
 *     goto L3                   7.     goto +4
 * L2: v0 <- v2 + 4              8.     add-int/lit16 v0, v2, #+4
 *     goto L1                  10.     goto +(-5)
 * L3: v2 <- v1 + 8             11.     add-int/lit16 v2, v1, #+8
 *     return v2                13.     return v2
 */
TEST_F(ConstantFoldingTest, IntConstantFoldingAndJumps) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 << 8 | 1 << 12,
    Instruction::CONST_4 | 1 << 8 | 2 << 12,
    Instruction::ADD_INT | 2 << 8, 0 | 1 << 8,
    Instruction::GOTO | 4 << 8,
    Instruction::ADD_INT_LIT16 | 1 << 8 | 0 << 12, 5,
    Instruction::GOTO | 4 << 8,
    Instruction::ADD_INT_LIT16 | 0 << 8 | 2 << 12, 4,
    static_cast<uint16_t>(Instruction::GOTO | 0xFFFFFFFB << 8),
    Instruction::ADD_INT_LIT16 | 2 << 8 | 1 << 12, 8,
    Instruction::RETURN | 2 << 8);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  2: IntConstant [4]\n"             // v0 <- 1
      "  3: IntConstant [4]\n"             // v1 <- 2
      "  6: IntConstant [7]\n"             // const 5
      "  9: IntConstant [10]\n"            // const 4
      "  12: IntConstant [13]\n"           // const 8
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 3\n"
      "  4: Add(2, 3) [7]\n"               // v2 <- v0 + v1 = 1 + 2 = 3
      "  5: Goto 3\n"                      // goto L2
      "BasicBlock 2, pred: 3, succ: 4\n"   // L1:
      "  10: Add(7, 9) [13]\n"             // v1 <- v0 + 3 = 7 + 5 = 12
      "  11: Goto 4\n"                     // goto L3
      "BasicBlock 3, pred: 1, succ: 2\n"   // L2:
      "  7: Add(4, 6) [10]\n"              // v0 <- v2 + 2 = 3 + 4 = 7
      "  8: Goto 2\n"                      // goto L1
      "BasicBlock 4, pred: 2, succ: 5\n"   // L3:
      "  13: Add(10, 12) [14]\n"           // v2 <- v1 + 4 = 12 + 8 = 20
      "  14: Return(13)\n"                 // return v2
      "BasicBlock 5, pred: 4\n"
      "  15: Exit\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  2: IntConstant [4]\n",   "  2: IntConstant\n" },
    { "  3: IntConstant [4]\n",   "  3: IntConstant\n" },
    { "  6: IntConstant [7]\n",   "  6: IntConstant\n" },
    { "  9: IntConstant [10]\n",  "  9: IntConstant\n" },
    { "  12: IntConstant [13]\n", "  12: IntConstant\n"
                                  "  16: IntConstant\n"
                                  "  17: IntConstant\n"
                                  "  18: IntConstant\n"
                                  "  19: IntConstant [14]\n" },
    { "  4: Add(2, 3) [7]\n",     removed },
    { "  10: Add(7, 9) [13]\n",   removed },
    { "  7: Add(4, 6) [10]\n",    removed },
    { "  13: Add(10, 12) [14]\n", removed },
    { "  14: Return(13)\n",       "  14: Return(19)\n"}
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the values of the computed constants.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst1 = graph->GetBlocks()[4]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst1->IsIntConstant());
    ASSERT_EQ(inst1->AsIntConstant()->GetValue(), 20);
    HInstruction* inst2 = inst1->GetPrevious();
    ASSERT_TRUE(inst2->IsIntConstant());
    ASSERT_EQ(inst2->AsIntConstant()->GetValue(), 12);
    HInstruction* inst3 = inst2->GetPrevious();
    ASSERT_TRUE(inst3->IsIntConstant());
    ASSERT_EQ(inst3->AsIntConstant()->GetValue(), 7);
    HInstruction* inst4 = inst3->GetPrevious();
    ASSERT_TRUE(inst4->IsIntConstant());
    ASSERT_EQ(inst4->AsIntConstant()->GetValue(), 3);
  };

  // Expected difference after dead code elimination.
  std::string expected_after_dce =
      "BasicBlock 0, succ: 1\n"
      "  19: IntConstant [14]\n"
      "  0: SuspendCheck\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 5\n"
      "  14: Return(19)\n"
      "BasicBlock 5, pred: 1\n"
      "  15: Exit\n";

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Three-register program with a constant (static) condition.
 *
 *                              16-bit
 *                              offset
 *                              ------
 *     v1 <- 1                  0.      const/4 v1, #+1
 *     v0 <- 0                  1.      const/4 v0, #+0
 *     if v1 >= 0 goto L1       2.      if-gez v1, +3
 *     v0 <- v1                 4.      move v0, v1
 * L1: v2 <- v0 + v1            5.      add-int v2, v0, v1
 *     return-void              7.      return
 */
TEST_F(ConstantFoldingTest, ConstantCondition) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 1 << 8 | 1 << 12,
    Instruction::CONST_4 | 0 << 8 | 0 << 12,
    Instruction::IF_GEZ | 1 << 8, 3,
    Instruction::MOVE | 0 << 8 | 1 << 12,
    Instruction::ADD_INT | 2 << 8, 0 | 1 << 8,
    Instruction::RETURN_VOID);

  std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  3: IntConstant [9, 8, 5]\n"
      "  4: IntConstant [8, 5]\n"
      "  1: SuspendCheck\n"
      "  2: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 5, 2\n"
      "  5: GreaterThanOrEqual(3, 4) [6]\n"
      "  6: If(5)\n"
      "BasicBlock 2, pred: 1, succ: 3\n"
      "  7: Goto 3\n"
      "BasicBlock 3, pred: 5, 2, succ: 4\n"
      "  8: Phi(4, 3) [9]\n"
      "  9: Add(8, 3)\n"
      "  10: ReturnVoid\n"
      "BasicBlock 4, pred: 3\n"
      "  11: Exit\n"
      "BasicBlock 5, pred: 1, succ: 3\n"
      "  0: Goto 3\n";

  // Expected difference after constant folding.
  diff_t expected_cf_diff = {
    { "  3: IntConstant [9, 8, 5]\n",        "  3: IntConstant [6, 9, 8]\n" },
    { "  4: IntConstant [8, 5]\n",           "  4: IntConstant [8]\n" },
    { "  5: GreaterThanOrEqual(3, 4) [6]\n", removed },
    { "  6: If(5)\n",                        "  6: If(3)\n" }
  };
  std::string expected_after_cf = Patch(expected_before, expected_cf_diff);

  // Check the values of the computed constants.
  auto check_after_cf = [](HGraph* graph) {
    HInstruction* inst = graph->GetBlocks()[1]->GetFirstInstruction()->InputAt(0);
    ASSERT_TRUE(inst->IsIntConstant());
    ASSERT_EQ(inst->AsIntConstant()->GetValue(), 1);
  };

  // Expected graph after dead code elimination.
  std::string expected_after_dce =
      "BasicBlock 0, succ: 1\n"
      "  1: SuspendCheck\n"
      "  2: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 4\n"
      "  10: ReturnVoid\n"
      "BasicBlock 4, pred: 1\n"
      "  11: Exit\n";

  TestCode(data,
           expected_before,
           expected_after_cf,
           expected_after_dce,
           check_after_cf);
}

/**
 * Unsigned comparisons with zero. Since these instructions are not present
 * in the bytecode, we need to set up the graph explicitly.
 */
TEST_F(ConstantFoldingTest, UnsignedComparisonsWithZero) {
  graph_ = CreateGraph();
  HBasicBlock* entry_block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(entry_block);
  graph_->SetEntryBlock(entry_block);
  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(block);
  HBasicBlock* exit_block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(exit_block);
  graph_->SetExitBlock(exit_block);
  entry_block->AddSuccessor(block);
  block->AddSuccessor(exit_block);

  // Make various unsigned comparisons with zero against a parameter.
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32, true);
  entry_block->AddInstruction(parameter);
  entry_block->AddInstruction(new (GetAllocator()) HGoto());

  HInstruction* zero = graph_->GetIntConstant(0);

  HInstruction* last;
  block->AddInstruction(last = new (GetAllocator()) HAbove(zero, parameter));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HAbove(parameter, zero));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HAboveOrEqual(zero, parameter));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HAboveOrEqual(parameter, zero));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HBelow(zero, parameter));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HBelow(parameter, zero));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HBelowOrEqual(zero, parameter));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(last = new (GetAllocator()) HBelowOrEqual(parameter, zero));
  block->AddInstruction(new (GetAllocator()) HSelect(last, parameter, parameter, 0));
  block->AddInstruction(new (GetAllocator()) HReturn(zero));

  exit_block->AddInstruction(new (GetAllocator()) HExit());

  graph_->BuildDominatorTree();

  const std::string expected_before =
      "BasicBlock 0, succ: 1\n"
      "  0: ParameterValue [18, 18, 17, 16, 16, 15, 14, 14, 13, 12, 12, 11, 10, 10, 9, "
                            "8, 8, 7, 6, 6, 5, 4, 4, 3]\n"
      "  2: IntConstant [19, 17, 15, 13, 11, 9, 7, 5, 3]\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  3: Above(2, 0) [4]\n"
      "  4: Select(0, 0, 3)\n"
      "  5: Above(0, 2) [6]\n"
      "  6: Select(0, 0, 5)\n"
      "  7: AboveOrEqual(2, 0) [8]\n"
      "  8: Select(0, 0, 7)\n"
      "  9: AboveOrEqual(0, 2) [10]\n"
      "  10: Select(0, 0, 9)\n"
      "  11: Below(2, 0) [12]\n"
      "  12: Select(0, 0, 11)\n"
      "  13: Below(0, 2) [14]\n"
      "  14: Select(0, 0, 13)\n"
      "  15: BelowOrEqual(2, 0) [16]\n"
      "  16: Select(0, 0, 15)\n"
      "  17: BelowOrEqual(0, 2) [18]\n"
      "  18: Select(0, 0, 17)\n"
      "  19: Return(2)\n"
      "BasicBlock 2, pred: 1\n"
      "  20: Exit\n";

  const std::string expected_after_cf =
      "BasicBlock 0, succ: 1\n"
      "  0: ParameterValue [18, 18, 17, 16, 16, 14, 14, 12, 12, 11, 10, 10, "
                            "8, 8, 7, 6, 6, 5, 4, 4]\n"
      "  2: IntConstant [14, 4, 19, 17, 11, 7, 5]\n"
      "  21: IntConstant [16, 10]\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  4: Select(0, 0, 2)\n"
      "  5: Above(0, 2) [6]\n"
      "  6: Select(0, 0, 5)\n"
      "  7: AboveOrEqual(2, 0) [8]\n"
      "  8: Select(0, 0, 7)\n"
      "  10: Select(0, 0, 21)\n"
      "  11: Below(2, 0) [12]\n"
      "  12: Select(0, 0, 11)\n"
      "  14: Select(0, 0, 2)\n"
      "  16: Select(0, 0, 21)\n"
      "  17: BelowOrEqual(0, 2) [18]\n"
      "  18: Select(0, 0, 17)\n"
      "  19: Return(2)\n"
      "BasicBlock 2, pred: 1\n"
      "  20: Exit\n";

  const std::string expected_after_dce =
      "BasicBlock 0, succ: 1\n"
      "  0: ParameterValue\n"
      "  2: IntConstant [19]\n"
      "  1: Goto 1\n"
      "BasicBlock 1, pred: 0, succ: 2\n"
      "  19: Return(2)\n"
      "BasicBlock 2, pred: 1\n"
      "  20: Exit\n";

  auto check_after_cf = [](HGraph* graph) {
    CHECK(graph != nullptr);
  };

  TestCodeOnReadyGraph(expected_before,
                       expected_after_cf,
                       expected_after_dce,
                       check_after_cf);
}

}  // namespace art
