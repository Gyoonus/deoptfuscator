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

#ifndef ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_STORAGE_H_
#define ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_STORAGE_H_

// Assumes:
// * #include "compiler_options_map.h"
// * namespace art
//
// Usage:
// #define COMPILER_OPTIONS_MAP_TYPE TheTypeOfTheMap
// #define COMPILER_OPTIONS_MAP_KEY_TYPE TheTypeOfTheMapsKey
// #include "driver/compiler_options_map-storage.h

#ifndef COMPILER_OPTIONS_MAP_TYPE
#error "Expected COMPILER_OPTIONS_MAP_TYPE"
#endif

#ifndef COMPILER_OPTIONS_MAP_KEY_TYPE
#error "Expected COMPILER_OPTIONS_MAP_KEY_TYPE"
#endif

#define COMPILER_OPTIONS_KEY(Type, Name, ...) \
  template <typename Base, template <typename TV> class KeyType> \
  const KeyType<Type> CompilerOptionsMap<Base, KeyType>::Name {__VA_ARGS__};
#include <driver/compiler_options_map.def>

template struct CompilerOptionsMap<COMPILER_OPTIONS_MAP_TYPE, COMPILER_OPTIONS_MAP_KEY_TYPE>;

#undef COMPILER_OPTIONS_MAP_TYPE
#undef COMPILER_OPTIONS_MAP_KEY_TYPE

#endif  // ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_STORAGE_H_
#undef ART_COMPILER_DRIVER_COMPILER_OPTIONS_MAP_STORAGE_H_  // Guard is only for cpplint
