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

#include "arena_allocator-inl.h"

#include <sys/mman.h>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <numeric>

#include <android-base/logging.h>

#include "base/systrace.h"
#include "mem_map.h"
#include "mutex.h"
#include "thread-current-inl.h"

namespace art {

constexpr size_t kMemoryToolRedZoneBytes = 8;

template <bool kCount>
const char* const ArenaAllocatorStatsImpl<kCount>::kAllocNames[] = {
  // Every name should have the same width and end with a space. Abbreviate if necessary:
  "Misc         ",
  "SwitchTbl    ",
  "SlowPaths    ",
  "GrowBitMap   ",
  "STL          ",
  "GraphBuilder ",
  "Graph        ",
  "BasicBlock   ",
  "BlockList    ",
  "RevPostOrder ",
  "LinearOrder  ",
  "ConstantsMap ",
  "Predecessors ",
  "Successors   ",
  "Dominated    ",
  "Instruction  ",
  "CtorFenceIns ",
  "InvokeInputs ",
  "PhiInputs    ",
  "LoopInfo     ",
  "LIBackEdges  ",
  "TryCatchInf  ",
  "UseListNode  ",
  "Environment  ",
  "EnvVRegs     ",
  "EnvLocations ",
  "LocSummary   ",
  "SsaBuilder   ",
  "MoveOperands ",
  "CodeBuffer   ",
  "StackMaps    ",
  "Optimization ",
  "GVN          ",
  "InductionVar ",
  "BCE          ",
  "DCE          ",
  "LSA          ",
  "LSE          ",
  "CFRE         ",
  "LICM         ",
  "LoopOpt      ",
  "SsaLiveness  ",
  "SsaPhiElim   ",
  "RefTypeProp  ",
  "SideEffects  ",
  "RegAllocator ",
  "RegAllocVldt ",
  "StackMapStm  ",
  "VectorNode   ",
  "CodeGen      ",
  "Assembler    ",
  "ParallelMove ",
  "GraphChecker ",
  "Verifier     ",
  "CallingConv  ",
  "CHA          ",
  "Scheduler    ",
  "Profile      ",
  "SBCloner     ",
};

template <bool kCount>
ArenaAllocatorStatsImpl<kCount>::ArenaAllocatorStatsImpl()
    : num_allocations_(0u),
      alloc_stats_(kNumArenaAllocKinds, 0u) {
}

template <bool kCount>
void ArenaAllocatorStatsImpl<kCount>::Copy(const ArenaAllocatorStatsImpl& other) {
  num_allocations_ = other.num_allocations_;
  std::copy_n(other.alloc_stats_.begin(), kNumArenaAllocKinds, alloc_stats_.begin());
}

template <bool kCount>
void ArenaAllocatorStatsImpl<kCount>::RecordAlloc(size_t bytes, ArenaAllocKind kind) {
  alloc_stats_[kind] += bytes;
  ++num_allocations_;
}

template <bool kCount>
size_t ArenaAllocatorStatsImpl<kCount>::NumAllocations() const {
  return num_allocations_;
}

template <bool kCount>
size_t ArenaAllocatorStatsImpl<kCount>::BytesAllocated() const {
  const size_t init = 0u;  // Initial value of the correct type.
  return std::accumulate(alloc_stats_.begin(), alloc_stats_.end(), init);
}

template <bool kCount>
void ArenaAllocatorStatsImpl<kCount>::Dump(std::ostream& os, const Arena* first,
                                           ssize_t lost_bytes_adjustment) const {
  size_t malloc_bytes = 0u;
  size_t lost_bytes = 0u;
  size_t num_arenas = 0u;
  for (const Arena* arena = first; arena != nullptr; arena = arena->next_) {
    malloc_bytes += arena->Size();
    lost_bytes += arena->RemainingSpace();
    ++num_arenas;
  }
  // The lost_bytes_adjustment is used to make up for the fact that the current arena
  // may not have the bytes_allocated_ updated correctly.
  lost_bytes += lost_bytes_adjustment;
  const size_t bytes_allocated = BytesAllocated();
  os << " MEM: used: " << bytes_allocated << ", allocated: " << malloc_bytes
     << ", lost: " << lost_bytes << "\n";
  size_t num_allocations = NumAllocations();
  if (num_allocations != 0) {
    os << "Number of arenas allocated: " << num_arenas << ", Number of allocations: "
       << num_allocations << ", avg size: " << bytes_allocated / num_allocations << "\n";
  }
  os << "===== Allocation by kind\n";
  static_assert(arraysize(kAllocNames) == kNumArenaAllocKinds, "arraysize of kAllocNames");
  for (int i = 0; i < kNumArenaAllocKinds; i++) {
    // Reduce output by listing only allocation kinds that actually have allocations.
    if (alloc_stats_[i] != 0u) {
      os << kAllocNames[i] << std::setw(10) << alloc_stats_[i] << "\n";
    }
  }
}

#pragma GCC diagnostic push
#if __clang_major__ >= 4
#pragma GCC diagnostic ignored "-Winstantiation-after-specialization"
#endif
// We're going to use ArenaAllocatorStatsImpl<kArenaAllocatorCountAllocations> which needs
// to be explicitly instantiated if kArenaAllocatorCountAllocations is true. Explicit
// instantiation of the specialization ArenaAllocatorStatsImpl<false> does not do anything
// but requires the warning "-Winstantiation-after-specialization" to be turned off.
//
// To avoid bit-rot of the ArenaAllocatorStatsImpl<true>, instantiate it also in debug builds
// (but keep the unnecessary code out of release builds) as we do not usually compile with
// kArenaAllocatorCountAllocations set to true.
template class ArenaAllocatorStatsImpl<kArenaAllocatorCountAllocations || kIsDebugBuild>;
#pragma GCC diagnostic pop

void ArenaAllocatorMemoryTool::DoMakeDefined(void* ptr, size_t size) {
  MEMORY_TOOL_MAKE_DEFINED(ptr, size);
}

void ArenaAllocatorMemoryTool::DoMakeUndefined(void* ptr, size_t size) {
  MEMORY_TOOL_MAKE_UNDEFINED(ptr, size);
}

void ArenaAllocatorMemoryTool::DoMakeInaccessible(void* ptr, size_t size) {
  MEMORY_TOOL_MAKE_NOACCESS(ptr, size);
}

Arena::Arena() : bytes_allocated_(0), memory_(nullptr), size_(0), next_(nullptr) {
}

class MallocArena FINAL : public Arena {
 public:
  explicit MallocArena(size_t size = arena_allocator::kArenaDefaultSize);
  virtual ~MallocArena();
 private:
  static constexpr size_t RequiredOverallocation() {
    return (alignof(std::max_align_t) < ArenaAllocator::kArenaAlignment)
        ? ArenaAllocator::kArenaAlignment - alignof(std::max_align_t)
        : 0u;
  }

  uint8_t* unaligned_memory_;
};

MallocArena::MallocArena(size_t size) {
  // We need to guarantee kArenaAlignment aligned allocation for the new arena.
  // TODO: Use std::aligned_alloc() when it becomes available with C++17.
  constexpr size_t overallocation = RequiredOverallocation();
  unaligned_memory_ = reinterpret_cast<uint8_t*>(calloc(1, size + overallocation));
  CHECK(unaligned_memory_ != nullptr);  // Abort on OOM.
  DCHECK_ALIGNED(unaligned_memory_, alignof(std::max_align_t));
  if (overallocation == 0u) {
    memory_ = unaligned_memory_;
  } else {
    memory_ = AlignUp(unaligned_memory_, ArenaAllocator::kArenaAlignment);
    if (UNLIKELY(RUNNING_ON_MEMORY_TOOL > 0)) {
      size_t head = memory_ - unaligned_memory_;
      size_t tail = overallocation - head;
      MEMORY_TOOL_MAKE_NOACCESS(unaligned_memory_, head);
      MEMORY_TOOL_MAKE_NOACCESS(memory_ + size, tail);
    }
  }
  DCHECK_ALIGNED(memory_, ArenaAllocator::kArenaAlignment);
  size_ = size;
}

MallocArena::~MallocArena() {
  constexpr size_t overallocation = RequiredOverallocation();
  if (overallocation != 0u && UNLIKELY(RUNNING_ON_MEMORY_TOOL > 0)) {
    size_t head = memory_ - unaligned_memory_;
    size_t tail = overallocation - head;
    MEMORY_TOOL_MAKE_UNDEFINED(unaligned_memory_, head);
    MEMORY_TOOL_MAKE_UNDEFINED(memory_ + size_, tail);
  }
  free(reinterpret_cast<void*>(unaligned_memory_));
}

class MemMapArena FINAL : public Arena {
 public:
  MemMapArena(size_t size, bool low_4gb, const char* name);
  virtual ~MemMapArena();
  void Release() OVERRIDE;

 private:
  std::unique_ptr<MemMap> map_;
};

MemMapArena::MemMapArena(size_t size, bool low_4gb, const char* name) {
  // Round up to a full page as that's the smallest unit of allocation for mmap()
  // and we want to be able to use all memory that we actually allocate.
  size = RoundUp(size, kPageSize);
  std::string error_msg;
  map_.reset(MemMap::MapAnonymous(
      name, nullptr, size, PROT_READ | PROT_WRITE, low_4gb, false, &error_msg));
  CHECK(map_.get() != nullptr) << error_msg;
  memory_ = map_->Begin();
  static_assert(ArenaAllocator::kArenaAlignment <= kPageSize,
                "Arena should not need stronger alignment than kPageSize.");
  DCHECK_ALIGNED(memory_, ArenaAllocator::kArenaAlignment);
  size_ = map_->Size();
}

MemMapArena::~MemMapArena() {
  // Destroys MemMap via std::unique_ptr<>.
}

void MemMapArena::Release() {
  if (bytes_allocated_ > 0) {
    map_->MadviseDontNeedAndZero();
    bytes_allocated_ = 0;
  }
}

void Arena::Reset() {
  if (bytes_allocated_ > 0) {
    memset(Begin(), 0, bytes_allocated_);
    bytes_allocated_ = 0;
  }
}

ArenaPool::ArenaPool(bool use_malloc, bool low_4gb, const char* name)
    : use_malloc_(use_malloc),
      lock_("Arena pool lock", kArenaPoolLock),
      free_arenas_(nullptr),
      low_4gb_(low_4gb),
      name_(name) {
  if (low_4gb) {
    CHECK(!use_malloc) << "low4gb must use map implementation";
  }
  if (!use_malloc) {
    MemMap::Init();
  }
}

ArenaPool::~ArenaPool() {
  ReclaimMemory();
}

void ArenaPool::ReclaimMemory() {
  while (free_arenas_ != nullptr) {
    Arena* arena = free_arenas_;
    free_arenas_ = free_arenas_->next_;
    delete arena;
  }
}

void ArenaPool::LockReclaimMemory() {
  MutexLock lock(Thread::Current(), lock_);
  ReclaimMemory();
}

Arena* ArenaPool::AllocArena(size_t size) {
  Thread* self = Thread::Current();
  Arena* ret = nullptr;
  {
    MutexLock lock(self, lock_);
    if (free_arenas_ != nullptr && LIKELY(free_arenas_->Size() >= size)) {
      ret = free_arenas_;
      free_arenas_ = free_arenas_->next_;
    }
  }
  if (ret == nullptr) {
    ret = use_malloc_ ? static_cast<Arena*>(new MallocArena(size)) :
        new MemMapArena(size, low_4gb_, name_);
  }
  ret->Reset();
  return ret;
}

void ArenaPool::TrimMaps() {
  if (!use_malloc_) {
    ScopedTrace trace(__PRETTY_FUNCTION__);
    // Doesn't work for malloc.
    MutexLock lock(Thread::Current(), lock_);
    for (Arena* arena = free_arenas_; arena != nullptr; arena = arena->next_) {
      arena->Release();
    }
  }
}

size_t ArenaPool::GetBytesAllocated() const {
  size_t total = 0;
  MutexLock lock(Thread::Current(), lock_);
  for (Arena* arena = free_arenas_; arena != nullptr; arena = arena->next_) {
    total += arena->GetBytesAllocated();
  }
  return total;
}

void ArenaPool::FreeArenaChain(Arena* first) {
  if (UNLIKELY(RUNNING_ON_MEMORY_TOOL > 0)) {
    for (Arena* arena = first; arena != nullptr; arena = arena->next_) {
      MEMORY_TOOL_MAKE_UNDEFINED(arena->memory_, arena->bytes_allocated_);
    }
  }

  if (arena_allocator::kArenaAllocatorPreciseTracking) {
    // Do not reuse arenas when tracking.
    while (first != nullptr) {
      Arena* next = first->next_;
      delete first;
      first = next;
    }
    return;
  }

  if (first != nullptr) {
    Arena* last = first;
    while (last->next_ != nullptr) {
      last = last->next_;
    }
    Thread* self = Thread::Current();
    MutexLock lock(self, lock_);
    last->next_ = free_arenas_;
    free_arenas_ = first;
  }
}

size_t ArenaAllocator::BytesAllocated() const {
  return ArenaAllocatorStats::BytesAllocated();
}

size_t ArenaAllocator::BytesUsed() const {
  size_t total = ptr_ - begin_;
  if (arena_head_ != nullptr) {
    for (Arena* cur_arena = arena_head_->next_; cur_arena != nullptr;
         cur_arena = cur_arena->next_) {
     total += cur_arena->GetBytesAllocated();
    }
  }
  return total;
}

ArenaAllocator::ArenaAllocator(ArenaPool* pool)
  : pool_(pool),
    begin_(nullptr),
    end_(nullptr),
    ptr_(nullptr),
    arena_head_(nullptr) {
}

void ArenaAllocator::UpdateBytesAllocated() {
  if (arena_head_ != nullptr) {
    // Update how many bytes we have allocated into the arena so that the arena pool knows how
    // much memory to zero out.
    arena_head_->bytes_allocated_ = ptr_ - begin_;
  }
}

void* ArenaAllocator::AllocWithMemoryTool(size_t bytes, ArenaAllocKind kind) {
  // We mark all memory for a newly retrieved arena as inaccessible and then
  // mark only the actually allocated memory as defined. That leaves red zones
  // and padding between allocations marked as inaccessible.
  size_t rounded_bytes = RoundUp(bytes + kMemoryToolRedZoneBytes, 8);
  ArenaAllocatorStats::RecordAlloc(rounded_bytes, kind);
  uint8_t* ret;
  if (UNLIKELY(rounded_bytes > static_cast<size_t>(end_ - ptr_))) {
    ret = AllocFromNewArenaWithMemoryTool(rounded_bytes);
  } else {
    ret = ptr_;
    ptr_ += rounded_bytes;
  }
  MEMORY_TOOL_MAKE_DEFINED(ret, bytes);
  // Check that the memory is already zeroed out.
  DCHECK(std::all_of(ret, ret + bytes, [](uint8_t val) { return val == 0u; }));
  return ret;
}

void* ArenaAllocator::AllocWithMemoryToolAlign16(size_t bytes, ArenaAllocKind kind) {
  // We mark all memory for a newly retrieved arena as inaccessible and then
  // mark only the actually allocated memory as defined. That leaves red zones
  // and padding between allocations marked as inaccessible.
  size_t rounded_bytes = bytes + kMemoryToolRedZoneBytes;
  DCHECK_ALIGNED(rounded_bytes, 8);  // `bytes` is 16-byte aligned, red zone is 8-byte aligned.
  uintptr_t padding =
      ((reinterpret_cast<uintptr_t>(ptr_) + 15u) & 15u) - reinterpret_cast<uintptr_t>(ptr_);
  ArenaAllocatorStats::RecordAlloc(rounded_bytes, kind);
  uint8_t* ret;
  if (UNLIKELY(padding + rounded_bytes > static_cast<size_t>(end_ - ptr_))) {
    static_assert(kArenaAlignment >= 16, "Expecting sufficient alignment for new Arena.");
    ret = AllocFromNewArenaWithMemoryTool(rounded_bytes);
  } else {
    ptr_ += padding;  // Leave padding inaccessible.
    ret = ptr_;
    ptr_ += rounded_bytes;
  }
  MEMORY_TOOL_MAKE_DEFINED(ret, bytes);
  // Check that the memory is already zeroed out.
  DCHECK(std::all_of(ret, ret + bytes, [](uint8_t val) { return val == 0u; }));
  return ret;
}

ArenaAllocator::~ArenaAllocator() {
  // Reclaim all the arenas by giving them back to the thread pool.
  UpdateBytesAllocated();
  pool_->FreeArenaChain(arena_head_);
}

uint8_t* ArenaAllocator::AllocFromNewArena(size_t bytes) {
  Arena* new_arena = pool_->AllocArena(std::max(arena_allocator::kArenaDefaultSize, bytes));
  DCHECK(new_arena != nullptr);
  DCHECK_LE(bytes, new_arena->Size());
  if (static_cast<size_t>(end_ - ptr_) > new_arena->Size() - bytes) {
    // The old arena has more space remaining than the new one, so keep using it.
    // This can happen when the requested size is over half of the default size.
    DCHECK(arena_head_ != nullptr);
    new_arena->bytes_allocated_ = bytes;  // UpdateBytesAllocated() on the new_arena.
    new_arena->next_ = arena_head_->next_;
    arena_head_->next_ = new_arena;
  } else {
    UpdateBytesAllocated();
    new_arena->next_ = arena_head_;
    arena_head_ = new_arena;
    // Update our internal data structures.
    begin_ = new_arena->Begin();
    DCHECK_ALIGNED(begin_, kAlignment);
    ptr_ = begin_ + bytes;
    end_ = new_arena->End();
  }
  return new_arena->Begin();
}

uint8_t* ArenaAllocator::AllocFromNewArenaWithMemoryTool(size_t bytes) {
  uint8_t* ret = AllocFromNewArena(bytes);
  uint8_t* noaccess_begin = ret + bytes;
  uint8_t* noaccess_end;
  if (ret == arena_head_->Begin()) {
    DCHECK(ptr_ - bytes == ret);
    noaccess_end = end_;
  } else {
    // We're still using the old arena but `ret` comes from a new one just after it.
    DCHECK(arena_head_->next_ != nullptr);
    DCHECK(ret == arena_head_->next_->Begin());
    DCHECK_EQ(bytes, arena_head_->next_->GetBytesAllocated());
    noaccess_end = arena_head_->next_->End();
  }
  MEMORY_TOOL_MAKE_NOACCESS(noaccess_begin, noaccess_end - noaccess_begin);
  return ret;
}

bool ArenaAllocator::Contains(const void* ptr) const {
  if (ptr >= begin_ && ptr < end_) {
    return true;
  }
  for (const Arena* cur_arena = arena_head_; cur_arena != nullptr; cur_arena = cur_arena->next_) {
    if (cur_arena->Contains(ptr)) {
      return true;
    }
  }
  return false;
}

MemStats::MemStats(const char* name,
                   const ArenaAllocatorStats* stats,
                   const Arena* first_arena,
                   ssize_t lost_bytes_adjustment)
    : name_(name),
      stats_(stats),
      first_arena_(first_arena),
      lost_bytes_adjustment_(lost_bytes_adjustment) {
}

void MemStats::Dump(std::ostream& os) const {
  os << name_ << " stats:\n";
  stats_->Dump(os, first_arena_, lost_bytes_adjustment_);
}

// Dump memory usage stats.
MemStats ArenaAllocator::GetMemStats() const {
  ssize_t lost_bytes_adjustment =
      (arena_head_ == nullptr) ? 0 : (end_ - ptr_) - arena_head_->RemainingSpace();
  return MemStats("ArenaAllocator", this, arena_head_, lost_bytes_adjustment);
}

}  // namespace art
