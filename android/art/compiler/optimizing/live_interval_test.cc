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
#include "optimizing_unit_test.h"
#include "ssa_liveness_analysis.h"

#include "gtest/gtest.h"

namespace art {

TEST(LiveInterval, GetStart) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    static constexpr size_t ranges[][2] = {{0, 42}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_EQ(0u, interval->GetStart());
  }

  {
    static constexpr size_t ranges[][2] = {{4, 12}, {14, 16}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_EQ(4u, interval->GetStart());
  }
}

TEST(LiveInterval, IsDeadAt) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    static constexpr size_t ranges[][2] = {{0, 42}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_TRUE(interval->IsDeadAt(42));
    ASSERT_TRUE(interval->IsDeadAt(43));
    ASSERT_FALSE(interval->IsDeadAt(41));
    ASSERT_FALSE(interval->IsDeadAt(0));
    ASSERT_FALSE(interval->IsDeadAt(22));
  }

  {
    static constexpr size_t ranges[][2] = {{4, 12}, {14, 16}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_TRUE(interval->IsDeadAt(16));
    ASSERT_TRUE(interval->IsDeadAt(32));
    ASSERT_FALSE(interval->IsDeadAt(0));
    ASSERT_FALSE(interval->IsDeadAt(4));
    ASSERT_FALSE(interval->IsDeadAt(12));
    ASSERT_FALSE(interval->IsDeadAt(13));
    ASSERT_FALSE(interval->IsDeadAt(14));
    ASSERT_FALSE(interval->IsDeadAt(15));
  }
}

TEST(LiveInterval, Covers) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    static constexpr size_t ranges[][2] = {{0, 42}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_TRUE(interval->Covers(0));
    ASSERT_TRUE(interval->Covers(4));
    ASSERT_TRUE(interval->Covers(41));
    ASSERT_FALSE(interval->Covers(42));
    ASSERT_FALSE(interval->Covers(54));
  }

  {
    static constexpr size_t ranges[][2] = {{4, 12}, {14, 16}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    ASSERT_FALSE(interval->Covers(0));
    ASSERT_TRUE(interval->Covers(4));
    ASSERT_TRUE(interval->Covers(11));
    ASSERT_FALSE(interval->Covers(12));
    ASSERT_FALSE(interval->Covers(13));
    ASSERT_TRUE(interval->Covers(14));
    ASSERT_TRUE(interval->Covers(15));
    ASSERT_FALSE(interval->Covers(16));
  }
}

TEST(LiveInterval, FirstIntersectionWith) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    static constexpr size_t ranges1[][2] = {{0, 4}, {8, 10}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{5, 6}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(kNoLifetime, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 4}, {8, 10}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{5, 42}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(8u, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 4}, {8, 10}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{5, 6}, {7, 8}, {11, 12}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(kNoLifetime, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 4}, {8, 10}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{5, 6}, {7, 8}, {9, 10}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(9u, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 1}, {2, 7}, {8, 10}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{1, 2}, {6, 7}, {9, 10}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(6u, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 1}, {2, 8}, {55, 58}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{1, 2}, {11, 42}, {43, 48}, {54, 56}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(55u, interval1->FirstIntersectionWith(interval2));
  }

  {
    static constexpr size_t ranges1[][2] = {{0, 1}, {2, 8}, {15, 18}, {27, 32}, {41, 53}, {54, 60}};
    LiveInterval* interval1 = BuildInterval(ranges1, arraysize(ranges1), allocator);
    static constexpr size_t ranges2[][2] = {{1, 2}, {11, 12}, {19, 25}, {34, 42}, {52, 60}};
    LiveInterval* interval2 = BuildInterval(ranges2, arraysize(ranges2), allocator);

    ASSERT_EQ(41u, interval1->FirstIntersectionWith(interval2));
  }
}

static bool RangesEquals(LiveInterval* interval,
                         const size_t expected[][2],
                         size_t number_of_expected_ranges) {
  LiveRange* current = interval->GetFirstRange();

  size_t i = 0;
  for (;
       i < number_of_expected_ranges && current != nullptr;
       ++i, current = current->GetNext()) {
    if (expected[i][0] != current->GetStart()) {
      return false;
    }
    if (expected[i][1] != current->GetEnd()) {
      return false;
    }
  }

  if (current != nullptr || i != number_of_expected_ranges) {
    return false;
  }

  return true;
}

TEST(LiveInterval, SplitAt) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    // Test within one range.
    static constexpr size_t ranges[][2] = {{0, 4}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(1);
    static constexpr size_t expected[][2] = {{0, 1}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{1, 4}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test just before the end of one range.
    static constexpr size_t ranges[][2] = {{0, 4}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(3);
    static constexpr size_t expected[][2] = {{0, 3}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{3, 4}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test withing the first range.
    static constexpr size_t ranges[][2] = {{0, 4}, {8, 12}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(1);
    static constexpr size_t expected[][2] = {{0, 1}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{1, 4}, {8, 12}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test in a hole.
    static constexpr size_t ranges[][2] = {{0, 4}, {8, 12}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(5);
    static constexpr size_t expected[][2] = {{0, 4}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{8, 12}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test withing the second range.
    static constexpr size_t ranges[][2] = {{0, 4}, {8, 12}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(9);
    static constexpr size_t expected[][2] = {{0, 4}, {8, 9}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{9, 12}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test at the beginning of the second range.
    static constexpr size_t ranges[][2] = {{0, 4}, {6, 10}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(6);
    static constexpr size_t expected[][2] = {{0, 4}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{6, 10}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test at the end of the first range.
    static constexpr size_t ranges[][2] = {{0, 4}, {6, 10}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(4);
    static constexpr size_t expected[][2] = {{0, 4}};
    ASSERT_TRUE(RangesEquals(interval, expected, arraysize(expected)));
    static constexpr size_t expected_split[][2] = {{6, 10}};
    ASSERT_TRUE(RangesEquals(split, expected_split, arraysize(expected_split)));
  }

  {
    // Test that we get null if we split at a position where the interval is dead.
    static constexpr size_t ranges[][2] = {{0, 4}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    LiveInterval* split = interval->SplitAt(5);
    ASSERT_TRUE(split == nullptr);
    ASSERT_TRUE(RangesEquals(interval, ranges, arraysize(ranges)));
  }
}

TEST(LiveInterval, AddLoopRange) {
  ArenaPoolAndAllocator pool;
  ScopedArenaAllocator* allocator = pool.GetScopedAllocator();

  {
    // Test when only used in a loop.
    static constexpr size_t ranges[][2] = {{0, 4}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    interval->AddLoopRange(0, 8);
    LiveRange* range = interval->GetFirstRange();
    ASSERT_TRUE(range->GetNext() == nullptr);
    ASSERT_EQ(range->GetStart(), 0u);
    ASSERT_EQ(range->GetEnd(), 8u);
  }

  {
    // Test when only used in a loop.
    static constexpr size_t ranges[][2] = {{2, 4}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    interval->AddLoopRange(0, 8);
    LiveRange* range = interval->GetFirstRange();
    ASSERT_TRUE(range->GetNext() == nullptr);
    ASSERT_EQ(range->GetStart(), 0u);
    ASSERT_EQ(range->GetEnd(), 8u);
  }

  {
    // Test when used just after the loop.
    static constexpr size_t ranges[][2] = {{2, 4}, {8, 10}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    interval->AddLoopRange(0, 8);
    LiveRange* range = interval->GetFirstRange();
    ASSERT_TRUE(range->GetNext() == nullptr);
    ASSERT_EQ(range->GetStart(), 0u);
    ASSERT_EQ(range->GetEnd(), 10u);
  }

  {
    // Test when use after the loop is after a lifetime hole.
    static constexpr size_t ranges[][2] = {{2, 4}, {10, 12}};
    LiveInterval* interval = BuildInterval(ranges, arraysize(ranges), allocator);
    interval->AddLoopRange(0, 8);
    LiveRange* range = interval->GetFirstRange();
    ASSERT_EQ(range->GetStart(), 0u);
    ASSERT_EQ(range->GetEnd(), 8u);
    range = range->GetNext();
    ASSERT_EQ(range->GetStart(), 10u);
    ASSERT_EQ(range->GetEnd(), 12u);
  }
}

}  // namespace art
