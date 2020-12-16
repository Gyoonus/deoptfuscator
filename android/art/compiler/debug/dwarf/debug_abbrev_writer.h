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

#ifndef ART_COMPILER_DEBUG_DWARF_DEBUG_ABBREV_WRITER_H_
#define ART_COMPILER_DEBUG_DWARF_DEBUG_ABBREV_WRITER_H_

#include <cstdint>
#include <type_traits>
#include <unordered_map>

#include "base/casts.h"
#include "base/leb128.h"
#include "base/stl_util.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/writer.h"

namespace art {
namespace dwarf {

// Writer for the .debug_abbrev.
//
// Abbreviations specify the format of entries in .debug_info.
// Each entry specifies abbreviation code, which in turns
// determines all the attributes and their format.
// It is possible to think of them as type definitions.
template <typename Vector = std::vector<uint8_t>>
class DebugAbbrevWriter FINAL : private Writer<Vector> {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");

 public:
  explicit DebugAbbrevWriter(Vector* buffer)
      : Writer<Vector>(buffer),
        current_abbrev_(buffer->get_allocator()) {
    this->PushUint8(0);  // Add abbrev table terminator.
  }

  // Start abbreviation declaration.
  void StartAbbrev(Tag tag) {
    DCHECK(current_abbrev_.empty());
    EncodeUnsignedLeb128(&current_abbrev_, tag);
    has_children_offset_ = current_abbrev_.size();
    current_abbrev_.push_back(0);  // Place-holder for DW_CHILDREN.
  }

  // Add attribute specification.
  void AddAbbrevAttribute(Attribute name, Form type) {
    EncodeUnsignedLeb128(&current_abbrev_, name);
    EncodeUnsignedLeb128(&current_abbrev_, type);
  }

  // End abbreviation declaration and return its code.
  // This will deduplicate abbreviations.
  uint32_t EndAbbrev(Children has_children) {
    DCHECK(!current_abbrev_.empty());
    current_abbrev_[has_children_offset_] = has_children;
    auto it = abbrev_codes_.insert(std::make_pair(std::move(current_abbrev_), NextAbbrevCode()));
    uint32_t abbrev_code = it.first->second;
    if (UNLIKELY(it.second)) {  // Inserted new entry.
      const Vector& abbrev = it.first->first;
      this->Pop();  // Remove abbrev table terminator.
      this->PushUleb128(abbrev_code);
      this->PushData(abbrev.data(), abbrev.size());
      this->PushUint8(0);  // Attribute list end.
      this->PushUint8(0);  // Attribute list end.
      this->PushUint8(0);  // Add abbrev table terminator.
    }
    current_abbrev_.clear();
    return abbrev_code;
  }

  // Get the next free abbrev code.
  uint32_t NextAbbrevCode() {
    return dchecked_integral_cast<uint32_t>(1 + abbrev_codes_.size());
  }

 private:
  Vector current_abbrev_;
  size_t has_children_offset_ = 0;
  std::unordered_map<Vector, uint32_t, FNVHash<Vector> > abbrev_codes_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_DEBUG_ABBREV_WRITER_H_
