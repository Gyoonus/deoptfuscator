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

#ifndef ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_INL_H_
#define ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_INL_H_

#include "compiler_options_map.h"

#include <memory>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android-base/stringprintf.h"

#include "base/macros.h"
#include "cmdline_parser.h"
#include "compiler_options.h"

namespace art {

template <class Base>
inline bool ReadCompilerOptions(Base& map, CompilerOptions* options, std::string* error_msg) {
  if (map.Exists(Base::CompilerFilter)) {
    CompilerFilter::Filter compiler_filter;
    if (!CompilerFilter::ParseCompilerFilter(map.Get(Base::CompilerFilter)->c_str(),
                                             &compiler_filter)) {
      *error_msg = android::base::StringPrintf("Unknown --compiler-filter value %s",
                                               map.Get(Base::CompilerFilter)->c_str());
      return false;
    }
    options->SetCompilerFilter(compiler_filter);
  }
  if (map.Exists(Base::PIC)) {
    options->compile_pic_ = true;
  }
  map.AssignIfExists(Base::HugeMethodMaxThreshold, &options->huge_method_threshold_);
  map.AssignIfExists(Base::LargeMethodMaxThreshold, &options->large_method_threshold_);
  map.AssignIfExists(Base::SmallMethodMaxThreshold, &options->small_method_threshold_);
  map.AssignIfExists(Base::TinyMethodMaxThreshold, &options->tiny_method_threshold_);
  map.AssignIfExists(Base::NumDexMethodsThreshold, &options->num_dex_methods_threshold_);
  map.AssignIfExists(Base::InlineMaxCodeUnitsThreshold, &options->inline_max_code_units_);
  map.AssignIfExists(Base::GenerateDebugInfo, &options->generate_debug_info_);
  map.AssignIfExists(Base::GenerateMiniDebugInfo, &options->generate_mini_debug_info_);
  map.AssignIfExists(Base::GenerateBuildID, &options->generate_build_id_);
  if (map.Exists(Base::Debuggable)) {
    options->debuggable_ = true;
  }
  map.AssignIfExists(Base::TopKProfileThreshold, &options->top_k_profile_threshold_);
  map.AssignIfExists(Base::AbortOnHardVerifierFailure, &options->abort_on_hard_verifier_failure_);
  map.AssignIfExists(Base::AbortOnSoftVerifierFailure, &options->abort_on_soft_verifier_failure_);
  if (map.Exists(Base::DumpInitFailures)) {
    if (!options->ParseDumpInitFailures(*map.Get(Base::DumpInitFailures), error_msg)) {
      return false;
    }
  }
  map.AssignIfExists(Base::DumpCFG, &options->dump_cfg_file_name_);
  if (map.Exists(Base::DumpCFGAppend)) {
    options->dump_cfg_append_ = true;
  }
  if (map.Exists(Base::RegisterAllocationStrategy)) {
    if (!options->ParseRegisterAllocationStrategy(*map.Get(Base::DumpInitFailures), error_msg)) {
      return false;
    }
  }
  map.AssignIfExists(Base::VerboseMethods, &options->verbose_methods_);
  options->deduplicate_code_ = map.GetOrDefault(Base::DeduplicateCode);
  if (map.Exists(Base::CountHotnessInCompiledCode)) {
    options->count_hotness_in_compiled_code_ = true;
  }

  if (map.Exists(Base::DumpTimings)) {
    options->dump_timings_ = true;
  }

  if (map.Exists(Base::DumpStats)) {
    options->dump_stats_ = true;
  }

  return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="

template <typename Map, typename Builder>
inline void AddCompilerOptionsArgumentParserOptions(Builder& b) {
  b.
      Define("--compiler-filter=_")
          .template WithType<std::string>()
          .IntoKey(Map::CompilerFilter)

      .Define("--compile-pic")
          .IntoKey(Map::PIC)

      .Define("--huge-method-max=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::HugeMethodMaxThreshold)
      .Define("--large-method-max=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::LargeMethodMaxThreshold)
      .Define("--small-method-max=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::SmallMethodMaxThreshold)
      .Define("--tiny-method-max=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::TinyMethodMaxThreshold)
      .Define("--num-dex-methods=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::NumDexMethodsThreshold)
      .Define("--inline-max-code-units=_")
          .template WithType<unsigned int>()
          .IntoKey(Map::InlineMaxCodeUnitsThreshold)

      .Define({"--generate-debug-info", "-g", "--no-generate-debug-info"})
          .WithValues({true, true, false})
          .IntoKey(Map::GenerateDebugInfo)
      .Define({"--generate-mini-debug-info", "--no-generate-mini-debug-info"})
          .WithValues({true, false})
          .IntoKey(Map::GenerateMiniDebugInfo)

      .Define({"--generate-build-id", "--no-generate-build-id"})
          .WithValues({true, false})
          .IntoKey(Map::GenerateBuildID)

      .Define({"--deduplicate-code=_"})
          .template WithType<bool>()
          .WithValueMap({{"false", false}, {"true", true}})
          .IntoKey(Map::DeduplicateCode)

      .Define({"--count-hotness-in-compiled-code"})
          .IntoKey(Map::CountHotnessInCompiledCode)

      .Define({"--dump-timings"})
          .IntoKey(Map::DumpTimings)

      .Define({"--dump-stats"})
          .IntoKey(Map::DumpStats)

      .Define("--debuggable")
          .IntoKey(Map::Debuggable)

      .Define("--top-k-profile-threshold=_")
          .template WithType<double>().WithRange(0.0, 100.0)
          .IntoKey(Map::TopKProfileThreshold)

      .Define({"--abort-on-hard-verifier-error", "--no-abort-on-hard-verifier-error"})
          .WithValues({true, false})
          .IntoKey(Map::AbortOnHardVerifierFailure)
      .Define({"--abort-on-soft-verifier-error", "--no-abort-on-soft-verifier-error"})
          .WithValues({true, false})
          .IntoKey(Map::AbortOnSoftVerifierFailure)

      .Define("--dump-init-failures=_")
          .template WithType<std::string>()
          .IntoKey(Map::DumpInitFailures)

      .Define("--dump-cfg=_")
          .template WithType<std::string>()
          .IntoKey(Map::DumpCFG)
      .Define("--dump-cfg-append")
          .IntoKey(Map::DumpCFGAppend)

      .Define("--register-allocation-strategy=_")
          .template WithType<std::string>()
          .IntoKey(Map::RegisterAllocationStrategy)

      .Define("--verbose-methods=_")
          .template WithType<ParseStringList<','>>()
          .IntoKey(Map::VerboseMethods);
}

#pragma GCC diagnostic pop

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_INL_H_
