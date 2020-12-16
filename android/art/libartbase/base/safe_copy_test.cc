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

#include "safe_copy.h"

#include "common_runtime_test.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/user.h>

#include "globals.h"

namespace art {

#if defined(__linux__)

TEST(SafeCopyTest, smoke) {
  DCHECK_EQ(kPageSize, static_cast<decltype(kPageSize)>(PAGE_SIZE));

  // Map four pages, mark the second one as PROT_NONE, unmap the last one.
  void* map = mmap(nullptr, kPageSize * 4, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, map);
  char* page1 = static_cast<char*>(map);
  char* page2 = page1 + kPageSize;
  char* page3 = page2 + kPageSize;
  char* page4 = page3 + kPageSize;
  ASSERT_EQ(0, mprotect(page1 + kPageSize, kPageSize, PROT_NONE));
  ASSERT_EQ(0, munmap(page4, kPageSize));

  page1[0] = 'a';
  page1[kPageSize - 1] = 'z';

  page3[0] = 'b';
  page3[kPageSize - 1] = 'y';

  char buf[kPageSize];

  // Completely valid read.
  memset(buf, 0xCC, sizeof(buf));
  EXPECT_EQ(static_cast<ssize_t>(kPageSize), SafeCopy(buf, page1, kPageSize)) << strerror(errno);
  EXPECT_EQ(0, memcmp(buf, page1, kPageSize));

  // Reading into a guard page.
  memset(buf, 0xCC, sizeof(buf));
  EXPECT_EQ(static_cast<ssize_t>(kPageSize - 1), SafeCopy(buf, page1 + 1, kPageSize));
  EXPECT_EQ(0, memcmp(buf, page1 + 1, kPageSize - 1));

  // Reading from a guard page into a real page.
  memset(buf, 0xCC, sizeof(buf));
  EXPECT_EQ(0, SafeCopy(buf, page2 + kPageSize - 1, kPageSize));

  // Reading off of the end of a mapping.
  memset(buf, 0xCC, sizeof(buf));
  EXPECT_EQ(static_cast<ssize_t>(kPageSize), SafeCopy(buf, page3, kPageSize * 2));
  EXPECT_EQ(0, memcmp(buf, page3, kPageSize));

  // Completely invalid.
  EXPECT_EQ(0, SafeCopy(buf, page1 + kPageSize, kPageSize));

  // Clean up.
  ASSERT_EQ(0, munmap(map, kPageSize * 3));
}

TEST(SafeCopyTest, alignment) {
  DCHECK_EQ(kPageSize, static_cast<decltype(kPageSize)>(PAGE_SIZE));

  // Copy the middle of a mapping to the end of another one.
  void* src_map = mmap(nullptr, kPageSize * 3, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, src_map);

  // Add a guard page to make sure we don't write past the end of the mapping.
  void* dst_map = mmap(nullptr, kPageSize * 4, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, dst_map);

  char* src = static_cast<char*>(src_map);
  char* dst = static_cast<char*>(dst_map);
  ASSERT_EQ(0, mprotect(dst + 3 * kPageSize, kPageSize, PROT_NONE));

  src[512] = 'a';
  src[kPageSize * 3 - 512 - 1] = 'z';

  EXPECT_EQ(static_cast<ssize_t>(kPageSize * 3 - 1024),
            SafeCopy(dst + 1024, src + 512, kPageSize * 3 - 1024));
  EXPECT_EQ(0, memcmp(dst + 1024, src + 512, kPageSize * 3 - 1024));

  ASSERT_EQ(0, munmap(src_map, kPageSize * 3));
  ASSERT_EQ(0, munmap(dst_map, kPageSize * 4));
}

#endif  // defined(__linux__)

}  // namespace art
