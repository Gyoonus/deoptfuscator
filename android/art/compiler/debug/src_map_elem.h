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

#ifndef ART_COMPILER_DEBUG_SRC_MAP_ELEM_H_
#define ART_COMPILER_DEBUG_SRC_MAP_ELEM_H_

#include <stdint.h>

namespace art {

class SrcMapElem {
 public:
  uint32_t from_;
  int32_t to_;
};

inline bool operator<(const SrcMapElem& lhs, const SrcMapElem& rhs) {
  if (lhs.from_ != rhs.from_) {
    return lhs.from_ < rhs.from_;
  }
  return lhs.to_ < rhs.to_;
}

inline bool operator==(const SrcMapElem& lhs, const SrcMapElem& rhs) {
  return lhs.from_ == rhs.from_ && lhs.to_ == rhs.to_;
}

}  // namespace art

#endif  // ART_COMPILER_DEBUG_SRC_MAP_ELEM_H_
