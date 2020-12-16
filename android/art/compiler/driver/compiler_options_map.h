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

#ifndef ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_H_
#define ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_H_

#include <string>
#include <vector>

#include "base/variant_map.h"
#include "cmdline_types.h"

namespace art {

// Defines a type-safe heterogeneous key->value map. This is to be used as the base for
// an extended map.
template <typename Base, template <typename TV> class KeyType>
struct CompilerOptionsMap : VariantMap<Base, KeyType> {
  // Make the next many usages of Key slightly shorter to type.
  template <typename TValue>
  using Key = KeyType<TValue>;

  // List of key declarations, shorthand for 'static const Key<T> Name'
#define COMPILER_OPTIONS_KEY(Type, Name, ...) static const Key<Type> (Name);
#include "compiler_options_map.def"
};

#undef DECLARE_KEY

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_H_
