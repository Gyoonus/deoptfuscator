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

#include "vdex_file.h"

#include <string>

#include <gtest/gtest.h>

#include "common_runtime_test.h"

namespace art {

class VdexFileTest : public CommonRuntimeTest {
};

TEST_F(VdexFileTest, OpenEmptyVdex) {
  // Verify we fail to open an empty vdex file.
  ScratchFile tmp;
  std::string error_msg;
  std::unique_ptr<VdexFile> vdex = VdexFile::Open(tmp.GetFd(),
                                                  0,
                                                  tmp.GetFilename(),
                                                  /*writable*/false,
                                                  /*low_4gb*/false,
                                                  /*quicken*/false,
                                                  &error_msg);
  EXPECT_TRUE(vdex == nullptr);

  vdex = VdexFile::Open(
      tmp.GetFilename(), /*writable*/false, /*low_4gb*/false, /*quicken*/ false, &error_msg);
  EXPECT_TRUE(vdex == nullptr);
}

}  // namespace art
