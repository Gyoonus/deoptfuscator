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

#include <fstream>
#include <iostream>
#include <unordered_set>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/hidden_api_access_flags.h"
#include "mem_map.h"

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

NO_RETURN static void Usage(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: hiddenapi [options]...");
  UsageError("");
  UsageError("  --dex=<filename>: specify dex file whose members' access flags are to be set.");
  UsageError("      At least one --dex parameter must be specified.");
  UsageError("");
  UsageError("  --light-greylist=<filename>:");
  UsageError("  --dark-greylist=<filename>:");
  UsageError("  --blacklist=<filename>: text files with signatures of methods/fields to be marked");
  UsageError("      greylisted/blacklisted respectively. At least one list must be provided.");
  UsageError("");
  UsageError("  --print-hidden-api: dump a list of marked methods/fields to the standard output.");
  UsageError("      There is no indication which API category they belong to.");
  UsageError("");

  exit(EXIT_FAILURE);
}

class DexClass {
 public:
  DexClass(const DexFile& dex_file, uint32_t idx)
      : dex_file_(dex_file), class_def_(dex_file.GetClassDef(idx)) {}

  const DexFile& GetDexFile() const { return dex_file_; }

  const dex::TypeIndex GetClassIndex() const { return class_def_.class_idx_; }

  const uint8_t* GetData() const { return dex_file_.GetClassData(class_def_); }

  const char* GetDescriptor() const { return dex_file_.GetClassDescriptor(class_def_); }

 private:
  const DexFile& dex_file_;
  const DexFile::ClassDef& class_def_;
};

class DexMember {
 public:
  DexMember(const DexClass& klass, const ClassDataItemIterator& it)
      : klass_(klass), it_(it) {
    DCHECK_EQ(it_.IsAtMethod() ? GetMethodId().class_idx_ : GetFieldId().class_idx_,
              klass_.GetClassIndex());
  }

  // Sets hidden bits in access flags and writes them back into the DEX in memory.
  // Note that this will not update the cached data of ClassDataItemIterator
  // until it iterates over this item again and therefore will fail a CHECK if
  // it is called multiple times on the same DexMember.
  void SetHidden(HiddenApiAccessFlags::ApiList value) {
    const uint32_t old_flags = it_.GetRawMemberAccessFlags();
    const uint32_t new_flags = HiddenApiAccessFlags::EncodeForDex(old_flags, value);
    CHECK_EQ(UnsignedLeb128Size(new_flags), UnsignedLeb128Size(old_flags));

    // Locate the LEB128-encoded access flags in class data.
    // `ptr` initially points to the next ClassData item. We iterate backwards
    // until we hit the terminating byte of the previous Leb128 value.
    const uint8_t* ptr = it_.DataPointer();
    if (it_.IsAtMethod()) {
      ptr = ReverseSearchUnsignedLeb128(ptr);
      DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), it_.GetMethodCodeItemOffset());
    }
    ptr = ReverseSearchUnsignedLeb128(ptr);
    DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), old_flags);

    // Overwrite the access flags.
    UpdateUnsignedLeb128(const_cast<uint8_t*>(ptr), new_flags);
  }

  // Returns true if this member's API entry is in `list`.
  bool IsOnApiList(const std::unordered_set<std::string>& list) const {
    return list.find(GetApiEntry()) != list.end();
  }

  // Constructs a string with a unique signature of this class member.
  std::string GetApiEntry() const {
    std::stringstream ss;
    ss << klass_.GetDescriptor() << "->";
    if (it_.IsAtMethod()) {
      const DexFile::MethodId& mid = GetMethodId();
      ss << klass_.GetDexFile().GetMethodName(mid)
         << klass_.GetDexFile().GetMethodSignature(mid).ToString();
    } else {
      const DexFile::FieldId& fid = GetFieldId();
      ss << klass_.GetDexFile().GetFieldName(fid) << ":"
         << klass_.GetDexFile().GetFieldTypeDescriptor(fid);
    }
    return ss.str();
  }

 private:
  inline const DexFile::MethodId& GetMethodId() const {
    DCHECK(it_.IsAtMethod());
    return klass_.GetDexFile().GetMethodId(it_.GetMemberIndex());
  }

  inline const DexFile::FieldId& GetFieldId() const {
    DCHECK(!it_.IsAtMethod());
    return klass_.GetDexFile().GetFieldId(it_.GetMemberIndex());
  }

  const DexClass& klass_;
  const ClassDataItemIterator& it_;
};

class HiddenApi FINAL {
 public:
  HiddenApi() : print_hidden_api_(false) {}

  void ParseArgs(int argc, char** argv) {
    original_argc = argc;
    original_argv = argv;

    android::base::InitLogging(argv);

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
        LOG(INFO) << "hiddenapi: option[" << i << "]=" << argv[i];
      }
      if (option == "--print-hidden-api") {
        print_hidden_api_ = true;
      } else if (option.starts_with("--dex=")) {
        dex_paths_.push_back(option.substr(strlen("--dex=")).ToString());
      } else if (option.starts_with("--light-greylist=")) {
        light_greylist_path_ = option.substr(strlen("--light-greylist=")).ToString();
      } else if (option.starts_with("--dark-greylist=")) {
        dark_greylist_path_ = option.substr(strlen("--dark-greylist=")).ToString();
      } else if (option.starts_with("--blacklist=")) {
        blacklist_path_ = option.substr(strlen("--blacklist=")).ToString();
      } else {
        Usage("Unknown argument '%s'", option.data());
      }
    }
  }

  bool ProcessDexFiles() {
    if (dex_paths_.empty()) {
      Usage("No DEX files specified");
    }

    if (light_greylist_path_.empty() && dark_greylist_path_.empty() && blacklist_path_.empty()) {
      Usage("No API file specified");
    }

    if (!light_greylist_path_.empty() && !OpenApiFile(light_greylist_path_, &light_greylist_)) {
      return false;
    }

    if (!dark_greylist_path_.empty() && !OpenApiFile(dark_greylist_path_, &dark_greylist_)) {
      return false;
    }

    if (!blacklist_path_.empty() && !OpenApiFile(blacklist_path_, &blacklist_)) {
      return false;
    }

    MemMap::Init();
    if (!OpenDexFiles()) {
      return false;
    }

    DCHECK(!dex_files_.empty());
    for (auto& dex_file : dex_files_) {
      CategorizeAllClasses(*dex_file.get());
    }

    UpdateDexChecksums();
    return true;
  }

 private:
  bool OpenApiFile(const std::string& path, std::unordered_set<std::string>* list) {
    DCHECK(list->empty());
    DCHECK(!path.empty());

    std::ifstream api_file(path, std::ifstream::in);
    if (api_file.fail()) {
      LOG(ERROR) << "Unable to open file '" << path << "' " << strerror(errno);
      return false;
    }

    for (std::string line; std::getline(api_file, line);) {
      list->insert(line);
    }

    api_file.close();
    return true;
  }

  bool OpenDexFiles() {
    ArtDexFileLoader dex_loader;
    DCHECK(dex_files_.empty());

    for (const std::string& filename : dex_paths_) {
      std::string error_msg;

      File fd(filename.c_str(), O_RDWR, /* check_usage */ false);
      if (fd.Fd() == -1) {
        LOG(ERROR) << "Unable to open file '" << filename << "': " << strerror(errno);
        return false;
      }

      // Memory-map the dex file with MAP_SHARED flag so that changes in memory
      // propagate to the underlying file. We run dex file verification as if
      // the dex file was not in boot claass path to check basic assumptions,
      // such as that at most one of public/private/protected flag is set.
      // We do those checks here and skip them when loading the processed file
      // into boot class path.
      std::unique_ptr<const DexFile> dex_file(dex_loader.OpenDex(fd.Release(),
                                                                 /* location */ filename,
                                                                 /* verify */ true,
                                                                 /* verify_checksum */ true,
                                                                 /* mmap_shared */ true,
                                                                 &error_msg));
      if (dex_file.get() == nullptr) {
        LOG(ERROR) << "Open failed for '" << filename << "' " << error_msg;
        return false;
      }

      if (!dex_file->IsStandardDexFile()) {
        LOG(ERROR) << "Expected a standard dex file '" << filename << "'";
        return false;
      }

      // Change the protection of the memory mapping to read-write.
      if (!dex_file->EnableWrite()) {
        LOG(ERROR) << "Failed to enable write permission for '" << filename << "'";
        return false;
      }

      dex_files_.push_back(std::move(dex_file));
    }
    return true;
  }

  void CategorizeAllClasses(const DexFile& dex_file) {
    for (uint32_t class_idx = 0; class_idx < dex_file.NumClassDefs(); ++class_idx) {
      DexClass klass(dex_file, class_idx);
      const uint8_t* klass_data = klass.GetData();
      if (klass_data == nullptr) {
        continue;
      }

      for (ClassDataItemIterator it(klass.GetDexFile(), klass_data); it.HasNext(); it.Next()) {
        DexMember member(klass, it);

        // Catagorize member and overwrite its access flags.
        // Note that if a member appears on multiple API lists, it will be categorized
        // as the strictest.
        bool is_hidden = true;
        if (member.IsOnApiList(blacklist_)) {
          member.SetHidden(HiddenApiAccessFlags::kBlacklist);
        } else if (member.IsOnApiList(dark_greylist_)) {
          member.SetHidden(HiddenApiAccessFlags::kDarkGreylist);
        } else if (member.IsOnApiList(light_greylist_)) {
          member.SetHidden(HiddenApiAccessFlags::kLightGreylist);
        } else {
          member.SetHidden(HiddenApiAccessFlags::kWhitelist);
          is_hidden = false;
        }

        if (print_hidden_api_ && is_hidden) {
          std::cout << member.GetApiEntry() << std::endl;
        }
      }
    }
  }

  void UpdateDexChecksums() {
    for (auto& dex_file : dex_files_) {
      // Obtain a writeable pointer to the dex header.
      DexFile::Header* header = const_cast<DexFile::Header*>(&dex_file->GetHeader());
      // Recalculate checksum and overwrite the value in the header.
      header->checksum_ = dex_file->CalculateChecksum();
    }
  }

  // Print signatures of APIs which have been grey-/blacklisted.
  bool print_hidden_api_;

  // Paths to DEX files which should be processed.
  std::vector<std::string> dex_paths_;

  // Paths to text files which contain the lists of API members.
  std::string light_greylist_path_;
  std::string dark_greylist_path_;
  std::string blacklist_path_;

  // Opened DEX files. Note that these are opened as `const` but eventually will be written into.
  std::vector<std::unique_ptr<const DexFile>> dex_files_;

  // Signatures of DEX members loaded from `light_greylist_path_`, `dark_greylist_path_`,
  // `blacklist_path_`.
  std::unordered_set<std::string> light_greylist_;
  std::unordered_set<std::string> dark_greylist_;
  std::unordered_set<std::string> blacklist_;
};

}  // namespace art

int main(int argc, char** argv) {
  art::HiddenApi hiddenapi;

  // Parse arguments. Argument mistakes will lead to exit(EXIT_FAILURE) in UsageError.
  hiddenapi.ParseArgs(argc, argv);
  return hiddenapi.ProcessDexFiles() ? EXIT_SUCCESS : EXIT_FAILURE;
}
