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

#ifndef ART_COMPILER_DEBUG_DWARF_DWARF_TEST_H_
#define ART_COMPILER_DEBUG_DWARF_DWARF_TEST_H_

#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"
#include "gtest/gtest.h"
#include "linker/elf_builder.h"
#include "linker/file_output_stream.h"

namespace art {
namespace dwarf {

#define DW_CHECK(substring) Check(substring, false, __FILE__, __LINE__)
#define DW_CHECK_NEXT(substring) Check(substring, true, __FILE__, __LINE__)

class DwarfTest : public CommonRuntimeTest {
 public:
  static constexpr bool kPrintObjdumpOutput = false;  // debugging.

  struct ExpectedLine {
    std::string substring;
    bool next;
    const char* at_file;
    int at_line;
  };

  // Check that the objdump output contains given output.
  // If next is true, it must be the next line.  Otherwise lines are skipped.
  void Check(const char* substr, bool next, const char* at_file, int at_line) {
    expected_lines_.push_back(ExpectedLine {substr, next, at_file, at_line});
  }

  // Pretty-print the generated DWARF data using objdump.
  template<typename ElfTypes>
  std::vector<std::string> Objdump(const char* args) {
    // Write simple elf file with just the DWARF sections.
    InstructionSet isa =
        (sizeof(typename ElfTypes::Addr) == 8) ? InstructionSet::kX86_64 : InstructionSet::kX86;
    ScratchFile file;
    linker::FileOutputStream output_stream(file.GetFile());
    linker::ElfBuilder<ElfTypes> builder(isa, nullptr, &output_stream);
    builder.Start();
    if (!debug_info_data_.empty()) {
      builder.WriteSection(".debug_info", &debug_info_data_);
    }
    if (!debug_abbrev_data_.empty()) {
      builder.WriteSection(".debug_abbrev", &debug_abbrev_data_);
    }
    if (!debug_str_data_.empty()) {
      builder.WriteSection(".debug_str", &debug_str_data_);
    }
    if (!debug_line_data_.empty()) {
      builder.WriteSection(".debug_line", &debug_line_data_);
    }
    if (!debug_frame_data_.empty()) {
      builder.WriteSection(".debug_frame", &debug_frame_data_);
    }
    builder.End();
    EXPECT_TRUE(builder.Good());

    // Read the elf file back using objdump.
    std::vector<std::string> lines;
    std::string cmd = GetAndroidHostToolsDir();
    cmd = cmd + "objdump " + args + " " + file.GetFilename() + " 2>&1";
    FILE* output = popen(cmd.data(), "r");
    char buffer[1024];
    const char* line;
    while ((line = fgets(buffer, sizeof(buffer), output)) != nullptr) {
      if (kPrintObjdumpOutput) {
        printf("%s", line);
      }
      if (line[0] != '\0' && line[0] != '\n') {
        EXPECT_TRUE(strstr(line, "objdump: Error:") == nullptr) << line;
        EXPECT_TRUE(strstr(line, "objdump: Warning:") == nullptr) << line;
        std::string str(line);
        if (str.back() == '\n') {
          str.pop_back();
        }
        lines.push_back(str);
      }
    }
    pclose(output);
    return lines;
  }

  std::vector<std::string> Objdump(bool is64bit, const char* args) {
    if (is64bit) {
      return Objdump<ElfTypes64>(args);
    } else {
      return Objdump<ElfTypes32>(args);
    }
  }

  // Compare objdump output to the recorded checks.
  void CheckObjdumpOutput(bool is64bit, const char* args) {
    std::vector<std::string> actual_lines = Objdump(is64bit, args);
    auto actual_line = actual_lines.begin();
    for (const ExpectedLine& expected_line : expected_lines_) {
      const std::string& substring = expected_line.substring;
      if (actual_line == actual_lines.end()) {
        ADD_FAILURE_AT(expected_line.at_file, expected_line.at_line) <<
            "Expected '" << substring << "'.\n" <<
            "Seen end of output.";
      } else if (expected_line.next) {
        if (actual_line->find(substring) == std::string::npos) {
          ADD_FAILURE_AT(expected_line.at_file, expected_line.at_line) <<
            "Expected '" << substring << "'.\n" <<
            "Seen '" << actual_line->data() << "'.";
        } else {
          // printf("Found '%s' in '%s'.\n", substring.data(), actual_line->data());
        }
        actual_line++;
      } else {
        bool found = false;
        for (auto it = actual_line; it < actual_lines.end(); it++) {
          if (it->find(substring) != std::string::npos) {
            actual_line = it;
            found = true;
            break;
          }
        }
        if (!found) {
          ADD_FAILURE_AT(expected_line.at_file, expected_line.at_line) <<
            "Expected '" << substring << "'.\n" <<
            "Not found anywhere in the rest of the output.";
        } else {
          // printf("Found '%s' in '%s'.\n", substring.data(), actual_line->data());
          actual_line++;
        }
      }
    }
  }

  // Buffers which are going to assembled into ELF file and passed to objdump.
  std::vector<uint8_t> debug_frame_data_;
  std::vector<uint8_t> debug_info_data_;
  std::vector<uint8_t> debug_abbrev_data_;
  std::vector<uint8_t> debug_str_data_;
  std::vector<uint8_t> debug_line_data_;

  // The expected output of objdump.
  std::vector<ExpectedLine> expected_lines_;
};

}  // namespace dwarf
}  // namespace art

#endif  // ART_COMPILER_DEBUG_DWARF_DWARF_TEST_H_
