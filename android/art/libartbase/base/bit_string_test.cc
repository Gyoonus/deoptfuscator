/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "base/bit_string.h"

#include "gtest/gtest.h"
#include "android-base/logging.h"

namespace art {

constexpr size_t BitString::kBitSizeAtPosition[BitString::kCapacity];
constexpr size_t BitString::kCapacity;

};  // namespace art

using namespace art;  // NOLINT [build/namespaces] [5]

// These helper functions are only used by the test,
// so they are not in the main BitString class.
std::string Stringify(BitString bit_string) {
  std::stringstream ss;
  ss << bit_string;
  return ss.str();
}

BitStringChar MakeBitStringChar(size_t idx, size_t val) {
  return BitStringChar(val, BitString::MaybeGetBitLengthAtPosition(idx));
}

BitStringChar MakeBitStringChar(size_t val) {
  return BitStringChar(val, MinimumBitsToStore(val));
}

BitString MakeBitString(std::initializer_list<size_t> values = {}) {
  CHECK_GE(BitString::kCapacity, values.size());

  BitString bs{};

  size_t i = 0;
  for (size_t val : values) {
    bs.SetAt(i, MakeBitStringChar(i, val));
    ++i;
  }

  return bs;
}

template <typename T>
size_t AsUint(const T& value) {
  size_t uint_value = 0;
  memcpy(&uint_value, &value, sizeof(value));
  return uint_value;
}

// Make max bitstring, e.g. BitString[4095,15,2047] for {12,4,11}
template <size_t kCount = BitString::kCapacity>
BitString MakeBitStringMax() {
  BitString bs{};

  for (size_t i = 0; i < kCount; ++i) {
    bs.SetAt(i,
             MakeBitStringChar(i, MaxInt<BitStringChar::StorageType>(BitString::kBitSizeAtPosition[i])));
  }

  return bs;
}

BitString SetBitStringCharAt(BitString bit_string, size_t i, size_t val) {
  BitString bs = bit_string;
  bs.SetAt(i, MakeBitStringChar(i, val));
  return bs;
}

#define EXPECT_BITSTRING_STR(expected_str, actual_value)                       \
  EXPECT_STREQ((expected_str), Stringify((actual_value)).c_str())

// TODO: Consider removing this test, it's kind of replicating the logic in GetLsbForPosition().
TEST(InstanceOfBitString, GetLsbForPosition) {
  ASSERT_LE(3u, BitString::kCapacity);
  // Test will fail if kCapacity is not at least 3. Update it.
  EXPECT_EQ(0u, BitString::GetLsbForPosition(0u));
  EXPECT_EQ(BitString::kBitSizeAtPosition[0u], BitString::GetLsbForPosition(1u));
  EXPECT_EQ(BitString::kBitSizeAtPosition[0u] + BitString::kBitSizeAtPosition[1u],
            BitString::GetLsbForPosition(2u));
}

TEST(InstanceOfBitString, ToString) {
  EXPECT_BITSTRING_STR("BitString[]", MakeBitString({0}));
  EXPECT_BITSTRING_STR("BitString[1]", MakeBitString({1}));
  EXPECT_BITSTRING_STR("BitString[1,2,3]", MakeBitString({1, 2, 3}));
}

TEST(InstanceOfBitString, ReadWrite) {
  BitString bs = MakeBitString();

  // Update tests if changing the capacity.
  ASSERT_EQ(BitString::kCapacity, 3u);

  EXPECT_BITSTRING_STR("BitString[]", bs);
  bs = SetBitStringCharAt(bs, /*i*/0, /*val*/1u);
  EXPECT_BITSTRING_STR("BitString[1]", bs);
  bs = SetBitStringCharAt(bs, /*i*/1, /*val*/2u);
  EXPECT_BITSTRING_STR("BitString[1,2]", bs);
  bs = SetBitStringCharAt(bs, /*i*/2, /*val*/3u);
  EXPECT_BITSTRING_STR("BitString[1,2,3]", bs);

  // There should be at least "kCapacity" # of checks here, 1 for each unique position.
  EXPECT_EQ(MakeBitStringChar(/*idx*/0, /*val*/1u), bs[0]);
  EXPECT_EQ(MakeBitStringChar(/*idx*/1, /*val*/2u), bs[1]);
  EXPECT_EQ(MakeBitStringChar(/*idx*/2, /*val*/3u), bs[2]);

  // Each maximal value should be tested here for each position.
  uint32_t max_bitstring_ints[] = {
      MaxInt<uint32_t>(12),
      MaxInt<uint32_t>(4),
      MaxInt<uint32_t>(11),
  };

  // Update tests if changing the tuning values above.
  for (size_t i = 0; i < arraysize(max_bitstring_ints); ++i) {
    ASSERT_EQ(MinimumBitsToStore(max_bitstring_ints[i]), BitString::kBitSizeAtPosition[i]) << i;
  }

  BitString bs_max = MakeBitStringMax();

  for (size_t i = 0; i < arraysize(max_bitstring_ints); ++i) {
    ASSERT_EQ(max_bitstring_ints[i], static_cast<uint32_t>(bs_max[i])) << i;
  }

  EXPECT_EQ(MaskLeastSignificant(BitString::GetBitLengthTotalAtPosition(BitString::kCapacity)),
            AsUint(MakeBitStringMax()));
}

template <size_t kPos>
constexpr auto MaxForPos() {
    return MaxInt<BitString::StorageType>(BitString::kBitSizeAtPosition[kPos]);
}

TEST(InstanceOfBitString, MemoryRepresentation) {
  // Verify that the lower positions are stored in less significant bits.
  BitString bs = MakeBitString({MaxForPos<0>(), MaxForPos<1>()});
  BitString::StorageType as_int = static_cast<BitString::StorageType>(bs);

  // Below tests assumes the capacity is at least 3.
  ASSERT_LE(3u, BitString::kCapacity);
  EXPECT_EQ((MaxForPos<0>() << 0) | (MaxForPos<1>() << BitString::kBitSizeAtPosition[0]),
            as_int);
}

TEST(InstanceOfBitString, Truncate) {
  EXPECT_BITSTRING_STR("BitString[]", MakeBitString({1, 2, 3}).Truncate(0));
  EXPECT_BITSTRING_STR("BitString[1]", MakeBitString({1, 2, 3}).Truncate(1));
  EXPECT_BITSTRING_STR("BitString[1,2]", MakeBitString({1, 2, 3}).Truncate(2));
  EXPECT_BITSTRING_STR("BitString[1,2,3]", MakeBitString({1, 2, 3}).Truncate(3));
}
