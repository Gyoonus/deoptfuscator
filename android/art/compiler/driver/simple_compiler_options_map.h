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

// This file declares a completion of the CompilerOptionsMap and should be included into a
// .cc file, only.

#ifndef ART_COMPILER_DRIVER_SIMPLE_COMPILER_OPTIONS_MAP_H_
#define ART_COMPILER_DRIVER_SIMPLE_COMPILER_OPTIONS_MAP_H_

#include <memory>

#include "compiler_options_map-inl.h"
#include "base/variant_map.h"

namespace art {

template <typename TValue>
struct SimpleParseArgumentMapKey : VariantMapKey<TValue> {
  SimpleParseArgumentMapKey() {}
  explicit SimpleParseArgumentMapKey(TValue default_value)
      : VariantMapKey<TValue>(std::move(default_value)) {}
  // Don't ODR-use constexpr default values, which means that Struct::Fields
  // that are declared 'static constexpr T Name = Value' don't need to have a matching definition.
};

struct SimpleParseArgumentMap : CompilerOptionsMap<SimpleParseArgumentMap,
                                                   SimpleParseArgumentMapKey> {
  // This 'using' line is necessary to inherit the variadic constructor.
  using CompilerOptionsMap<SimpleParseArgumentMap, SimpleParseArgumentMapKey>::CompilerOptionsMap;
};

#define COMPILER_OPTIONS_MAP_TYPE SimpleParseArgumentMap
#define COMPILER_OPTIONS_MAP_KEY_TYPE SimpleParseArgumentMapKey
#include "compiler_options_map-storage.h"

using Parser = CmdlineParser<SimpleParseArgumentMap, SimpleParseArgumentMapKey>;

static inline Parser CreateSimpleParser(bool ignore_unrecognized) {
  std::unique_ptr<Parser::Builder> parser_builder =
      std::unique_ptr<Parser::Builder>(new Parser::Builder());

  AddCompilerOptionsArgumentParserOptions<SimpleParseArgumentMap>(*parser_builder);

  parser_builder->IgnoreUnrecognized(ignore_unrecognized);

  return parser_builder->Build();
}

}  // namespace art

#endif  // ART_COMPILER_DRIVER_SIMPLE_COMPILER_OPTIONS_MAP_H_
