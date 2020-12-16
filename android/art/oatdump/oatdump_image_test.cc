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

#include "oatdump_test.h"

namespace art {

// Disable tests on arm and mips as they are taking too long to run. b/27824283.
#if !defined(__arm__) && !defined(__mips__)
TEST_F(OatDumpTest, TestImage) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {}, kListAndCode, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestImageStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {}, kListAndCode, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestOatImage) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeOat, {}, kListAndCode, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestOatImageStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeOat, {}, kListAndCode, &error_msg)) << error_msg;
}
#endif
}  // namespace art
