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

#include "induction_var_range.h"

#include "base/arena_allocator.h"
#include "builder.h"
#include "induction_var_analysis.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

using Value = InductionVarRange::Value;

/**
 * Fixture class for the InductionVarRange tests.
 */
class InductionVarRangeTest : public OptimizingUnitTest {
 public:
  InductionVarRangeTest()
      : graph_(CreateGraph()),
        iva_(new (GetAllocator()) HInductionVarAnalysis(graph_)),
        range_(iva_) {
    BuildGraph();
  }

  ~InductionVarRangeTest() { }

  void ExpectEqual(Value v1, Value v2) {
    EXPECT_EQ(v1.instruction, v2.instruction);
    EXPECT_EQ(v1.a_constant, v2.a_constant);
    EXPECT_EQ(v1.b_constant, v2.b_constant);
    EXPECT_EQ(v1.is_known, v2.is_known);
  }

  void ExpectInt(int32_t value, HInstruction* i) {
    ASSERT_TRUE(i->IsIntConstant());
    EXPECT_EQ(value, i->AsIntConstant()->GetValue());
  }

  //
  // Construction methods.
  //

  /** Constructs bare minimum graph. */
  void BuildGraph() {
    graph_->SetNumberOfVRegs(1);
    entry_block_ = new (GetAllocator()) HBasicBlock(graph_);
    exit_block_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->AddBlock(exit_block_);
    graph_->SetEntryBlock(entry_block_);
    graph_->SetExitBlock(exit_block_);
    // Two parameters.
    x_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                              dex::TypeIndex(0),
                                              0,
                                              DataType::Type::kInt32);
    entry_block_->AddInstruction(x_);
    y_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                              dex::TypeIndex(0),
                                              0,
                                              DataType::Type::kInt32);
    entry_block_->AddInstruction(y_);
    // Set arbitrary range analysis hint while testing private methods.
    SetHint(x_);
  }

  /** Constructs loop with given upper bound. */
  void BuildLoop(int32_t lower, HInstruction* upper, int32_t stride) {
    // Control flow.
    loop_preheader_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_preheader_);
    loop_header_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_header_);
    loop_body_ = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(loop_body_);
    HBasicBlock* return_block = new (GetAllocator()) HBasicBlock(graph_);
    graph_->AddBlock(return_block);
    entry_block_->AddSuccessor(loop_preheader_);
    loop_preheader_->AddSuccessor(loop_header_);
    loop_header_->AddSuccessor(loop_body_);
    loop_header_->AddSuccessor(return_block);
    loop_body_->AddSuccessor(loop_header_);
    return_block->AddSuccessor(exit_block_);
    // Instructions.
    loop_preheader_->AddInstruction(new (GetAllocator()) HGoto());
    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    loop_header_->AddPhi(phi);
    phi->AddInput(graph_->GetIntConstant(lower));  // i = l
    if (stride > 0) {
      condition_ = new (GetAllocator()) HLessThan(phi, upper);  // i < u
    } else {
      condition_ = new (GetAllocator()) HGreaterThan(phi, upper);  // i > u
    }
    loop_header_->AddInstruction(condition_);
    loop_header_->AddInstruction(new (GetAllocator()) HIf(condition_));
    increment_ =
        new (GetAllocator()) HAdd(DataType::Type::kInt32, phi, graph_->GetIntConstant(stride));
    loop_body_->AddInstruction(increment_);  // i += s
    phi->AddInput(increment_);
    loop_body_->AddInstruction(new (GetAllocator()) HGoto());
    return_block->AddInstruction(new (GetAllocator()) HReturnVoid());
    exit_block_->AddInstruction(new (GetAllocator()) HExit());
  }

  /** Constructs SSA and performs induction variable analysis. */
  void PerformInductionVarAnalysis() {
    graph_->BuildDominatorTree();
    iva_->Run();
  }

  /** Sets hint. */
  void SetHint(HInstruction* hint) {
    range_.chase_hint_ = hint;
  }

  /** Constructs an invariant. */
  HInductionVarAnalysis::InductionInfo* CreateInvariant(char opc,
                                                        HInductionVarAnalysis::InductionInfo* a,
                                                        HInductionVarAnalysis::InductionInfo* b) {
    HInductionVarAnalysis::InductionOp op;
    switch (opc) {
      case '+': op = HInductionVarAnalysis::kAdd; break;
      case '-': op = HInductionVarAnalysis::kSub; break;
      case 'n': op = HInductionVarAnalysis::kNeg; break;
      case '*': op = HInductionVarAnalysis::kMul; break;
      case '/': op = HInductionVarAnalysis::kDiv; break;
      case '%': op = HInductionVarAnalysis::kRem; break;
      case '^': op = HInductionVarAnalysis::kXor; break;
      case '<': op = HInductionVarAnalysis::kLT;  break;
      default:  op = HInductionVarAnalysis::kNop; break;
    }
    return iva_->CreateInvariantOp(op, a, b);
  }

  /** Constructs a fetch. */
  HInductionVarAnalysis::InductionInfo* CreateFetch(HInstruction* fetch) {
    return iva_->CreateInvariantFetch(fetch);
  }

  /** Constructs a constant. */
  HInductionVarAnalysis::InductionInfo* CreateConst(int32_t c) {
    return CreateFetch(graph_->GetIntConstant(c));
  }

  /** Constructs a constant trip-count. */
  HInductionVarAnalysis::InductionInfo* CreateTripCount(int32_t tc, bool in_loop, bool safe) {
    HInductionVarAnalysis::InductionOp op = HInductionVarAnalysis::kTripCountInBodyUnsafe;
    if (in_loop && safe) {
      op = HInductionVarAnalysis::kTripCountInLoop;
    } else if (in_loop) {
      op = HInductionVarAnalysis::kTripCountInLoopUnsafe;
    } else if (safe) {
      op = HInductionVarAnalysis::kTripCountInBody;
    }
    // Return TC with taken-test 0 < TC.
    return iva_->CreateTripCount(op,
                                 CreateConst(tc),
                                 CreateInvariant('<', CreateConst(0), CreateConst(tc)),
                                 DataType::Type::kInt32);
  }

  /** Constructs a linear a * i + b induction. */
  HInductionVarAnalysis::InductionInfo* CreateLinear(int32_t a, int32_t b) {
    return iva_->CreateInduction(HInductionVarAnalysis::kLinear,
                                 HInductionVarAnalysis::kNop,
                                 CreateConst(a),
                                 CreateConst(b),
                                 nullptr,
                                 DataType::Type::kInt32);
  }

  /** Constructs a polynomial sum(a * i + b) + c induction. */
  HInductionVarAnalysis::InductionInfo* CreatePolynomial(int32_t a, int32_t b, int32_t c) {
    return iva_->CreateInduction(HInductionVarAnalysis::kPolynomial,
                                 HInductionVarAnalysis::kNop,
                                 CreateLinear(a, b),
                                 CreateConst(c),
                                 nullptr,
                                 DataType::Type::kInt32);
  }

  /** Constructs a geometric a * f^i + b induction. */
  HInductionVarAnalysis::InductionInfo* CreateGeometric(int32_t a, int32_t b, int32_t f, char op) {
    return iva_->CreateInduction(HInductionVarAnalysis::kGeometric,
                                 op == '*' ? HInductionVarAnalysis::kMul
                                           : HInductionVarAnalysis::kDiv,
                                 CreateConst(a),
                                 CreateConst(b),
                                 graph_->GetIntConstant(f),
                                 DataType::Type::kInt32);
  }

  /** Constructs a range [lo, hi] using a periodic induction. */
  HInductionVarAnalysis::InductionInfo* CreateRange(int32_t lo, int32_t hi) {
    return iva_->CreateInduction(HInductionVarAnalysis::kPeriodic,
                                 HInductionVarAnalysis::kNop,
                                 CreateConst(lo),
                                 CreateConst(hi),
                                 nullptr,
                                 DataType::Type::kInt32);
  }

  /** Constructs a wrap-around induction consisting of a constant, followed by info. */
  HInductionVarAnalysis::InductionInfo* CreateWrapAround(
      int32_t initial,
      HInductionVarAnalysis::InductionInfo* info) {
    return iva_->CreateInduction(HInductionVarAnalysis::kWrapAround,
                                 HInductionVarAnalysis::kNop,
                                 CreateConst(initial),
                                 info,
                                 nullptr,
                                 DataType::Type::kInt32);
  }

  /** Constructs a wrap-around induction consisting of a constant, followed by a range. */
  HInductionVarAnalysis::InductionInfo* CreateWrapAround(int32_t initial, int32_t lo, int32_t hi) {
    return CreateWrapAround(initial, CreateRange(lo, hi));
  }

  //
  // Relay methods.
  //

  bool NeedsTripCount(HInductionVarAnalysis::InductionInfo* info) {
    int64_t s = 0;
    return range_.NeedsTripCount(info, &s);
  }

  bool IsBodyTripCount(HInductionVarAnalysis::InductionInfo* trip) {
    return range_.IsBodyTripCount(trip);
  }

  bool IsUnsafeTripCount(HInductionVarAnalysis::InductionInfo* trip) {
    return range_.IsUnsafeTripCount(trip);
  }

  Value GetMin(HInductionVarAnalysis::InductionInfo* info,
               HInductionVarAnalysis::InductionInfo* trip) {
    return range_.GetVal(info, trip, /* in_body */ true, /* is_min */ true);
  }

  Value GetMax(HInductionVarAnalysis::InductionInfo* info,
               HInductionVarAnalysis::InductionInfo* trip) {
    return range_.GetVal(info, trip, /* in_body */ true, /* is_min */ false);
  }

  Value GetMul(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2,
               bool is_min) {
    return range_.GetMul(info1, info2, nullptr, /* in_body */ true, is_min);
  }

  Value GetDiv(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2,
               bool is_min) {
    return range_.GetDiv(info1, info2, nullptr, /* in_body */ true, is_min);
  }

  Value GetRem(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2) {
    return range_.GetRem(info1, info2);
  }

  Value GetXor(HInductionVarAnalysis::InductionInfo* info1,
               HInductionVarAnalysis::InductionInfo* info2) {
    return range_.GetXor(info1, info2);
  }

  bool IsExact(HInductionVarAnalysis::InductionInfo* info, int64_t* value) {
    return range_.IsConstant(info, InductionVarRange::kExact, value);
  }

  bool IsAtMost(HInductionVarAnalysis::InductionInfo* info, int64_t* value) {
    return range_.IsConstant(info, InductionVarRange::kAtMost, value);
  }

  bool IsAtLeast(HInductionVarAnalysis::InductionInfo* info, int64_t* value) {
    return range_.IsConstant(info, InductionVarRange::kAtLeast, value);
  }

  Value AddValue(Value v1, Value v2) { return range_.AddValue(v1, v2); }
  Value SubValue(Value v1, Value v2) { return range_.SubValue(v1, v2); }
  Value MulValue(Value v1, Value v2) { return range_.MulValue(v1, v2); }
  Value DivValue(Value v1, Value v2) { return range_.DivValue(v1, v2); }
  Value MinValue(Value v1, Value v2) { return range_.MergeVal(v1, v2, true); }
  Value MaxValue(Value v1, Value v2) { return range_.MergeVal(v1, v2, false); }

  // General building fields.
  HGraph* graph_;
  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;
  HBasicBlock* loop_preheader_;
  HBasicBlock* loop_header_;
  HBasicBlock* loop_body_;
  HInductionVarAnalysis* iva_;
  InductionVarRange range_;

  // Instructions.
  HInstruction* condition_;
  HInstruction* increment_;
  HInstruction* x_;
  HInstruction* y_;
};

//
// Tests on private methods.
//

TEST_F(InductionVarRangeTest, IsConstant) {
  int64_t value;
  // Constant.
  EXPECT_TRUE(IsExact(CreateConst(12345), &value));
  EXPECT_EQ(12345, value);
  EXPECT_TRUE(IsAtMost(CreateConst(12345), &value));
  EXPECT_EQ(12345, value);
  EXPECT_TRUE(IsAtLeast(CreateConst(12345), &value));
  EXPECT_EQ(12345, value);
  // Constant trivial range.
  EXPECT_TRUE(IsExact(CreateRange(111, 111), &value));
  EXPECT_EQ(111, value);
  EXPECT_TRUE(IsAtMost(CreateRange(111, 111), &value));
  EXPECT_EQ(111, value);
  EXPECT_TRUE(IsAtLeast(CreateRange(111, 111), &value));
  EXPECT_EQ(111, value);
  // Constant non-trivial range.
  EXPECT_FALSE(IsExact(CreateRange(11, 22), &value));
  EXPECT_TRUE(IsAtMost(CreateRange(11, 22), &value));
  EXPECT_EQ(22, value);
  EXPECT_TRUE(IsAtLeast(CreateRange(11, 22), &value));
  EXPECT_EQ(11, value);
  // Symbolic.
  EXPECT_FALSE(IsExact(CreateFetch(x_), &value));
  EXPECT_FALSE(IsAtMost(CreateFetch(x_), &value));
  EXPECT_FALSE(IsAtLeast(CreateFetch(x_), &value));
}

TEST_F(InductionVarRangeTest, TripCountProperties) {
  EXPECT_FALSE(NeedsTripCount(nullptr));
  EXPECT_FALSE(NeedsTripCount(CreateConst(1)));
  EXPECT_TRUE(NeedsTripCount(CreateLinear(1, 1)));
  EXPECT_FALSE(NeedsTripCount(CreateWrapAround(1, 2, 3)));
  EXPECT_TRUE(NeedsTripCount(CreateWrapAround(1, CreateLinear(1, 1))));

  EXPECT_FALSE(IsBodyTripCount(nullptr));
  EXPECT_FALSE(IsBodyTripCount(CreateTripCount(100, true, true)));
  EXPECT_FALSE(IsBodyTripCount(CreateTripCount(100, true, false)));
  EXPECT_TRUE(IsBodyTripCount(CreateTripCount(100, false, true)));
  EXPECT_TRUE(IsBodyTripCount(CreateTripCount(100, false, false)));

  EXPECT_FALSE(IsUnsafeTripCount(nullptr));
  EXPECT_FALSE(IsUnsafeTripCount(CreateTripCount(100, true, true)));
  EXPECT_TRUE(IsUnsafeTripCount(CreateTripCount(100, true, false)));
  EXPECT_FALSE(IsUnsafeTripCount(CreateTripCount(100, false, true)));
  EXPECT_TRUE(IsUnsafeTripCount(CreateTripCount(100, false, false)));
}

TEST_F(InductionVarRangeTest, GetMinMaxNull) {
  ExpectEqual(Value(), GetMin(nullptr, nullptr));
  ExpectEqual(Value(), GetMax(nullptr, nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxAdd) {
  ExpectEqual(Value(12),
              GetMin(CreateInvariant('+', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(22),
              GetMax(CreateInvariant('+', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(x_, 1, -20),
              GetMin(CreateInvariant('+', CreateFetch(x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(x_, 1, -10),
              GetMax(CreateInvariant('+', CreateFetch(x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(x_, 1, 10),
              GetMin(CreateInvariant('+', CreateRange(10, 20), CreateFetch(x_)), nullptr));
  ExpectEqual(Value(x_, 1, 20),
              GetMax(CreateInvariant('+', CreateRange(10, 20), CreateFetch(x_)), nullptr));
  ExpectEqual(Value(5),
              GetMin(CreateInvariant('+', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(19),
              GetMax(CreateInvariant('+', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxSub) {
  ExpectEqual(Value(-18),
              GetMin(CreateInvariant('-', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-8),
              GetMax(CreateInvariant('-', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(x_, 1, 10),
              GetMin(CreateInvariant('-', CreateFetch(x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(x_, 1, 20),
              GetMax(CreateInvariant('-', CreateFetch(x_), CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(x_, -1, 10),
              GetMin(CreateInvariant('-', CreateRange(10, 20), CreateFetch(x_)), nullptr));
  ExpectEqual(Value(x_, -1, 20),
              GetMax(CreateInvariant('-', CreateRange(10, 20), CreateFetch(x_)), nullptr));
  ExpectEqual(Value(-25),
              GetMin(CreateInvariant('-', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-11),
              GetMax(CreateInvariant('-', CreateRange(-5, -1), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxNeg) {
  ExpectEqual(Value(-20), GetMin(CreateInvariant('n', nullptr, CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(-10), GetMax(CreateInvariant('n', nullptr, CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(10), GetMin(CreateInvariant('n', nullptr, CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(20), GetMax(CreateInvariant('n', nullptr, CreateRange(-20, -10)), nullptr));
  ExpectEqual(Value(x_, -1, 0), GetMin(CreateInvariant('n', nullptr, CreateFetch(x_)), nullptr));
  ExpectEqual(Value(x_, -1, 0), GetMax(CreateInvariant('n', nullptr, CreateFetch(x_)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxMul) {
  ExpectEqual(Value(20),
              GetMin(CreateInvariant('*', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(40),
              GetMax(CreateInvariant('*', CreateConst(2), CreateRange(10, 20)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxDiv) {
  ExpectEqual(Value(3),
              GetMin(CreateInvariant('/', CreateRange(12, 20), CreateConst(4)), nullptr));
  ExpectEqual(Value(5),
              GetMax(CreateInvariant('/', CreateRange(12, 20), CreateConst(4)), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxConstant) {
  ExpectEqual(Value(12345), GetMin(CreateConst(12345), nullptr));
  ExpectEqual(Value(12345), GetMax(CreateConst(12345), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxFetch) {
  ExpectEqual(Value(x_, 1, 0), GetMin(CreateFetch(x_), nullptr));
  ExpectEqual(Value(x_, 1, 0), GetMax(CreateFetch(x_), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxLinear) {
  ExpectEqual(Value(20), GetMin(CreateLinear(10, 20), CreateTripCount(100, true, true)));
  ExpectEqual(Value(1010), GetMax(CreateLinear(10, 20), CreateTripCount(100, true, true)));
  ExpectEqual(Value(-970), GetMin(CreateLinear(-10, 20), CreateTripCount(100, true, true)));
  ExpectEqual(Value(20), GetMax(CreateLinear(-10, 20), CreateTripCount(100, true, true)));
}

TEST_F(InductionVarRangeTest, GetMinMaxWrapAround) {
  ExpectEqual(Value(-5), GetMin(CreateWrapAround(-5, -1, 10), nullptr));
  ExpectEqual(Value(10), GetMax(CreateWrapAround(-5, -1, 10), nullptr));
  ExpectEqual(Value(-1), GetMin(CreateWrapAround(2, -1, 10), nullptr));
  ExpectEqual(Value(10), GetMax(CreateWrapAround(2, -1, 10), nullptr));
  ExpectEqual(Value(-1), GetMin(CreateWrapAround(20, -1, 10), nullptr));
  ExpectEqual(Value(20), GetMax(CreateWrapAround(20, -1, 10), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxPolynomial) {
  ExpectEqual(Value(7), GetMin(CreatePolynomial(3, 5, 7), nullptr));
  ExpectEqual(Value(), GetMax(CreatePolynomial(3, 5, 7), nullptr));
  ExpectEqual(Value(7), GetMin(CreatePolynomial(3, 5, 7), CreateTripCount(5, true, true)));
  ExpectEqual(Value(45), GetMax(CreatePolynomial(3, 5, 7), CreateTripCount(5, true, true)));
  ExpectEqual(Value(7), GetMin(CreatePolynomial(3, 5, 7), CreateTripCount(10, true, true)));
  ExpectEqual(Value(160), GetMax(CreatePolynomial(3, 5, 7), CreateTripCount(10, true, true)));
  ExpectEqual(Value(-7), GetMin(CreatePolynomial(11, 13, -7),
                               CreateTripCount(5, true, true)));
  ExpectEqual(Value(111), GetMax(CreatePolynomial(11, 13, -7),
                                 CreateTripCount(5, true, true)));
  ExpectEqual(Value(-7), GetMin(CreatePolynomial(11, 13, -7),
                               CreateTripCount(10, true, true)));
  ExpectEqual(Value(506), GetMax(CreatePolynomial(11, 13, -7),
                                 CreateTripCount(10, true, true)));
  ExpectEqual(Value(), GetMin(CreatePolynomial(-3, 5, 7), CreateTripCount(10, true, true)));
  ExpectEqual(Value(), GetMax(CreatePolynomial(-3, 5, 7), CreateTripCount(10, true, true)));
  ExpectEqual(Value(), GetMin(CreatePolynomial(3, -5, 7), CreateTripCount(10, true, true)));
  ExpectEqual(Value(), GetMax(CreatePolynomial(3, -5, 7), CreateTripCount(10, true, true)));
}

TEST_F(InductionVarRangeTest, GetMinMaxGeometricMul) {
  ExpectEqual(Value(), GetMin(CreateGeometric(1, 1, 1, '*'), nullptr));
  ExpectEqual(Value(), GetMax(CreateGeometric(1, 1, 1, '*'), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxGeometricDiv) {
  ExpectEqual(Value(5), GetMin(CreateGeometric(11, 5, 3, '/'), nullptr));
  ExpectEqual(Value(16), GetMax(CreateGeometric(11, 5, 3, '/'), nullptr));
  ExpectEqual(Value(-5), GetMin(CreateGeometric(11, -5, 3, '/'), nullptr));
  ExpectEqual(Value(6), GetMax(CreateGeometric(11, -5, 3, '/'), nullptr));
  ExpectEqual(Value(-6), GetMin(CreateGeometric(-11, 5, 3, '/'), nullptr));
  ExpectEqual(Value(5), GetMax(CreateGeometric(-11, 5, 3, '/'), nullptr));
  ExpectEqual(Value(-16), GetMin(CreateGeometric(-11, -5, 3, '/'), nullptr));
  ExpectEqual(Value(-5), GetMax(CreateGeometric(-11, -5, 3, '/'), nullptr));
}

TEST_F(InductionVarRangeTest, GetMinMaxPeriodic) {
  ExpectEqual(Value(-2), GetMin(CreateRange(-2, 99), nullptr));
  ExpectEqual(Value(99), GetMax(CreateRange(-2, 99), nullptr));
}

TEST_F(InductionVarRangeTest, GetMulMin) {
  ExpectEqual(Value(-14), GetMul(CreateConst(2), CreateRange(-7, 8), true));
  ExpectEqual(Value(-16), GetMul(CreateConst(-2), CreateRange(-7, 8), true));
  ExpectEqual(Value(-14), GetMul(CreateRange(-7, 8), CreateConst(2), true));
  ExpectEqual(Value(-16), GetMul(CreateRange(-7, 8), CreateConst(-2), true));
  ExpectEqual(Value(6), GetMul(CreateRange(2, 10), CreateRange(3, 5), true));
  ExpectEqual(Value(-50), GetMul(CreateRange(2, 10), CreateRange(-5, -3), true));
  ExpectEqual(Value(), GetMul(CreateRange(2, 10), CreateRange(-1, 1), true));
  ExpectEqual(Value(-50), GetMul(CreateRange(-10, -2), CreateRange(3, 5), true));
  ExpectEqual(Value(6), GetMul(CreateRange(-10, -2), CreateRange(-5, -3), true));
  ExpectEqual(Value(), GetMul(CreateRange(-10, -2), CreateRange(-1, 1), true));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(2, 10), true));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(-10, -2), true));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(-1, 1), true));
}

TEST_F(InductionVarRangeTest, GetMulMax) {
  ExpectEqual(Value(16), GetMul(CreateConst(2), CreateRange(-7, 8), false));
  ExpectEqual(Value(14), GetMul(CreateConst(-2), CreateRange(-7, 8), false));
  ExpectEqual(Value(16), GetMul(CreateRange(-7, 8), CreateConst(2), false));
  ExpectEqual(Value(14), GetMul(CreateRange(-7, 8), CreateConst(-2), false));
  ExpectEqual(Value(50), GetMul(CreateRange(2, 10), CreateRange(3, 5), false));
  ExpectEqual(Value(-6), GetMul(CreateRange(2, 10), CreateRange(-5, -3), false));
  ExpectEqual(Value(), GetMul(CreateRange(2, 10), CreateRange(-1, 1), false));
  ExpectEqual(Value(-6), GetMul(CreateRange(-10, -2), CreateRange(3, 5), false));
  ExpectEqual(Value(50), GetMul(CreateRange(-10, -2), CreateRange(-5, -3), false));
  ExpectEqual(Value(), GetMul(CreateRange(-10, -2), CreateRange(-1, 1), false));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(2, 10), false));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(-10, -2), false));
  ExpectEqual(Value(), GetMul(CreateRange(-1, 1), CreateRange(-1, 1), false));
}

TEST_F(InductionVarRangeTest, GetDivMin) {
  ExpectEqual(Value(-5), GetDiv(CreateRange(-10, 20), CreateConst(2), true));
  ExpectEqual(Value(-10), GetDiv(CreateRange(-10, 20), CreateConst(-2), true));
  ExpectEqual(Value(10), GetDiv(CreateRange(40, 1000), CreateRange(2, 4), true));
  ExpectEqual(Value(-500), GetDiv(CreateRange(40, 1000), CreateRange(-4, -2), true));
  ExpectEqual(Value(), GetDiv(CreateRange(40, 1000), CreateRange(-1, 1), true));
  ExpectEqual(Value(-500), GetDiv(CreateRange(-1000, -40), CreateRange(2, 4), true));
  ExpectEqual(Value(10), GetDiv(CreateRange(-1000, -40), CreateRange(-4, -2), true));
  ExpectEqual(Value(), GetDiv(CreateRange(-1000, -40), CreateRange(-1, 1), true));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(40, 1000), true));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(-1000, -40), true));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(-1, 1), true));
}

TEST_F(InductionVarRangeTest, GetDivMax) {
  ExpectEqual(Value(10), GetDiv(CreateRange(-10, 20), CreateConst(2), false));
  ExpectEqual(Value(5), GetDiv(CreateRange(-10, 20), CreateConst(-2), false));
  ExpectEqual(Value(500), GetDiv(CreateRange(40, 1000), CreateRange(2, 4), false));
  ExpectEqual(Value(-10), GetDiv(CreateRange(40, 1000), CreateRange(-4, -2), false));
  ExpectEqual(Value(), GetDiv(CreateRange(40, 1000), CreateRange(-1, 1), false));
  ExpectEqual(Value(-10), GetDiv(CreateRange(-1000, -40), CreateRange(2, 4), false));
  ExpectEqual(Value(500), GetDiv(CreateRange(-1000, -40), CreateRange(-4, -2), false));
  ExpectEqual(Value(), GetDiv(CreateRange(-1000, -40), CreateRange(-1, 1), false));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(40, 1000), false));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(-1000, 40), false));
  ExpectEqual(Value(), GetDiv(CreateRange(-1, 1), CreateRange(-1, 1), false));
}

TEST_F(InductionVarRangeTest, GetMinMaxRem) {
  ExpectEqual(Value(), GetMin(CreateInvariant('%', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(), GetMax(CreateInvariant('%', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(), GetMin(CreateInvariant('%', CreateRange(10, 20), CreateConst(2)), nullptr));
  ExpectEqual(Value(), GetMax(CreateInvariant('%', CreateRange(10, 20), CreateConst(2)), nullptr));
  ExpectEqual(Value(2), GetMin(CreateInvariant('%', CreateConst(2), CreateConst(5)), nullptr));
  ExpectEqual(Value(2), GetMax(CreateInvariant('%', CreateConst(2), CreateConst(5)), nullptr));
  ExpectEqual(Value(1), GetMin(CreateInvariant('%', CreateConst(11), CreateConst(5)), nullptr));
  ExpectEqual(Value(1), GetMax(CreateInvariant('%', CreateConst(11), CreateConst(5)), nullptr));
}

TEST_F(InductionVarRangeTest, GetRem) {
  ExpectEqual(Value(0), GetRem(CreateConst(1), CreateConst(1)));
  ExpectEqual(Value(2), GetRem(CreateConst(2), CreateConst(5)));
  ExpectEqual(Value(1), GetRem(CreateConst(11), CreateConst(5)));
  ExpectEqual(Value(-2), GetRem(CreateConst(-2), CreateConst(5)));
  ExpectEqual(Value(-1), GetRem(CreateConst(-11), CreateConst(5)));
  ExpectEqual(Value(2), GetRem(CreateConst(2), CreateConst(-5)));
  ExpectEqual(Value(1), GetRem(CreateConst(11), CreateConst(-5)));
  ExpectEqual(Value(-2), GetRem(CreateConst(-2), CreateConst(-5)));
  ExpectEqual(Value(-1), GetRem(CreateConst(-11), CreateConst(-5)));
  ExpectEqual(Value(), GetRem(CreateConst(1), CreateConst(0)));
}

TEST_F(InductionVarRangeTest, GetMinMaxXor) {
  ExpectEqual(Value(), GetMin(CreateInvariant('^', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(), GetMax(CreateInvariant('^', CreateConst(2), CreateRange(10, 20)), nullptr));
  ExpectEqual(Value(), GetMin(CreateInvariant('^', CreateRange(10, 20), CreateConst(2)), nullptr));
  ExpectEqual(Value(), GetMax(CreateInvariant('^', CreateRange(10, 20), CreateConst(2)), nullptr));
  ExpectEqual(Value(3), GetMin(CreateInvariant('^', CreateConst(1), CreateConst(2)), nullptr));
  ExpectEqual(Value(3), GetMax(CreateInvariant('^', CreateConst(1), CreateConst(2)), nullptr));
}

TEST_F(InductionVarRangeTest, GetXor) {
  ExpectEqual(Value(0), GetXor(CreateConst(1), CreateConst(1)));
  ExpectEqual(Value(3), GetXor(CreateConst(1), CreateConst(2)));
  ExpectEqual(Value(-2), GetXor(CreateConst(1), CreateConst(-1)));
  ExpectEqual(Value(0), GetXor(CreateConst(-1), CreateConst(-1)));
}

TEST_F(InductionVarRangeTest, AddValue) {
  ExpectEqual(Value(110), AddValue(Value(10), Value(100)));
  ExpectEqual(Value(-5), AddValue(Value(x_, 1, -4), Value(x_, -1, -1)));
  ExpectEqual(Value(x_, 3, -5), AddValue(Value(x_, 2, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(), AddValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(x_, 1, 23), AddValue(Value(x_, 1, 20), Value(3)));
  ExpectEqual(Value(y_, 1, 5), AddValue(Value(55), Value(y_, 1, -50)));
  const int32_t max_value = std::numeric_limits<int32_t>::max();
  ExpectEqual(Value(max_value), AddValue(Value(max_value - 5), Value(5)));
  ExpectEqual(Value(), AddValue(Value(max_value - 5), Value(6)));  // unsafe
}

TEST_F(InductionVarRangeTest, SubValue) {
  ExpectEqual(Value(-90), SubValue(Value(10), Value(100)));
  ExpectEqual(Value(-3), SubValue(Value(x_, 1, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(x_, 2, -3), SubValue(Value(x_, 3, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(), SubValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(x_, 1, 17), SubValue(Value(x_, 1, 20), Value(3)));
  ExpectEqual(Value(y_, -4, 105), SubValue(Value(55), Value(y_, 4, -50)));
  const int32_t min_value = std::numeric_limits<int32_t>::min();
  ExpectEqual(Value(min_value), SubValue(Value(min_value + 5), Value(5)));
  ExpectEqual(Value(), SubValue(Value(min_value + 5), Value(6)));  // unsafe
}

TEST_F(InductionVarRangeTest, MulValue) {
  ExpectEqual(Value(1000), MulValue(Value(10), Value(100)));
  ExpectEqual(Value(), MulValue(Value(x_, 1, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(), MulValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(x_, 9, 60), MulValue(Value(x_, 3, 20), Value(3)));
  ExpectEqual(Value(y_, 55, -110), MulValue(Value(55), Value(y_, 1, -2)));
  ExpectEqual(Value(), MulValue(Value(90000), Value(-90000)));  // unsafe
}

TEST_F(InductionVarRangeTest, MulValueSpecial) {
  const int32_t min_value = std::numeric_limits<int32_t>::min();
  const int32_t max_value = std::numeric_limits<int32_t>::max();

  // Unsafe.
  ExpectEqual(Value(), MulValue(Value(min_value), Value(min_value)));
  ExpectEqual(Value(), MulValue(Value(min_value), Value(-1)));
  ExpectEqual(Value(), MulValue(Value(min_value), Value(max_value)));
  ExpectEqual(Value(), MulValue(Value(max_value), Value(max_value)));

  // Safe.
  ExpectEqual(Value(min_value), MulValue(Value(min_value), Value(1)));
  ExpectEqual(Value(max_value), MulValue(Value(max_value), Value(1)));
  ExpectEqual(Value(-max_value), MulValue(Value(max_value), Value(-1)));
  ExpectEqual(Value(-1), MulValue(Value(1), Value(-1)));
  ExpectEqual(Value(1), MulValue(Value(-1), Value(-1)));
}

TEST_F(InductionVarRangeTest, DivValue) {
  ExpectEqual(Value(25), DivValue(Value(100), Value(4)));
  ExpectEqual(Value(), DivValue(Value(x_, 1, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(), DivValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(), DivValue(Value(x_, 12, 24), Value(3)));
  ExpectEqual(Value(), DivValue(Value(55), Value(y_, 1, -50)));
  ExpectEqual(Value(), DivValue(Value(1), Value(0)));  // unsafe
}

TEST_F(InductionVarRangeTest, DivValueSpecial) {
  const int32_t min_value = std::numeric_limits<int32_t>::min();
  const int32_t max_value = std::numeric_limits<int32_t>::max();

  // Unsafe.
  ExpectEqual(Value(), DivValue(Value(min_value), Value(-1)));

  // Safe.
  ExpectEqual(Value(1), DivValue(Value(min_value), Value(min_value)));
  ExpectEqual(Value(1), DivValue(Value(max_value), Value(max_value)));
  ExpectEqual(Value(min_value), DivValue(Value(min_value), Value(1)));
  ExpectEqual(Value(max_value), DivValue(Value(max_value), Value(1)));
  ExpectEqual(Value(-max_value), DivValue(Value(max_value), Value(-1)));
  ExpectEqual(Value(-1), DivValue(Value(1), Value(-1)));
  ExpectEqual(Value(1), DivValue(Value(-1), Value(-1)));
}

TEST_F(InductionVarRangeTest, MinValue) {
  ExpectEqual(Value(10), MinValue(Value(10), Value(100)));
  ExpectEqual(Value(x_, 1, -4), MinValue(Value(x_, 1, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(x_, 4, -4), MinValue(Value(x_, 4, -4), Value(x_, 4, -1)));
  ExpectEqual(Value(), MinValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(), MinValue(Value(x_, 1, 20), Value(3)));
  ExpectEqual(Value(), MinValue(Value(55), Value(y_, 1, -50)));
}

TEST_F(InductionVarRangeTest, MaxValue) {
  ExpectEqual(Value(100), MaxValue(Value(10), Value(100)));
  ExpectEqual(Value(x_, 1, -1), MaxValue(Value(x_, 1, -4), Value(x_, 1, -1)));
  ExpectEqual(Value(x_, 4, -1), MaxValue(Value(x_, 4, -4), Value(x_, 4, -1)));
  ExpectEqual(Value(), MaxValue(Value(x_, 1, 5), Value(y_, 1, -7)));
  ExpectEqual(Value(), MaxValue(Value(x_, 1, 20), Value(3)));
  ExpectEqual(Value(), MaxValue(Value(55), Value(y_, 1, -50)));
}

TEST_F(InductionVarRangeTest, ArrayLengthAndHints) {
  // We pass a bogus constant for the class to avoid mocking one.
  HInstruction* new_array = new (GetAllocator()) HNewArray(x_, x_, 0);
  entry_block_->AddInstruction(new_array);
  HInstruction* array_length = new (GetAllocator()) HArrayLength(new_array, 0);
  entry_block_->AddInstruction(array_length);
  // With null hint: yields extreme constants.
  const int32_t max_value = std::numeric_limits<int32_t>::max();
  SetHint(nullptr);
  ExpectEqual(Value(0), GetMin(CreateFetch(array_length), nullptr));
  ExpectEqual(Value(max_value), GetMax(CreateFetch(array_length), nullptr));
  // With explicit hint: yields the length instruction.
  SetHint(array_length);
  ExpectEqual(Value(array_length, 1, 0), GetMin(CreateFetch(array_length), nullptr));
  ExpectEqual(Value(array_length, 1, 0), GetMax(CreateFetch(array_length), nullptr));
  // With any non-null hint: chases beyond the length instruction.
  SetHint(x_);
  ExpectEqual(Value(x_, 1, 0), GetMin(CreateFetch(array_length), nullptr));
  ExpectEqual(Value(x_, 1, 0), GetMax(CreateFetch(array_length), nullptr));
}

TEST_F(InductionVarRangeTest, AddOrSubAndConstant) {
  HInstruction* add = new (GetAllocator())
      HAdd(DataType::Type::kInt32, x_, graph_->GetIntConstant(-1));
  HInstruction* alt = new (GetAllocator())
      HAdd(DataType::Type::kInt32, graph_->GetIntConstant(-1), x_);
  HInstruction* sub = new (GetAllocator())
      HSub(DataType::Type::kInt32, x_, graph_->GetIntConstant(1));
  HInstruction* rev = new (GetAllocator())
      HSub(DataType::Type::kInt32, graph_->GetIntConstant(1), x_);
  entry_block_->AddInstruction(add);
  entry_block_->AddInstruction(alt);
  entry_block_->AddInstruction(sub);
  entry_block_->AddInstruction(rev);
  ExpectEqual(Value(x_, 1, -1), GetMin(CreateFetch(add), nullptr));
  ExpectEqual(Value(x_, 1, -1), GetMax(CreateFetch(add), nullptr));
  ExpectEqual(Value(x_, 1, -1), GetMin(CreateFetch(alt), nullptr));
  ExpectEqual(Value(x_, 1, -1), GetMax(CreateFetch(alt), nullptr));
  ExpectEqual(Value(x_, 1, -1), GetMin(CreateFetch(sub), nullptr));
  ExpectEqual(Value(x_, 1, -1), GetMax(CreateFetch(sub), nullptr));
  ExpectEqual(Value(x_, -1, 1), GetMin(CreateFetch(rev), nullptr));
  ExpectEqual(Value(x_, -1, 1), GetMax(CreateFetch(rev), nullptr));
}

//
// Tests on public methods.
//

TEST_F(InductionVarRangeTest, ConstantTripCountUp) {
  BuildLoop(0, graph_->GetIntConstant(1000), 1);
  PerformInductionVarAnalysis();

  Value v1, v2;
  bool needs_finite_test = true;
  bool needs_taken_test = true;

  HInstruction* phi = condition_->InputAt(0);
  HInstruction* exit = exit_block_->GetLastInstruction();

  // In context of header: known.
  range_.GetInductionRange(condition_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(1000), v2);

  // In context of loop-body: known.
  range_.GetInductionRange(increment_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(999), v2);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(1), v1);
  ExpectEqual(Value(1000), v2);

  // Induction vs. no-induction.
  EXPECT_TRUE(range_.CanGenerateRange(increment_, phi, &needs_finite_test, &needs_taken_test));
  EXPECT_TRUE(range_.CanGenerateLastValue(phi));
  EXPECT_FALSE(range_.CanGenerateRange(exit, exit, &needs_finite_test, &needs_taken_test));
  EXPECT_FALSE(range_.CanGenerateLastValue(exit));

  // Last value (unsimplified).
  HInstruction* last = range_.GenerateLastValue(phi, graph_, loop_preheader_);
  ASSERT_TRUE(last->IsAdd());
  ExpectInt(1000, last->InputAt(0));
  ExpectInt(0, last->InputAt(1));

  // Loop logic.
  int64_t tc = 0;
  EXPECT_TRUE(range_.IsFinite(loop_header_->GetLoopInformation(), &tc));
  EXPECT_EQ(1000, tc);
  HInstruction* offset = nullptr;
  EXPECT_TRUE(range_.IsUnitStride(phi, phi, graph_, &offset));
  ExpectInt(0, offset);
  HInstruction* tce = range_.GenerateTripCount(
      loop_header_->GetLoopInformation(), graph_, loop_preheader_);
  ASSERT_TRUE(tce != nullptr);
  ExpectInt(1000, tce);
}

TEST_F(InductionVarRangeTest, ConstantTripCountDown) {
  BuildLoop(1000, graph_->GetIntConstant(0), -1);
  PerformInductionVarAnalysis();

  Value v1, v2;
  bool needs_finite_test = true;
  bool needs_taken_test = true;

  HInstruction* phi = condition_->InputAt(0);
  HInstruction* exit = exit_block_->GetLastInstruction();

  // In context of header: known.
  range_.GetInductionRange(condition_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(1000), v2);

  // In context of loop-body: known.
  range_.GetInductionRange(increment_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(1), v1);
  ExpectEqual(Value(1000), v2);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(999), v2);

  // Induction vs. no-induction.
  EXPECT_TRUE(range_.CanGenerateRange(increment_, phi, &needs_finite_test, &needs_taken_test));
  EXPECT_TRUE(range_.CanGenerateLastValue(phi));
  EXPECT_FALSE(range_.CanGenerateRange(exit, exit, &needs_finite_test, &needs_taken_test));
  EXPECT_FALSE(range_.CanGenerateLastValue(exit));

  // Last value (unsimplified).
  HInstruction* last = range_.GenerateLastValue(phi, graph_, loop_preheader_);
  ASSERT_TRUE(last->IsSub());
  ExpectInt(1000, last->InputAt(0));
  ASSERT_TRUE(last->InputAt(1)->IsNeg());
  last = last->InputAt(1)->InputAt(0);
  ASSERT_TRUE(last->IsSub());
  ExpectInt(0, last->InputAt(0));
  ExpectInt(1000, last->InputAt(1));

  // Loop logic.
  int64_t tc = 0;
  EXPECT_TRUE(range_.IsFinite(loop_header_->GetLoopInformation(), &tc));
  EXPECT_EQ(1000, tc);
  HInstruction* offset = nullptr;
  EXPECT_FALSE(range_.IsUnitStride(phi, phi, graph_, &offset));
  HInstruction* tce = range_.GenerateTripCount(
      loop_header_->GetLoopInformation(), graph_, loop_preheader_);
  ASSERT_TRUE(tce != nullptr);
  ASSERT_TRUE(tce->IsNeg());
  last = tce->InputAt(0);
  EXPECT_TRUE(last->IsSub());
  ExpectInt(0, last->InputAt(0));
  ExpectInt(1000, last->InputAt(1));
}

TEST_F(InductionVarRangeTest, SymbolicTripCountUp) {
  BuildLoop(0, x_, 1);
  PerformInductionVarAnalysis();

  Value v1, v2;
  bool needs_finite_test = true;
  bool needs_taken_test = true;

  HInstruction* phi = condition_->InputAt(0);

  // In context of header: upper unknown.
  range_.GetInductionRange(condition_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(), v2);

  // In context of loop-body: known.
  range_.GetInductionRange(increment_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(0), v1);
  ExpectEqual(Value(x_, 1, -1), v2);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(1), v1);
  ExpectEqual(Value(x_, 1, 0), v2);

  HInstruction* lower = nullptr;
  HInstruction* upper = nullptr;

  // Can generate code in context of loop-body only.
  EXPECT_FALSE(range_.CanGenerateRange(condition_, phi, &needs_finite_test, &needs_taken_test));
  ASSERT_TRUE(range_.CanGenerateRange(increment_, phi, &needs_finite_test, &needs_taken_test));
  EXPECT_FALSE(needs_finite_test);
  EXPECT_TRUE(needs_taken_test);

  // Generates code (unsimplified).
  range_.GenerateRange(increment_, phi, graph_, loop_preheader_, &lower, &upper);

  // Verify lower is 0+0.
  ASSERT_TRUE(lower != nullptr);
  ASSERT_TRUE(lower->IsAdd());
  ExpectInt(0, lower->InputAt(0));
  ExpectInt(0, lower->InputAt(1));

  // Verify upper is (V-1)+0.
  ASSERT_TRUE(upper != nullptr);
  ASSERT_TRUE(upper->IsAdd());
  ASSERT_TRUE(upper->InputAt(0)->IsSub());
  EXPECT_TRUE(upper->InputAt(0)->InputAt(0)->IsParameterValue());
  ExpectInt(1, upper->InputAt(0)->InputAt(1));
  ExpectInt(0, upper->InputAt(1));

  // Verify taken-test is 0<V.
  HInstruction* taken = range_.GenerateTakenTest(increment_, graph_, loop_preheader_);
  ASSERT_TRUE(taken != nullptr);
  ASSERT_TRUE(taken->IsLessThan());
  ExpectInt(0, taken->InputAt(0));
  EXPECT_TRUE(taken->InputAt(1)->IsParameterValue());

  // Replacement.
  range_.Replace(loop_header_->GetLastInstruction(), x_, y_);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(1), v1);
  ExpectEqual(Value(y_, 1, 0), v2);

  // Loop logic.
  int64_t tc = 0;
  EXPECT_TRUE(range_.IsFinite(loop_header_->GetLoopInformation(), &tc));
  EXPECT_EQ(0, tc);  // unknown
  HInstruction* offset = nullptr;
  EXPECT_TRUE(range_.IsUnitStride(phi, phi, graph_, &offset));
  ExpectInt(0, offset);
  HInstruction* tce = range_.GenerateTripCount(
      loop_header_->GetLoopInformation(), graph_, loop_preheader_);
  ASSERT_TRUE(tce != nullptr);
  EXPECT_TRUE(tce->IsSelect());  // guarded by taken-test
  ExpectInt(0, tce->InputAt(0));
  EXPECT_TRUE(tce->InputAt(1)->IsParameterValue());
  EXPECT_TRUE(tce->InputAt(2)->IsLessThan());
}

TEST_F(InductionVarRangeTest, SymbolicTripCountDown) {
  BuildLoop(1000, x_, -1);
  PerformInductionVarAnalysis();

  Value v1, v2;
  bool needs_finite_test = true;
  bool needs_taken_test = true;

  HInstruction* phi = condition_->InputAt(0);

  // In context of header: lower unknown.
  range_.GetInductionRange(condition_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(), v1);
  ExpectEqual(Value(1000), v2);

  // In context of loop-body: known.
  range_.GetInductionRange(increment_, phi, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(x_, 1, 1), v1);
  ExpectEqual(Value(1000), v2);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(x_, 1, 0), v1);
  ExpectEqual(Value(999), v2);

  HInstruction* lower = nullptr;
  HInstruction* upper = nullptr;

  // Can generate code in context of loop-body only.
  EXPECT_FALSE(range_.CanGenerateRange(condition_, phi, &needs_finite_test, &needs_taken_test));
  ASSERT_TRUE(range_.CanGenerateRange(increment_, phi, &needs_finite_test, &needs_taken_test));
  EXPECT_FALSE(needs_finite_test);
  EXPECT_TRUE(needs_taken_test);

  // Generates code (unsimplified).
  range_.GenerateRange(increment_, phi, graph_, loop_preheader_, &lower, &upper);

  // Verify lower is 1000-((1000-V)-1).
  ASSERT_TRUE(lower != nullptr);
  ASSERT_TRUE(lower->IsSub());
  ExpectInt(1000, lower->InputAt(0));
  lower = lower->InputAt(1);
  ASSERT_TRUE(lower->IsSub());
  ExpectInt(1, lower->InputAt(1));
  lower = lower->InputAt(0);
  ASSERT_TRUE(lower->IsSub());
  ExpectInt(1000, lower->InputAt(0));
  EXPECT_TRUE(lower->InputAt(1)->IsParameterValue());

  // Verify upper is 1000-0.
  ASSERT_TRUE(upper != nullptr);
  ASSERT_TRUE(upper->IsSub());
  ExpectInt(1000, upper->InputAt(0));
  ExpectInt(0, upper->InputAt(1));

  // Verify taken-test is 1000>V.
  HInstruction* taken = range_.GenerateTakenTest(increment_, graph_, loop_preheader_);
  ASSERT_TRUE(taken != nullptr);
  ASSERT_TRUE(taken->IsGreaterThan());
  ExpectInt(1000, taken->InputAt(0));
  EXPECT_TRUE(taken->InputAt(1)->IsParameterValue());

  // Replacement.
  range_.Replace(loop_header_->GetLastInstruction(), x_, y_);
  range_.GetInductionRange(increment_, increment_, x_, &v1, &v2, &needs_finite_test);
  EXPECT_FALSE(needs_finite_test);
  ExpectEqual(Value(y_, 1, 0), v1);
  ExpectEqual(Value(999), v2);

  // Loop logic.
  int64_t tc = 0;
  EXPECT_TRUE(range_.IsFinite(loop_header_->GetLoopInformation(), &tc));
  EXPECT_EQ(0, tc);  // unknown
  HInstruction* offset = nullptr;
  EXPECT_FALSE(range_.IsUnitStride(phi, phi, graph_, &offset));
  HInstruction* tce = range_.GenerateTripCount(
      loop_header_->GetLoopInformation(), graph_, loop_preheader_);
  ASSERT_TRUE(tce != nullptr);
  EXPECT_TRUE(tce->IsSelect());  // guarded by taken-test
  ExpectInt(0, tce->InputAt(0));
  EXPECT_TRUE(tce->InputAt(1)->IsSub());
  EXPECT_TRUE(tce->InputAt(2)->IsGreaterThan());
  tce = tce->InputAt(1);
  ExpectInt(1000, taken->InputAt(0));
  EXPECT_TRUE(taken->InputAt(1)->IsParameterValue());
}

}  // namespace art
