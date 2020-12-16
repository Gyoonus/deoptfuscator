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

#ifndef ART_COMPILER_COMPILED_METHOD_INL_H_
#define ART_COMPILER_COMPILED_METHOD_INL_H_

#include "compiled_method.h"

#include "base/array_ref.h"
#include "base/length_prefixed_array.h"
#include "linker/linker_patch.h"

namespace art {

inline ArrayRef<const uint8_t> CompiledCode::GetQuickCode() const {
  return GetArray(quick_code_);
}

template <typename T>
inline ArrayRef<const T> CompiledCode::GetArray(const LengthPrefixedArray<T>* array) {
  if (array == nullptr) {
    return ArrayRef<const T>();
  }
  DCHECK_NE(array->size(), 0u);
  return ArrayRef<const T>(&array->At(0), array->size());
}

inline ArrayRef<const uint8_t> CompiledMethod::GetMethodInfo() const {
  return GetArray(method_info_);
}

inline ArrayRef<const uint8_t> CompiledMethod::GetVmapTable() const {
  return GetArray(vmap_table_);
}

inline ArrayRef<const uint8_t> CompiledMethod::GetCFIInfo() const {
  return GetArray(cfi_info_);
}

inline ArrayRef<const linker::LinkerPatch> CompiledMethod::GetPatches() const {
  return GetArray(patches_);
}

}  // namespace art

#endif  // ART_COMPILER_COMPILED_METHOD_INL_H_
