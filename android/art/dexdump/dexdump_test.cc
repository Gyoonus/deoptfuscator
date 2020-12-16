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

#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include "arch/instruction_set.h"
#include "base/os.h"
#include "base/utils.h"
#include "common_runtime_test.h"
#include "exec_utils.h"

namespace art {

class DexDumpTest : public CommonRuntimeTest {
 protected:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();
    // Dogfood our own lib core dex file.
    dex_file_ = GetLibCoreDexFileNames()[0];
  }

  // Runs test with given arguments.
  bool Exec(const std::vector<std::string>& args, std::string* error_msg) {
    // TODO(ajcbik): dexdump2 -> dexdump
    std::string file_path = GetTestAndroidRoot() + "/bin/dexdump2";
    EXPECT_TRUE(OS::FileExists(file_path.c_str())) << file_path << " should be a valid file path";
    std::vector<std::string> exec_argv = { file_path };
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());
    return ::art::Exec(exec_argv, error_msg);
  }

  std::string dex_file_;
};


TEST_F(DexDumpTest, NoInputFileGiven) {
  std::string error_msg;
  ASSERT_FALSE(Exec({}, &error_msg)) << error_msg;
}

TEST_F(DexDumpTest, CantOpenOutput) {
  std::string error_msg;
  ASSERT_FALSE(Exec({"-o", "/joho", dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexDumpTest, BadFlagCombination) {
  std::string error_msg;
  ASSERT_FALSE(Exec({"-c", "-i", dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexDumpTest, FullPlainOutput) {
  std::string error_msg;
  ASSERT_TRUE(Exec({"-d", "-f", "-h", "-l", "plain", "-o", "/dev/null",
    dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexDumpTest, XMLOutput) {
  std::string error_msg;
  ASSERT_TRUE(Exec({"-l", "xml", "-o", "/dev/null",
    dex_file_}, &error_msg)) << error_msg;
}

}  // namespace art
