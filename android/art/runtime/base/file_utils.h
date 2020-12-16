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

#ifndef ART_RUNTIME_BASE_FILE_UTILS_H_
#define ART_RUNTIME_BASE_FILE_UTILS_H_

#include <stdlib.h>

#include <string>

#include <android-base/logging.h>

#include "arch/instruction_set.h"

namespace art {

bool ReadFileToString(const std::string& file_name, std::string* result);
bool PrintFileToLog(const std::string& file_name, android::base::LogSeverity level);

// Find $ANDROID_ROOT, /system, or abort.
std::string GetAndroidRoot();
// Find $ANDROID_ROOT, /system, or return an empty string.
std::string GetAndroidRootSafe(std::string* error_msg);

// Find $ANDROID_DATA, /data, or abort.
const char* GetAndroidData();
// Find $ANDROID_DATA, /data, or return null.
const char* GetAndroidDataSafe(std::string* error_msg);

// Returns the default boot image location (ANDROID_ROOT/framework/boot.art).
// Returns an empty string if ANDROID_ROOT is not set.
std::string GetDefaultBootImageLocation(std::string* error_msg);

// Returns the dalvik-cache location, with subdir appended. Returns the empty string if the cache
// could not be found.
std::string GetDalvikCache(const char* subdir);
// Return true if we found the dalvik cache and stored it in the dalvik_cache argument.
// have_android_data will be set to true if we have an ANDROID_DATA that exists,
// dalvik_cache_exists will be true if there is a dalvik-cache directory that is present.
// The flag is_global_cache tells whether this cache is /data/dalvik-cache.
void GetDalvikCache(const char* subdir, bool create_if_absent, std::string* dalvik_cache,
                    bool* have_android_data, bool* dalvik_cache_exists, bool* is_global_cache);

// Returns the absolute dalvik-cache path for a DexFile or OatFile. The path returned will be
// rooted at cache_location.
bool GetDalvikCacheFilename(const char* file_location, const char* cache_location,
                            std::string* filename, std::string* error_msg);

// Returns the system location for an image
std::string GetSystemImageFilename(const char* location, InstructionSet isa);

// Returns the vdex filename for the given oat filename.
std::string GetVdexFilename(const std::string& oat_filename);

// Returns `filename` with the text after the last occurrence of '.' replaced with
// `extension`. If `filename` does not contain a period, returns a string containing `filename`,
// a period, and `new_extension`.
// Example: ReplaceFileExtension("foo.bar", "abc") == "foo.abc"
//          ReplaceFileExtension("foo", "abc") == "foo.abc"
std::string ReplaceFileExtension(const std::string& filename, const std::string& new_extension);

// Return whether the location is on system (i.e. android root).
bool LocationIsOnSystem(const char* location);

// Return whether the location is on system/framework (i.e. android_root/framework).
bool LocationIsOnSystemFramework(const char* location);

}  // namespace art

#endif  // ART_RUNTIME_BASE_FILE_UTILS_H_
