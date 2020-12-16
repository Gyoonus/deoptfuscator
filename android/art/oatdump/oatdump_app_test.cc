/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "oatdump_test.h"

namespace art {

TEST_F(OatDumpTest, TestAppWithBootImage) {
  std::string error_msg;
  ASSERT_TRUE(GenerateAppOdexFile(kDynamic, {"--runtime-arg", "-Xmx64M"}, &error_msg)) << error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeOatWithBootImage, {}, kListAndCode, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestAppWithBootImageStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(GenerateAppOdexFile(kStatic, {"--runtime-arg", "-Xmx64M"}, &error_msg)) << error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeOatWithBootImage, {}, kListAndCode, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestPicAppWithBootImage) {
  std::string error_msg;
  ASSERT_TRUE(
      GenerateAppOdexFile(kDynamic, {"--runtime-arg", "-Xmx64M", "--compile-pic"}, &error_msg))
      << error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeOatWithBootImage, {}, kListAndCode, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestPicAppWithBootImageStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(
      GenerateAppOdexFile(kStatic, {"--runtime-arg", "-Xmx64M", "--compile-pic"}, &error_msg))
      << error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeOatWithBootImage, {}, kListAndCode, &error_msg)) << error_msg;
}

}  // namespace art
