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

#ifndef ART_COMPILER_DEBUG_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_
#define ART_COMPILER_DEBUG_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_

#include "base/bit_utils.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/register.h"
#include "debug/dwarf/writer.h"

namespace art {
namespace dwarf {

// Writer for .debug_frame opcodes (DWARF-3).
// See the DWARF specification for the precise meaning of the opcodes.
// The writer is very light-weight, however it will do the following for you:
//  * Choose the most compact encoding of a given opcode.
//  * Keep track of current state and convert absolute values to deltas.
//  * Divide by header-defined factors as appropriate.
template<typename Vector = std::vector<uint8_t> >
class DebugFrameOpCodeWriter : private Writer<Vector> {
  static_assert(std::is_same<typename Vector::value_type, uint8_t>::value, "Invalid value type");

 public:
  // To save space, DWARF divides most offsets by header-defined factors.
  // They are used in integer divisions, so we make them constants.
  // We usually subtract from stack base pointer, so making the factor
  // negative makes the encoded values positive and thus easier to encode.
  static constexpr int kDataAlignmentFactor = -4;
  static constexpr int kCodeAlignmentFactor = 1;

  // Explicitely advance the program counter to given location.
  void ALWAYS_INLINE AdvancePC(int absolute_pc) {
    DCHECK_GE(absolute_pc, current_pc_);
    if (UNLIKELY(enabled_)) {
      int delta = FactorCodeOffset(absolute_pc - current_pc_);
      if (delta != 0) {
        if (delta <= 0x3F) {
          this->PushUint8(DW_CFA_advance_loc | delta);
        } else if (delta <= UINT8_MAX) {
          this->PushUint8(DW_CFA_advance_loc1);
          this->PushUint8(delta);
        } else if (delta <= UINT16_MAX) {
          this->PushUint8(DW_CFA_advance_loc2);
          this->PushUint16(delta);
        } else {
          this->PushUint8(DW_CFA_advance_loc4);
          this->PushUint32(delta);
        }
      }
      current_pc_ = absolute_pc;
    }
  }

  // Override this method to automatically advance the PC before each opcode.
  virtual void ImplicitlyAdvancePC() { }

  // Common alias in assemblers - spill relative to current stack pointer.
  void ALWAYS_INLINE RelOffset(Reg reg, int offset) {
    Offset(reg, offset - current_cfa_offset_);
  }

  // Common alias in assemblers - increase stack frame size.
  void ALWAYS_INLINE AdjustCFAOffset(int delta) {
    DefCFAOffset(current_cfa_offset_ + delta);
  }

  // Custom alias - spill many registers based on bitmask.
  void ALWAYS_INLINE RelOffsetForMany(Reg reg_base, int offset,
                                      uint32_t reg_mask, int reg_size) {
    DCHECK(reg_size == 4 || reg_size == 8);
    if (UNLIKELY(enabled_)) {
      for (int i = 0; reg_mask != 0u; reg_mask >>= 1, i++) {
        // Skip zero bits and go to the set bit.
        int num_zeros = CTZ(reg_mask);
        i += num_zeros;
        reg_mask >>= num_zeros;
        RelOffset(Reg(reg_base.num() + i), offset);
        offset += reg_size;
      }
    }
  }

  // Custom alias - unspill many registers based on bitmask.
  void ALWAYS_INLINE RestoreMany(Reg reg_base, uint32_t reg_mask) {
    if (UNLIKELY(enabled_)) {
      for (int i = 0; reg_mask != 0u; reg_mask >>= 1, i++) {
        // Skip zero bits and go to the set bit.
        int num_zeros = CTZ(reg_mask);
        i += num_zeros;
        reg_mask >>= num_zeros;
        Restore(Reg(reg_base.num() + i));
      }
    }
  }

  void ALWAYS_INLINE Nop() {
    if (UNLIKELY(enabled_)) {
      this->PushUint8(DW_CFA_nop);
    }
  }

  void ALWAYS_INLINE Offset(Reg reg, int offset) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      int factored_offset = FactorDataOffset(offset);  // May change sign.
      if (factored_offset >= 0) {
        if (0 <= reg.num() && reg.num() <= 0x3F) {
          this->PushUint8(DW_CFA_offset | reg.num());
          this->PushUleb128(factored_offset);
        } else {
          this->PushUint8(DW_CFA_offset_extended);
          this->PushUleb128(reg.num());
          this->PushUleb128(factored_offset);
        }
      } else {
        uses_dwarf3_features_ = true;
        this->PushUint8(DW_CFA_offset_extended_sf);
        this->PushUleb128(reg.num());
        this->PushSleb128(factored_offset);
      }
    }
  }

  void ALWAYS_INLINE Restore(Reg reg) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      if (0 <= reg.num() && reg.num() <= 0x3F) {
        this->PushUint8(DW_CFA_restore | reg.num());
      } else {
        this->PushUint8(DW_CFA_restore_extended);
        this->PushUleb128(reg.num());
      }
    }
  }

  void ALWAYS_INLINE Undefined(Reg reg) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_undefined);
      this->PushUleb128(reg.num());
    }
  }

  void ALWAYS_INLINE SameValue(Reg reg) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_same_value);
      this->PushUleb128(reg.num());
    }
  }

  // The previous value of "reg" is stored in register "new_reg".
  void ALWAYS_INLINE Register(Reg reg, Reg new_reg) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_register);
      this->PushUleb128(reg.num());
      this->PushUleb128(new_reg.num());
    }
  }

  void ALWAYS_INLINE RememberState() {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_remember_state);
    }
  }

  void ALWAYS_INLINE RestoreState() {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_restore_state);
    }
  }

  void ALWAYS_INLINE DefCFA(Reg reg, int offset) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      if (offset >= 0) {
        this->PushUint8(DW_CFA_def_cfa);
        this->PushUleb128(reg.num());
        this->PushUleb128(offset);  // Non-factored.
      } else {
        uses_dwarf3_features_ = true;
        this->PushUint8(DW_CFA_def_cfa_sf);
        this->PushUleb128(reg.num());
        this->PushSleb128(FactorDataOffset(offset));
      }
    }
    current_cfa_offset_ = offset;
  }

  void ALWAYS_INLINE DefCFARegister(Reg reg) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      this->PushUint8(DW_CFA_def_cfa_register);
      this->PushUleb128(reg.num());
    }
  }

  void ALWAYS_INLINE DefCFAOffset(int offset) {
    if (UNLIKELY(enabled_)) {
      if (current_cfa_offset_ != offset) {
        ImplicitlyAdvancePC();
        if (offset >= 0) {
          this->PushUint8(DW_CFA_def_cfa_offset);
          this->PushUleb128(offset);  // Non-factored.
        } else {
          uses_dwarf3_features_ = true;
          this->PushUint8(DW_CFA_def_cfa_offset_sf);
          this->PushSleb128(FactorDataOffset(offset));
        }
      }
    }
    // Uncoditional so that the user can still get and check the value.
    current_cfa_offset_ = offset;
  }

  void ALWAYS_INLINE ValOffset(Reg reg, int offset) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      uses_dwarf3_features_ = true;
      int factored_offset = FactorDataOffset(offset);  // May change sign.
      if (factored_offset >= 0) {
        this->PushUint8(DW_CFA_val_offset);
        this->PushUleb128(reg.num());
        this->PushUleb128(factored_offset);
      } else {
        this->PushUint8(DW_CFA_val_offset_sf);
        this->PushUleb128(reg.num());
        this->PushSleb128(factored_offset);
      }
    }
  }

  void ALWAYS_INLINE DefCFAExpression(uint8_t* expr, int expr_size) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      uses_dwarf3_features_ = true;
      this->PushUint8(DW_CFA_def_cfa_expression);
      this->PushUleb128(expr_size);
      this->PushData(expr, expr_size);
    }
  }

  void ALWAYS_INLINE Expression(Reg reg, uint8_t* expr, int expr_size) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      uses_dwarf3_features_ = true;
      this->PushUint8(DW_CFA_expression);
      this->PushUleb128(reg.num());
      this->PushUleb128(expr_size);
      this->PushData(expr, expr_size);
    }
  }

  void ALWAYS_INLINE ValExpression(Reg reg, uint8_t* expr, int expr_size) {
    if (UNLIKELY(enabled_)) {
      ImplicitlyAdvancePC();
      uses_dwarf3_features_ = true;
      this->PushUint8(DW_CFA_val_expression);
      this->PushUleb128(reg.num());
      this->PushUleb128(expr_size);
      this->PushData(expr, expr_size);
    }
  }

  bool IsEnabled() const { return enabled_; }

  void SetEnabled(bool value) {
    enabled_ = value;
    if (enabled_ && opcodes_.capacity() == 0u) {
      opcodes_.reserve(kDefaultCapacity);
    }
  }

  int GetCurrentPC() const { return current_pc_; }

  int GetCurrentCFAOffset() const { return current_cfa_offset_; }

  void SetCurrentCFAOffset(int offset) { current_cfa_offset_ = offset; }

  using Writer<Vector>::data;

  explicit DebugFrameOpCodeWriter(bool enabled = true,
                                  const typename Vector::allocator_type& alloc =
                                      typename Vector::allocator_type())
      : Writer<Vector>(&opcodes_),
        enabled_(false),
        opcodes_(alloc),
        current_cfa_offset_(0),
        current_pc_(0),
        uses_dwarf3_features_(false) {
    SetEnabled(enabled);
  }

  virtual ~DebugFrameOpCodeWriter() { }

 protected:
  // Best guess based on couple of observed outputs.
  static constexpr size_t kDefaultCapacity = 32u;

  int FactorDataOffset(int offset) const {
    DCHECK_EQ(offset % kDataAlignmentFactor, 0);
    return offset / kDataAlignmentFactor;
  }

  int FactorCodeOffset(int offset) const {
    DCHECK_EQ(offset % kCodeAlignmentFactor, 0);
    return offset / kCodeAlignmentFactor;
  }

  bool enabled_;  // If disabled all writes are no-ops.
  Vector opcodes_;
  int current_cfa_offset_;
  int current_pc_;
  bool uses_dwarf3_features_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugFrameOpCodeWriter);
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_DEBUG_FRAME_OPCODE_WRITER_H_
