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

#include <memory>
#include <set>

#include "boot_image_profile.h"
#include "dex/dex_file-inl.h"
#include "dex/method_reference.h"
#include "dex/type_reference.h"
#include "jit/profile_compilation_info.h"

namespace art {

using Hotness = ProfileCompilationInfo::MethodHotness;

void GenerateBootImageProfile(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files,
    const std::vector<std::unique_ptr<const ProfileCompilationInfo>>& profiles,
    const BootImageOptions& options,
    bool verbose,
    ProfileCompilationInfo* out_profile) {
  for (const std::unique_ptr<const ProfileCompilationInfo>& profile : profiles) {
    // Avoid merging classes since we may want to only add classes that fit a certain criteria.
    // If we merged the classes, every single class in each profile would be in the out_profile,
    // but we want to only included classes that are in at least a few profiles.
    out_profile->MergeWith(*profile, /*merge_classes*/ false);
  }

  // Image classes that were added because they are commonly used.
  size_t class_count = 0;
  // Image classes that were only added because they were clean.
  size_t clean_class_count = 0;
  // Total clean classes.
  size_t clean_count = 0;
  // Total dirty classes.
  size_t dirty_count = 0;

  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    // Inferred classes are classes inferred from method samples.
    std::set<std::pair<const ProfileCompilationInfo*, dex::TypeIndex>> inferred_classes;
    for (size_t i = 0; i < dex_file->NumMethodIds(); ++i) {
      MethodReference ref(dex_file.get(), i);
      // This counter is how many profiles contain the method as sampled or hot.
      size_t counter = 0;
      for (const std::unique_ptr<const ProfileCompilationInfo>& profile : profiles) {
        Hotness hotness = profile->GetMethodHotness(ref);
        if (hotness.IsInProfile()) {
          ++counter;
          out_profile->AddMethodHotness(ref, hotness);
          inferred_classes.emplace(profile.get(), ref.GetMethodId().class_idx_);
        }
      }
      // If the counter is greater or equal to the compile threshold, mark the method as hot.
      // Note that all hot methods are also marked as hot in the out profile during the merging
      // process.
      if (counter >= options.compiled_method_threshold) {
        Hotness hotness;
        hotness.AddFlag(Hotness::kFlagHot);
        out_profile->AddMethodHotness(ref, hotness);
      }
    }
    // Walk all of the classes and add them to the profile if they meet the requirements.
    for (size_t i = 0; i < dex_file->NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
      TypeReference ref(dex_file.get(), class_def.class_idx_);
      bool is_clean = true;
      const uint8_t* class_data = dex_file->GetClassData(class_def);
      if (class_data != nullptr) {
        ClassDataItemIterator it(*dex_file, class_data);
        while (it.HasNextStaticField()) {
          const uint32_t flags = it.GetFieldAccessFlags();
          if ((flags & kAccFinal) == 0) {
            // Not final static field will probably dirty the class.
            is_clean = false;
            break;
          }
          it.Next();
        }
        it.SkipInstanceFields();
        while (it.HasNextMethod()) {
          const uint32_t flags = it.GetMethodAccessFlags();
          if ((flags & kAccNative) != 0) {
            // Native method will get dirtied.
            is_clean = false;
            break;
          }
          if ((flags & kAccConstructor) != 0 && (flags & kAccStatic) != 0) {
            // Class initializer, may get dirtied (not sure).
            is_clean = false;
            break;
          }
          it.Next();
        }
      }
      ++(is_clean ? clean_count : dirty_count);
      // This counter is how many profiles contain the class.
      size_t counter = 0;
      for (const std::unique_ptr<const ProfileCompilationInfo>& profile : profiles) {
        auto it = inferred_classes.find(std::make_pair(profile.get(), ref.TypeIndex()));
        if (it != inferred_classes.end() ||
            profile->ContainsClass(*ref.dex_file, ref.TypeIndex())) {
          ++counter;
        }
      }
      if (counter == 0) {
        continue;
      }
      if (counter >= options.image_class_theshold) {
        ++class_count;
        out_profile->AddClassForDex(ref);
      } else if (is_clean && counter >= options.image_class_clean_theshold) {
        ++clean_class_count;
        out_profile->AddClassForDex(ref);
      }
    }
  }
  if (verbose) {
    LOG(INFO) << "Image classes " << class_count + clean_class_count
              << " added because clean " << clean_class_count
              << " total clean " << clean_count << " total dirty " << dirty_count;
  }
}

}  // namespace art
