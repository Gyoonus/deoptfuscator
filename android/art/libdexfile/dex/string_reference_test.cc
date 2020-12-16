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

#include "string_reference.h"

#include <memory>

#include "dex/dex_file_types.h"
#include "dex/test_dex_file_builder.h"
#include "gtest/gtest.h"

namespace art {

TEST(StringReference, ValueComparator) {
  // This is a regression test for the StringReferenceValueComparator using the wrong
  // dex file to get the string data from a StringId. We construct two dex files with
  // just a single string with the same length but different value. This creates dex
  // files that have the same layout, so the byte offset read from the StringId in one
  // dex file, when used in the other dex file still points to valid string data, except
  // that it's the wrong string. Without the fix the strings would then compare equal.
  TestDexFileBuilder builder1;
  builder1.AddString("String1");
  std::unique_ptr<const DexFile> dex_file1 = builder1.Build("dummy location 1");
  ASSERT_EQ(1u, dex_file1->NumStringIds());
  ASSERT_STREQ("String1", dex_file1->GetStringData(dex_file1->GetStringId(dex::StringIndex(0))));
  StringReference sr1(dex_file1.get(), dex::StringIndex(0));

  TestDexFileBuilder builder2;
  builder2.AddString("String2");
  std::unique_ptr<const DexFile> dex_file2 = builder2.Build("dummy location 2");
  ASSERT_EQ(1u, dex_file2->NumStringIds());
  ASSERT_STREQ("String2", dex_file2->GetStringData(dex_file2->GetStringId(dex::StringIndex(0))));
  StringReference sr2(dex_file2.get(), dex::StringIndex(0));

  StringReferenceValueComparator cmp;
  EXPECT_TRUE(cmp(sr1, sr2));  // "String1" < "String2" is true.
  EXPECT_FALSE(cmp(sr2, sr1));  // "String2" < "String1" is false.
}

TEST(StringReference, ValueComparator2) {
  const char* const kDexFile1Strings[] = {
      "",
      "abc",
      "abcxyz",
  };
  const char* const kDexFile2Strings[] = {
      "a",
      "abc",
      "abcdef",
      "def",
  };
  const bool expectedCmp12[arraysize(kDexFile1Strings)][arraysize(kDexFile2Strings)] = {
      { true, true, true, true },
      { false, false, true, true },
      { false, false, false, true },
  };
  const bool expectedCmp21[arraysize(kDexFile2Strings)][arraysize(kDexFile1Strings)] = {
      { false, true, true },
      { false, false, true },
      { false, false, true },
      { false, false, false },
  };

  TestDexFileBuilder builder1;
  for (const char* s : kDexFile1Strings) {
    builder1.AddString(s);
  }
  std::unique_ptr<const DexFile> dex_file1 = builder1.Build("dummy location 1");
  ASSERT_EQ(arraysize(kDexFile1Strings), dex_file1->NumStringIds());
  for (size_t index = 0; index != arraysize(kDexFile1Strings); ++index) {
    ASSERT_STREQ(kDexFile1Strings[index],
                 dex_file1->GetStringData(dex_file1->GetStringId(dex::StringIndex(index))));
  }

  TestDexFileBuilder builder2;
  for (const char* s : kDexFile2Strings) {
    builder2.AddString(s);
  }
  std::unique_ptr<const DexFile> dex_file2 = builder2.Build("dummy location 1");
  ASSERT_EQ(arraysize(kDexFile2Strings), dex_file2->NumStringIds());
  for (size_t index = 0; index != arraysize(kDexFile2Strings); ++index) {
    ASSERT_STREQ(kDexFile2Strings[index],
                 dex_file2->GetStringData(dex_file2->GetStringId(dex::StringIndex(index))));
  }

  StringReferenceValueComparator cmp;
  for (size_t index1 = 0; index1 != arraysize(kDexFile1Strings); ++index1) {
    for (size_t index2 = 0; index2 != arraysize(kDexFile2Strings); ++index2) {
      StringReference sr1(dex_file1.get(), dex::StringIndex(index1));
      StringReference sr2(dex_file2.get(), dex::StringIndex(index2));
      EXPECT_EQ(expectedCmp12[index1][index2], cmp(sr1, sr2)) << index1 << " " << index2;
      EXPECT_EQ(expectedCmp21[index2][index1], cmp(sr2, sr1)) << index1 << " " << index2;
    }
  }
}

}  // namespace art
