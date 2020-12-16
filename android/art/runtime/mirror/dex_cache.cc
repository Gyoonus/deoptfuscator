/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "dex_cache-inl.h"

#include "art_method-inl.h"
#include "class_linker.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "globals.h"
#include "linear_alloc.h"
#include "oat_file.h"
#include "object-inl.h"
#include "object.h"
#include "object_array-inl.h"
#include "runtime.h"
#include "string.h"
#include "thread.h"
#include "utils/dex_cache_arrays_layout-inl.h"

namespace art {
namespace mirror {

void DexCache::InitializeDexCache(Thread* self,
                                  ObjPtr<mirror::DexCache> dex_cache,
                                  ObjPtr<mirror::String> location,
                                  const DexFile* dex_file,
                                  LinearAlloc* linear_alloc,
                                  PointerSize image_pointer_size) {
  DCHECK(dex_file != nullptr);
  ScopedAssertNoThreadSuspension sants(__FUNCTION__);
  DexCacheArraysLayout layout(image_pointer_size, dex_file);
  uint8_t* raw_arrays = nullptr;

  if (dex_file->NumStringIds() != 0u ||
      dex_file->NumTypeIds() != 0u ||
      dex_file->NumMethodIds() != 0u ||
      dex_file->NumFieldIds() != 0u) {
    static_assert(ArenaAllocator::kAlignment == 8, "Expecting arena alignment of 8.");
    DCHECK(layout.Alignment() == 8u || layout.Alignment() == 16u);
    // Zero-initialized.
    raw_arrays = (layout.Alignment() == 16u)
        ? reinterpret_cast<uint8_t*>(linear_alloc->AllocAlign16(self, layout.Size()))
        : reinterpret_cast<uint8_t*>(linear_alloc->Alloc(self, layout.Size()));
  }

  StringDexCacheType* strings = (dex_file->NumStringIds() == 0u) ? nullptr :
      reinterpret_cast<StringDexCacheType*>(raw_arrays + layout.StringsOffset());
  TypeDexCacheType* types = (dex_file->NumTypeIds() == 0u) ? nullptr :
      reinterpret_cast<TypeDexCacheType*>(raw_arrays + layout.TypesOffset());
  MethodDexCacheType* methods = (dex_file->NumMethodIds() == 0u) ? nullptr :
      reinterpret_cast<MethodDexCacheType*>(raw_arrays + layout.MethodsOffset());
  FieldDexCacheType* fields = (dex_file->NumFieldIds() == 0u) ? nullptr :
      reinterpret_cast<FieldDexCacheType*>(raw_arrays + layout.FieldsOffset());

  size_t num_strings = kDexCacheStringCacheSize;
  if (dex_file->NumStringIds() < num_strings) {
    num_strings = dex_file->NumStringIds();
  }
  size_t num_types = kDexCacheTypeCacheSize;
  if (dex_file->NumTypeIds() < num_types) {
    num_types = dex_file->NumTypeIds();
  }
  size_t num_fields = kDexCacheFieldCacheSize;
  if (dex_file->NumFieldIds() < num_fields) {
    num_fields = dex_file->NumFieldIds();
  }
  size_t num_methods = kDexCacheMethodCacheSize;
  if (dex_file->NumMethodIds() < num_methods) {
    num_methods = dex_file->NumMethodIds();
  }

  // Note that we allocate the method type dex caches regardless of this flag,
  // and we make sure here that they're not used by the runtime. This is in the
  // interest of simplicity and to avoid extensive compiler and layout class changes.
  //
  // If this needs to be mitigated in a production system running this code,
  // DexCache::kDexCacheMethodTypeCacheSize can be set to zero.
  MethodTypeDexCacheType* method_types = nullptr;
  size_t num_method_types = 0;

  if (dex_file->NumProtoIds() < kDexCacheMethodTypeCacheSize) {
    num_method_types = dex_file->NumProtoIds();
  } else {
    num_method_types = kDexCacheMethodTypeCacheSize;
  }

  if (num_method_types > 0) {
    method_types = reinterpret_cast<MethodTypeDexCacheType*>(
        raw_arrays + layout.MethodTypesOffset());
  }

  GcRoot<mirror::CallSite>* call_sites = (dex_file->NumCallSiteIds() == 0)
      ? nullptr
      : reinterpret_cast<GcRoot<CallSite>*>(raw_arrays + layout.CallSitesOffset());

  DCHECK_ALIGNED(raw_arrays, alignof(StringDexCacheType)) <<
                 "Expected raw_arrays to align to StringDexCacheType.";
  DCHECK_ALIGNED(layout.StringsOffset(), alignof(StringDexCacheType)) <<
                 "Expected StringsOffset() to align to StringDexCacheType.";
  DCHECK_ALIGNED(strings, alignof(StringDexCacheType)) <<
                 "Expected strings to align to StringDexCacheType.";
  static_assert(alignof(StringDexCacheType) == 8u,
                "Expected StringDexCacheType to have align of 8.");
  if (kIsDebugBuild) {
    // Sanity check to make sure all the dex cache arrays are empty. b/28992179
    for (size_t i = 0; i < num_strings; ++i) {
      CHECK_EQ(strings[i].load(std::memory_order_relaxed).index, 0u);
      CHECK(strings[i].load(std::memory_order_relaxed).object.IsNull());
    }
    for (size_t i = 0; i < num_types; ++i) {
      CHECK_EQ(types[i].load(std::memory_order_relaxed).index, 0u);
      CHECK(types[i].load(std::memory_order_relaxed).object.IsNull());
    }
    for (size_t i = 0; i < num_methods; ++i) {
      CHECK_EQ(GetNativePairPtrSize(methods, i, image_pointer_size).index, 0u);
      CHECK(GetNativePairPtrSize(methods, i, image_pointer_size).object == nullptr);
    }
    for (size_t i = 0; i < num_fields; ++i) {
      CHECK_EQ(GetNativePairPtrSize(fields, i, image_pointer_size).index, 0u);
      CHECK(GetNativePairPtrSize(fields, i, image_pointer_size).object == nullptr);
    }
    for (size_t i = 0; i < num_method_types; ++i) {
      CHECK_EQ(method_types[i].load(std::memory_order_relaxed).index, 0u);
      CHECK(method_types[i].load(std::memory_order_relaxed).object.IsNull());
    }
    for (size_t i = 0; i < dex_file->NumCallSiteIds(); ++i) {
      CHECK(call_sites[i].IsNull());
    }
  }
  if (strings != nullptr) {
    mirror::StringDexCachePair::Initialize(strings);
  }
  if (types != nullptr) {
    mirror::TypeDexCachePair::Initialize(types);
  }
  if (fields != nullptr) {
    mirror::FieldDexCachePair::Initialize(fields, image_pointer_size);
  }
  if (methods != nullptr) {
    mirror::MethodDexCachePair::Initialize(methods, image_pointer_size);
  }
  if (method_types != nullptr) {
    mirror::MethodTypeDexCachePair::Initialize(method_types);
  }
  dex_cache->Init(dex_file,
                  location,
                  strings,
                  num_strings,
                  types,
                  num_types,
                  methods,
                  num_methods,
                  fields,
                  num_fields,
                  method_types,
                  num_method_types,
                  call_sites,
                  dex_file->NumCallSiteIds());
}

void DexCache::Init(const DexFile* dex_file,
                    ObjPtr<String> location,
                    StringDexCacheType* strings,
                    uint32_t num_strings,
                    TypeDexCacheType* resolved_types,
                    uint32_t num_resolved_types,
                    MethodDexCacheType* resolved_methods,
                    uint32_t num_resolved_methods,
                    FieldDexCacheType* resolved_fields,
                    uint32_t num_resolved_fields,
                    MethodTypeDexCacheType* resolved_method_types,
                    uint32_t num_resolved_method_types,
                    GcRoot<CallSite>* resolved_call_sites,
                    uint32_t num_resolved_call_sites) {
  CHECK(dex_file != nullptr);
  CHECK(location != nullptr);
  CHECK_EQ(num_strings != 0u, strings != nullptr);
  CHECK_EQ(num_resolved_types != 0u, resolved_types != nullptr);
  CHECK_EQ(num_resolved_methods != 0u, resolved_methods != nullptr);
  CHECK_EQ(num_resolved_fields != 0u, resolved_fields != nullptr);
  CHECK_EQ(num_resolved_method_types != 0u, resolved_method_types != nullptr);
  CHECK_EQ(num_resolved_call_sites != 0u, resolved_call_sites != nullptr);

  SetDexFile(dex_file);
  SetLocation(location);
  SetStrings(strings);
  SetResolvedTypes(resolved_types);
  SetResolvedMethods(resolved_methods);
  SetResolvedFields(resolved_fields);
  SetResolvedMethodTypes(resolved_method_types);
  SetResolvedCallSites(resolved_call_sites);
  SetField32<false>(NumStringsOffset(), num_strings);
  SetField32<false>(NumResolvedTypesOffset(), num_resolved_types);
  SetField32<false>(NumResolvedMethodsOffset(), num_resolved_methods);
  SetField32<false>(NumResolvedFieldsOffset(), num_resolved_fields);
  SetField32<false>(NumResolvedMethodTypesOffset(), num_resolved_method_types);
  SetField32<false>(NumResolvedCallSitesOffset(), num_resolved_call_sites);
}

void DexCache::SetLocation(ObjPtr<mirror::String> location) {
  SetFieldObject<false>(OFFSET_OF_OBJECT_MEMBER(DexCache, location_), location);
}

#if !defined(__aarch64__) && !defined(__x86_64__) && !defined(__mips__)
static pthread_mutex_t dex_cache_slow_atomic_mutex = PTHREAD_MUTEX_INITIALIZER;

DexCache::ConversionPair64 DexCache::AtomicLoadRelaxed16B(std::atomic<ConversionPair64>* target) {
  pthread_mutex_lock(&dex_cache_slow_atomic_mutex);
  DexCache::ConversionPair64 value = *reinterpret_cast<ConversionPair64*>(target);
  pthread_mutex_unlock(&dex_cache_slow_atomic_mutex);
  return value;
}

void DexCache::AtomicStoreRelease16B(std::atomic<ConversionPair64>* target,
                                     ConversionPair64 value) {
  pthread_mutex_lock(&dex_cache_slow_atomic_mutex);
  *reinterpret_cast<ConversionPair64*>(target) = value;
  pthread_mutex_unlock(&dex_cache_slow_atomic_mutex);
}
#endif

}  // namespace mirror
}  // namespace art
