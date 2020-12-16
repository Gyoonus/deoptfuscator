/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "linker/mips/relative_patcher_mips.h"
#include "linker/relative_patcher_test.h"

namespace art {
namespace linker {

class Mips32r6RelativePatcherTest : public RelativePatcherTest {
 public:
  Mips32r6RelativePatcherTest() : RelativePatcherTest(InstructionSet::kMips, "mips32r6") {}

 protected:
  static const uint8_t kUnpatchedPcRelativeRawCode[];
  static const uint32_t kLiteralOffsetHigh;
  static const uint32_t kLiteralOffsetLow1;
  static const uint32_t kLiteralOffsetLow2;
  static const uint32_t kAnchorOffset;
  static const ArrayRef<const uint8_t> kUnpatchedPcRelativeCode;

  uint32_t GetMethodOffset(uint32_t method_idx) {
    auto result = method_offset_map_.FindMethodOffset(MethodRef(method_idx));
    CHECK(result.first);
    return result.second;
  }

  void CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches, uint32_t target_offset);
  void TestStringBssEntry(uint32_t bss_begin, uint32_t string_entry_offset);
  void TestStringReference(uint32_t string_offset);
};

const uint8_t Mips32r6RelativePatcherTest::kUnpatchedPcRelativeRawCode[] = {
    0x34, 0x12, 0x5E, 0xEE,  // auipc s2, high(diff); placeholder = 0x1234
    0x78, 0x56, 0x52, 0x26,  // addiu s2, s2, low(diff); placeholder = 0x5678
    0x78, 0x56, 0x52, 0x8E,  // lw    s2, (low(diff))(s2) ; placeholder = 0x5678
};
const uint32_t Mips32r6RelativePatcherTest::kLiteralOffsetHigh = 0;  // At auipc.
const uint32_t Mips32r6RelativePatcherTest::kLiteralOffsetLow1 = 4;  // At addiu.
const uint32_t Mips32r6RelativePatcherTest::kLiteralOffsetLow2 = 8;  // At lw.
const uint32_t Mips32r6RelativePatcherTest::kAnchorOffset = 0;  // At auipc (where PC+0 points).
const ArrayRef<const uint8_t> Mips32r6RelativePatcherTest::kUnpatchedPcRelativeCode(
    kUnpatchedPcRelativeRawCode);

void Mips32r6RelativePatcherTest::CheckPcRelativePatch(const ArrayRef<const LinkerPatch>& patches,
                                                       uint32_t target_offset) {
  AddCompiledMethod(MethodRef(1u), kUnpatchedPcRelativeCode, ArrayRef<const LinkerPatch>(patches));
  Link();

  auto result = method_offset_map_.FindMethodOffset(MethodRef(1u));
  ASSERT_TRUE(result.first);

  uint32_t diff = target_offset - (result.second + kAnchorOffset);
  diff += (diff & 0x8000) << 1;  // Account for sign extension in addiu/lw.

  const uint8_t expected_code[] = {
      static_cast<uint8_t>(diff >> 16), static_cast<uint8_t>(diff >> 24), 0x5E, 0xEE,
      static_cast<uint8_t>(diff), static_cast<uint8_t>(diff >> 8), 0x52, 0x26,
      static_cast<uint8_t>(diff), static_cast<uint8_t>(diff >> 8), 0x52, 0x8E,
  };
  EXPECT_TRUE(CheckLinkedMethod(MethodRef(1u), ArrayRef<const uint8_t>(expected_code)));
}

void Mips32r6RelativePatcherTest::TestStringBssEntry(uint32_t bss_begin,
                                                     uint32_t string_entry_offset) {
  constexpr uint32_t kStringIndex = 1u;
  string_index_to_offset_map_.Put(kStringIndex, string_entry_offset);
  bss_begin_ = bss_begin;
  LinkerPatch patches[] = {
      LinkerPatch::StringBssEntryPatch(kLiteralOffsetHigh, nullptr, kAnchorOffset, kStringIndex),
      LinkerPatch::StringBssEntryPatch(kLiteralOffsetLow1, nullptr, kAnchorOffset, kStringIndex),
      LinkerPatch::StringBssEntryPatch(kLiteralOffsetLow2, nullptr, kAnchorOffset, kStringIndex)
  };
  CheckPcRelativePatch(ArrayRef<const LinkerPatch>(patches), bss_begin_ + string_entry_offset);
}

void Mips32r6RelativePatcherTest::TestStringReference(uint32_t string_offset) {
  constexpr uint32_t kStringIndex = 1u;
  string_index_to_offset_map_.Put(kStringIndex, string_offset);
  LinkerPatch patches[] = {
      LinkerPatch::RelativeStringPatch(kLiteralOffsetHigh, nullptr, kAnchorOffset, kStringIndex),
      LinkerPatch::RelativeStringPatch(kLiteralOffsetLow1, nullptr, kAnchorOffset, kStringIndex),
      LinkerPatch::RelativeStringPatch(kLiteralOffsetLow2, nullptr, kAnchorOffset, kStringIndex)
  };
  CheckPcRelativePatch(ArrayRef<const LinkerPatch>(patches), string_offset);
}

TEST_F(Mips32r6RelativePatcherTest, StringBssEntry) {
  TestStringBssEntry(/* bss_begin */ 0x12345678, /* string_entry_offset */ 0x1234);
}

TEST_F(Mips32r6RelativePatcherTest, StringReference) {
  TestStringReference(/* string_offset*/ 0x87651234);
}

}  // namespace linker
}  // namespace art
