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
#include "gc/heap.h"
#include "gc/space/image_space.h"

namespace art {

class DexListTest : public CommonRuntimeTest {
 protected:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();
    // Dogfood our own lib core dex file.
    dex_file_ = GetLibCoreDexFileNames()[0];
  }

  // Runs test with given arguments.
  bool Exec(const std::vector<std::string>& args, std::string* error_msg) {
    std::string file_path = GetTestAndroidRoot();
    file_path += "/bin/dexlist";
    EXPECT_TRUE(OS::FileExists(file_path.c_str())) << file_path << " should be a valid file path";
    std::vector<std::string> exec_argv = { file_path };
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());
    return ::art::Exec(exec_argv, error_msg);
  }

  std::string dex_file_;
};


TEST_F(DexListTest, NoInputFileGiven) {
  std::string error_msg;
  ASSERT_FALSE(Exec({}, &error_msg)) << error_msg;
}

TEST_F(DexListTest, CantOpenOutput) {
  std::string error_msg;
  ASSERT_FALSE(Exec({"-o", "/joho", dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexListTest, IllFormedMethod) {
  std::string error_msg;
  ASSERT_FALSE(Exec({"-m", "joho", dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexListTest, FullOutput) {
  std::string error_msg;
  ASSERT_TRUE(Exec({"-o", "/dev/null", dex_file_}, &error_msg)) << error_msg;
}

TEST_F(DexListTest, MethodOutput) {
  std::string error_msg;
  ASSERT_TRUE(Exec({"-o", "/dev/null", "-m", "java.lang.Object.toString",
    dex_file_}, &error_msg)) << error_msg;
}

}  // namespace art
