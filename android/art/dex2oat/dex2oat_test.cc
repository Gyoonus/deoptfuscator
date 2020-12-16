/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "common_runtime_test.h"

#include "base/macros.h"
#include "base/mutex-inl.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/base64_test_util.h"
#include "dex/bytecode_utils.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex2oat_environment_test.h"
#include "dex2oat_return_codes.h"
#include "jit/profile_compilation_info.h"
#include "oat.h"
#include "oat_file.h"
#include "vdex_file.h"
#include "ziparchive/zip_writer.h"

namespace art {

static constexpr size_t kMaxMethodIds = 65535;
static constexpr bool kDebugArgs = false;
static const char* kDisableCompactDex = "--compact-dex-level=none";

using android::base::StringPrintf;

class Dex2oatTest : public Dex2oatEnvironmentTest {
 public:
  virtual void TearDown() OVERRIDE {
    Dex2oatEnvironmentTest::TearDown();

    output_ = "";
    error_msg_ = "";
    success_ = false;
  }

 protected:
  int GenerateOdexForTestWithStatus(const std::vector<std::string>& dex_locations,
                                    const std::string& odex_location,
                                    CompilerFilter::Filter filter,
                                    std::string* error_msg,
                                    const std::vector<std::string>& extra_args = {},
                                    bool use_fd = false) {
    std::unique_ptr<File> oat_file;
    std::vector<std::string> args;
    // Add dex file args.
    for (const std::string& dex_location : dex_locations) {
      args.push_back("--dex-file=" + dex_location);
    }
    if (use_fd) {
      oat_file.reset(OS::CreateEmptyFile(odex_location.c_str()));
      CHECK(oat_file != nullptr) << odex_location;
      args.push_back("--oat-fd=" + std::to_string(oat_file->Fd()));
      args.push_back("--oat-location=" + odex_location);
    } else {
      args.push_back("--oat-file=" + odex_location);
    }
    args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
    args.push_back("--runtime-arg");
    args.push_back("-Xnorelocate");

    args.insert(args.end(), extra_args.begin(), extra_args.end());

    int status = Dex2Oat(args, error_msg);
    if (oat_file != nullptr) {
      CHECK_EQ(oat_file->FlushClose(), 0) << "Could not flush and close oat file";
    }
    return status;
  }

  void GenerateOdexForTest(
      const std::string& dex_location,
      const std::string& odex_location,
      CompilerFilter::Filter filter,
      const std::vector<std::string>& extra_args = {},
      bool expect_success = true,
      bool use_fd = false) {
    GenerateOdexForTest(dex_location,
                        odex_location,
                        filter,
                        extra_args,
                        expect_success,
                        use_fd,
                        [](const OatFile&) {});
  }

  bool test_accepts_odex_file_on_failure = false;

  template <typename T>
  void GenerateOdexForTest(
      const std::string& dex_location,
      const std::string& odex_location,
      CompilerFilter::Filter filter,
      const std::vector<std::string>& extra_args,
      bool expect_success,
      bool use_fd,
      T check_oat) {
    std::string error_msg;
    int status = GenerateOdexForTestWithStatus({dex_location},
                                               odex_location,
                                               filter,
                                               &error_msg,
                                               extra_args,
                                               use_fd);
    bool success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (expect_success) {
      ASSERT_TRUE(success) << error_msg << std::endl << output_;

      // Verify the odex file was generated as expected.
      std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                       odex_location.c_str(),
                                                       odex_location.c_str(),
                                                       nullptr,
                                                       nullptr,
                                                       false,
                                                       /*low_4gb*/false,
                                                       dex_location.c_str(),
                                                       &error_msg));
      ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;

      CheckFilter(filter, odex_file->GetCompilerFilter());
      check_oat(*(odex_file.get()));
    } else {
      ASSERT_FALSE(success) << output_;

      error_msg_ = error_msg;

      if (!test_accepts_odex_file_on_failure) {
        // Verify there's no loadable odex file.
        std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                         odex_location.c_str(),
                                                         odex_location.c_str(),
                                                         nullptr,
                                                         nullptr,
                                                         false,
                                                         /*low_4gb*/false,
                                                         dex_location.c_str(),
                                                         &error_msg));
        ASSERT_TRUE(odex_file.get() == nullptr);
      }
    }
  }

  // Check the input compiler filter against the generated oat file's filter. May be overridden
  // in subclasses when equality is not expected.
  virtual void CheckFilter(CompilerFilter::Filter expected, CompilerFilter::Filter actual) {
    EXPECT_EQ(expected, actual);
  }

  int Dex2Oat(const std::vector<std::string>& dex2oat_args, std::string* error_msg) {
    Runtime* runtime = Runtime::Current();

    const std::vector<gc::space::ImageSpace*>& image_spaces =
        runtime->GetHeap()->GetBootImageSpaces();
    if (image_spaces.empty()) {
      *error_msg = "No image location found for Dex2Oat.";
      return false;
    }
    std::string image_location = image_spaces[0]->GetImageLocation();

    std::vector<std::string> argv;
    argv.push_back(runtime->GetCompilerExecutable());

    if (runtime->IsJavaDebuggable()) {
      argv.push_back("--debuggable");
    }
    runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

    if (!runtime->IsVerificationEnabled()) {
      argv.push_back("--compiler-filter=assume-verified");
    }

    if (runtime->MustRelocateIfPossible()) {
      argv.push_back("--runtime-arg");
      argv.push_back("-Xrelocate");
    } else {
      argv.push_back("--runtime-arg");
      argv.push_back("-Xnorelocate");
    }

    if (!kIsTargetBuild) {
      argv.push_back("--host");
    }

    argv.push_back("--boot-image=" + image_location);

    std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
    argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

    argv.insert(argv.end(), dex2oat_args.begin(), dex2oat_args.end());

    // We must set --android-root.
    const char* android_root = getenv("ANDROID_ROOT");
    CHECK(android_root != nullptr);
    argv.push_back("--android-root=" + std::string(android_root));

    if (kDebugArgs) {
      std::string all_args;
      for (const std::string& arg : argv) {
        all_args += arg + " ";
      }
      LOG(ERROR) << all_args;
    }

    int link[2];

    if (pipe(link) == -1) {
      return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
      return false;
    }

    if (pid == 0) {
      // We need dex2oat to actually log things.
      setenv("ANDROID_LOG_TAGS", "*:d", 1);
      dup2(link[1], STDERR_FILENO);
      close(link[0]);
      close(link[1]);
      std::vector<const char*> c_args;
      for (const std::string& str : argv) {
        c_args.push_back(str.c_str());
      }
      c_args.push_back(nullptr);
      execv(c_args[0], const_cast<char* const*>(c_args.data()));
      exit(1);
      UNREACHABLE();
    } else {
      close(link[1]);
      char buffer[128];
      memset(buffer, 0, 128);
      ssize_t bytes_read = 0;

      while (TEMP_FAILURE_RETRY(bytes_read = read(link[0], buffer, 128)) > 0) {
        output_ += std::string(buffer, bytes_read);
      }
      close(link[0]);
      int status = -1;
      if (waitpid(pid, &status, 0) != -1) {
        success_ = (status == 0);
      }
      return status;
    }
  }

  std::string output_ = "";
  std::string error_msg_ = "";
  bool success_ = false;
};

class Dex2oatSwapTest : public Dex2oatTest {
 protected:
  void RunTest(bool use_fd, bool expect_use, const std::vector<std::string>& extra_args = {}) {
    std::string dex_location = GetScratchDir() + "/Dex2OatSwapTest.jar";
    std::string odex_location = GetOdexDir() + "/Dex2OatSwapTest.odex";

    Copy(GetTestDexFileName(), dex_location);

    std::vector<std::string> copy(extra_args);

    std::unique_ptr<ScratchFile> sf;
    if (use_fd) {
      sf.reset(new ScratchFile());
      copy.push_back(android::base::StringPrintf("--swap-fd=%d", sf->GetFd()));
    } else {
      std::string swap_location = GetOdexDir() + "/Dex2OatSwapTest.odex.swap";
      copy.push_back("--swap-file=" + swap_location);
    }
    GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed, copy);

    CheckValidity();
    ASSERT_TRUE(success_);
    CheckResult(expect_use);
  }

  virtual std::string GetTestDexFileName() {
    return Dex2oatEnvironmentTest::GetTestDexFileName("VerifierDeps");
  }

  virtual void CheckResult(bool expect_use) {
    if (kIsTargetBuild) {
      CheckTargetResult(expect_use);
    } else {
      CheckHostResult(expect_use);
    }
  }

  virtual void CheckTargetResult(bool expect_use ATTRIBUTE_UNUSED) {
    // TODO: Ignore for now, as we won't capture any output (it goes to the logcat). We may do
    //       something for variants with file descriptor where we can control the lifetime of
    //       the swap file and thus take a look at it.
  }

  virtual void CheckHostResult(bool expect_use) {
    if (!kIsTargetBuild) {
      if (expect_use) {
        EXPECT_NE(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      } else {
        EXPECT_EQ(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      }
    }
  }

  // Check whether the dex2oat run was really successful.
  virtual void CheckValidity() {
    if (kIsTargetBuild) {
      CheckTargetValidity();
    } else {
      CheckHostValidity();
    }
  }

  virtual void CheckTargetValidity() {
    // TODO: Ignore for now, as we won't capture any output (it goes to the logcat). We may do
    //       something for variants with file descriptor where we can control the lifetime of
    //       the swap file and thus take a look at it.
  }

  // On the host, we can get the dex2oat output. Here, look for "dex2oat took."
  virtual void CheckHostValidity() {
    EXPECT_NE(output_.find("dex2oat took"), std::string::npos) << output_;
  }
};

TEST_F(Dex2oatSwapTest, DoNotUseSwapDefaultSingleSmall) {
  RunTest(false /* use_fd */, false /* expect_use */);
  RunTest(true /* use_fd */, false /* expect_use */);
}

TEST_F(Dex2oatSwapTest, DoNotUseSwapSingle) {
  RunTest(false /* use_fd */, false /* expect_use */, { "--swap-dex-size-threshold=0" });
  RunTest(true /* use_fd */, false /* expect_use */, { "--swap-dex-size-threshold=0" });
}

TEST_F(Dex2oatSwapTest, DoNotUseSwapSmall) {
  RunTest(false /* use_fd */, false /* expect_use */, { "--swap-dex-count-threshold=0" });
  RunTest(true /* use_fd */, false /* expect_use */, { "--swap-dex-count-threshold=0" });
}

TEST_F(Dex2oatSwapTest, DoUseSwapSingleSmall) {
  RunTest(false /* use_fd */,
          true /* expect_use */,
          { "--swap-dex-size-threshold=0", "--swap-dex-count-threshold=0" });
  RunTest(true /* use_fd */,
          true /* expect_use */,
          { "--swap-dex-size-threshold=0", "--swap-dex-count-threshold=0" });
}

class Dex2oatSwapUseTest : public Dex2oatSwapTest {
 protected:
  void CheckHostResult(bool expect_use) OVERRIDE {
    if (!kIsTargetBuild) {
      if (expect_use) {
        EXPECT_NE(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      } else {
        EXPECT_EQ(output_.find("Large app, accepted running with swap."), std::string::npos)
            << output_;
      }
    }
  }

  std::string GetTestDexFileName() OVERRIDE {
    // Use Statics as it has a handful of functions.
    return CommonRuntimeTest::GetTestDexFileName("Statics");
  }

  void GrabResult1() {
    if (!kIsTargetBuild) {
      native_alloc_1_ = ParseNativeAlloc();
      swap_1_ = ParseSwap(false /* expected */);
    } else {
      native_alloc_1_ = std::numeric_limits<size_t>::max();
      swap_1_ = 0;
    }
  }

  void GrabResult2() {
    if (!kIsTargetBuild) {
      native_alloc_2_ = ParseNativeAlloc();
      swap_2_ = ParseSwap(true /* expected */);
    } else {
      native_alloc_2_ = 0;
      swap_2_ = std::numeric_limits<size_t>::max();
    }
  }

 private:
  size_t ParseNativeAlloc() {
    std::regex native_alloc_regex("dex2oat took.*native alloc=[^ ]+ \\(([0-9]+)B\\)");
    std::smatch native_alloc_match;
    bool found = std::regex_search(output_, native_alloc_match, native_alloc_regex);
    if (!found) {
      EXPECT_TRUE(found);
      return 0;
    }
    if (native_alloc_match.size() != 2U) {
      EXPECT_EQ(native_alloc_match.size(), 2U);
      return 0;
    }

    std::istringstream stream(native_alloc_match[1].str());
    size_t value;
    stream >> value;

    return value;
  }

  size_t ParseSwap(bool expected) {
    std::regex swap_regex("dex2oat took[^\\n]+swap=[^ ]+ \\(([0-9]+)B\\)");
    std::smatch swap_match;
    bool found = std::regex_search(output_, swap_match, swap_regex);
    if (found != expected) {
      EXPECT_EQ(expected, found);
      return 0;
    }

    if (!found) {
      return 0;
    }

    if (swap_match.size() != 2U) {
      EXPECT_EQ(swap_match.size(), 2U);
      return 0;
    }

    std::istringstream stream(swap_match[1].str());
    size_t value;
    stream >> value;

    return value;
  }

 protected:
  size_t native_alloc_1_;
  size_t native_alloc_2_;

  size_t swap_1_;
  size_t swap_2_;
};

TEST_F(Dex2oatSwapUseTest, CheckSwapUsage) {
  // Native memory usage isn't correctly tracked under sanitization.
  TEST_DISABLED_FOR_MEMORY_TOOL_ASAN();

  // The `native_alloc_2_ >= native_alloc_1_` assertion below may not
  // hold true on some x86 systems; disable this test while we
  // investigate (b/29259363).
  TEST_DISABLED_FOR_X86();

  RunTest(false /* use_fd */,
          false /* expect_use */);
  GrabResult1();
  std::string output_1 = output_;

  output_ = "";

  RunTest(false /* use_fd */,
          true /* expect_use */,
          { "--swap-dex-size-threshold=0", "--swap-dex-count-threshold=0" });
  GrabResult2();
  std::string output_2 = output_;

  if (native_alloc_2_ >= native_alloc_1_ || swap_1_ >= swap_2_) {
    EXPECT_LT(native_alloc_2_, native_alloc_1_);
    EXPECT_LT(swap_1_, swap_2_);

    LOG(ERROR) << output_1;
    LOG(ERROR) << output_2;
  }
}

class Dex2oatVeryLargeTest : public Dex2oatTest {
 protected:
  void CheckFilter(CompilerFilter::Filter input ATTRIBUTE_UNUSED,
                   CompilerFilter::Filter result ATTRIBUTE_UNUSED) OVERRIDE {
    // Ignore, we'll do our own checks.
  }

  void RunTest(CompilerFilter::Filter filter,
               bool expect_large,
               bool expect_downgrade,
               const std::vector<std::string>& extra_args = {}) {
    std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
    std::string odex_location = GetOdexDir() + "/DexOdexNoOat.odex";
    std::string app_image_file = GetScratchDir() + "/Test.art";

    Copy(GetDexSrc1(), dex_location);

    std::vector<std::string> new_args(extra_args);
    new_args.push_back("--app-image-file=" + app_image_file);
    GenerateOdexForTest(dex_location, odex_location, filter, new_args);

    CheckValidity();
    ASSERT_TRUE(success_);
    CheckResult(dex_location,
                odex_location,
                app_image_file,
                filter,
                expect_large,
                expect_downgrade);
  }

  void CheckResult(const std::string& dex_location,
                   const std::string& odex_location,
                   const std::string& app_image_file,
                   CompilerFilter::Filter filter,
                   bool expect_large,
                   bool expect_downgrade) {
    if (expect_downgrade) {
      EXPECT_TRUE(expect_large);
    }
    // Host/target independent checks.
    std::string error_msg;
    std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                     odex_location.c_str(),
                                                     odex_location.c_str(),
                                                     nullptr,
                                                     nullptr,
                                                     false,
                                                     /*low_4gb*/false,
                                                     dex_location.c_str(),
                                                     &error_msg));
    ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;
    EXPECT_GT(app_image_file.length(), 0u);
    std::unique_ptr<File> file(OS::OpenFileForReading(app_image_file.c_str()));
    if (expect_large) {
      // Note: we cannot check the following
      // EXPECT_FALSE(CompilerFilter::IsAotCompilationEnabled(odex_file->GetCompilerFilter()));
      // The reason is that the filter override currently happens when the dex files are
      // loaded in dex2oat, which is after the oat file has been started. Thus, the header
      // store cannot be changed, and the original filter is set in stone.

      for (const OatDexFile* oat_dex_file : odex_file->GetOatDexFiles()) {
        std::unique_ptr<const DexFile> dex_file = oat_dex_file->OpenDexFile(&error_msg);
        ASSERT_TRUE(dex_file != nullptr);
        uint32_t class_def_count = dex_file->NumClassDefs();
        ASSERT_LT(class_def_count, std::numeric_limits<uint16_t>::max());
        for (uint16_t class_def_index = 0; class_def_index < class_def_count; ++class_def_index) {
          OatFile::OatClass oat_class = oat_dex_file->GetOatClass(class_def_index);
          EXPECT_EQ(oat_class.GetType(), OatClassType::kOatClassNoneCompiled);
        }
      }

      // If the input filter was "below," it should have been used.
      if (!CompilerFilter::IsAsGoodAs(CompilerFilter::kExtract, filter)) {
        EXPECT_EQ(odex_file->GetCompilerFilter(), filter);
      }

      // If expect large, make sure the app image isn't generated or is empty.
      if (file != nullptr) {
        EXPECT_EQ(file->GetLength(), 0u);
      }
    } else {
      EXPECT_EQ(odex_file->GetCompilerFilter(), filter);
      ASSERT_TRUE(file != nullptr) << app_image_file;
      EXPECT_GT(file->GetLength(), 0u);
    }

    // Host/target dependent checks.
    if (kIsTargetBuild) {
      CheckTargetResult(expect_downgrade);
    } else {
      CheckHostResult(expect_downgrade);
    }
  }

  void CheckTargetResult(bool expect_downgrade ATTRIBUTE_UNUSED) {
    // TODO: Ignore for now. May do something for fd things.
  }

  void CheckHostResult(bool expect_downgrade) {
    if (!kIsTargetBuild) {
      if (expect_downgrade) {
        EXPECT_NE(output_.find("Very large app, downgrading to"), std::string::npos) << output_;
      } else {
        EXPECT_EQ(output_.find("Very large app, downgrading to"), std::string::npos) << output_;
      }
    }
  }

  // Check whether the dex2oat run was really successful.
  void CheckValidity() {
    if (kIsTargetBuild) {
      CheckTargetValidity();
    } else {
      CheckHostValidity();
    }
  }

  void CheckTargetValidity() {
    // TODO: Ignore for now.
  }

  // On the host, we can get the dex2oat output. Here, look for "dex2oat took."
  void CheckHostValidity() {
    EXPECT_NE(output_.find("dex2oat took"), std::string::npos) << output_;
  }
};

TEST_F(Dex2oatVeryLargeTest, DontUseVeryLarge) {
  RunTest(CompilerFilter::kAssumeVerified, false, false);
  RunTest(CompilerFilter::kExtract, false, false);
  RunTest(CompilerFilter::kQuicken, false, false);
  RunTest(CompilerFilter::kSpeed, false, false);

  RunTest(CompilerFilter::kAssumeVerified, false, false, { "--very-large-app-threshold=10000000" });
  RunTest(CompilerFilter::kExtract, false, false, { "--very-large-app-threshold=10000000" });
  RunTest(CompilerFilter::kQuicken, false, false, { "--very-large-app-threshold=10000000" });
  RunTest(CompilerFilter::kSpeed, false, false, { "--very-large-app-threshold=10000000" });
}

TEST_F(Dex2oatVeryLargeTest, UseVeryLarge) {
  RunTest(CompilerFilter::kAssumeVerified, true, false, { "--very-large-app-threshold=100" });
  RunTest(CompilerFilter::kExtract, true, false, { "--very-large-app-threshold=100" });
  RunTest(CompilerFilter::kQuicken, true, true, { "--very-large-app-threshold=100" });
  RunTest(CompilerFilter::kSpeed, true, true, { "--very-large-app-threshold=100" });
}

// Regressin test for b/35665292.
TEST_F(Dex2oatVeryLargeTest, SpeedProfileNoProfile) {
  // Test that dex2oat doesn't crash with speed-profile but no input profile.
  RunTest(CompilerFilter::kSpeedProfile, false, false);
}

class Dex2oatLayoutTest : public Dex2oatTest {
 protected:
  void CheckFilter(CompilerFilter::Filter input ATTRIBUTE_UNUSED,
                   CompilerFilter::Filter result ATTRIBUTE_UNUSED) OVERRIDE {
    // Ignore, we'll do our own checks.
  }

  // Emits a profile with a single dex file with the given location and a single class index of 1.
  void GenerateProfile(const std::string& test_profile,
                       const std::string& dex_location,
                       size_t num_classes,
                       uint32_t checksum) {
    int profile_test_fd = open(test_profile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    CHECK_GE(profile_test_fd, 0);

    ProfileCompilationInfo info;
    std::string profile_key = ProfileCompilationInfo::GetProfileDexFileKey(dex_location);
    for (size_t i = 0; i < num_classes; ++i) {
      info.AddClassIndex(profile_key, checksum, dex::TypeIndex(1 + i), kMaxMethodIds);
    }
    bool result = info.Save(profile_test_fd);
    close(profile_test_fd);
    ASSERT_TRUE(result);
  }

  void CompileProfileOdex(const std::string& dex_location,
                          const std::string& odex_location,
                          const std::string& app_image_file_name,
                          bool use_fd,
                          size_t num_profile_classes,
                          const std::vector<std::string>& extra_args = {},
                          bool expect_success = true) {
    const std::string profile_location = GetScratchDir() + "/primary.prof";
    const char* location = dex_location.c_str();
    std::string error_msg;
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    const ArtDexFileLoader dex_file_loader;
    ASSERT_TRUE(dex_file_loader.Open(
        location, location, /* verify */ true, /* verify_checksum */ true, &error_msg, &dex_files));
    EXPECT_EQ(dex_files.size(), 1U);
    std::unique_ptr<const DexFile>& dex_file = dex_files[0];
    GenerateProfile(profile_location,
                    dex_location,
                    num_profile_classes,
                    dex_file->GetLocationChecksum());
    std::vector<std::string> copy(extra_args);
    copy.push_back("--profile-file=" + profile_location);
    std::unique_ptr<File> app_image_file;
    if (!app_image_file_name.empty()) {
      if (use_fd) {
        app_image_file.reset(OS::CreateEmptyFile(app_image_file_name.c_str()));
        copy.push_back("--app-image-fd=" + std::to_string(app_image_file->Fd()));
      } else {
        copy.push_back("--app-image-file=" + app_image_file_name);
      }
    }
    GenerateOdexForTest(dex_location,
                        odex_location,
                        CompilerFilter::kSpeedProfile,
                        copy,
                        expect_success,
                        use_fd);
    if (app_image_file != nullptr) {
      ASSERT_EQ(app_image_file->FlushCloseOrErase(), 0) << "Could not flush and close art file";
    }
  }

  uint64_t GetImageObjectSectionSize(const std::string& image_file_name) {
    EXPECT_FALSE(image_file_name.empty());
    std::unique_ptr<File> file(OS::OpenFileForReading(image_file_name.c_str()));
    CHECK(file != nullptr);
    ImageHeader image_header;
    const bool success = file->ReadFully(&image_header, sizeof(image_header));
    CHECK(success);
    CHECK(image_header.IsValid());
    ReaderMutexLock mu(Thread::Current(), *Locks::mutator_lock_);
    return image_header.GetObjectsSection().Size();
  }

  void RunTest(bool app_image) {
    std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
    std::string odex_location = GetOdexDir() + "/DexOdexNoOat.odex";
    std::string app_image_file = app_image ? (GetOdexDir() + "/DexOdexNoOat.art"): "";
    Copy(GetDexSrc2(), dex_location);

    uint64_t image_file_empty_profile = 0;
    if (app_image) {
      CompileProfileOdex(dex_location,
                         odex_location,
                         app_image_file,
                         /* use_fd */ false,
                         /* num_profile_classes */ 0);
      CheckValidity();
      ASSERT_TRUE(success_);
      // Don't check the result since CheckResult relies on the class being in the profile.
      image_file_empty_profile = GetImageObjectSectionSize(app_image_file);
      EXPECT_GT(image_file_empty_profile, 0u);
    }

    // Small profile.
    CompileProfileOdex(dex_location,
                       odex_location,
                       app_image_file,
                       /* use_fd */ false,
                       /* num_profile_classes */ 1);
    CheckValidity();
    ASSERT_TRUE(success_);
    CheckResult(dex_location, odex_location, app_image_file);

    if (app_image) {
      // Test that the profile made a difference by adding more classes.
      const uint64_t image_file_small_profile = GetImageObjectSectionSize(app_image_file);
      ASSERT_LT(image_file_empty_profile, image_file_small_profile);
    }
  }

  void RunTestVDex() {
    std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
    std::string odex_location = GetOdexDir() + "/DexOdexNoOat.odex";
    std::string vdex_location = GetOdexDir() + "/DexOdexNoOat.vdex";
    std::string app_image_file_name = GetOdexDir() + "/DexOdexNoOat.art";
    Copy(GetDexSrc2(), dex_location);

    std::unique_ptr<File> vdex_file1(OS::CreateEmptyFile(vdex_location.c_str()));
    CHECK(vdex_file1 != nullptr) << vdex_location;
    ScratchFile vdex_file2;
    {
      std::string input_vdex = "--input-vdex-fd=-1";
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file1->Fd());
      CompileProfileOdex(dex_location,
                         odex_location,
                         app_image_file_name,
                         /* use_fd */ true,
                         /* num_profile_classes */ 1,
                         { input_vdex, output_vdex });
      EXPECT_GT(vdex_file1->GetLength(), 0u);
    }
    {
      // Test that vdex and dexlayout fail gracefully.
      std::string input_vdex = StringPrintf("--input-vdex-fd=%d", vdex_file1->Fd());
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file2.GetFd());
      CompileProfileOdex(dex_location,
                         odex_location,
                         app_image_file_name,
                         /* use_fd */ true,
                         /* num_profile_classes */ 1,
                         { input_vdex, output_vdex },
                         /* expect_success */ true);
      EXPECT_GT(vdex_file2.GetFile()->GetLength(), 0u);
    }
    ASSERT_EQ(vdex_file1->FlushCloseOrErase(), 0) << "Could not flush and close vdex file";
    CheckValidity();
    ASSERT_TRUE(success_);
  }

  void CheckResult(const std::string& dex_location,
                   const std::string& odex_location,
                   const std::string& app_image_file_name) {
    // Host/target independent checks.
    std::string error_msg;
    std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                     odex_location.c_str(),
                                                     odex_location.c_str(),
                                                     nullptr,
                                                     nullptr,
                                                     false,
                                                     /*low_4gb*/false,
                                                     dex_location.c_str(),
                                                     &error_msg));
    ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;

    const char* location = dex_location.c_str();
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    const ArtDexFileLoader dex_file_loader;
    ASSERT_TRUE(dex_file_loader.Open(
        location, location, /* verify */ true, /* verify_checksum */ true, &error_msg, &dex_files));
    EXPECT_EQ(dex_files.size(), 1U);
    std::unique_ptr<const DexFile>& old_dex_file = dex_files[0];

    for (const OatDexFile* oat_dex_file : odex_file->GetOatDexFiles()) {
      std::unique_ptr<const DexFile> new_dex_file = oat_dex_file->OpenDexFile(&error_msg);
      ASSERT_TRUE(new_dex_file != nullptr);
      uint32_t class_def_count = new_dex_file->NumClassDefs();
      ASSERT_LT(class_def_count, std::numeric_limits<uint16_t>::max());
      ASSERT_GE(class_def_count, 2U);

      // Make sure the indexes stay the same.
      std::string old_class0 = old_dex_file->PrettyType(old_dex_file->GetClassDef(0).class_idx_);
      std::string old_class1 = old_dex_file->PrettyType(old_dex_file->GetClassDef(1).class_idx_);
      std::string new_class0 = new_dex_file->PrettyType(new_dex_file->GetClassDef(0).class_idx_);
      std::string new_class1 = new_dex_file->PrettyType(new_dex_file->GetClassDef(1).class_idx_);
      EXPECT_EQ(old_class0, new_class0);
      EXPECT_EQ(old_class1, new_class1);
    }

    EXPECT_EQ(odex_file->GetCompilerFilter(), CompilerFilter::kSpeedProfile);

    if (!app_image_file_name.empty()) {
      // Go peek at the image header to make sure it was large enough to contain the class.
      std::unique_ptr<File> file(OS::OpenFileForReading(app_image_file_name.c_str()));
      ImageHeader image_header;
      bool success = file->ReadFully(&image_header, sizeof(image_header));
      ASSERT_TRUE(success);
      ASSERT_TRUE(image_header.IsValid());
      EXPECT_GT(image_header.GetObjectsSection().Size(), 0u);
    }
  }

  // Check whether the dex2oat run was really successful.
  void CheckValidity() {
    if (kIsTargetBuild) {
      CheckTargetValidity();
    } else {
      CheckHostValidity();
    }
  }

  void CheckTargetValidity() {
    // TODO: Ignore for now.
  }

  // On the host, we can get the dex2oat output. Here, look for "dex2oat took."
  void CheckHostValidity() {
    EXPECT_NE(output_.find("dex2oat took"), std::string::npos) << output_;
  }
};

TEST_F(Dex2oatLayoutTest, TestLayout) {
  RunTest(/* app-image */ false);
}

TEST_F(Dex2oatLayoutTest, TestLayoutAppImage) {
  RunTest(/* app-image */ true);
}

TEST_F(Dex2oatLayoutTest, TestVdexLayout) {
  RunTestVDex();
}

class Dex2oatUnquickenTest : public Dex2oatTest {
 protected:
  void RunUnquickenMultiDex() {
    std::string dex_location = GetScratchDir() + "/UnquickenMultiDex.jar";
    std::string odex_location = GetOdexDir() + "/UnquickenMultiDex.odex";
    std::string vdex_location = GetOdexDir() + "/UnquickenMultiDex.vdex";
    Copy(GetTestDexFileName("MultiDex"), dex_location);

    std::unique_ptr<File> vdex_file1(OS::CreateEmptyFile(vdex_location.c_str()));
    CHECK(vdex_file1 != nullptr) << vdex_location;
    // Quicken the dex file into a vdex file.
    {
      std::string input_vdex = "--input-vdex-fd=-1";
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file1->Fd());
      GenerateOdexForTest(dex_location,
                          odex_location,
                          CompilerFilter::kQuicken,
                          { input_vdex, output_vdex },
                          /* expect_success */ true,
                          /* use_fd */ true);
      EXPECT_GT(vdex_file1->GetLength(), 0u);
    }
    // Unquicken by running the verify compiler filter on the vdex file.
    {
      std::string input_vdex = StringPrintf("--input-vdex-fd=%d", vdex_file1->Fd());
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file1->Fd());
      GenerateOdexForTest(dex_location,
                          odex_location,
                          CompilerFilter::kVerify,
                          { input_vdex, output_vdex, kDisableCompactDex },
                          /* expect_success */ true,
                          /* use_fd */ true);
    }
    ASSERT_EQ(vdex_file1->FlushCloseOrErase(), 0) << "Could not flush and close vdex file";
    CheckResult(dex_location, odex_location);
    ASSERT_TRUE(success_);
  }

  void RunUnquickenMultiDexCDex() {
    std::string dex_location = GetScratchDir() + "/UnquickenMultiDex.jar";
    std::string odex_location = GetOdexDir() + "/UnquickenMultiDex.odex";
    std::string odex_location2 = GetOdexDir() + "/UnquickenMultiDex2.odex";
    std::string vdex_location = GetOdexDir() + "/UnquickenMultiDex.vdex";
    std::string vdex_location2 = GetOdexDir() + "/UnquickenMultiDex2.vdex";
    Copy(GetTestDexFileName("MultiDex"), dex_location);

    std::unique_ptr<File> vdex_file1(OS::CreateEmptyFile(vdex_location.c_str()));
    std::unique_ptr<File> vdex_file2(OS::CreateEmptyFile(vdex_location2.c_str()));
    CHECK(vdex_file1 != nullptr) << vdex_location;
    CHECK(vdex_file2 != nullptr) << vdex_location2;

    // Quicken the dex file into a vdex file.
    {
      std::string input_vdex = "--input-vdex-fd=-1";
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file1->Fd());
      GenerateOdexForTest(dex_location,
                          odex_location,
                          CompilerFilter::kQuicken,
                          { input_vdex, output_vdex, "--compact-dex-level=fast"},
                          /* expect_success */ true,
                          /* use_fd */ true);
      EXPECT_GT(vdex_file1->GetLength(), 0u);
    }

    // Unquicken by running the verify compiler filter on the vdex file.
    {
      std::string input_vdex = StringPrintf("--input-vdex-fd=%d", vdex_file1->Fd());
      std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_file2->Fd());
      GenerateOdexForTest(dex_location,
                          odex_location2,
                          CompilerFilter::kVerify,
                          { input_vdex, output_vdex, "--compact-dex-level=none"},
                          /* expect_success */ true,
                          /* use_fd */ true);
    }
    ASSERT_EQ(vdex_file1->FlushCloseOrErase(), 0) << "Could not flush and close vdex file";
    ASSERT_EQ(vdex_file2->FlushCloseOrErase(), 0) << "Could not flush and close vdex file";
    CheckResult(dex_location, odex_location2);
    ASSERT_TRUE(success_);
  }

  void CheckResult(const std::string& dex_location, const std::string& odex_location) {
    std::string error_msg;
    std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                     odex_location.c_str(),
                                                     odex_location.c_str(),
                                                     nullptr,
                                                     nullptr,
                                                     false,
                                                     /*low_4gb*/false,
                                                     dex_location.c_str(),
                                                     &error_msg));
    ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;
    ASSERT_GE(odex_file->GetOatDexFiles().size(), 1u);

    // Iterate over the dex files and ensure there is no quickened instruction.
    for (const OatDexFile* oat_dex_file : odex_file->GetOatDexFiles()) {
      std::unique_ptr<const DexFile> dex_file = oat_dex_file->OpenDexFile(&error_msg);
      for (uint32_t i = 0; i < dex_file->NumClassDefs(); ++i) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
        const uint8_t* class_data = dex_file->GetClassData(class_def);
        if (class_data != nullptr) {
          for (ClassDataItemIterator class_it(*dex_file, class_data);
               class_it.HasNext();
               class_it.Next()) {
            if (class_it.IsAtMethod() && class_it.GetMethodCodeItem() != nullptr) {
              for (const DexInstructionPcPair& inst :
                       CodeItemInstructionAccessor(*dex_file, class_it.GetMethodCodeItem())) {
                ASSERT_FALSE(inst->IsQuickened()) << inst->Opcode() << " " << output_;
              }
            }
          }
        }
      }
    }
  }
};

TEST_F(Dex2oatUnquickenTest, UnquickenMultiDex) {
  RunUnquickenMultiDex();
}

TEST_F(Dex2oatUnquickenTest, UnquickenMultiDexCDex) {
  RunUnquickenMultiDexCDex();
}

class Dex2oatWatchdogTest : public Dex2oatTest {
 protected:
  void RunTest(bool expect_success, const std::vector<std::string>& extra_args = {}) {
    std::string dex_location = GetScratchDir() + "/Dex2OatSwapTest.jar";
    std::string odex_location = GetOdexDir() + "/Dex2OatSwapTest.odex";

    Copy(GetTestDexFileName(), dex_location);

    std::vector<std::string> copy(extra_args);

    std::string swap_location = GetOdexDir() + "/Dex2OatSwapTest.odex.swap";
    copy.push_back("--swap-file=" + swap_location);
    copy.push_back("-j512");  // Excessive idle threads just slow down dex2oat.
    GenerateOdexForTest(dex_location,
                        odex_location,
                        CompilerFilter::kSpeed,
                        copy,
                        expect_success);
  }

  std::string GetTestDexFileName() {
    return GetDexSrc1();
  }
};

TEST_F(Dex2oatWatchdogTest, TestWatchdogOK) {
  // Check with default.
  RunTest(true);

  // Check with ten minutes.
  RunTest(true, { "--watchdog-timeout=600000" });
}

TEST_F(Dex2oatWatchdogTest, TestWatchdogTrigger) {
  TEST_DISABLED_FOR_MEMORY_TOOL_VALGRIND();  // b/63052624

  // The watchdog is independent of dex2oat and will not delete intermediates. It is possible
  // that the compilation succeeds and the file is completely written by the time the watchdog
  // kills dex2oat (but the dex2oat threads must have been scheduled pretty badly).
  test_accepts_odex_file_on_failure = true;

  // Check with ten milliseconds.
  RunTest(false, { "--watchdog-timeout=10" });
}

class Dex2oatReturnCodeTest : public Dex2oatTest {
 protected:
  int RunTest(const std::vector<std::string>& extra_args = {}) {
    std::string dex_location = GetScratchDir() + "/Dex2OatSwapTest.jar";
    std::string odex_location = GetOdexDir() + "/Dex2OatSwapTest.odex";

    Copy(GetTestDexFileName(), dex_location);

    std::string error_msg;
    return GenerateOdexForTestWithStatus({dex_location},
                                         odex_location,
                                         CompilerFilter::kSpeed,
                                         &error_msg,
                                         extra_args);
  }

  std::string GetTestDexFileName() {
    return GetDexSrc1();
  }
};

TEST_F(Dex2oatReturnCodeTest, TestCreateRuntime) {
  TEST_DISABLED_FOR_MEMORY_TOOL();  // b/19100793
  int status = RunTest({ "--boot-image=/this/does/not/exist/yolo.oat" });
  EXPECT_EQ(static_cast<int>(dex2oat::ReturnCode::kCreateRuntime), WEXITSTATUS(status)) << output_;
}

class Dex2oatClassLoaderContextTest : public Dex2oatTest {
 protected:
  void RunTest(const char* class_loader_context,
               const char* expected_classpath_key,
               bool expected_success,
               bool use_second_source = false) {
    std::string dex_location = GetUsedDexLocation();
    std::string odex_location = GetUsedOatLocation();

    Copy(use_second_source ? GetDexSrc2() : GetDexSrc1(), dex_location);

    std::string error_msg;
    std::vector<std::string> extra_args;
    if (class_loader_context != nullptr) {
      extra_args.push_back(std::string("--class-loader-context=") + class_loader_context);
    }
    auto check_oat = [expected_classpath_key](const OatFile& oat_file) {
      ASSERT_TRUE(expected_classpath_key != nullptr);
      const char* classpath = oat_file.GetOatHeader().GetStoreValueByKey(OatHeader::kClassPathKey);
      ASSERT_TRUE(classpath != nullptr);
      ASSERT_STREQ(expected_classpath_key, classpath);
    };

    GenerateOdexForTest(dex_location,
                        odex_location,
                        CompilerFilter::kQuicken,
                        extra_args,
                        expected_success,
                        /*use_fd*/ false,
                        check_oat);
  }

  std::string GetUsedDexLocation() {
    return GetScratchDir() + "/Context.jar";
  }

  std::string GetUsedOatLocation() {
    return GetOdexDir() + "/Context.odex";
  }

  const char* kEmptyClassPathKey = "PCL[]";
};

TEST_F(Dex2oatClassLoaderContextTest, InvalidContext) {
  RunTest("Invalid[]", /*expected_classpath_key*/ nullptr, /*expected_success*/ false);
}

TEST_F(Dex2oatClassLoaderContextTest, EmptyContext) {
  RunTest("PCL[]", kEmptyClassPathKey, /*expected_success*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, SpecialContext) {
  RunTest(OatFile::kSpecialSharedLibrary,
          OatFile::kSpecialSharedLibrary,
          /*expected_success*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, ContextWithTheSourceDexFiles) {
  std::string context = "PCL[" + GetUsedDexLocation() + "]";
  RunTest(context.c_str(), kEmptyClassPathKey, /*expected_success*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, ContextWithOtherDexFiles) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("Nested");

  std::string context = "PCL[" + dex_files[0]->GetLocation() + "]";
  std::string expected_classpath_key = "PCL[" +
      dex_files[0]->GetLocation() + "*" + std::to_string(dex_files[0]->GetLocationChecksum()) + "]";
  RunTest(context.c_str(), expected_classpath_key.c_str(), true);
}

TEST_F(Dex2oatClassLoaderContextTest, ContextWithStrippedDexFiles) {
  std::string stripped_classpath = GetScratchDir() + "/stripped_classpath.jar";
  Copy(GetStrippedDexSrc1(), stripped_classpath);

  std::string context = "PCL[" + stripped_classpath + "]";
  // Expect an empty context because stripped dex files cannot be open.
  RunTest(context.c_str(), kEmptyClassPathKey , /*expected_success*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, ContextWithStrippedDexFilesBackedByOdex) {
  std::string stripped_classpath = GetScratchDir() + "/stripped_classpath.jar";
  std::string odex_for_classpath = GetOdexDir() + "/stripped_classpath.odex";

  Copy(GetDexSrc1(), stripped_classpath);

  GenerateOdexForTest(stripped_classpath,
                      odex_for_classpath,
                      CompilerFilter::kQuicken,
                      {},
                      true);

  // Strip the dex file
  Copy(GetStrippedDexSrc1(), stripped_classpath);

  std::string context = "PCL[" + stripped_classpath + "]";
  std::string expected_classpath_key;
  {
    // Open the oat file to get the expected classpath.
    OatFileAssistant oat_file_assistant(stripped_classpath.c_str(), kRuntimeISA, false, false);
    std::unique_ptr<OatFile> oat_file(oat_file_assistant.GetBestOatFile());
    std::vector<std::unique_ptr<const DexFile>> oat_dex_files =
        OatFileAssistant::LoadDexFiles(*oat_file, stripped_classpath.c_str());
    expected_classpath_key = "PCL[";
    for (size_t i = 0; i < oat_dex_files.size(); i++) {
      if (i > 0) {
        expected_classpath_key + ":";
      }
      expected_classpath_key += oat_dex_files[i]->GetLocation() + "*" +
          std::to_string(oat_dex_files[i]->GetLocationChecksum());
    }
    expected_classpath_key += "]";
  }

  RunTest(context.c_str(),
          expected_classpath_key.c_str(),
          /*expected_success*/ true,
          /*use_second_source*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, ContextWithNotExistentDexFiles) {
  std::string context = "PCL[does_not_exists.dex]";
  // Expect an empty context because stripped dex files cannot be open.
  RunTest(context.c_str(), kEmptyClassPathKey, /*expected_success*/ true);
}

TEST_F(Dex2oatClassLoaderContextTest, ChainContext) {
  std::vector<std::unique_ptr<const DexFile>> dex_files1 = OpenTestDexFiles("Nested");
  std::vector<std::unique_ptr<const DexFile>> dex_files2 = OpenTestDexFiles("MultiDex");

  std::string context = "PCL[" + GetTestDexFileName("Nested") + "];" +
      "DLC[" + GetTestDexFileName("MultiDex") + "]";
  std::string expected_classpath_key = "PCL[" + CreateClassPathWithChecksums(dex_files1) + "];" +
      "DLC[" + CreateClassPathWithChecksums(dex_files2) + "]";

  RunTest(context.c_str(), expected_classpath_key.c_str(), true);
}

class Dex2oatDeterminism : public Dex2oatTest {};

TEST_F(Dex2oatDeterminism, UnloadCompile) {
  if (!kUseReadBarrier &&
      gc::kCollectorTypeDefault != gc::kCollectorTypeCMS &&
      gc::kCollectorTypeDefault != gc::kCollectorTypeMS) {
    LOG(INFO) << "Test requires determinism support.";
    return;
  }
  Runtime* const runtime = Runtime::Current();
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  const std::string base_vdex_name = out_dir + "/base.vdex";
  const std::string unload_oat_name = out_dir + "/unload.oat";
  const std::string unload_vdex_name = out_dir + "/unload.vdex";
  const std::string no_unload_oat_name = out_dir + "/nounload.oat";
  const std::string no_unload_vdex_name = out_dir + "/nounload.vdex";
  const std::string app_image_name = out_dir + "/unload.art";
  std::string error_msg;
  const std::vector<gc::space::ImageSpace*>& spaces = runtime->GetHeap()->GetBootImageSpaces();
  ASSERT_GT(spaces.size(), 0u);
  const std::string image_location = spaces[0]->GetImageLocation();
  // Without passing in an app image, it will unload in between compilations.
  const int res = GenerateOdexForTestWithStatus(
      GetLibCoreDexFileNames(),
      base_oat_name,
      CompilerFilter::Filter::kQuicken,
      &error_msg,
      {"--force-determinism", "--avoid-storing-invocation"});
  EXPECT_EQ(res, 0);
  Copy(base_oat_name, unload_oat_name);
  Copy(base_vdex_name, unload_vdex_name);
  std::unique_ptr<File> unload_oat(OS::OpenFileForReading(unload_oat_name.c_str()));
  std::unique_ptr<File> unload_vdex(OS::OpenFileForReading(unload_vdex_name.c_str()));
  ASSERT_TRUE(unload_oat != nullptr);
  ASSERT_TRUE(unload_vdex != nullptr);
  EXPECT_GT(unload_oat->GetLength(), 0u);
  EXPECT_GT(unload_vdex->GetLength(), 0u);
  // Regenerate with an app image to disable the dex2oat unloading and verify that the output is
  // the same.
  const int res2 = GenerateOdexForTestWithStatus(
      GetLibCoreDexFileNames(),
      base_oat_name,
      CompilerFilter::Filter::kQuicken,
      &error_msg,
      {"--force-determinism", "--avoid-storing-invocation", "--app-image-file=" + app_image_name});
  EXPECT_EQ(res2, 0);
  Copy(base_oat_name, no_unload_oat_name);
  Copy(base_vdex_name, no_unload_vdex_name);
  std::unique_ptr<File> no_unload_oat(OS::OpenFileForReading(no_unload_oat_name.c_str()));
  std::unique_ptr<File> no_unload_vdex(OS::OpenFileForReading(no_unload_vdex_name.c_str()));
  ASSERT_TRUE(no_unload_oat != nullptr);
  ASSERT_TRUE(no_unload_vdex != nullptr);
  EXPECT_GT(no_unload_oat->GetLength(), 0u);
  EXPECT_GT(no_unload_vdex->GetLength(), 0u);
  // Verify that both of the files are the same (odex and vdex).
  EXPECT_EQ(unload_oat->GetLength(), no_unload_oat->GetLength());
  EXPECT_EQ(unload_vdex->GetLength(), no_unload_vdex->GetLength());
  EXPECT_EQ(unload_oat->Compare(no_unload_oat.get()), 0)
      << unload_oat_name << " " << no_unload_oat_name;
  EXPECT_EQ(unload_vdex->Compare(no_unload_vdex.get()), 0)
      << unload_vdex_name << " " << no_unload_vdex_name;
  // App image file.
  std::unique_ptr<File> app_image_file(OS::OpenFileForReading(app_image_name.c_str()));
  ASSERT_TRUE(app_image_file != nullptr);
  EXPECT_GT(app_image_file->GetLength(), 0u);
}

// Test that dexlayout section info is correctly written to the oat file for profile based
// compilation.
TEST_F(Dex2oatTest, LayoutSections) {
  using Hotness = ProfileCompilationInfo::MethodHotness;
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
  ScratchFile profile_file;
  // We can only layout method indices with code items, figure out which ones have this property
  // first.
  std::vector<uint16_t> methods;
  {
    const DexFile::TypeId* type_id = dex->FindTypeId("LManyMethods;");
    dex::TypeIndex type_idx = dex->GetIndexForTypeId(*type_id);
    const DexFile::ClassDef* class_def = dex->FindClassDef(type_idx);
    ClassDataItemIterator it(*dex, dex->GetClassData(*class_def));
    it.SkipAllFields();
    std::set<size_t> code_item_offsets;
    for (; it.HasNextMethod(); it.Next()) {
      const uint16_t method_idx = it.GetMemberIndex();
      const size_t code_item_offset = it.GetMethodCodeItemOffset();
      if (code_item_offsets.insert(code_item_offset).second) {
        // Unique code item, add the method index.
        methods.push_back(method_idx);
      }
    }
    DCHECK(!it.HasNext());
  }
  ASSERT_GE(methods.size(), 8u);
  std::vector<uint16_t> hot_methods = {methods[1], methods[3], methods[5]};
  std::vector<uint16_t> startup_methods = {methods[1], methods[2], methods[7]};
  std::vector<uint16_t> post_methods = {methods[0], methods[2], methods[6]};
  // Here, we build the profile from the method lists.
  ProfileCompilationInfo info;
  info.AddMethodsForDex(
      static_cast<Hotness::Flag>(Hotness::kFlagHot | Hotness::kFlagStartup),
      dex.get(),
      hot_methods.begin(),
      hot_methods.end());
  info.AddMethodsForDex(
      Hotness::kFlagStartup,
      dex.get(),
      startup_methods.begin(),
      startup_methods.end());
  info.AddMethodsForDex(
      Hotness::kFlagPostStartup,
      dex.get(),
      post_methods.begin(),
      post_methods.end());
  for (uint16_t id : hot_methods) {
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsHot());
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
  }
  for (uint16_t id : startup_methods) {
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsStartup());
  }
  for (uint16_t id : post_methods) {
    EXPECT_TRUE(info.GetMethodHotness(MethodReference(dex.get(), id)).IsPostStartup());
  }
  // Save the profile since we want to use it with dex2oat to produce an oat file.
  ASSERT_TRUE(info.Save(profile_file.GetFd()));
  // Generate a profile based odex.
  const std::string dir = GetScratchDir();
  const std::string oat_filename = dir + "/base.oat";
  const std::string vdex_filename = dir + "/base.vdex";
  std::string error_msg;
  const int res = GenerateOdexForTestWithStatus(
      {dex->GetLocation()},
      oat_filename,
      CompilerFilter::Filter::kQuicken,
      &error_msg,
      {"--profile-file=" + profile_file.GetFilename()});
  EXPECT_EQ(res, 0);

  // Open our generated oat file.
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   oat_filename.c_str(),
                                                   oat_filename.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   dex->GetLocation().c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  std::vector<const OatDexFile*> oat_dex_files = odex_file->GetOatDexFiles();
  ASSERT_EQ(oat_dex_files.size(), 1u);
  // Check that the code sections match what we expect.
  for (const OatDexFile* oat_dex : oat_dex_files) {
    const DexLayoutSections* const sections = oat_dex->GetDexLayoutSections();
    // Testing of logging the sections.
    ASSERT_TRUE(sections != nullptr);
    LOG(INFO) << *sections;

    // Load the sections into temporary variables for convenience.
    const DexLayoutSection& code_section =
        sections->sections_[static_cast<size_t>(DexLayoutSections::SectionType::kSectionTypeCode)];
    const DexLayoutSection::Subsection& section_hot_code =
        code_section.parts_[static_cast<size_t>(LayoutType::kLayoutTypeHot)];
    const DexLayoutSection::Subsection& section_sometimes_used =
        code_section.parts_[static_cast<size_t>(LayoutType::kLayoutTypeSometimesUsed)];
    const DexLayoutSection::Subsection& section_startup_only =
        code_section.parts_[static_cast<size_t>(LayoutType::kLayoutTypeStartupOnly)];
    const DexLayoutSection::Subsection& section_unused =
        code_section.parts_[static_cast<size_t>(LayoutType::kLayoutTypeUnused)];

    // All the sections should be non-empty.
    EXPECT_GT(section_hot_code.Size(), 0u);
    EXPECT_GT(section_sometimes_used.Size(), 0u);
    EXPECT_GT(section_startup_only.Size(), 0u);
    EXPECT_GT(section_unused.Size(), 0u);

    // Open the dex file since we need to peek at the code items to verify the layout matches what
    // we expect.
    std::unique_ptr<const DexFile> dex_file(oat_dex->OpenDexFile(&error_msg));
    ASSERT_TRUE(dex_file != nullptr) << error_msg;
    const DexFile::TypeId* type_id = dex_file->FindTypeId("LManyMethods;");
    ASSERT_TRUE(type_id != nullptr);
    dex::TypeIndex type_idx = dex_file->GetIndexForTypeId(*type_id);
    const DexFile::ClassDef* class_def = dex_file->FindClassDef(type_idx);
    ASSERT_TRUE(class_def != nullptr);

    // Count how many code items are for each category, there should be at least one per category.
    size_t hot_count = 0;
    size_t post_startup_count = 0;
    size_t startup_count = 0;
    size_t unused_count = 0;
    // Visit all of the methdos of the main class and cross reference the method indices to their
    // corresponding code item offsets to verify the layout.
    ClassDataItemIterator it(*dex_file, dex_file->GetClassData(*class_def));
    it.SkipAllFields();
    for (; it.HasNextMethod(); it.Next()) {
      const size_t method_idx = it.GetMemberIndex();
      const size_t code_item_offset = it.GetMethodCodeItemOffset();
      const bool is_hot = ContainsElement(hot_methods, method_idx);
      const bool is_startup = ContainsElement(startup_methods, method_idx);
      const bool is_post_startup = ContainsElement(post_methods, method_idx);
      if (is_hot) {
        // Hot is highest precedence, check that the hot methods are in the hot section.
        EXPECT_TRUE(section_hot_code.Contains(code_item_offset));
        ++hot_count;
      } else if (is_post_startup) {
        // Post startup is sometimes used section.
        EXPECT_TRUE(section_sometimes_used.Contains(code_item_offset));
        ++post_startup_count;
      } else if (is_startup) {
        // Startup at this point means not hot or post startup, these must be startup only then.
        EXPECT_TRUE(section_startup_only.Contains(code_item_offset));
        ++startup_count;
      } else {
        if (section_unused.Contains(code_item_offset)) {
          // If no flags are set, the method should be unused ...
          ++unused_count;
        } else {
          // or this method is part of the last code item and the end is 4 byte aligned.
          ClassDataItemIterator it2(*dex_file, dex_file->GetClassData(*class_def));
          it2.SkipAllFields();
          for (; it2.HasNextMethod(); it2.Next()) {
              EXPECT_LE(it2.GetMethodCodeItemOffset(), code_item_offset);
          }
          uint32_t code_item_size = dex_file->FindCodeItemOffset(*class_def, method_idx);
          EXPECT_EQ((code_item_offset + code_item_size) % 4, 0u);
        }
      }
    }
    DCHECK(!it.HasNext());
    EXPECT_GT(hot_count, 0u);
    EXPECT_GT(post_startup_count, 0u);
    EXPECT_GT(startup_count, 0u);
    EXPECT_GT(unused_count, 0u);
  }
}

// Test that generating compact dex works.
TEST_F(Dex2oatTest, GenerateCompactDex) {
  // Generate a compact dex based odex.
  const std::string dir = GetScratchDir();
  const std::string oat_filename = dir + "/base.oat";
  const std::string vdex_filename = dir + "/base.vdex";
  const std::string dex_location = GetTestDexFileName("MultiDex");
  std::string error_msg;
  const int res = GenerateOdexForTestWithStatus(
      { dex_location },
      oat_filename,
      CompilerFilter::Filter::kQuicken,
      &error_msg,
      {"--compact-dex-level=fast"});
  EXPECT_EQ(res, 0);
  // Open our generated oat file.
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   oat_filename.c_str(),
                                                   oat_filename.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   dex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  std::vector<const OatDexFile*> oat_dex_files = odex_file->GetOatDexFiles();
  ASSERT_GT(oat_dex_files.size(), 1u);
  // Check that each dex is a compact dex file.
  std::vector<std::unique_ptr<const CompactDexFile>> compact_dex_files;
  for (const OatDexFile* oat_dex : oat_dex_files) {
    std::unique_ptr<const DexFile> dex_file(oat_dex->OpenDexFile(&error_msg));
    ASSERT_TRUE(dex_file != nullptr) << error_msg;
    ASSERT_TRUE(dex_file->IsCompactDexFile());
    compact_dex_files.push_back(
        std::unique_ptr<const CompactDexFile>(dex_file.release()->AsCompactDexFile()));
  }
  for (const std::unique_ptr<const CompactDexFile>& dex_file : compact_dex_files) {
    // Test that every code item is in the owned section.
    const CompactDexFile::Header& header = dex_file->GetHeader();
    EXPECT_LE(header.OwnedDataBegin(), header.OwnedDataEnd());
    EXPECT_LE(header.OwnedDataBegin(), header.data_size_);
    EXPECT_LE(header.OwnedDataEnd(), header.data_size_);
    for (uint32_t i = 0; i < dex_file->NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
      class_def.VisitMethods(dex_file.get(), [&](const ClassDataItemIterator& it) {
        if (it.GetMethodCodeItemOffset() != 0u) {
          ASSERT_GE(it.GetMethodCodeItemOffset(), header.OwnedDataBegin());
          ASSERT_LT(it.GetMethodCodeItemOffset(), header.OwnedDataEnd());
        }
      });
    }
    // Test that the owned sections don't overlap.
    for (const std::unique_ptr<const CompactDexFile>& other_dex : compact_dex_files) {
      if (dex_file != other_dex) {
        ASSERT_TRUE(
            (dex_file->GetHeader().OwnedDataBegin() >= other_dex->GetHeader().OwnedDataEnd()) ||
            (dex_file->GetHeader().OwnedDataEnd() <= other_dex->GetHeader().OwnedDataBegin()));
      }
    }
  }
}

class Dex2oatVerifierAbort : public Dex2oatTest {};

TEST_F(Dex2oatVerifierAbort, HardFail) {
  // Use VerifierDeps as it has hard-failing classes.
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("VerifierDeps"));
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  std::string error_msg;
  const int res_fail = GenerateOdexForTestWithStatus(
        {dex->GetLocation()},
        base_oat_name,
        CompilerFilter::Filter::kQuicken,
        &error_msg,
        {"--abort-on-hard-verifier-error"});
  EXPECT_NE(0, res_fail);

  const int res_no_fail = GenerateOdexForTestWithStatus(
        {dex->GetLocation()},
        base_oat_name,
        CompilerFilter::Filter::kQuicken,
        &error_msg,
        {"--no-abort-on-hard-verifier-error"});
  EXPECT_EQ(0, res_no_fail);
}

TEST_F(Dex2oatVerifierAbort, SoftFail) {
  // Use VerifierDepsMulti as it has hard-failing classes.
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("VerifierDepsMulti"));
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  std::string error_msg;
  const int res_fail = GenerateOdexForTestWithStatus(
        {dex->GetLocation()},
        base_oat_name,
        CompilerFilter::Filter::kQuicken,
        &error_msg,
        {"--abort-on-soft-verifier-error"});
  EXPECT_NE(0, res_fail);

  const int res_no_fail = GenerateOdexForTestWithStatus(
        {dex->GetLocation()},
        base_oat_name,
        CompilerFilter::Filter::kQuicken,
        &error_msg,
        {"--no-abort-on-soft-verifier-error"});
  EXPECT_EQ(0, res_no_fail);
}

class Dex2oatDedupeCode : public Dex2oatTest {};

TEST_F(Dex2oatDedupeCode, DedupeTest) {
  // Use MyClassNatives. It has lots of native methods that will produce deduplicate-able code.
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("MyClassNatives"));
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  size_t no_dedupe_size = 0;
  GenerateOdexForTest(dex->GetLocation(),
                      base_oat_name,
                      CompilerFilter::Filter::kSpeed,
                      { "--deduplicate-code=false" },
                      true,  // expect_success
                      false,  // use_fd
                      [&no_dedupe_size](const OatFile& o) {
                        no_dedupe_size = o.Size();
                      });

  size_t dedupe_size = 0;
  GenerateOdexForTest(dex->GetLocation(),
                      base_oat_name,
                      CompilerFilter::Filter::kSpeed,
                      { "--deduplicate-code=true" },
                      true,  // expect_success
                      false,  // use_fd
                      [&dedupe_size](const OatFile& o) {
                        dedupe_size = o.Size();
                      });

  EXPECT_LT(dedupe_size, no_dedupe_size);
}

TEST_F(Dex2oatTest, UncompressedTest) {
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("MainUncompressed"));
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  GenerateOdexForTest(dex->GetLocation(),
                      base_oat_name,
                      CompilerFilter::Filter::kQuicken,
                      { },
                      true,  // expect_success
                      false,  // use_fd
                      [](const OatFile& o) {
                        CHECK(!o.ContainsDexCode());
                      });
}

TEST_F(Dex2oatTest, EmptyUncompressedDexTest) {
  std::string out_dir = GetScratchDir();
  const std::string base_oat_name = out_dir + "/base.oat";
  std::string error_msg;
  int status = GenerateOdexForTestWithStatus(
      { GetTestDexFileName("MainEmptyUncompressed") },
      base_oat_name,
      CompilerFilter::Filter::kQuicken,
      &error_msg,
      { },
      /*use_fd*/ false);
  // Expect to fail with code 1 and not SIGSEGV or SIGABRT.
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 1) << error_msg;
}

// Dex file that has duplicate methods have different code items and debug info.
static const char kDuplicateMethodInputDex[] =
    "ZGV4CjAzOQDEy8VPdj4qHpgPYFWtLCtOykfFP4kB8tGYDAAAcAAAAHhWNBIAAAAAAAAAANALAABI"
    "AAAAcAAAAA4AAACQAQAABQAAAMgBAAANAAAABAIAABkAAABsAgAABAAAADQDAADgCAAAuAMAADgI"
    "AABCCAAASggAAE8IAABcCAAAaggAAHkIAACICAAAlggAAKQIAACyCAAAwAgAAM4IAADcCAAA6ggA"
    "APgIAAD7CAAA/wgAABcJAAAuCQAARQkAAFQJAAB4CQAAmAkAALsJAADSCQAA5gkAAPoJAAAVCgAA"
    "KQoAADsKAABCCgAASgoAAFIKAABbCgAAZAoAAGwKAAB0CgAAfAoAAIQKAACMCgAAlAoAAJwKAACk"
    "CgAArQoAALcKAADACgAAwwoAAMcKAADcCgAA6QoAAPEKAAD3CgAA/QoAAAMLAAAJCwAAEAsAABcL"
    "AAAdCwAAIwsAACkLAAAvCwAANQsAADsLAABBCwAARwsAAE0LAABSCwAAWwsAAF4LAABoCwAAbwsA"
    "ABEAAAASAAAAEwAAABQAAAAVAAAAFgAAABcAAAAYAAAAGQAAABoAAAAbAAAAHAAAAC4AAAAwAAAA"
    "DwAAAAkAAAAAAAAAEAAAAAoAAACoBwAALgAAAAwAAAAAAAAALwAAAAwAAACoBwAALwAAAAwAAACw"
    "BwAAAgAJADUAAAACAAkANgAAAAIACQA3AAAAAgAJADgAAAACAAkAOQAAAAIACQA6AAAAAgAJADsA"
    "AAACAAkAPAAAAAIACQA9AAAAAgAJAD4AAAACAAkAPwAAAAIACQBAAAAACwAHAEIAAAAAAAIAAQAA"
    "AAAAAwAeAAAAAQACAAEAAAABAAMAHgAAAAIAAgAAAAAAAgACAAEAAAADAAIAAQAAAAMAAgAfAAAA"
    "AwACACAAAAADAAIAIQAAAAMAAgAiAAAAAwACACMAAAADAAIAJAAAAAMAAgAlAAAAAwACACYAAAAD"
    "AAIAJwAAAAMAAgAoAAAAAwACACkAAAADAAIAKgAAAAMABAA0AAAABwADAEMAAAAIAAIAAQAAAAoA"
    "AgABAAAACgABADIAAAAKAAAARQAAAAAAAAAAAAAACAAAAAAAAAAdAAAAaAcAALYHAAAAAAAAAQAA"
    "AAAAAAAIAAAAAAAAAB0AAAB4BwAAxAcAAAAAAAACAAAAAAAAAAgAAAAAAAAAHQAAAIgHAADSBwAA"
    "AAAAAAMAAAAAAAAACAAAAAAAAAAdAAAAmAcAAPoHAAAAAAAAAAAAAAEAAAAAAAAArAYAADEAAAAa"
    "AAMAaQAAABoABABpAAEAGgAHAGkABAAaAAgAaQAFABoACQBpAAYAGgAKAGkABwAaAAsAaQAIABoA"
    "DABpAAkAGgANAGkACgAaAA4AaQALABoABQBpAAIAGgAGAGkAAwAOAAAAAQABAAEAAACSBgAABAAA"
    "AHAQFQAAAA4ABAABAAIAAACWBgAAFwAAAGIADAAiAQoAcBAWAAEAGgICAG4gFwAhAG4gFwAxAG4Q"
    "GAABAAwBbiAUABAADgAAAAEAAQABAAAAngYAAAQAAABwEBUAAAAOAAIAAQACAAAAogYAAAYAAABi"
    "AAwAbiAUABAADgABAAEAAQAAAKgGAAAEAAAAcBAVAAAADgABAAEAAQAAALsGAAAEAAAAcBAVAAAA"
    "DgABAAAAAQAAAL8GAAAGAAAAYgAAAHEQAwAAAA4AAQAAAAEAAADEBgAABgAAAGIAAQBxEAMAAAAO"
    "AAEAAAABAAAA8QYAAAYAAABiAAIAcRABAAAADgABAAAAAQAAAPYGAAAGAAAAYgADAHEQAwAAAA4A"
    "AQAAAAEAAADJBgAABgAAAGIABABxEAMAAAAOAAEAAAABAAAAzgYAAAYAAABiAAEAcRADAAAADgAB"
    "AAAAAQAAANMGAAAGAAAAYgAGAHEQAwAAAA4AAQAAAAEAAADYBgAABgAAAGIABwBxEAMAAAAOAAEA"
    "AAABAAAA3QYAAAYAAABiAAgAcRABAAAADgABAAAAAQAAAOIGAAAGAAAAYgAJAHEQAwAAAA4AAQAA"
    "AAEAAADnBgAABgAAAGIACgBxEAMAAAAOAAEAAAABAAAA7AYAAAYAAABiAAsAcRABAAAADgABAAEA"
    "AAAAAPsGAAAlAAAAcQAHAAAAcQAIAAAAcQALAAAAcQAMAAAAcQANAAAAcQAOAAAAcQAPAAAAcQAQ"
    "AAAAcQARAAAAcQASAAAAcQAJAAAAcQAKAAAADgAnAA4AKQFFDgEWDwAhAA4AIwFFDloAEgAOABMA"
    "DktLS0tLS0tLS0tLABEADgAuAA5aADIADloANgAOWgA6AA5aAD4ADloAQgAOWgBGAA5aAEoADloA"
    "TgAOWgBSAA5aAFYADloAWgAOWgBeATQOPDw8PDw8PDw8PDw8AAIEAUYYAwIFAjEECEEXLAIFAjEE"
    "CEEXKwIFAjEECEEXLQIGAUYcAxgAGAEYAgAAAAIAAAAMBwAAEgcAAAIAAAAMBwAAGwcAAAIAAAAM"
    "BwAAJAcAAAEAAAAtBwAAPAcAAAAAAAAAAAAAAAAAAEgHAAAAAAAAAAAAAAAAAABUBwAAAAAAAAAA"
    "AAAAAAAAYAcAAAAAAAAAAAAAAAAAAAEAAAAJAAAAAQAAAA0AAAACAACAgASsCAEIxAgAAAIAAoCA"
    "BIQJAQicCQwAAgAACQEJAQkBCQEJAQkBCQEJAQkBCQEJAQkEiIAEuAcBgIAEuAkAAA4ABoCABNAJ"
    "AQnoCQAJhAoACaAKAAm8CgAJ2AoACfQKAAmQCwAJrAsACcgLAAnkCwAJgAwACZwMAAm4DAg8Y2xp"
    "bml0PgAGPGluaXQ+AANBQUEAC0hlbGxvIFdvcmxkAAxIZWxsbyBXb3JsZDEADUhlbGxvIFdvcmxk"
    "MTAADUhlbGxvIFdvcmxkMTEADEhlbGxvIFdvcmxkMgAMSGVsbG8gV29ybGQzAAxIZWxsbyBXb3Js"
    "ZDQADEhlbGxvIFdvcmxkNQAMSGVsbG8gV29ybGQ2AAxIZWxsbyBXb3JsZDcADEhlbGxvIFdvcmxk"
    "OAAMSGVsbG8gV29ybGQ5AAFMAAJMTAAWTE1hbnlNZXRob2RzJFByaW50ZXIyOwAVTE1hbnlNZXRo"
    "b2RzJFByaW50ZXI7ABVMTWFueU1ldGhvZHMkU3RyaW5nczsADUxNYW55TWV0aG9kczsAIkxkYWx2"
    "aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNs"
    "YXNzOwAhTGRhbHZpay9hbm5vdGF0aW9uL01lbWJlckNsYXNzZXM7ABVMamF2YS9pby9QcmludFN0"
    "cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABlMamF2YS9sYW5n"
    "L1N0cmluZ0J1aWxkZXI7ABJMamF2YS9sYW5nL1N5c3RlbTsAEE1hbnlNZXRob2RzLmphdmEABVBy"
    "aW50AAZQcmludDAABlByaW50MQAHUHJpbnQxMAAHUHJpbnQxMQAGUHJpbnQyAAZQcmludDMABlBy"
    "aW50NAAGUHJpbnQ1AAZQcmludDYABlByaW50NwAGUHJpbnQ4AAZQcmludDkAB1ByaW50ZXIACFBy"
    "aW50ZXIyAAdTdHJpbmdzAAFWAAJWTAATW0xqYXZhL2xhbmcvU3RyaW5nOwALYWNjZXNzRmxhZ3MA"
    "BmFwcGVuZAAEYXJncwAEbWFpbgAEbXNnMAAEbXNnMQAFbXNnMTAABW1zZzExAARtc2cyAARtc2cz"
    "AARtc2c0AARtc2c1AARtc2c2AARtc2c3AARtc2c4AARtc2c5AARuYW1lAANvdXQAB3ByaW50bG4A"
    "AXMACHRvU3RyaW5nAAV2YWx1ZQBffn5EOHsibWluLWFwaSI6MTAwMDAsInNoYS0xIjoiZmViODZj"
    "MDA2ZWZhY2YxZDc5ODRiODVlMTc5MGZlZjdhNzY3YWViYyIsInZlcnNpb24iOiJ2MS4xLjUtZGV2"
    "In0AEAAAAAAAAAABAAAAAAAAAAEAAABIAAAAcAAAAAIAAAAOAAAAkAEAAAMAAAAFAAAAyAEAAAQA"
    "AAANAAAABAIAAAUAAAAZAAAAbAIAAAYAAAAEAAAANAMAAAEgAAAUAAAAuAMAAAMgAAAUAAAAkgYA"
    "AAQgAAAFAAAADAcAAAMQAAAEAAAAOQcAAAYgAAAEAAAAaAcAAAEQAAACAAAAqAcAAAAgAAAEAAAA"
    "tgcAAAIgAABIAAAAOAgAAAAQAAABAAAA0AsAAAAAAAA=";

static void WriteBase64ToFile(const char* base64, File* file) {
  // Decode base64.
  CHECK(base64 != nullptr);
  size_t length;
  std::unique_ptr<uint8_t[]> bytes(DecodeBase64(base64, &length));
  CHECK(bytes != nullptr);
  if (!file->WriteFully(bytes.get(), length)) {
    PLOG(FATAL) << "Failed to write base64 as file";
  }
}

TEST_F(Dex2oatTest, CompactDexGenerationFailure) {
  ScratchFile temp_dex;
  WriteBase64ToFile(kDuplicateMethodInputDex, temp_dex.GetFile());
  std::string out_dir = GetScratchDir();
  const std::string oat_filename = out_dir + "/base.oat";
  // The dex won't pass the method verifier, only use the verify filter.
  GenerateOdexForTest(temp_dex.GetFilename(),
                      oat_filename,
                      CompilerFilter::Filter::kVerify,
                      { },
                      true,  // expect_success
                      false,  // use_fd
                      [](const OatFile& o) {
                        CHECK(o.ContainsDexCode());
                      });
  // Open our generated oat file.
  std::string error_msg;
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   oat_filename.c_str(),
                                                   oat_filename.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   temp_dex.GetFilename().c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  std::vector<const OatDexFile*> oat_dex_files = odex_file->GetOatDexFiles();
  ASSERT_EQ(oat_dex_files.size(), 1u);
  // The dexes should have failed to convert to compact dex.
  for (const OatDexFile* oat_dex : oat_dex_files) {
    std::unique_ptr<const DexFile> dex_file(oat_dex->OpenDexFile(&error_msg));
    ASSERT_TRUE(dex_file != nullptr) << error_msg;
    ASSERT_TRUE(!dex_file->IsCompactDexFile());
  }
}

TEST_F(Dex2oatTest, CompactDexGenerationFailureMultiDex) {
  // Create a multidex file with only one dex that gets rejected for cdex conversion.
  ScratchFile apk_file;
  {
    FILE* file = fdopen(apk_file.GetFd(), "w+b");
    ZipWriter writer(file);
    // Add vdex to zip.
    writer.StartEntry("classes.dex", ZipWriter::kCompress);
    size_t length = 0u;
    std::unique_ptr<uint8_t[]> bytes(DecodeBase64(kDuplicateMethodInputDex, &length));
    ASSERT_GE(writer.WriteBytes(&bytes[0], length), 0);
    writer.FinishEntry();
    writer.StartEntry("classes2.dex", ZipWriter::kCompress);
    std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
    ASSERT_GE(writer.WriteBytes(dex->Begin(), dex->Size()), 0);
    writer.FinishEntry();
    writer.Finish();
    ASSERT_EQ(apk_file.GetFile()->Flush(), 0);
  }
  const std::string dex_location = apk_file.GetFilename();
  const std::string odex_location = GetOdexDir() + "/output.odex";
  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::kQuicken,
                      { "--compact-dex-level=fast" },
                      true);
}

TEST_F(Dex2oatTest, StderrLoggerOutput) {
  std::string dex_location = GetScratchDir() + "/Dex2OatStderrLoggerTest.jar";
  std::string odex_location = GetOdexDir() + "/Dex2OatStderrLoggerTest.odex";

  // Test file doesn't matter.
  Copy(GetDexSrc1(), dex_location);

  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::kQuicken,
                      { "--runtime-arg", "-Xuse-stderr-logger" },
                      true);
  // Look for some random part of dex2oat logging. With the stderr logger this should be captured,
  // even on device.
  EXPECT_NE(std::string::npos, output_.find("dex2oat took"));
}

TEST_F(Dex2oatTest, VerifyCompilationReason) {
  std::string dex_location = GetScratchDir() + "/Dex2OatCompilationReason.jar";
  std::string odex_location = GetOdexDir() + "/Dex2OatCompilationReason.odex";

  // Test file doesn't matter.
  Copy(GetDexSrc1(), dex_location);

  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::kVerify,
                      { "--compilation-reason=install" },
                      true);
  std::string error_msg;
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   odex_location.c_str(),
                                                   odex_location.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   dex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  ASSERT_STREQ("install", odex_file->GetCompilationReason());
}

TEST_F(Dex2oatTest, VerifyNoCompilationReason) {
  std::string dex_location = GetScratchDir() + "/Dex2OatNoCompilationReason.jar";
  std::string odex_location = GetOdexDir() + "/Dex2OatNoCompilationReason.odex";

  // Test file doesn't matter.
  Copy(GetDexSrc1(), dex_location);

  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::kVerify,
                      {},
                      true);
  std::string error_msg;
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   odex_location.c_str(),
                                                   odex_location.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   dex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  ASSERT_EQ(nullptr, odex_file->GetCompilationReason());
}

TEST_F(Dex2oatTest, DontExtract) {
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("ManyMethods"));
  std::string error_msg;
  const std::string out_dir = GetScratchDir();
  const std::string dex_location = dex->GetLocation();
  const std::string odex_location = out_dir + "/base.oat";
  const std::string vdex_location = out_dir + "/base.vdex";
  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::Filter::kVerify,
                      { "--copy-dex-files=false" },
                      true,  // expect_success
                      false,  // use_fd
                      [](const OatFile&) {
                      });
  {
    // Check the vdex doesn't have dex.
    std::unique_ptr<VdexFile> vdex(VdexFile::Open(vdex_location.c_str(),
                                                  /*writable*/ false,
                                                  /*low_4gb*/ false,
                                                  /*unquicken*/ false,
                                                  &error_msg));
    ASSERT_TRUE(vdex != nullptr);
    EXPECT_FALSE(vdex->HasDexSection()) << output_;
  }
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   odex_location.c_str(),
                                                   odex_location.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/ false,
                                                   dex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr) << dex_location;
  std::vector<const OatDexFile*> oat_dex_files = odex_file->GetOatDexFiles();
  ASSERT_EQ(oat_dex_files.size(), 1u);
  // Verify that the oat file can still open the dex files.
  for (const OatDexFile* oat_dex : oat_dex_files) {
    std::unique_ptr<const DexFile> dex_file(oat_dex->OpenDexFile(&error_msg));
    ASSERT_TRUE(dex_file != nullptr) << error_msg;
  }
  // Create a dm file and use it to verify.
  // Add produced artifacts to a zip file that doesn't contain the classes.dex.
  ScratchFile dm_file;
  {
    std::unique_ptr<File> vdex_file(OS::OpenFileForReading(vdex_location.c_str()));
    ASSERT_TRUE(vdex_file != nullptr);
    ASSERT_GT(vdex_file->GetLength(), 0u);
    FILE* file = fdopen(dm_file.GetFd(), "w+b");
    ZipWriter writer(file);
    auto write_all_bytes = [&](File* file) {
      std::unique_ptr<uint8_t[]> bytes(new uint8_t[file->GetLength()]);
      ASSERT_TRUE(file->ReadFully(&bytes[0], file->GetLength()));
      ASSERT_GE(writer.WriteBytes(&bytes[0], file->GetLength()), 0);
    };
    // Add vdex to zip.
    writer.StartEntry(VdexFile::kVdexNameInDmFile, ZipWriter::kCompress);
    write_all_bytes(vdex_file.get());
    writer.FinishEntry();
    writer.Finish();
    ASSERT_EQ(dm_file.GetFile()->Flush(), 0);
  }

  // Generate a quickened dex by using the input dm file to verify.
  GenerateOdexForTest(dex_location,
                      odex_location,
                      CompilerFilter::Filter::kQuicken,
                      { "--dump-timings",
                        "--dm-file=" + dm_file.GetFilename(),
                        // Pass -Xuse-stderr-logger have dex2oat output in output_ on target.
                        "--runtime-arg",
                        "-Xuse-stderr-logger" },
                      true,  // expect_success
                      false,  // use_fd
                      [](const OatFile& o) {
                        CHECK(o.ContainsDexCode());
                      });
  // Check the output for "Fast verify", this is printed from --dump-timings.
  std::istringstream iss(output_);
  std::string line;
  bool found_fast_verify = false;
  const std::string kFastVerifyString = "Fast Verify";
  while (std::getline(iss, line) && !found_fast_verify) {
    found_fast_verify = found_fast_verify || line.find(kFastVerifyString) != std::string::npos;
  }
  EXPECT_TRUE(found_fast_verify) << "Expected to find " << kFastVerifyString << "\n" << output_;
}

// Test that dex files with quickened opcodes aren't dequickened.
TEST_F(Dex2oatTest, QuickenedInput) {
  std::string error_msg;
  ScratchFile temp_dex;
  MutateDexFile(temp_dex.GetFile(), GetTestDexFileName("ManyMethods"), [] (DexFile* dex) {
    bool mutated_successfully = false;
    // Change the dex instructions to make an opcode that spans past the end of the code item.
    for (size_t i = 0; i < dex->NumClassDefs(); ++i) {
      const DexFile::ClassDef& def = dex->GetClassDef(i);
      const uint8_t* data = dex->GetClassData(def);
      if (data == nullptr) {
        continue;
      }
      ClassDataItemIterator it(*dex, data);
      it.SkipAllFields();
      while (it.HasNextMethod()) {
        DexFile::CodeItem* item = const_cast<DexFile::CodeItem*>(it.GetMethodCodeItem());
        if (item != nullptr) {
          CodeItemInstructionAccessor instructions(*dex, item);
          // Make a quickened instruction that doesn't run past the end of the code item.
          if (instructions.InsnsSizeInCodeUnits() > 2) {
            const_cast<Instruction&>(instructions.InstructionAt(0)).SetOpcode(
                Instruction::IGET_BYTE_QUICK);
            mutated_successfully = true;
          }
        }
        it.Next();
      }
    }
    CHECK(mutated_successfully)
        << "Failed to find candidate code item with only one code unit in last instruction.";
  });

  std::string dex_location = temp_dex.GetFilename();
  std::string odex_location = GetOdexDir() + "/quickened.odex";
  std::string vdex_location = GetOdexDir() + "/quickened.vdex";
  std::unique_ptr<File> vdex_output(OS::CreateEmptyFile(vdex_location.c_str()));
  // Quicken the dex
  {
    std::string input_vdex = "--input-vdex-fd=-1";
    std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_output->Fd());
    GenerateOdexForTest(dex_location,
                        odex_location,
                        CompilerFilter::kQuicken,
                        // Disable cdex since we want to compare against the original dex file
                        // after unquickening.
                        { input_vdex, output_vdex, kDisableCompactDex },
                        /* expect_success */ true,
                        /* use_fd */ true);
  }
  // Unquicken by running the verify compiler filter on the vdex file and verify it matches.
  std::string odex_location2 = GetOdexDir() + "/unquickened.odex";
  std::string vdex_location2 = GetOdexDir() + "/unquickened.vdex";
  std::unique_ptr<File> vdex_unquickened(OS::CreateEmptyFile(vdex_location2.c_str()));
  {
    std::string input_vdex = StringPrintf("--input-vdex-fd=%d", vdex_output->Fd());
    std::string output_vdex = StringPrintf("--output-vdex-fd=%d", vdex_unquickened->Fd());
    GenerateOdexForTest(dex_location,
                        odex_location2,
                        CompilerFilter::kVerify,
                        // Disable cdex to avoid needing to write out the shared section.
                        { input_vdex, output_vdex, kDisableCompactDex },
                        /* expect_success */ true,
                        /* use_fd */ true);
  }
  ASSERT_EQ(vdex_unquickened->Flush(), 0) << "Could not flush and close vdex file";
  ASSERT_TRUE(success_);
  {
    // Check that hte vdex has one dex and compare it to the original one.
    std::unique_ptr<VdexFile> vdex(VdexFile::Open(vdex_location2.c_str(),
                                                  /*writable*/ false,
                                                  /*low_4gb*/ false,
                                                  /*unquicken*/ false,
                                                  &error_msg));
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    bool result = vdex->OpenAllDexFiles(&dex_files, &error_msg);
    ASSERT_TRUE(result) << error_msg;
    ASSERT_EQ(dex_files.size(), 1u) << error_msg;
    ScratchFile temp;
    ASSERT_TRUE(temp.GetFile()->WriteFully(dex_files[0]->Begin(), dex_files[0]->Size()));
    ASSERT_EQ(temp.GetFile()->Flush(), 0) << "Could not flush extracted dex";
    EXPECT_EQ(temp.GetFile()->Compare(temp_dex.GetFile()), 0);
  }
  ASSERT_EQ(vdex_output->FlushCloseOrErase(), 0) << "Could not flush and close";
  ASSERT_EQ(vdex_unquickened->FlushCloseOrErase(), 0) << "Could not flush and close";
}

// Test that compact dex generation with invalid dex files doesn't crash dex2oat. b/75970654
TEST_F(Dex2oatTest, CompactDexInvalidSource) {
  ScratchFile invalid_dex;
  {
    FILE* file = fdopen(invalid_dex.GetFd(), "w+b");
    ZipWriter writer(file);
    writer.StartEntry("classes.dex", ZipWriter::kAlign32);
    DexFile::Header header = {};
    StandardDexFile::WriteMagic(header.magic_);
    StandardDexFile::WriteCurrentVersion(header.magic_);
    header.file_size_ = 4 * KB;
    header.data_size_ = 4 * KB;
    header.data_off_ = 10 * MB;
    header.map_off_ = 10 * MB;
    header.class_defs_off_ = 10 * MB;
    header.class_defs_size_ = 10000;
    ASSERT_GE(writer.WriteBytes(&header, sizeof(header)), 0);
    writer.FinishEntry();
    writer.Finish();
    ASSERT_EQ(invalid_dex.GetFile()->Flush(), 0);
  }
  const std::string dex_location = invalid_dex.GetFilename();
  const std::string odex_location = GetOdexDir() + "/output.odex";
  std::string error_msg;
  int status = GenerateOdexForTestWithStatus(
      {dex_location},
      odex_location,
      CompilerFilter::kQuicken,
      &error_msg,
      { "--compact-dex-level=fast" });
  ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) != 0) << status << " " << output_;
}

// Test that dex2oat with a CompactDex file in the APK fails.
TEST_F(Dex2oatTest, CompactDexInZip) {
  CompactDexFile::Header header = {};
  CompactDexFile::WriteMagic(header.magic_);
  CompactDexFile::WriteCurrentVersion(header.magic_);
  header.file_size_ = sizeof(CompactDexFile::Header);
  header.data_off_ = 10 * MB;
  header.map_off_ = 10 * MB;
  header.class_defs_off_ = 10 * MB;
  header.class_defs_size_ = 10000;
  // Create a zip containing the invalid dex.
  ScratchFile invalid_dex_zip;
  {
    FILE* file = fdopen(invalid_dex_zip.GetFd(), "w+b");
    ZipWriter writer(file);
    writer.StartEntry("classes.dex", ZipWriter::kCompress);
    ASSERT_GE(writer.WriteBytes(&header, sizeof(header)), 0);
    writer.FinishEntry();
    writer.Finish();
    ASSERT_EQ(invalid_dex_zip.GetFile()->Flush(), 0);
  }
  // Create the dex file directly.
  ScratchFile invalid_dex;
  {
    ASSERT_GE(invalid_dex.GetFile()->WriteFully(&header, sizeof(header)), 0);
    ASSERT_EQ(invalid_dex.GetFile()->Flush(), 0);
  }
  std::string error_msg;
  int status = 0u;

  status = GenerateOdexForTestWithStatus(
      { invalid_dex_zip.GetFilename() },
      GetOdexDir() + "/output_apk.odex",
      CompilerFilter::kQuicken,
      &error_msg,
      { "--compact-dex-level=fast" });
  ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) != 0) << status << " " << output_;

  status = GenerateOdexForTestWithStatus(
      { invalid_dex.GetFilename() },
      GetOdexDir() + "/output.odex",
      CompilerFilter::kQuicken,
      &error_msg,
      { "--compact-dex-level=fast" });
  ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) != 0) << status << " " << output_;
}

TEST_F(Dex2oatTest, AppImageNoProfile) {
  ScratchFile app_image_file;
  const std::string out_dir = GetScratchDir();
  const std::string odex_location = out_dir + "/base.odex";
  GenerateOdexForTest(GetTestDexFileName("ManyMethods"),
                      odex_location,
                      CompilerFilter::Filter::kSpeedProfile,
                      { "--app-image-fd=" + std::to_string(app_image_file.GetFd()) },
                      true,  // expect_success
                      false,  // use_fd
                      [](const OatFile&) {});
  // Open our generated oat file.
  std::string error_msg;
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/* zip_fd */ -1,
                                                   odex_location.c_str(),
                                                   odex_location.c_str(),
                                                   nullptr,
                                                   nullptr,
                                                   false,
                                                   /*low_4gb*/false,
                                                   odex_location.c_str(),
                                                   &error_msg));
  ASSERT_TRUE(odex_file != nullptr);
  ImageHeader header = {};
  ASSERT_TRUE(app_image_file.GetFile()->PreadFully(
      reinterpret_cast<void*>(&header),
      sizeof(header),
      /*offset*/ 0u)) << app_image_file.GetFile()->GetLength();
  EXPECT_GT(header.GetImageSection(ImageHeader::kSectionObjects).Size(), 0u);
  EXPECT_EQ(header.GetImageSection(ImageHeader::kSectionArtMethods).Size(), 0u);
  EXPECT_EQ(header.GetImageSection(ImageHeader::kSectionArtFields).Size(), 0u);
}

TEST_F(Dex2oatClassLoaderContextTest, StoredClassLoaderContext) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles("MultiDex");
  const std::string out_dir = GetScratchDir();
  const std::string odex_location = out_dir + "/base.odex";
  const std::string valid_context = "PCL[" + dex_files[0]->GetLocation() + "]";
  const std::string stored_context = "PCL[/system/not_real_lib.jar]";
  std::string expected_stored_context = "PCL[";
  size_t index = 1;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    const bool is_first = index == 1u;
    if (!is_first) {
      expected_stored_context += ":";
    }
    expected_stored_context += "/system/not_real_lib.jar";
    if (!is_first) {
      expected_stored_context += "!classes" + std::to_string(index) + ".dex";
    }
    expected_stored_context += "*" + std::to_string(dex_file->GetLocationChecksum());
    ++index;
  }
  expected_stored_context +=    + "]";
  // The class path should not be valid and should fail being stored.
  GenerateOdexForTest(GetTestDexFileName("ManyMethods"),
                      odex_location,
                      CompilerFilter::Filter::kQuicken,
                      { "--class-loader-context=" + stored_context },
                      true,  // expect_success
                      false,  // use_fd
                      [&](const OatFile& oat_file) {
    EXPECT_NE(oat_file.GetClassLoaderContext(), stored_context) << output_;
    EXPECT_NE(oat_file.GetClassLoaderContext(), valid_context) << output_;
  });
  // The stored context should match what we expect even though it's invalid.
  GenerateOdexForTest(GetTestDexFileName("ManyMethods"),
                      odex_location,
                      CompilerFilter::Filter::kQuicken,
                      { "--class-loader-context=" + valid_context,
                        "--stored-class-loader-context=" + stored_context },
                      true,  // expect_success
                      false,  // use_fd
                      [&](const OatFile& oat_file) {
    EXPECT_EQ(oat_file.GetClassLoaderContext(), expected_stored_context) << output_;
  });
}

}  // namespace art
