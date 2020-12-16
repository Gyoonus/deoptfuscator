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
#include "nodes.h"
#include "parallel_move_resolver.h"

#include "gtest/gtest-typed-test.h"
#include "gtest/gtest.h"

namespace art {

constexpr int kScratchRegisterStartIndexForTest = 100;

static void DumpRegisterForTest(std::ostream& os, int reg) {
  if (reg >= kScratchRegisterStartIndexForTest) {
    os << "T" << reg - kScratchRegisterStartIndexForTest;
  } else {
    os << reg;
  }
}

static void DumpLocationForTest(std::ostream& os, Location location) {
  if (location.IsConstant()) {
    os << "C";
  } else if (location.IsPair()) {
    DumpRegisterForTest(os, location.low());
    os << ",";
    DumpRegisterForTest(os, location.high());
  } else if (location.IsRegister()) {
    DumpRegisterForTest(os, location.reg());
  } else if (location.IsStackSlot()) {
    os << location.GetStackIndex() << "(sp)";
  } else {
    DCHECK(location.IsDoubleStackSlot())<< location;
    os << "2x" << location.GetStackIndex() << "(sp)";
  }
}

class TestParallelMoveResolverWithSwap : public ParallelMoveResolverWithSwap {
 public:
  explicit TestParallelMoveResolverWithSwap(ArenaAllocator* allocator)
      : ParallelMoveResolverWithSwap(allocator) {}

  void EmitMove(size_t index) OVERRIDE {
    MoveOperands* move = moves_[index];
    if (!message_.str().empty()) {
      message_ << " ";
    }
    message_ << "(";
    DumpLocationForTest(message_, move->GetSource());
    message_ << " -> ";
    DumpLocationForTest(message_, move->GetDestination());
    message_ << ")";
  }

  void EmitSwap(size_t index) OVERRIDE {
    MoveOperands* move = moves_[index];
    if (!message_.str().empty()) {
      message_ << " ";
    }
    message_ << "(";
    DumpLocationForTest(message_, move->GetSource());
    message_ << " <-> ";
    DumpLocationForTest(message_, move->GetDestination());
    message_ << ")";
  }

  void SpillScratch(int reg ATTRIBUTE_UNUSED) OVERRIDE {}
  void RestoreScratch(int reg ATTRIBUTE_UNUSED) OVERRIDE {}

  std::string GetMessage() const {
    return  message_.str();
  }

 private:
  std::ostringstream message_;


  DISALLOW_COPY_AND_ASSIGN(TestParallelMoveResolverWithSwap);
};

class TestParallelMoveResolverNoSwap : public ParallelMoveResolverNoSwap {
 public:
  explicit TestParallelMoveResolverNoSwap(ArenaAllocator* allocator)
      : ParallelMoveResolverNoSwap(allocator), scratch_index_(kScratchRegisterStartIndexForTest) {}

  void PrepareForEmitNativeCode() OVERRIDE {
    scratch_index_ = kScratchRegisterStartIndexForTest;
  }

  void FinishEmitNativeCode() OVERRIDE {}

  Location AllocateScratchLocationFor(Location::Kind kind) OVERRIDE {
    if (kind == Location::kStackSlot || kind == Location::kFpuRegister ||
        kind == Location::kRegister) {
      kind = Location::kRegister;
    } else {
      // Allocate register pair for double stack slot which simulates 32-bit backend's behavior.
      kind = Location::kRegisterPair;
    }
    Location scratch = GetScratchLocation(kind);
    if (scratch.Equals(Location::NoLocation())) {
      AddScratchLocation(Location::RegisterLocation(scratch_index_));
      AddScratchLocation(Location::RegisterLocation(scratch_index_ + 1));
      AddScratchLocation(Location::RegisterPairLocation(scratch_index_, scratch_index_ + 1));
      scratch = (kind == Location::kRegister) ? Location::RegisterLocation(scratch_index_)
          : Location::RegisterPairLocation(scratch_index_, scratch_index_ + 1);
      scratch_index_ += 2;
    }
    return scratch;
  }

  void FreeScratchLocation(Location loc ATTRIBUTE_UNUSED) OVERRIDE {}

  void EmitMove(size_t index) OVERRIDE {
    MoveOperands* move = moves_[index];
    if (!message_.str().empty()) {
      message_ << " ";
    }
    message_ << "(";
    DumpLocationForTest(message_, move->GetSource());
    message_ << " -> ";
    DumpLocationForTest(message_, move->GetDestination());
    message_ << ")";
  }

  std::string GetMessage() const {
    return  message_.str();
  }

 private:
  std::ostringstream message_;

  int scratch_index_;

  DISALLOW_COPY_AND_ASSIGN(TestParallelMoveResolverNoSwap);
};

static HParallelMove* BuildParallelMove(ArenaAllocator* allocator,
                                        const size_t operands[][2],
                                        size_t number_of_moves) {
  HParallelMove* moves = new (allocator) HParallelMove(allocator);
  for (size_t i = 0; i < number_of_moves; ++i) {
    moves->AddMove(
        Location::RegisterLocation(operands[i][0]),
        Location::RegisterLocation(operands[i][1]),
        DataType::Type::kInt32,
        nullptr);
  }
  return moves;
}

template <typename T>
class ParallelMoveTest : public ::testing::Test {
 public:
  static const bool has_swap;
};

template<> const bool ParallelMoveTest<TestParallelMoveResolverWithSwap>::has_swap = true;
template<> const bool ParallelMoveTest<TestParallelMoveResolverNoSwap>::has_swap = false;

typedef ::testing::Types<TestParallelMoveResolverWithSwap, TestParallelMoveResolverNoSwap>
    ParallelMoveResolverTestTypes;

TYPED_TEST_CASE(ParallelMoveTest, ParallelMoveResolverTestTypes);


TYPED_TEST(ParallelMoveTest, Dependency) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 2}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(1 -> 2) (0 -> 1)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(1 -> 2) (0 -> 1)", resolver.GetMessage().c_str());
    }
  }

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 2}, {2, 3}, {1, 4}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(2 -> 3) (1 -> 2) (1 -> 4) (0 -> 1)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2 -> 3) (1 -> 2) (0 -> 1) (2 -> 4)", resolver.GetMessage().c_str());
    }
  }
}

TYPED_TEST(ParallelMoveTest, Cycle) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 0}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(1 <-> 0)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(1 -> T0) (0 -> 1) (T0 -> 0)", resolver.GetMessage().c_str());
    }
  }

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 2}, {1, 0}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(1 -> 2) (1 <-> 0)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(1 -> 2) (0 -> 1) (2 -> 0)", resolver.GetMessage().c_str());
    }
  }

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 0}, {0, 2}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0 -> 2) (1 <-> 0)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(0 -> 2) (1 -> 0) (2 -> 1)", resolver.GetMessage().c_str());
    }
  }

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(4 <-> 0) (3 <-> 4) (2 <-> 3) (1 <-> 2)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(4 -> T0) (3 -> 4) (2 -> 3) (1 -> 2) (0 -> 1) (T0 -> 0)",
          resolver.GetMessage().c_str());
    }
  }
}

TYPED_TEST(ParallelMoveTest, ConstantLast) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  TypeParam resolver(&allocator);
  HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
  moves->AddMove(
      Location::ConstantLocation(new (&allocator) HIntConstant(0)),
      Location::RegisterLocation(0),
      DataType::Type::kInt32,
      nullptr);
  moves->AddMove(
      Location::RegisterLocation(1),
      Location::RegisterLocation(2),
      DataType::Type::kInt32,
      nullptr);
  resolver.EmitNativeCode(moves);
  ASSERT_STREQ("(1 -> 2) (C -> 0)", resolver.GetMessage().c_str());
}

TYPED_TEST(ParallelMoveTest, Pairs) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(4),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    ASSERT_STREQ("(2 -> 4) (0,1 -> 2,3)", resolver.GetMessage().c_str());
  }

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(4),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    ASSERT_STREQ("(2 -> 4) (0,1 -> 2,3)", resolver.GetMessage().c_str());
  }

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(0),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2 -> T0) (0,1 -> 2,3) (T0 -> 0)", resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(7),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(7),
        Location::RegisterLocation(1),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3) (7 -> 1) (0 -> 7)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(0,1 -> T0,T1) (7 -> 1) (2 -> 7) (T0,T1 -> 2,3)",
          resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(7),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(7),
        Location::RegisterLocation(1),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3) (7 -> 1) (0 -> 7)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(0,1 -> T0,T1) (7 -> 1) (2 -> 7) (T0,T1 -> 2,3)",
          resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(7),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(7),
        Location::RegisterLocation(1),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3) (7 -> 1) (0 -> 7)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(7 -> T0) (2 -> 7) (0,1 -> 2,3) (T0 -> 1)", resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(2, 3),
        Location::RegisterPairLocation(0, 1),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(2,3 <-> 0,1)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2,3 -> T0,T1) (0,1 -> 2,3) (T0,T1 -> 0,1)", resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(2, 3),
        Location::RegisterPairLocation(0, 1),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(0,1 -> T0,T1) (2,3 -> 0,1) (T0,T1 -> 2,3)", resolver.GetMessage().c_str());
    }
  }
}

TYPED_TEST(ParallelMoveTest, MultiCycles) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    static constexpr size_t moves[][2] = {{0, 1}, {1, 0}, {2, 3}, {3, 2}};
    resolver.EmitNativeCode(BuildParallelMove(&allocator, moves, arraysize(moves)));
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(1 <-> 0) (3 <-> 2)",  resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(1 -> T0) (0 -> 1) (T0 -> 0) (3 -> T0) (2 -> 3) (T0 -> 2)",
          resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(0),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(3),
        Location::RegisterLocation(1),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2 -> T0) (3 -> T1) (0,1 -> 2,3) (T0 -> 0) (T1 -> 1)",
          resolver.GetMessage().c_str());
    }
  }
  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(2),
        Location::RegisterLocation(0),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(3),
        Location::RegisterLocation(1),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0,1 <-> 2,3)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(3 -> T0) (0,1 -> T2,T3) (T0 -> 1) (2 -> 0) (T2,T3 -> 2,3)",
          resolver.GetMessage().c_str());
    }
  }

  {
    // Test involving registers used in single context and pair context.
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(10),
        Location::RegisterLocation(5),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(4, 5),
        Location::DoubleStackSlot(32),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::DoubleStackSlot(32),
        Location::RegisterPairLocation(10, 11),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(2x32(sp) <-> 10,11) (4,5 <-> 2x32(sp)) (4 -> 5)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2x32(sp) -> T0,T1) (4,5 -> 2x32(sp)) (10 -> 5) (T0,T1 -> 10,11)",
          resolver.GetMessage().c_str());
    }
  }
}

// Test that we do 64bits moves before 32bits moves.
TYPED_TEST(ParallelMoveTest, CyclesWith64BitsMoves) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(0),
        Location::RegisterLocation(1),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(1),
        Location::StackSlot(48),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::StackSlot(48),
        Location::RegisterLocation(0),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(0 <-> 1) (48(sp) <-> 0)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(48(sp) -> T0) (1 -> 48(sp)) (0 -> 1) (T0 -> 0)",
          resolver.GetMessage().c_str());
    }
  }

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterPairLocation(0, 1),
        Location::RegisterPairLocation(2, 3),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(2, 3),
        Location::DoubleStackSlot(32),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::DoubleStackSlot(32),
        Location::RegisterPairLocation(0, 1),
        DataType::Type::kInt64,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(2x32(sp) <-> 0,1) (2,3 <-> 2x32(sp))", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2x32(sp) -> T0,T1) (2,3 -> 2x32(sp)) (0,1 -> 2,3) (T0,T1 -> 0,1)",
          resolver.GetMessage().c_str());
    }
  }
}

TYPED_TEST(ParallelMoveTest, CyclesWith64BitsMoves2) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  {
    TypeParam resolver(&allocator);
    HParallelMove* moves = new (&allocator) HParallelMove(&allocator);
    moves->AddMove(
        Location::RegisterLocation(0),
        Location::RegisterLocation(3),
        DataType::Type::kInt32,
        nullptr);
    moves->AddMove(
        Location::RegisterPairLocation(2, 3),
        Location::RegisterPairLocation(0, 1),
        DataType::Type::kInt64,
        nullptr);
    moves->AddMove(
        Location::RegisterLocation(7),
        Location::RegisterLocation(2),
        DataType::Type::kInt32,
        nullptr);
    resolver.EmitNativeCode(moves);
    if (TestFixture::has_swap) {
      ASSERT_STREQ("(2,3 <-> 0,1) (2 -> 3) (7 -> 2)", resolver.GetMessage().c_str());
    } else {
      ASSERT_STREQ("(2,3 -> T0,T1) (0 -> 3) (T0,T1 -> 0,1) (7 -> 2)",
          resolver.GetMessage().c_str());
    }
  }
}

}  // namespace art
