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

#ifndef ART_OATDUMP_OATDUMP_TEST_H_
#define ART_OATDUMP_OATDUMP_TEST_H_

#include <sstream>
#include <string>
#include <vector>

#include "android-base/strings.h"

#include "arch/instruction_set.h"
#include "base/file_utils.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "common_runtime_test.h"
#include "exec_utils.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"

#include <sys/types.h>
#include <unistd.h>

namespace art {

class OatDumpTest : public CommonRuntimeTest {
 protected:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();
    core_art_location_ = GetCoreArtLocation();
    core_oat_location_ = GetSystemImageFilename(GetCoreOatLocation().c_str(), kRuntimeISA);
    tmp_dir_ = GetScratchDir();
  }

  virtual void TearDown() {
    ClearDirectory(tmp_dir_.c_str(), /*recursive*/ false);
    ASSERT_EQ(rmdir(tmp_dir_.c_str()), 0);
    CommonRuntimeTest::TearDown();
  }

  std::string GetScratchDir() {
    // ANDROID_DATA needs to be set
    CHECK_NE(static_cast<char*>(nullptr), getenv("ANDROID_DATA"));
    std::string dir = getenv("ANDROID_DATA");
    dir += "/oatdump-tmp-dir-XXXXXX";
    if (mkdtemp(&dir[0]) == nullptr) {
      PLOG(FATAL) << "mkdtemp(\"" << &dir[0] << "\") failed";
    }
    return dir;
  }

  // Linking flavor.
  enum Flavor {
    kDynamic,  // oatdump(d), dex2oat(d)
    kStatic,   // oatdump(d)s, dex2oat(d)s
  };

  // Returns path to the oatdump/dex2oat/dexdump binary.
  std::string GetExecutableFilePath(const char* name, bool is_debug, bool is_static) {
    std::string root = GetTestAndroidRoot();
    root += "/bin/";
    root += name;
    if (is_debug) {
      root += "d";
    }
    if (is_static) {
      root += "s";
    }
    return root;
  }

  std::string GetExecutableFilePath(Flavor flavor, const char* name) {
    return GetExecutableFilePath(name, kIsDebugBuild, flavor == kStatic);
  }

  enum Mode {
    kModeOat,
    kModeOatWithBootImage,
    kModeArt,
    kModeSymbolize,
  };

  // Display style.
  enum Display {
    kListOnly,
    kListAndCode
  };

  std::string GetAppBaseName() {
    // Use ProfileTestMultiDex as it contains references to boot image strings
    // that shall use different code for PIC and non-PIC.
    return "ProfileTestMultiDex";
  }

  std::string GetAppOdexName() {
    return tmp_dir_ + "/" + GetAppBaseName() + ".odex";
  }

  bool GenerateAppOdexFile(Flavor flavor,
                           const std::vector<std::string>& args,
                           /*out*/ std::string* error_msg) {
    std::string dex2oat_path = GetExecutableFilePath(flavor, "dex2oat");
    std::vector<std::string> exec_argv = {
        dex2oat_path,
        "--runtime-arg",
        "-Xms64m",
        "--runtime-arg",
        "-Xmx512m",
        "--runtime-arg",
        "-Xnorelocate",
        "--boot-image=" + GetCoreArtLocation(),
        "--instruction-set=" + std::string(GetInstructionSetString(kRuntimeISA)),
        "--dex-file=" + GetTestDexFileName(GetAppBaseName().c_str()),
        "--oat-file=" + GetAppOdexName(),
        "--compiler-filter=speed"
    };
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());

    return ForkAndExecAndWait(exec_argv, error_msg);
  }

  // Run the test with custom arguments.
  bool Exec(Flavor flavor,
            Mode mode,
            const std::vector<std::string>& args,
            Display display,
            /*out*/ std::string* error_msg) {
    std::string file_path = GetExecutableFilePath(flavor, "oatdump");

    EXPECT_TRUE(OS::FileExists(file_path.c_str())) << file_path << " should be a valid file path";

    // ScratchFile scratch;
    std::vector<std::string> exec_argv = { file_path };
    std::vector<std::string> expected_prefixes;
    if (mode == kModeSymbolize) {
      exec_argv.push_back("--symbolize=" + core_oat_location_);
      exec_argv.push_back("--output=" + core_oat_location_ + ".symbolize");
    } else {
      expected_prefixes.push_back("Dex file data for");
      expected_prefixes.push_back("Num string ids:");
      expected_prefixes.push_back("Num field ids:");
      expected_prefixes.push_back("Num method ids:");
      expected_prefixes.push_back("LOCATION:");
      expected_prefixes.push_back("MAGIC:");
      expected_prefixes.push_back("DEX FILE COUNT:");
      if (display == kListAndCode) {
        // Code and dex code do not show up if list only.
        expected_prefixes.push_back("DEX CODE:");
        expected_prefixes.push_back("CODE:");
        expected_prefixes.push_back("CodeInfoEncoding");
        expected_prefixes.push_back("CodeInfoInlineInfo");
      }
      if (mode == kModeArt) {
        exec_argv.push_back("--image=" + core_art_location_);
        exec_argv.push_back("--instruction-set=" + std::string(
            GetInstructionSetString(kRuntimeISA)));
        expected_prefixes.push_back("IMAGE LOCATION:");
        expected_prefixes.push_back("IMAGE BEGIN:");
        expected_prefixes.push_back("kDexCaches:");
      } else if (mode == kModeOatWithBootImage) {
        exec_argv.push_back("--boot-image=" + GetCoreArtLocation());
        exec_argv.push_back("--instruction-set=" + std::string(
            GetInstructionSetString(kRuntimeISA)));
        exec_argv.push_back("--oat-file=" + GetAppOdexName());
      } else {
        CHECK_EQ(static_cast<size_t>(mode), static_cast<size_t>(kModeOat));
        exec_argv.push_back("--oat-file=" + core_oat_location_);
      }
    }
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());

    pid_t pid;
    int pipe_fd;
    bool result = ForkAndExec(exec_argv, &pid, &pipe_fd, error_msg);
    if (result) {
      static const size_t kLineMax = 256;
      char line[kLineMax] = {};
      size_t line_len = 0;
      size_t total = 0;
      std::vector<bool> found(expected_prefixes.size(), false);
      while (true) {
        while (true) {
          size_t spaces = 0;
          // Trim spaces at the start of the line.
          for (; spaces < line_len && isspace(line[spaces]); ++spaces) {}
          if (spaces > 0) {
            line_len -= spaces;
            memmove(&line[0], &line[spaces], line_len);
          }
          ssize_t bytes_read =
              TEMP_FAILURE_RETRY(read(pipe_fd, &line[line_len], kLineMax - line_len));
          if (bytes_read <= 0) {
            break;
          }
          line_len += bytes_read;
          total += bytes_read;
        }
        if (line_len == 0) {
          break;
        }
        // Check contents.
        for (size_t i = 0; i < expected_prefixes.size(); ++i) {
          const std::string& expected = expected_prefixes[i];
          if (!found[i] &&
              line_len >= expected.length() &&
              memcmp(line, expected.c_str(), expected.length()) == 0) {
            found[i] = true;
          }
        }
        // Skip to next line.
        size_t next_line = 0;
        for (; next_line + 1 < line_len && line[next_line] != '\n'; ++next_line) {}
        line_len -= next_line + 1;
        memmove(&line[0], &line[next_line + 1], line_len);
      }
      if (mode == kModeSymbolize) {
        EXPECT_EQ(total, 0u);
      } else {
        EXPECT_GT(total, 0u);
      }
      LOG(INFO) << "Processed bytes " << total;
      close(pipe_fd);
      int status = 0;
      if (waitpid(pid, &status, 0) != -1) {
        result = (status == 0);
      }

      for (size_t i = 0; i < expected_prefixes.size(); ++i) {
        if (!found[i]) {
          LOG(ERROR) << "Did not find prefix " << expected_prefixes[i];
          result = false;
        }
      }
    }

    return result;
  }

  bool ForkAndExec(const std::vector<std::string>& exec_argv,
                   /*out*/ pid_t* pid,
                   /*out*/ int* pipe_fd,
                   /*out*/ std::string* error_msg) {
    int link[2];
    if (pipe(link) == -1) {
      *error_msg = strerror(errno);
      return false;
    }

    *pid = fork();
    if (*pid == -1) {
      *error_msg = strerror(errno);
      close(link[0]);
      close(link[1]);
      return false;
    }

    if (*pid == 0) {
      dup2(link[1], STDOUT_FILENO);
      close(link[0]);
      close(link[1]);
      // change process groups, so we don't get reaped by ProcessManager
      setpgid(0, 0);
      // Use execv here rather than art::Exec to avoid blocking on waitpid here.
      std::vector<char*> argv;
      for (size_t i = 0; i < exec_argv.size(); ++i) {
        argv.push_back(const_cast<char*>(exec_argv[i].c_str()));
      }
      argv.push_back(nullptr);
      UNUSED(execv(argv[0], &argv[0]));
      const std::string command_line(android::base::Join(exec_argv, ' '));
      PLOG(ERROR) << "Failed to execv(" << command_line << ")";
      // _exit to avoid atexit handlers in child.
      _exit(1);
      UNREACHABLE();
    } else {
      close(link[1]);
      *pipe_fd = link[0];
      return true;
    }
  }

  bool ForkAndExecAndWait(const std::vector<std::string>& exec_argv,
                          /*out*/ std::string* error_msg) {
    pid_t pid;
    int pipe_fd;
    bool result = ForkAndExec(exec_argv, &pid, &pipe_fd, error_msg);
    if (result) {
      close(pipe_fd);
      int status = 0;
      if (waitpid(pid, &status, 0) != -1) {
        result = (status == 0);
      }
    }
    return result;
  }

  std::string tmp_dir_;

 private:
  std::string core_art_location_;
  std::string core_oat_location_;
};

}  // namespace art

#endif  // ART_OATDUMP_OATDUMP_TEST_H_
