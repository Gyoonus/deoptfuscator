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

#include "cha.h"

#include "common_runtime_test.h"

namespace art {

class CHATest : public CommonRuntimeTest {};

// Mocks some methods.
#define METHOD1 (reinterpret_cast<ArtMethod*>(8u))
#define METHOD2 (reinterpret_cast<ArtMethod*>(16u))
#define METHOD3 (reinterpret_cast<ArtMethod*>(24u))

// Mocks some method headers.
#define METHOD_HEADER1 (reinterpret_cast<OatQuickMethodHeader*>(128u))
#define METHOD_HEADER2 (reinterpret_cast<OatQuickMethodHeader*>(136u))
#define METHOD_HEADER3 (reinterpret_cast<OatQuickMethodHeader*>(144u))

TEST_F(CHATest, CHACheckDependency) {
  ClassHierarchyAnalysis cha;
  MutexLock cha_mu(Thread::Current(), *Locks::cha_lock_);

  ASSERT_TRUE(cha.GetDependents(METHOD1).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD2).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());

  cha.AddDependency(METHOD1, METHOD2, METHOD_HEADER2);
  ASSERT_TRUE(cha.GetDependents(METHOD2).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
  auto dependents = cha.GetDependents(METHOD1);
  ASSERT_EQ(dependents.size(), 1u);
  ASSERT_EQ(dependents[0].first, METHOD2);
  ASSERT_EQ(dependents[0].second, METHOD_HEADER2);

  cha.AddDependency(METHOD1, METHOD3, METHOD_HEADER3);
  ASSERT_TRUE(cha.GetDependents(METHOD2).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
  dependents = cha.GetDependents(METHOD1);
  ASSERT_EQ(dependents.size(), 2u);
  ASSERT_EQ(dependents[0].first, METHOD2);
  ASSERT_EQ(dependents[0].second, METHOD_HEADER2);
  ASSERT_EQ(dependents[1].first, METHOD3);
  ASSERT_EQ(dependents[1].second, METHOD_HEADER3);

  std::unordered_set<OatQuickMethodHeader*> headers;
  headers.insert(METHOD_HEADER2);
  cha.RemoveDependentsWithMethodHeaders(headers);
  ASSERT_TRUE(cha.GetDependents(METHOD2).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
  dependents = cha.GetDependents(METHOD1);
  ASSERT_EQ(dependents.size(), 1u);
  ASSERT_EQ(dependents[0].first, METHOD3);
  ASSERT_EQ(dependents[0].second, METHOD_HEADER3);

  cha.AddDependency(METHOD2, METHOD1, METHOD_HEADER1);
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
  dependents = cha.GetDependents(METHOD1);
  ASSERT_EQ(dependents.size(), 1u);
  dependents = cha.GetDependents(METHOD2);
  ASSERT_EQ(dependents.size(), 1u);

  headers.insert(METHOD_HEADER3);
  cha.RemoveDependentsWithMethodHeaders(headers);
  ASSERT_TRUE(cha.GetDependents(METHOD1).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
  dependents = cha.GetDependents(METHOD2);
  ASSERT_EQ(dependents.size(), 1u);
  ASSERT_EQ(dependents[0].first, METHOD1);
  ASSERT_EQ(dependents[0].second, METHOD_HEADER1);

  cha.RemoveAllDependenciesFor(METHOD2);
  ASSERT_TRUE(cha.GetDependents(METHOD1).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD2).empty());
  ASSERT_TRUE(cha.GetDependents(METHOD3).empty());
}

}  // namespace art
