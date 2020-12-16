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
class OTest : public OptimizingUnitTest {
 public:
  OTest() : graph_(nullptr) { }
  
  std::string GetTestDexFileName(const char* name) const;
  
  void TestCode(const std::vector<uint16_t>& data,
                DataType::Type return_type = DataType::Type::kInt32) {
    graph_ = CreateCFG(data, return_type);
    TestCodeOnReadyGraph();
  }

  void TestCodeOnReadyGraph()
  {
                           
    ASSERT_NE(graph_, nullptr);
    
    StringPrettyPrinter printer_before(graph_);
    printer_before.VisitInsertionOrder();
    std::string actual_before = printer_before.str();
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


    HDeadCodeElimination(graph_, nullptr /* stats */, "dead_code_elimination").Run();
    GraphChecker graph_checker_dce(graph_);
    graph_checker_dce.Run();
    ASSERT_TRUE(graph_checker_dce.IsValid());
    RemoveSuspendChecks(graph_);

    StringPrettyPrinter printer_after_dce(graph_);
    printer_after_dce.VisitInsertionOrder();
    std::string actual_after_dce = printer_after_dce.str();
    
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
TEST_F(OTest, IntConstantFoldingNegation) {
#if 0
  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
      Instruction::CONST_4 | 0 << 8 | 1 << 12,
      Instruction::NEG_INT | 1 << 8 | 0 << 12,
      Instruction::RETURN | 1 << 8);
  const std::vector<uint16_t> data = {0x0006, 0, 0, 0, 0, 0, 15, 0, 
    Instruction::CONST_4 | 0 << 8 | 0 << 12,
    Instruction::CONST_4 | 5 << 8 | 1 << 12,
    0x1312,
    0x3101,
    0x2312,
    0x3201,
    0x1301,
    0x1412,
    0x4333,
    0x0006,
    0x2301,
    0x2412,
    0x4333,
    0x0002,
    0x000e
  };
#endif
  const std::vector<uint16_t> data = {0x0006, 0, 0, 0, 0, 0,
    22,
    0,
    0x1512,
    0x1012,
    0x1312,
    0x3101,
    0x2312,
    0x3201,
    0x2301,
    0x0413,
    0x0080,
    0x33d4,
    0x0080,
    0x1412,
    0x4333,
    0x0009,
    0x1301,
    0x0413,
    0x0040,
    0x03dc,
    0x4003,
    0x0339,
    0x0002,
    0x000e
  };
  TestCode(data);
  for(auto &option : runtime_->GetCompilerOptions())
  {
    std::cout << option << std::endl;
  }
}

}  // namespace art
