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

#include "base/logging.h"  // For InitLogging.
#include "base/mutex.h"
#include "base/os.h"
#include "base/utils.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "base/file_utils.h"
#include "compiler_filter.h"
#include "class_loader_context.h"
#include "dex/dex_file.h"
#include "noop_compiler_callbacks.h"
#include "oat_file_assistant.h"
#include "runtime.h"
#include "thread-inl.h"

namespace art {

// See OatFileAssistant docs for the meaning of the valid return codes.
enum ReturnCodes {
  kNoDexOptNeeded = 0,
  kDex2OatFromScratch = 1,
  kDex2OatForBootImageOat = 2,
  kDex2OatForFilterOat = 3,
  kDex2OatForRelocationOat = 4,
  kDex2OatForBootImageOdex = 5,
  kDex2OatForFilterOdex = 6,
  kDex2OatForRelocationOdex = 7,

  kErrorInvalidArguments = 101,
  kErrorCannotCreateRuntime = 102,
  kErrorUnknownDexOptNeeded = 103
};

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("  Performs a dexopt analysis on the given dex file and returns whether or not");
  UsageError("  the dex file needs to be dexopted.");
  UsageError("Usage: dexoptanalyzer [options]...");
  UsageError("");
  UsageError("  --dex-file=<filename>: the dex file which should be analyzed.");
  UsageError("");
  UsageError("  --isa=<string>: the instruction set for which the analysis should be performed.");
  UsageError("");
  UsageError("  --compiler-filter=<string>: the target compiler filter to be used as reference");
  UsageError("       when deciding if the dex file needs to be optimized.");
  UsageError("");
  UsageError("  --assume-profile-changed: assumes the profile information has changed");
  UsageError("       when deciding if the dex file needs to be optimized.");
  UsageError("");
  UsageError("  --image=<filename>: optional, the image to be used to decide if the associated");
  UsageError("       oat file is up to date. Defaults to $ANDROID_ROOT/framework/boot.art.");
  UsageError("       Example: --image=/system/framework/boot.art");
  UsageError("");
  UsageError("  --android-data=<directory>: optional, the directory which should be used as");
  UsageError("       android-data. By default ANDROID_DATA env variable is used.");
  UsageError("");
  UsageError("  --oat-fd=number: file descriptor of the oat file which should be analyzed");
  UsageError("");
  UsageError("  --vdex-fd=number: file descriptor of the vdex file corresponding to the oat file");
  UsageError("");
  UsageError("  --zip-fd=number: specifies a file descriptor corresponding to the dex file.");
  UsageError("");
  UsageError("  --downgrade: optional, if the purpose of dexopt is to downgrade the dex file");
  UsageError("       By default, dexopt considers upgrade case.");
  UsageError("");
  UsageError("Return code:");
  UsageError("  To make it easier to integrate with the internal tools this command will make");
  UsageError("    available its result (dexoptNeeded) as the exit/return code. i.e. it will not");
  UsageError("    return 0 for success and a non zero values for errors as the conventional");
  UsageError("    commands. The following return codes are possible:");
  UsageError("        kNoDexOptNeeded = 0");
  UsageError("        kDex2OatFromScratch = 1");
  UsageError("        kDex2OatForBootImageOat = 2");
  UsageError("        kDex2OatForFilterOat = 3");
  UsageError("        kDex2OatForRelocationOat = 4");
  UsageError("        kDex2OatForBootImageOdex = 5");
  UsageError("        kDex2OatForFilterOdex = 6");
  UsageError("        kDex2OatForRelocationOdex = 7");

  UsageError("        kErrorInvalidArguments = 101");
  UsageError("        kErrorCannotCreateRuntime = 102");
  UsageError("        kErrorUnknownDexOptNeeded = 103");
  UsageError("");

  exit(kErrorInvalidArguments);
}

class DexoptAnalyzer FINAL {
 public:
  DexoptAnalyzer() :
      assume_profile_changed_(false),
      downgrade_(false) {}

  void ParseArgs(int argc, char **argv) {
    original_argc = argc;
    original_argv = argv;

    Locks::Init();
    InitLogging(argv, Runtime::Abort);
    // Skip over the command name.
    argv++;
    argc--;

    if (argc == 0) {
      Usage("No arguments specified");
    }

    for (int i = 0; i < argc; ++i) {
      const StringPiece option(argv[i]);
      if (option == "--assume-profile-changed") {
        assume_profile_changed_ = true;
      } else if (option.starts_with("--dex-file=")) {
        dex_file_ = option.substr(strlen("--dex-file=")).ToString();
      } else if (option.starts_with("--compiler-filter=")) {
        std::string filter_str = option.substr(strlen("--compiler-filter=")).ToString();
        if (!CompilerFilter::ParseCompilerFilter(filter_str.c_str(), &compiler_filter_)) {
          Usage("Invalid compiler filter '%s'", option.data());
        }
      } else if (option.starts_with("--isa=")) {
        std::string isa_str = option.substr(strlen("--isa=")).ToString();
        isa_ = GetInstructionSetFromString(isa_str.c_str());
        if (isa_ == InstructionSet::kNone) {
          Usage("Invalid isa '%s'", option.data());
        }
      } else if (option.starts_with("--image=")) {
        image_ = option.substr(strlen("--image=")).ToString();
      } else if (option.starts_with("--android-data=")) {
        // Overwrite android-data if needed (oat file assistant relies on a valid directory to
        // compute dalvik-cache folder). This is mostly used in tests.
        std::string new_android_data = option.substr(strlen("--android-data=")).ToString();
        setenv("ANDROID_DATA", new_android_data.c_str(), 1);
      } else if (option.starts_with("--downgrade")) {
        downgrade_ = true;
      } else if (option.starts_with("--oat-fd")) {
        oat_fd_ = std::stoi(option.substr(strlen("--oat-fd=")).ToString(), nullptr, 0);
        if (oat_fd_ < 0) {
          Usage("Invalid --oat-fd %d", oat_fd_);
        }
      } else if (option.starts_with("--vdex-fd")) {
        vdex_fd_ = std::stoi(option.substr(strlen("--vdex-fd=")).ToString(), nullptr, 0);
        if (vdex_fd_ < 0) {
          Usage("Invalid --vdex-fd %d", vdex_fd_);
        }
      } else if (option.starts_with("--zip-fd")) {
          zip_fd_ = std::stoi(option.substr(strlen("--zip-fd=")).ToString(), nullptr, 0);
          if (zip_fd_ < 0) {
            Usage("Invalid --zip-fd %d", zip_fd_);
          }
      } else if (option.starts_with("--class-loader-context=")) {
        std::string context_str = option.substr(strlen("--class-loader-context=")).ToString();
        class_loader_context_ = ClassLoaderContext::Create(context_str);
        if (class_loader_context_ == nullptr) {
          Usage("Invalid --class-loader-context '%s'", context_str.c_str());
        }
      } else {
        Usage("Unknown argument '%s'", option.data());
      }
    }

    if (image_.empty()) {
      // If we don't receive the image, try to use the default one.
      // Tests may specify a different image (e.g. core image).
      std::string error_msg;
      image_ = GetDefaultBootImageLocation(&error_msg);

      if (image_.empty()) {
        LOG(ERROR) << error_msg;
        Usage("--image unspecified and ANDROID_ROOT not set or image file does not exist.");
      }
    }
  }

  bool CreateRuntime() {
    RuntimeOptions options;
    // The image could be custom, so make sure we explicitly pass it.
    std::string img = "-Ximage:" + image_;
    options.push_back(std::make_pair(img.c_str(), nullptr));
    // The instruction set of the image should match the instruction set we will test.
    const void* isa_opt = reinterpret_cast<const void*>(GetInstructionSetString(isa_));
    options.push_back(std::make_pair("imageinstructionset", isa_opt));
     // Disable libsigchain. We don't don't need it to evaluate DexOptNeeded status.
    options.push_back(std::make_pair("-Xno-sig-chain", nullptr));
    // Pretend we are a compiler so that we can re-use the same infrastructure to load a different
    // ISA image and minimize the amount of things that get started.
    NoopCompilerCallbacks callbacks;
    options.push_back(std::make_pair("compilercallbacks", &callbacks));
    // Make sure we don't attempt to relocate. The tool should only retrieve the DexOptNeeded
    // status and not attempt to relocate the boot image.
    options.push_back(std::make_pair("-Xnorelocate", nullptr));

    if (!Runtime::Create(options, false)) {
      LOG(ERROR) << "Unable to initialize runtime";
      return false;
    }
    // Runtime::Create acquired the mutator_lock_ that is normally given away when we
    // Runtime::Start. Give it away now.
    Thread::Current()->TransitionFromRunnableToSuspended(kNative);

    return true;
  }

  int GetDexOptNeeded() {
    if (!CreateRuntime()) {
      return kErrorCannotCreateRuntime;
    }
    std::unique_ptr<Runtime> runtime(Runtime::Current());

    std::unique_ptr<OatFileAssistant> oat_file_assistant;
    oat_file_assistant = std::make_unique<OatFileAssistant>(dex_file_.c_str(),
                                                            isa_,
                                                            false /*load_executable*/,
                                                            false /*only_load_system_executable*/,
                                                            vdex_fd_,
                                                            oat_fd_,
                                                            zip_fd_);
    // Always treat elements of the bootclasspath as up-to-date.
    // TODO(calin): this check should be in OatFileAssistant.
    if (oat_file_assistant->IsInBootClassPath()) {
      return kNoDexOptNeeded;
    }

    int dexoptNeeded = oat_file_assistant->GetDexOptNeeded(
        compiler_filter_, assume_profile_changed_, downgrade_, class_loader_context_.get());

    // Convert OatFileAssitant codes to dexoptanalyzer codes.
    switch (dexoptNeeded) {
      case OatFileAssistant::kNoDexOptNeeded: return kNoDexOptNeeded;
      case OatFileAssistant::kDex2OatFromScratch: return kDex2OatFromScratch;
      case OatFileAssistant::kDex2OatForBootImage: return kDex2OatForBootImageOat;
      case OatFileAssistant::kDex2OatForFilter: return kDex2OatForFilterOat;
      case OatFileAssistant::kDex2OatForRelocation: return kDex2OatForRelocationOat;

      case -OatFileAssistant::kDex2OatForBootImage: return kDex2OatForBootImageOdex;
      case -OatFileAssistant::kDex2OatForFilter: return kDex2OatForFilterOdex;
      case -OatFileAssistant::kDex2OatForRelocation: return kDex2OatForRelocationOdex;
      default:
        LOG(ERROR) << "Unknown dexoptNeeded " << dexoptNeeded;
        return kErrorUnknownDexOptNeeded;
    }
  }

 private:
  std::string dex_file_;
  InstructionSet isa_;
  CompilerFilter::Filter compiler_filter_;
  std::unique_ptr<ClassLoaderContext> class_loader_context_;
  bool assume_profile_changed_;
  bool downgrade_;
  std::string image_;
  int oat_fd_ = -1;
  int vdex_fd_ = -1;
  // File descriptor corresponding to apk, dex_file, or zip.
  int zip_fd_ = -1;
};

static int dexoptAnalyze(int argc, char** argv) {
  DexoptAnalyzer analyzer;

  // Parse arguments. Argument mistakes will lead to exit(kErrorInvalidArguments) in UsageError.
  analyzer.ParseArgs(argc, argv);
  return analyzer.GetDexOptNeeded();
}

}  // namespace art

int main(int argc, char **argv) {
  return art::dexoptAnalyze(argc, argv);
}
