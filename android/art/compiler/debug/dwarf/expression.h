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

#ifndef ART_COMPILER_DEBUG_DWARF_EXPRESSION_H_
#define ART_COMPILER_DEBUG_DWARF_EXPRESSION_H_

#include <cstddef>
#include <cstdint>

#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/writer.h"

namespace art {
namespace dwarf {

// Writer for DWARF expressions which are used in .debug_info and .debug_loc sections.
// See the DWARF specification for the precise meaning of the opcodes.
// If multiple equivalent encodings are possible, it will choose the most compact one.
// The writer is not exhaustive - it only implements opcodes we have needed so far.
class Expression : private Writer<> {
 public:
  using Writer<>::data;
  using Writer<>::size;

  // Push signed integer on the stack.
  void WriteOpConsts(int32_t value) {
    if (0 <= value && value < 32) {
      PushUint8(DW_OP_lit0 + value);
    } else {
      PushUint8(DW_OP_consts);
      PushSleb128(value);
    }
  }

  // Push unsigned integer on the stack.
  void WriteOpConstu(uint32_t value) {
    if (value < 32) {
      PushUint8(DW_OP_lit0 + value);
    } else {
      PushUint8(DW_OP_constu);
      PushUleb128(value);
    }
  }

  // Variable is stored in given register.
  void WriteOpReg(uint32_t dwarf_reg_num) {
    if (dwarf_reg_num < 32) {
      PushUint8(DW_OP_reg0 + dwarf_reg_num);
    } else {
      PushUint8(DW_OP_regx);
      PushUleb128(dwarf_reg_num);
    }
  }

  // Variable is stored on stack.  Also see DW_AT_frame_base.
  void WriteOpFbreg(int32_t stack_offset) {
    PushUint8(DW_OP_fbreg);
    PushSleb128(stack_offset);
  }

  // The variable is stored in multiple locations (pieces).
  void WriteOpPiece(uint32_t num_bytes) {
    PushUint8(DW_OP_piece);
    PushUleb128(num_bytes);
  }

  // Loads 32-bit or 64-bit value depending on architecture.
  void WriteOpDeref() { PushUint8(DW_OP_deref); }

  // Loads value of given byte size.
  void WriteOpDerefSize(uint8_t num_bytes) {
    PushUint8(DW_OP_deref_size);
    PushUint8(num_bytes);
  }

  // Pop two values and push their sum.
  void WriteOpPlus() { PushUint8(DW_OP_plus); }

  // Add constant value to value on top of stack.
  void WriteOpPlusUconst(uint32_t offset) {
    PushUint8(DW_OP_plus_uconst);
    PushUleb128(offset);
  }

  // Negate top of stack.
  void WriteOpNeg() { PushUint8(DW_OP_neg); }

  // Pop two values and push their bitwise-AND.
  void WriteOpAnd() { PushUint8(DW_OP_and); }

  // Push stack base pointer as determined from .debug_frame.
  void WriteOpCallFrameCfa() { PushUint8(DW_OP_call_frame_cfa); }

  // Push address of the variable we are working with.
  void WriteOpPushObjectAddress() { PushUint8(DW_OP_push_object_address); }

  // Return the top stack as the value of the variable.
  // Otherwise, the top of stack is the variable's location.
  void WriteOpStackValue() { PushUint8(DW_OP_stack_value); }

  explicit Expression(std::vector<uint8_t>* buffer) : Writer<>(buffer) {
    buffer->clear();
  }
};
}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_EXPRESSION_H_
