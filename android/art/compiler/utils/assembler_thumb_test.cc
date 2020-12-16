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

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fstream>
#include <map>

#include "gtest/gtest.h"

#include "jni/quick/calling_convention.h"
#include "utils/arm/jni_macro_assembler_arm_vixl.h"

#include "base/hex_dump.h"
#include "common_runtime_test.h"

namespace art {
namespace arm {

// Include results file (generated manually)
#include "assembler_thumb_test_expected.cc.inc"

#ifndef ART_TARGET_ANDROID
// This controls whether the results are printed to the
// screen or compared against the expected output.
// To generate new expected output, set this to true and
// copy the output into the .cc.inc file in the form
// of the other results.
//
// When this is false, the results are not printed to the
// output, but are compared against the expected results
// in the .cc.inc file.
static constexpr bool kPrintResults = false;
#endif

void SetAndroidData() {
  const char* data = getenv("ANDROID_DATA");
  if (data == nullptr) {
    setenv("ANDROID_DATA", "/tmp", 1);
  }
}

int CompareIgnoringSpace(const char* s1, const char* s2) {
  while (*s1 != '\0') {
    while (isspace(*s1)) ++s1;
    while (isspace(*s2)) ++s2;
    if (*s1 == '\0' || *s1 != *s2) {
      break;
    }
    ++s1;
    ++s2;
  }
  return *s1 - *s2;
}

void InitResults() {
  if (test_results.empty()) {
    setup_results();
  }
}

std::string GetToolsDir() {
#ifndef ART_TARGET_ANDROID
  // This will only work on the host.  There is no as, objcopy or objdump on the device.
  static std::string toolsdir;

  if (toolsdir.empty()) {
    setup_results();
    toolsdir = CommonRuntimeTest::GetAndroidTargetToolsDir(InstructionSet::kThumb2);
    SetAndroidData();
  }

  return toolsdir;
#else
  return std::string();
#endif
}

void DumpAndCheck(std::vector<uint8_t>& code, const char* testname, const char* const* results) {
#ifndef ART_TARGET_ANDROID
  static std::string toolsdir = GetToolsDir();

  ScratchFile file;

  const char* filename = file.GetFilename().c_str();

  std::ofstream out(filename);
  if (out) {
    out << ".section \".text\"\n";
    out << ".syntax unified\n";
    out << ".arch armv7-a\n";
    out << ".thumb\n";
    out << ".thumb_func\n";
    out << ".type " << testname << ", #function\n";
    out << ".global " << testname << "\n";
    out << testname << ":\n";
    out << ".fnstart\n";

    for (uint32_t i = 0 ; i < code.size(); ++i) {
      out << ".byte " << (static_cast<int>(code[i]) & 0xff) << "\n";
    }
    out << ".fnend\n";
    out << ".size " << testname << ", .-" << testname << "\n";
  }
  out.close();

  char cmd[1024];

  // Assemble the .S
  snprintf(cmd, sizeof(cmd), "%sas %s -o %s.o", toolsdir.c_str(), filename, filename);
  int cmd_result = system(cmd);
  ASSERT_EQ(cmd_result, 0) << strerror(errno);

  // Disassemble.
  snprintf(cmd, sizeof(cmd), "%sobjdump -D -M force-thumb --section=.text %s.o  | grep '^  *[0-9a-f][0-9a-f]*:'",
    toolsdir.c_str(), filename);
  if (kPrintResults) {
    // Print the results only, don't check. This is used to generate new output for inserting
    // into the .inc file, so let's add the appropriate prefix/suffix needed in the C++ code.
    strcat(cmd, " | sed '-es/^/  \"/' | sed '-es/$/\\\\n\",/'");
    int cmd_result3 = system(cmd);
    ASSERT_EQ(cmd_result3, 0) << strerror(errno);
  } else {
    // Check the results match the appropriate results in the .inc file.
    FILE *fp = popen(cmd, "r");
    ASSERT_TRUE(fp != nullptr);

    uint32_t lineindex = 0;

    while (!feof(fp)) {
      char testline[256];
      char *s = fgets(testline, sizeof(testline), fp);
      if (s == nullptr) {
        break;
      }
      if (CompareIgnoringSpace(results[lineindex], testline) != 0) {
        LOG(FATAL) << "Output is not as expected at line: " << lineindex
          << results[lineindex] << "/" << testline << ", test name: " << testname;
      }
      ++lineindex;
    }
    // Check that we are at the end.
    ASSERT_TRUE(results[lineindex] == nullptr);
    fclose(fp);
  }

  char buf[FILENAME_MAX];
  snprintf(buf, sizeof(buf), "%s.o", filename);
  unlink(buf);
#endif  // ART_TARGET_ANDROID
}

class ArmVIXLAssemblerTest : public ::testing::Test {
 public:
  ArmVIXLAssemblerTest() : pool(), allocator(&pool), assembler(&allocator) { }

  ArenaPool pool;
  ArenaAllocator allocator;
  ArmVIXLJNIMacroAssembler assembler;
};

#define __ assembler->

void EmitAndCheck(ArmVIXLJNIMacroAssembler* assembler, const char* testname,
                  const char* const* results) {
  __ FinalizeCode();
  size_t cs = __ CodeSize();
  std::vector<uint8_t> managed_code(cs);
  MemoryRegion code(&managed_code[0], managed_code.size());
  __ FinalizeInstructions(code);

  DumpAndCheck(managed_code, testname, results);
}

void EmitAndCheck(ArmVIXLJNIMacroAssembler* assembler, const char* testname) {
  InitResults();
  std::map<std::string, const char* const*>::iterator results = test_results.find(testname);
  ASSERT_NE(results, test_results.end());

  EmitAndCheck(assembler, testname, results->second);
}

#undef __

#define __ assembler.

TEST_F(ArmVIXLAssemblerTest, VixlJniHelpers) {
  // Run the test only with Baker read barriers, as the expected
  // generated code contains a Marking Register refresh instruction.
  TEST_DISABLED_WITHOUT_BAKER_READ_BARRIERS();

  const bool is_static = true;
  const bool is_synchronized = false;
  const bool is_critical_native = false;
  const char* shorty = "IIFII";

  std::unique_ptr<JniCallingConvention> jni_conv(
      JniCallingConvention::Create(&allocator,
                                   is_static,
                                   is_synchronized,
                                   is_critical_native,
                                   shorty,
                                   InstructionSet::kThumb2));
  std::unique_ptr<ManagedRuntimeCallingConvention> mr_conv(
      ManagedRuntimeCallingConvention::Create(
          &allocator, is_static, is_synchronized, shorty, InstructionSet::kThumb2));
  const int frame_size(jni_conv->FrameSize());
  ArrayRef<const ManagedRegister> callee_save_regs = jni_conv->CalleeSaveRegisters();

  const ManagedRegister method_register = ArmManagedRegister::FromCoreRegister(R0);
  const ManagedRegister scratch_register = ArmManagedRegister::FromCoreRegister(R12);

  __ BuildFrame(frame_size, mr_conv->MethodRegister(), callee_save_regs, mr_conv->EntrySpills());
  __ IncreaseFrameSize(32);

  // Loads
  __ IncreaseFrameSize(4096);
  __ Load(method_register, FrameOffset(32), 4);
  __ Load(method_register, FrameOffset(124), 4);
  __ Load(method_register, FrameOffset(132), 4);
  __ Load(method_register, FrameOffset(1020), 4);
  __ Load(method_register, FrameOffset(1024), 4);
  __ Load(scratch_register, FrameOffset(4092), 4);
  __ Load(scratch_register, FrameOffset(4096), 4);
  __ LoadRawPtrFromThread(scratch_register, ThreadOffset32(512));
  __ LoadRef(method_register, scratch_register, MemberOffset(128), /* unpoison_reference */ false);

  // Stores
  __ Store(FrameOffset(32), method_register, 4);
  __ Store(FrameOffset(124), method_register, 4);
  __ Store(FrameOffset(132), method_register, 4);
  __ Store(FrameOffset(1020), method_register, 4);
  __ Store(FrameOffset(1024), method_register, 4);
  __ Store(FrameOffset(4092), scratch_register, 4);
  __ Store(FrameOffset(4096), scratch_register, 4);
  __ StoreImmediateToFrame(FrameOffset(48), 0xFF, scratch_register);
  __ StoreImmediateToFrame(FrameOffset(48), 0xFFFFFF, scratch_register);
  __ StoreRawPtr(FrameOffset(48), scratch_register);
  __ StoreRef(FrameOffset(48), scratch_register);
  __ StoreSpanning(FrameOffset(48), method_register, FrameOffset(48), scratch_register);
  __ StoreStackOffsetToThread(ThreadOffset32(512), FrameOffset(4096), scratch_register);
  __ StoreStackPointerToThread(ThreadOffset32(512));

  // Other
  __ Call(method_register, FrameOffset(48), scratch_register);
  __ Copy(FrameOffset(48), FrameOffset(44), scratch_register, 4);
  __ CopyRawPtrFromThread(FrameOffset(44), ThreadOffset32(512), scratch_register);
  __ CopyRef(FrameOffset(48), FrameOffset(44), scratch_register);
  __ GetCurrentThread(method_register);
  __ GetCurrentThread(FrameOffset(48), scratch_register);
  __ Move(scratch_register, method_register, 4);
  __ VerifyObject(scratch_register, false);

  __ CreateHandleScopeEntry(scratch_register, FrameOffset(48), scratch_register, true);
  __ CreateHandleScopeEntry(scratch_register, FrameOffset(48), scratch_register, false);
  __ CreateHandleScopeEntry(method_register, FrameOffset(48), scratch_register, true);
  __ CreateHandleScopeEntry(FrameOffset(48), FrameOffset(64), scratch_register, true);
  __ CreateHandleScopeEntry(method_register, FrameOffset(0), scratch_register, true);
  __ CreateHandleScopeEntry(method_register, FrameOffset(1025), scratch_register, true);
  __ CreateHandleScopeEntry(scratch_register, FrameOffset(1025), scratch_register, true);

  __ ExceptionPoll(scratch_register, 0);

  // Push the target out of range of branch emitted by ExceptionPoll.
  for (int i = 0; i < 64; i++) {
    __ Store(FrameOffset(2047), scratch_register, 4);
  }

  __ DecreaseFrameSize(4096);
  __ DecreaseFrameSize(32);
  __ RemoveFrame(frame_size, callee_save_regs, /* may_suspend */ true);

  EmitAndCheck(&assembler, "VixlJniHelpers");
}

#undef __

// TODO: Avoid these macros.
#define R0 vixl::aarch32::r0
#define R2 vixl::aarch32::r2
#define R4 vixl::aarch32::r4
#define R12 vixl::aarch32::r12

#define __ assembler.asm_.

TEST_F(ArmVIXLAssemblerTest, VixlLoadFromOffset) {
  __ LoadFromOffset(kLoadWord, R2, R4, 12);
  __ LoadFromOffset(kLoadWord, R2, R4, 0xfff);
  __ LoadFromOffset(kLoadWord, R2, R4, 0x1000);
  __ LoadFromOffset(kLoadWord, R2, R4, 0x1000a4);
  __ LoadFromOffset(kLoadWord, R2, R4, 0x101000);
  __ LoadFromOffset(kLoadWord, R4, R4, 0x101000);
  __ LoadFromOffset(kLoadUnsignedHalfword, R2, R4, 12);
  __ LoadFromOffset(kLoadUnsignedHalfword, R2, R4, 0xfff);
  __ LoadFromOffset(kLoadUnsignedHalfword, R2, R4, 0x1000);
  __ LoadFromOffset(kLoadUnsignedHalfword, R2, R4, 0x1000a4);
  __ LoadFromOffset(kLoadUnsignedHalfword, R2, R4, 0x101000);
  __ LoadFromOffset(kLoadUnsignedHalfword, R4, R4, 0x101000);
  __ LoadFromOffset(kLoadWordPair, R2, R4, 12);
  __ LoadFromOffset(kLoadWordPair, R2, R4, 0x3fc);
  __ LoadFromOffset(kLoadWordPair, R2, R4, 0x400);
  __ LoadFromOffset(kLoadWordPair, R2, R4, 0x400a4);
  __ LoadFromOffset(kLoadWordPair, R2, R4, 0x40400);
  __ LoadFromOffset(kLoadWordPair, R4, R4, 0x40400);

  vixl::aarch32::UseScratchRegisterScope temps(assembler.asm_.GetVIXLAssembler());
  temps.Exclude(R12);
  __ LoadFromOffset(kLoadWord, R0, R12, 12);  // 32-bit because of R12.
  temps.Include(R12);
  __ LoadFromOffset(kLoadWord, R2, R4, 0xa4 - 0x100000);

  __ LoadFromOffset(kLoadSignedByte, R2, R4, 12);
  __ LoadFromOffset(kLoadUnsignedByte, R2, R4, 12);
  __ LoadFromOffset(kLoadSignedHalfword, R2, R4, 12);

  EmitAndCheck(&assembler, "VixlLoadFromOffset");
}

TEST_F(ArmVIXLAssemblerTest, VixlStoreToOffset) {
  __ StoreToOffset(kStoreWord, R2, R4, 12);
  __ StoreToOffset(kStoreWord, R2, R4, 0xfff);
  __ StoreToOffset(kStoreWord, R2, R4, 0x1000);
  __ StoreToOffset(kStoreWord, R2, R4, 0x1000a4);
  __ StoreToOffset(kStoreWord, R2, R4, 0x101000);
  __ StoreToOffset(kStoreWord, R4, R4, 0x101000);
  __ StoreToOffset(kStoreHalfword, R2, R4, 12);
  __ StoreToOffset(kStoreHalfword, R2, R4, 0xfff);
  __ StoreToOffset(kStoreHalfword, R2, R4, 0x1000);
  __ StoreToOffset(kStoreHalfword, R2, R4, 0x1000a4);
  __ StoreToOffset(kStoreHalfword, R2, R4, 0x101000);
  __ StoreToOffset(kStoreHalfword, R4, R4, 0x101000);
  __ StoreToOffset(kStoreWordPair, R2, R4, 12);
  __ StoreToOffset(kStoreWordPair, R2, R4, 0x3fc);
  __ StoreToOffset(kStoreWordPair, R2, R4, 0x400);
  __ StoreToOffset(kStoreWordPair, R2, R4, 0x400a4);
  __ StoreToOffset(kStoreWordPair, R2, R4, 0x40400);
  __ StoreToOffset(kStoreWordPair, R4, R4, 0x40400);

  vixl::aarch32::UseScratchRegisterScope temps(assembler.asm_.GetVIXLAssembler());
  temps.Exclude(R12);
  __ StoreToOffset(kStoreWord, R0, R12, 12);  // 32-bit because of R12.
  temps.Include(R12);
  __ StoreToOffset(kStoreWord, R2, R4, 0xa4 - 0x100000);

  __ StoreToOffset(kStoreByte, R2, R4, 12);

  EmitAndCheck(&assembler, "VixlStoreToOffset");
}

#undef __
}  // namespace arm
}  // namespace art
