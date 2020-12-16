/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_ASSEMBLER_TEST_BASE_H_
#define ART_COMPILER_UTILS_ASSEMBLER_TEST_BASE_H_

#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include "android-base/strings.h"

#include "base/utils.h"
#include "common_runtime_test.h"  // For ScratchFile
#include "exec_utils.h"

namespace art {

// If you want to take a look at the differences between the ART assembler and GCC, set this flag
// to true. The disassembled files will then remain in the tmp directory.
static constexpr bool kKeepDisassembledFiles = false;

// Use a glocal static variable to keep the same name for all test data. Else we'll just spam the
// temp directory.
static std::string tmpnam_;  // NOLINT [runtime/string] [4]

// We put this into a class as gtests are self-contained, so this helper needs to be in an h-file.
class AssemblerTestInfrastructure {
 public:
  AssemblerTestInfrastructure(std::string architecture,
                              std::string as,
                              std::string as_params,
                              std::string objdump,
                              std::string objdump_params,
                              std::string disasm,
                              std::string disasm_params,
                              const char* asm_header) :
      architecture_string_(architecture),
      asm_header_(asm_header),
      assembler_cmd_name_(as),
      assembler_parameters_(as_params),
      objdump_cmd_name_(objdump),
      objdump_parameters_(objdump_params),
      disassembler_cmd_name_(disasm),
      disassembler_parameters_(disasm_params) {
    // Fake a runtime test for ScratchFile
    CommonRuntimeTest::SetUpAndroidData(android_data_);
  }

  virtual ~AssemblerTestInfrastructure() {
    // We leave temporaries in case this failed so we can debug issues.
    CommonRuntimeTest::TearDownAndroidData(android_data_, false);
    tmpnam_ = "";
  }

  // This is intended to be run as a test.
  bool CheckTools() {
    std::string asm_tool = FindTool(assembler_cmd_name_);
    if (!FileExists(asm_tool)) {
      LOG(ERROR) << "Could not find assembler from " << assembler_cmd_name_;
      LOG(ERROR) << "FindTool returned " << asm_tool;
      FindToolDump(assembler_cmd_name_);
      return false;
    }
    LOG(INFO) << "Chosen assembler command: " << GetAssemblerCommand();

    std::string objdump_tool = FindTool(objdump_cmd_name_);
    if (!FileExists(objdump_tool)) {
      LOG(ERROR) << "Could not find objdump from " << objdump_cmd_name_;
      LOG(ERROR) << "FindTool returned " << objdump_tool;
      FindToolDump(objdump_cmd_name_);
      return false;
    }
    LOG(INFO) << "Chosen objdump command: " << GetObjdumpCommand();

    // Disassembly is optional.
    std::string disassembler = GetDisassembleCommand();
    if (disassembler.length() != 0) {
      std::string disassembler_tool = FindTool(disassembler_cmd_name_);
      if (!FileExists(disassembler_tool)) {
        LOG(ERROR) << "Could not find disassembler from " << disassembler_cmd_name_;
        LOG(ERROR) << "FindTool returned " << disassembler_tool;
        FindToolDump(disassembler_cmd_name_);
        return false;
      }
      LOG(INFO) << "Chosen disassemble command: " << GetDisassembleCommand();
    } else {
      LOG(INFO) << "No disassembler given.";
    }

    return true;
  }

  // Driver() assembles and compares the results. If the results are not equal and we have a
  // disassembler, disassemble both and check whether they have the same mnemonics (in which case
  // we just warn).
  void Driver(const std::vector<uint8_t>& data,
              const std::string& assembly_text,
              const std::string& test_name) {
    EXPECT_NE(assembly_text.length(), 0U) << "Empty assembly";

    NativeAssemblerResult res;
    Compile(assembly_text, &res, test_name);

    EXPECT_TRUE(res.ok) << res.error_msg;
    if (!res.ok) {
      // No way of continuing.
      return;
    }

    if (data == *res.code) {
      Clean(&res);
    } else {
      if (DisassembleBinaries(data, *res.code, test_name)) {
        if (data.size() > res.code->size()) {
          // Fail this test with a fancy colored warning being printed.
          EXPECT_TRUE(false) << "Assembly code is not identical, but disassembly of machine code "
              "is equal: this implies sub-optimal encoding! Our code size=" << data.size() <<
              ", gcc size=" << res.code->size();
        } else {
          // Otherwise just print an info message and clean up.
          LOG(INFO) << "GCC chose a different encoding than ours, but the overall length is the "
              "same.";
          Clean(&res);
        }
      } else {
        // This will output the assembly.
        EXPECT_EQ(*res.code, data) << "Outputs (and disassembly) not identical.";
      }
    }
  }

 protected:
  // Return the host assembler command for this test.
  virtual std::string GetAssemblerCommand() {
    // Already resolved it once?
    if (resolved_assembler_cmd_.length() != 0) {
      return resolved_assembler_cmd_;
    }

    std::string line = FindTool(assembler_cmd_name_);
    if (line.length() == 0) {
      return line;
    }

    resolved_assembler_cmd_ = line + assembler_parameters_;

    return resolved_assembler_cmd_;
  }

  // Return the host objdump command for this test.
  virtual std::string GetObjdumpCommand() {
    // Already resolved it once?
    if (resolved_objdump_cmd_.length() != 0) {
      return resolved_objdump_cmd_;
    }

    std::string line = FindTool(objdump_cmd_name_);
    if (line.length() == 0) {
      return line;
    }

    resolved_objdump_cmd_ = line + objdump_parameters_;

    return resolved_objdump_cmd_;
  }

  // Return the host disassembler command for this test.
  virtual std::string GetDisassembleCommand() {
    // Already resolved it once?
    if (resolved_disassemble_cmd_.length() != 0) {
      return resolved_disassemble_cmd_;
    }

    std::string line = FindTool(disassembler_cmd_name_);
    if (line.length() == 0) {
      return line;
    }

    resolved_disassemble_cmd_ = line + disassembler_parameters_;

    return resolved_disassemble_cmd_;
  }

 private:
  // Structure to store intermediates and results.
  struct NativeAssemblerResult {
    bool ok;
    std::string error_msg;
    std::string base_name;
    std::unique_ptr<std::vector<uint8_t>> code;
    uintptr_t length;
  };

  // Compile the assembly file from_file to a binary file to_file. Returns true on success.
  bool Assemble(const char* from_file, const char* to_file, std::string* error_msg) {
    bool have_assembler = FileExists(FindTool(assembler_cmd_name_));
    EXPECT_TRUE(have_assembler) << "Cannot find assembler:" << GetAssemblerCommand();
    if (!have_assembler) {
      return false;
    }

    std::vector<std::string> args;

    // Encaspulate the whole command line in a single string passed to
    // the shell, so that GetAssemblerCommand() may contain arguments
    // in addition to the program name.
    args.push_back(GetAssemblerCommand());
    args.push_back("-o");
    args.push_back(to_file);
    args.push_back(from_file);
    std::string cmd = android::base::Join(args, ' ');

    args.clear();
    args.push_back("/bin/sh");
    args.push_back("-c");
    args.push_back(cmd);

    bool success = Exec(args, error_msg);
    if (!success) {
      LOG(ERROR) << "Assembler command line:";
      for (const std::string& arg : args) {
        LOG(ERROR) << arg;
      }
    }
    return success;
  }

  // Runs objdump -h on the binary file and extracts the first line with .text.
  // Returns "" on failure.
  std::string Objdump(const std::string& file) {
    bool have_objdump = FileExists(FindTool(objdump_cmd_name_));
    EXPECT_TRUE(have_objdump) << "Cannot find objdump: " << GetObjdumpCommand();
    if (!have_objdump) {
      return "";
    }

    std::string error_msg;
    std::vector<std::string> args;

    // Encaspulate the whole command line in a single string passed to
    // the shell, so that GetObjdumpCommand() may contain arguments
    // in addition to the program name.
    args.push_back(GetObjdumpCommand());
    args.push_back(file);
    args.push_back(">");
    args.push_back(file+".dump");
    std::string cmd = android::base::Join(args, ' ');

    args.clear();
    args.push_back("/bin/sh");
    args.push_back("-c");
    args.push_back(cmd);

    if (!Exec(args, &error_msg)) {
      EXPECT_TRUE(false) << error_msg;
    }

    std::ifstream dump(file+".dump");

    std::string line;
    bool found = false;
    while (std::getline(dump, line)) {
      if (line.find(".text") != line.npos) {
        found = true;
        break;
      }
    }

    dump.close();

    if (found) {
      return line;
    } else {
      return "";
    }
  }

  // Disassemble both binaries and compare the text.
  bool DisassembleBinaries(const std::vector<uint8_t>& data,
                           const std::vector<uint8_t>& as,
                           const std::string& test_name) {
    std::string disassembler = GetDisassembleCommand();
    if (disassembler.length() == 0) {
      LOG(WARNING) << "No dissassembler command.";
      return false;
    }

    std::string data_name = WriteToFile(data, test_name + ".ass");
    std::string error_msg;
    if (!DisassembleBinary(data_name, &error_msg)) {
      LOG(INFO) << "Error disassembling: " << error_msg;
      std::remove(data_name.c_str());
      return false;
    }

    std::string as_name = WriteToFile(as, test_name + ".gcc");
    if (!DisassembleBinary(as_name, &error_msg)) {
      LOG(INFO) << "Error disassembling: " << error_msg;
      std::remove(data_name.c_str());
      std::remove((data_name + ".dis").c_str());
      std::remove(as_name.c_str());
      return false;
    }

    bool result = CompareFiles(data_name + ".dis", as_name + ".dis");

    if (!kKeepDisassembledFiles) {
      std::remove(data_name.c_str());
      std::remove(as_name.c_str());
      std::remove((data_name + ".dis").c_str());
      std::remove((as_name + ".dis").c_str());
    }

    return result;
  }

  bool DisassembleBinary(const std::string& file, std::string* error_msg) {
    std::vector<std::string> args;

    // Encaspulate the whole command line in a single string passed to
    // the shell, so that GetDisassembleCommand() may contain arguments
    // in addition to the program name.
    args.push_back(GetDisassembleCommand());
    args.push_back(file);
    args.push_back("| sed -n \'/<.data>/,$p\' | sed -e \'s/.*://\'");
    args.push_back(">");
    args.push_back(file+".dis");
    std::string cmd = android::base::Join(args, ' ');

    args.clear();
    args.push_back("/bin/sh");
    args.push_back("-c");
    args.push_back(cmd);

    return Exec(args, error_msg);
  }

  std::string WriteToFile(const std::vector<uint8_t>& buffer, const std::string& test_name) {
    std::string file_name = GetTmpnam() + std::string("---") + test_name;
    const char* data = reinterpret_cast<const char*>(buffer.data());
    std::ofstream s_out(file_name + ".o");
    s_out.write(data, buffer.size());
    s_out.close();
    return file_name + ".o";
  }

  bool CompareFiles(const std::string& f1, const std::string& f2) {
    std::ifstream f1_in(f1);
    std::ifstream f2_in(f2);

    bool result = std::equal(std::istreambuf_iterator<char>(f1_in),
                             std::istreambuf_iterator<char>(),
                             std::istreambuf_iterator<char>(f2_in));

    f1_in.close();
    f2_in.close();

    return result;
  }

  // Compile the given assembly code and extract the binary, if possible. Put result into res.
  bool Compile(const std::string& assembly_code,
               NativeAssemblerResult* res,
               const std::string& test_name) {
    res->ok = false;
    res->code.reset(nullptr);

    res->base_name = GetTmpnam() + std::string("---") + test_name;

    // TODO: Lots of error checking.

    std::ofstream s_out(res->base_name + ".S");
    if (asm_header_ != nullptr) {
      s_out << asm_header_;
    }
    s_out << assembly_code;
    s_out.close();

    if (!Assemble((res->base_name + ".S").c_str(), (res->base_name + ".o").c_str(),
                  &res->error_msg)) {
      res->error_msg = "Could not compile.";
      return false;
    }

    std::string odump = Objdump(res->base_name + ".o");
    if (odump.length() == 0) {
      res->error_msg = "Objdump failed.";
      return false;
    }

    std::istringstream iss(odump);
    std::istream_iterator<std::string> start(iss);
    std::istream_iterator<std::string> end;
    std::vector<std::string> tokens(start, end);

    if (tokens.size() < OBJDUMP_SECTION_LINE_MIN_TOKENS) {
      res->error_msg = "Objdump output not recognized: too few tokens.";
      return false;
    }

    if (tokens[1] != ".text") {
      res->error_msg = "Objdump output not recognized: .text not second token.";
      return false;
    }

    std::string lengthToken = "0x" + tokens[2];
    std::istringstream(lengthToken) >> std::hex >> res->length;

    std::string offsetToken = "0x" + tokens[5];
    uintptr_t offset;
    std::istringstream(offsetToken) >> std::hex >> offset;

    std::ifstream obj(res->base_name + ".o");
    obj.seekg(offset);
    res->code.reset(new std::vector<uint8_t>(res->length));
    obj.read(reinterpret_cast<char*>(&(*res->code)[0]), res->length);
    obj.close();

    res->ok = true;
    return true;
  }

  // Remove temporary files.
  void Clean(const NativeAssemblerResult* res) {
    std::remove((res->base_name + ".S").c_str());
    std::remove((res->base_name + ".o").c_str());
    std::remove((res->base_name + ".o.dump").c_str());
  }

  // Check whether file exists. Is used for commands, so strips off any parameters: anything after
  // the first space. We skip to the last slash for this, so it should work with directories with
  // spaces.
  static bool FileExists(const std::string& file) {
    if (file.length() == 0) {
      return false;
    }

    // Need to strip any options.
    size_t last_slash = file.find_last_of('/');
    if (last_slash == std::string::npos) {
      // No slash, start looking at the start.
      last_slash = 0;
    }
    size_t space_index = file.find(' ', last_slash);

    if (space_index == std::string::npos) {
      std::ifstream infile(file.c_str());
      return infile.good();
    } else {
      std::string copy = file.substr(0, space_index - 1);

      struct stat buf;
      return stat(copy.c_str(), &buf) == 0;
    }
  }

  static std::string GetGCCRootPath() {
    return "prebuilts/gcc/linux-x86";
  }

  static std::string GetRootPath() {
    // 1) Check ANDROID_BUILD_TOP
    char* build_top = getenv("ANDROID_BUILD_TOP");
    if (build_top != nullptr) {
      return std::string(build_top) + "/";
    }

    // 2) Do cwd
    char temp[1024];
    return getcwd(temp, 1024) ? std::string(temp) + "/" : std::string("");
  }

  std::string FindTool(const std::string& tool_name) {
    // Find the current tool. Wild-card pattern is "arch-string*tool-name".
    std::string gcc_path = GetRootPath() + GetGCCRootPath();
    std::vector<std::string> args;
    args.push_back("find");
    args.push_back(gcc_path);
    args.push_back("-name");
    args.push_back(architecture_string_ + "*" + tool_name);
    args.push_back("|");
    args.push_back("sort");
    args.push_back("|");
    args.push_back("tail");
    args.push_back("-n");
    args.push_back("1");
    std::string tmp_file = GetTmpnam();
    args.push_back(">");
    args.push_back(tmp_file);
    std::string sh_args = android::base::Join(args, ' ');

    args.clear();
    args.push_back("/bin/sh");
    args.push_back("-c");
    args.push_back(sh_args);

    std::string error_msg;
    if (!Exec(args, &error_msg)) {
      EXPECT_TRUE(false) << error_msg;
      UNREACHABLE();
    }

    std::ifstream in(tmp_file.c_str());
    std::string line;
    if (!std::getline(in, line)) {
      in.close();
      std::remove(tmp_file.c_str());
      return "";
    }
    in.close();
    std::remove(tmp_file.c_str());
    return line;
  }

  // Helper for below. If name_predicate is empty, search for all files, otherwise use it for the
  // "-name" option.
  static void FindToolDumpPrintout(const std::string& name_predicate,
                                   const std::string& tmp_file) {
    std::string gcc_path = GetRootPath() + GetGCCRootPath();
    std::vector<std::string> args;
    args.push_back("find");
    args.push_back(gcc_path);
    if (!name_predicate.empty()) {
      args.push_back("-name");
      args.push_back(name_predicate);
    }
    args.push_back("|");
    args.push_back("sort");
    args.push_back(">");
    args.push_back(tmp_file);
    std::string sh_args = android::base::Join(args, ' ');

    args.clear();
    args.push_back("/bin/sh");
    args.push_back("-c");
    args.push_back(sh_args);

    std::string error_msg;
    if (!Exec(args, &error_msg)) {
      EXPECT_TRUE(false) << error_msg;
      UNREACHABLE();
    }

    LOG(ERROR) << "FindToolDump: gcc_path=" << gcc_path
               << " cmd=" << sh_args;
    std::ifstream in(tmp_file.c_str());
    if (in) {
      std::string line;
      while (std::getline(in, line)) {
        LOG(ERROR) << line;
      }
    }
    in.close();
    std::remove(tmp_file.c_str());
  }

  // For debug purposes.
  void FindToolDump(const std::string& tool_name) {
    // Check with the tool name.
    FindToolDumpPrintout(architecture_string_ + "*" + tool_name, GetTmpnam());
    FindToolDumpPrintout("", GetTmpnam());
  }

  // Use a consistent tmpnam, so store it.
  std::string GetTmpnam() {
    if (tmpnam_.length() == 0) {
      ScratchFile tmp;
      tmpnam_ = tmp.GetFilename() + "asm";
    }
    return tmpnam_;
  }

  static constexpr size_t OBJDUMP_SECTION_LINE_MIN_TOKENS = 6;

  std::string architecture_string_;
  const char* asm_header_;

  std::string assembler_cmd_name_;
  std::string assembler_parameters_;

  std::string objdump_cmd_name_;
  std::string objdump_parameters_;

  std::string disassembler_cmd_name_;
  std::string disassembler_parameters_;

  std::string resolved_assembler_cmd_;
  std::string resolved_objdump_cmd_;
  std::string resolved_disassemble_cmd_;

  std::string android_data_;

  DISALLOW_COPY_AND_ASSIGN(AssemblerTestInfrastructure);
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_ASSEMBLER_TEST_BASE_H_
