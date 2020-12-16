/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "file_utils.h"

#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// We need dladdr.
#ifndef __APPLE__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#define DEFINED_GNU_SOURCE
#endif
#include <dlfcn.h>
#include <libgen.h>
#ifdef DEFINED_GNU_SOURCE
#undef _GNU_SOURCE
#undef DEFINED_GNU_SOURCE
#endif
#endif


#include <memory>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/bit_utils.h"
#include "base/stl_util.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex/dex_file_loader.h"
#include "globals.h"

#if defined(__APPLE__)
#include <crt_externs.h>
#include <sys/syscall.h>
#include "AvailabilityMacros.h"  // For MAC_OS_X_VERSION_MAX_ALLOWED
#endif

#if defined(__linux__)
#include <linux/unistd.h>
#endif

namespace art {

using android::base::StringAppendF;
using android::base::StringPrintf;

bool ReadFileToString(const std::string& file_name, std::string* result) {
  File file(file_name, O_RDONLY, false);
  if (!file.IsOpened()) {
    return false;
  }

  std::vector<char> buf(8 * KB);
  while (true) {
    int64_t n = TEMP_FAILURE_RETRY(read(file.Fd(), &buf[0], buf.size()));
    if (n == -1) {
      return false;
    }
    if (n == 0) {
      return true;
    }
    result->append(&buf[0], n);
  }
}

bool PrintFileToLog(const std::string& file_name, android::base::LogSeverity level) {
  File file(file_name, O_RDONLY, false);
  if (!file.IsOpened()) {
    return false;
  }

  constexpr size_t kBufSize = 256;  // Small buffer. Avoid stack overflow and stack size warnings.
  char buf[kBufSize + 1];           // +1 for terminator.
  size_t filled_to = 0;
  while (true) {
    DCHECK_LT(filled_to, kBufSize);
    int64_t n = TEMP_FAILURE_RETRY(read(file.Fd(), &buf[filled_to], kBufSize - filled_to));
    if (n <= 0) {
      // Print the rest of the buffer, if it exists.
      if (filled_to > 0) {
        buf[filled_to] = 0;
        LOG(level) << buf;
      }
      return n == 0;
    }
    // Scan for '\n'.
    size_t i = filled_to;
    bool found_newline = false;
    for (; i < filled_to + n; ++i) {
      if (buf[i] == '\n') {
        // Found a line break, that's something to print now.
        buf[i] = 0;
        LOG(level) << buf;
        // Copy the rest to the front.
        if (i + 1 < filled_to + n) {
          memmove(&buf[0], &buf[i + 1], filled_to + n - i - 1);
          filled_to = filled_to + n - i - 1;
        } else {
          filled_to = 0;
        }
        found_newline = true;
        break;
      }
    }
    if (found_newline) {
      continue;
    } else {
      filled_to += n;
      // Check if we must flush now.
      if (filled_to == kBufSize) {
        buf[kBufSize] = 0;
        LOG(level) << buf;
        filled_to = 0;
      }
    }
  }
}

std::string GetAndroidRootSafe(std::string* error_msg) {
  // Prefer ANDROID_ROOT if it's set.
  const char* android_dir = getenv("ANDROID_ROOT");
  if (android_dir != nullptr) {
    if (!OS::DirectoryExists(android_dir)) {
      *error_msg = StringPrintf("Failed to find ANDROID_ROOT directory %s", android_dir);
      return "";
    }
    return android_dir;
  }

  // Check where libart is from, and derive from there. Only do this for non-Mac.
#ifndef __APPLE__
  {
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&GetAndroidRootSafe), /* out */ &info) != 0) {
      // Make a duplicate of the fname so dirname can modify it.
      UniqueCPtr<char> fname(strdup(info.dli_fname));

      char* dir1 = dirname(fname.get());  // This is the lib directory.
      char* dir2 = dirname(dir1);         // This is the "system" directory.
      if (OS::DirectoryExists(dir2)) {
        std::string tmp = dir2;  // Make a copy here so that fname can be released.
        return tmp;
      }
    }
  }
#endif

  // Try "/system".
  if (!OS::DirectoryExists("/system")) {
    *error_msg = "Failed to find ANDROID_ROOT directory /system";
    return "";
  }
  return "/system";
}

std::string GetAndroidRoot() {
  std::string error_msg;
  std::string ret = GetAndroidRootSafe(&error_msg);
  if (ret.empty()) {
    LOG(FATAL) << error_msg;
    UNREACHABLE();
  }
  return ret;
}


static const char* GetAndroidDirSafe(const char* env_var,
                                     const char* default_dir,
                                     std::string* error_msg) {
  const char* android_dir = getenv(env_var);
  if (android_dir == nullptr) {
    if (OS::DirectoryExists(default_dir)) {
      android_dir = default_dir;
    } else {
      *error_msg = StringPrintf("%s not set and %s does not exist", env_var, default_dir);
      return nullptr;
    }
  }
  if (!OS::DirectoryExists(android_dir)) {
    *error_msg = StringPrintf("Failed to find %s directory %s", env_var, android_dir);
    return nullptr;
  }
  return android_dir;
}

static const char* GetAndroidDir(const char* env_var, const char* default_dir) {
  std::string error_msg;
  const char* dir = GetAndroidDirSafe(env_var, default_dir, &error_msg);
  if (dir != nullptr) {
    return dir;
  } else {
    LOG(FATAL) << error_msg;
    return nullptr;
  }
}

const char* GetAndroidData() {
  return GetAndroidDir("ANDROID_DATA", "/data");
}

const char* GetAndroidDataSafe(std::string* error_msg) {
  return GetAndroidDirSafe("ANDROID_DATA", "/data", error_msg);
}

std::string GetDefaultBootImageLocation(std::string* error_msg) {
  std::string android_root = GetAndroidRootSafe(error_msg);
  if (android_root.empty()) {
    return "";
  }
  return StringPrintf("%s/framework/boot.art", android_root.c_str());
}

void GetDalvikCache(const char* subdir, const bool create_if_absent, std::string* dalvik_cache,
                    bool* have_android_data, bool* dalvik_cache_exists, bool* is_global_cache) {
  CHECK(subdir != nullptr);
  std::string error_msg;
  const char* android_data = GetAndroidDataSafe(&error_msg);
  if (android_data == nullptr) {
    *have_android_data = false;
    *dalvik_cache_exists = false;
    *is_global_cache = false;
    return;
  } else {
    *have_android_data = true;
  }
  const std::string dalvik_cache_root(StringPrintf("%s/dalvik-cache/", android_data));
  *dalvik_cache = dalvik_cache_root + subdir;
  *dalvik_cache_exists = OS::DirectoryExists(dalvik_cache->c_str());
  *is_global_cache = strcmp(android_data, "/data") == 0;
  if (create_if_absent && !*dalvik_cache_exists && !*is_global_cache) {
    // Don't create the system's /data/dalvik-cache/... because it needs special permissions.
    *dalvik_cache_exists = ((mkdir(dalvik_cache_root.c_str(), 0700) == 0 || errno == EEXIST) &&
                            (mkdir(dalvik_cache->c_str(), 0700) == 0 || errno == EEXIST));
  }
}

std::string GetDalvikCache(const char* subdir) {
  CHECK(subdir != nullptr);
  const char* android_data = GetAndroidData();
  const std::string dalvik_cache_root(StringPrintf("%s/dalvik-cache/", android_data));
  const std::string dalvik_cache = dalvik_cache_root + subdir;
  if (!OS::DirectoryExists(dalvik_cache.c_str())) {
    // TODO: Check callers. Traditional behavior is to not abort.
    return "";
  }
  return dalvik_cache;
}

bool GetDalvikCacheFilename(const char* location, const char* cache_location,
                            std::string* filename, std::string* error_msg) {
  if (location[0] != '/') {
    *error_msg = StringPrintf("Expected path in location to be absolute: %s", location);
    return false;
  }
  std::string cache_file(&location[1]);  // skip leading slash
  if (!android::base::EndsWith(location, ".dex") &&
      !android::base::EndsWith(location, ".art") &&
      !android::base::EndsWith(location, ".oat")) {
    cache_file += "/";
    cache_file += DexFileLoader::kClassesDex;
  }
  std::replace(cache_file.begin(), cache_file.end(), '/', '@');
  *filename = StringPrintf("%s/%s", cache_location, cache_file.c_str());
  return true;
}

std::string GetVdexFilename(const std::string& oat_location) {
  return ReplaceFileExtension(oat_location, "vdex");
}

static void InsertIsaDirectory(const InstructionSet isa, std::string* filename) {
  // in = /foo/bar/baz
  // out = /foo/bar/<isa>/baz
  size_t pos = filename->rfind('/');
  CHECK_NE(pos, std::string::npos) << *filename << " " << isa;
  filename->insert(pos, "/", 1);
  filename->insert(pos + 1, GetInstructionSetString(isa));
}

std::string GetSystemImageFilename(const char* location, const InstructionSet isa) {
  // location = /system/framework/boot.art
  // filename = /system/framework/<isa>/boot.art
  std::string filename(location);
  InsertIsaDirectory(isa, &filename);
  return filename;
}

std::string ReplaceFileExtension(const std::string& filename, const std::string& new_extension) {
  const size_t last_ext = filename.find_last_of('.');
  if (last_ext == std::string::npos) {
    return filename + "." + new_extension;
  } else {
    return filename.substr(0, last_ext + 1) + new_extension;
  }
}

bool LocationIsOnSystem(const char* path) {
  UniqueCPtr<const char[]> full_path(realpath(path, nullptr));
  return full_path != nullptr &&
      android::base::StartsWith(full_path.get(), GetAndroidRoot().c_str());
}

bool LocationIsOnSystemFramework(const char* full_path) {
  std::string error_msg;
  std::string root_path = GetAndroidRootSafe(&error_msg);
  if (root_path.empty()) {
    // Could not find Android root.
    // TODO(dbrazdil): change to stricter GetAndroidRoot() once b/76452688 is resolved.
    return false;
  }
  std::string framework_path = root_path + "/framework/";
  return android::base::StartsWith(full_path, framework_path);
}

}  // namespace art
