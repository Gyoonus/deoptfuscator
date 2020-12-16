/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <string>
#include <vector>

#include "common_runtime_test.h"

#include "base/file_utils.h"
#include "base/os.h"
#include "exec_utils.h"
#include "oat_file.h"

namespace art {

static const char* kDexDiagContains = "--contains=core.vdex";
static const char* kDexDiagContainsFails = "--contains=anything_other_than_core.vdex";
static const char* kDexDiagHelp = "--help";
static const char* kDexDiagVerbose = "--verbose";
static const char* kDexDiagBinaryName = "dexdiag";

class DexDiagTest : public CommonRuntimeTest {
 protected:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();
  }

  // Path to the dexdiag(d?)[32|64] binary.
  std::string GetDexDiagFilePath() {
    std::string root = GetTestAndroidRoot();

    root += "/bin/";
    root += kDexDiagBinaryName;

    std::string root32 = root + "32";
    // If we have both a 32-bit and a 64-bit build, the 32-bit file will have a 32 suffix.
    if (OS::FileExists(root32.c_str()) && !Is64BitInstructionSet(kRuntimeISA)) {
      return root32;
    } else {
      // This is a 64-bit build or only a single build exists.
      return root;
    }
  }

  std::unique_ptr<OatFile> OpenOatAndVdexFiles() {
    // Open the core.oat file.
    // This is a little convoluted because we have to
    // get the location of the default core image (.../framework/core.oat),
    // find it in the right architecture subdirectory (.../framework/arm/core.oat),
    // Then, opening the oat file has the side-effect of opening the corresponding
    // vdex file (.../framework/arm/core.vdex).
    const std::string default_location = GetCoreOatLocation();
    EXPECT_TRUE(!default_location.empty());
    std::string oat_location = GetSystemImageFilename(default_location.c_str(), kRuntimeISA);
    EXPECT_TRUE(!oat_location.empty());
    std::cout << "==" << oat_location << std::endl;
    std::string error_msg;
    std::unique_ptr<OatFile> oat(OatFile::Open(/* zip_fd */ -1,
                                               oat_location.c_str(),
                                               oat_location.c_str(),
                                               nullptr,
                                               nullptr,
                                               false,
                                               /*low_4gb*/false,
                                               nullptr,
                                               &error_msg));
    EXPECT_TRUE(oat != nullptr) << error_msg;
    return oat;
  }

  // Run dexdiag with a custom boot image location.
  bool Exec(pid_t this_pid, const std::vector<std::string>& args, std::string* error_msg) {
    // Invoke 'dexdiag' against the current process.
    // This should succeed because we have a runtime and so it should
    // be able to map in the boot.art and do a diff for it.
    std::vector<std::string> exec_argv;

    // Build the command line "dexdiag <args> this_pid".
    std::string executable_path = GetDexDiagFilePath();
    EXPECT_TRUE(OS::FileExists(executable_path.c_str())) << executable_path
                                                         << " should be a valid file path";
    exec_argv.push_back(executable_path);
    for (const auto& arg : args) {
      exec_argv.push_back(arg);
    }
    exec_argv.push_back(std::to_string(this_pid));

    return ::art::Exec(exec_argv, error_msg);
  }
};

// We can't run these tests on the host, as they will fail when trying to open
// /proc/pid/pagemap.
// On the target, we invoke 'dexdiag' against the current process.
// This should succeed because we have a runtime and so dexdiag should
// be able to find the map for, e.g., boot.vdex and friends.
TEST_F(DexDiagTest, DexDiagHelpTest) {
  // TODO: test the resulting output.
  std::string error_msg;
  ASSERT_TRUE(Exec(getpid(), { kDexDiagHelp }, &error_msg)) << "Failed to execute -- because: "
                                                            << error_msg;
}

#if defined (ART_TARGET)
TEST_F(DexDiagTest, DexDiagContainsTest) {
#else
TEST_F(DexDiagTest, DISABLED_DexDiagContainsTest) {
#endif
  std::unique_ptr<OatFile> oat = OpenOatAndVdexFiles();
  // TODO: test the resulting output.
  std::string error_msg;
  ASSERT_TRUE(Exec(getpid(), { kDexDiagContains }, &error_msg)) << "Failed to execute -- because: "
                                                                << error_msg;
}

#if defined (ART_TARGET)
TEST_F(DexDiagTest, DexDiagContainsFailsTest) {
#else
TEST_F(DexDiagTest, DISABLED_DexDiagContainsFailsTest) {
#endif
  std::unique_ptr<OatFile> oat = OpenOatAndVdexFiles();
  // TODO: test the resulting output.
  std::string error_msg;
  ASSERT_FALSE(Exec(getpid(), { kDexDiagContainsFails }, &error_msg))
      << "Failed to execute -- because: "
      << error_msg;
}

#if defined (ART_TARGET)
TEST_F(DexDiagTest, DexDiagVerboseTest) {
#else
TEST_F(DexDiagTest, DISABLED_DexDiagVerboseTest) {
#endif
  // TODO: test the resulting output.
  std::unique_ptr<OatFile> oat = OpenOatAndVdexFiles();
  std::string error_msg;
  ASSERT_TRUE(Exec(getpid(), { kDexDiagVerbose }, &error_msg)) << "Failed to execute -- because: "
                                                               << error_msg;
}

}  // namespace art
