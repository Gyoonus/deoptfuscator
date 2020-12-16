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

#ifndef ART_DISASSEMBLER_DISASSEMBLER_H_
#define ART_DISASSEMBLER_DISASSEMBLER_H_

#include <stdint.h>

#include <iosfwd>

#include "android-base/macros.h"

#include "arch/instruction_set.h"

namespace art {

class DisassemblerOptions {
 public:
  using ThreadOffsetNameFunction = void (*)(std::ostream& os, uint32_t offset);

  ThreadOffsetNameFunction thread_offset_name_function_;

  // Base address for calculating relative code offsets when absolute_addresses_ is false.
  const uint8_t* const base_address_;

  // End address (exclusive);
  const uint8_t* const end_address_;

  // Should the disassembler print absolute or relative addresses.
  const bool absolute_addresses_;

  // If set, the disassembler is allowed to look at load targets in literal
  // pools.
  const bool can_read_literals_;

  DisassemblerOptions(bool absolute_addresses,
                      const uint8_t* base_address,
                      const uint8_t* end_address,
                      bool can_read_literals,
                      ThreadOffsetNameFunction fn)
      : thread_offset_name_function_(fn),
        base_address_(base_address),
        end_address_(end_address),
        absolute_addresses_(absolute_addresses),
        can_read_literals_(can_read_literals) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassemblerOptions);
};

class Disassembler {
 public:
  // Creates a Disassembler for the given InstructionSet with the
  // non-null DisassemblerOptions which become owned by the
  // Disassembler.
  static Disassembler* Create(InstructionSet instruction_set, DisassemblerOptions* options);

  virtual ~Disassembler() {
    delete disassembler_options_;
  }

  // Dump a single instruction returning the length of that instruction.
  virtual size_t Dump(std::ostream& os, const uint8_t* begin) = 0;
  // Dump instructions within a range.
  virtual void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) = 0;

  const DisassemblerOptions* GetDisassemblerOptions() const {
    return disassembler_options_;
  }

 protected:
  explicit Disassembler(DisassemblerOptions* disassembler_options);

  std::string FormatInstructionPointer(const uint8_t* begin);

 private:
  DisassemblerOptions* disassembler_options_;
  DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

static inline bool HasBitSet(uint32_t value, uint32_t bit) {
  return (value & (1 << bit)) != 0;
}

extern "C"
Disassembler* create_disassembler(InstructionSet instruction_set, DisassemblerOptions* options);

}  // namespace art

#endif  // ART_DISASSEMBLER_DISASSEMBLER_H_
