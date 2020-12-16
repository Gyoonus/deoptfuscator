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

#include "register_allocator.h"

#include "arch/x86/instruction_set_features_x86.h"
#include "base/arena_allocator.h"
#include "builder.h"
#include "code_generator.h"
#include "code_generator_x86.h"
#include "dex/dex_file.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "register_allocator_linear_scan.h"
#include "ssa_liveness_analysis.h"
#include "ssa_phi_elimination.h"

namespace art {

using Strategy = RegisterAllocator::Strategy;

// Note: the register allocator tests rely on the fact that constants have live
// intervals and registers get allocated to them.

class RegisterAllocatorTest : public OptimizingUnitTest {
 protected:
  // These functions need to access private variables of LocationSummary, so we declare it
  // as a member of RegisterAllocatorTest, which we make a friend class.
  void SameAsFirstInputHint(Strategy strategy);
  void ExpectedInRegisterHint(Strategy strategy);

  // Helper functions that make use of the OptimizingUnitTest's members.
  bool Check(const std::vector<uint16_t>& data, Strategy strategy);
  void CFG1(Strategy strategy);
  void Loop1(Strategy strategy);
  void Loop2(Strategy strategy);
  void Loop3(Strategy strategy);
  void DeadPhi(Strategy strategy);
  HGraph* BuildIfElseWithPhi(HPhi** phi, HInstruction** input1, HInstruction** input2);
  void PhiHint(Strategy strategy);
  HGraph* BuildFieldReturn(HInstruction** field, HInstruction** ret);
  HGraph* BuildTwoSubs(HInstruction** first_sub, HInstruction** second_sub);
  HGraph* BuildDiv(HInstruction** div);
  void ExpectedExactInRegisterAndSameOutputHint(Strategy strategy);

  bool ValidateIntervals(const ScopedArenaVector<LiveInterval*>& intervals,
                         const CodeGenerator& codegen) {
    return RegisterAllocator::ValidateIntervals(ArrayRef<LiveInterval* const>(intervals),
                                                /* number_of_spill_slots */ 0u,
                                                /* number_of_out_slots */ 0u,
                                                codegen,
                                                /* processing_core_registers */ true,
                                                /* log_fatal_on_failure */ false);
  }
};

// This macro should include all register allocation strategies that should be tested.
#define TEST_ALL_STRATEGIES(test_name)\
TEST_F(RegisterAllocatorTest, test_name##_LinearScan) {\
  test_name(Strategy::kRegisterAllocatorLinearScan);\
}\
TEST_F(RegisterAllocatorTest, test_name##_GraphColor) {\
  test_name(Strategy::kRegisterAllocatorGraphColor);\
}

bool RegisterAllocatorTest::Check(const std::vector<uint16_t>& data, Strategy strategy) {
  HGraph* graph = CreateCFG(data);
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();
  std::unique_ptr<RegisterAllocator> register_allocator =
      RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
  register_allocator->AllocateRegisters();
  return register_allocator->Validate(false);
}

/**
 * Unit testing of RegisterAllocator::ValidateIntervals. Register allocator
 * tests are based on this validation method.
 */
TEST_F(RegisterAllocatorTest, ValidateIntervals) {
  HGraph* graph = CreateGraph();
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  ScopedArenaVector<LiveInterval*> intervals(GetScopedAllocator()->Adapter());

  // Test with two intervals of the same range.
  {
    static constexpr size_t ranges[][2] = {{0, 42}};
    intervals.push_back(BuildInterval(ranges, arraysize(ranges), GetScopedAllocator(), 0));
    intervals.push_back(BuildInterval(ranges, arraysize(ranges), GetScopedAllocator(), 1));
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[1]->SetRegister(0);
    ASSERT_FALSE(ValidateIntervals(intervals, codegen));
    intervals.clear();
  }

  // Test with two non-intersecting intervals.
  {
    static constexpr size_t ranges1[][2] = {{0, 42}};
    intervals.push_back(BuildInterval(ranges1, arraysize(ranges1), GetScopedAllocator(), 0));
    static constexpr size_t ranges2[][2] = {{42, 43}};
    intervals.push_back(BuildInterval(ranges2, arraysize(ranges2), GetScopedAllocator(), 1));
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[1]->SetRegister(0);
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));
    intervals.clear();
  }

  // Test with two non-intersecting intervals, with one with a lifetime hole.
  {
    static constexpr size_t ranges1[][2] = {{0, 42}, {45, 48}};
    intervals.push_back(BuildInterval(ranges1, arraysize(ranges1), GetScopedAllocator(), 0));
    static constexpr size_t ranges2[][2] = {{42, 43}};
    intervals.push_back(BuildInterval(ranges2, arraysize(ranges2), GetScopedAllocator(), 1));
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[1]->SetRegister(0);
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));
    intervals.clear();
  }

  // Test with intersecting intervals.
  {
    static constexpr size_t ranges1[][2] = {{0, 42}, {44, 48}};
    intervals.push_back(BuildInterval(ranges1, arraysize(ranges1), GetScopedAllocator(), 0));
    static constexpr size_t ranges2[][2] = {{42, 47}};
    intervals.push_back(BuildInterval(ranges2, arraysize(ranges2), GetScopedAllocator(), 1));
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[1]->SetRegister(0);
    ASSERT_FALSE(ValidateIntervals(intervals, codegen));
    intervals.clear();
  }

  // Test with siblings.
  {
    static constexpr size_t ranges1[][2] = {{0, 42}, {44, 48}};
    intervals.push_back(BuildInterval(ranges1, arraysize(ranges1), GetScopedAllocator(), 0));
    intervals[0]->SplitAt(43);
    static constexpr size_t ranges2[][2] = {{42, 47}};
    intervals.push_back(BuildInterval(ranges2, arraysize(ranges2), GetScopedAllocator(), 1));
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[1]->SetRegister(0);
    // Sibling of the first interval has no register allocated to it.
    ASSERT_TRUE(ValidateIntervals(intervals, codegen));

    intervals[0]->GetNextSibling()->SetRegister(0);
    ASSERT_FALSE(ValidateIntervals(intervals, codegen));
  }
}

void RegisterAllocatorTest::CFG1(Strategy strategy) {
  /*
   * Test the following snippet:
   *  return 0;
   *
   * Which becomes the following graph:
   *       constant0
   *       goto
   *        |
   *       return
   *        |
   *       exit
   */
  const std::vector<uint16_t> data = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN);

  ASSERT_TRUE(Check(data, strategy));
}

TEST_ALL_STRATEGIES(CFG1);

void RegisterAllocatorTest::Loop1(Strategy strategy) {
  /*
   * Test the following snippet:
   *  int a = 0;
   *  while (a == a) {
   *    a = 4;
   *  }
   *  return 5;
   *
   * Which becomes the following graph:
   *       constant0
   *       constant4
   *       constant5
   *       goto
   *        |
   *       goto
   *        |
   *       phi
   *       equal
   *       if +++++
   *        |       \ +
   *        |     goto
   *        |
   *       return
   *        |
   *       exit
   */

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 4,
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::GOTO | 0xFD00,
    Instruction::CONST_4 | 5 << 12 | 1 << 8,
    Instruction::RETURN | 1 << 8);

  ASSERT_TRUE(Check(data, strategy));
}

TEST_ALL_STRATEGIES(Loop1);

void RegisterAllocatorTest::Loop2(Strategy strategy) {
  /*
   * Test the following snippet:
   *  int a = 0;
   *  while (a == 8) {
   *    a = 4 + 5;
   *  }
   *  return 6 + 7;
   *
   * Which becomes the following graph:
   *       constant0
   *       constant4
   *       constant5
   *       constant6
   *       constant7
   *       constant8
   *       goto
   *        |
   *       goto
   *        |
   *       phi
   *       equal
   *       if +++++
   *        |       \ +
   *        |      4 + 5
   *        |      goto
   *        |
   *       6 + 7
   *       return
   *        |
   *       exit
   */

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 8 << 12 | 1 << 8,
    Instruction::IF_EQ | 1 << 8, 7,
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::CONST_4 | 5 << 12 | 1 << 8,
    Instruction::ADD_INT, 1 << 8 | 0,
    Instruction::GOTO | 0xFA00,
    Instruction::CONST_4 | 6 << 12 | 1 << 8,
    Instruction::CONST_4 | 7 << 12 | 1 << 8,
    Instruction::ADD_INT, 1 << 8 | 0,
    Instruction::RETURN | 1 << 8);

  ASSERT_TRUE(Check(data, strategy));
}

TEST_ALL_STRATEGIES(Loop2);

void RegisterAllocatorTest::Loop3(Strategy strategy) {
  /*
   * Test the following snippet:
   *  int a = 0
   *  do {
   *    b = a;
   *    a++;
   *  } while (a != 5)
   *  return b;
   *
   * Which becomes the following graph:
   *       constant0
   *       constant1
   *       constant5
   *       goto
   *        |
   *       goto
   *        |++++++++++++
   *       phi          +
   *       a++          +
   *       equals       +
   *       if           +
   *        |++++++++++++
   *       return
   *        |
   *       exit
   */

  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::ADD_INT_LIT8 | 1 << 8, 1 << 8,
    Instruction::CONST_4 | 5 << 12 | 2 << 8,
    Instruction::IF_NE | 1 << 8 | 2 << 12, 3,
    Instruction::RETURN | 0 << 8,
    Instruction::MOVE | 1 << 12 | 0 << 8,
    Instruction::GOTO | 0xF900);

  HGraph* graph = CreateCFG(data);
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();
  std::unique_ptr<RegisterAllocator> register_allocator =
      RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
  register_allocator->AllocateRegisters();
  ASSERT_TRUE(register_allocator->Validate(false));

  HBasicBlock* loop_header = graph->GetBlocks()[2];
  HPhi* phi = loop_header->GetFirstPhi()->AsPhi();

  LiveInterval* phi_interval = phi->GetLiveInterval();
  LiveInterval* loop_update = phi->InputAt(1)->GetLiveInterval();
  ASSERT_TRUE(phi_interval->HasRegister());
  ASSERT_TRUE(loop_update->HasRegister());
  ASSERT_NE(phi_interval->GetRegister(), loop_update->GetRegister());

  HBasicBlock* return_block = graph->GetBlocks()[3];
  HReturn* ret = return_block->GetLastInstruction()->AsReturn();
  ASSERT_EQ(phi_interval->GetRegister(), ret->InputAt(0)->GetLiveInterval()->GetRegister());
}

TEST_ALL_STRATEGIES(Loop3);

TEST_F(RegisterAllocatorTest, FirstRegisterUse) {
  const std::vector<uint16_t> data = THREE_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::XOR_INT_LIT8 | 1 << 8, 1 << 8,
    Instruction::XOR_INT_LIT8 | 0 << 8, 1 << 8,
    Instruction::XOR_INT_LIT8 | 1 << 8, 1 << 8 | 1,
    Instruction::RETURN_VOID);

  HGraph* graph = CreateCFG(data);
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();

  HXor* first_xor = graph->GetBlocks()[1]->GetFirstInstruction()->AsXor();
  HXor* last_xor = graph->GetBlocks()[1]->GetLastInstruction()->GetPrevious()->AsXor();
  ASSERT_EQ(last_xor->InputAt(0), first_xor);
  LiveInterval* interval = first_xor->GetLiveInterval();
  ASSERT_EQ(interval->GetEnd(), last_xor->GetLifetimePosition());
  ASSERT_TRUE(interval->GetNextSibling() == nullptr);

  // We need a register for the output of the instruction.
  ASSERT_EQ(interval->FirstRegisterUse(), first_xor->GetLifetimePosition());

  // Split at the next instruction.
  interval = interval->SplitAt(first_xor->GetLifetimePosition() + 2);
  // The user of the split is the last add.
  ASSERT_EQ(interval->FirstRegisterUse(), last_xor->GetLifetimePosition());

  // Split before the last add.
  LiveInterval* new_interval = interval->SplitAt(last_xor->GetLifetimePosition() - 1);
  // Ensure the current interval has no register use...
  ASSERT_EQ(interval->FirstRegisterUse(), kNoLifetime);
  // And the new interval has it for the last add.
  ASSERT_EQ(new_interval->FirstRegisterUse(), last_xor->GetLifetimePosition());
}

void RegisterAllocatorTest::DeadPhi(Strategy strategy) {
  /* Test for a dead loop phi taking as back-edge input a phi that also has
   * this loop phi as input. Walking backwards in SsaDeadPhiElimination
   * does not solve the problem because the loop phi will be visited last.
   *
   * Test the following snippet:
   *  int a = 0
   *  do {
   *    if (true) {
   *      a = 2;
   *    }
   *  } while (true);
   */

  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 1 << 8 | 0,
    Instruction::IF_NE | 1 << 8 | 1 << 12, 3,
    Instruction::CONST_4 | 2 << 12 | 0 << 8,
    Instruction::GOTO | 0xFD00,
    Instruction::RETURN_VOID);

  HGraph* graph = CreateCFG(data);
  SsaDeadPhiElimination(graph).Run();
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();
  std::unique_ptr<RegisterAllocator> register_allocator =
      RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
  register_allocator->AllocateRegisters();
  ASSERT_TRUE(register_allocator->Validate(false));
}

TEST_ALL_STRATEGIES(DeadPhi);

/**
 * Test that the TryAllocateFreeReg method works in the presence of inactive intervals
 * that share the same register. It should split the interval it is currently
 * allocating for at the minimum lifetime position between the two inactive intervals.
 * This test only applies to the linear scan allocator.
 */
TEST_F(RegisterAllocatorTest, FreeUntil) {
  const std::vector<uint16_t> data = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN);

  HGraph* graph = CreateCFG(data);
  SsaDeadPhiElimination(graph).Run();
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();
  RegisterAllocatorLinearScan register_allocator(GetScopedAllocator(), &codegen, liveness);

  // Add an artifical range to cover the temps that will be put in the unhandled list.
  LiveInterval* unhandled = graph->GetEntryBlock()->GetFirstInstruction()->GetLiveInterval();
  unhandled->AddLoopRange(0, 60);

  // Populate the instructions in the liveness object, to please the register allocator.
  for (size_t i = 0; i < 60; ++i) {
    liveness.instructions_from_lifetime_position_.push_back(
        graph->GetEntryBlock()->GetFirstInstruction());
  }

  // For SSA value intervals, only an interval resulted from a split may intersect
  // with inactive intervals.
  unhandled = register_allocator.Split(unhandled, 5);

  // Add three temps holding the same register, and starting at different positions.
  // Put the one that should be picked in the middle of the inactive list to ensure
  // we do not depend on an order.
  LiveInterval* interval =
      LiveInterval::MakeFixedInterval(GetScopedAllocator(), 0, DataType::Type::kInt32);
  interval->AddRange(40, 50);
  register_allocator.inactive_.push_back(interval);

  interval = LiveInterval::MakeFixedInterval(GetScopedAllocator(), 0, DataType::Type::kInt32);
  interval->AddRange(20, 30);
  register_allocator.inactive_.push_back(interval);

  interval = LiveInterval::MakeFixedInterval(GetScopedAllocator(), 0, DataType::Type::kInt32);
  interval->AddRange(60, 70);
  register_allocator.inactive_.push_back(interval);

  register_allocator.number_of_registers_ = 1;
  register_allocator.registers_array_ = GetAllocator()->AllocArray<size_t>(1);
  register_allocator.processing_core_registers_ = true;
  register_allocator.unhandled_ = &register_allocator.unhandled_core_intervals_;

  ASSERT_TRUE(register_allocator.TryAllocateFreeReg(unhandled));

  // Check that we have split the interval.
  ASSERT_EQ(1u, register_allocator.unhandled_->size());
  // Check that we know need to find a new register where the next interval
  // that uses the register starts.
  ASSERT_EQ(20u, register_allocator.unhandled_->front()->GetStart());
}

HGraph* RegisterAllocatorTest::BuildIfElseWithPhi(HPhi** phi,
                                                  HInstruction** input1,
                                                  HInstruction** input2) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  HInstruction* test = new (GetAllocator()) HInstanceFieldGet(parameter,
                                                              nullptr,
                                                              DataType::Type::kBool,
                                                              MemberOffset(22),
                                                              false,
                                                              kUnknownFieldIndex,
                                                              kUnknownClassDefIndex,
                                                              graph->GetDexFile(),
                                                              0);
  block->AddInstruction(test);
  block->AddInstruction(new (GetAllocator()) HIf(test));
  HBasicBlock* then = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* else_ = new (GetAllocator()) HBasicBlock(graph);
  HBasicBlock* join = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(then);
  graph->AddBlock(else_);
  graph->AddBlock(join);

  block->AddSuccessor(then);
  block->AddSuccessor(else_);
  then->AddSuccessor(join);
  else_->AddSuccessor(join);
  then->AddInstruction(new (GetAllocator()) HGoto());
  else_->AddInstruction(new (GetAllocator()) HGoto());

  *phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
  join->AddPhi(*phi);
  *input1 = new (GetAllocator()) HInstanceFieldGet(parameter,
                                                   nullptr,
                                                   DataType::Type::kInt32,
                                                   MemberOffset(42),
                                                   false,
                                                   kUnknownFieldIndex,
                                                   kUnknownClassDefIndex,
                                                   graph->GetDexFile(),
                                                   0);
  *input2 = new (GetAllocator()) HInstanceFieldGet(parameter,
                                                   nullptr,
                                                   DataType::Type::kInt32,
                                                   MemberOffset(42),
                                                   false,
                                                   kUnknownFieldIndex,
                                                   kUnknownClassDefIndex,
                                                   graph->GetDexFile(),
                                                   0);
  then->AddInstruction(*input1);
  else_->AddInstruction(*input2);
  join->AddInstruction(new (GetAllocator()) HExit());
  (*phi)->AddInput(*input1);
  (*phi)->AddInput(*input2);

  graph->BuildDominatorTree();
  graph->AnalyzeLoops();
  return graph;
}

void RegisterAllocatorTest::PhiHint(Strategy strategy) {
  HPhi *phi;
  HInstruction *input1, *input2;

  {
    HGraph* graph = BuildIfElseWithPhi(&phi, &input1, &input2);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // Check that the register allocator is deterministic.
    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(input1->GetLiveInterval()->GetRegister(), 0);
    ASSERT_EQ(input2->GetLiveInterval()->GetRegister(), 0);
    ASSERT_EQ(phi->GetLiveInterval()->GetRegister(), 0);
  }

  {
    HGraph* graph = BuildIfElseWithPhi(&phi, &input1, &input2);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // Set the phi to a specific register, and check that the inputs get allocated
    // the same register.
    phi->GetLocations()->UpdateOut(Location::RegisterLocation(2));
    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(input1->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(input2->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(phi->GetLiveInterval()->GetRegister(), 2);
  }

  {
    HGraph* graph = BuildIfElseWithPhi(&phi, &input1, &input2);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // Set input1 to a specific register, and check that the phi and other input get allocated
    // the same register.
    input1->GetLocations()->UpdateOut(Location::RegisterLocation(2));
    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(input1->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(input2->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(phi->GetLiveInterval()->GetRegister(), 2);
  }

  {
    HGraph* graph = BuildIfElseWithPhi(&phi, &input1, &input2);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // Set input2 to a specific register, and check that the phi and other input get allocated
    // the same register.
    input2->GetLocations()->UpdateOut(Location::RegisterLocation(2));
    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(input1->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(input2->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(phi->GetLiveInterval()->GetRegister(), 2);
  }
}

// TODO: Enable this test for graph coloring register allocation when iterative move
//       coalescing is merged.
TEST_F(RegisterAllocatorTest, PhiHint_LinearScan) {
  PhiHint(Strategy::kRegisterAllocatorLinearScan);
}

HGraph* RegisterAllocatorTest::BuildFieldReturn(HInstruction** field, HInstruction** ret) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference);
  entry->AddInstruction(parameter);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  *field = new (GetAllocator()) HInstanceFieldGet(parameter,
                                                  nullptr,
                                                  DataType::Type::kInt32,
                                                  MemberOffset(42),
                                                  false,
                                                  kUnknownFieldIndex,
                                                  kUnknownClassDefIndex,
                                                  graph->GetDexFile(),
                                                  0);
  block->AddInstruction(*field);
  *ret = new (GetAllocator()) HReturn(*field);
  block->AddInstruction(*ret);

  HBasicBlock* exit = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(exit);
  block->AddSuccessor(exit);
  exit->AddInstruction(new (GetAllocator()) HExit());

  graph->BuildDominatorTree();
  return graph;
}

void RegisterAllocatorTest::ExpectedInRegisterHint(Strategy strategy) {
  HInstruction *field, *ret;

  {
    HGraph* graph = BuildFieldReturn(&field, &ret);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    // Sanity check that in normal conditions, the register should be hinted to 0 (EAX).
    ASSERT_EQ(field->GetLiveInterval()->GetRegister(), 0);
  }

  {
    HGraph* graph = BuildFieldReturn(&field, &ret);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // Check that the field gets put in the register expected by its use.
    // Don't use SetInAt because we are overriding an already allocated location.
    ret->GetLocations()->inputs_[0] = Location::RegisterLocation(2);

    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(field->GetLiveInterval()->GetRegister(), 2);
  }
}

// TODO: Enable this test for graph coloring register allocation when iterative move
//       coalescing is merged.
TEST_F(RegisterAllocatorTest, ExpectedInRegisterHint_LinearScan) {
  ExpectedInRegisterHint(Strategy::kRegisterAllocatorLinearScan);
}

HGraph* RegisterAllocatorTest::BuildTwoSubs(HInstruction** first_sub, HInstruction** second_sub) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* parameter = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry->AddInstruction(parameter);

  HInstruction* constant1 = graph->GetIntConstant(1);
  HInstruction* constant2 = graph->GetIntConstant(2);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  *first_sub = new (GetAllocator()) HSub(DataType::Type::kInt32, parameter, constant1);
  block->AddInstruction(*first_sub);
  *second_sub = new (GetAllocator()) HSub(DataType::Type::kInt32, *first_sub, constant2);
  block->AddInstruction(*second_sub);

  block->AddInstruction(new (GetAllocator()) HExit());

  graph->BuildDominatorTree();
  return graph;
}

void RegisterAllocatorTest::SameAsFirstInputHint(Strategy strategy) {
  HInstruction *first_sub, *second_sub;

  {
    HGraph* graph = BuildTwoSubs(&first_sub, &second_sub);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    // Sanity check that in normal conditions, the registers are the same.
    ASSERT_EQ(first_sub->GetLiveInterval()->GetRegister(), 1);
    ASSERT_EQ(second_sub->GetLiveInterval()->GetRegister(), 1);
  }

  {
    HGraph* graph = BuildTwoSubs(&first_sub, &second_sub);
    std::unique_ptr<const X86InstructionSetFeatures> features_x86(
        X86InstructionSetFeatures::FromCppDefines());
    x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
    SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
    liveness.Analyze();

    // check that both adds get the same register.
    // Don't use UpdateOutput because output is already allocated.
    first_sub->InputAt(0)->GetLocations()->output_ = Location::RegisterLocation(2);
    ASSERT_EQ(first_sub->GetLocations()->Out().GetPolicy(), Location::kSameAsFirstInput);
    ASSERT_EQ(second_sub->GetLocations()->Out().GetPolicy(), Location::kSameAsFirstInput);

    std::unique_ptr<RegisterAllocator> register_allocator =
        RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
    register_allocator->AllocateRegisters();

    ASSERT_EQ(first_sub->GetLiveInterval()->GetRegister(), 2);
    ASSERT_EQ(second_sub->GetLiveInterval()->GetRegister(), 2);
  }
}

// TODO: Enable this test for graph coloring register allocation when iterative move
//       coalescing is merged.
TEST_F(RegisterAllocatorTest, SameAsFirstInputHint_LinearScan) {
  SameAsFirstInputHint(Strategy::kRegisterAllocatorLinearScan);
}

HGraph* RegisterAllocatorTest::BuildDiv(HInstruction** div) {
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* first = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* second = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry->AddInstruction(first);
  entry->AddInstruction(second);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);

  *div = new (GetAllocator()) HDiv(
      DataType::Type::kInt32, first, second, 0);  // don't care about dex_pc.
  block->AddInstruction(*div);

  block->AddInstruction(new (GetAllocator()) HExit());

  graph->BuildDominatorTree();
  return graph;
}

void RegisterAllocatorTest::ExpectedExactInRegisterAndSameOutputHint(Strategy strategy) {
  HInstruction *div;
  HGraph* graph = BuildDiv(&div);
  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  liveness.Analyze();

  std::unique_ptr<RegisterAllocator> register_allocator =
      RegisterAllocator::Create(GetScopedAllocator(), &codegen, liveness, strategy);
  register_allocator->AllocateRegisters();

  // div on x86 requires its first input in eax and the output be the same as the first input.
  ASSERT_EQ(div->GetLiveInterval()->GetRegister(), 0);
}

// TODO: Enable this test for graph coloring register allocation when iterative move
//       coalescing is merged.
TEST_F(RegisterAllocatorTest, ExpectedExactInRegisterAndSameOutputHint_LinearScan) {
  ExpectedExactInRegisterAndSameOutputHint(Strategy::kRegisterAllocatorLinearScan);
}

// Test a bug in the register allocator, where allocating a blocked
// register would lead to spilling an inactive interval at the wrong
// position.
// This test only applies to the linear scan allocator.
TEST_F(RegisterAllocatorTest, SpillInactive) {
  // Create a synthesized graph to please the register_allocator and
  // ssa_liveness_analysis code.
  HGraph* graph = CreateGraph();
  HBasicBlock* entry = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  HInstruction* one = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* two = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* three = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  HInstruction* four = new (GetAllocator()) HParameterValue(
      graph->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kInt32);
  entry->AddInstruction(one);
  entry->AddInstruction(two);
  entry->AddInstruction(three);
  entry->AddInstruction(four);

  HBasicBlock* block = new (GetAllocator()) HBasicBlock(graph);
  graph->AddBlock(block);
  entry->AddSuccessor(block);
  block->AddInstruction(new (GetAllocator()) HExit());

  // We create a synthesized user requesting a register, to avoid just spilling the
  // intervals.
  HPhi* user = new (GetAllocator()) HPhi(GetAllocator(), 0, 1, DataType::Type::kInt32);
  user->AddInput(one);
  user->SetBlock(block);
  LocationSummary* locations = new (GetAllocator()) LocationSummary(user, LocationSummary::kNoCall);
  locations->SetInAt(0, Location::RequiresRegister());
  static constexpr size_t phi_ranges[][2] = {{20, 30}};
  BuildInterval(phi_ranges, arraysize(phi_ranges), GetScopedAllocator(), -1, user);

  // Create an interval with lifetime holes.
  static constexpr size_t ranges1[][2] = {{0, 2}, {4, 6}, {8, 10}};
  LiveInterval* first = BuildInterval(ranges1, arraysize(ranges1), GetScopedAllocator(), -1, one);
  first->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 8));
  first->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 7));
  first->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 6));

  locations = new (GetAllocator()) LocationSummary(first->GetDefinedBy(), LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
  first = first->SplitAt(1);

  // Create an interval that conflicts with the next interval, to force the next
  // interval to call `AllocateBlockedReg`.
  static constexpr size_t ranges2[][2] = {{2, 4}};
  LiveInterval* second = BuildInterval(ranges2, arraysize(ranges2), GetScopedAllocator(), -1, two);
  locations =
      new (GetAllocator()) LocationSummary(second->GetDefinedBy(), LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());

  // Create an interval that will lead to splitting the first interval. The bug occured
  // by splitting at a wrong position, in this case at the next intersection between
  // this interval and the first interval. We would have then put the interval with ranges
  // "[0, 2(, [4, 6(" in the list of handled intervals, even though we haven't processed intervals
  // before lifetime position 6 yet.
  static constexpr size_t ranges3[][2] = {{2, 4}, {8, 10}};
  LiveInterval* third = BuildInterval(ranges3, arraysize(ranges3), GetScopedAllocator(), -1, three);
  third->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 8));
  third->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 4));
  third->uses_.push_front(*new (GetScopedAllocator()) UsePosition(user, false, 3));
  locations = new (GetAllocator()) LocationSummary(third->GetDefinedBy(), LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());
  third = third->SplitAt(3);

  // Because the first part of the split interval was considered handled, this interval
  // was free to allocate the same register, even though it conflicts with it.
  static constexpr size_t ranges4[][2] = {{4, 6}};
  LiveInterval* fourth = BuildInterval(ranges4, arraysize(ranges4), GetScopedAllocator(), -1, four);
  locations =
      new (GetAllocator()) LocationSummary(fourth->GetDefinedBy(), LocationSummary::kNoCall);
  locations->SetOut(Location::RequiresRegister());

  std::unique_ptr<const X86InstructionSetFeatures> features_x86(
      X86InstructionSetFeatures::FromCppDefines());
  x86::CodeGeneratorX86 codegen(graph, *features_x86.get(), CompilerOptions());
  SsaLivenessAnalysis liveness(graph, &codegen, GetScopedAllocator());
  // Populate the instructions in the liveness object, to please the register allocator.
  for (size_t i = 0; i < 32; ++i) {
    liveness.instructions_from_lifetime_position_.push_back(user);
  }

  RegisterAllocatorLinearScan register_allocator(GetScopedAllocator(), &codegen, liveness);
  register_allocator.unhandled_core_intervals_.push_back(fourth);
  register_allocator.unhandled_core_intervals_.push_back(third);
  register_allocator.unhandled_core_intervals_.push_back(second);
  register_allocator.unhandled_core_intervals_.push_back(first);

  // Set just one register available to make all intervals compete for the same.
  register_allocator.number_of_registers_ = 1;
  register_allocator.registers_array_ = GetAllocator()->AllocArray<size_t>(1);
  register_allocator.processing_core_registers_ = true;
  register_allocator.unhandled_ = &register_allocator.unhandled_core_intervals_;
  register_allocator.LinearScan();

  // Test that there is no conflicts between intervals.
  ScopedArenaVector<LiveInterval*> intervals(GetScopedAllocator()->Adapter());
  intervals.push_back(first);
  intervals.push_back(second);
  intervals.push_back(third);
  intervals.push_back(fourth);
  ASSERT_TRUE(ValidateIntervals(intervals, codegen));
}

}  // namespace art
