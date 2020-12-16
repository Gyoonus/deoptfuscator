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

#include "bit_struct.h"

#include "gtest/gtest.h"

namespace art {

// A copy of detail::ValidateBitStructSize that uses EXPECT for a more
// human-readable message.
template <typename T>
static constexpr bool ValidateBitStructSize(const char* name) {
  const size_t kBitStructSizeOf = BitStructSizeOf<T>();
  const size_t kExpectedSize = (BitStructSizeOf<T>() < kBitsPerByte)
                                   ? kBitsPerByte
                                   : RoundUpToPowerOfTwo(kBitStructSizeOf);

  // Ensure no extra fields were added in between START/END.
  const size_t kActualSize = sizeof(T) * kBitsPerByte;
  EXPECT_EQ(kExpectedSize, kActualSize) << name;
  return true;
}

#define VALIDATE_BITSTRUCT_SIZE(type) ValidateBitStructSize<type>(#type)

TEST(BitStructs, MinimumType) {
  EXPECT_EQ(1u, sizeof(typename detail::MinimumTypeUnsignedHelper<1>::type));
  EXPECT_EQ(1u, sizeof(typename detail::MinimumTypeUnsignedHelper<2>::type));
  EXPECT_EQ(1u, sizeof(typename detail::MinimumTypeUnsignedHelper<3>::type));
  EXPECT_EQ(1u, sizeof(typename detail::MinimumTypeUnsignedHelper<8>::type));
  EXPECT_EQ(2u, sizeof(typename detail::MinimumTypeUnsignedHelper<9>::type));
  EXPECT_EQ(2u, sizeof(typename detail::MinimumTypeUnsignedHelper<10>::type));
  EXPECT_EQ(2u, sizeof(typename detail::MinimumTypeUnsignedHelper<15>::type));
  EXPECT_EQ(2u, sizeof(typename detail::MinimumTypeUnsignedHelper<16>::type));
  EXPECT_EQ(4u, sizeof(typename detail::MinimumTypeUnsignedHelper<17>::type));
  EXPECT_EQ(4u, sizeof(typename detail::MinimumTypeUnsignedHelper<32>::type));
  EXPECT_EQ(8u, sizeof(typename detail::MinimumTypeUnsignedHelper<33>::type));
  EXPECT_EQ(8u, sizeof(typename detail::MinimumTypeUnsignedHelper<64>::type));
}

template <typename T>
size_t AsUint(const T& value) {
  size_t uint_value = 0;
  memcpy(&uint_value, &value, sizeof(value));
  return uint_value;
}

struct CustomBitStruct {
  CustomBitStruct() = default;
  explicit CustomBitStruct(int8_t data) : data(data) {}

  static constexpr size_t BitStructSizeOf() {
    return 4;
  }

  int8_t data;
};

TEST(BitStructs, Custom) {
  CustomBitStruct expected(0b1111);

  BitStructField<CustomBitStruct, /*lsb*/4, /*width*/4> f{};

  EXPECT_EQ(1u, sizeof(f));

  f = CustomBitStruct(0b1111);

  CustomBitStruct read_out = f;
  EXPECT_EQ(read_out.data, 0b1111);

  EXPECT_EQ(AsUint(f), 0b11110000u);
}

BITSTRUCT_DEFINE_START(TestTwoCustom, /* size */ 8)
  BitStructField<CustomBitStruct, /*lsb*/0, /*width*/4> f4_a;
  BitStructField<CustomBitStruct, /*lsb*/4, /*width*/4> f4_b;
BITSTRUCT_DEFINE_END(TestTwoCustom);

TEST(BitStructs, TwoCustom) {
  EXPECT_EQ(sizeof(TestTwoCustom), 1u);

  VALIDATE_BITSTRUCT_SIZE(TestTwoCustom);

  TestTwoCustom cst{};

  // Test the write to most-significant field doesn't clobber least-significant.
  cst.f4_a = CustomBitStruct(0b0110);
  cst.f4_b = CustomBitStruct(0b0101);

  int8_t read_out = static_cast<CustomBitStruct>(cst.f4_a).data;
  int8_t read_out_b = static_cast<CustomBitStruct>(cst.f4_b).data;

  EXPECT_EQ(0b0110, static_cast<int>(read_out));
  EXPECT_EQ(0b0101, static_cast<int>(read_out_b));

  EXPECT_EQ(AsUint(cst), 0b01010110u);

  // Test write to least-significant field doesn't clobber most-significant.
  cst.f4_a = CustomBitStruct(0);

  read_out = static_cast<CustomBitStruct>(cst.f4_a).data;
  read_out_b = static_cast<CustomBitStruct>(cst.f4_b).data;

  EXPECT_EQ(0b0, static_cast<int>(read_out));
  EXPECT_EQ(0b0101, static_cast<int>(read_out_b));

  EXPECT_EQ(AsUint(cst), 0b01010000u);
}

TEST(BitStructs, Number) {
  BitStructNumber<uint16_t, /*lsb*/4, /*width*/4> bsn{};
  EXPECT_EQ(2u, sizeof(bsn));

  bsn = 0b1111;

  uint32_t read_out = static_cast<uint32_t>(bsn);
  uint32_t read_out_impl = bsn;

  EXPECT_EQ(read_out, read_out_impl);
  EXPECT_EQ(read_out, 0b1111u);
  EXPECT_EQ(AsUint(bsn), 0b11110000u);
}

BITSTRUCT_DEFINE_START(TestBitStruct, /* size */ 8)
  BitStructInt</*lsb*/0, /*width*/3> i3;
  BitStructUint</*lsb*/3, /*width*/4> u4;

  BitStructUint</*lsb*/0, /*width*/7> alias_all;
BITSTRUCT_DEFINE_END(TestBitStruct);

TEST(BitStructs, Test1) {
  {
    // Check minimal size selection is correct.
    BitStructInt</*lsb*/0, /*width*/3> i3;
    BitStructUint</*lsb*/3, /*width*/4> u4;

    BitStructUint</*lsb*/0, /*width*/7> alias_all;

    EXPECT_EQ(1u, sizeof(i3));
    EXPECT_EQ(1u, sizeof(u4));
    EXPECT_EQ(1u, sizeof(alias_all));
  }
  TestBitStruct tst{};

  // Check minimal size selection is correct.
  EXPECT_EQ(1u, sizeof(TestBitStruct));
  EXPECT_EQ(1u, sizeof(tst._));
  EXPECT_EQ(1u, sizeof(tst.i3));
  EXPECT_EQ(1u, sizeof(tst.u4));
  EXPECT_EQ(1u, sizeof(tst.alias_all));

  // Check operator assignment.
  tst.i3 = -1;
  tst.u4 = 0b1010;

  // Check implicit operator conversion.
  int8_t read_i3 = tst.i3;
  uint8_t read_u4 = tst.u4;

  // Ensure read-out values were correct.
  EXPECT_EQ(static_cast<int8_t>(-1), read_i3);
  EXPECT_EQ(0b1010, read_u4);

  // Ensure aliasing is working.
  EXPECT_EQ(0b1010111, static_cast<uint8_t>(tst.alias_all));

  // Ensure the bit pattern is correct.
  EXPECT_EQ(0b1010111u, AsUint(tst));

  // Math operator checks
  {
    // In-place
    ++tst.u4;
    EXPECT_EQ(static_cast<uint8_t>(0b1011), static_cast<uint8_t>(tst.u4));
    --tst.u4;
    EXPECT_EQ(static_cast<uint8_t>(0b1010), static_cast<uint8_t>(tst.u4));

    // Copy
    uint8_t read_and_convert = tst.u4++;
    EXPECT_EQ(static_cast<uint8_t>(0b1011), read_and_convert);
    EXPECT_EQ(static_cast<uint8_t>(0b1010), static_cast<uint8_t>(tst.u4));
    read_and_convert = tst.u4--;
    EXPECT_EQ(static_cast<uint8_t>(0b1001), read_and_convert);
    EXPECT_EQ(static_cast<uint8_t>(0b1010), static_cast<uint8_t>(tst.u4));

    // Check boolean operator conversion.
    tst.u4 = 0b1010;
    EXPECT_TRUE(static_cast<bool>(tst.u4));
    bool succ = tst.u4 ? true : false;
    EXPECT_TRUE(succ);

    tst.u4 = 0;
    EXPECT_FALSE(static_cast<bool>(tst.u4));

/*
    // Disabled: Overflow is caught by the BitFieldInsert DCHECKs.
    // Check overflow for uint.
    tst.u4 = 0b1111;
    ++tst.u4;
    EXPECT_EQ(static_cast<uint8_t>(0), static_cast<uint8_t>(tst.u4));
*/
  }
}

BITSTRUCT_DEFINE_START(MixedSizeBitStruct, /* size */ 32)
  BitStructUint</*lsb*/0, /*width*/3> u3;
  BitStructUint</*lsb*/3, /*width*/10> u10;
  BitStructUint</*lsb*/13, /*width*/19> u19;

  BitStructUint</*lsb*/0, /*width*/32> alias_all;
BITSTRUCT_DEFINE_END(MixedSizeBitStruct);

// static_assert(sizeof(MixedSizeBitStruct) == sizeof(uint32_t), "TestBitStructs#MixedSize");

TEST(BitStructs, Mixed) {
  EXPECT_EQ(4u, sizeof(MixedSizeBitStruct));

  MixedSizeBitStruct tst{};

  // Check operator assignment.
  tst.u3 = 0b111u;
  tst.u10 = 0b1111010100u;
  tst.u19 = 0b1010101010101010101u;

  // Check implicit operator conversion.
  uint8_t read_u3 = tst.u3;
  uint16_t read_u10 = tst.u10;
  uint32_t read_u19 = tst.u19;

  // Ensure read-out values were correct.
  EXPECT_EQ(0b111u, read_u3);
  EXPECT_EQ(0b1111010100u, read_u10);
  EXPECT_EQ(0b1010101010101010101u, read_u19);

  uint32_t read_all = tst.alias_all;

  // Ensure aliasing is working.
  EXPECT_EQ(0b10101010101010101011111010100111u, read_all);

  // Ensure the bit pattern is correct.
  EXPECT_EQ(0b10101010101010101011111010100111u, AsUint(tst));
}

BITSTRUCT_DEFINE_START(TestBitStruct_u8, /* size */ 8)
  BitStructInt</*lsb*/0, /*width*/3> i3;
  BitStructUint</*lsb*/3, /*width*/4> u4;

  BitStructUint</*lsb*/0, /*width*/8> alias_all;
BITSTRUCT_DEFINE_END(TestBitStruct_u8);

TEST(BitStructs, FieldAssignment) {
  TestBitStruct_u8 all_1s{};
  all_1s.alias_all = 0xffu;

  {
    TestBitStruct_u8 tst{};
    tst.i3 = all_1s.i3;

    // Copying a single bitfield does not copy all bitfields.
    EXPECT_EQ(0b111, tst.alias_all);
  }

  {
    TestBitStruct_u8 tst{};
    tst.u4 = all_1s.u4;

    // Copying a single bitfield does not copy all bitfields.
    EXPECT_EQ(0b1111000, tst.alias_all);
  }
}

BITSTRUCT_DEFINE_START(NestedStruct, /* size */ 64)
  BitStructField<MixedSizeBitStruct, /*lsb*/0> mixed_lower;
  BitStructField<MixedSizeBitStruct, /*lsb*/32> mixed_upper;

  BitStructUint</*lsb*/0, /*width*/64> alias_all;
BITSTRUCT_DEFINE_END(NestedStruct);

TEST(BitStructs, NestedFieldAssignment) {
  MixedSizeBitStruct mixed_all_1s{};
  mixed_all_1s.alias_all = 0xFFFFFFFFu;

  {
    NestedStruct xyz{};

    NestedStruct other{};
    other.mixed_upper = mixed_all_1s;
    other.mixed_lower = mixed_all_1s;

    // Copying a single bitfield does not copy all bitfields.
    xyz.mixed_lower = other.mixed_lower;
    EXPECT_EQ(0xFFFFFFFFu, xyz.alias_all);
  }

  {
    NestedStruct xyz{};

    NestedStruct other{};
    other.mixed_upper = mixed_all_1s;
    other.mixed_lower = mixed_all_1s;

    // Copying a single bitfield does not copy all bitfields.
    xyz.mixed_upper = other.mixed_upper;
    EXPECT_EQ(0xFFFFFFFF00000000u, xyz.alias_all);
  }
}

}  // namespace art
