/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_DEDUPE_SET_H_
#define ART_COMPILER_UTILS_DEDUPE_SET_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/macros.h"

namespace art {

class Thread;

// A set of Keys that support a HashFunc returning HashType. Used to find duplicates of Key in the
// Add method. The data-structure is thread-safe through the use of internal locks, it also
// supports the lock being sharded.
template <typename InKey,
          typename StoreKey,
          typename Alloc,
          typename HashType,
          typename HashFunc,
          HashType kShard = 1>
class DedupeSet {
 public:
  // Add a new key to the dedupe set if not present. Return the equivalent deduplicated stored key.
  const StoreKey* Add(Thread* self, const InKey& key);

  DedupeSet(const char* set_name, const Alloc& alloc);

  ~DedupeSet();

  std::string DumpStats(Thread* self) const;

 private:
  struct Stats;
  class Shard;

  std::unique_ptr<Shard> shards_[kShard];
  uint64_t hash_time_;

  DISALLOW_COPY_AND_ASSIGN(DedupeSet);
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_DEDUPE_SET_H_
