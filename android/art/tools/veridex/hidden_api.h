/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_TOOLS_VERIDEX_HIDDEN_API_H_
#define ART_TOOLS_VERIDEX_HIDDEN_API_H_

#include "dex/hidden_api_access_flags.h"
#include "dex/method_reference.h"

#include <ostream>
#include <set>
#include <string>

namespace art {

class DexFile;

/**
 * Helper class for logging if a method/field is in a hidden API list.
 */
class HiddenApi {
 public:
  HiddenApi(const char* blacklist, const char* dark_greylist, const char* light_greylist) {
    FillList(light_greylist, light_greylist_);
    FillList(dark_greylist, dark_greylist_);
    FillList(blacklist, blacklist_);
  }

  HiddenApiAccessFlags::ApiList GetApiList(const std::string& name) const {
    if (IsInList(name, blacklist_)) {
      return HiddenApiAccessFlags::kBlacklist;
    } else if (IsInList(name, dark_greylist_)) {
      return HiddenApiAccessFlags::kDarkGreylist;
    } else if (IsInList(name, light_greylist_)) {
      return HiddenApiAccessFlags::kLightGreylist;
    } else {
      return HiddenApiAccessFlags::kWhitelist;
    }
  }

  bool IsInRestrictionList(const std::string& name) const {
    return GetApiList(name) != HiddenApiAccessFlags::kWhitelist;
  }

  static std::string GetApiMethodName(const DexFile& dex_file, uint32_t method_index);

  static std::string GetApiFieldName(const DexFile& dex_file, uint32_t field_index);

  static std::string GetApiMethodName(MethodReference ref) {
    return HiddenApi::GetApiMethodName(*ref.dex_file, ref.index);
  }

  static std::string ToInternalName(const std::string& str) {
    std::string val = str;
    std::replace(val.begin(), val.end(), '.', '/');
    return "L" + val + ";";
  }

 private:
  static bool IsInList(const std::string& name, const std::set<std::string>& list) {
    return list.find(name) != list.end();
  }

  static void FillList(const char* filename, std::set<std::string>& entries);

  std::set<std::string> blacklist_;
  std::set<std::string> light_greylist_;
  std::set<std::string> dark_greylist_;
};

struct HiddenApiStats {
  uint32_t count = 0;
  uint32_t reflection_count = 0;
  uint32_t linking_count = 0;
  uint32_t api_counts[4] = { 0, 0, 0, 0 };
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_HIDDEN_API_H_
