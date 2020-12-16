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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/dumpable.h"
#include "base/logging.h"  // For InitLogging.
#include "base/mutex.h"
#include "base/scoped_flock.h"
#include "base/stringpiece.h"
#include "base/time_utils.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "boot_image_profile.h"
#include "dex/art_dex_file_loader.h"
#include "dex/bytecode_utils.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_file_types.h"
#include "dex/type_reference.h"
#include "jit/profile_compilation_info.h"
#include "profile_assistant.h"
#include "runtime.h"
#include "zip_archive.h"

namespace art {

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static constexpr int kInvalidFd = -1;

static bool FdIsValid(int fd) {
  return fd != kInvalidFd;
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
  UsageError("Usage: profman [options]...");
  UsageError("");
  UsageError("  --dump-only: dumps the content of the specified profile files");
  UsageError("      to standard output (default) in a human readable form.");
  UsageError("");
  UsageError("  --dump-output-to-fd=<number>: redirects --dump-only output to a file descriptor.");
  UsageError("");
  UsageError("  --dump-classes-and-methods: dumps a sorted list of classes and methods that are");
  UsageError("      in the specified profile file to standard output (default) in a human");
  UsageError("      readable form. The output is valid input for --create-profile-from");
  UsageError("");
  UsageError("  --profile-file=<filename>: specify profiler output file to use for compilation.");
  UsageError("      Can be specified multiple time, in which case the data from the different");
  UsageError("      profiles will be aggregated.");
  UsageError("");
  UsageError("  --profile-file-fd=<number>: same as --profile-file but accepts a file descriptor.");
  UsageError("      Cannot be used together with --profile-file.");
  UsageError("");
  UsageError("  --reference-profile-file=<filename>: specify a reference profile.");
  UsageError("      The data in this file will be compared with the data obtained by merging");
  UsageError("      all the files specified with --profile-file or --profile-file-fd.");
  UsageError("      If the exit code is EXIT_COMPILE then all --profile-file will be merged into");
  UsageError("      --reference-profile-file. ");
  UsageError("");
  UsageError("  --reference-profile-file-fd=<number>: same as --reference-profile-file but");
  UsageError("      accepts a file descriptor. Cannot be used together with");
  UsageError("      --reference-profile-file.");
  UsageError("");
  UsageError("  --generate-test-profile=<filename>: generates a random profile file for testing.");
  UsageError("  --generate-test-profile-num-dex=<number>: number of dex files that should be");
  UsageError("      included in the generated profile. Defaults to 20.");
  UsageError("  --generate-test-profile-method-percentage=<number>: the percentage from the maximum");
  UsageError("      number of methods that should be generated. Defaults to 5.");
  UsageError("  --generate-test-profile-class-percentage=<number>: the percentage from the maximum");
  UsageError("      number of classes that should be generated. Defaults to 5.");
  UsageError("  --generate-test-profile-seed=<number>: seed for random number generator used when");
  UsageError("      generating random test profiles. Defaults to using NanoTime.");
  UsageError("");
  UsageError("  --create-profile-from=<filename>: creates a profile from a list of classes and");
  UsageError("      methods.");
  UsageError("");
  UsageError("  --dex-location=<string>: location string to use with corresponding");
  UsageError("      apk-fd to find dex files");
  UsageError("");
  UsageError("  --apk-fd=<number>: file descriptor containing an open APK to");
  UsageError("      search for dex files");
  UsageError("  --apk-=<filename>: an APK to search for dex files");
  UsageError("  --skip-apk-verification: do not attempt to verify APKs");
  UsageError("");
  UsageError("  --generate-boot-image-profile: Generate a boot image profile based on input");
  UsageError("      profiles. Requires passing in dex files to inspect properties of classes.");
  UsageError("  --boot-image-class-threshold=<value>: specify minimum number of class occurrences");
  UsageError("      to include a class in the boot image profile. Default is 10.");
  UsageError("  --boot-image-clean-class-threshold=<value>: specify minimum number of clean class");
  UsageError("      occurrences to include a class in the boot image profile. A clean class is a");
  UsageError("      class that doesn't have any static fields or native methods and is likely to");
  UsageError("      remain clean in the image. Default is 3.");
  UsageError("  --boot-image-sampled-method-threshold=<value>: minimum number of profiles a");
  UsageError("      non-hot method needs to be in order to be hot in the output profile. The");
  UsageError("      default is max int.");
  UsageError("  --copy-and-update-profile-key: if present, profman will copy the profile from");
  UsageError("      the file passed with --profile-fd(file) to the profile passed with");
  UsageError("      --reference-profile-fd(file) and update at the same time the profile-key");
  UsageError("      of entries corresponding to the apks passed with --apk(-fd).");
  UsageError("");

  exit(EXIT_FAILURE);
}

// Note: make sure you update the Usage if you change these values.
static constexpr uint16_t kDefaultTestProfileNumDex = 20;
static constexpr uint16_t kDefaultTestProfileMethodPercentage = 5;
static constexpr uint16_t kDefaultTestProfileClassPercentage = 5;

// Separators used when parsing human friendly representation of profiles.
static const std::string kMethodSep = "->";  // NOLINT [runtime/string] [4]
static const std::string kMissingTypesMarker = "missing_types";  // NOLINT [runtime/string] [4]
static const std::string kInvalidClassDescriptor = "invalid_class";  // NOLINT [runtime/string] [4]
static const std::string kInvalidMethod = "invalid_method";  // NOLINT [runtime/string] [4]
static const std::string kClassAllMethods = "*";  // NOLINT [runtime/string] [4]
static constexpr char kProfileParsingInlineChacheSep = '+';
static constexpr char kProfileParsingTypeSep = ',';
static constexpr char kProfileParsingFirstCharInSignature = '(';
static constexpr char kMethodFlagStringHot = 'H';
static constexpr char kMethodFlagStringStartup = 'S';
static constexpr char kMethodFlagStringPostStartup = 'P';

// TODO(calin): This class has grown too much from its initial design. Split the functionality
// into smaller, more contained pieces.
class ProfMan FINAL {
 public:
  ProfMan() :
      reference_profile_file_fd_(kInvalidFd),
      dump_only_(false),
      dump_classes_and_methods_(false),
      generate_boot_image_profile_(false),
      dump_output_to_fd_(kInvalidFd),
      test_profile_num_dex_(kDefaultTestProfileNumDex),
      test_profile_method_percerntage_(kDefaultTestProfileMethodPercentage),
      test_profile_class_percentage_(kDefaultTestProfileClassPercentage),
      test_profile_seed_(NanoTime()),
      start_ns_(NanoTime()),
      copy_and_update_profile_key_(false) {}

  ~ProfMan() {
    LogCompletionTime();
  }

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
      const bool log_options = false;
      if (log_options) {
        LOG(INFO) << "profman: option[" << i << "]=" << argv[i];
      }
      if (option == "--dump-only") {
        dump_only_ = true;
      } else if (option == "--dump-classes-and-methods") {
        dump_classes_and_methods_ = true;
      } else if (option.starts_with("--create-profile-from=")) {
        create_profile_from_file_ = option.substr(strlen("--create-profile-from=")).ToString();
      } else if (option.starts_with("--dump-output-to-fd=")) {
        ParseUintOption(option, "--dump-output-to-fd", &dump_output_to_fd_, Usage);
      } else if (option == "--generate-boot-image-profile") {
        generate_boot_image_profile_ = true;
      } else if (option.starts_with("--boot-image-class-threshold=")) {
        ParseUintOption(option,
                        "--boot-image-class-threshold",
                        &boot_image_options_.image_class_theshold,
                        Usage);
      } else if (option.starts_with("--boot-image-clean-class-threshold=")) {
        ParseUintOption(option,
                        "--boot-image-clean-class-threshold",
                        &boot_image_options_.image_class_clean_theshold,
                        Usage);
      } else if (option.starts_with("--boot-image-sampled-method-threshold=")) {
        ParseUintOption(option,
                        "--boot-image-sampled-method-threshold",
                        &boot_image_options_.compiled_method_threshold,
                        Usage);
      } else if (option.starts_with("--profile-file=")) {
        profile_files_.push_back(option.substr(strlen("--profile-file=")).ToString());
      } else if (option.starts_with("--profile-file-fd=")) {
        ParseFdForCollection(option, "--profile-file-fd", &profile_files_fd_);
      } else if (option.starts_with("--reference-profile-file=")) {
        reference_profile_file_ = option.substr(strlen("--reference-profile-file=")).ToString();
      } else if (option.starts_with("--reference-profile-file-fd=")) {
        ParseUintOption(option, "--reference-profile-file-fd", &reference_profile_file_fd_, Usage);
      } else if (option.starts_with("--dex-location=")) {
        dex_locations_.push_back(option.substr(strlen("--dex-location=")).ToString());
      } else if (option.starts_with("--apk-fd=")) {
        ParseFdForCollection(option, "--apk-fd", &apks_fd_);
      } else if (option.starts_with("--apk=")) {
        apk_files_.push_back(option.substr(strlen("--apk=")).ToString());
      } else if (option.starts_with("--generate-test-profile=")) {
        test_profile_ = option.substr(strlen("--generate-test-profile=")).ToString();
      } else if (option.starts_with("--generate-test-profile-num-dex=")) {
        ParseUintOption(option,
                        "--generate-test-profile-num-dex",
                        &test_profile_num_dex_,
                        Usage);
      } else if (option.starts_with("--generate-test-profile-method-percentage")) {
        ParseUintOption(option,
                        "--generate-test-profile-method-percentage",
                        &test_profile_method_percerntage_,
                        Usage);
      } else if (option.starts_with("--generate-test-profile-class-percentage")) {
        ParseUintOption(option,
                        "--generate-test-profile-class-percentage",
                        &test_profile_class_percentage_,
                        Usage);
      } else if (option.starts_with("--generate-test-profile-seed=")) {
        ParseUintOption(option, "--generate-test-profile-seed", &test_profile_seed_, Usage);
      } else if (option.starts_with("--copy-and-update-profile-key")) {
        copy_and_update_profile_key_ = true;
      } else {
        Usage("Unknown argument '%s'", option.data());
      }
    }

    // Validate global consistency between file/fd options.
    if (!profile_files_.empty() && !profile_files_fd_.empty()) {
      Usage("Profile files should not be specified with both --profile-file-fd and --profile-file");
    }
    if (!reference_profile_file_.empty() && FdIsValid(reference_profile_file_fd_)) {
      Usage("Reference profile should not be specified with both "
            "--reference-profile-file-fd and --reference-profile-file");
    }
    if (!apk_files_.empty() && !apks_fd_.empty()) {
      Usage("APK files should not be specified with both --apk-fd and --apk");
    }
  }

  struct ProfileFilterKey {
    ProfileFilterKey(const std::string& dex_location, uint32_t checksum)
        : dex_location_(dex_location), checksum_(checksum) {}
    const std::string dex_location_;
    uint32_t checksum_;

    bool operator==(const ProfileFilterKey& other) const {
      return checksum_ == other.checksum_ && dex_location_ == other.dex_location_;
    }
    bool operator<(const ProfileFilterKey& other) const {
      return checksum_ == other.checksum_
          ?  dex_location_ < other.dex_location_
          : checksum_ < other.checksum_;
    }
  };

  ProfileAssistant::ProcessingResult ProcessProfiles() {
    // Validate that at least one profile file was passed, as well as a reference profile.
    if (profile_files_.empty() && profile_files_fd_.empty()) {
      Usage("No profile files specified.");
    }
    if (reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
      Usage("No reference profile file specified.");
    }
    if ((!profile_files_.empty() && FdIsValid(reference_profile_file_fd_)) ||
        (!profile_files_fd_.empty() && !FdIsValid(reference_profile_file_fd_))) {
      Usage("Options --profile-file-fd and --reference-profile-file-fd "
            "should only be used together");
    }

    // Check if we have any apks which we should use to filter the profile data.
    std::set<ProfileFilterKey> profile_filter_keys;
    if (!GetProfileFilterKeyFromApks(&profile_filter_keys)) {
      return ProfileAssistant::kErrorIO;
    }

    // Build the profile filter function. If the set of keys is empty it means we
    // don't have any apks; as such we do not filter anything.
    const ProfileCompilationInfo::ProfileLoadFilterFn& filter_fn =
        [profile_filter_keys](const std::string& dex_location, uint32_t checksum) {
            if (profile_filter_keys.empty()) {
              // No --apk was specified. Accept all dex files.
              return true;
            } else {
              bool res = profile_filter_keys.find(
                  ProfileFilterKey(dex_location, checksum)) != profile_filter_keys.end();
              return res;
            }
        };

    ProfileAssistant::ProcessingResult result;

    if (profile_files_.empty()) {
      // The file doesn't need to be flushed here (ProcessProfiles will do it)
      // so don't check the usage.
      File file(reference_profile_file_fd_, false);
      result = ProfileAssistant::ProcessProfiles(profile_files_fd_,
                                                 reference_profile_file_fd_,
                                                 filter_fn);
      CloseAllFds(profile_files_fd_, "profile_files_fd_");
    } else {
      result = ProfileAssistant::ProcessProfiles(profile_files_,
                                                 reference_profile_file_,
                                                 filter_fn);
    }
    return result;
  }

  bool GetProfileFilterKeyFromApks(std::set<ProfileFilterKey>* profile_filter_keys) {
    auto process_fn = [profile_filter_keys](std::unique_ptr<const DexFile>&& dex_file) {
      // Store the profile key of the location instead of the location itself.
      // This will make the matching in the profile filter method much easier.
      profile_filter_keys->emplace(ProfileCompilationInfo::GetProfileDexFileKey(
          dex_file->GetLocation()), dex_file->GetLocationChecksum());
    };
    return OpenApkFilesFromLocations(process_fn);
  }

  bool OpenApkFilesFromLocations(std::vector<std::unique_ptr<const DexFile>>* dex_files) {
    auto process_fn = [dex_files](std::unique_ptr<const DexFile>&& dex_file) {
      dex_files->emplace_back(std::move(dex_file));
    };
    return OpenApkFilesFromLocations(process_fn);
  }

  bool OpenApkFilesFromLocations(
      std::function<void(std::unique_ptr<const DexFile>&&)> process_fn) {
    bool use_apk_fd_list = !apks_fd_.empty();
    if (use_apk_fd_list) {
      // Get the APKs from the collection of FDs.
      if (dex_locations_.empty()) {
        // Try to compute the dex locations from the file paths of the descriptions.
        // This will make it easier to invoke profman with --apk-fd and without
        // being force to pass --dex-location when the location would be the apk path.
        if (!ComputeDexLocationsFromApkFds()) {
          return false;
        }
      } else {
        if (dex_locations_.size() != apks_fd_.size()) {
            Usage("The number of apk-fds must match the number of dex-locations.");
        }
      }
    } else if (!apk_files_.empty()) {
      if (dex_locations_.empty()) {
        // If no dex locations are specified use the apk names as locations.
        dex_locations_ = apk_files_;
      } else if (dex_locations_.size() != apk_files_.size()) {
          Usage("The number of apk-fds must match the number of dex-locations.");
      }
    } else {
      // No APKs were specified.
      CHECK(dex_locations_.empty());
      return true;
    }
    static constexpr bool kVerifyChecksum = true;
    for (size_t i = 0; i < dex_locations_.size(); ++i) {
      std::string error_msg;
      const ArtDexFileLoader dex_file_loader;
      std::vector<std::unique_ptr<const DexFile>> dex_files_for_location;
      // We do not need to verify the apk for processing profiles.
      if (use_apk_fd_list) {
        if (dex_file_loader.OpenZip(apks_fd_[i],
                                    dex_locations_[i],
                                    /* verify */ false,
                                    kVerifyChecksum,
                                    &error_msg,
                                    &dex_files_for_location)) {
        } else {
          LOG(ERROR) << "OpenZip failed for '" << dex_locations_[i] << "' " << error_msg;
          return false;
        }
      } else {
        if (dex_file_loader.Open(apk_files_[i].c_str(),
                                 dex_locations_[i],
                                 /* verify */ false,
                                 kVerifyChecksum,
                                 &error_msg,
                                 &dex_files_for_location)) {
        } else {
          LOG(ERROR) << "Open failed for '" << dex_locations_[i] << "' " << error_msg;
          return false;
        }
      }
      for (std::unique_ptr<const DexFile>& dex_file : dex_files_for_location) {
        process_fn(std::move(dex_file));
      }
    }
    return true;
  }

  // Get the dex locations from the apk fds.
  // The methods reads the links from /proc/self/fd/ to find the original apk paths
  // and puts them in the dex_locations_ vector.
  bool ComputeDexLocationsFromApkFds() {
    // We can't use a char array of PATH_MAX size without exceeding the frame size.
    // So we use a vector as the buffer for the path.
    std::vector<char> buffer(PATH_MAX, 0);
    for (size_t i = 0; i < apks_fd_.size(); ++i) {
      std::string fd_path = "/proc/self/fd/" + std::to_string(apks_fd_[i]);
      ssize_t len = readlink(fd_path.c_str(), buffer.data(), buffer.size() - 1);
      if (len == -1) {
        PLOG(ERROR) << "Could not open path from fd";
        return false;
      }

      buffer[len] = '\0';
      dex_locations_.push_back(buffer.data());
    }
    return true;
  }

  std::unique_ptr<const ProfileCompilationInfo> LoadProfile(const std::string& filename, int fd) {
    if (!filename.empty()) {
      fd = open(filename.c_str(), O_RDWR);
      if (fd < 0) {
        LOG(ERROR) << "Cannot open " << filename << strerror(errno);
        return nullptr;
      }
    }
    std::unique_ptr<ProfileCompilationInfo> info(new ProfileCompilationInfo);
    if (!info->Load(fd)) {
      LOG(ERROR) << "Cannot load profile info from fd=" << fd << "\n";
      return nullptr;
    }
    return info;
  }

  int DumpOneProfile(const std::string& banner,
                     const std::string& filename,
                     int fd,
                     const std::vector<std::unique_ptr<const DexFile>>* dex_files,
                     std::string* dump) {
    std::unique_ptr<const ProfileCompilationInfo> info(LoadProfile(filename, fd));
    if (info == nullptr) {
      LOG(ERROR) << "Cannot load profile info from filename=" << filename << " fd=" << fd;
      return -1;
    }
    *dump += banner + "\n" + info->DumpInfo(dex_files) + "\n";
    return 0;
  }

  int DumpProfileInfo() {
    // Validate that at least one profile file or reference was specified.
    if (profile_files_.empty() && profile_files_fd_.empty() &&
        reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
      Usage("No profile files or reference profile specified.");
    }
    static const char* kEmptyString = "";
    static const char* kOrdinaryProfile = "=== profile ===";
    static const char* kReferenceProfile = "=== reference profile ===";

    std::vector<std::unique_ptr<const DexFile>> dex_files;
    OpenApkFilesFromLocations(&dex_files);
    std::string dump;
    // Dump individual profile files.
    if (!profile_files_fd_.empty()) {
      for (int profile_file_fd : profile_files_fd_) {
        int ret = DumpOneProfile(kOrdinaryProfile,
                                 kEmptyString,
                                 profile_file_fd,
                                 &dex_files,
                                 &dump);
        if (ret != 0) {
          return ret;
        }
      }
    }
    if (!profile_files_.empty()) {
      for (const std::string& profile_file : profile_files_) {
        int ret = DumpOneProfile(kOrdinaryProfile, profile_file, kInvalidFd, &dex_files, &dump);
        if (ret != 0) {
          return ret;
        }
      }
    }
    // Dump reference profile file.
    if (FdIsValid(reference_profile_file_fd_)) {
      int ret = DumpOneProfile(kReferenceProfile,
                               kEmptyString,
                               reference_profile_file_fd_,
                               &dex_files,
                               &dump);
      if (ret != 0) {
        return ret;
      }
    }
    if (!reference_profile_file_.empty()) {
      int ret = DumpOneProfile(kReferenceProfile,
                               reference_profile_file_,
                               kInvalidFd,
                               &dex_files,
                               &dump);
      if (ret != 0) {
        return ret;
      }
    }
    if (!FdIsValid(dump_output_to_fd_)) {
      std::cout << dump;
    } else {
      unix_file::FdFile out_fd(dump_output_to_fd_, false /*check_usage*/);
      if (!out_fd.WriteFully(dump.c_str(), dump.length())) {
        return -1;
      }
    }
    return 0;
  }

  bool ShouldOnlyDumpProfile() {
    return dump_only_;
  }

  bool GetClassNamesAndMethods(int fd,
                               std::vector<std::unique_ptr<const DexFile>>* dex_files,
                               std::set<std::string>* out_lines) {
    ProfileCompilationInfo profile_info;
    if (!profile_info.Load(fd)) {
      LOG(ERROR) << "Cannot load profile info";
      return false;
    }
    for (const std::unique_ptr<const DexFile>& dex_file : *dex_files) {
      std::set<dex::TypeIndex> class_types;
      std::set<uint16_t> hot_methods;
      std::set<uint16_t> startup_methods;
      std::set<uint16_t> post_startup_methods;
      std::set<uint16_t> combined_methods;
      if (profile_info.GetClassesAndMethods(*dex_file.get(),
                                            &class_types,
                                            &hot_methods,
                                            &startup_methods,
                                            &post_startup_methods)) {
        for (const dex::TypeIndex& type_index : class_types) {
          const DexFile::TypeId& type_id = dex_file->GetTypeId(type_index);
          out_lines->insert(std::string(dex_file->GetTypeDescriptor(type_id)));
        }
        combined_methods = hot_methods;
        combined_methods.insert(startup_methods.begin(), startup_methods.end());
        combined_methods.insert(post_startup_methods.begin(), post_startup_methods.end());
        for (uint16_t dex_method_idx : combined_methods) {
          const DexFile::MethodId& id = dex_file->GetMethodId(dex_method_idx);
          std::string signature_string(dex_file->GetMethodSignature(id).ToString());
          std::string type_string(dex_file->GetTypeDescriptor(dex_file->GetTypeId(id.class_idx_)));
          std::string method_name(dex_file->GetMethodName(id));
          std::string flags_string;
          if (hot_methods.find(dex_method_idx) != hot_methods.end()) {
            flags_string += kMethodFlagStringHot;
          }
          if (startup_methods.find(dex_method_idx) != startup_methods.end()) {
            flags_string += kMethodFlagStringStartup;
          }
          if (post_startup_methods.find(dex_method_idx) != post_startup_methods.end()) {
            flags_string += kMethodFlagStringPostStartup;
          }
          out_lines->insert(flags_string +
                            type_string +
                            kMethodSep +
                            method_name +
                            signature_string);
        }
      }
    }
    return true;
  }

  bool GetClassNamesAndMethods(const std::string& profile_file,
                               std::vector<std::unique_ptr<const DexFile>>* dex_files,
                               std::set<std::string>* out_lines) {
    int fd = open(profile_file.c_str(), O_RDONLY);
    if (!FdIsValid(fd)) {
      LOG(ERROR) << "Cannot open " << profile_file << strerror(errno);
      return false;
    }
    if (!GetClassNamesAndMethods(fd, dex_files, out_lines)) {
      return false;
    }
    if (close(fd) < 0) {
      PLOG(WARNING) << "Failed to close descriptor";
    }
    return true;
  }

  int DumpClassesAndMethods() {
    // Validate that at least one profile file or reference was specified.
    if (profile_files_.empty() && profile_files_fd_.empty() &&
        reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
      Usage("No profile files or reference profile specified.");
    }

    // Open the dex files to get the names for classes.
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    OpenApkFilesFromLocations(&dex_files);
    // Build a vector of class names from individual profile files.
    std::set<std::string> class_names;
    if (!profile_files_fd_.empty()) {
      for (int profile_file_fd : profile_files_fd_) {
        if (!GetClassNamesAndMethods(profile_file_fd, &dex_files, &class_names)) {
          return -1;
        }
      }
    }
    if (!profile_files_.empty()) {
      for (const std::string& profile_file : profile_files_) {
        if (!GetClassNamesAndMethods(profile_file, &dex_files, &class_names)) {
          return -1;
        }
      }
    }
    // Concatenate class names from reference profile file.
    if (FdIsValid(reference_profile_file_fd_)) {
      if (!GetClassNamesAndMethods(reference_profile_file_fd_, &dex_files, &class_names)) {
        return -1;
      }
    }
    if (!reference_profile_file_.empty()) {
      if (!GetClassNamesAndMethods(reference_profile_file_, &dex_files, &class_names)) {
        return -1;
      }
    }
    // Dump the class names.
    std::string dump;
    for (const std::string& class_name : class_names) {
      dump += class_name + std::string("\n");
    }
    if (!FdIsValid(dump_output_to_fd_)) {
      std::cout << dump;
    } else {
      unix_file::FdFile out_fd(dump_output_to_fd_, false /*check_usage*/);
      if (!out_fd.WriteFully(dump.c_str(), dump.length())) {
        return -1;
      }
    }
    return 0;
  }

  bool ShouldOnlyDumpClassesAndMethods() {
    return dump_classes_and_methods_;
  }

  // Read lines from the given file, dropping comments and empty lines. Post-process each line with
  // the given function.
  template <typename T>
  static T* ReadCommentedInputFromFile(
      const char* input_filename, std::function<std::string(const char*)>* process) {
    std::unique_ptr<std::ifstream> input_file(new std::ifstream(input_filename, std::ifstream::in));
    if (input_file.get() == nullptr) {
      LOG(ERROR) << "Failed to open input file " << input_filename;
      return nullptr;
    }
    std::unique_ptr<T> result(
        ReadCommentedInputStream<T>(*input_file, process));
    input_file->close();
    return result.release();
  }

  // Read lines from the given stream, dropping comments and empty lines. Post-process each line
  // with the given function.
  template <typename T>
  static T* ReadCommentedInputStream(
      std::istream& in_stream,
      std::function<std::string(const char*)>* process) {
    std::unique_ptr<T> output(new T());
    while (in_stream.good()) {
      std::string dot;
      std::getline(in_stream, dot);
      if (android::base::StartsWith(dot, "#") || dot.empty()) {
        continue;
      }
      if (process != nullptr) {
        std::string descriptor((*process)(dot.c_str()));
        output->insert(output->end(), descriptor);
      } else {
        output->insert(output->end(), dot);
      }
    }
    return output.release();
  }

  // Find class klass_descriptor in the given dex_files and store its reference
  // in the out parameter class_ref.
  // Return true if the definition of the class was found in any of the dex_files.
  bool FindClass(const std::vector<std::unique_ptr<const DexFile>>& dex_files,
                 const std::string& klass_descriptor,
                 /*out*/TypeReference* class_ref) {
    constexpr uint16_t kInvalidTypeIndex = std::numeric_limits<uint16_t>::max() - 1;
    for (const std::unique_ptr<const DexFile>& dex_file_ptr : dex_files) {
      const DexFile* dex_file = dex_file_ptr.get();
      if (klass_descriptor == kInvalidClassDescriptor) {
        if (kInvalidTypeIndex >= dex_file->NumTypeIds()) {
          // The dex file does not contain all possible type ids which leaves us room
          // to add an "invalid" type id.
          *class_ref = TypeReference(dex_file, dex::TypeIndex(kInvalidTypeIndex));
          return true;
        } else {
          // The dex file contains all possible type ids. We don't have any free type id
          // that we can use as invalid.
          continue;
        }
      }

      const DexFile::TypeId* type_id = dex_file->FindTypeId(klass_descriptor.c_str());
      if (type_id == nullptr) {
        continue;
      }
      dex::TypeIndex type_index = dex_file->GetIndexForTypeId(*type_id);
      if (dex_file->FindClassDef(type_index) == nullptr) {
        // Class is only referenced in the current dex file but not defined in it.
        continue;
      }
      *class_ref = TypeReference(dex_file, type_index);
      return true;
    }
    return false;
  }

  // Find the method specified by method_spec in the class class_ref.
  uint32_t FindMethodIndex(const TypeReference& class_ref,
                           const std::string& method_spec) {
    const DexFile* dex_file = class_ref.dex_file;
    if (method_spec == kInvalidMethod) {
      constexpr uint16_t kInvalidMethodIndex = std::numeric_limits<uint16_t>::max() - 1;
      return kInvalidMethodIndex >= dex_file->NumMethodIds()
             ? kInvalidMethodIndex
             : dex::kDexNoIndex;
    }

    std::vector<std::string> name_and_signature;
    Split(method_spec, kProfileParsingFirstCharInSignature, &name_and_signature);
    if (name_and_signature.size() != 2) {
      LOG(ERROR) << "Invalid method name and signature " << method_spec;
      return dex::kDexNoIndex;
    }

    const std::string& name = name_and_signature[0];
    const std::string& signature = kProfileParsingFirstCharInSignature + name_and_signature[1];

    const DexFile::StringId* name_id = dex_file->FindStringId(name.c_str());
    if (name_id == nullptr) {
      LOG(WARNING) << "Could not find name: "  << name;
      return dex::kDexNoIndex;
    }
    dex::TypeIndex return_type_idx;
    std::vector<dex::TypeIndex> param_type_idxs;
    if (!dex_file->CreateTypeList(signature, &return_type_idx, &param_type_idxs)) {
      LOG(WARNING) << "Could not create type list" << signature;
      return dex::kDexNoIndex;
    }
    const DexFile::ProtoId* proto_id = dex_file->FindProtoId(return_type_idx, param_type_idxs);
    if (proto_id == nullptr) {
      LOG(WARNING) << "Could not find proto_id: " << name;
      return dex::kDexNoIndex;
    }
    const DexFile::MethodId* method_id = dex_file->FindMethodId(
        dex_file->GetTypeId(class_ref.TypeIndex()), *name_id, *proto_id);
    if (method_id == nullptr) {
      LOG(WARNING) << "Could not find method_id: " << name;
      return dex::kDexNoIndex;
    }

    return dex_file->GetIndexForMethodId(*method_id);
  }

  // Given a method, return true if the method has a single INVOKE_VIRTUAL in its byte code.
  // Upon success it returns true and stores the method index and the invoke dex pc
  // in the output parameters.
  // The format of the method spec is "inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,LSubC;".
  //
  // TODO(calin): support INVOKE_INTERFACE and the range variants.
  bool HasSingleInvoke(const TypeReference& class_ref,
                       uint16_t method_index,
                       /*out*/uint32_t* dex_pc) {
    const DexFile* dex_file = class_ref.dex_file;
    uint32_t offset = dex_file->FindCodeItemOffset(
        *dex_file->FindClassDef(class_ref.TypeIndex()),
        method_index);
    const DexFile::CodeItem* code_item = dex_file->GetCodeItem(offset);

    bool found_invoke = false;
    for (const DexInstructionPcPair& inst : CodeItemInstructionAccessor(*dex_file, code_item)) {
      if (inst->Opcode() == Instruction::INVOKE_VIRTUAL) {
        if (found_invoke) {
          LOG(ERROR) << "Multiple invoke INVOKE_VIRTUAL found: "
                     << dex_file->PrettyMethod(method_index);
          return false;
        }
        found_invoke = true;
        *dex_pc = inst.DexPc();
      }
    }
    if (!found_invoke) {
      LOG(ERROR) << "Could not find any INVOKE_VIRTUAL: " << dex_file->PrettyMethod(method_index);
    }
    return found_invoke;
  }

  // Process a line defining a class or a method and its inline caches.
  // Upon success return true and add the class or the method info to profile.
  // The possible line formats are:
  // "LJustTheCass;".
  // "LTestInline;->inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,LSubC;".
  // "LTestInline;->inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,invalid_class".
  // "LTestInline;->inlineMissingTypes(LSuper;)I+missing_types".
  // "LTestInline;->inlineNoInlineCaches(LSuper;)I".
  // "LTestInline;->*".
  // "invalid_class".
  // "LTestInline;->invalid_method".
  // The method and classes are searched only in the given dex files.
  bool ProcessLine(const std::vector<std::unique_ptr<const DexFile>>& dex_files,
                   const std::string& line,
                   /*out*/ProfileCompilationInfo* profile) {
    std::string klass;
    std::string method_str;
    bool is_hot = false;
    bool is_startup = false;
    bool is_post_startup = false;
    const size_t method_sep_index = line.find(kMethodSep, 0);
    if (method_sep_index == std::string::npos) {
      klass = line.substr(0);
    } else {
      // The method prefix flags are only valid for method strings.
      size_t start_index = 0;
      while (start_index < line.size() && line[start_index] != 'L') {
        const char c = line[start_index];
        if (c == kMethodFlagStringHot) {
          is_hot = true;
        } else if (c == kMethodFlagStringStartup) {
          is_startup = true;
        } else if (c == kMethodFlagStringPostStartup) {
          is_post_startup = true;
        } else {
          LOG(WARNING) << "Invalid flag " << c;
          return false;
        }
        ++start_index;
      }
      klass = line.substr(start_index, method_sep_index - start_index);
      method_str = line.substr(method_sep_index + kMethodSep.size());
    }

    uint32_t flags = 0;
    if (is_hot) {
      flags |= ProfileCompilationInfo::MethodHotness::kFlagHot;
    }
    if (is_startup) {
      flags |= ProfileCompilationInfo::MethodHotness::kFlagStartup;
    }
    if (is_post_startup) {
      flags |= ProfileCompilationInfo::MethodHotness::kFlagPostStartup;
    }

    TypeReference class_ref(/* dex_file */ nullptr, dex::TypeIndex());
    if (!FindClass(dex_files, klass, &class_ref)) {
      LOG(WARNING) << "Could not find class: " << klass;
      return false;
    }

    if (method_str.empty() || method_str == kClassAllMethods) {
      // Start by adding the class.
      std::set<DexCacheResolvedClasses> resolved_class_set;
      const DexFile* dex_file = class_ref.dex_file;
      const auto& dex_resolved_classes = resolved_class_set.emplace(
            dex_file->GetLocation(),
            DexFileLoader::GetBaseLocation(dex_file->GetLocation()),
            dex_file->GetLocationChecksum(),
            dex_file->NumMethodIds());
      dex_resolved_classes.first->AddClass(class_ref.TypeIndex());
      std::vector<ProfileMethodInfo> methods;
      if (method_str == kClassAllMethods) {
        // Add all of the methods.
        const DexFile::ClassDef* class_def = dex_file->FindClassDef(class_ref.TypeIndex());
        const uint8_t* class_data = dex_file->GetClassData(*class_def);
        if (class_data != nullptr) {
          ClassDataItemIterator it(*dex_file, class_data);
          it.SkipAllFields();
          while (it.HasNextMethod()) {
            if (it.GetMethodCodeItemOffset() != 0) {
              // Add all of the methods that have code to the profile.
              const uint32_t method_idx = it.GetMemberIndex();
              methods.push_back(ProfileMethodInfo(MethodReference(dex_file, method_idx)));
            }
            it.Next();
          }
        }
      }
      // TODO: Check return values?
      profile->AddMethods(methods, static_cast<ProfileCompilationInfo::MethodHotness::Flag>(flags));
      profile->AddClasses(resolved_class_set);
      return true;
    }

    // Process the method.
    std::string method_spec;
    std::vector<std::string> inline_cache_elems;

    // If none of the flags are set, default to hot.
    is_hot = is_hot || (!is_hot && !is_startup && !is_post_startup);

    std::vector<std::string> method_elems;
    bool is_missing_types = false;
    Split(method_str, kProfileParsingInlineChacheSep, &method_elems);
    if (method_elems.size() == 2) {
      method_spec = method_elems[0];
      is_missing_types = method_elems[1] == kMissingTypesMarker;
      if (!is_missing_types) {
        Split(method_elems[1], kProfileParsingTypeSep, &inline_cache_elems);
      }
    } else if (method_elems.size() == 1) {
      method_spec = method_elems[0];
    } else {
      LOG(ERROR) << "Invalid method line: " << line;
      return false;
    }

    const uint32_t method_index = FindMethodIndex(class_ref, method_spec);
    if (method_index == dex::kDexNoIndex) {
      return false;
    }

    std::vector<ProfileMethodInfo::ProfileInlineCache> inline_caches;
    if (is_missing_types || !inline_cache_elems.empty()) {
      uint32_t dex_pc;
      if (!HasSingleInvoke(class_ref, method_index, &dex_pc)) {
        return false;
      }
      std::vector<TypeReference> classes(inline_cache_elems.size(),
                                         TypeReference(/* dex_file */ nullptr, dex::TypeIndex()));
      size_t class_it = 0;
      for (const std::string& ic_class : inline_cache_elems) {
        if (!FindClass(dex_files, ic_class, &(classes[class_it++]))) {
          LOG(ERROR) << "Could not find class: " << ic_class;
          return false;
        }
      }
      inline_caches.emplace_back(dex_pc, is_missing_types, classes);
    }
    MethodReference ref(class_ref.dex_file, method_index);
    if (is_hot) {
      profile->AddMethod(ProfileMethodInfo(ref, inline_caches),
          static_cast<ProfileCompilationInfo::MethodHotness::Flag>(flags));
    }
    if (flags != 0) {
      if (!profile->AddMethodIndex(
          static_cast<ProfileCompilationInfo::MethodHotness::Flag>(flags), ref)) {
        return false;
      }
      DCHECK(profile->GetMethodHotness(ref).IsInProfile());
    }
    return true;
  }

  int OpenReferenceProfile() const {
    int fd = reference_profile_file_fd_;
    if (!FdIsValid(fd)) {
      CHECK(!reference_profile_file_.empty());
      fd = open(reference_profile_file_.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
      if (fd < 0) {
        LOG(ERROR) << "Cannot open " << reference_profile_file_ << strerror(errno);
        return kInvalidFd;
      }
    }
    return fd;
  }

  // Creates a profile from a human friendly textual representation.
  // The expected input format is:
  //   # Classes
  //   Ljava/lang/Comparable;
  //   Ljava/lang/Math;
  //   # Methods with inline caches
  //   LTestInline;->inlinePolymorphic(LSuper;)I+LSubA;,LSubB;,LSubC;
  //   LTestInline;->noInlineCache(LSuper;)I
  int CreateProfile() {
    // Validate parameters for this command.
    if (apk_files_.empty() && apks_fd_.empty()) {
      Usage("APK files must be specified");
    }
    if (dex_locations_.empty()) {
      Usage("DEX locations must be specified");
    }
    if (reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
      Usage("Reference profile must be specified with --reference-profile-file or "
            "--reference-profile-file-fd");
    }
    if (!profile_files_.empty() || !profile_files_fd_.empty()) {
      Usage("Profile must be specified with --reference-profile-file or "
            "--reference-profile-file-fd");
    }
    // Open the profile output file if needed.
    int fd = OpenReferenceProfile();
    if (!FdIsValid(fd)) {
        return -1;
    }
    // Read the user-specified list of classes and methods.
    std::unique_ptr<std::unordered_set<std::string>>
        user_lines(ReadCommentedInputFromFile<std::unordered_set<std::string>>(
            create_profile_from_file_.c_str(), nullptr));  // No post-processing.

    // Open the dex files to look up classes and methods.
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    OpenApkFilesFromLocations(&dex_files);

    // Process the lines one by one and add the successful ones to the profile.
    ProfileCompilationInfo info;

    for (const auto& line : *user_lines) {
      ProcessLine(dex_files, line, &info);
    }

    // Write the profile file.
    CHECK(info.Save(fd));
    if (close(fd) < 0) {
      PLOG(WARNING) << "Failed to close descriptor";
    }
    return 0;
  }

  bool ShouldCreateBootProfile() const {
    return generate_boot_image_profile_;
  }

  int CreateBootProfile() {
    // Open the profile output file.
    const int reference_fd = OpenReferenceProfile();
    if (!FdIsValid(reference_fd)) {
      PLOG(ERROR) << "Error opening reference profile";
      return -1;
    }
    // Open the dex files.
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    OpenApkFilesFromLocations(&dex_files);
    if (dex_files.empty()) {
      PLOG(ERROR) << "Expected dex files for creating boot profile";
      return -2;
    }
    // Open the input profiles.
    std::vector<std::unique_ptr<const ProfileCompilationInfo>> profiles;
    if (!profile_files_fd_.empty()) {
      for (int profile_file_fd : profile_files_fd_) {
        std::unique_ptr<const ProfileCompilationInfo> profile(LoadProfile("", profile_file_fd));
        if (profile == nullptr) {
          return -3;
        }
        profiles.emplace_back(std::move(profile));
      }
    }
    if (!profile_files_.empty()) {
      for (const std::string& profile_file : profile_files_) {
        std::unique_ptr<const ProfileCompilationInfo> profile(LoadProfile(profile_file, kInvalidFd));
        if (profile == nullptr) {
          return -4;
        }
        profiles.emplace_back(std::move(profile));
      }
    }
    ProfileCompilationInfo out_profile;
    GenerateBootImageProfile(dex_files,
                             profiles,
                             boot_image_options_,
                             VLOG_IS_ON(profiler),
                             &out_profile);
    out_profile.Save(reference_fd);
    close(reference_fd);
    return 0;
  }

  bool ShouldCreateProfile() {
    return !create_profile_from_file_.empty();
  }

  int GenerateTestProfile() {
    // Validate parameters for this command.
    if (test_profile_method_percerntage_ > 100) {
      Usage("Invalid percentage for --generate-test-profile-method-percentage");
    }
    if (test_profile_class_percentage_ > 100) {
      Usage("Invalid percentage for --generate-test-profile-class-percentage");
    }
    // If given APK files or DEX locations, check that they're ok.
    if (!apk_files_.empty() || !apks_fd_.empty() || !dex_locations_.empty()) {
      if (apk_files_.empty() && apks_fd_.empty()) {
        Usage("APK files must be specified when passing DEX locations to --generate-test-profile");
      }
      if (dex_locations_.empty()) {
        Usage("DEX locations must be specified when passing APK files to --generate-test-profile");
      }
    }
    // ShouldGenerateTestProfile confirms !test_profile_.empty().
    int profile_test_fd = open(test_profile_.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (profile_test_fd < 0) {
      LOG(ERROR) << "Cannot open " << test_profile_ << strerror(errno);
      return -1;
    }
    bool result;
    if (apk_files_.empty() && apks_fd_.empty() && dex_locations_.empty()) {
      result = ProfileCompilationInfo::GenerateTestProfile(profile_test_fd,
                                                           test_profile_num_dex_,
                                                           test_profile_method_percerntage_,
                                                           test_profile_class_percentage_,
                                                           test_profile_seed_);
    } else {
      // Open the dex files to look up classes and methods.
      std::vector<std::unique_ptr<const DexFile>> dex_files;
      OpenApkFilesFromLocations(&dex_files);
      // Create a random profile file based on the set of dex files.
      result = ProfileCompilationInfo::GenerateTestProfile(profile_test_fd,
                                                           dex_files,
                                                           test_profile_method_percerntage_,
                                                           test_profile_class_percentage_,
                                                           test_profile_seed_);
    }
    close(profile_test_fd);  // ignore close result.
    return result ? 0 : -1;
  }

  bool ShouldGenerateTestProfile() {
    return !test_profile_.empty();
  }

  bool ShouldCopyAndUpdateProfileKey() const {
    return copy_and_update_profile_key_;
  }

  int32_t CopyAndUpdateProfileKey() {
    // Validate that at least one profile file was passed, as well as a reference profile.
    if (!(profile_files_.size() == 1 ^ profile_files_fd_.size() == 1)) {
      Usage("Only one profile file should be specified.");
    }
    if (reference_profile_file_.empty() && !FdIsValid(reference_profile_file_fd_)) {
      Usage("No reference profile file specified.");
    }

    if (apk_files_.empty() && apks_fd_.empty()) {
      Usage("No apk files specified");
    }

    static constexpr int32_t kErrorFailedToUpdateProfile = -1;
    static constexpr int32_t kErrorFailedToSaveProfile = -2;
    static constexpr int32_t kErrorFailedToLoadProfile = -3;

    bool use_fds = profile_files_fd_.size() == 1;

    ProfileCompilationInfo profile;
    // Do not clear if invalid. The input might be an archive.
    bool load_ok = use_fds
        ? profile.Load(profile_files_fd_[0])
        : profile.Load(profile_files_[0], /*clear_if_invalid*/ false);
    if (load_ok) {
      // Open the dex files to look up classes and methods.
      std::vector<std::unique_ptr<const DexFile>> dex_files;
      OpenApkFilesFromLocations(&dex_files);
      if (!profile.UpdateProfileKeys(dex_files)) {
        return kErrorFailedToUpdateProfile;
      }
      bool result = use_fds
          ? profile.Save(reference_profile_file_fd_)
          : profile.Save(reference_profile_file_, /*bytes_written*/ nullptr);
      return result ? 0 : kErrorFailedToSaveProfile;
    } else {
      return kErrorFailedToLoadProfile;
    }
  }

 private:
  static void ParseFdForCollection(const StringPiece& option,
                                   const char* arg_name,
                                   std::vector<int>* fds) {
    int fd;
    ParseUintOption(option, arg_name, &fd, Usage);
    fds->push_back(fd);
  }

  static void CloseAllFds(const std::vector<int>& fds, const char* descriptor) {
    for (size_t i = 0; i < fds.size(); i++) {
      if (close(fds[i]) < 0) {
        PLOG(WARNING) << "Failed to close descriptor for "
            << descriptor << " at index " << i << ": " << fds[i];
      }
    }
  }

  void LogCompletionTime() {
    static constexpr uint64_t kLogThresholdTime = MsToNs(100);  // 100ms
    uint64_t time_taken = NanoTime() - start_ns_;
    if (time_taken > kLogThresholdTime) {
      LOG(WARNING) << "profman took " << PrettyDuration(time_taken);
    }
  }

  std::vector<std::string> profile_files_;
  std::vector<int> profile_files_fd_;
  std::vector<std::string> dex_locations_;
  std::vector<std::string> apk_files_;
  std::vector<int> apks_fd_;
  std::string reference_profile_file_;
  int reference_profile_file_fd_;
  bool dump_only_;
  bool dump_classes_and_methods_;
  bool generate_boot_image_profile_;
  int dump_output_to_fd_;
  BootImageOptions boot_image_options_;
  std::string test_profile_;
  std::string create_profile_from_file_;
  uint16_t test_profile_num_dex_;
  uint16_t test_profile_method_percerntage_;
  uint16_t test_profile_class_percentage_;
  uint32_t test_profile_seed_;
  uint64_t start_ns_;
  bool copy_and_update_profile_key_;
};

// See ProfileAssistant::ProcessingResult for return codes.
static int profman(int argc, char** argv) {
  ProfMan profman;

  // Parse arguments. Argument mistakes will lead to exit(EXIT_FAILURE) in UsageError.
  profman.ParseArgs(argc, argv);

  // Initialize MemMap for ZipArchive::OpenFromFd.
  MemMap::Init();

  if (profman.ShouldGenerateTestProfile()) {
    return profman.GenerateTestProfile();
  }
  if (profman.ShouldOnlyDumpProfile()) {
    return profman.DumpProfileInfo();
  }
  if (profman.ShouldOnlyDumpClassesAndMethods()) {
    return profman.DumpClassesAndMethods();
  }
  if (profman.ShouldCreateProfile()) {
    return profman.CreateProfile();
  }

  if (profman.ShouldCreateBootProfile()) {
    return profman.CreateBootProfile();
  }

  if (profman.ShouldCopyAndUpdateProfileKey()) {
    return profman.CopyAndUpdateProfileKey();
  }

  // Process profile information and assess if we need to do a profile guided compilation.
  // This operation involves I/O.
  return profman.ProcessProfiles();
}

}  // namespace art

int main(int argc, char **argv) {
  return art::profman(argc, argv);
}

