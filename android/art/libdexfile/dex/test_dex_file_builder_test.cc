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

#include "test_dex_file_builder.h"

#include "dex/dex_file-inl.h"
#include "gtest/gtest.h"

namespace art {

TEST(TestDexFileBuilderTest, SimpleTest) {
  TestDexFileBuilder builder;
  builder.AddString("Arbitrary string");
  builder.AddType("Ljava/lang/Class;");
  builder.AddField("LTestClass;", "[I", "intField");
  builder.AddMethod("LTestClass;", "()I", "foo");
  builder.AddMethod("LTestClass;", "(Ljava/lang/Object;[Ljava/lang/Object;)LTestClass;", "bar");
  const char* dex_location = "TestDexFileBuilder/SimpleTest";
  std::unique_ptr<const DexFile> dex_file(builder.Build(dex_location));
  ASSERT_TRUE(dex_file != nullptr);
  EXPECT_STREQ(dex_location, dex_file->GetLocation().c_str());

  static const char* const expected_strings[] = {
      "Arbitrary string",
      "I",
      "LLL",  // shorty
      "LTestClass;",
      "Ljava/lang/Class;",
      "Ljava/lang/Object;",
      "[I",
      "[Ljava/lang/Object;",
      "bar",
      "foo",
      "intField",
  };
  ASSERT_EQ(arraysize(expected_strings), dex_file->NumStringIds());
  for (size_t i = 0; i != arraysize(expected_strings); ++i) {
    EXPECT_STREQ(expected_strings[i],
                 dex_file->GetStringData(dex_file->GetStringId(dex::StringIndex(i)))) << i;
  }

  static const char* const expected_types[] = {
      "I",
      "LTestClass;",
      "Ljava/lang/Class;",
      "Ljava/lang/Object;",
      "[I",
      "[Ljava/lang/Object;",
  };
  ASSERT_EQ(arraysize(expected_types), dex_file->NumTypeIds());
  for (size_t i = 0; i != arraysize(expected_types); ++i) {
    EXPECT_STREQ(expected_types[i],
                 dex_file->GetTypeDescriptor(dex_file->GetTypeId(dex::TypeIndex(i)))) << i;
  }

  ASSERT_EQ(1u, dex_file->NumFieldIds());
  EXPECT_STREQ("[I TestClass.intField", dex_file->PrettyField(0u).c_str());

  ASSERT_EQ(2u, dex_file->NumProtoIds());
  ASSERT_EQ(2u, dex_file->NumMethodIds());
  EXPECT_STREQ("TestClass TestClass.bar(java.lang.Object, java.lang.Object[])",
               dex_file->PrettyMethod(0u).c_str());
  EXPECT_STREQ("int TestClass.foo()",
               dex_file->PrettyMethod(1u).c_str());

  EXPECT_EQ(0u, builder.GetStringIdx("Arbitrary string"));
  EXPECT_EQ(2u, builder.GetTypeIdx("Ljava/lang/Class;"));
  EXPECT_EQ(0u, builder.GetFieldIdx("LTestClass;", "[I", "intField"));
  EXPECT_EQ(1u, builder.GetMethodIdx("LTestClass;", "()I", "foo"));
  EXPECT_EQ(0u, builder.GetMethodIdx("LTestClass;", "(Ljava/lang/Object;[Ljava/lang/Object;)LTestClass;", "bar"));
}

}  // namespace art
