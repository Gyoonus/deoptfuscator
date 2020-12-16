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

#include "bump_pointer_space-inl.h"
#include "bump_pointer_space.h"
#include "gc/accounting/read_barrier_table.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "thread_list.h"

namespace art {
namespace gc {
namespace space {

// If a region has live objects whose size is less than this percent
// value of the region size, evaculate the region.
static constexpr uint kEvacuateLivePercentThreshold = 75U;

// If we protect the cleared regions.
// Only protect for target builds to prevent flaky test failures (b/63131961).
static constexpr bool kProtectClearedRegions = kIsTargetBuild;

MemMap* RegionSpace::CreateMemMap(const std::string& name, size_t capacity,
                                  uint8_t* requested_begin) {
  CHECK_ALIGNED(capacity, kRegionSize);
  std::string error_msg;
  // Ask for the capacity of an additional kRegionSize so that we can align the map by kRegionSize
  // even if we get unaligned base address. This is necessary for the ReadBarrierTable to work.
  std::unique_ptr<MemMap> mem_map;
  while (true) {
    mem_map.reset(MemMap::MapAnonymous(name.c_str(),
                                       requested_begin,
                                       capacity + kRegionSize,
                                       PROT_READ | PROT_WRITE,
                                       true,
                                       false,
                                       &error_msg));
    if (mem_map.get() != nullptr || requested_begin == nullptr) {
      break;
    }
    // Retry with no specified request begin.
    requested_begin = nullptr;
  }
  if (mem_map.get() == nullptr) {
    LOG(ERROR) << "Failed to allocate pages for alloc space (" << name << ") of size "
        << PrettySize(capacity) << " with message " << error_msg;
    MemMap::DumpMaps(LOG_STREAM(ERROR));
    return nullptr;
  }
  CHECK_EQ(mem_map->Size(), capacity + kRegionSize);
  CHECK_EQ(mem_map->Begin(), mem_map->BaseBegin());
  CHECK_EQ(mem_map->Size(), mem_map->BaseSize());
  if (IsAlignedParam(mem_map->Begin(), kRegionSize)) {
    // Got an aligned map. Since we requested a map that's kRegionSize larger. Shrink by
    // kRegionSize at the end.
    mem_map->SetSize(capacity);
  } else {
    // Got an unaligned map. Align the both ends.
    mem_map->AlignBy(kRegionSize);
  }
  CHECK_ALIGNED(mem_map->Begin(), kRegionSize);
  CHECK_ALIGNED(mem_map->End(), kRegionSize);
  CHECK_EQ(mem_map->Size(), capacity);
  return mem_map.release();
}

RegionSpace* RegionSpace::Create(const std::string& name, MemMap* mem_map) {
  return new RegionSpace(name, mem_map);
}

RegionSpace::RegionSpace(const std::string& name, MemMap* mem_map)
    : ContinuousMemMapAllocSpace(name, mem_map, mem_map->Begin(), mem_map->End(), mem_map->End(),
                                 kGcRetentionPolicyAlwaysCollect),
      region_lock_("Region lock", kRegionSpaceRegionLock),
      time_(1U),
      num_regions_(mem_map->Size() / kRegionSize),
      num_non_free_regions_(0U),
      num_evac_regions_(0U),
      max_peak_num_non_free_regions_(0U),
      non_free_region_index_limit_(0U),
      current_region_(&full_region_),
      evac_region_(nullptr) {
  CHECK_ALIGNED(mem_map->Size(), kRegionSize);
  CHECK_ALIGNED(mem_map->Begin(), kRegionSize);
  DCHECK_GT(num_regions_, 0U);
  regions_.reset(new Region[num_regions_]);
  uint8_t* region_addr = mem_map->Begin();
  for (size_t i = 0; i < num_regions_; ++i, region_addr += kRegionSize) {
    regions_[i].Init(i, region_addr, region_addr + kRegionSize);
  }
  mark_bitmap_.reset(
      accounting::ContinuousSpaceBitmap::Create("region space live bitmap", Begin(), Capacity()));
  if (kIsDebugBuild) {
    CHECK_EQ(regions_[0].Begin(), Begin());
    for (size_t i = 0; i < num_regions_; ++i) {
      CHECK(regions_[i].IsFree());
      CHECK_EQ(static_cast<size_t>(regions_[i].End() - regions_[i].Begin()), kRegionSize);
      if (i + 1 < num_regions_) {
        CHECK_EQ(regions_[i].End(), regions_[i + 1].Begin());
      }
    }
    CHECK_EQ(regions_[num_regions_ - 1].End(), Limit());
  }
  DCHECK(!full_region_.IsFree());
  DCHECK(full_region_.IsAllocated());
  size_t ignored;
  DCHECK(full_region_.Alloc(kAlignment, &ignored, nullptr, &ignored) == nullptr);
}

size_t RegionSpace::FromSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInFromSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

size_t RegionSpace::UnevacFromSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInUnevacFromSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

size_t RegionSpace::ToSpaceSize() {
  uint64_t num_regions = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsInToSpace()) {
      ++num_regions;
    }
  }
  return num_regions * kRegionSize;
}

inline bool RegionSpace::Region::ShouldBeEvacuated() {
  DCHECK((IsAllocated() || IsLarge()) && IsInToSpace());
  // The region should be evacuated if:
  // - the region was allocated after the start of the previous GC (newly allocated region); or
  // - the live ratio is below threshold (`kEvacuateLivePercentThreshold`).
  bool result;
  if (is_newly_allocated_) {
    result = true;
  } else {
    bool is_live_percent_valid = (live_bytes_ != static_cast<size_t>(-1));
    if (is_live_percent_valid) {
      DCHECK(IsInToSpace());
      DCHECK(!IsLargeTail());
      DCHECK_NE(live_bytes_, static_cast<size_t>(-1));
      DCHECK_LE(live_bytes_, BytesAllocated());
      const size_t bytes_allocated = RoundUp(BytesAllocated(), kRegionSize);
      DCHECK_LE(live_bytes_, bytes_allocated);
      if (IsAllocated()) {
        // Side node: live_percent == 0 does not necessarily mean
        // there's no live objects due to rounding (there may be a
        // few).
        result = (live_bytes_ * 100U < kEvacuateLivePercentThreshold * bytes_allocated);
      } else {
        DCHECK(IsLarge());
        result = (live_bytes_ == 0U);
      }
    } else {
      result = false;
    }
  }
  return result;
}

// Determine which regions to evacuate and mark them as
// from-space. Mark the rest as unevacuated from-space.
void RegionSpace::SetFromSpace(accounting::ReadBarrierTable* rb_table, bool force_evacuate_all) {
  ++time_;
  if (kUseTableLookupReadBarrier) {
    DCHECK(rb_table->IsAllCleared());
    rb_table->SetAll();
  }
  MutexLock mu(Thread::Current(), region_lock_);
  // Counter for the number of expected large tail regions following a large region.
  size_t num_expected_large_tails = 0U;
  // Flag to store whether the previously seen large region has been evacuated.
  // This is used to apply the same evacuation policy to related large tail regions.
  bool prev_large_evacuated = false;
  VerifyNonFreeRegionLimit();
  const size_t iter_limit = kUseTableLookupReadBarrier
      ? num_regions_
      : std::min(num_regions_, non_free_region_index_limit_);
  for (size_t i = 0; i < iter_limit; ++i) {
    Region* r = &regions_[i];
    RegionState state = r->State();
    RegionType type = r->Type();
    if (!r->IsFree()) {
      DCHECK(r->IsInToSpace());
      if (LIKELY(num_expected_large_tails == 0U)) {
        DCHECK((state == RegionState::kRegionStateAllocated ||
                state == RegionState::kRegionStateLarge) &&
               type == RegionType::kRegionTypeToSpace);
        bool should_evacuate = force_evacuate_all || r->ShouldBeEvacuated();
        if (should_evacuate) {
          r->SetAsFromSpace();
          DCHECK(r->IsInFromSpace());
        } else {
          r->SetAsUnevacFromSpace();
          DCHECK(r->IsInUnevacFromSpace());
        }
        if (UNLIKELY(state == RegionState::kRegionStateLarge &&
                     type == RegionType::kRegionTypeToSpace)) {
          prev_large_evacuated = should_evacuate;
          num_expected_large_tails = RoundUp(r->BytesAllocated(), kRegionSize) / kRegionSize - 1;
          DCHECK_GT(num_expected_large_tails, 0U);
        }
      } else {
        DCHECK(state == RegionState::kRegionStateLargeTail &&
               type == RegionType::kRegionTypeToSpace);
        if (prev_large_evacuated) {
          r->SetAsFromSpace();
          DCHECK(r->IsInFromSpace());
        } else {
          r->SetAsUnevacFromSpace();
          DCHECK(r->IsInUnevacFromSpace());
        }
        --num_expected_large_tails;
      }
    } else {
      DCHECK_EQ(num_expected_large_tails, 0U);
      if (kUseTableLookupReadBarrier) {
        // Clear the rb table for to-space regions.
        rb_table->Clear(r->Begin(), r->End());
      }
    }
  }
  DCHECK_EQ(num_expected_large_tails, 0U);
  current_region_ = &full_region_;
  evac_region_ = &full_region_;
}

static void ZeroAndProtectRegion(uint8_t* begin, uint8_t* end) {
  ZeroAndReleasePages(begin, end - begin);
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, begin, end - begin, PROT_NONE);
  }
}

void RegionSpace::ClearFromSpace(/* out */ uint64_t* cleared_bytes,
                                 /* out */ uint64_t* cleared_objects) {
  DCHECK(cleared_bytes != nullptr);
  DCHECK(cleared_objects != nullptr);
  *cleared_bytes = 0;
  *cleared_objects = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  VerifyNonFreeRegionLimit();
  size_t new_non_free_region_index_limit = 0;

  // Update max of peak non free region count before reclaiming evacuated regions.
  max_peak_num_non_free_regions_ = std::max(max_peak_num_non_free_regions_,
                                            num_non_free_regions_);

  // Lambda expression `clear_region` clears a region and adds a region to the
  // "clear block".
  //
  // As we sweep regions to clear them, we maintain a "clear block", composed of
  // adjacent cleared regions and whose bounds are `clear_block_begin` and
  // `clear_block_end`. When processing a new region which is not adjacent to
  // the clear block (discontinuity in cleared regions), the clear block
  // is zeroed and released and the clear block is reset (to the most recent
  // cleared region).
  //
  // This is done in order to combine zeroing and releasing pages to reduce how
  // often madvise is called. This helps reduce contention on the mmap semaphore
  // (see b/62194020).
  uint8_t* clear_block_begin = nullptr;
  uint8_t* clear_block_end = nullptr;
  auto clear_region = [&clear_block_begin, &clear_block_end](Region* r) {
    r->Clear(/*zero_and_release_pages*/false);
    if (clear_block_end != r->Begin()) {
      // Region `r` is not adjacent to the current clear block; zero and release
      // pages within the current block and restart a new clear block at the
      // beginning of region `r`.
      ZeroAndProtectRegion(clear_block_begin, clear_block_end);
      clear_block_begin = r->Begin();
    }
    // Add region `r` to the clear block.
    clear_block_end = r->End();
  };
  for (size_t i = 0; i < std::min(num_regions_, non_free_region_index_limit_); ++i) {
    Region* r = &regions_[i];
    if (r->IsInFromSpace()) {
      *cleared_bytes += r->BytesAllocated();
      *cleared_objects += r->ObjectsAllocated();
      --num_non_free_regions_;
      clear_region(r);
    } else if (r->IsInUnevacFromSpace()) {
      if (r->LiveBytes() == 0) {
        DCHECK(!r->IsLargeTail());
        // Special case for 0 live bytes, this means all of the objects in the region are dead and
        // we can clear it. This is important for large objects since we must not visit dead ones in
        // RegionSpace::Walk because they may contain dangling references to invalid objects.
        // It is also better to clear these regions now instead of at the end of the next GC to
        // save RAM. If we don't clear the regions here, they will be cleared next GC by the normal
        // live percent evacuation logic.
        size_t free_regions = 1;
        // Also release RAM for large tails.
        while (i + free_regions < num_regions_ && regions_[i + free_regions].IsLargeTail()) {
          DCHECK(r->IsLarge());
          clear_region(&regions_[i + free_regions]);
          ++free_regions;
        }
        *cleared_bytes += r->BytesAllocated();
        *cleared_objects += r->ObjectsAllocated();
        num_non_free_regions_ -= free_regions;
        clear_region(r);
        GetLiveBitmap()->ClearRange(
            reinterpret_cast<mirror::Object*>(r->Begin()),
            reinterpret_cast<mirror::Object*>(r->Begin() + free_regions * kRegionSize));
        continue;
      }
      r->SetUnevacFromSpaceAsToSpace();
      if (r->AllAllocatedBytesAreLive()) {
        // Try to optimize the number of ClearRange calls by checking whether the next regions
        // can also be cleared.
        size_t regions_to_clear_bitmap = 1;
        while (i + regions_to_clear_bitmap < num_regions_) {
          Region* const cur = &regions_[i + regions_to_clear_bitmap];
          if (!cur->AllAllocatedBytesAreLive()) {
            DCHECK(!cur->IsLargeTail());
            break;
          }
          CHECK(cur->IsInUnevacFromSpace());
          cur->SetUnevacFromSpaceAsToSpace();
          ++regions_to_clear_bitmap;
        }

        // Optimization: If the live bytes are *all* live in a region
        // then the live-bit information for these objects is superfluous:
        // - We can determine that these objects are all live by using
        //   Region::AllAllocatedBytesAreLive (which just checks whether
        //   `LiveBytes() == static_cast<size_t>(Top() - Begin())`.
        // - We can visit the objects in this region using
        //   RegionSpace::GetNextObject, i.e. without resorting to the
        //   live bits (see RegionSpace::WalkInternal).
        // Therefore, we can clear the bits for these objects in the
        // (live) region space bitmap (and release the corresponding pages).
        GetLiveBitmap()->ClearRange(
            reinterpret_cast<mirror::Object*>(r->Begin()),
            reinterpret_cast<mirror::Object*>(r->Begin() + regions_to_clear_bitmap * kRegionSize));
        // Skip over extra regions for which we cleared the bitmaps: we shall not clear them,
        // as they are unevac regions that are live.
        // Subtract one for the for-loop.
        i += regions_to_clear_bitmap - 1;
      }
    }
    // Note r != last_checked_region if r->IsInUnevacFromSpace() was true above.
    Region* last_checked_region = &regions_[i];
    if (!last_checked_region->IsFree()) {
      new_non_free_region_index_limit = std::max(new_non_free_region_index_limit,
                                                 last_checked_region->Idx() + 1);
    }
  }
  // Clear pages for the last block since clearing happens when a new block opens.
  ZeroAndReleasePages(clear_block_begin, clear_block_end - clear_block_begin);
  // Update non_free_region_index_limit_.
  SetNonFreeRegionLimit(new_non_free_region_index_limit);
  evac_region_ = nullptr;
  num_non_free_regions_ += num_evac_regions_;
  num_evac_regions_ = 0;
}

void RegionSpace::LogFragmentationAllocFailure(std::ostream& os,
                                               size_t /* failed_alloc_bytes */) {
  size_t max_contiguous_allocation = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  if (current_region_->End() - current_region_->Top() > 0) {
    max_contiguous_allocation = current_region_->End() - current_region_->Top();
  }
  if (num_non_free_regions_ * 2 < num_regions_) {
    // We reserve half of the regions for evaluation only. If we
    // occupy more than half the regions, do not report the free
    // regions as available.
    size_t max_contiguous_free_regions = 0;
    size_t num_contiguous_free_regions = 0;
    bool prev_free_region = false;
    for (size_t i = 0; i < num_regions_; ++i) {
      Region* r = &regions_[i];
      if (r->IsFree()) {
        if (!prev_free_region) {
          CHECK_EQ(num_contiguous_free_regions, 0U);
          prev_free_region = true;
        }
        ++num_contiguous_free_regions;
      } else {
        if (prev_free_region) {
          CHECK_NE(num_contiguous_free_regions, 0U);
          max_contiguous_free_regions = std::max(max_contiguous_free_regions,
                                                 num_contiguous_free_regions);
          num_contiguous_free_regions = 0U;
          prev_free_region = false;
        }
      }
    }
    max_contiguous_allocation = std::max(max_contiguous_allocation,
                                         max_contiguous_free_regions * kRegionSize);
  }
  os << "; failed due to fragmentation (largest possible contiguous allocation "
     <<  max_contiguous_allocation << " bytes)";
  // Caller's job to print failed_alloc_bytes.
}

void RegionSpace::Clear() {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (!r->IsFree()) {
      --num_non_free_regions_;
    }
    r->Clear(/*zero_and_release_pages*/true);
  }
  SetNonFreeRegionLimit(0);
  current_region_ = &full_region_;
  evac_region_ = &full_region_;
}

void RegionSpace::ClampGrowthLimit(size_t new_capacity) {
  MutexLock mu(Thread::Current(), region_lock_);
  CHECK_LE(new_capacity, NonGrowthLimitCapacity());
  size_t new_num_regions = new_capacity / kRegionSize;
  if (non_free_region_index_limit_ > new_num_regions) {
    LOG(WARNING) << "Couldn't clamp region space as there are regions in use beyond growth limit.";
    return;
  }
  num_regions_ = new_num_regions;
  SetLimit(Begin() + new_capacity);
  if (Size() > new_capacity) {
    SetEnd(Limit());
  }
  GetMarkBitmap()->SetHeapSize(new_capacity);
  GetMemMap()->SetSize(new_capacity);
}

void RegionSpace::Dump(std::ostream& os) const {
  os << GetName() << " "
     << reinterpret_cast<void*>(Begin()) << "-" << reinterpret_cast<void*>(Limit());
}

void RegionSpace::DumpRegionForObject(std::ostream& os, mirror::Object* obj) {
  CHECK(HasAddress(obj));
  MutexLock mu(Thread::Current(), region_lock_);
  RefToRegionUnlocked(obj)->Dump(os);
}

void RegionSpace::DumpRegions(std::ostream& os) {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    regions_[i].Dump(os);
  }
}

void RegionSpace::DumpNonFreeRegions(std::ostream& os) {
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* reg = &regions_[i];
    if (!reg->IsFree()) {
      reg->Dump(os);
    }
  }
}

void RegionSpace::RecordAlloc(mirror::Object* ref) {
  CHECK(ref != nullptr);
  Region* r = RefToRegion(ref);
  r->objects_allocated_.FetchAndAddSequentiallyConsistent(1);
}

bool RegionSpace::AllocNewTlab(Thread* self, size_t min_bytes) {
  MutexLock mu(self, region_lock_);
  RevokeThreadLocalBuffersLocked(self);
  // Retain sufficient free regions for full evacuation.

  Region* r = AllocateRegion(/*for_evac*/ false);
  if (r != nullptr) {
    r->is_a_tlab_ = true;
    r->thread_ = self;
    r->SetTop(r->End());
    self->SetTlab(r->Begin(), r->Begin() + min_bytes, r->End());
    return true;
  }
  return false;
}

size_t RegionSpace::RevokeThreadLocalBuffers(Thread* thread) {
  MutexLock mu(Thread::Current(), region_lock_);
  RevokeThreadLocalBuffersLocked(thread);
  return 0U;
}

void RegionSpace::RevokeThreadLocalBuffersLocked(Thread* thread) {
  uint8_t* tlab_start = thread->GetTlabStart();
  DCHECK_EQ(thread->HasTlab(), tlab_start != nullptr);
  if (tlab_start != nullptr) {
    DCHECK_ALIGNED(tlab_start, kRegionSize);
    Region* r = RefToRegionLocked(reinterpret_cast<mirror::Object*>(tlab_start));
    DCHECK(r->IsAllocated());
    DCHECK_LE(thread->GetThreadLocalBytesAllocated(), kRegionSize);
    r->RecordThreadLocalAllocations(thread->GetThreadLocalObjectsAllocated(),
                                    thread->GetThreadLocalBytesAllocated());
    r->is_a_tlab_ = false;
    r->thread_ = nullptr;
  }
  thread->SetTlab(nullptr, nullptr, nullptr);
}

size_t RegionSpace::RevokeAllThreadLocalBuffers() {
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::runtime_shutdown_lock_);
  MutexLock mu2(self, *Locks::thread_list_lock_);
  std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
  for (Thread* thread : thread_list) {
    RevokeThreadLocalBuffers(thread);
  }
  return 0U;
}

void RegionSpace::AssertThreadLocalBuffersAreRevoked(Thread* thread) {
  if (kIsDebugBuild) {
    DCHECK(!thread->HasTlab());
  }
}

void RegionSpace::AssertAllThreadLocalBuffersAreRevoked() {
  if (kIsDebugBuild) {
    Thread* self = Thread::Current();
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    MutexLock mu2(self, *Locks::thread_list_lock_);
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      AssertThreadLocalBuffersAreRevoked(thread);
    }
  }
}

void RegionSpace::Region::Dump(std::ostream& os) const {
  os << "Region[" << idx_ << "]="
     << reinterpret_cast<void*>(begin_)
     << "-" << reinterpret_cast<void*>(Top())
     << "-" << reinterpret_cast<void*>(end_)
     << " state=" << state_
     << " type=" << type_
     << " objects_allocated=" << objects_allocated_
     << " alloc_time=" << alloc_time_
     << " live_bytes=" << live_bytes_
     << " is_newly_allocated=" << std::boolalpha << is_newly_allocated_ << std::noboolalpha
     << " is_a_tlab=" << std::boolalpha << is_a_tlab_ << std::noboolalpha
     << " thread=" << thread_ << '\n';
}

size_t RegionSpace::AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size) {
  size_t num_bytes = obj->SizeOf();
  if (usable_size != nullptr) {
    if (LIKELY(num_bytes <= kRegionSize)) {
      DCHECK(RefToRegion(obj)->IsAllocated());
      *usable_size = RoundUp(num_bytes, kAlignment);
    } else {
      DCHECK(RefToRegion(obj)->IsLarge());
      *usable_size = RoundUp(num_bytes, kRegionSize);
    }
  }
  return num_bytes;
}

void RegionSpace::Region::Clear(bool zero_and_release_pages) {
  top_.StoreRelaxed(begin_);
  state_ = RegionState::kRegionStateFree;
  type_ = RegionType::kRegionTypeNone;
  objects_allocated_.StoreRelaxed(0);
  alloc_time_ = 0;
  live_bytes_ = static_cast<size_t>(-1);
  if (zero_and_release_pages) {
    ZeroAndProtectRegion(begin_, end_);
  }
  is_newly_allocated_ = false;
  is_a_tlab_ = false;
  thread_ = nullptr;
}

RegionSpace::Region* RegionSpace::AllocateRegion(bool for_evac) {
  if (!for_evac && (num_non_free_regions_ + 1) * 2 > num_regions_) {
    return nullptr;
  }
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree()) {
      r->Unfree(this, time_);
      if (for_evac) {
        ++num_evac_regions_;
        // Evac doesn't count as newly allocated.
      } else {
        r->SetNewlyAllocated();
        ++num_non_free_regions_;
      }
      return r;
    }
  }
  return nullptr;
}

void RegionSpace::Region::MarkAsAllocated(RegionSpace* region_space, uint32_t alloc_time) {
  DCHECK(IsFree());
  alloc_time_ = alloc_time;
  region_space->AdjustNonFreeRegionLimit(idx_);
  type_ = RegionType::kRegionTypeToSpace;
  if (kProtectClearedRegions) {
    CheckedCall(mprotect, __FUNCTION__, Begin(), kRegionSize, PROT_READ | PROT_WRITE);
  }
}

void RegionSpace::Region::Unfree(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateAllocated;
}

void RegionSpace::Region::UnfreeLarge(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateLarge;
}

void RegionSpace::Region::UnfreeLargeTail(RegionSpace* region_space, uint32_t alloc_time) {
  MarkAsAllocated(region_space, alloc_time);
  state_ = RegionState::kRegionStateLargeTail;
}

}  // namespace space
}  // namespace gc
}  // namespace art
