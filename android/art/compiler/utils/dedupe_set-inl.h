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

#ifndef ART_COMPILER_UTILS_DEDUPE_SET_INL_H_
#define ART_COMPILER_UTILS_DEDUPE_SET_INL_H_

#include "dedupe_set.h"

#include <inttypes.h>

#include <algorithm>
#include <unordered_map>

#include "android-base/stringprintf.h"

#include "base/hash_set.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "base/time_utils.h"

namespace art {

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
struct DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::Stats {
  size_t collision_sum = 0u;
  size_t collision_max = 0u;
  size_t total_probe_distance = 0u;
  size_t total_size = 0u;
};

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
class DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::Shard {
 public:
  Shard(const Alloc& alloc, const std::string& lock_name)
      : alloc_(alloc),
        lock_name_(lock_name),
        lock_(lock_name_.c_str()),
        keys_() {
  }

  ~Shard() {
    for (const HashedKey<StoreKey>& key : keys_) {
      DCHECK(key.Key() != nullptr);
      alloc_.Destroy(key.Key());
    }
  }

  const StoreKey* Add(Thread* self, size_t hash, const InKey& in_key) REQUIRES(!lock_) {
    MutexLock lock(self, lock_);
    HashedKey<InKey> hashed_in_key(hash, &in_key);
    auto it = keys_.Find(hashed_in_key);
    if (it != keys_.end()) {
      DCHECK(it->Key() != nullptr);
      return it->Key();
    }
    const StoreKey* store_key = alloc_.Copy(in_key);
    keys_.Insert(HashedKey<StoreKey> { hash, store_key });
    return store_key;
  }

  void UpdateStats(Thread* self, Stats* global_stats) REQUIRES(!lock_) {
    // HashSet<> doesn't keep entries ordered by hash, so we actually allocate memory
    // for bookkeeping while collecting the stats.
    std::unordered_map<HashType, size_t> stats;
    {
      MutexLock lock(self, lock_);
      // Note: The total_probe_distance will be updated with the current state.
      // It may have been higher before a re-hash.
      global_stats->total_probe_distance += keys_.TotalProbeDistance();
      global_stats->total_size += keys_.Size();
      for (const HashedKey<StoreKey>& key : keys_) {
        auto it = stats.find(key.Hash());
        if (it == stats.end()) {
          stats.insert({key.Hash(), 1u});
        } else {
          ++it->second;
        }
      }
    }
    for (const auto& entry : stats) {
      size_t number_of_entries = entry.second;
      if (number_of_entries > 1u) {
        global_stats->collision_sum += number_of_entries - 1u;
        global_stats->collision_max = std::max(global_stats->collision_max, number_of_entries);
      }
    }
  }

 private:
  template <typename T>
  class HashedKey {
   public:
    HashedKey() : hash_(0u), key_(nullptr) { }
    HashedKey(size_t hash, const T* key) : hash_(hash), key_(key) { }

    size_t Hash() const {
      return hash_;
    }

    const T* Key() const {
      return key_;
    }

    bool IsEmpty() const {
      return Key() == nullptr;
    }

    void MakeEmpty() {
      key_ = nullptr;
    }

   private:
    size_t hash_;
    const T* key_;
  };

  class ShardEmptyFn {
   public:
    bool IsEmpty(const HashedKey<StoreKey>& key) const {
      return key.IsEmpty();
    }

    void MakeEmpty(HashedKey<StoreKey>& key) {
      key.MakeEmpty();
    }
  };

  struct ShardHashFn {
    template <typename T>
    size_t operator()(const HashedKey<T>& key) const {
      return key.Hash();
    }
  };

  struct ShardPred {
    typename std::enable_if<!std::is_same<StoreKey, InKey>::value, bool>::type
    operator()(const HashedKey<StoreKey>& lhs, const HashedKey<StoreKey>& rhs) const {
      DCHECK(lhs.Key() != nullptr);
      DCHECK(rhs.Key() != nullptr);
      // Rehashing: stored keys are already deduplicated, so we can simply compare key pointers.
      return lhs.Key() == rhs.Key();
    }

    template <typename LeftT, typename RightT>
    bool operator()(const HashedKey<LeftT>& lhs, const HashedKey<RightT>& rhs) const {
      DCHECK(lhs.Key() != nullptr);
      DCHECK(rhs.Key() != nullptr);
      return lhs.Hash() == rhs.Hash() &&
          lhs.Key()->size() == rhs.Key()->size() &&
          std::equal(lhs.Key()->begin(), lhs.Key()->end(), rhs.Key()->begin());
    }
  };

  Alloc alloc_;
  const std::string lock_name_;
  Mutex lock_;
  HashSet<HashedKey<StoreKey>, ShardEmptyFn, ShardHashFn, ShardPred> keys_ GUARDED_BY(lock_);
};

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
const StoreKey* DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::Add(
    Thread* self, const InKey& key) {
  uint64_t hash_start;
  if (kIsDebugBuild) {
    hash_start = NanoTime();
  }
  HashType raw_hash = HashFunc()(key);
  if (kIsDebugBuild) {
    uint64_t hash_end = NanoTime();
    hash_time_ += hash_end - hash_start;
  }
  HashType shard_hash = raw_hash / kShard;
  HashType shard_bin = raw_hash % kShard;
  return shards_[shard_bin]->Add(self, shard_hash, key);
}

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::DedupeSet(const char* set_name,
                                                                         const Alloc& alloc)
    : hash_time_(0) {
  for (HashType i = 0; i < kShard; ++i) {
    std::ostringstream oss;
    oss << set_name << " lock " << i;
    shards_[i].reset(new Shard(alloc, oss.str()));
  }
}

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::~DedupeSet() {
  // Everything done by member destructors.
}

template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard>
std::string DedupeSet<InKey, StoreKey, Alloc, HashType, HashFunc, kShard>::DumpStats(
    Thread* self) const {
  Stats stats;
  for (HashType shard = 0; shard < kShard; ++shard) {
    shards_[shard]->UpdateStats(self, &stats);
  }
  return android::base::StringPrintf("%zu collisions, %zu max hash collisions, "
                                     "%zu/%zu probe distance, %" PRIu64 " ns hash time",
                                     stats.collision_sum,
                                     stats.collision_max,
                                     stats.total_probe_distance,
                                     stats.total_size,
                                     hash_time_);
}


}  // namespace art

#endif  // ART_COMPILER_UTILS_DEDUPE_SET_INL_H_
