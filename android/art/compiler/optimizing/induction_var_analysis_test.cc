/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <regex>

#include "base/arena_allocator.h"
#include "builder.h"
#include "induction_var_analysis.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

/**
 * Fixture class for the InductionVarAnalysis tests.
 */
class InductionVarAnalysisTest : public OptimizingUnitTest {
 public:
  InductionVarAnalysisTest()
      : iva_(nullptr),
        entry_(nullptr),
        return_(nullptr),
        exit_(nullptr),
        parameter_(nullptr),
        constant0_(nullptr),
        constant1_(nullptr),
        constant2_(nullptr),
        constant7_(nullptr),
        constant100_(nullptr),
        constantm1_(nullptr),
        float_constant0_(nullptr) {
    graph_ = CreateGraph();
  }

  ~InductionVarAnalysisTest() { }

  // Builds single for-loop at depth d.
  void BuildForLoop(int d, int n) {
    ASSERT_LT(d, n);
    loop_preheader_[d] = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_preheader_[d]);
    loop_header_[d] = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_header_[d]);
    loop_preheader_[d]->AddSuccessor(loop_header_[d]);
    if (d < (n - 1)) {
      BuildForLoop(d + 1, n);
    }
    loop_body_[d] = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_body_[d]);
    loop_body_[d]->AddSuccessor(loop_header_[d]);
    if (d < (n - 1)) {
      loop_header_[d]->AddSuccessor(loop_preheader_[d + 1]);
      loop_header_[d + 1]->AddSuccessor(loop_body_[d]);
    } else {
      loop_header_[d]->AddSuccessor(loop_body_[d]);
    }
  }

  // Builds a n-nested loop in CFG where each loop at depth 0 <= d < n
  // is defined as "for (int i_d = 0; i_d < 100; i_d++)". Tests can further
  // populate the loop with instructions to set up interesting scenarios.
  void BuildLoopNest(int n) {
    ASSERT_LE(n, 10);
    graph_->SetNumberOfVRegs(n + 3);

    // Build basic blocks with entry, nested loop, exit.
    entry_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_);
    BuildForLoop(0, n);
    return_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(return_);
    exit_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(exit_);
    entry_->AddSuccessor(loop_preheader_[0]);
    loop_header_[0]->AddSuccessor(return_);
    return_->AddSuccessor(exit_);
    graph_->SetEntryBlock(entry_);
    graph_->SetExitBlock(exit_);

    // Provide entry and exit instructions.
    parameter_ = new (GetAllocator()) HParameterValue(
        graph_->GetDexFile(), dex::TypeIndex(0), 0, DataType::Type::kReference, true);
    entry_->AddInstruction(parameter_);
    constant0_ = graph_->GetIntConstant(0);
    constant1_ = graph_->GetIntConstant(1);
    constant2_ = graph_->GetIntConstant(2);
    constant7_ = graph_->GetIntConstant(7);
    constant100_ = graph_->GetIntConstant(100);
    constantm1_ = graph_->GetIntConstant(-1);
    float_constant0_ = graph_->GetFloatConstant(0.0f);
    return_->AddInstruction(new (GetAllocator()) HReturnVoid());
    exit_->AddInstruction(new (GetAllocator()) HExit());

    // Provide loop instructions.
    for (int d = 0; d < n; d++) {
      basic_[d] = new (GetAllocator()) HPhi(GetAllocator(), d, 0, DataType::Type::kInt32);
      loop_preheader_[d]->AddInstruction(new (GetAllocator()) HGoto());
      loop_header_[d]->AddPhi(basic_[d]);
      HInstruction* compare = new (GetAllocator()) HLessThan(basic_[d], constant100_);
      loop_header_[d]->AddInstruction(compare);
      loop_header_[d]->AddInstruction(new (GetAllocator()) HIf(compare));
      increment_[d] = new (GetAllocator()) HAdd(DataType::Type::kInt32, basic_[d], constant1_);
      loop_body_[d]->AddInstruction(increment_[d]);
      loop_body_[d]->AddInstruction(new (GetAllocator()) HGoto());

      basic_[d]->AddInput(constant0_);
      basic_[d]->AddInput(increment_[d]);
    }
  }

  // Builds if-statement at depth d.
  HPhi* BuildIf(int d, HBasicBlock** ifT, HBasicBlock** ifF) {
    HBasicBlock* cond = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* ifTrue = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* ifFalse = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(cond);
    graph_->AddBlock(ifTrue);
    graph_->AddBlock(ifFalse);
    // Conditional split.
    loop_header_[d]->ReplaceSuccessor(loop_body_[d], cond);
    cond->AddSuccessor(ifTrue);
    cond->AddSuccessor(ifFalse);
    ifTrue->AddSuccessor(loop_body_[d]);
    ifFalse->AddSuccessor(loop_body_[d]);
    cond->AddInstruction(new (GetAllocator()) HIf(parameter_));
    *ifT = ifTrue;
    *ifF = ifFalse;

    HPhi* select_phi = new (GetAllocator()) HPhi(GetAllocator(), -1, 0, DataType::Type::kInt32);
    loop_body_[d]->AddPhi(select_phi);
    return select_phi;
  }

  // Inserts instruction right before increment at depth d.
  HInstruction* InsertInstruction(HInstruction* instruction, int d) {
    loop_body_[d]->InsertInstructionBefore(instruction, increment_[d]);
    return instruction;
  }

  // Inserts a phi to loop header at depth d and returns it.
  HPhi* InsertLoopPhi(int vreg, int d) {
    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), vreg, 0, DataType::Type::kInt32);
    loop_header_[d]->AddPhi(phi);
    return phi;
  }

  // Inserts an array store with given `subscript` at depth d to
  // enable tests to inspect the computed induction at that point easily.
  HInstruction* InsertArrayStore(HInstruction* subscript, int d) {
    // ArraySet is given a float value in order to avoid SsaBuilder typing
    // it from the array's non-existent reference type info.
    return InsertInstruction(new (GetAllocator()) HArraySet(
        parameter_, subscript, float_constant0_, DataType::Type::kFloat32, 0), d);
  }

  // Returns induction information of instruction in loop at depth d.
  std::string GetInductionInfo(HInstruction* instruction, int d) {
    return HInductionVarAnalysis::InductionToString(
        iva_->LookupInfo(loop_body_[d]->GetLoopInformation(), instruction));
  }

  // Returns induction information of the trip-count of loop at depth d.
  std::string GetTripCount(int d) {
    HInstruction* control = loop_header_[d]->GetLastInstruction();
    DCHECK(control->IsIf());
    return GetInductionInfo(control, d);
  }

  // Returns true if instructions have identical induction.
  bool HaveSameInduction(HInstruction* instruction1, HInstruction* instruction2) {
    return HInductionVarAnalysis::InductionEqual(
      iva_->LookupInfo(loop_body_[0]->GetLoopInformation(), instruction1),
      iva_->LookupInfo(loop_body_[0]->GetLoopInformation(), instruction2));
  }

  // Returns true for narrowing linear induction.
  bool IsNarrowingLinear(HInstruction* instruction) {
    return HInductionVarAnalysis::IsNarrowingLinear(
        iva_->LookupInfo(loop_body_[0]->GetLoopInformation(), instruction));
  }

  // Performs InductionVarAnalysis (after proper set up).
  void PerformInductionVarAnalysis() {
    graph_->BuildDominatorTree();
    iva_ = new (GetAllocator()) HInductionVarAnalysis(graph_);
    iva_->Run();
  }

  // General building fields.
  HGraph* graph_;
  HInductionVarAnalysis* iva_;

  // Fixed basic blocks and instructions.
  HBasicBlock* entry_;
  HBasicBlock* return_;
  HBasicBlock* exit_;
  HInstruction* parameter_;  // "this"
  HInstruction* constant0_;
  HInstruction* constant1_;
  HInstruction* constant2_;
  HInstruction* constant7_;
  HInstruction* constant100_;
  HInstruction* constantm1_;
  HInstruction* float_constant0_;

  // Loop specifics.
  HBasicBlock* loop_preheader_[10];
  HBasicBlock* loop_header_[10];
  HBasicBlock* loop_body_[10];
  HInstruction* increment_[10];
  HPhi* basic_[10];  // "vreg_d", the "i_d"
};

//
// The actual InductionVarAnalysis tests.
//

TEST_F(InductionVarAnalysisTest, ProperLoopSetup) {
  // Setup:
  // for (int i_0 = 0; i_0 < 100; i_0++) {
  //   ..
  //     for (int i_9 = 0; i_9 < 100; i_9++) {
  //     }
  //   ..
  // }
  BuildLoopNest(10);
  graph_->BuildDominatorTree();

  ASSERT_EQ(entry_->GetLoopInformation(), nullptr);
  for (int d = 0; d < 1; d++) {
    ASSERT_EQ(loop_preheader_[d]->GetLoopInformation(),
              (d == 0) ? nullptr
                       : loop_header_[d - 1]->GetLoopInformation());
    ASSERT_NE(loop_header_[d]->GetLoopInformation(), nullptr);
    ASSERT_NE(loop_body_[d]->GetLoopInformation(), nullptr);
    ASSERT_EQ(loop_header_[d]->GetLoopInformation(),
              loop_body_[d]->GetLoopInformation());
  }
  ASSERT_EQ(exit_->GetLoopInformation(), nullptr);
}

TEST_F(InductionVarAnalysisTest, FindBasicInduction) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   a[i] = 0;
  // }
  BuildLoopNest(1);
  HInstruction* store = InsertArrayStore(basic_[0], 0);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("((1) * i + (0)):Int32", GetInductionInfo(store->InputAt(1), 0).c_str());
  EXPECT_STREQ("((1) * i + (1)):Int32", GetInductionInfo(increment_[0], 0).c_str());

  // Offset matters!
  EXPECT_FALSE(HaveSameInduction(store->InputAt(1), increment_[0]));

  // Trip-count.
  EXPECT_STREQ("((100) (TC-loop) ((0) < (100)))", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindDerivedInduction) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   t = 100 + i;
  //   t = 100 - i;
  //   t = 100 * i;
  //   t = i << 1;
  //   t = - i;
  // }
  BuildLoopNest(1);
  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, constant100_, basic_[0]), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, constant100_, basic_[0]), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, constant100_, basic_[0]), 0);
  HInstruction* shl = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, basic_[0], constant1_), 0);
  HInstruction* neg = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, basic_[0]), 0);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("((1) * i + (100)):Int32", GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("(( - (1)) * i + (100)):Int32", GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("((100) * i + (0)):Int32", GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("((2) * i + (0)):Int32", GetInductionInfo(shl, 0).c_str());
  EXPECT_STREQ("(( - (1)) * i + (0)):Int32", GetInductionInfo(neg, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindChainInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = k + 100;
  //   a[k] = 0;
  //   k = k - 1;
  //   a[k] = 0;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* store1 = InsertArrayStore(add, 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, add, constant1_), 0);
  HInstruction* store2 = InsertArrayStore(sub, 0);
  k_header->AddInput(sub);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("(((100) - (1)) * i + (0)):Int32",
               GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("(((100) - (1)) * i + (100)):Int32",
               GetInductionInfo(store1->InputAt(1), 0).c_str());
  EXPECT_STREQ("(((100) - (1)) * i + ((100) - (1))):Int32",
               GetInductionInfo(store2->InputAt(1), 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindTwoWayBasicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   if () k = k + 1;
  //   else  k = k + 1;
  //   a[k] = 0;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HBasicBlock* ifTrue;
  HBasicBlock* ifFalse;
  HPhi* k_body = BuildIf(0, &ifTrue, &ifFalse);

  // True-branch.
  HInstruction* inc1 = new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant1_);
  ifTrue->AddInstruction(inc1);
  k_body->AddInput(inc1);
  // False-branch.
  HInstruction* inc2 = new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant1_);
  ifFalse->AddInstruction(inc2);
  k_body->AddInput(inc2);
  // Merge over a phi.
  HInstruction* store = InsertArrayStore(k_body, 0);
  k_header->AddInput(k_body);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("((1) * i + (0)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("((1) * i + (1)):Int32", GetInductionInfo(store->InputAt(1), 0).c_str());

  // Both increments get same induction.
  EXPECT_TRUE(HaveSameInduction(store->InputAt(1), inc1));
  EXPECT_TRUE(HaveSameInduction(store->InputAt(1), inc2));
}

TEST_F(InductionVarAnalysisTest, FindTwoWayDerivedInduction) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   if () k = i + 1;
  //   else  k = i + 1;
  //   a[k] = 0;
  // }
  BuildLoopNest(1);
  HBasicBlock* ifTrue;
  HBasicBlock* ifFalse;
  HPhi* k = BuildIf(0, &ifTrue, &ifFalse);

  // True-branch.
  HInstruction* inc1 = new (GetAllocator()) HAdd(DataType::Type::kInt32, basic_[0], constant1_);
  ifTrue->AddInstruction(inc1);
  k->AddInput(inc1);
  // False-branch.
  HInstruction* inc2 = new (GetAllocator()) HAdd(DataType::Type::kInt32, basic_[0], constant1_);
  ifFalse->AddInstruction(inc2);
  k->AddInput(inc2);
  // Merge over a phi.
  HInstruction* store = InsertArrayStore(k, 0);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("((1) * i + (1)):Int32", GetInductionInfo(store->InputAt(1), 0).c_str());

  // Both increments get same induction.
  EXPECT_TRUE(HaveSameInduction(store->InputAt(1), inc1));
  EXPECT_TRUE(HaveSameInduction(store->InputAt(1), inc2));
}

TEST_F(InductionVarAnalysisTest, AddLinear) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   t1 = i + i;
  //   t2 = 7 + i;
  //   t3 = t1 + t2;
  // }
  BuildLoopNest(1);

  HInstruction* add1 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, basic_[0], basic_[0]), 0);
  HInstruction* add2 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, constant7_, basic_[0]), 0);
  HInstruction* add3 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, add1, add2), 0);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("((1) * i + (0)):Int32", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("(((1) + (1)) * i + (0)):Int32", GetInductionInfo(add1, 0).c_str());
  EXPECT_STREQ("((1) * i + (7)):Int32", GetInductionInfo(add2, 0).c_str());
  EXPECT_STREQ("((((1) + (1)) + (1)) * i + (7)):Int32", GetInductionInfo(add3, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindPolynomialInduction) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   t = i * 2;
  //   t = 100 + t
  //   k = t + k;  // polynomial
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, basic_[0], constant2_), 0);
  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, constant100_, mul), 0);
  HInstruction* pol = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, add, k_header), 0);
  k_header->AddInput(pol);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle and the base linear induction are classified.
  EXPECT_STREQ("poly(sum_lt(((2) * i + (100)):Int32) + (1)):Int32",
               GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("((2) * i + (100)):Int32", GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(pol, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindPolynomialInductionAndDerived) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   t = k + 100;
  //   t = k - 1;
  //   t = - t
  //   t = k * 2;
  //   t = k << 2;
  //   k = k + i;  // polynomial
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* neg = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, sub), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* shl = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* pol = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, basic_[0]), 0);
  k_header->AddInput(pol);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle and derived are classified.
  EXPECT_STREQ("poly(sum_lt(((1) * i + (0)):Int32) + (1)):Int32",
               GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("poly(sum_lt(((1) * i + (0)):Int32) + ((1) + (100))):Int32",
               GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("poly(sum_lt(((1) * i + (0)):Int32) + ((1) - (1))):Int32",
               GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("poly(sum_lt((( - (1)) * i + (0)):Int32) + ((1) - (1))):Int32",
               GetInductionInfo(neg, 0).c_str());
  EXPECT_STREQ("poly(sum_lt(((2) * i + (0)):Int32) + (2)):Int32",
               GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("poly(sum_lt(((4) * i + (0)):Int32) + (4)):Int32",
               GetInductionInfo(shl, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(pol, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, AddPolynomial) {
  // Setup:
  // k = 7;
  // for (int i = 0; i < 100; i++) {
  //   t = k + k;
  //   t = t + k;
  //   k = k + i
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant7_);

  HInstruction* add1 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, k_header), 0);
  HInstruction* add2 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, add1, k_header), 0);
  HInstruction* add3 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, basic_[0]), 0);
  k_header->AddInput(add3);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle and added-derived are classified.
  EXPECT_STREQ("poly(sum_lt(((1) * i + (0)):Int32) + (7)):Int32",
               GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("poly(sum_lt((((1) + (1)) * i + (0)):Int32) + ((7) + (7))):Int32",
               GetInductionInfo(add1, 0).c_str());
  EXPECT_STREQ(
      "poly(sum_lt(((((1) + (1)) + (1)) * i + (0)):Int32) + (((7) + (7)) + (7))):Int32",
      GetInductionInfo(add2, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(add3, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindGeometricMulInduction) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   k = k * 100;  // geometric (x 100)
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, k_header, constant100_), 0);
  k_header->AddInput(mul);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("geo((1) * 100 ^ i + (0)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("geo((100) * 100 ^ i + (0)):Int32", GetInductionInfo(mul, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindGeometricShlInductionAndDerived) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   t = k + 1;
  //   k = k << 1;  // geometric (x 2)
  //   t = k + 100;
  //   t = k - 1;
  //   t = - t;
  //   t = k * 2;
  //   t = k << 2;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* add1 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* shl1 = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* add2 = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, shl1, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, shl1, constant1_), 0);
  HInstruction* neg = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, sub), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, shl1, constant2_), 0);
  HInstruction* shl2 = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, shl1, constant2_), 0);
  k_header->AddInput(shl1);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("geo((1) * 2 ^ i + (0)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("geo((1) * 2 ^ i + (1)):Int32", GetInductionInfo(add1, 0).c_str());
  EXPECT_STREQ("geo((2) * 2 ^ i + (0)):Int32", GetInductionInfo(shl1, 0).c_str());
  EXPECT_STREQ("geo((2) * 2 ^ i + (100)):Int32", GetInductionInfo(add2, 0).c_str());
  EXPECT_STREQ("geo((2) * 2 ^ i + ((0) - (1))):Int32", GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("geo(( - (2)) * 2 ^ i + ( - ((0) - (1)))):Int32",
               GetInductionInfo(neg, 0).c_str());
  EXPECT_STREQ("geo(((2) * (2)) * 2 ^ i + (0)):Int32", GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("geo(((2) * (4)) * 2 ^ i + (0)):Int32", GetInductionInfo(shl2, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindGeometricDivInductionAndDerived) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   t = k + 100;
  //   t = k - 1;
  //   t = - t
  //   t = k * 2;
  //   t = k << 2;
  //   k = k / 100;  // geometric (/ 100)
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* neg = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, sub), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* shl = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* div = InsertInstruction(
      new (GetAllocator()) HDiv(DataType::Type::kInt32, k_header, constant100_, kNoDexPc), 0);
  k_header->AddInput(div);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle and direct additive derived are classified.
  EXPECT_STREQ("geo((1) * 100 ^ -i + (0)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("geo((1) * 100 ^ -i + (100)):Int32", GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("geo((1) * 100 ^ -i + ((0) - (1))):Int32", GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(neg, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(shl, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(div, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindGeometricShrInduction) {
  // Setup:
  // k = 100;
  // for (int i = 0; i < 100; i++) {
  //   k = k >> 1;  // geometric (/ 2)
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant100_);

  HInstruction* shr = InsertInstruction(
      new (GetAllocator()) HShr(DataType::Type::kInt32, k_header, constant1_), 0);
  k_header->AddInput(shr);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle is classified.
  EXPECT_STREQ("geo((100) * 2 ^ -i + (0)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(shr, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindNotGeometricShrInduction) {
  // Setup:
  // k = -1;
  // for (int i = 0; i < 100; i++) {
  //   k = k >> 1;  // initial value is negative
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constantm1_);

  HInstruction* shr = InsertInstruction(
      new (GetAllocator()) HShr(DataType::Type::kInt32, k_header, constant1_), 0);
  k_header->AddInput(shr);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(shr, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindRemWrapAroundInductionAndDerived) {
  // Setup:
  // k = 100;
  // for (int i = 0; i < 100; i++) {
  //   t = k + 100;
  //   t = k - 1;
  //   t = -t
  //   t = k * 2;
  //   t = k << 2;
  //   k = k % 7;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant100_);

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* neg = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, sub), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* shl = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, k_header, constant2_), 0);
  HInstruction* rem = InsertInstruction(
      new (GetAllocator()) HRem(DataType::Type::kInt32, k_header, constant7_, kNoDexPc), 0);
  k_header->AddInput(rem);
  PerformInductionVarAnalysis();

  // Note, only the phi in the cycle and derived are classified.
  EXPECT_STREQ("wrap((100), ((100) % (7))):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("wrap(((100) + (100)), (((100) % (7)) + (100))):Int32",
               GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("wrap(((100) - (1)), (((100) % (7)) - (1))):Int32",
               GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("wrap(( - ((100) - (1))), ( - (((100) % (7)) - (1)))):Int32",
               GetInductionInfo(neg, 0).c_str());
  EXPECT_STREQ("wrap(((100) * (2)), (((100) % (7)) * (2))):Int32",
               GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("wrap(((100) * (4)), (((100) % (7)) * (4))):Int32",
               GetInductionInfo(shl, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(rem, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindFirstOrderWrapAroundInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   a[k] = 0;
  //   k = 100 - i;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* store = InsertArrayStore(k_header, 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, constant100_, basic_[0]), 0);
  k_header->AddInput(sub);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("wrap((0), (( - (1)) * i + (100)):Int32):Int32",
               GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("wrap((0), (( - (1)) * i + (100)):Int32):Int32",
               GetInductionInfo(store->InputAt(1), 0).c_str());
  EXPECT_STREQ("(( - (1)) * i + (100)):Int32", GetInductionInfo(sub, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindSecondOrderWrapAroundInduction) {
  // Setup:
  // k = 0;
  // t = 100;
  // for (int i = 0; i < 100; i++) {
  //   a[k] = 0;
  //   k = t;
  //   t = 100 - i;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);
  HPhi* t = InsertLoopPhi(1, 0);
  t->AddInput(constant100_);

  HInstruction* store = InsertArrayStore(k_header, 0);
  k_header->AddInput(t);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, constant100_, basic_[0], 0), 0);
  t->AddInput(sub);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("wrap((0), wrap((100), (( - (1)) * i + (100)):Int32):Int32):Int32",
               GetInductionInfo(store->InputAt(1), 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindWrapAroundDerivedInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   t = k + 100;
  //   t = k - 100;
  //   t = k * 100;
  //   t = k << 1;
  //   t = - k;
  //   k = i << 1;
  //   t = - k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, k_header, constant100_), 0);
  HInstruction* shl1 = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* neg1 = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, k_header), 0);
  HInstruction* shl2 = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, basic_[0], constant1_), 0);
  HInstruction* neg2 = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, shl2), 0);
  k_header->AddInput(shl2);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("wrap((100), ((2) * i + (100)):Int32):Int32",
               GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("wrap(((0) - (100)), ((2) * i + ((0) - (100))):Int32):Int32",
               GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("wrap((0), (((2) * (100)) * i + (0)):Int32):Int32",
               GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("wrap((0), (((2) * (2)) * i + (0)):Int32):Int32",
               GetInductionInfo(shl1, 0).c_str());
  EXPECT_STREQ("wrap((0), (( - (2)) * i + (0)):Int32):Int32",
               GetInductionInfo(neg1, 0).c_str());
  EXPECT_STREQ("((2) * i + (0)):Int32", GetInductionInfo(shl2, 0).c_str());
  EXPECT_STREQ("(( - (2)) * i + (0)):Int32", GetInductionInfo(neg2, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindPeriodicInduction) {
  // Setup:
  // k = 0;
  // t = 100;
  // for (int i = 0; i < 100; i++) {
  //   a[k] = 0;
  //   a[t] = 0;
  //   // Swap t <-> k.
  //   d = t;
  //   t = k;
  //   k = d;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);
  HPhi* t = InsertLoopPhi(1, 0);
  t->AddInput(constant100_);

  HInstruction* store1 = InsertArrayStore(k_header, 0);
  HInstruction* store2 = InsertArrayStore(t, 0);
  k_header->AddInput(t);
  t->AddInput(k_header);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (100)):Int32", GetInductionInfo(store1->InputAt(1), 0).c_str());
  EXPECT_STREQ("periodic((100), (0)):Int32", GetInductionInfo(store2->InputAt(1), 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindIdiomaticPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   a[k] = 0;
  //   k = 1 - k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* store = InsertArrayStore(k_header, 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, constant1_, k_header), 0);
  k_header->AddInput(sub);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Int32", GetInductionInfo(store->InputAt(1), 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Int32", GetInductionInfo(sub, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindXorPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   a[k] = 0;
  //   k = k ^ 1;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* store = InsertArrayStore(k_header, 0);
  HInstruction* x = InsertInstruction(
      new (GetAllocator()) HXor(DataType::Type::kInt32, k_header, constant1_), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Int32", GetInductionInfo(store->InputAt(1), 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Int32", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindXorConstantLeftPeriodicInduction) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   k = 1 ^ k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* x = InsertInstruction(
      new (GetAllocator()) HXor(DataType::Type::kInt32, constant1_, k_header), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((1), ((1) ^ (1))):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic(((1) ^ (1)), (1)):Int32", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindXor100PeriodicInduction) {
  // Setup:
  // k = 1;
  // for (int i = 0; i < 100; i++) {
  //   k = k ^ 100;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant1_);

  HInstruction* x = InsertInstruction(
      new (GetAllocator()) HXor(DataType::Type::kInt32, k_header, constant100_), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((1), ((1) ^ (100))):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic(((1) ^ (100)), (1)):Int32", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindBooleanEqPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = (k == 0);
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* x = InsertInstruction(new (GetAllocator()) HEqual(k_header, constant0_), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Bool", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Bool", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindBooleanEqConstantLeftPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = (0 == k);
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* x = InsertInstruction(new (GetAllocator()) HEqual(constant0_, k_header), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Bool", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Bool", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindBooleanNePeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = (k != 1);
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* x = InsertInstruction(new (GetAllocator()) HNotEqual(k_header, constant1_), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Bool", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Bool", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindBooleanNeConstantLeftPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = (1 != k);
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* x = InsertInstruction(new (GetAllocator()) HNotEqual(constant1_, k_header), 0);
  k_header->AddInput(x);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Bool", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Bool", GetInductionInfo(x, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindDerivedPeriodicInduction) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   t = - k;
  //   k = 1 - k;
  //   t = k + 100;
  //   t = k - 100;
  //   t = k * 100;
  //   t = k << 1;
  //   t = - k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* neg1 = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, k_header), 0);
  HInstruction* idiom = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, constant1_, k_header), 0);
  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, idiom, constant100_), 0);
  HInstruction* sub = InsertInstruction(
      new (GetAllocator()) HSub(DataType::Type::kInt32, idiom, constant100_), 0);
  HInstruction* mul = InsertInstruction(
      new (GetAllocator()) HMul(DataType::Type::kInt32, idiom, constant100_), 0);
  HInstruction* shl = InsertInstruction(
      new (GetAllocator()) HShl(DataType::Type::kInt32, idiom, constant1_), 0);
  HInstruction* neg2 = InsertInstruction(
      new (GetAllocator()) HNeg(DataType::Type::kInt32, idiom), 0);
  k_header->AddInput(idiom);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("periodic((0), (1)):Int32", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("periodic((0), ( - (1))):Int32", GetInductionInfo(neg1, 0).c_str());
  EXPECT_STREQ("periodic((1), (0)):Int32", GetInductionInfo(idiom, 0).c_str());
  EXPECT_STREQ("periodic(((1) + (100)), (100)):Int32", GetInductionInfo(add, 0).c_str());
  EXPECT_STREQ("periodic(((1) - (100)), ((0) - (100))):Int32", GetInductionInfo(sub, 0).c_str());
  EXPECT_STREQ("periodic((100), (0)):Int32", GetInductionInfo(mul, 0).c_str());
  EXPECT_STREQ("periodic((2), (0)):Int32", GetInductionInfo(shl, 0).c_str());
  EXPECT_STREQ("periodic(( - (1)), (0)):Int32", GetInductionInfo(neg2, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, FindDeepLoopInduction) {
  // Setup:
  // k = 0;
  // for (int i_0 = 0; i_0 < 100; i_0++) {
  //   ..
  //     for (int i_9 = 0; i_9 < 100; i_9++) {
  //       k = 1 + k;
  //       a[k] = 0;
  //     }
  //   ..
  // }
  BuildLoopNest(10);

  HPhi* k_header[10];
  for (int d = 0; d < 10; d++) {
    k_header[d] = InsertLoopPhi(0, d);
  }

  HInstruction* inc = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, constant1_, k_header[9]), 9);
  HInstruction* store = InsertArrayStore(inc, 9);

  for (int d = 0; d < 10; d++) {
    k_header[d]->AddInput((d != 0) ? k_header[d - 1] : constant0_);
    k_header[d]->AddInput((d != 9) ? k_header[d + 1] : inc);
  }
  PerformInductionVarAnalysis();

  // Avoid exact phi number, since that depends on the SSA building phase.
  std::regex r("\\(\\(1\\) \\* i \\+ "
               "\\(\\(1\\) \\+ \\(\\d+:Phi\\)\\)\\):Int32");

  for (int d = 0; d < 10; d++) {
    if (d == 9) {
      EXPECT_TRUE(std::regex_match(GetInductionInfo(store->InputAt(1), d), r));
    } else {
      EXPECT_STREQ("", GetInductionInfo(store->InputAt(1), d).c_str());
    }
    EXPECT_STREQ("((1) * i + (1)):Int32", GetInductionInfo(increment_[d], d).c_str());
    // Trip-count.
    EXPECT_STREQ("((100) (TC-loop) ((0) < (100)))", GetTripCount(d).c_str());
  }
}

TEST_F(InductionVarAnalysisTest, ByteInductionIntLoopControl) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   k = (byte) i;
  //   a[k] = 0;
  //   a[i] = 0;
  // }
  BuildLoopNest(1);
  HInstruction* conv = InsertInstruction(
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, basic_[0], kNoDexPc), 0);
  HInstruction* store1 = InsertArrayStore(conv, 0);
  HInstruction* store2 = InsertArrayStore(basic_[0], 0);
  PerformInductionVarAnalysis();

  // Regular int induction (i) is transferred over conversion into byte induction (k).
  EXPECT_STREQ("((1) * i + (0)):Int8", GetInductionInfo(store1->InputAt(1), 0).c_str());
  EXPECT_STREQ("((1) * i + (0)):Int32",  GetInductionInfo(store2->InputAt(1), 0).c_str());
  EXPECT_STREQ("((1) * i + (1)):Int32",  GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(store1->InputAt(1)));
  EXPECT_FALSE(IsNarrowingLinear(store2->InputAt(1)));

  // Type matters!
  EXPECT_FALSE(HaveSameInduction(store1->InputAt(1), store2->InputAt(1)));

  // Trip-count.
  EXPECT_STREQ("((100) (TC-loop) ((0) < (100)))", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, ByteInductionDerivedIntLoopControl) {
  // Setup:
  // for (int i = 0; i < 100; i++) {
  //   k = (byte) i;
  //   a[k] = 0;
  //   k = k + 1
  //   a[k] = 0;
  // }
  BuildLoopNest(1);
  HInstruction* conv = InsertInstruction(
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, basic_[0], kNoDexPc), 0);
  HInstruction* store1 = InsertArrayStore(conv, 0);
  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, conv, constant1_), 0);
  HInstruction* store2 = InsertArrayStore(add, 0);

  PerformInductionVarAnalysis();

  // Byte induction (k) is detected, but it does not transfer over the addition,
  // since this may yield out-of-type values.
  EXPECT_STREQ("((1) * i + (0)):Int8", GetInductionInfo(store1->InputAt(1), 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(store2->InputAt(1), 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(store1->InputAt(1)));
  EXPECT_FALSE(IsNarrowingLinear(store2->InputAt(1)));  // works for null
}

TEST_F(InductionVarAnalysisTest, ByteInduction) {
  // Setup:
  // k = -128;
  // for (int i = 0; i < 100; i++) {
  //   k = k + 1;
  //   k = (byte) k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(graph_->GetIntConstant(-128));

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* conv = InsertInstruction(
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, add, kNoDexPc), 0);
  k_header->AddInput(conv);
  PerformInductionVarAnalysis();

  // Byte induction (k) is detected, but it does not transfer over the addition,
  // since this may yield out-of-type values.
  EXPECT_STREQ("((1) * i + (-128)):Int8", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(add, 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(k_header));
  EXPECT_FALSE(IsNarrowingLinear(add));  // works for null
}

TEST_F(InductionVarAnalysisTest, NoByteInduction1) {
  // Setup:
  // k = -129;  / does not fit!
  // for (int i = 0; i < 100; i++) {
  //   k = k + 1;
  //   k = (byte) k;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(graph_->GetIntConstant(-129));

  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, k_header, constant1_), 0);
  HInstruction* conv = InsertInstruction(
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, add, kNoDexPc), 0);
  k_header->AddInput(conv);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(add, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, NoByteInduction2) {
  // Setup:
  // k = 0;
  // for (int i = 0; i < 100; i++) {
  //   k = (byte) k;   // conversion not done last!
  //   k = k + 1;
  // }
  BuildLoopNest(1);
  HPhi* k_header = InsertLoopPhi(0, 0);
  k_header->AddInput(constant0_);

  HInstruction* conv = InsertInstruction(
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, k_header, kNoDexPc), 0);
  HInstruction* add = InsertInstruction(
      new (GetAllocator()) HAdd(DataType::Type::kInt32, conv, constant1_), 0);
  k_header->AddInput(add);
  PerformInductionVarAnalysis();

  EXPECT_STREQ("", GetInductionInfo(k_header, 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(add, 0).c_str());
}

TEST_F(InductionVarAnalysisTest, ByteLoopControl1) {
  // Setup:
  // for (byte i = -128; i < 127; i++) {  // just fits!
  // }
  BuildLoopNest(1);
  basic_[0]->ReplaceInput(graph_->GetIntConstant(-128), 0);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(127), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (-128)):Int8", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count.
  EXPECT_STREQ("(((127) - (-128)) (TC-loop) ((-128) < (127)))", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, ByteLoopControl2) {
  // Setup:
  // for (byte i = -128; i < 128; i++) {  // infinite loop!
  // }
  BuildLoopNest(1);
  basic_[0]->ReplaceInput(graph_->GetIntConstant(-128), 0);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(128), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt8, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (-128)):Int8", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count undefined.
  EXPECT_STREQ("", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, ShortLoopControl1) {
  // Setup:
  // for (short i = -32768; i < 32767; i++) {  // just fits!
  // }
  BuildLoopNest(1);
  basic_[0]->ReplaceInput(graph_->GetIntConstant(-32768), 0);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(32767), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt16, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (-32768)):Int16", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count.
  EXPECT_STREQ("(((32767) - (-32768)) (TC-loop) ((-32768) < (32767)))", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, ShortLoopControl2) {
  // Setup:
  // for (short i = -32768; i < 32768; i++) {  // infinite loop!
  // }
  BuildLoopNest(1);
  basic_[0]->ReplaceInput(graph_->GetIntConstant(-32768), 0);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(32768), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kInt16, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (-32768)):Int16", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count undefined.
  EXPECT_STREQ("", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, CharLoopControl1) {
  // Setup:
  // for (char i = 0; i < 65535; i++) {  // just fits!
  // }
  BuildLoopNest(1);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(65535), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kUint16, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (0)):Uint16", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count.
  EXPECT_STREQ("((65535) (TC-loop) ((0) < (65535)))", GetTripCount(0).c_str());
}

TEST_F(InductionVarAnalysisTest, CharLoopControl2) {
  // Setup:
  // for (char i = 0; i < 65536; i++) {  // infinite loop!
  // }
  BuildLoopNest(1);
  HInstruction* ifs = loop_header_[0]->GetLastInstruction()->GetPrevious();
  ifs->ReplaceInput(graph_->GetIntConstant(65536), 1);
  HInstruction* conv =
      new (GetAllocator()) HTypeConversion(DataType::Type::kUint16, increment_[0], kNoDexPc);
  loop_body_[0]->InsertInstructionBefore(conv, increment_[0]->GetNext());
  basic_[0]->ReplaceInput(conv, 1);
  PerformInductionVarAnalysis();

  // Recorded at the phi, but not transferred to increment.
  EXPECT_STREQ("((1) * i + (0)):Uint16", GetInductionInfo(basic_[0], 0).c_str());
  EXPECT_STREQ("", GetInductionInfo(increment_[0], 0).c_str());

  // Narrowing detected.
  EXPECT_TRUE(IsNarrowingLinear(basic_[0]));
  EXPECT_FALSE(IsNarrowingLinear(increment_[0]));  // works for null

  // Trip-count undefined.
  EXPECT_STREQ("", GetTripCount(0).c_str());
}

}  // namespace art
