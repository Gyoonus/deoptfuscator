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

#ifndef ART_DEX2OAT_DEX2OAT_OPTIONS_H_
#define ART_DEX2OAT_DEX2OAT_OPTIONS_H_

#include <cstdio>
#include <string>
#include <vector>

#include "arch/instruction_set.h"
#include "base/variant_map.h"
#include "cmdline_types.h"  // TODO: don't need to include this file here
#include "compiler.h"
#include "dex/compact_dex_level.h"
#include "driver/compiler_options_map.h"
#include "image.h"

namespace art {

template <typename TVariantMap,
          template <typename TKeyValue> class TVariantMapKey>
struct CmdlineParser;

// Define a key that is usable with a Dex2oatArgumentMap.
// This key will *not* work with other subtypes of VariantMap.
template <typename TValue>
struct Dex2oatArgumentMapKey : VariantMapKey<TValue> {
  Dex2oatArgumentMapKey() {}
  explicit Dex2oatArgumentMapKey(TValue default_value)
      : VariantMapKey<TValue>(std::move(default_value)) {}
  // Don't ODR-use constexpr default values, which means that Struct::Fields
  // that are declared 'static constexpr T Name = Value' don't need to have a matching definition.
};

// Defines a type-safe heterogeneous key->value map.
// Use the VariantMap interface to look up or to store a Dex2oatArgumentMapKey,Value pair.
//
// Example:
//    auto map = Dex2oatArgumentMap();
//    map.Set(Dex2oatArgumentMap::ZipFd, -1);
//    int *target_utilization = map.Get(Dex2oatArgumentMap::ZipFd);
//
struct Dex2oatArgumentMap : CompilerOptionsMap<Dex2oatArgumentMap, Dex2oatArgumentMapKey> {
  // This 'using' line is necessary to inherit the variadic constructor.
  using CompilerOptionsMap<Dex2oatArgumentMap, Dex2oatArgumentMapKey>::CompilerOptionsMap;

  static std::unique_ptr<Dex2oatArgumentMap> Parse(int argc,
                                                   const char** argv,
                                                   std::string* error_msg);

  // Make the next many usages of Key slightly shorter to type.
  template <typename TValue>
  using Key = Dex2oatArgumentMapKey<TValue>;

  // List of key declarations, shorthand for 'static const Key<T> Name'
#define DEX2OAT_OPTIONS_KEY(Type, Name, ...) static const Key<Type> (Name);
#include "dex2oat_options.def"
};

extern template struct CompilerOptionsMap<Dex2oatArgumentMap, Dex2oatArgumentMapKey>;

}  // namespace art

#endif  // ART_DEX2OAT_DEX2OAT_OPTIONS_H_
