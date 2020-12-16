/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "disassembler_arm.h"

#include <memory>
#include <string>

#include "android-base/logging.h"

#include "arch/arm/registers_arm.h"
#include "base/bit_utils.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "aarch32/disasm-aarch32.h"
#include "aarch32/instructions-aarch32.h"
#pragma GCC diagnostic pop

namespace art {
namespace arm {

using vixl::aarch32::MemOperand;
using vixl::aarch32::PrintDisassembler;
using vixl::aarch32::pc;

static const vixl::aarch32::Register tr(TR);

class DisassemblerArm::CustomDisassembler FINAL : public PrintDisassembler {
  class CustomDisassemblerStream FINAL : public DisassemblerStream {
   public:
    CustomDisassemblerStream(std::ostream& os,
                             const CustomDisassembler* disasm,
                             const DisassemblerOptions* options)
        : DisassemblerStream(os), disasm_(disasm), options_(options) {}

    DisassemblerStream& operator<<(const PrintLabel& label) OVERRIDE {
      const LocationType type = label.GetLocationType();

      switch (type) {
        case kLoadByteLocation:
        case kLoadHalfWordLocation:
        case kLoadWordLocation:
        case kLoadDoubleWordLocation:
        case kLoadSignedByteLocation:
        case kLoadSignedHalfWordLocation:
        case kLoadSinglePrecisionLocation:
        case kLoadDoublePrecisionLocation:
        case kVld1Location:
        case kVld2Location:
        case kVld3Location:
        case kVld4Location: {
          const int32_t offset = label.GetImmediate();
          os() << "[pc, #" << offset << "]";
          PrintLiteral(type, offset);
          return *this;
        }
        default:
          return DisassemblerStream::operator<<(label);
      }
    }

    DisassemblerStream& operator<<(vixl::aarch32::Register reg) OVERRIDE {
      if (reg.Is(tr)) {
        os() << "tr";
        return *this;
      } else {
        return DisassemblerStream::operator<<(reg);
      }
    }

    DisassemblerStream& operator<<(const MemOperand& operand) OVERRIDE {
      // VIXL must use a PrintLabel object whenever the base register is PC;
      // the following check verifies this invariant, and guards against bugs.
      DCHECK(!operand.GetBaseRegister().Is(pc));
      DisassemblerStream::operator<<(operand);

      if (operand.GetBaseRegister().Is(tr) && operand.IsImmediate()) {
        os() << " ; ";
        options_->thread_offset_name_function_(os(), operand.GetOffsetImmediate());
      }

      return *this;
    }

    DisassemblerStream& operator<<(const vixl::aarch32::AlignedMemOperand& operand) OVERRIDE {
      // VIXL must use a PrintLabel object whenever the base register is PC;
      // the following check verifies this invariant, and guards against bugs.
      DCHECK(!operand.GetBaseRegister().Is(pc));
      return DisassemblerStream::operator<<(operand);
    }

   private:
    void PrintLiteral(LocationType type, int32_t offset);

    const CustomDisassembler* disasm_;
    const DisassemblerOptions* options_;
  };

 public:
  CustomDisassembler(std::ostream& os, const DisassemblerOptions* options)
      : PrintDisassembler(&disassembler_stream_),
        disassembler_stream_(os, this, options),
        is_t32_(true) {}

  void PrintCodeAddress(uint32_t prog_ctr) OVERRIDE {
    os() << "0x" << std::hex << std::setw(8) << std::setfill('0') << prog_ctr << ": ";
  }

  void SetIsT32(bool is_t32) {
    is_t32_ = is_t32;
  }

  bool GetIsT32() const {
    return is_t32_;
  }

 private:
  CustomDisassemblerStream disassembler_stream_;
  // Whether T32 stream is decoded.
  bool is_t32_;
};

void DisassemblerArm::CustomDisassembler::CustomDisassemblerStream::PrintLiteral(LocationType type,
                                                                                 int32_t offset) {
  // Literal offsets are not required to be aligned, so we may need unaligned access.
  typedef const int16_t unaligned_int16_t __attribute__ ((aligned (1)));
  typedef const uint16_t unaligned_uint16_t __attribute__ ((aligned (1)));
  typedef const int32_t unaligned_int32_t __attribute__ ((aligned (1)));
  typedef const int64_t unaligned_int64_t __attribute__ ((aligned (1)));
  typedef const float unaligned_float __attribute__ ((aligned (1)));
  typedef const double unaligned_double __attribute__ ((aligned (1)));

  // Zeros are used for the LocationType values this function does not care about.
  const size_t literal_size[kVst4Location + 1] = {
      0, 0, 0, 0, sizeof(uint8_t), sizeof(unaligned_uint16_t), sizeof(unaligned_int32_t),
      sizeof(unaligned_int64_t), sizeof(int8_t), sizeof(unaligned_int16_t),
      sizeof(unaligned_float), sizeof(unaligned_double)};
  const uintptr_t begin = reinterpret_cast<uintptr_t>(options_->base_address_);
  const uintptr_t end = reinterpret_cast<uintptr_t>(options_->end_address_);
  uintptr_t literal_addr =
      RoundDown(disasm_->GetCodeAddress(), vixl::aarch32::kRegSizeInBytes) + offset;
  literal_addr += disasm_->GetIsT32() ? vixl::aarch32::kT32PcDelta : vixl::aarch32::kA32PcDelta;

  if (!options_->absolute_addresses_) {
    literal_addr += begin;
  }

  os() << "  ; ";

  // Bail out if not within expected buffer range to avoid trying to fetch invalid literals
  // (we can encounter them when interpreting raw data as instructions).
  if (literal_addr < begin || literal_addr > end - literal_size[type]) {
    os() << "(?)";
  } else {
    switch (type) {
      case kLoadByteLocation:
        os() << *reinterpret_cast<const uint8_t*>(literal_addr);
        break;
      case kLoadHalfWordLocation:
        os() << *reinterpret_cast<unaligned_uint16_t*>(literal_addr);
        break;
      case kLoadWordLocation: {
        const int32_t value = *reinterpret_cast<unaligned_int32_t*>(literal_addr);
        os() << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
        break;
      }
      case kLoadDoubleWordLocation: {
        const int64_t value = *reinterpret_cast<unaligned_int64_t*>(literal_addr);
        os() << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
        break;
      }
      case kLoadSignedByteLocation:
        os() << *reinterpret_cast<const int8_t*>(literal_addr);
        break;
      case kLoadSignedHalfWordLocation:
        os() << *reinterpret_cast<unaligned_int16_t*>(literal_addr);
        break;
      case kLoadSinglePrecisionLocation:
        os() << *reinterpret_cast<unaligned_float*>(literal_addr);
        break;
      case kLoadDoublePrecisionLocation:
        os() << *reinterpret_cast<unaligned_double*>(literal_addr);
        break;
      default:
        UNIMPLEMENTED(FATAL) << "Unexpected literal type: " << type;
    }
  }
}

DisassemblerArm::DisassemblerArm(DisassemblerOptions* options)
    : Disassembler(options), disasm_(std::make_unique<CustomDisassembler>(output_, options)) {}

size_t DisassemblerArm::Dump(std::ostream& os, const uint8_t* begin) {
  uintptr_t next;
  // Remove the Thumb specifier bit; no effect if begin does not point to T32 code.
  const uintptr_t instr_ptr = reinterpret_cast<uintptr_t>(begin) & ~1;

  const bool is_t32 = (reinterpret_cast<uintptr_t>(begin) & 1) != 0;
  disasm_->SetCodeAddress(GetPc(instr_ptr));
  disasm_->SetIsT32(is_t32);

  if (is_t32) {
    const uint16_t* const ip = reinterpret_cast<const uint16_t*>(instr_ptr);
    const uint16_t* const end_address = reinterpret_cast<const uint16_t*>(
        GetDisassemblerOptions()->end_address_);
    next = reinterpret_cast<uintptr_t>(disasm_->DecodeT32At(ip, end_address));
  } else {
    const uint32_t* const ip = reinterpret_cast<const uint32_t*>(instr_ptr);
    next = reinterpret_cast<uintptr_t>(disasm_->DecodeA32At(ip));
  }

  os << output_.str();
  output_.str(std::string());
  return next - instr_ptr;
}

void DisassemblerArm::Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) {
  DCHECK_LE(begin, end);

  // Remove the Thumb specifier bit; no effect if begin does not point to T32 code.
  const uintptr_t base = reinterpret_cast<uintptr_t>(begin) & ~1;

  const bool is_t32 = (reinterpret_cast<uintptr_t>(begin) & 1) != 0;
  disasm_->SetCodeAddress(GetPc(base));
  disasm_->SetIsT32(is_t32);

  if (is_t32) {
    // The Thumb specifier bits cancel each other.
    disasm_->DisassembleT32Buffer(reinterpret_cast<const uint16_t*>(base), end - begin);
  } else {
    disasm_->DisassembleA32Buffer(reinterpret_cast<const uint32_t*>(base), end - begin);
  }

  os << output_.str();
  output_.str(std::string());
}

}  // namespace arm
}  // namespace art
