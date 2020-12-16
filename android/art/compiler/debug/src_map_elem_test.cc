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

#include <gtest/gtest.h>

#include "src_map_elem.h"

#include "base/macros.h"

namespace art {
namespace debug {

TEST(SrcMapElem, Operators) {
  SrcMapElem elems[] = {
      { 1u, -1 },
      { 1u, 0 },
      { 1u, 1 },
      { 2u, -1 },
      { 2u, 0 },    // Index 4.
      { 2u, 1 },
      { 2u, 0u },   // Index 6: Arbitrarily add identical SrcMapElem with index 4.
  };

  for (size_t i = 0; i != arraysize(elems); ++i) {
    for (size_t j = 0; j != arraysize(elems); ++j) {
      bool expected = (i != 6u ? i : 4u) == (j != 6u ? j : 4u);
      EXPECT_EQ(expected, elems[i] == elems[j]) << i << " " << j;
    }
  }

  for (size_t i = 0; i != arraysize(elems); ++i) {
    for (size_t j = 0; j != arraysize(elems); ++j) {
      bool expected = (i != 6u ? i : 4u) < (j != 6u ? j : 4u);
      EXPECT_EQ(expected, elems[i] < elems[j]) << i << " " << j;
    }
  }
}

}  // namespace debug
}  // namespace art
