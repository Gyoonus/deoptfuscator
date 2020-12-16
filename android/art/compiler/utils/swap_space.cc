/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "swap_space.h"

#include <sys/mman.h>

#include <algorithm>
#include <numeric>

#include "base/bit_utils.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "thread-current-inl.h"

namespace art {

// The chunk size by which the swap file is increased and mapped.
static constexpr size_t kMininumMapSize = 16 * MB;

static constexpr bool kCheckFreeMaps = false;

template <typename FreeBySizeSet>
static void DumpFreeMap(const FreeBySizeSet& free_by_size) {
  size_t last_size = static_cast<size_t>(-1);
  for (const auto& entry : free_by_size) {
    if (last_size != entry.size) {
      last_size = entry.size;
      LOG(INFO) << "Size " << last_size;
    }
    LOG(INFO) << "  0x" << std::hex << entry.free_by_start_entry->Start()
        << " size=" << std::dec << entry.free_by_start_entry->size;
  }
}

void SwapSpace::RemoveChunk(FreeBySizeSet::const_iterator free_by_size_pos) {
  auto free_by_start_pos = free_by_size_pos->free_by_start_entry;
  free_by_size_.erase(free_by_size_pos);
  free_by_start_.erase(free_by_start_pos);
}

inline void SwapSpace::InsertChunk(const SpaceChunk& chunk) {
  DCHECK_NE(chunk.size, 0u);
  auto insert_result = free_by_start_.insert(chunk);
  DCHECK(insert_result.second);
  free_by_size_.emplace(chunk.size, insert_result.first);
}

SwapSpace::SwapSpace(int fd, size_t initial_size)
    : fd_(fd),
      size_(0),
      lock_("SwapSpace lock", static_cast<LockLevel>(LockLevel::kDefaultMutexLevel - 1)) {
  // Assume that the file is unlinked.

  InsertChunk(NewFileChunk(initial_size));
}

SwapSpace::~SwapSpace() {
  // Unmap all mmapped chunks. Nothing should be allocated anymore at
  // this point, so there should be only full size chunks in free_by_start_.
  for (const SpaceChunk& chunk : free_by_start_) {
    if (munmap(chunk.ptr, chunk.size) != 0) {
      PLOG(ERROR) << "Failed to unmap swap space chunk at "
          << static_cast<const void*>(chunk.ptr) << " size=" << chunk.size;
    }
  }
  // All arenas are backed by the same file. Just close the descriptor.
  close(fd_);
}

template <typename FreeByStartSet, typename FreeBySizeSet>
static size_t CollectFree(const FreeByStartSet& free_by_start, const FreeBySizeSet& free_by_size) {
  if (free_by_start.size() != free_by_size.size()) {
    LOG(FATAL) << "Size: " << free_by_start.size() << " vs " << free_by_size.size();
  }

  // Calculate over free_by_size.
  size_t sum1 = 0;
  for (const auto& entry : free_by_size) {
    sum1 += entry.free_by_start_entry->size;
  }

  // Calculate over free_by_start.
  size_t sum2 = 0;
  for (const auto& entry : free_by_start) {
    sum2 += entry.size;
  }

  if (sum1 != sum2) {
    LOG(FATAL) << "Sum: " << sum1 << " vs " << sum2;
  }
  return sum1;
}

void* SwapSpace::Alloc(size_t size) {
  MutexLock lock(Thread::Current(), lock_);
  size = RoundUp(size, 8U);

  // Check the free list for something that fits.
  // TODO: Smarter implementation. Global biggest chunk, ...
  auto it = free_by_start_.empty()
      ? free_by_size_.end()
      : free_by_size_.lower_bound(FreeBySizeEntry { size, free_by_start_.begin() });
  if (it != free_by_size_.end()) {
    auto entry = it->free_by_start_entry;
    SpaceChunk old_chunk = *entry;
    if (old_chunk.size == size) {
      RemoveChunk(it);
    } else {
      // Try to avoid deallocating and allocating the std::set<> nodes.
      // This would be much simpler if we could use replace() from Boost.Bimap.

      // The free_by_start_ map contains disjoint intervals ordered by the `ptr`.
      // Shrinking the interval does not affect the ordering.
      it->free_by_start_entry->ptr += size;
      it->free_by_start_entry->size -= size;

      // The free_by_size_ map is ordered by the `size` and then `free_by_start_entry->ptr`.
      // Adjusting the `ptr` above does not change that ordering but decreasing `size` can
      // push the node before the previous node(s).
      if (it == free_by_size_.begin()) {
        it->size -= size;
      } else {
        auto prev = it;
        --prev;
        FreeBySizeEntry new_value(old_chunk.size - size, entry);
        if (free_by_size_.key_comp()(*prev, new_value)) {
          it->size -= size;
        } else {
          // Changing in place would break the std::set<> ordering, we need to remove and insert.
          free_by_size_.erase(it);
          free_by_size_.insert(new_value);
        }
      }
    }
    return old_chunk.ptr;
  } else {
    // Not a big enough free chunk, need to increase file size.
    SpaceChunk new_chunk = NewFileChunk(size);
    if (new_chunk.size != size) {
      // Insert the remainder.
      SpaceChunk remainder = { new_chunk.ptr + size, new_chunk.size - size };
      InsertChunk(remainder);
    }
    return new_chunk.ptr;
  }
}

SwapSpace::SpaceChunk SwapSpace::NewFileChunk(size_t min_size) {
#if !defined(__APPLE__)
  size_t next_part = std::max(RoundUp(min_size, kPageSize), RoundUp(kMininumMapSize, kPageSize));
  int result = TEMP_FAILURE_RETRY(ftruncate64(fd_, size_ + next_part));
  if (result != 0) {
    PLOG(FATAL) << "Unable to increase swap file.";
  }
  uint8_t* ptr = reinterpret_cast<uint8_t*>(
      mmap(nullptr, next_part, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, size_));
  if (ptr == MAP_FAILED) {
    LOG(ERROR) << "Unable to mmap new swap file chunk.";
    LOG(ERROR) << "Current size: " << size_ << " requested: " << next_part << "/" << min_size;
    LOG(ERROR) << "Free list:";
    DumpFreeMap(free_by_size_);
    LOG(ERROR) << "In free list: " << CollectFree(free_by_start_, free_by_size_);
    LOG(FATAL) << "Aborting...";
  }
  size_ += next_part;
  SpaceChunk new_chunk = {ptr, next_part};
  return new_chunk;
#else
  UNUSED(min_size, kMininumMapSize);
  LOG(FATAL) << "No swap file support on the Mac.";
  UNREACHABLE();
#endif
}

// TODO: Full coalescing.
void SwapSpace::Free(void* ptr, size_t size) {
  MutexLock lock(Thread::Current(), lock_);
  size = RoundUp(size, 8U);

  size_t free_before = 0;
  if (kCheckFreeMaps) {
    free_before = CollectFree(free_by_start_, free_by_size_);
  }

  SpaceChunk chunk = { reinterpret_cast<uint8_t*>(ptr), size };
  auto it = free_by_start_.lower_bound(chunk);
  if (it != free_by_start_.begin()) {
    auto prev = it;
    --prev;
    CHECK_LE(prev->End(), chunk.Start());
    if (prev->End() == chunk.Start()) {
      // Merge *prev with this chunk.
      chunk.size += prev->size;
      chunk.ptr -= prev->size;
      auto erase_pos = free_by_size_.find(FreeBySizeEntry { prev->size, prev });
      DCHECK(erase_pos != free_by_size_.end());
      RemoveChunk(erase_pos);
      // "prev" is invalidated but "it" remains valid.
    }
  }
  if (it != free_by_start_.end()) {
    CHECK_LE(chunk.End(), it->Start());
    if (chunk.End() == it->Start()) {
      // Merge *it with this chunk.
      chunk.size += it->size;
      auto erase_pos = free_by_size_.find(FreeBySizeEntry { it->size, it });
      DCHECK(erase_pos != free_by_size_.end());
      RemoveChunk(erase_pos);
      // "it" is invalidated but we don't need it anymore.
    }
  }
  InsertChunk(chunk);

  if (kCheckFreeMaps) {
    size_t free_after = CollectFree(free_by_start_, free_by_size_);

    if (free_after != free_before + size) {
      DumpFreeMap(free_by_size_);
      CHECK_EQ(free_after, free_before + size) << "Should be " << size << " difference from " << free_before;
    }
  }
}

}  // namespace art
