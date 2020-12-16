/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "atomic_dex_ref_map-inl.h"

#include <memory>

#include "common_runtime_test.h"
#include "dex/dex_file-inl.h"
#include "dex/method_reference.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

class AtomicDexRefMapTest : public CommonRuntimeTest {};

TEST_F(AtomicDexRefMapTest, RunTests) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("Interfaces"));
  ASSERT_TRUE(dex != nullptr);
  using Map = AtomicDexRefMap<MethodReference, int>;
  Map map;
  int value = 123;
  // Error case: Not already inserted.
  EXPECT_FALSE(map.Get(MethodReference(dex.get(), 1), &value));
  EXPECT_FALSE(map.HaveDexFile(dex.get()));
  // Error case: Dex file not registered.
  EXPECT_TRUE(map.Insert(MethodReference(dex.get(), 1), 0, 1) == Map::kInsertResultInvalidDexFile);
  map.AddDexFile(dex.get());
  EXPECT_TRUE(map.HaveDexFile(dex.get()));
  EXPECT_GT(dex->NumMethodIds(), 10u);
  // After we have added the get should succeed but return the default value.
  EXPECT_TRUE(map.Get(MethodReference(dex.get(), 1), &value));
  EXPECT_EQ(value, 0);
  // Actually insert an item and make sure we can retreive it.
  static const int kInsertValue = 44;
  EXPECT_TRUE(map.Insert(MethodReference(dex.get(), 1), 0, kInsertValue) ==
              Map::kInsertResultSuccess);
  EXPECT_TRUE(map.Get(MethodReference(dex.get(), 1), &value));
  EXPECT_EQ(value, kInsertValue);
  static const int kInsertValue2 = 123;
  EXPECT_TRUE(map.Insert(MethodReference(dex.get(), 2), 0, kInsertValue2) ==
              Map::kInsertResultSuccess);
  EXPECT_TRUE(map.Get(MethodReference(dex.get(), 1), &value));
  EXPECT_EQ(value, kInsertValue);
  EXPECT_TRUE(map.Get(MethodReference(dex.get(), 2), &value));
  EXPECT_EQ(value, kInsertValue2);
  // Error case: Incorrect expected value for CAS.
  EXPECT_TRUE(map.Insert(MethodReference(dex.get(), 1), 0, kInsertValue + 1) ==
      Map::kInsertResultCASFailure);
  // Correctly overwrite the value and verify.
  EXPECT_TRUE(map.Insert(MethodReference(dex.get(), 1), kInsertValue, kInsertValue + 1) ==
      Map::kInsertResultSuccess);
  EXPECT_TRUE(map.Get(MethodReference(dex.get(), 1), &value));
  EXPECT_EQ(value, kInsertValue + 1);
}

}  // namespace art
