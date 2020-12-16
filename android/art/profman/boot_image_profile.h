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

#ifndef ART_PROFMAN_BOOT_IMAGE_PROFILE_H_
#define ART_PROFMAN_BOOT_IMAGE_PROFILE_H_

#include <limits>
#include <memory>
#include <vector>

#include "dex/dex_file.h"

namespace art {

class ProfileCompilationInfo;

struct BootImageOptions {
 public:
  // Threshold for classes that may be dirty or clean. The threshold specifies how
  // many different profiles need to have the class before it gets added to the boot profile.
  uint32_t image_class_theshold = 10;

  // Threshold for classes that are likely to remain clean. The threshold specifies how
  // many different profiles need to have the class before it gets added to the boot profile.
  uint32_t image_class_clean_theshold = 3;

  // Threshold for non-hot methods to be compiled. The threshold specifies how
  // many different profiles need to have the method before it gets added to the boot profile.
  uint32_t compiled_method_threshold = std::numeric_limits<uint32_t>::max();
};

// Merge a bunch of profiles together to generate a boot profile. Classes and methods are added
// to the out_profile if they meet the options.
void GenerateBootImageProfile(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files,
    const std::vector<std::unique_ptr<const ProfileCompilationInfo>>& profiles,
    const BootImageOptions& options,
    bool verbose,
    ProfileCompilationInfo* out_profile);

}  // namespace art

#endif  // ART_PROFMAN_BOOT_IMAGE_PROFILE_H_
