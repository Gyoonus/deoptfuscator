/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "descriptors_names.h"

#include "gtest/gtest.h"

namespace art {

class DescriptorsNamesTest : public testing::Test {};

TEST_F(DescriptorsNamesTest, PrettyDescriptor_ArrayReferences) {
  EXPECT_EQ("java.lang.Class[]", PrettyDescriptor("[Ljava/lang/Class;"));
  EXPECT_EQ("java.lang.Class[][]", PrettyDescriptor("[[Ljava/lang/Class;"));
}

TEST_F(DescriptorsNamesTest, PrettyDescriptor_ScalarReferences) {
  EXPECT_EQ("java.lang.String", PrettyDescriptor("Ljava.lang.String;"));
  EXPECT_EQ("java.lang.String", PrettyDescriptor("Ljava/lang/String;"));
}

TEST_F(DescriptorsNamesTest, PrettyDescriptor_Primitive) {
  EXPECT_EQ("boolean", PrettyDescriptor(Primitive::kPrimBoolean));
  EXPECT_EQ("byte", PrettyDescriptor(Primitive::kPrimByte));
  EXPECT_EQ("char", PrettyDescriptor(Primitive::kPrimChar));
  EXPECT_EQ("short", PrettyDescriptor(Primitive::kPrimShort));
  EXPECT_EQ("int", PrettyDescriptor(Primitive::kPrimInt));
  EXPECT_EQ("float", PrettyDescriptor(Primitive::kPrimFloat));
  EXPECT_EQ("long", PrettyDescriptor(Primitive::kPrimLong));
  EXPECT_EQ("double", PrettyDescriptor(Primitive::kPrimDouble));
  EXPECT_EQ("void", PrettyDescriptor(Primitive::kPrimVoid));
}

TEST_F(DescriptorsNamesTest, PrettyDescriptor_PrimitiveArrays) {
  EXPECT_EQ("boolean[]", PrettyDescriptor("[Z"));
  EXPECT_EQ("boolean[][]", PrettyDescriptor("[[Z"));
  EXPECT_EQ("byte[]", PrettyDescriptor("[B"));
  EXPECT_EQ("byte[][]", PrettyDescriptor("[[B"));
  EXPECT_EQ("char[]", PrettyDescriptor("[C"));
  EXPECT_EQ("char[][]", PrettyDescriptor("[[C"));
  EXPECT_EQ("double[]", PrettyDescriptor("[D"));
  EXPECT_EQ("double[][]", PrettyDescriptor("[[D"));
  EXPECT_EQ("float[]", PrettyDescriptor("[F"));
  EXPECT_EQ("float[][]", PrettyDescriptor("[[F"));
  EXPECT_EQ("int[]", PrettyDescriptor("[I"));
  EXPECT_EQ("int[][]", PrettyDescriptor("[[I"));
  EXPECT_EQ("long[]", PrettyDescriptor("[J"));
  EXPECT_EQ("long[][]", PrettyDescriptor("[[J"));
  EXPECT_EQ("short[]", PrettyDescriptor("[S"));
  EXPECT_EQ("short[][]", PrettyDescriptor("[[S"));
}

TEST_F(DescriptorsNamesTest, PrettyDescriptor_PrimitiveScalars) {
  EXPECT_EQ("boolean", PrettyDescriptor("Z"));
  EXPECT_EQ("byte", PrettyDescriptor("B"));
  EXPECT_EQ("char", PrettyDescriptor("C"));
  EXPECT_EQ("double", PrettyDescriptor("D"));
  EXPECT_EQ("float", PrettyDescriptor("F"));
  EXPECT_EQ("int", PrettyDescriptor("I"));
  EXPECT_EQ("long", PrettyDescriptor("J"));
  EXPECT_EQ("short", PrettyDescriptor("S"));
}

TEST_F(DescriptorsNamesTest, MangleForJni) {
  EXPECT_EQ("hello_00024world", MangleForJni("hello$world"));
  EXPECT_EQ("hello_000a9world", MangleForJni("hello\xc2\xa9world"));
  EXPECT_EQ("hello_1world", MangleForJni("hello_world"));
  EXPECT_EQ("Ljava_lang_String_2", MangleForJni("Ljava/lang/String;"));
  EXPECT_EQ("_3C", MangleForJni("[C"));
}

TEST_F(DescriptorsNamesTest, IsValidDescriptor) {
  std::vector<uint8_t> descriptor(
      { 'L', 'a', '/', 'b', '$', 0xed, 0xa0, 0x80, 0xed, 0xb0, 0x80, ';', 0x00 });
  EXPECT_TRUE(IsValidDescriptor(reinterpret_cast<char*>(&descriptor[0])));

  std::vector<uint8_t> unpaired_surrogate(
      { 'L', 'a', '/', 'b', '$', 0xed, 0xa0, 0x80, ';', 0x00 });
  EXPECT_FALSE(IsValidDescriptor(reinterpret_cast<char*>(&unpaired_surrogate[0])));

  std::vector<uint8_t> unpaired_surrogate_at_end(
      { 'L', 'a', '/', 'b', '$', 0xed, 0xa0, 0x80, 0x00 });
  EXPECT_FALSE(IsValidDescriptor(reinterpret_cast<char*>(&unpaired_surrogate_at_end[0])));

  std::vector<uint8_t> invalid_surrogate(
      { 'L', 'a', '/', 'b', '$', 0xed, 0xb0, 0x80, ';', 0x00 });
  EXPECT_FALSE(IsValidDescriptor(reinterpret_cast<char*>(&invalid_surrogate[0])));

  std::vector<uint8_t> unpaired_surrogate_with_multibyte_sequence(
      { 'L', 'a', '/', 'b', '$', 0xed, 0xb0, 0x80, 0xf0, 0x9f, 0x8f, 0xa0, ';', 0x00 });
  EXPECT_FALSE(
      IsValidDescriptor(reinterpret_cast<char*>(&unpaired_surrogate_with_multibyte_sequence[0])));
}

}  // namespace art
