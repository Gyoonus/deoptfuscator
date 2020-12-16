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

#include "dex2oat_options.h"

#include <memory>

#include "cmdline_parser.h"
#include "driver/compiler_options_map-inl.h"

namespace art {

template<>
struct CmdlineType<InstructionSet> : CmdlineTypeParser<InstructionSet> {
  Result Parse(const std::string& option) {
    InstructionSet set = GetInstructionSetFromString(option.c_str());
    if (set == InstructionSet::kNone) {
      return Result::Failure(std::string("Not a valid instruction set: '") + option + "'");
    }
    return Result::Success(set);
  }

  static const char* Name() { return "InstructionSet"; }
};

#define COMPILER_OPTIONS_MAP_TYPE Dex2oatArgumentMap
#define COMPILER_OPTIONS_MAP_KEY_TYPE Dex2oatArgumentMapKey
#include "driver/compiler_options_map-storage.h"

// Specify storage for the Dex2oatOptions keys.

#define DEX2OAT_OPTIONS_KEY(Type, Name, ...) \
  const Dex2oatArgumentMap::Key<Type> Dex2oatArgumentMap::Name {__VA_ARGS__};
#include "dex2oat_options.def"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="

using M = Dex2oatArgumentMap;
using Parser = CmdlineParser<Dex2oatArgumentMap, Dex2oatArgumentMap::Key>;
using Builder = Parser::Builder;

static void AddInputMappings(Builder& builder) {
  builder.
      Define("--dex-file=_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::DexFiles)
      .Define("--dex-location=_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::DexLocations)
      .Define("--zip-fd=_")
          .WithType<int>()
          .IntoKey(M::ZipFd)
      .Define("--zip-location=_")
          .WithType<std::string>()
          .IntoKey(M::ZipLocation)
      .Define("--boot-image=_")
          .WithType<std::string>()
          .IntoKey(M::BootImage);
}

static void AddGeneratedArtifactMappings(Builder& builder) {
  builder.
      Define("--input-vdex-fd=_")
          .WithType<int>()
          .IntoKey(M::InputVdexFd)
      .Define("--input-vdex=_")
          .WithType<std::string>()
          .IntoKey(M::InputVdex)
      .Define("--output-vdex-fd=_")
          .WithType<int>()
          .IntoKey(M::OutputVdexFd)
      .Define("--output-vdex=_")
          .WithType<std::string>()
          .IntoKey(M::OutputVdex)
      .Define("--dm-fd=_")
          .WithType<int>()
          .IntoKey(M::DmFd)
      .Define("--dm-file=_")
          .WithType<std::string>()
          .IntoKey(M::DmFile)
      .Define("--oat-file=_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::OatFiles)
      .Define("--oat-symbols=_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::OatSymbols)
      .Define("--oat-fd=_")
          .WithType<int>()
          .IntoKey(M::OatFd)
      .Define("--oat-location=_")
          .WithType<std::string>()
          .IntoKey(M::OatLocation);
}

static void AddImageMappings(Builder& builder) {
  builder.
      Define("--image=_")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::ImageFilenames)
      .Define("--image-classes=_")
          .WithType<std::string>()
          .IntoKey(M::ImageClasses)
      .Define("--image-classes-zip=_")
          .WithType<std::string>()
          .IntoKey(M::ImageClassesZip)
      .Define("--base=_")
          .WithType<std::string>()
          .IntoKey(M::Base)
      .Define("--app-image-file=_")
          .WithType<std::string>()
          .IntoKey(M::AppImageFile)
      .Define("--app-image-fd=_")
          .WithType<int>()
          .IntoKey(M::AppImageFileFd)
      .Define("--multi-image")
          .IntoKey(M::MultiImage)
      .Define("--dirty-image-objects=_")
          .WithType<std::string>()
          .IntoKey(M::DirtyImageObjects)
      .Define("--image-format=_")
          .WithType<ImageHeader::StorageMode>()
          .WithValueMap({{"lz4", ImageHeader::kStorageModeLZ4},
                         {"lz4hc", ImageHeader::kStorageModeLZ4HC},
                         {"uncompressed", ImageHeader::kStorageModeUncompressed}})
          .IntoKey(M::ImageFormat);
}

static void AddSwapMappings(Builder& builder) {
  builder.
      Define("--swap-file=_")
          .WithType<std::string>()
          .IntoKey(M::SwapFile)
      .Define("--swap-fd=_")
          .WithType<int>()
          .IntoKey(M::SwapFileFd)
      .Define("--swap-dex-size-threshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::SwapDexSizeThreshold)
      .Define("--swap-dex-count-threshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::SwapDexCountThreshold);
}

static void AddCompilerMappings(Builder& builder) {
  builder.
      Define("--compiled-classes=_")
          .WithType<std::string>()
          .IntoKey(M::CompiledClasses)
      .Define("--compiled-classes-zip=_")
          .WithType<std::string>()
          .IntoKey(M::CompiledClassesZip)
      .Define("--compiled-methods=_")
          .WithType<std::string>()
          .IntoKey(M::CompiledMethods)
      .Define("--compiled-methods-zip=_")
          .WithType<std::string>()
          .IntoKey(M::CompiledMethodsZip)
      .Define("--run-passes=_")
          .WithType<std::string>()
          .IntoKey(M::Passes)
      .Define("--profile-file=_")
          .WithType<std::string>()
          .IntoKey(M::Profile)
      .Define("--profile-file-fd=_")
          .WithType<int>()
          .IntoKey(M::ProfileFd)
      .Define("--no-inline-from=_")
          .WithType<std::string>()
          .IntoKey(M::NoInlineFrom);
}

static void AddTargetMappings(Builder& builder) {
  builder.
      Define("--instruction-set=_")
          .WithType<InstructionSet>()
          .IntoKey(M::TargetInstructionSet)
      .Define("--instruction-set-variant=_")
          .WithType<std::string>()
          .IntoKey(M::TargetInstructionSetVariant)
      .Define("--instruction-set-features=_")
          .WithType<std::string>()
          .IntoKey(M::TargetInstructionSetFeatures);
}

static Parser CreateArgumentParser() {
  std::unique_ptr<Builder> parser_builder = std::unique_ptr<Builder>(new Builder());

  AddInputMappings(*parser_builder);
  AddGeneratedArtifactMappings(*parser_builder);
  AddImageMappings(*parser_builder);
  AddSwapMappings(*parser_builder);
  AddCompilerMappings(*parser_builder);
  AddTargetMappings(*parser_builder);

  parser_builder->
      Define({"--watch-dog", "--no-watch-dog"})
          .WithValues({true, false})
          .IntoKey(M::Watchdog)
      .Define("--watchdog-timeout=_")
          .WithType<int>()
          .IntoKey(M::WatchdogTimeout)
      .Define("-j_")
          .WithType<unsigned int>()
          .IntoKey(M::Threads)
      .Define("--android-root=_")
          .WithType<std::string>()
          .IntoKey(M::AndroidRoot)
      .Define("--compiler-backend=_")
          .WithType<Compiler::Kind>()
          .WithValueMap({{"Quick", Compiler::Kind::kQuick},
                         {"Optimizing", Compiler::Kind::kOptimizing}})
          .IntoKey(M::Backend)
      .Define("--host")
          .IntoKey(M::Host)
      .Define("--avoid-storing-invocation")
          .IntoKey(M::AvoidStoringInvocation)
      .Define("--very-large-app-threshold=_")
          .WithType<unsigned int>()
          .IntoKey(M::VeryLargeAppThreshold)
      .Define("--force-determinism")
          .IntoKey(M::ForceDeterminism)
      .Define("--copy-dex-files=_")
          .WithType<CopyOption>()
          .WithValueMap({{"true", CopyOption::kOnlyIfCompressed},
                         {"false", CopyOption::kNever},
                         {"always", CopyOption::kAlways}})
          .IntoKey(M::CopyDexFiles)
      .Define("--classpath-dir=_")
          .WithType<std::string>()
          .IntoKey(M::ClasspathDir)
      .Define("--class-loader-context=_")
          .WithType<std::string>()
          .IntoKey(M::ClassLoaderContext)
      .Define("--stored-class-loader-context=_")
          .WithType<std::string>()
          .IntoKey(M::StoredClassLoaderContext)
      .Define("--compact-dex-level=_")
          .WithType<CompactDexLevel>()
          .WithValueMap({{"none", CompactDexLevel::kCompactDexLevelNone},
                         {"fast", CompactDexLevel::kCompactDexLevelFast}})
          .IntoKey(M::CompactDexLevel)
      .Define("--runtime-arg _")
          .WithType<std::vector<std::string>>().AppendValues()
          .IntoKey(M::RuntimeOptions)
      .Define("--compilation-reason=_")
          .WithType<std::string>()
          .IntoKey(M::CompilationReason);

  AddCompilerOptionsArgumentParserOptions<Dex2oatArgumentMap>(*parser_builder);

  parser_builder->IgnoreUnrecognized(false);

  return parser_builder->Build();
}

std::unique_ptr<Dex2oatArgumentMap> Dex2oatArgumentMap::Parse(int argc,
                                                              const char** argv,
                                                              std::string* error_msg) {
  Parser parser = CreateArgumentParser();
  CmdlineResult parse_result = parser.Parse(argv, argc);
  if (!parse_result.IsSuccess()) {
    *error_msg = parse_result.GetMessage();
    return nullptr;
  }

  return std::unique_ptr<Dex2oatArgumentMap>(new Dex2oatArgumentMap(parser.ReleaseArgumentsMap()));
}

#pragma GCC diagnostic pop
}  // namespace art
