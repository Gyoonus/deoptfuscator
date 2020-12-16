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

#include "subtype_check_info.h"

#include "gtest/gtest.h"
#include "android-base/logging.h"

namespace art {

constexpr size_t BitString::kBitSizeAtPosition[BitString::kCapacity];
constexpr size_t BitString::kCapacity;

};  // namespace art

using namespace art;  // NOLINT

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

// Make max bistring, e.g. BitString[4095,15,2047] for {12,4,11}
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

struct SubtypeCheckInfoTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    android::base::InitLogging(/*argv*/nullptr);
  }

  virtual void TearDown() {
  }

  static SubtypeCheckInfo MakeSubtypeCheckInfo(BitString path_to_root = {},
                                               BitStringChar next = {},
                                               bool overflow = false,
                                               size_t depth = 1u) {
    // Depth=1 is good default because it will go through all state transitions,
    // and its children will also go through all state transitions.
    return SubtypeCheckInfo(path_to_root, next, overflow, depth);
  }

  static SubtypeCheckInfo MakeSubtypeCheckInfoInfused(BitString bs = {},
                                                      bool overflow = false,
                                                      size_t depth = 1u) {
    // Depth=1 is good default because it will go through all state transitions,
    // and its children will also go through all state transitions.
    SubtypeCheckBits iod;
    iod.bitstring_ = bs;
    iod.overflow_ = overflow;
    return SubtypeCheckInfo::Create(iod, depth);
  }

  static SubtypeCheckInfo MakeSubtypeCheckInfoUnchecked(BitString bs = {},
                                                        bool overflow = false,
                                                        size_t depth = 1u) {
    // Depth=1 is good default because it will go through all state transitions,
    // and its children will also go through all state transitions.
    return SubtypeCheckInfo::MakeUnchecked(bs, overflow, depth);
  }

  static bool HasNext(SubtypeCheckInfo io) {
    return io.HasNext();
  }

  static BitString GetPathToRoot(SubtypeCheckInfo io) {
    return io.GetPathToRoot();
  }

  // Create an SubtypeCheckInfo with the same depth, but with everything else reset.
  // Returns: SubtypeCheckInfo in the Uninitialized state.
  static SubtypeCheckInfo CopyCleared(SubtypeCheckInfo sc) {
    SubtypeCheckInfo cleared_copy{};
    cleared_copy.depth_ = sc.depth_;
    DCHECK_EQ(SubtypeCheckInfo::kUninitialized, cleared_copy.GetState());
    return cleared_copy;
  }
};

const char* GetExpectedMessageForDeathTest(const char* msg) {
#ifdef ART_TARGET_ANDROID
  // On Android, dcheck failure messages go to logcat,
  // which gtest death tests does not check, and thus the tests would fail with
  // "unexpected message ''"
  UNUSED(msg);
  return "";  // Still ensures there was a bad return code, but match anything.
#else
  return msg;
#endif
}

TEST_F(SubtypeCheckInfoTest, IllegalValues) {
  // This test relies on BitString being at least 3 large.
  // It will need to be updated otherwise.
  ASSERT_LE(3u, BitString::kCapacity);

  // Illegal values during construction would cause a Dcheck failure and crash.
  ASSERT_DEATH(MakeSubtypeCheckInfo(MakeBitString({1u}),
                                    /*next*/MakeBitStringChar(0),
                                    /*overflow*/false,
                                    /*depth*/0u),
               GetExpectedMessageForDeathTest("Path was too long for the depth"));
  ASSERT_DEATH(MakeSubtypeCheckInfoInfused(MakeBitString({1u, 1u}),
                                           /*overflow*/false,
                                           /*depth*/0u),
               GetExpectedMessageForDeathTest("Bitstring too long for depth"));
  ASSERT_DEATH(MakeSubtypeCheckInfo(MakeBitString({1u}),
                                    /*next*/MakeBitStringChar(0),
                                    /*overflow*/false,
                                    /*depth*/1u),
               GetExpectedMessageForDeathTest("Expected \\(Assigned\\|Initialized\\) "
                                              "state to have >0 Next value"));
  ASSERT_DEATH(MakeSubtypeCheckInfoInfused(MakeBitString({0u, 2u, 1u}),
                                           /*overflow*/false,
                                           /*depth*/2u),
               GetExpectedMessageForDeathTest("Path to root had non-0s following 0s"));
  ASSERT_DEATH(MakeSubtypeCheckInfo(MakeBitString({0u, 2u}),
                                    /*next*/MakeBitStringChar(1u),
                                    /*overflow*/false,
                                    /*depth*/2u),
               GetExpectedMessageForDeathTest("Path to root had non-0s following 0s"));
  ASSERT_DEATH(MakeSubtypeCheckInfo(MakeBitString({0u, 1u, 1u}),
                                    /*next*/MakeBitStringChar(0),
                                    /*overflow*/false,
                                    /*depth*/3u),
               GetExpectedMessageForDeathTest("Path to root had non-0s following 0s"));

  // These are really slow (~1sec per death test on host),
  // keep them down to a minimum.
}

TEST_F(SubtypeCheckInfoTest, States) {
  EXPECT_EQ(SubtypeCheckInfo::kUninitialized, MakeSubtypeCheckInfo().GetState());
  EXPECT_EQ(SubtypeCheckInfo::kInitialized,
            MakeSubtypeCheckInfo(/*path*/{}, /*next*/MakeBitStringChar(1)).GetState());
  EXPECT_EQ(SubtypeCheckInfo::kOverflowed,
            MakeSubtypeCheckInfo(/*path*/{},
                                 /*next*/MakeBitStringChar(1),
                                 /*overflow*/true,
                                 /*depth*/1u).GetState());
  EXPECT_EQ(SubtypeCheckInfo::kAssigned,
            MakeSubtypeCheckInfo(/*path*/MakeBitString({1u}),
                                 /*next*/MakeBitStringChar(1),
                                 /*overflow*/false,
                                 /*depth*/1u).GetState());

  // Test edge conditions: depth == BitString::kCapacity (No Next value).
  EXPECT_EQ(SubtypeCheckInfo::kAssigned,
            MakeSubtypeCheckInfo(/*path*/MakeBitStringMax(),
                                 /*next*/MakeBitStringChar(0),
                                 /*overflow*/false,
                                 /*depth*/BitString::kCapacity).GetState());
  EXPECT_EQ(SubtypeCheckInfo::kInitialized,
            MakeSubtypeCheckInfo(/*path*/MakeBitStringMax<BitString::kCapacity - 1u>(),
                                 /*next*/MakeBitStringChar(0),
                                 /*overflow*/false,
                                 /*depth*/BitString::kCapacity).GetState());
  // Test edge conditions: depth > BitString::kCapacity (Must overflow).
  EXPECT_EQ(SubtypeCheckInfo::kOverflowed,
            MakeSubtypeCheckInfo(/*path*/MakeBitStringMax(),
                                 /*next*/MakeBitStringChar(0),
                                 /*overflow*/true,
                                 /*depth*/BitString::kCapacity + 1u).GetState());
}

TEST_F(SubtypeCheckInfoTest, NextValue) {
  // Validate "Next" is correctly aliased as the Bitstring[Depth] character.
  EXPECT_EQ(MakeBitStringChar(1u), MakeSubtypeCheckInfoUnchecked(MakeBitString({1u, 2u, 3u}),
                                                           /*overflow*/false,
                                                           /*depth*/0u).GetNext());
  EXPECT_EQ(MakeBitStringChar(2u), MakeSubtypeCheckInfoUnchecked(MakeBitString({1u, 2u, 3u}),
                                                           /*overflow*/false,
                                                           /*depth*/1u).GetNext());
  EXPECT_EQ(MakeBitStringChar(3u), MakeSubtypeCheckInfoUnchecked(MakeBitString({1u, 2u, 3u}),
                                                           /*overflow*/false,
                                                           /*depth*/2u).GetNext());
  EXPECT_EQ(MakeBitStringChar(1u), MakeSubtypeCheckInfoUnchecked(MakeBitString({0u, 2u, 1u}),
                                                           /*overflow*/false,
                                                           /*depth*/2u).GetNext());
  // Test edge conditions: depth == BitString::kCapacity (No Next value).
  EXPECT_FALSE(HasNext(MakeSubtypeCheckInfoUnchecked(MakeBitStringMax<BitString::kCapacity>(),
                                                     /*overflow*/false,
                                                     /*depth*/BitString::kCapacity)));
  // Anything with depth >= BitString::kCapacity has no next value.
  EXPECT_FALSE(HasNext(MakeSubtypeCheckInfoUnchecked(MakeBitStringMax<BitString::kCapacity>(),
                                                     /*overflow*/false,
                                                     /*depth*/BitString::kCapacity + 1u)));
  EXPECT_FALSE(HasNext(MakeSubtypeCheckInfoUnchecked(MakeBitStringMax(),
                                                     /*overflow*/false,
                                                     /*depth*/std::numeric_limits<size_t>::max())));
}

template <size_t kPos = BitString::kCapacity>
size_t LenForPos() { return BitString::GetBitLengthTotalAtPosition(kPos); }

TEST_F(SubtypeCheckInfoTest, EncodedPathToRoot) {
  using StorageType = BitString::StorageType;

  SubtypeCheckInfo sci =
      MakeSubtypeCheckInfo(/*path_to_root*/MakeBitStringMax(),
                           /*next*/BitStringChar{},
                           /*overflow*/false,
                           /*depth*/BitString::kCapacity);
  // 0b000...111 where LSB == 1, and trailing 1s = the maximum bitstring representation.
  EXPECT_EQ(MaxInt<StorageType>(LenForPos()), sci.GetEncodedPathToRoot());

  // The rest of this test is written assuming kCapacity == 3 for convenience.
  // Please update the test if this changes.
  ASSERT_EQ(3u, BitString::kCapacity);
  ASSERT_EQ(12u, BitString::kBitSizeAtPosition[0]);
  ASSERT_EQ(4u, BitString::kBitSizeAtPosition[1]);
  ASSERT_EQ(11u, BitString::kBitSizeAtPosition[2]);

  SubtypeCheckInfo sci2 =
      MakeSubtypeCheckInfoUnchecked(MakeBitStringMax<2u>(),
                                   /*overflow*/false,
                                   /*depth*/BitString::kCapacity);

#define MAKE_ENCODED_PATH(pos0, pos1, pos2) \
    (((pos0) << 0) | \
     ((pos1) << BitString::kBitSizeAtPosition[0]) | \
     ((pos2) << (BitString::kBitSizeAtPosition[0] + BitString::kBitSizeAtPosition[1])))

  EXPECT_EQ(MAKE_ENCODED_PATH(MaxInt<BitString::StorageType>(12), 0b1111, 0b0),
            sci2.GetEncodedPathToRoot());
  EXPECT_EQ(MAKE_ENCODED_PATH(MaxInt<BitString::StorageType>(12), 0b1111, 0b11111111111),
            sci2.GetEncodedPathToRootMask());

  SubtypeCheckInfo sci3 =
      MakeSubtypeCheckInfoUnchecked(MakeBitStringMax<2u>(),
                                   /*overflow*/false,
                                   /*depth*/BitString::kCapacity - 1u);

  EXPECT_EQ(MAKE_ENCODED_PATH(MaxInt<BitString::StorageType>(12), 0b1111, 0b0),
            sci3.GetEncodedPathToRoot());
  EXPECT_EQ(MAKE_ENCODED_PATH(MaxInt<BitString::StorageType>(12), 0b1111, 0b0),
            sci3.GetEncodedPathToRootMask());

  SubtypeCheckInfo sci4 =
      MakeSubtypeCheckInfoUnchecked(MakeBitString({0b1010101u}),
                                   /*overflow*/false,
                                   /*depth*/BitString::kCapacity - 2u);

  EXPECT_EQ(MAKE_ENCODED_PATH(0b1010101u, 0b0000, 0b0), sci4.GetEncodedPathToRoot());
  EXPECT_EQ(MAKE_ENCODED_PATH(MaxInt<BitString::StorageType>(12), 0b0000, 0b0),
            sci4.GetEncodedPathToRootMask());
}

TEST_F(SubtypeCheckInfoTest, NewForRoot) {
  SubtypeCheckInfo sci = SubtypeCheckInfo::CreateRoot();
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, sci.GetState());  // Root is always assigned.
  EXPECT_EQ(0u, GetPathToRoot(sci).Length());  // Root's path length is 0.
  EXPECT_TRUE(HasNext(sci));  // Root always has a "Next".
  EXPECT_EQ(MakeBitStringChar(1u), sci.GetNext());  // Next>=1 to disambiguate from Uninitialized.
}

TEST_F(SubtypeCheckInfoTest, CopyCleared) {
  SubtypeCheckInfo root = SubtypeCheckInfo::CreateRoot();
  EXPECT_EQ(MakeBitStringChar(1u), root.GetNext());

  SubtypeCheckInfo childC = root.CreateChild(/*assign*/true);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, childC.GetState());
  EXPECT_EQ(MakeBitStringChar(2u), root.GetNext());  // Next incremented for Assign.
  EXPECT_EQ(MakeBitString({1u}), GetPathToRoot(childC));

  SubtypeCheckInfo cleared_copy = CopyCleared(childC);
  EXPECT_EQ(SubtypeCheckInfo::kUninitialized, cleared_copy.GetState());
  EXPECT_EQ(MakeBitString({}), GetPathToRoot(cleared_copy));

  // CopyCleared is just a thin wrapper around value-init and providing the depth.
  SubtypeCheckInfo cleared_copy_value =
      SubtypeCheckInfo::Create(SubtypeCheckBits{}, /*depth*/1u);
  EXPECT_EQ(SubtypeCheckInfo::kUninitialized, cleared_copy_value.GetState());
  EXPECT_EQ(MakeBitString({}), GetPathToRoot(cleared_copy_value));
}

TEST_F(SubtypeCheckInfoTest, NewForChild2) {
  SubtypeCheckInfo root = SubtypeCheckInfo::CreateRoot();
  EXPECT_EQ(MakeBitStringChar(1u), root.GetNext());

  SubtypeCheckInfo childC = root.CreateChild(/*assign*/true);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, childC.GetState());
  EXPECT_EQ(MakeBitStringChar(2u), root.GetNext());  // Next incremented for Assign.
  EXPECT_EQ(MakeBitString({1u}), GetPathToRoot(childC));
}

TEST_F(SubtypeCheckInfoTest, NewForChild) {
  SubtypeCheckInfo root = SubtypeCheckInfo::CreateRoot();
  EXPECT_EQ(MakeBitStringChar(1u), root.GetNext());

  SubtypeCheckInfo childA = root.CreateChild(/*assign*/false);
  EXPECT_EQ(SubtypeCheckInfo::kInitialized, childA.GetState());
  EXPECT_EQ(MakeBitStringChar(1u), root.GetNext());  // Next unchanged for Initialize.
  EXPECT_EQ(MakeBitString({}), GetPathToRoot(childA));

  SubtypeCheckInfo childB = root.CreateChild(/*assign*/false);
  EXPECT_EQ(SubtypeCheckInfo::kInitialized, childB.GetState());
  EXPECT_EQ(MakeBitStringChar(1u), root.GetNext());  // Next unchanged for Initialize.
  EXPECT_EQ(MakeBitString({}), GetPathToRoot(childB));

  SubtypeCheckInfo childC = root.CreateChild(/*assign*/true);
  EXPECT_EQ(SubtypeCheckInfo::kAssigned, childC.GetState());
  EXPECT_EQ(MakeBitStringChar(2u), root.GetNext());  // Next incremented for Assign.
  EXPECT_EQ(MakeBitString({1u}), GetPathToRoot(childC));

  {
    size_t cur_depth = 1u;
    SubtypeCheckInfo latest_child = childC;
    while (cur_depth != BitString::kCapacity) {
      latest_child = latest_child.CreateChild(/*assign*/true);
      ASSERT_EQ(SubtypeCheckInfo::kAssigned, latest_child.GetState());
      ASSERT_EQ(cur_depth + 1u, GetPathToRoot(latest_child).Length());
      cur_depth++;
    }

    // Future assignments will result in a too-deep overflow.
    SubtypeCheckInfo child_of_deep = latest_child.CreateChild(/*assign*/true);
    EXPECT_EQ(SubtypeCheckInfo::kOverflowed, child_of_deep.GetState());
    EXPECT_EQ(GetPathToRoot(latest_child), GetPathToRoot(child_of_deep));

    // Assignment of too-deep overflow also causes overflow.
    SubtypeCheckInfo child_of_deep_2 = child_of_deep.CreateChild(/*assign*/true);
    EXPECT_EQ(SubtypeCheckInfo::kOverflowed, child_of_deep_2.GetState());
    EXPECT_EQ(GetPathToRoot(child_of_deep), GetPathToRoot(child_of_deep_2));
  }

  {
    size_t cur_next = 2u;
    while (true) {
      if (cur_next == MaxInt<BitString::StorageType>(BitString::kBitSizeAtPosition[0u])) {
        break;
      }

      SubtypeCheckInfo child = root.CreateChild(/*assign*/true);
      ASSERT_EQ(SubtypeCheckInfo::kAssigned, child.GetState());
      ASSERT_EQ(MakeBitStringChar(cur_next+1u), root.GetNext());
      ASSERT_EQ(MakeBitString({cur_next}), GetPathToRoot(child));

      cur_next++;
    }
    // Now the root will be in a state that further assigns will be too-wide overflow.

    // Initialization still succeeds.
    SubtypeCheckInfo child = root.CreateChild(/*assign*/false);
    EXPECT_EQ(SubtypeCheckInfo::kInitialized, child.GetState());
    EXPECT_EQ(MakeBitStringChar(cur_next), root.GetNext());
    EXPECT_EQ(MakeBitString({}), GetPathToRoot(child));

    // Assignment goes to too-wide Overflow.
    SubtypeCheckInfo child_of = root.CreateChild(/*assign*/true);
    EXPECT_EQ(SubtypeCheckInfo::kOverflowed, child_of.GetState());
    EXPECT_EQ(MakeBitStringChar(cur_next), root.GetNext());
    EXPECT_EQ(MakeBitString({}), GetPathToRoot(child_of));

    // Assignment of overflowed child still succeeds.
    // The path to root is the same.
    SubtypeCheckInfo child_of2 = child_of.CreateChild(/*assign*/true);
    EXPECT_EQ(SubtypeCheckInfo::kOverflowed, child_of2.GetState());
    EXPECT_EQ(GetPathToRoot(child_of), GetPathToRoot(child_of2));
  }
}
