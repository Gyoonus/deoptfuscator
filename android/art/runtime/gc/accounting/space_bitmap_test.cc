/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "space_bitmap.h"

#include <stdint.h>
#include <memory>

#include "base/mutex.h"
#include "common_runtime_test.h"
#include "globals.h"
#include "space_bitmap-inl.h"

namespace art {
namespace gc {
namespace accounting {

class SpaceBitmapTest : public CommonRuntimeTest {};

TEST_F(SpaceBitmapTest, Init) {
  uint8_t* heap_begin = reinterpret_cast<uint8_t*>(0x10000000);
  size_t heap_capacity = 16 * MB;
  std::unique_ptr<ContinuousSpaceBitmap> space_bitmap(
      ContinuousSpaceBitmap::Create("test bitmap", heap_begin, heap_capacity));
  EXPECT_TRUE(space_bitmap.get() != nullptr);
}

class BitmapVerify {
 public:
  BitmapVerify(ContinuousSpaceBitmap* bitmap, const mirror::Object* begin,
               const mirror::Object* end)
    : bitmap_(bitmap),
      begin_(begin),
      end_(end) {}

  void operator()(const mirror::Object* obj) {
    EXPECT_TRUE(obj >= begin_);
    EXPECT_TRUE(obj <= end_);
    EXPECT_EQ(bitmap_->Test(obj), ((reinterpret_cast<uintptr_t>(obj) & 0xF) != 0));
  }

  ContinuousSpaceBitmap* const bitmap_;
  const mirror::Object* begin_;
  const mirror::Object* end_;
};

TEST_F(SpaceBitmapTest, ScanRange) {
  uint8_t* heap_begin = reinterpret_cast<uint8_t*>(0x10000000);
  size_t heap_capacity = 16 * MB;

  std::unique_ptr<ContinuousSpaceBitmap> space_bitmap(
      ContinuousSpaceBitmap::Create("test bitmap", heap_begin, heap_capacity));
  EXPECT_TRUE(space_bitmap != nullptr);

  // Set all the odd bits in the first BitsPerIntPtrT * 3 to one.
  for (size_t j = 0; j < kBitsPerIntPtrT * 3; ++j) {
    const mirror::Object* obj =
        reinterpret_cast<mirror::Object*>(heap_begin + j * kObjectAlignment);
    if (reinterpret_cast<uintptr_t>(obj) & 0xF) {
      space_bitmap->Set(obj);
    }
  }
  // Try every possible starting bit in the first word. Then for each starting bit, try each
  // possible length up to a maximum of `kBitsPerIntPtrT * 2 - 1` bits.
  // This handles all the cases, having runs which start and end on the same word, and different
  // words.
  for (size_t i = 0; i < static_cast<size_t>(kBitsPerIntPtrT); ++i) {
    mirror::Object* start =
        reinterpret_cast<mirror::Object*>(heap_begin + i * kObjectAlignment);
    for (size_t j = 0; j < static_cast<size_t>(kBitsPerIntPtrT * 2); ++j) {
      mirror::Object* end =
          reinterpret_cast<mirror::Object*>(heap_begin + (i + j) * kObjectAlignment);
      BitmapVerify(space_bitmap.get(), start, end);
    }
  }
}

TEST_F(SpaceBitmapTest, ClearRange) {
  uint8_t* heap_begin = reinterpret_cast<uint8_t*>(0x10000000);
  size_t heap_capacity = 16 * MB;

  std::unique_ptr<ContinuousSpaceBitmap> bitmap(
      ContinuousSpaceBitmap::Create("test bitmap", heap_begin, heap_capacity));
  EXPECT_TRUE(bitmap != nullptr);

  // Set all of the bits in the bitmap.
  for (size_t j = 0; j < heap_capacity; j += kObjectAlignment) {
    const mirror::Object* obj = reinterpret_cast<mirror::Object*>(heap_begin + j);
    bitmap->Set(obj);
  }

  std::vector<std::pair<uintptr_t, uintptr_t>> ranges = {
      {0, 10 * KB + kObjectAlignment},
      {kObjectAlignment, kObjectAlignment},
      {kObjectAlignment, 2 * kObjectAlignment},
      {kObjectAlignment, 5 * kObjectAlignment},
      {1 * KB + kObjectAlignment, 2 * KB + 5 * kObjectAlignment},
  };
  // Try clearing a few ranges.
  for (const std::pair<uintptr_t, uintptr_t>& range : ranges) {
    const mirror::Object* obj_begin = reinterpret_cast<mirror::Object*>(heap_begin + range.first);
    const mirror::Object* obj_end = reinterpret_cast<mirror::Object*>(heap_begin + range.second);
    bitmap->ClearRange(obj_begin, obj_end);
    // Boundaries should still be marked.
    for (uintptr_t i = 0; i < range.first; i += kObjectAlignment) {
      EXPECT_TRUE(bitmap->Test(reinterpret_cast<mirror::Object*>(heap_begin + i)));
    }
    for (uintptr_t i = range.second; i < range.second + kPageSize; i += kObjectAlignment) {
      EXPECT_TRUE(bitmap->Test(reinterpret_cast<mirror::Object*>(heap_begin + i)));
    }
    // Everything inside should be cleared.
    for (uintptr_t i = range.first; i < range.second; i += kObjectAlignment) {
      EXPECT_FALSE(bitmap->Test(reinterpret_cast<mirror::Object*>(heap_begin + i)));
      bitmap->Set(reinterpret_cast<mirror::Object*>(heap_begin + i));
    }
  }
}


class SimpleCounter {
 public:
  explicit SimpleCounter(size_t* counter) : count_(counter) {}

  void operator()(mirror::Object* obj ATTRIBUTE_UNUSED) const {
    (*count_)++;
  }

  size_t* const count_;
};

class RandGen {
 public:
  explicit RandGen(uint32_t seed) : val_(seed) {}

  uint32_t next() {
    val_ = val_ * 48271 % 2147483647 + 13;
    return val_;
  }

  uint32_t val_;
};

template <size_t kAlignment, typename TestFn>
static void RunTest(TestFn&& fn) NO_THREAD_SAFETY_ANALYSIS {
  uint8_t* heap_begin = reinterpret_cast<uint8_t*>(0x10000000);
  size_t heap_capacity = 16 * MB;

  // Seed with 0x1234 for reproducability.
  RandGen r(0x1234);

  for (int i = 0; i < 5 ; ++i) {
    std::unique_ptr<ContinuousSpaceBitmap> space_bitmap(
        ContinuousSpaceBitmap::Create("test bitmap", heap_begin, heap_capacity));

    for (int j = 0; j < 10000; ++j) {
      size_t offset = RoundDown(r.next() % heap_capacity, kAlignment);
      bool set = r.next() % 2 == 1;

      if (set) {
        space_bitmap->Set(reinterpret_cast<mirror::Object*>(heap_begin + offset));
      } else {
        space_bitmap->Clear(reinterpret_cast<mirror::Object*>(heap_begin + offset));
      }
    }

    for (int j = 0; j < 50; ++j) {
      const size_t offset = RoundDown(r.next() % heap_capacity, kAlignment);
      const size_t remain = heap_capacity - offset;
      const size_t end = offset + RoundDown(r.next() % (remain + 1), kAlignment);

      size_t manual = 0;
      for (uintptr_t k = offset; k < end; k += kAlignment) {
        if (space_bitmap->Test(reinterpret_cast<mirror::Object*>(heap_begin + k))) {
          manual++;
        }
      }

      uintptr_t range_begin = reinterpret_cast<uintptr_t>(heap_begin) + offset;
      uintptr_t range_end = reinterpret_cast<uintptr_t>(heap_begin) + end;

      fn(space_bitmap.get(), range_begin, range_end, manual);
    }
  }
}

template <size_t kAlignment>
static void RunTestCount() {
  auto count_test_fn = [](ContinuousSpaceBitmap* space_bitmap,
                          uintptr_t range_begin,
                          uintptr_t range_end,
                          size_t manual_count) {
    size_t count = 0;
    auto count_fn = [&count](mirror::Object* obj ATTRIBUTE_UNUSED) {
      count++;
    };
    space_bitmap->VisitMarkedRange(range_begin, range_end, count_fn);
    EXPECT_EQ(count, manual_count);
  };
  RunTest<kAlignment>(count_test_fn);
}

TEST_F(SpaceBitmapTest, VisitorObjectAlignment) {
  RunTestCount<kObjectAlignment>();
}

TEST_F(SpaceBitmapTest, VisitorPageAlignment) {
  RunTestCount<kPageSize>();
}

template <size_t kAlignment>
void RunTestOrder() {
  auto order_test_fn = [](ContinuousSpaceBitmap* space_bitmap,
                          uintptr_t range_begin,
                          uintptr_t range_end,
                          size_t manual_count)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_, Locks::mutator_lock_) {
    mirror::Object* last_ptr = nullptr;
    auto order_check = [&last_ptr](mirror::Object* obj) {
      EXPECT_LT(last_ptr, obj);
      last_ptr = obj;
    };

    // Test complete walk.
    space_bitmap->Walk(order_check);
    if (manual_count > 0) {
      EXPECT_NE(nullptr, last_ptr);
    }

    // Test range.
    last_ptr = nullptr;
    space_bitmap->VisitMarkedRange(range_begin, range_end, order_check);
    if (manual_count > 0) {
      EXPECT_NE(nullptr, last_ptr);
    }
  };
  RunTest<kAlignment>(order_test_fn);
}

TEST_F(SpaceBitmapTest, OrderObjectAlignment) {
  RunTestOrder<kObjectAlignment>();
}

TEST_F(SpaceBitmapTest, OrderPageAlignment) {
  RunTestOrder<kPageSize>();
}

}  // namespace accounting
}  // namespace gc
}  // namespace art
