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

#include "oatdump_test.h"

namespace art {

// Disable tests on arm and mips as they are taking too long to run. b/27824283.
#if !defined(__arm__) && !defined(__mips__)
TEST_F(OatDumpTest, TestNoDumpVmap) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--no-dump:vmap"}, kListAndCode, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestNoDumpVmapStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--no-dump:vmap"}, kListAndCode, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestNoDisassemble) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--no-disassemble"}, kListAndCode, &error_msg))
      << error_msg;
}
TEST_F(OatDumpTest, TestNoDisassembleStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--no-disassemble"}, kListAndCode, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestListClasses) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--list-classes"}, kListOnly, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestListClassesStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--list-classes"}, kListOnly, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestListMethods) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--list-methods"}, kListOnly, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestListMethodsStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--list-methods"}, kListOnly, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestSymbolize) {
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeSymbolize, {}, kListOnly, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestSymbolizeStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeSymbolize, {}, kListOnly, &error_msg)) << error_msg;
}

TEST_F(OatDumpTest, TestExportDex) {
  // Test is failing on target, b/77469384.
  TEST_DISABLED_FOR_TARGET();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeOat, {"--export-dex-to=" + tmp_dir_}, kListOnly, &error_msg))
      << error_msg;
  const std::string dex_location = tmp_dir_+ "/core-oj-hostdex.jar_export.dex";
  const std::string dexdump2 = GetExecutableFilePath("dexdump2",
                                                     /*is_debug*/false,
                                                     /*is_static*/false);
  ASSERT_TRUE(ForkAndExecAndWait({dexdump2, "-d", dex_location}, &error_msg)) << error_msg;
}
TEST_F(OatDumpTest, TestExportDexStatic) {
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeOat, {"--export-dex-to=" + tmp_dir_}, kListOnly, &error_msg))
      << error_msg;
}
#endif
}  // namespace art
