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

#ifndef ART_DISASSEMBLER_DISASSEMBLER_X86_H_
#define ART_DISASSEMBLER_DISASSEMBLER_X86_H_

#include "disassembler.h"

namespace art {
namespace x86 {

enum RegFile { GPR, MMX, SSE };

class DisassemblerX86 FINAL : public Disassembler {
 public:
  DisassemblerX86(DisassemblerOptions* options, bool supports_rex)
      : Disassembler(options), supports_rex_(supports_rex) {}

  size_t Dump(std::ostream& os, const uint8_t* begin) OVERRIDE;
  void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) OVERRIDE;

 private:
  size_t DumpNops(std::ostream& os, const uint8_t* instr);
  size_t DumpInstruction(std::ostream& os, const uint8_t* instr);

  std::string DumpAddress(uint8_t mod, uint8_t rm, uint8_t rex64, uint8_t rex_w, bool no_ops,
                          bool byte_operand, bool byte_second_operand, uint8_t* prefix, bool load,
                          RegFile src_reg_file, RegFile dst_reg_file, const uint8_t** instr,
                          uint32_t* address_bits);

  const bool supports_rex_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerX86);
};

}  // namespace x86
}  // namespace art

#endif  // ART_DISASSEMBLER_DISASSEMBLER_X86_H_
