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

#ifndef ART_TOOLS_VERIDEX_PRECISE_HIDDEN_API_FINDER_H_
#define ART_TOOLS_VERIDEX_PRECISE_HIDDEN_API_FINDER_H_

#include "dex/method_reference.h"
#include "flow_analysis.h"

#include <iostream>
#include <map>
#include <set>
#include <string>

namespace art {

class HiddenApi;
struct HiddenApiStats;
class VeridexResolver;

/**
 * Reports known uses of hidden APIs from reflection.
 */
class PreciseHiddenApiFinder {
 public:
  explicit PreciseHiddenApiFinder(const HiddenApi& hidden_api) : hidden_api_(hidden_api) {}

  // Iterate over the dex files associated with the passed resolvers to report
  // hidden API uses.
  void Run(const std::vector<std::unique_ptr<VeridexResolver>>& app_resolvers);

  void Dump(std::ostream& os, HiddenApiStats* stats);

 private:
  // Run over all methods of all dex files, and call `action` on each.
  void RunInternal(
      const std::vector<std::unique_ptr<VeridexResolver>>& resolvers,
      const std::function<void(VeridexResolver*, const ClassDataItemIterator&)>& action);

  // Add uses found in method `ref`.
  void AddUsesAt(const std::vector<ReflectAccessInfo>& accesses, MethodReference ref);

  const HiddenApi& hidden_api_;

  std::map<MethodReference, std::vector<ReflectAccessInfo>> concrete_uses_;
  std::map<MethodReference, std::vector<ReflectAccessInfo>> abstract_uses_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_PRECISE_HIDDEN_API_FINDER_H_
