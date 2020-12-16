/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_COMPILER_DEBUG_DWARF_DEBUG_LINE_OPCODE_WRITER_H_
#define ART_COMPILER_DEBUG_DWARF_DEBUG_LINE_OPCODE_WRITER_H_

#include <cstdint>

#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/writer.h"

namespace art {
namespace dwarf {

// Writer for the .debug_line opcodes (DWARF-3).
// The writer is very light-weight, however it will do the following for you:
//  * Choose the most compact encoding of a given opcode.
//  * Keep track of current state and convert absolute values to deltas.
//  * Divide by header-defined factors as appropriate.
template<typename Vector = std::vector<uint8_t>>
class DebugLineOpCodeWriter FINAL : private Writer<Vector> {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");

 public:
  static constexpr int kOpcodeBase = 13;
  static constexpr bool kDefaultIsStmt = false;
  static constexpr int kLineBase = -5;
  static constexpr int kLineRange = 14;

  void AddRow() {
    this->PushUint8(DW_LNS_copy);
  }

  void AdvancePC(uint64_t absolute_address) {
    DCHECK_NE(current_address_, 0u);  // Use SetAddress for the first advance.
    DCHECK_GE(absolute_address, current_address_);
    if (absolute_address != current_address_) {
      uint64_t delta = FactorCodeOffset(absolute_address - current_address_);
      if (delta <= INT32_MAX) {
        this->PushUint8(DW_LNS_advance_pc);
        this->PushUleb128(static_cast<int>(delta));
        current_address_ = absolute_address;
      } else {
        SetAddress(absolute_address);
      }
    }
  }

  void AdvanceLine(int absolute_line) {
    int delta = absolute_line - current_line_;
    if (delta != 0) {
      this->PushUint8(DW_LNS_advance_line);
      this->PushSleb128(delta);
      current_line_ = absolute_line;
    }
  }

  void SetFile(int file) {
    if (current_file_ != file) {
      this->PushUint8(DW_LNS_set_file);
      this->PushUleb128(file);
      current_file_ = file;
    }
  }

  void SetColumn(int column) {
    this->PushUint8(DW_LNS_set_column);
    this->PushUleb128(column);
  }

  void SetIsStmt(bool is_stmt) {
    if (is_stmt_ != is_stmt) {
      this->PushUint8(DW_LNS_negate_stmt);
      is_stmt_ = is_stmt;
    }
  }

  void SetBasicBlock() {
    this->PushUint8(DW_LNS_set_basic_block);
  }

  void SetPrologueEnd() {
    uses_dwarf3_features_ = true;
    this->PushUint8(DW_LNS_set_prologue_end);
  }

  void SetEpilogueBegin() {
    uses_dwarf3_features_ = true;
    this->PushUint8(DW_LNS_set_epilogue_begin);
  }

  void SetISA(int isa) {
    uses_dwarf3_features_ = true;
    this->PushUint8(DW_LNS_set_isa);
    this->PushUleb128(isa);
  }

  void EndSequence() {
    this->PushUint8(0);
    this->PushUleb128(1);
    this->PushUint8(DW_LNE_end_sequence);
    current_address_ = 0;
    current_file_ = 1;
    current_line_ = 1;
    is_stmt_ = kDefaultIsStmt;
  }

  // Uncoditionally set address using the long encoding.
  // This gives the linker opportunity to relocate the address.
  void SetAddress(uint64_t absolute_address) {
    DCHECK_GE(absolute_address, current_address_);
    FactorCodeOffset(absolute_address);  // Check if it is factorable.
    this->PushUint8(0);
    if (use_64bit_address_) {
      this->PushUleb128(1 + 8);
      this->PushUint8(DW_LNE_set_address);
      patch_locations_.push_back(this->data()->size());
      this->PushUint64(absolute_address);
    } else {
      this->PushUleb128(1 + 4);
      this->PushUint8(DW_LNE_set_address);
      patch_locations_.push_back(this->data()->size());
      this->PushUint32(absolute_address);
    }
    current_address_ = absolute_address;
  }

  void DefineFile(const char* filename,
                  int directory_index,
                  int modification_time,
                  int file_size) {
    int size = 1 +
               strlen(filename) + 1 +
               UnsignedLeb128Size(directory_index) +
               UnsignedLeb128Size(modification_time) +
               UnsignedLeb128Size(file_size);
    this->PushUint8(0);
    this->PushUleb128(size);
    size_t start = data()->size();
    this->PushUint8(DW_LNE_define_file);
    this->PushString(filename);
    this->PushUleb128(directory_index);
    this->PushUleb128(modification_time);
    this->PushUleb128(file_size);
    DCHECK_EQ(start + size, data()->size());
  }

  // Compact address and line opcode.
  void AddRow(uint64_t absolute_address, int absolute_line) {
    DCHECK_GE(absolute_address, current_address_);

    // If the address is definitely too far, use the long encoding.
    uint64_t delta_address = FactorCodeOffset(absolute_address - current_address_);
    if (delta_address > UINT8_MAX) {
      AdvancePC(absolute_address);
      delta_address = 0;
    }

    // If the line is definitely too far, use the long encoding.
    int delta_line = absolute_line - current_line_;
    if (!(kLineBase <= delta_line && delta_line < kLineBase + kLineRange)) {
      AdvanceLine(absolute_line);
      delta_line = 0;
    }

    // Both address and line should be reasonable now.  Use the short encoding.
    int opcode = kOpcodeBase + (delta_line - kLineBase) +
                 (static_cast<int>(delta_address) * kLineRange);
    if (opcode > UINT8_MAX) {
      // If the address is still too far, try to increment it by const amount.
      int const_advance = (0xFF - kOpcodeBase) / kLineRange;
      opcode -= (kLineRange * const_advance);
      if (opcode <= UINT8_MAX) {
        this->PushUint8(DW_LNS_const_add_pc);
      } else {
        // Give up and use long encoding for address.
        AdvancePC(absolute_address);
        // Still use the opcode to do line advance and copy.
        opcode = kOpcodeBase + (delta_line - kLineBase);
      }
    }
    DCHECK(kOpcodeBase <= opcode && opcode <= 0xFF);
    this->PushUint8(opcode);  // Special opcode.
    current_line_ = absolute_line;
    current_address_ = absolute_address;
  }

  int GetCodeFactorBits() const {
    return code_factor_bits_;
  }

  uint64_t CurrentAddress() const {
    return current_address_;
  }

  int CurrentFile() const {
    return current_file_;
  }

  int CurrentLine() const {
    return current_line_;
  }

  const std::vector<uintptr_t>& GetPatchLocations() const {
    return patch_locations_;
  }

  using Writer<Vector>::data;

  DebugLineOpCodeWriter(bool use64bitAddress,
                        int codeFactorBits,
                        const typename Vector::allocator_type& alloc =
                            typename Vector::allocator_type())
      : Writer<Vector>(&opcodes_),
        opcodes_(alloc),
        uses_dwarf3_features_(false),
        use_64bit_address_(use64bitAddress),
        code_factor_bits_(codeFactorBits),
        current_address_(0),
        current_file_(1),
        current_line_(1),
        is_stmt_(kDefaultIsStmt) {
  }

 private:
  uint64_t FactorCodeOffset(uint64_t offset) const {
    DCHECK_GE(code_factor_bits_, 0);
    DCHECK_EQ((offset >> code_factor_bits_) << code_factor_bits_, offset);
    return offset >> code_factor_bits_;
  }

  Vector opcodes_;
  bool uses_dwarf3_features_;
  bool use_64bit_address_;
  int code_factor_bits_;
  uint64_t current_address_;
  int current_file_;
  int current_line_;
  bool is_stmt_;
  std::vector<uintptr_t> patch_locations_;

  DISALLOW_COPY_AND_ASSIGN(DebugLineOpCodeWriter);
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_DEBUG_LINE_OPCODE_WRITER_H_
