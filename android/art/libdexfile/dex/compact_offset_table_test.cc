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

#include <vector>

#include <android-base/logging.h>
#include "dex/compact_offset_table.h"
#include "gtest/gtest.h"

namespace art {

TEST(CompactOffsetTableTest, TestBuildAndAccess) {
  const size_t kDebugInfoMinOffset = 1234567;
  std::vector<uint32_t> offsets = {
      0, 17, 2, 3, 11, 0, 0, 0, 0, 1, 0, 1552, 100, 122, 44, 1234567, 0, 0,
      std::numeric_limits<uint32_t>::max() - kDebugInfoMinOffset, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12,
  };
  // Add some large offset since the debug info section will never be that close to the beginning
  // of the file.
  for (uint32_t& offset : offsets) {
    if (offset != 0u) {
      offset += kDebugInfoMinOffset;
    }
  }

  std::vector<uint8_t> data;
  uint32_t min_offset = 0;
  uint32_t table_offset = 0;
  CompactOffsetTable::Build(offsets, /*out*/ &data, /*out*/ &min_offset, /*out*/ &table_offset);
  EXPECT_GE(min_offset, kDebugInfoMinOffset);
  EXPECT_LT(table_offset, data.size());
  ASSERT_GT(data.size(), 0u);
  const size_t before_size = offsets.size() * sizeof(offsets.front());
  EXPECT_LT(data.size(), before_size);

  // Note that the accessor requires the data to be aligned. Use memmap to accomplish this.
  std::string error_msg;
  // Leave some extra room since we don't copy the table at the start (for testing).
  constexpr size_t kExtraOffset = 4 * 128;
  std::vector<uint8_t> fake_dex(data.size() + kExtraOffset);
  std::copy(data.begin(), data.end(), fake_dex.data() + kExtraOffset);

  CompactOffsetTable::Accessor accessor(fake_dex.data() + kExtraOffset, min_offset, table_offset);
  for (size_t i = 0; i < offsets.size(); ++i) {
    EXPECT_EQ(offsets[i], accessor.GetOffset(i));
  }

  // Sort to produce a try and produce a smaller table. This happens because the leb diff is smaller
  // for sorted increasing order.
  std::sort(offsets.begin(), offsets.end());
  std::vector<uint8_t> sorted_data;
  CompactOffsetTable::Build(offsets,
                            /*out*/ &sorted_data,
                            /*out*/ &min_offset,
                            /*out*/ &table_offset);
  EXPECT_LT(sorted_data.size(), data.size());
  {
    android::base::ScopedLogSeverity sls(android::base::LogSeverity::INFO);
    LOG(INFO) << "raw size " << before_size
              << " table size " << data.size()
              << " sorted table size " << sorted_data.size();
  }

  // Test constructor and accessor that serialize/read offsets.
  {
    std::vector<uint8_t> data2;
    CompactOffsetTable::Build(offsets, /*out*/ &data2);
    CompactOffsetTable::Accessor accessor2(&data2[0]);
    for (size_t i = 0; i < offsets.size(); ++i) {
      EXPECT_EQ(offsets[i], accessor2.GetOffset(i));
    }
  }
}

}  // namespace art
