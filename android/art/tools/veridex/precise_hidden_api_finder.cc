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

#include "precise_hidden_api_finder.h"

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_file.h"
#include "dex/method_reference.h"
#include "flow_analysis.h"
#include "hidden_api.h"
#include "resolver.h"
#include "veridex.h"

#include <iostream>

namespace art {

void PreciseHiddenApiFinder::RunInternal(
    const std::vector<std::unique_ptr<VeridexResolver>>& resolvers,
    const std::function<void(VeridexResolver*, const ClassDataItemIterator&)>& action) {
  for (const std::unique_ptr<VeridexResolver>& resolver : resolvers) {
    const DexFile& dex_file = resolver->GetDexFile();
    size_t class_def_count = dex_file.NumClassDefs();
    for (size_t class_def_index = 0; class_def_index < class_def_count; ++class_def_index) {
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
      const uint8_t* class_data = dex_file.GetClassData(class_def);
      if (class_data == nullptr) {
        // Empty class.
        continue;
      }
      ClassDataItemIterator it(dex_file, class_data);
      it.SkipAllFields();
      for (; it.HasNextMethod(); it.Next()) {
        const DexFile::CodeItem* code_item = it.GetMethodCodeItem();
        if (code_item == nullptr) {
          continue;
        }
        action(resolver.get(), it);
      }
    }
  }
}

void PreciseHiddenApiFinder::AddUsesAt(const std::vector<ReflectAccessInfo>& accesses,
                                       MethodReference ref) {
  for (const ReflectAccessInfo& info : accesses) {
    if (info.IsConcrete()) {
      concrete_uses_[ref].push_back(info);
    } else {
      abstract_uses_[ref].push_back(info);
    }
  }
}

void PreciseHiddenApiFinder::Run(const std::vector<std::unique_ptr<VeridexResolver>>& resolvers) {
  // Collect reflection uses.
  RunInternal(resolvers, [this] (VeridexResolver* resolver, const ClassDataItemIterator& it) {
    FlowAnalysisCollector collector(resolver, it);
    collector.Run();
    AddUsesAt(collector.GetUses(), MethodReference(&resolver->GetDexFile(), it.GetMemberIndex()));
  });

  // For non-final reflection uses, do a limited fixed point calculation over the code to try
  // substituting them with final reflection uses.
  // We limit the number of times we iterate over the code as one run can be long.
  static const int kMaximumIterations = 10;
  uint32_t i = 0;
  while (!abstract_uses_.empty() && (i++ < kMaximumIterations)) {
    // Fetch and clear the worklist.
    std::map<MethodReference, std::vector<ReflectAccessInfo>> current_uses
        = std::move(abstract_uses_);
    RunInternal(resolvers,
                [this, current_uses] (VeridexResolver* resolver, const ClassDataItemIterator& it) {
      FlowAnalysisSubstitutor substitutor(resolver, it, current_uses);
      substitutor.Run();
      AddUsesAt(substitutor.GetUses(),
                MethodReference(&resolver->GetDexFile(), it.GetMemberIndex()));
    });
  }
}

void PreciseHiddenApiFinder::Dump(std::ostream& os, HiddenApiStats* stats) {
  static const char* kPrefix = "       ";
  std::map<std::string, std::vector<MethodReference>> named_uses;
  for (auto it : concrete_uses_) {
    MethodReference ref = it.first;
    for (const ReflectAccessInfo& info : it.second) {
      std::string cls(info.cls.ToString());
      std::string name(info.name.ToString());
      std::string full_name = cls + "->" + name;
      HiddenApiAccessFlags::ApiList api_list = hidden_api_.GetApiList(full_name);
      if (api_list != HiddenApiAccessFlags::kWhitelist) {
        named_uses[full_name].push_back(ref);
      }
    }
  }

  for (auto it : named_uses) {
    ++stats->reflection_count;
    const std::string& full_name = it.first;
    HiddenApiAccessFlags::ApiList api_list = hidden_api_.GetApiList(full_name);
    stats->api_counts[api_list]++;
    os << "#" << ++stats->count << ": Reflection " << api_list << " " << full_name << " use(s):";
    os << std::endl;
    for (const MethodReference& ref : it.second) {
      os << kPrefix << HiddenApi::GetApiMethodName(ref) << std::endl;
    }
    os << std::endl;
  }
}

}  // namespace art
