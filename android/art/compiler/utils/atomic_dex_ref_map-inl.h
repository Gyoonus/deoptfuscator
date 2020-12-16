/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_INL_H_
#define ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_INL_H_

#include "atomic_dex_ref_map.h"

#include <type_traits>

#include "dex/class_reference.h"
#include "dex/dex_file-inl.h"
#include "dex/method_reference.h"
#include "dex/type_reference.h"

namespace art {

template <typename DexFileReferenceType, typename Value>
inline size_t AtomicDexRefMap<DexFileReferenceType, Value>::NumberOfDexIndices(
    const DexFile* dex_file) {
  // TODO: Use specialization for this? Not sure if worth it.
  static_assert(std::is_same<DexFileReferenceType, MethodReference>::value ||
                std::is_same<DexFileReferenceType, ClassReference>::value ||
                std::is_same<DexFileReferenceType, TypeReference>::value,
                "invalid index type");
  if (std::is_same<DexFileReferenceType, MethodReference>::value) {
    return dex_file->NumMethodIds();
  }
  if (std::is_same<DexFileReferenceType, ClassReference>::value) {
    return dex_file->NumClassDefs();
  }
  if (std::is_same<DexFileReferenceType, TypeReference>::value) {
    return dex_file->NumTypeIds();
  }
  UNREACHABLE();
}

template <typename DexFileReferenceType, typename Value>
inline typename AtomicDexRefMap<DexFileReferenceType, Value>::InsertResult
    AtomicDexRefMap<DexFileReferenceType, Value>::Insert(const DexFileReferenceType& ref,
                                                         const Value& expected,
                                                         const Value& desired) {
  ElementArray* const array = GetArray(ref.dex_file);
  if (array == nullptr) {
    return kInsertResultInvalidDexFile;
  }
  DCHECK_LT(ref.index, array->size());
  return (*array)[ref.index].CompareAndSetStrongSequentiallyConsistent(expected, desired)
      ? kInsertResultSuccess
      : kInsertResultCASFailure;
}

template <typename DexFileReferenceType, typename Value>
inline bool AtomicDexRefMap<DexFileReferenceType, Value>::Get(const DexFileReferenceType& ref,
                                                              Value* out) const {
  const ElementArray* const array = GetArray(ref.dex_file);
  if (array == nullptr) {
    return false;
  }
  *out = (*array)[ref.index].LoadRelaxed();
  return true;
}

template <typename DexFileReferenceType, typename Value>
inline bool AtomicDexRefMap<DexFileReferenceType, Value>::Remove(const DexFileReferenceType& ref,
                                                                 Value* out) {
  ElementArray* const array = GetArray(ref.dex_file);
  if (array == nullptr) {
    return false;
  }
  *out = (*array)[ref.index].ExchangeSequentiallyConsistent(nullptr);
  return true;
}

template <typename DexFileReferenceType, typename Value>
inline void AtomicDexRefMap<DexFileReferenceType, Value>::AddDexFile(const DexFile* dex_file) {
  arrays_.Put(dex_file, std::move(ElementArray(NumberOfDexIndices(dex_file))));
}

template <typename DexFileReferenceType, typename Value>
inline void AtomicDexRefMap<DexFileReferenceType, Value>::AddDexFiles(
    const std::vector<const DexFile*>& dex_files) {
  for (const DexFile* dex_file : dex_files) {
    if (!HaveDexFile(dex_file)) {
      AddDexFile(dex_file);
    }
  }
}

template <typename DexFileReferenceType, typename Value>
inline typename AtomicDexRefMap<DexFileReferenceType, Value>::ElementArray*
    AtomicDexRefMap<DexFileReferenceType, Value>::GetArray(const DexFile* dex_file) {
  auto it = arrays_.find(dex_file);
  return (it != arrays_.end()) ? &it->second : nullptr;
}

template <typename DexFileReferenceType, typename Value>
inline const typename AtomicDexRefMap<DexFileReferenceType, Value>::ElementArray*
    AtomicDexRefMap<DexFileReferenceType, Value>::GetArray(const DexFile* dex_file) const {
  auto it = arrays_.find(dex_file);
  return (it != arrays_.end()) ? &it->second : nullptr;
}

template <typename DexFileReferenceType, typename Value> template <typename Visitor>
inline void AtomicDexRefMap<DexFileReferenceType, Value>::Visit(const Visitor& visitor) {
  for (auto& pair : arrays_) {
    const DexFile* dex_file = pair.first;
    const ElementArray& elements = pair.second;
    for (size_t i = 0; i < elements.size(); ++i) {
      visitor(DexFileReference(dex_file, i), elements[i].LoadRelaxed());
    }
  }
}

template <typename DexFileReferenceType, typename Value>
inline void AtomicDexRefMap<DexFileReferenceType, Value>::ClearEntries() {
  for (auto& it : arrays_) {
    for (auto& element : it.second) {
      element.StoreRelaxed(nullptr);
    }
  }
}

}  // namespace art

#endif  // ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_INL_H_
