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

#include "memory_region.h"

#include "gtest/gtest.h"

#include "bit_memory_region.h"

namespace art {

TEST(MemoryRegion, LoadUnaligned) {
  const size_t n = 8;
  uint8_t data[n] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  MemoryRegion region(&data, n);

  ASSERT_EQ(0, region.LoadUnaligned<char>(0));
  ASSERT_EQ(1u
            + (2u << kBitsPerByte)
            + (3u << 2 * kBitsPerByte)
            + (4u << 3 * kBitsPerByte),
            region.LoadUnaligned<uint32_t>(1));
  ASSERT_EQ(5 + (6 << kBitsPerByte), region.LoadUnaligned<int16_t>(5));
  ASSERT_EQ(7u, region.LoadUnaligned<unsigned char>(7));
}

TEST(MemoryRegion, StoreUnaligned) {
  const size_t n = 8;
  uint8_t data[n] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  MemoryRegion region(&data, n);

  region.StoreUnaligned<unsigned char>(0u, 7);
  region.StoreUnaligned<int16_t>(1, 6 + (5 << kBitsPerByte));
  region.StoreUnaligned<uint32_t>(3,
                                  4u
                                  + (3u << kBitsPerByte)
                                  + (2u << 2 * kBitsPerByte)
                                  + (1u << 3 * kBitsPerByte));
  region.StoreUnaligned<char>(7, 0);

  uint8_t expected[n] = { 7, 6, 5, 4, 3, 2, 1, 0 };
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(expected[i], data[i]);
  }
}

TEST(MemoryRegion, TestBits) {
  const size_t n = 8;
  uint8_t data[n] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  MemoryRegion region(&data, n);
  uint32_t value = 0xDEADBEEF;
  // Try various offsets and lengths.
  for (size_t bit_offset = 0; bit_offset < 2 * kBitsPerByte; ++bit_offset) {
    for (size_t length = 0; length < 2 * kBitsPerByte; ++length) {
      const uint32_t length_mask = (1 << length) - 1;
      uint32_t masked_value = value & length_mask;
      BitMemoryRegion bmr(region, bit_offset, length);
      region.StoreBits(bit_offset, masked_value, length);
      EXPECT_EQ(region.LoadBits(bit_offset, length), masked_value);
      EXPECT_EQ(bmr.LoadBits(0, length), masked_value);
      // Check adjacent bits to make sure they were not incorrectly cleared.
      EXPECT_EQ(region.LoadBits(0, bit_offset), (1u << bit_offset) - 1);
      EXPECT_EQ(region.LoadBits(bit_offset + length, length), length_mask);
      region.StoreBits(bit_offset, length_mask, length);
      // Store with bit memory region.
      bmr.StoreBits(0, masked_value, length);
      EXPECT_EQ(bmr.LoadBits(0, length), masked_value);
      // Check adjacent bits to make sure they were not incorrectly cleared.
      EXPECT_EQ(region.LoadBits(0, bit_offset), (1u << bit_offset) - 1);
      EXPECT_EQ(region.LoadBits(bit_offset + length, length), length_mask);
      region.StoreBits(bit_offset, length_mask, length);
      // Flip the value to try different edge bit combinations.
      value = ~value;
    }
  }
}

}  // namespace art
