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

#ifndef ART_COMPILER_CFI_TEST_H_
#define ART_COMPILER_CFI_TEST_H_

#include <memory>
#include <sstream>
#include <vector>

#include "arch/instruction_set.h"
#include "base/enums.h"
#include "debug/dwarf/dwarf_constants.h"
#include "debug/dwarf/dwarf_test.h"
#include "debug/dwarf/headers.h"
#include "disassembler.h"
#include "gtest/gtest.h"
#include "thread.h"

namespace art {

constexpr dwarf::CFIFormat kCFIFormat = dwarf::DW_DEBUG_FRAME_FORMAT;

class CFITest : public dwarf::DwarfTest {
 public:
  void GenerateExpected(FILE* f, InstructionSet isa, const char* isa_str,
                        const std::vector<uint8_t>& actual_asm,
                        const std::vector<uint8_t>& actual_cfi) {
    std::vector<std::string> lines;
    // Print the raw bytes.
    fprintf(f, "static constexpr uint8_t expected_asm_%s[] = {", isa_str);
    HexDump(f, actual_asm);
    fprintf(f, "\n};\n");
    fprintf(f, "static constexpr uint8_t expected_cfi_%s[] = {", isa_str);
    HexDump(f, actual_cfi);
    fprintf(f, "\n};\n");
    // Pretty-print CFI opcodes.
    constexpr bool is64bit = false;
    dwarf::DebugFrameOpCodeWriter<> initial_opcodes;
    dwarf::WriteCIE(is64bit, dwarf::Reg(8),
                    initial_opcodes, kCFIFormat, &debug_frame_data_);
    std::vector<uintptr_t> debug_frame_patches;
    dwarf::WriteFDE(is64bit, 0, 0, 0, actual_asm.size(), ArrayRef<const uint8_t>(actual_cfi),
                    kCFIFormat, 0, &debug_frame_data_, &debug_frame_patches);
    ReformatCfi(Objdump(false, "-W"), &lines);
    // Pretty-print assembly.
    const uint8_t* asm_base = actual_asm.data();
    const uint8_t* asm_end = asm_base + actual_asm.size();
    auto* opts = new DisassemblerOptions(false,
                                         asm_base,
                                         asm_end,
                                         true,
                                         is64bit
                                             ? &Thread::DumpThreadOffset<PointerSize::k64>
                                             : &Thread::DumpThreadOffset<PointerSize::k32>);
    std::unique_ptr<Disassembler> disasm(Disassembler::Create(isa, opts));
    std::stringstream stream;
    const uint8_t* base = actual_asm.data() + (isa == InstructionSet::kThumb2 ? 1 : 0);
    disasm->Dump(stream, base, base + actual_asm.size());
    ReformatAsm(&stream, &lines);
    // Print CFI and assembly interleaved.
    std::stable_sort(lines.begin(), lines.end(), CompareByAddress);
    for (const std::string& line : lines) {
      fprintf(f, "// %s\n", line.c_str());
    }
    fprintf(f, "\n");
  }

 private:
  // Helper - get offset just past the end of given string.
  static size_t FindEndOf(const std::string& str, const char* substr) {
    size_t pos = str.find(substr);
    CHECK_NE(std::string::npos, pos);
    return pos + strlen(substr);
  }

  // Spit to lines and remove raw instruction bytes.
  static void ReformatAsm(std::stringstream* stream,
                          std::vector<std::string>* output) {
    std::string line;
    while (std::getline(*stream, line)) {
      line = line.substr(0, FindEndOf(line, ": ")) +
             line.substr(FindEndOf(line, "\t"));
      size_t pos;
      while ((pos = line.find("  ")) != std::string::npos) {
        line = line.replace(pos, 2, " ");
      }
      while (!line.empty() && line.back() == ' ') {
        line.pop_back();
      }
      output->push_back(line);
    }
  }

  // Find interesting parts of objdump output and prefix the lines with address.
  static void ReformatCfi(const std::vector<std::string>& lines,
                          std::vector<std::string>* output) {
    std::string address;
    for (const std::string& line : lines) {
      if (line.find("DW_CFA_nop") != std::string::npos) {
        // Ignore.
      } else if (line.find("DW_CFA_advance_loc") != std::string::npos) {
        // The last 8 characters are the address.
        address = "0x" + line.substr(line.size() - 8);
      } else if (line.find("DW_CFA_") != std::string::npos) {
        std::string new_line(line);
        // "bad register" warning is caused by always using host (x86) objdump.
        const char* bad_reg = "bad register: ";
        size_t pos;
        if ((pos = new_line.find(bad_reg)) != std::string::npos) {
          new_line = new_line.replace(pos, strlen(bad_reg), "");
        }
        // Remove register names in parentheses since they have x86 names.
        if ((pos = new_line.find(" (")) != std::string::npos) {
          new_line = new_line.replace(pos, FindEndOf(new_line, ")") - pos, "");
        }
        // Use the .cfi_ prefix.
        new_line = ".cfi_" + new_line.substr(FindEndOf(new_line, "DW_CFA_"));
        output->push_back(address + ": " + new_line);
      }
    }
  }

  // Compare strings by the address prefix.
  static bool CompareByAddress(const std::string& lhs, const std::string& rhs) {
    EXPECT_EQ(lhs[10], ':');
    EXPECT_EQ(rhs[10], ':');
    return strncmp(lhs.c_str(), rhs.c_str(), 10) < 0;
  }

  // Pretty-print byte array.  12 bytes per line.
  static void HexDump(FILE* f, const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); i++) {
      fprintf(f, i % 12 == 0 ? "\n    " : " ");  // Whitespace.
      fprintf(f, "0x%02X,", data[i]);
    }
  }
};

}  // namespace art

#endif  // ART_COMPILER_CFI_TEST_H_
