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

#include <gtest/gtest.h>

#include "arch/instruction_set.h"
#include "compiler_filter.h"
#include "dexopt_test.h"

namespace art {

class DexoptAnalyzerTest : public DexoptTest {
 protected:
  std::string GetDexoptAnalyzerCmd() {
    std::string file_path = GetTestAndroidRoot();
    file_path += "/bin/dexoptanalyzer";
    if (kIsDebugBuild) {
      file_path += "d";
    }
    EXPECT_TRUE(OS::FileExists(file_path.c_str())) << file_path << " should be a valid file path";
    return file_path;
  }

  int Analyze(const std::string& dex_file,
              CompilerFilter::Filter compiler_filter,
              bool assume_profile_changed) {
    std::string dexoptanalyzer_cmd = GetDexoptAnalyzerCmd();
    std::vector<std::string> argv_str;
    argv_str.push_back(dexoptanalyzer_cmd);
    argv_str.push_back("--dex-file=" + dex_file);
    argv_str.push_back("--isa=" + std::string(GetInstructionSetString(kRuntimeISA)));
    argv_str.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(compiler_filter));
    if (assume_profile_changed) {
      argv_str.push_back("--assume-profile-changed");
    }
    argv_str.push_back("--image=" + GetImageLocation());
    argv_str.push_back("--android-data=" + android_data_);

    std::string error;
    return ExecAndReturnCode(argv_str, &error);
  }

  int DexoptanalyzerToOatFileAssistant(int dexoptanalyzerResult) {
    switch (dexoptanalyzerResult) {
      case 0: return OatFileAssistant::kNoDexOptNeeded;
      case 1: return OatFileAssistant::kDex2OatFromScratch;
      case 2: return OatFileAssistant::kDex2OatForBootImage;
      case 3: return OatFileAssistant::kDex2OatForFilter;
      case 4: return OatFileAssistant::kDex2OatForRelocation;
      case 5: return -OatFileAssistant::kDex2OatForBootImage;
      case 6: return -OatFileAssistant::kDex2OatForFilter;
      case 7: return -OatFileAssistant::kDex2OatForRelocation;
      default: return dexoptanalyzerResult;
    }
  }

  // Verify that the output of dexoptanalyzer for the given arguments is the same
  // as the output of OatFileAssistant::GetDexOptNeeded.
  void Verify(const std::string& dex_file,
              CompilerFilter::Filter compiler_filter,
              bool assume_profile_changed = false,
              bool downgrade = false) {
    int dexoptanalyzerResult = Analyze(dex_file, compiler_filter, assume_profile_changed);
    dexoptanalyzerResult = DexoptanalyzerToOatFileAssistant(dexoptanalyzerResult);
    OatFileAssistant oat_file_assistant(dex_file.c_str(), kRuntimeISA, /*load_executable*/ false);
    int assistantResult = oat_file_assistant.GetDexOptNeeded(
        compiler_filter, assume_profile_changed, downgrade);
    EXPECT_EQ(assistantResult, dexoptanalyzerResult);
  }
};

// The tests below exercise the same test case from oat_file_assistant_test.cc.

// Case: We have a DEX file, but no OAT file for it.
TEST_F(DexoptAnalyzerTest, DexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
  Copy(GetDexSrc1(), dex_location);

  Verify(dex_location, CompilerFilter::kSpeed);
  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kQuicken);
  Verify(dex_location, CompilerFilter::kSpeedProfile);
}

// Case: We have a DEX file and up-to-date OAT file for it.
TEST_F(DexoptAnalyzerTest, OatUpToDate) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  Verify(dex_location, CompilerFilter::kSpeed);
  Verify(dex_location, CompilerFilter::kQuicken);
  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kEverything);
}

// Case: We have a DEX file and speed-profile OAT file for it.
TEST_F(DexoptAnalyzerTest, ProfileOatUpToDate) {
  std::string dex_location = GetScratchDir() + "/ProfileOatUpToDate.jar";
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeedProfile);

  Verify(dex_location, CompilerFilter::kSpeedProfile, false);
  Verify(dex_location, CompilerFilter::kQuicken, false);
  Verify(dex_location, CompilerFilter::kSpeedProfile, true);
  Verify(dex_location, CompilerFilter::kQuicken, true);
}

TEST_F(DexoptAnalyzerTest, Downgrade) {
  std::string dex_location = GetScratchDir() + "/Downgrade.jar";
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kQuicken);

  Verify(dex_location, CompilerFilter::kSpeedProfile, false, true);
  Verify(dex_location, CompilerFilter::kQuicken, false, true);
  Verify(dex_location, CompilerFilter::kVerify, false, true);
}

// Case: We have a MultiDEX file and up-to-date OAT file for it.
TEST_F(DexoptAnalyzerTest, MultiDexOatUpToDate) {
  std::string dex_location = GetScratchDir() + "/MultiDexOatUpToDate.jar";
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  Verify(dex_location, CompilerFilter::kSpeed, false);
}

// Case: We have a MultiDEX file where the secondary dex file is out of date.
TEST_F(DexoptAnalyzerTest, MultiDexSecondaryOutOfDate) {
  std::string dex_location = GetScratchDir() + "/MultiDexSecondaryOutOfDate.jar";

  // Compile code for GetMultiDexSrc1.
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Now overwrite the dex file with GetMultiDexSrc2 so the secondary checksum
  // is out of date.
  Copy(GetMultiDexSrc2(), dex_location);

  Verify(dex_location, CompilerFilter::kSpeed, false);
}


// Case: We have a DEX file and an OAT file out of date with respect to the
// dex checksum.
TEST_F(DexoptAnalyzerTest, OatDexOutOfDate) {
  std::string dex_location = GetScratchDir() + "/OatDexOutOfDate.jar";

  // We create a dex, generate an oat for it, then overwrite the dex with a
  // different dex to make the oat out of date.
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);
  Copy(GetDexSrc2(), dex_location);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: We have a DEX file and an OAT file out of date with respect to the
// boot image.
TEST_F(DexoptAnalyzerTest, OatImageOutOfDate) {
  std::string dex_location = GetScratchDir() + "/OatImageOutOfDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     CompilerFilter::kSpeed,
                     /*relocate*/true,
                     /*pic*/false,
                     /*with_alternate_image*/true);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kQuicken);
  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: We have a DEX file and a verify-at-runtime OAT file out of date with
// respect to the boot image.
// It shouldn't matter that the OAT file is out of date, because it is
// verify-at-runtime.
TEST_F(DexoptAnalyzerTest, OatVerifyAtRuntimeImageOutOfDate) {
  std::string dex_location = GetScratchDir() + "/OatVerifyAtRuntimeImageOutOfDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     CompilerFilter::kExtract,
                     /*relocate*/true,
                     /*pic*/false,
                     /*with_alternate_image*/true);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kQuicken);
}

// Case: We have a DEX file and an ODEX file, but no OAT file.
TEST_F(DexoptAnalyzerTest, DexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/DexOdexNoOat.odex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: We have a stripped DEX file and a PIC ODEX file, but no OAT file.
TEST_F(DexoptAnalyzerTest, StrippedDexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/StrippedDexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/StrippedDexOdexNoOat.odex";

  Copy(GetDexSrc1(), dex_location);
  GeneratePicOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Strip the dex file
  Copy(GetStrippedDexSrc1(), dex_location);

  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: We have a stripped DEX file, a PIC ODEX file, and an out-of-date OAT file.
TEST_F(DexoptAnalyzerTest, StrippedDexOdexOat) {
  std::string dex_location = GetScratchDir() + "/StrippedDexOdexOat.jar";
  std::string odex_location = GetOdexDir() + "/StrippedDexOdexOat.odex";

  // Create the oat file from a different dex file so it looks out of date.
  Copy(GetDexSrc2(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Create the odex file
  Copy(GetDexSrc1(), dex_location);
  GeneratePicOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Strip the dex file.
  Copy(GetStrippedDexSrc1(), dex_location);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kSpeed);
  Verify(dex_location, CompilerFilter::kEverything);
}

// Case: We have a stripped (or resource-only) DEX file, no ODEX file and no
// OAT file. Expect: The status is kNoDexOptNeeded.
TEST_F(DexoptAnalyzerTest, ResourceOnlyDex) {
  std::string dex_location = GetScratchDir() + "/ResourceOnlyDex.jar";

  Copy(GetStrippedDexSrc1(), dex_location);

  Verify(dex_location, CompilerFilter::kSpeed);
  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kQuicken);
}

// Case: We have a DEX file, an ODEX file and an OAT file, where the ODEX and
// OAT files both have patch delta of 0.
TEST_F(DexoptAnalyzerTest, OdexOatOverlap) {
  std::string dex_location = GetScratchDir() + "/OdexOatOverlap.jar";
  std::string odex_location = GetOdexDir() + "/OdexOatOverlap.odex";
  std::string oat_location = GetOdexDir() + "/OdexOatOverlap.oat";

  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Create the oat file by copying the odex so they are located in the same
  // place in memory.
  Copy(odex_location, oat_location);

  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: We have a DEX file and a PIC ODEX file, but no OAT file.
TEST_F(DexoptAnalyzerTest, DexPicOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexPicOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/DexPicOdexNoOat.odex";

  Copy(GetDexSrc1(), dex_location);
  GeneratePicOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  Verify(dex_location, CompilerFilter::kSpeed);
  Verify(dex_location, CompilerFilter::kEverything);
}

// Case: We have a DEX file and a VerifyAtRuntime ODEX file, but no OAT file..
TEST_F(DexoptAnalyzerTest, DexVerifyAtRuntimeOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexVerifyAtRuntimeOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/DexVerifyAtRuntimeOdexNoOat.odex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kExtract);

  Verify(dex_location, CompilerFilter::kExtract);
  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: Non-standard extension for dex file.
TEST_F(DexoptAnalyzerTest, LongDexExtension) {
  std::string dex_location = GetScratchDir() + "/LongDexExtension.jarx";
  Copy(GetDexSrc1(), dex_location);

  Verify(dex_location, CompilerFilter::kSpeed);
}

// Case: Very short, non-existent Dex location.
TEST_F(DexoptAnalyzerTest, ShortDexLocation) {
  std::string dex_location = "/xx";

  Verify(dex_location, CompilerFilter::kSpeed);
}

}  // namespace art
