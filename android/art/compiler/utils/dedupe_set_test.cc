/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "dedupe_set.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "base/array_ref.h"
#include "dedupe_set-inl.h"
#include "gtest/gtest.h"
#include "thread-current-inl.h"

namespace art {

class DedupeSetTestHashFunc {
 public:
  size_t operator()(const ArrayRef<const uint8_t>& array) const {
    size_t hash = 0;
    for (uint8_t c : array) {
      hash += c;
      hash += hash << 10;
      hash += hash >> 6;
    }
    return hash;
  }
};

class DedupeSetTestAlloc {
 public:
  const std::vector<uint8_t>* Copy(const ArrayRef<const uint8_t>& src) {
    return new std::vector<uint8_t>(src.begin(), src.end());
  }

  void Destroy(const std::vector<uint8_t>* key) {
    delete key;
  }
};

TEST(DedupeSetTest, Test) {
  Thread* self = Thread::Current();
  DedupeSetTestAlloc alloc;
  DedupeSet<ArrayRef<const uint8_t>,
            std::vector<uint8_t>,
            DedupeSetTestAlloc,
            size_t,
            DedupeSetTestHashFunc> deduplicator("test", alloc);
  const std::vector<uint8_t>* array1;
  {
    uint8_t raw_test1[] = { 10u, 20u, 30u, 45u };
    ArrayRef<const uint8_t> test1(raw_test1);
    array1 = deduplicator.Add(self, test1);
    ASSERT_NE(array1, nullptr);
    ASSERT_TRUE(std::equal(test1.begin(), test1.end(), array1->begin()));
  }

  const std::vector<uint8_t>* array2;
  {
    uint8_t raw_test2[] = { 10u, 20u, 30u, 45u };
    ArrayRef<const uint8_t> test2(raw_test2);
    array2 = deduplicator.Add(self, test2);
    ASSERT_EQ(array2, array1);
    ASSERT_TRUE(std::equal(test2.begin(), test2.end(), array2->begin()));
  }

  const std::vector<uint8_t>* array3;
  {
    uint8_t raw_test3[] = { 10u, 22u, 30u, 47u };
    ArrayRef<const uint8_t> test3(raw_test3);
    array3 = deduplicator.Add(self, test3);
    ASSERT_NE(array3, nullptr);
    ASSERT_NE(array3, array1);
    ASSERT_TRUE(std::equal(test3.begin(), test3.end(), array3->begin()));
  }
}

}  // namespace art
