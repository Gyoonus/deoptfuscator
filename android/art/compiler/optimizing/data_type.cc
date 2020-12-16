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

#include "data_type.h"

namespace art {

static const char* kTypeNames[] = {
    "Reference",
    "Bool",
    "Uint8",
    "Int8",
    "Uint16",
    "Int16",
    "Uint32",
    "Int32",
    "Uint64",
    "Int64",
    "Float32",
    "Float64",
    "Void",
};

const char* DataType::PrettyDescriptor(Type type) {
  static_assert(arraysize(kTypeNames) == static_cast<size_t>(Type::kLast) + 1,
                "Missing element");
  uint32_t uint_type = static_cast<uint32_t>(type);
  CHECK_LE(uint_type, static_cast<uint32_t>(Type::kLast));
  return kTypeNames[uint_type];
}

std::ostream& operator<<(std::ostream& os, DataType::Type type) {
  uint32_t uint_type = static_cast<uint32_t>(type);
  if (uint_type <= static_cast<uint32_t>(DataType::Type::kLast)) {
    os << kTypeNames[uint_type];
  } else {
    os << "Type[" << uint_type << "]";
  }
  return os;
}

}  // namespace art
