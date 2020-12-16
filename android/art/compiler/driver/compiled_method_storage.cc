/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <algorithm>
#include <ostream>

#include "compiled_method_storage.h"

#include <android-base/logging.h>

#include "base/utils.h"
#include "compiled_method.h"
#include "linker/linker_patch.h"
#include "thread-current-inl.h"
#include "utils/dedupe_set-inl.h"
#include "utils/swap_space.h"

namespace art {

namespace {  // anonymous namespace

template <typename T>
const LengthPrefixedArray<T>* CopyArray(SwapSpace* swap_space, const ArrayRef<const T>& array) {
  DCHECK(!array.empty());
  SwapAllocator<uint8_t> allocator(swap_space);
  void* storage = allocator.allocate(LengthPrefixedArray<T>::ComputeSize(array.size()));
  LengthPrefixedArray<T>* array_copy = new(storage) LengthPrefixedArray<T>(array.size());
  std::copy(array.begin(), array.end(), array_copy->begin());
  return array_copy;
}

template <typename T>
void ReleaseArray(SwapSpace* swap_space, const LengthPrefixedArray<T>* array) {
  SwapAllocator<uint8_t> allocator(swap_space);
  size_t size = LengthPrefixedArray<T>::ComputeSize(array->size());
  array->~LengthPrefixedArray<T>();
  allocator.deallocate(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(array)), size);
}

}  // anonymous namespace

template <typename T, typename DedupeSetType>
inline const LengthPrefixedArray<T>* CompiledMethodStorage::AllocateOrDeduplicateArray(
    const ArrayRef<const T>& data,
    DedupeSetType* dedupe_set) {
  if (data.empty()) {
    return nullptr;
  } else if (!DedupeEnabled()) {
    return CopyArray(swap_space_.get(), data);
  } else {
    return dedupe_set->Add(Thread::Current(), data);
  }
}

template <typename T>
inline void CompiledMethodStorage::ReleaseArrayIfNotDeduplicated(
    const LengthPrefixedArray<T>* array) {
  if (array != nullptr && !DedupeEnabled()) {
    ReleaseArray(swap_space_.get(), array);
  }
}

template <typename ContentType>
class CompiledMethodStorage::DedupeHashFunc {
 private:
  static constexpr bool kUseMurmur3Hash = true;

 public:
  size_t operator()(const ArrayRef<ContentType>& array) const {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
    // TODO: More reasonable assertion.
    // static_assert(IsPowerOfTwo(sizeof(ContentType)),
    //    "ContentType is not power of two, don't know whether array layout is as assumed");
    uint32_t len = sizeof(ContentType) * array.size();
    if (kUseMurmur3Hash) {
      static constexpr uint32_t c1 = 0xcc9e2d51;
      static constexpr uint32_t c2 = 0x1b873593;
      static constexpr uint32_t r1 = 15;
      static constexpr uint32_t r2 = 13;
      static constexpr uint32_t m = 5;
      static constexpr uint32_t n = 0xe6546b64;

      uint32_t hash = 0;

      const int nblocks = len / 4;
      typedef __attribute__((__aligned__(1))) uint32_t unaligned_uint32_t;
      const unaligned_uint32_t *blocks = reinterpret_cast<const uint32_t*>(data);
      int i;
      for (i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
      }

      const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);
      uint32_t k1 = 0;

      switch (len & 3) {
        case 3:
          k1 ^= tail[2] << 16;
          FALLTHROUGH_INTENDED;
        case 2:
          k1 ^= tail[1] << 8;
          FALLTHROUGH_INTENDED;
        case 1:
          k1 ^= tail[0];

          k1 *= c1;
          k1 = (k1 << r1) | (k1 >> (32 - r1));
          k1 *= c2;
          hash ^= k1;
      }

      hash ^= len;
      hash ^= (hash >> 16);
      hash *= 0x85ebca6b;
      hash ^= (hash >> 13);
      hash *= 0xc2b2ae35;
      hash ^= (hash >> 16);

      return hash;
    } else {
      return HashBytes(data, len);
    }
  }
};

template <typename T>
class CompiledMethodStorage::LengthPrefixedArrayAlloc {
 public:
  explicit LengthPrefixedArrayAlloc(SwapSpace* swap_space)
      : swap_space_(swap_space) {
  }

  const LengthPrefixedArray<T>* Copy(const ArrayRef<const T>& array) {
    return CopyArray(swap_space_, array);
  }

  void Destroy(const LengthPrefixedArray<T>* array) {
    ReleaseArray(swap_space_, array);
  }

 private:
  SwapSpace* const swap_space_;
};

CompiledMethodStorage::CompiledMethodStorage(int swap_fd)
    : swap_space_(swap_fd == -1 ? nullptr : new SwapSpace(swap_fd, 10 * MB)),
      dedupe_enabled_(true),
      dedupe_code_("dedupe code", LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_method_info_("dedupe method info",
                          LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_vmap_table_("dedupe vmap table",
                         LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_cfi_info_("dedupe cfi info", LengthPrefixedArrayAlloc<uint8_t>(swap_space_.get())),
      dedupe_linker_patches_("dedupe cfi info",
                             LengthPrefixedArrayAlloc<linker::LinkerPatch>(swap_space_.get())) {
}

CompiledMethodStorage::~CompiledMethodStorage() {
  // All done by member destructors.
}

void CompiledMethodStorage::DumpMemoryUsage(std::ostream& os, bool extended) const {
  if (swap_space_.get() != nullptr) {
    const size_t swap_size = swap_space_->GetSize();
    os << " swap=" << PrettySize(swap_size) << " (" << swap_size << "B)";
  }
  if (extended) {
    Thread* self = Thread::Current();
    os << "\nCode dedupe: " << dedupe_code_.DumpStats(self);
    os << "\nVmap table dedupe: " << dedupe_vmap_table_.DumpStats(self);
    os << "\nCFI info dedupe: " << dedupe_cfi_info_.DumpStats(self);
  }
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCode(
    const ArrayRef<const uint8_t>& code) {
  return AllocateOrDeduplicateArray(code, &dedupe_code_);
}

void CompiledMethodStorage::ReleaseCode(const LengthPrefixedArray<uint8_t>* code) {
  ReleaseArrayIfNotDeduplicated(code);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateMethodInfo(
    const ArrayRef<const uint8_t>& src_map) {
  return AllocateOrDeduplicateArray(src_map, &dedupe_method_info_);
}

void CompiledMethodStorage::ReleaseMethodInfo(const LengthPrefixedArray<uint8_t>* method_info) {
  ReleaseArrayIfNotDeduplicated(method_info);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateVMapTable(
    const ArrayRef<const uint8_t>& table) {
  return AllocateOrDeduplicateArray(table, &dedupe_vmap_table_);
}

void CompiledMethodStorage::ReleaseVMapTable(const LengthPrefixedArray<uint8_t>* table) {
  ReleaseArrayIfNotDeduplicated(table);
}

const LengthPrefixedArray<uint8_t>* CompiledMethodStorage::DeduplicateCFIInfo(
    const ArrayRef<const uint8_t>& cfi_info) {
  return AllocateOrDeduplicateArray(cfi_info, &dedupe_cfi_info_);
}

void CompiledMethodStorage::ReleaseCFIInfo(const LengthPrefixedArray<uint8_t>* cfi_info) {
  ReleaseArrayIfNotDeduplicated(cfi_info);
}

const LengthPrefixedArray<linker::LinkerPatch>* CompiledMethodStorage::DeduplicateLinkerPatches(
    const ArrayRef<const linker::LinkerPatch>& linker_patches) {
  return AllocateOrDeduplicateArray(linker_patches, &dedupe_linker_patches_);
}

void CompiledMethodStorage::ReleaseLinkerPatches(
    const LengthPrefixedArray<linker::LinkerPatch>* linker_patches) {
  ReleaseArrayIfNotDeduplicated(linker_patches);
}

}  // namespace art
