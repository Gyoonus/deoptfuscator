/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "utils/swap_space.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>

#include "gtest/gtest.h"

#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"

namespace art {

class SwapSpaceTest : public CommonRuntimeTest {
};

static void SwapTest(bool use_file) {
  ScratchFile scratch;
  int fd = scratch.GetFd();
  unlink(scratch.GetFilename().c_str());

  SwapSpace pool(fd, 1 * MB);
  SwapAllocator<void> alloc(use_file ? &pool : nullptr);

  SwapVector<int32_t> v(alloc);
  v.reserve(1000000);
  for (int32_t i = 0; i < 1000000; ++i) {
    v.push_back(i);
    EXPECT_EQ(i, v[i]);
  }

  SwapVector<int32_t> v2(alloc);
  v2.reserve(1000000);
  for (int32_t i = 0; i < 1000000; ++i) {
    v2.push_back(i);
    EXPECT_EQ(i, v2[i]);
  }

  SwapVector<int32_t> v3(alloc);
  v3.reserve(500000);
  for (int32_t i = 0; i < 1000000; ++i) {
    v3.push_back(i);
    EXPECT_EQ(i, v2[i]);
  }

  // Verify contents.
  for (int32_t i = 0; i < 1000000; ++i) {
    EXPECT_EQ(i, v[i]);
    EXPECT_EQ(i, v2[i]);
    EXPECT_EQ(i, v3[i]);
  }

  scratch.Close();
}

TEST_F(SwapSpaceTest, Memory) {
  SwapTest(false);
}

TEST_F(SwapSpaceTest, Swap) {
  SwapTest(true);
}

}  // namespace art
