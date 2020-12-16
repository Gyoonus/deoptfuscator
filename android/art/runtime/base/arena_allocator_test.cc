/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "base/arena_allocator-inl.h"
#include "base/arena_bit_vector.h"
#include "base/memory_tool.h"
#include "gtest/gtest.h"

namespace art {

class ArenaAllocatorTest : public testing::Test {
 protected:
  size_t NumberOfArenas(ArenaAllocator* allocator) {
    size_t result = 0u;
    for (Arena* a = allocator->arena_head_; a != nullptr; a = a->next_) {
      ++result;
    }
    return result;
  }
};

TEST_F(ArenaAllocatorTest, Test) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  ArenaBitVector bv(&allocator, 10, true);
  bv.SetBit(5);
  EXPECT_EQ(1U, bv.GetStorageSize());
  bv.SetBit(35);
  EXPECT_EQ(2U, bv.GetStorageSize());
}

TEST_F(ArenaAllocatorTest, MakeDefined) {
  // Regression test to make sure we mark the allocated area defined.
  ArenaPool pool;
  static constexpr size_t kSmallArraySize = 10;
  static constexpr size_t kLargeArraySize = 50;
  uint32_t* small_array;
  {
    // Allocate a small array from an arena and release it.
    ArenaAllocator allocator(&pool);
    small_array = allocator.AllocArray<uint32_t>(kSmallArraySize);
    ASSERT_EQ(0u, small_array[kSmallArraySize - 1u]);
  }
  {
    // Reuse the previous arena and allocate more than previous allocation including red zone.
    ArenaAllocator allocator(&pool);
    uint32_t* large_array = allocator.AllocArray<uint32_t>(kLargeArraySize);
    ASSERT_EQ(0u, large_array[kLargeArraySize - 1u]);
    // Verify that the allocation was made on the same arena.
    ASSERT_EQ(small_array, large_array);
  }
}

TEST_F(ArenaAllocatorTest, LargeAllocations) {
  if (arena_allocator::kArenaAllocatorPreciseTracking) {
    printf("WARNING: TEST DISABLED FOR precise arena tracking\n");
    return;
  }

  {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    // Note: Leaving some space for memory tool red zones.
    void* alloc1 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 5 / 8);
    void* alloc2 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 2 / 8);
    ASSERT_NE(alloc1, alloc2);
    ASSERT_EQ(1u, NumberOfArenas(&allocator));
  }
  {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    void* alloc1 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 13 / 16);
    void* alloc2 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 11 / 16);
    ASSERT_NE(alloc1, alloc2);
    ASSERT_EQ(2u, NumberOfArenas(&allocator));
    void* alloc3 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 7 / 16);
    ASSERT_NE(alloc1, alloc3);
    ASSERT_NE(alloc2, alloc3);
    ASSERT_EQ(3u, NumberOfArenas(&allocator));
  }
  {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    void* alloc1 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 13 / 16);
    void* alloc2 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 9 / 16);
    ASSERT_NE(alloc1, alloc2);
    ASSERT_EQ(2u, NumberOfArenas(&allocator));
    // Note: Leaving some space for memory tool red zones.
    void* alloc3 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 5 / 16);
    ASSERT_NE(alloc1, alloc3);
    ASSERT_NE(alloc2, alloc3);
    ASSERT_EQ(2u, NumberOfArenas(&allocator));
  }
  {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    void* alloc1 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 9 / 16);
    void* alloc2 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 13 / 16);
    ASSERT_NE(alloc1, alloc2);
    ASSERT_EQ(2u, NumberOfArenas(&allocator));
    // Note: Leaving some space for memory tool red zones.
    void* alloc3 = allocator.Alloc(arena_allocator::kArenaDefaultSize * 5 / 16);
    ASSERT_NE(alloc1, alloc3);
    ASSERT_NE(alloc2, alloc3);
    ASSERT_EQ(2u, NumberOfArenas(&allocator));
  }
  {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    // Note: Leaving some space for memory tool red zones.
    for (size_t i = 0; i != 15; ++i) {
      // Allocate 15 times from the same arena.
      allocator.Alloc(arena_allocator::kArenaDefaultSize * 1 / 16);
      ASSERT_EQ(i + 1u, NumberOfArenas(&allocator));
      // Allocate a separate arena.
      allocator.Alloc(arena_allocator::kArenaDefaultSize * 17 / 16);
      ASSERT_EQ(i + 2u, NumberOfArenas(&allocator));
    }
  }
}

TEST_F(ArenaAllocatorTest, AllocAlignment) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);
  for (size_t iterations = 0; iterations <= 10; ++iterations) {
    for (size_t size = 1; size <= ArenaAllocator::kAlignment + 1; ++size) {
      void* allocation = allocator.Alloc(size);
      EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(allocation))
          << reinterpret_cast<uintptr_t>(allocation);
    }
  }
}

TEST_F(ArenaAllocatorTest, ReallocReuse) {
  // Realloc does not reuse arenas when running under sanitization. So we cannot do those
  if (RUNNING_ON_MEMORY_TOOL != 0) {
    printf("WARNING: TEST DISABLED FOR MEMORY_TOOL\n");
    return;
  }

  {
    // Case 1: small aligned allocation, aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2;
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = ArenaAllocator::kAlignment * 3;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_EQ(original_allocation, realloc_allocation);
  }

  {
    // Case 2: small aligned allocation, non-aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2;
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_EQ(original_allocation, realloc_allocation);
  }

  {
    // Case 3: small non-aligned allocation, aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = ArenaAllocator::kAlignment * 4;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_EQ(original_allocation, realloc_allocation);
  }

  {
    // Case 4: small non-aligned allocation, aligned non-extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = ArenaAllocator::kAlignment * 3;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_EQ(original_allocation, realloc_allocation);
  }

  // The next part is brittle, as the default size for an arena is variable, and we don't know about
  // sanitization.

  {
    // Case 5: large allocation, aligned extend into next arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = arena_allocator::kArenaDefaultSize -
        ArenaAllocator::kAlignment * 5;
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = arena_allocator::kArenaDefaultSize + ArenaAllocator::kAlignment * 2;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_NE(original_allocation, realloc_allocation);
  }

  {
    // Case 6: large allocation, non-aligned extend into next arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = arena_allocator::kArenaDefaultSize -
        ArenaAllocator::kAlignment * 4 -
        ArenaAllocator::kAlignment / 2;
    void* original_allocation = allocator.Alloc(original_size);

    const size_t new_size = arena_allocator::kArenaDefaultSize +
        ArenaAllocator::kAlignment * 2 +
        ArenaAllocator::kAlignment / 2;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_NE(original_allocation, realloc_allocation);
  }
}

TEST_F(ArenaAllocatorTest, ReallocAlignment) {
  {
    // Case 1: small aligned allocation, aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2;
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = ArenaAllocator::kAlignment * 3;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }

  {
    // Case 2: small aligned allocation, non-aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2;
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }

  {
    // Case 3: small non-aligned allocation, aligned extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = ArenaAllocator::kAlignment * 4;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }

  {
    // Case 4: small non-aligned allocation, aligned non-extend inside arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = ArenaAllocator::kAlignment * 2 + (ArenaAllocator::kAlignment / 2);
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = ArenaAllocator::kAlignment * 3;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }

  // The next part is brittle, as the default size for an arena is variable, and we don't know about
  // sanitization.

  {
    // Case 5: large allocation, aligned extend into next arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = arena_allocator::kArenaDefaultSize -
        ArenaAllocator::kAlignment * 5;
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = arena_allocator::kArenaDefaultSize + ArenaAllocator::kAlignment * 2;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }

  {
    // Case 6: large allocation, non-aligned extend into next arena.
    ArenaPool pool;
    ArenaAllocator allocator(&pool);

    const size_t original_size = arena_allocator::kArenaDefaultSize -
        ArenaAllocator::kAlignment * 4 -
        ArenaAllocator::kAlignment / 2;
    void* original_allocation = allocator.Alloc(original_size);
    ASSERT_TRUE(IsAligned<ArenaAllocator::kAlignment>(original_allocation));

    const size_t new_size = arena_allocator::kArenaDefaultSize +
        ArenaAllocator::kAlignment * 2 +
        ArenaAllocator::kAlignment / 2;
    void* realloc_allocation = allocator.Realloc(original_allocation, original_size, new_size);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(realloc_allocation));

    void* after_alloc = allocator.Alloc(1);
    EXPECT_TRUE(IsAligned<ArenaAllocator::kAlignment>(after_alloc));
  }
}


}  // namespace art
