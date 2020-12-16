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

#ifndef ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_H_
#define ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_H_

#include "base/atomic.h"
#include "base/dchecked_vector.h"
#include "base/safe_map.h"
#include "dex/dex_file_reference.h"

namespace art {

class DexFile;

// Used by CompilerCallbacks to track verification information from the Runtime.
template <typename DexFileReferenceType, typename Value>
class AtomicDexRefMap {
 public:
  AtomicDexRefMap() {}
  ~AtomicDexRefMap() {}

  // Atomically swap the element in if the existing value matches expected.
  enum InsertResult {
    kInsertResultInvalidDexFile,
    kInsertResultCASFailure,
    kInsertResultSuccess,
  };
  InsertResult Insert(const DexFileReferenceType& ref,
                      const Value& expected,
                      const Value& desired);

  // Retreive an item, returns false if the dex file is not added.
  bool Get(const DexFileReferenceType& ref, Value* out) const;

  // Remove an item and return the existing value. Returns false if the dex file is not added.
  bool Remove(const DexFileReferenceType& ref, Value* out);

  // Dex files must be added before method references belonging to them can be used as keys. Not
  // thread safe.
  void AddDexFile(const DexFile* dex_file);
  void AddDexFiles(const std::vector<const DexFile*>& dex_files);

  bool HaveDexFile(const DexFile* dex_file) const {
    return arrays_.find(dex_file) != arrays_.end();
  }

  // Visit all of the dex files and elements.
  template <typename Visitor>
  void Visit(const Visitor& visitor);

  void ClearEntries();

 private:
  // Verified methods. The method array is fixed to avoid needing a lock to extend it.
  using ElementArray = dchecked_vector<Atomic<Value>>;
  using DexFileArrays = SafeMap<const DexFile*, ElementArray>;

  const ElementArray* GetArray(const DexFile* dex_file) const;
  ElementArray* GetArray(const DexFile* dex_file);

  static size_t NumberOfDexIndices(const DexFile* dex_file);

  DexFileArrays arrays_;
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_ATOMIC_DEX_REF_MAP_H_
