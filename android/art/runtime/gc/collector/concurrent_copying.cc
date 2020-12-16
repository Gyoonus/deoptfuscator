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

#include "concurrent_copying.h"

#include "art_field-inl.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/histogram-inl.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "debugger.h"
#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/accounting/mod_union_table-inl.h"
#include "gc/accounting/read_barrier_table.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/gc_pause_listener.h"
#include "gc/reference_processor.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "gc/verification.h"
#include "image-inl.h"
#include "intern_table.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_list.h"
#include "well_known_classes.h"

namespace art {
namespace gc {
namespace collector {

static constexpr size_t kDefaultGcMarkStackSize = 2 * MB;
// If kFilterModUnionCards then we attempt to filter cards that don't need to be dirty in the mod
// union table. Disabled since it does not seem to help the pause much.
static constexpr bool kFilterModUnionCards = kIsDebugBuild;
// If kDisallowReadBarrierDuringScan is true then the GC aborts if there are any that occur during
// ConcurrentCopying::Scan. May be used to diagnose possibly unnecessary read barriers.
// Only enabled for kIsDebugBuild to avoid performance hit.
static constexpr bool kDisallowReadBarrierDuringScan = kIsDebugBuild;
// Slow path mark stack size, increase this if the stack is getting full and it is causing
// performance problems.
static constexpr size_t kReadBarrierMarkStackSize = 512 * KB;
// Verify that there are no missing card marks.
static constexpr bool kVerifyNoMissingCardMarks = kIsDebugBuild;

ConcurrentCopying::ConcurrentCopying(Heap* heap,
                                     const std::string& name_prefix,
                                     bool measure_read_barrier_slow_path)
    : GarbageCollector(heap,
                       name_prefix + (name_prefix.empty() ? "" : " ") +
                       "concurrent copying"),
      region_space_(nullptr), gc_barrier_(new Barrier(0)),
      gc_mark_stack_(accounting::ObjectStack::Create("concurrent copying gc mark stack",
                                                     kDefaultGcMarkStackSize,
                                                     kDefaultGcMarkStackSize)),
      rb_mark_bit_stack_(accounting::ObjectStack::Create("rb copying gc mark stack",
                                                         kReadBarrierMarkStackSize,
                                                         kReadBarrierMarkStackSize)),
      rb_mark_bit_stack_full_(false),
      mark_stack_lock_("concurrent copying mark stack lock", kMarkSweepMarkStackLock),
      thread_running_gc_(nullptr),
      is_marking_(false),
      is_using_read_barrier_entrypoints_(false),
      is_active_(false),
      is_asserting_to_space_invariant_(false),
      region_space_bitmap_(nullptr),
      heap_mark_bitmap_(nullptr),
      live_stack_freeze_size_(0),
      from_space_num_objects_at_first_pause_(0),
      from_space_num_bytes_at_first_pause_(0),
      mark_stack_mode_(kMarkStackModeOff),
      weak_ref_access_enabled_(true),
      skipped_blocks_lock_("concurrent copying bytes blocks lock", kMarkSweepMarkStackLock),
      measure_read_barrier_slow_path_(measure_read_barrier_slow_path),
      mark_from_read_barrier_measurements_(false),
      rb_slow_path_ns_(0),
      rb_slow_path_count_(0),
      rb_slow_path_count_gc_(0),
      rb_slow_path_histogram_lock_("Read barrier histogram lock"),
      rb_slow_path_time_histogram_("Mutator time in read barrier slow path", 500, 32),
      rb_slow_path_count_total_(0),
      rb_slow_path_count_gc_total_(0),
      rb_table_(heap_->GetReadBarrierTable()),
      force_evacuate_all_(false),
      gc_grays_immune_objects_(false),
      immune_gray_stack_lock_("concurrent copying immune gray stack lock",
                              kMarkSweepMarkStackLock) {
  static_assert(space::RegionSpace::kRegionSize == accounting::ReadBarrierTable::kRegionSize,
                "The region space size and the read barrier table region size must match");
  Thread* self = Thread::Current();
  {
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // Cache this so that we won't have to lock heap_bitmap_lock_ in
    // Mark() which could cause a nested lock on heap_bitmap_lock_
    // when GC causes a RB while doing GC or a lock order violation
    // (class_linker_lock_ and heap_bitmap_lock_).
    heap_mark_bitmap_ = heap->GetMarkBitmap();
  }
  {
    MutexLock mu(self, mark_stack_lock_);
    for (size_t i = 0; i < kMarkStackPoolSize; ++i) {
      accounting::AtomicStack<mirror::Object>* mark_stack =
          accounting::AtomicStack<mirror::Object>::Create(
              "thread local mark stack", kMarkStackSize, kMarkStackSize);
      pooled_mark_stacks_.push_back(mark_stack);
    }
  }
}

void ConcurrentCopying::MarkHeapReference(mirror::HeapReference<mirror::Object>* field,
                                          bool do_atomic_update) {
  if (UNLIKELY(do_atomic_update)) {
    // Used to mark the referent in DelayReferenceReferent in transaction mode.
    mirror::Object* from_ref = field->AsMirrorPtr();
    if (from_ref == nullptr) {
      return;
    }
    mirror::Object* to_ref = Mark(from_ref);
    if (from_ref != to_ref) {
      do {
        if (field->AsMirrorPtr() != from_ref) {
          // Concurrently overwritten by a mutator.
          break;
        }
      } while (!field->CasWeakRelaxed(from_ref, to_ref));
    }
  } else {
    // Used for preserving soft references, should be OK to not have a CAS here since there should be
    // no other threads which can trigger read barriers on the same referent during reference
    // processing.
    field->Assign(Mark(field->AsMirrorPtr()));
  }
}

ConcurrentCopying::~ConcurrentCopying() {
  STLDeleteElements(&pooled_mark_stacks_);
}

void ConcurrentCopying::RunPhases() {
  CHECK(kUseBakerReadBarrier || kUseTableLookupReadBarrier);
  CHECK(!is_active_);
  is_active_ = true;
  Thread* self = Thread::Current();
  thread_running_gc_ = self;
  Locks::mutator_lock_->AssertNotHeld(self);
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    InitializePhase();
  }
  if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
    // Switch to read barrier mark entrypoints before we gray the objects. This is required in case
    // a mutator sees a gray bit and dispatches on the entrypoint. (b/37876887).
    ActivateReadBarrierEntrypoints();
    // Gray dirty immune objects concurrently to reduce GC pause times. We re-process gray cards in
    // the pause.
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    GrayAllDirtyImmuneObjects();
  }
  FlipThreadRoots();
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    MarkingPhase();
  }
  // Verify no from space refs. This causes a pause.
  if (kEnableNoFromSpaceRefsVerification) {
    TimingLogger::ScopedTiming split("(Paused)VerifyNoFromSpaceReferences", GetTimings());
    ScopedPause pause(this, false);
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "Verifying no from-space refs";
    }
    VerifyNoFromSpaceReferences();
    if (kVerboseMode) {
      LOG(INFO) << "Done verifying no from-space refs";
    }
    CheckEmptyMarkStack();
  }
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    ReclaimPhase();
  }
  FinishPhase();
  CHECK(is_active_);
  is_active_ = false;
  thread_running_gc_ = nullptr;
}

class ConcurrentCopying::ActivateReadBarrierEntrypointsCheckpoint : public Closure {
 public:
  explicit ActivateReadBarrierEntrypointsCheckpoint(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {}

  void Run(Thread* thread) OVERRIDE NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    DCHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Switch to the read barrier entrypoints.
    thread->SetReadBarrierEntrypoints();
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::ActivateReadBarrierEntrypointsCallback : public Closure {
 public:
  explicit ActivateReadBarrierEntrypointsCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {}

  void Run(Thread* self ATTRIBUTE_UNUSED) OVERRIDE REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a race with ThreadList::Register().
    CHECK(!concurrent_copying_->is_using_read_barrier_entrypoints_);
    concurrent_copying_->is_using_read_barrier_entrypoints_ = true;
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::ActivateReadBarrierEntrypoints() {
  Thread* const self = Thread::Current();
  ActivateReadBarrierEntrypointsCheckpoint checkpoint(this);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  ActivateReadBarrierEntrypointsCallback callback(this);
  const size_t barrier_count = thread_list->RunCheckpoint(&checkpoint, &callback);
  // If there are no threads to wait which implies that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
  gc_barrier_->Increment(self, barrier_count);
}

void ConcurrentCopying::BindBitmaps() {
  Thread* self = Thread::Current();
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  // Mark all of the spaces we never collect as immune.
  for (const auto& space : heap_->GetContinuousSpaces()) {
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyNeverCollect ||
        space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect) {
      CHECK(space->IsZygoteSpace() || space->IsImageSpace());
      immune_spaces_.AddSpace(space);
    } else if (space == region_space_) {
      // It is OK to clear the bitmap with mutators running since the only place it is read is
      // VisitObjects which has exclusion with CC.
      region_space_bitmap_ = region_space_->GetMarkBitmap();
      region_space_bitmap_->Clear();
    }
  }
}

void ConcurrentCopying::InitializePhase() {
  TimingLogger::ScopedTiming split("InitializePhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC InitializePhase";
    LOG(INFO) << "Region-space : " << reinterpret_cast<void*>(region_space_->Begin()) << "-"
              << reinterpret_cast<void*>(region_space_->Limit());
  }
  CheckEmptyMarkStack();
  if (kIsDebugBuild) {
    MutexLock mu(Thread::Current(), mark_stack_lock_);
    CHECK(false_gray_stack_.empty());
  }

  rb_mark_bit_stack_full_ = false;
  mark_from_read_barrier_measurements_ = measure_read_barrier_slow_path_;
  if (measure_read_barrier_slow_path_) {
    rb_slow_path_ns_.StoreRelaxed(0);
    rb_slow_path_count_.StoreRelaxed(0);
    rb_slow_path_count_gc_.StoreRelaxed(0);
  }

  immune_spaces_.Reset();
  bytes_moved_.StoreRelaxed(0);
  objects_moved_.StoreRelaxed(0);
  GcCause gc_cause = GetCurrentIteration()->GetGcCause();
  if (gc_cause == kGcCauseExplicit ||
      gc_cause == kGcCauseCollectorTransition ||
      GetCurrentIteration()->GetClearSoftReferences()) {
    force_evacuate_all_ = true;
  } else {
    force_evacuate_all_ = false;
  }
  if (kUseBakerReadBarrier) {
    updated_all_immune_objects_.StoreRelaxed(false);
    // GC may gray immune objects in the thread flip.
    gc_grays_immune_objects_ = true;
    if (kIsDebugBuild) {
      MutexLock mu(Thread::Current(), immune_gray_stack_lock_);
      DCHECK(immune_gray_stack_.empty());
    }
  }
  BindBitmaps();
  if (kVerboseMode) {
    LOG(INFO) << "force_evacuate_all=" << force_evacuate_all_;
    LOG(INFO) << "Largest immune region: " << immune_spaces_.GetLargestImmuneRegion().Begin()
              << "-" << immune_spaces_.GetLargestImmuneRegion().End();
    for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
      LOG(INFO) << "Immune space: " << *space;
    }
    LOG(INFO) << "GC end of InitializePhase";
  }
  // Mark all of the zygote large objects without graying them.
  MarkZygoteLargeObjects();
}

// Used to switch the thread roots of a thread from from-space refs to to-space refs.
class ConcurrentCopying::ThreadFlipVisitor : public Closure, public RootVisitor {
 public:
  ThreadFlipVisitor(ConcurrentCopying* concurrent_copying, bool use_tlab)
      : concurrent_copying_(concurrent_copying), use_tlab_(use_tlab) {
  }

  virtual void Run(Thread* thread) OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    CHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    thread->SetIsGcMarkingAndUpdateEntrypoints(true);
    if (use_tlab_ && thread->HasTlab()) {
      if (ConcurrentCopying::kEnableFromSpaceAccountingCheck) {
        // This must come before the revoke.
        size_t thread_local_objects = thread->GetThreadLocalObjectsAllocated();
        concurrent_copying_->region_space_->RevokeThreadLocalBuffers(thread);
        reinterpret_cast<Atomic<size_t>*>(
            &concurrent_copying_->from_space_num_objects_at_first_pause_)->
                FetchAndAddSequentiallyConsistent(thread_local_objects);
      } else {
        concurrent_copying_->region_space_->RevokeThreadLocalBuffers(thread);
      }
    }
    if (kUseThreadLocalAllocationStack) {
      thread->RevokeThreadLocalAllocationStack();
    }
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // We can use the non-CAS VisitRoots functions below because we update thread-local GC roots
    // only.
    thread->VisitRoots(this, kVisitRootFlagAllRoots);
    concurrent_copying_->GetBarrier().Pass(self);
  }

  void VisitRoots(mirror::Object*** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      mirror::Object** root = roots[i];
      mirror::Object* ref = *root;
      if (ref != nullptr) {
        mirror::Object* to_ref = concurrent_copying_->Mark(ref);
        if (to_ref != ref) {
          *root = to_ref;
        }
      }
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      mirror::CompressedReference<mirror::Object>* const root = roots[i];
      if (!root->IsNull()) {
        mirror::Object* ref = root->AsMirrorPtr();
        mirror::Object* to_ref = concurrent_copying_->Mark(ref);
        if (to_ref != ref) {
          root->Assign(to_ref);
        }
      }
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
  const bool use_tlab_;
};

// Called back from Runtime::FlipThreadRoots() during a pause.
class ConcurrentCopying::FlipCallback : public Closure {
 public:
  explicit FlipCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  virtual void Run(Thread* thread) OVERRIDE REQUIRES(Locks::mutator_lock_) {
    ConcurrentCopying* cc = concurrent_copying_;
    TimingLogger::ScopedTiming split("(Paused)FlipCallback", cc->GetTimings());
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    if (kVerifyNoMissingCardMarks) {
      cc->VerifyNoMissingCardMarks();
    }
    CHECK_EQ(thread, self);
    Locks::mutator_lock_->AssertExclusiveHeld(self);
    {
      TimingLogger::ScopedTiming split2("(Paused)SetFromSpace", cc->GetTimings());
      cc->region_space_->SetFromSpace(cc->rb_table_, cc->force_evacuate_all_);
    }
    cc->SwapStacks();
    if (ConcurrentCopying::kEnableFromSpaceAccountingCheck) {
      cc->RecordLiveStackFreezeSize(self);
      cc->from_space_num_objects_at_first_pause_ = cc->region_space_->GetObjectsAllocated();
      cc->from_space_num_bytes_at_first_pause_ = cc->region_space_->GetBytesAllocated();
    }
    cc->is_marking_ = true;
    cc->mark_stack_mode_.StoreRelaxed(ConcurrentCopying::kMarkStackModeThreadLocal);
    if (kIsDebugBuild) {
      cc->region_space_->AssertAllRegionLiveBytesZeroOrCleared();
    }
    if (UNLIKELY(Runtime::Current()->IsActiveTransaction())) {
      CHECK(Runtime::Current()->IsAotCompiler());
      TimingLogger::ScopedTiming split3("(Paused)VisitTransactionRoots", cc->GetTimings());
      Runtime::Current()->VisitTransactionRoots(cc);
    }
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      cc->GrayAllNewlyDirtyImmuneObjects();
      if (kIsDebugBuild) {
        // Check that all non-gray immune objects only reference immune objects.
        cc->VerifyGrayImmuneObjects();
      }
    }
    // May be null during runtime creation, in this case leave java_lang_Object null.
    // This is safe since single threaded behavior should mean FillDummyObject does not
    // happen when java_lang_Object_ is null.
    if (WellKnownClasses::java_lang_Object != nullptr) {
      cc->java_lang_Object_ = down_cast<mirror::Class*>(cc->Mark(
          WellKnownClasses::ToClass(WellKnownClasses::java_lang_Object).Ptr()));
    } else {
      cc->java_lang_Object_ = nullptr;
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::VerifyGrayImmuneObjectsVisitor {
 public:
  explicit VerifyGrayImmuneObjectsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj, MemberOffset offset, bool /* is_static */)
      const ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_) {
    CheckReference(obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset),
                   obj, offset);
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    CheckReference(ref->GetReferent<kWithoutReadBarrier>(),
                   ref,
                   mirror::Reference::ReferentOffset());
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CheckReference(root->AsMirrorPtr(), nullptr, MemberOffset(0));
  }

 private:
  ConcurrentCopying* const collector_;

  void CheckReference(ObjPtr<mirror::Object> ref,
                      ObjPtr<mirror::Object> holder,
                      MemberOffset offset) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (ref != nullptr) {
      if (!collector_->immune_spaces_.ContainsObject(ref.Ptr())) {
        // Not immune, must be a zygote large object.
        CHECK(Runtime::Current()->GetHeap()->GetLargeObjectsSpace()->IsZygoteLargeObject(
            Thread::Current(), ref.Ptr()))
            << "Non gray object references non immune, non zygote large object "<< ref << " "
            << mirror::Object::PrettyTypeOf(ref) << " in holder " << holder << " "
            << mirror::Object::PrettyTypeOf(holder) << " offset=" << offset.Uint32Value();
      } else {
        // Make sure the large object class is immune since we will never scan the large object.
        CHECK(collector_->immune_spaces_.ContainsObject(
            ref->GetClass<kVerifyNone, kWithoutReadBarrier>()));
      }
    }
  }
};

void ConcurrentCopying::VerifyGrayImmuneObjects() {
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  for (auto& space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
    VerifyGrayImmuneObjectsVisitor visitor(this);
    live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(space->Begin()),
                                  reinterpret_cast<uintptr_t>(space->Limit()),
                                  [&visitor](mirror::Object* obj)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      // If an object is not gray, it should only have references to things in the immune spaces.
      if (obj->GetReadBarrierState() != ReadBarrier::GrayState()) {
        obj->VisitReferences</*kVisitNativeRoots*/true,
                             kDefaultVerifyFlags,
                             kWithoutReadBarrier>(visitor, visitor);
      }
    });
  }
}

class ConcurrentCopying::VerifyNoMissingCardMarkVisitor {
 public:
  VerifyNoMissingCardMarkVisitor(ConcurrentCopying* cc, ObjPtr<mirror::Object> holder)
    : cc_(cc),
      holder_(holder) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    if (offset.Uint32Value() != mirror::Object::ClassOffset().Uint32Value()) {
     CheckReference(obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(
         offset), offset.Uint32Value());
    }
  }
  void operator()(ObjPtr<mirror::Class> klass,
                  ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CheckReference(root->AsMirrorPtr());
  }

  void CheckReference(mirror::Object* ref, int32_t offset = -1) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(ref == nullptr || !cc_->region_space_->IsInNewlyAllocatedRegion(ref))
        << holder_->PrettyTypeOf() << "(" << holder_.Ptr() << ") references object "
        << ref->PrettyTypeOf() << "(" << ref << ") in newly allocated region at offset=" << offset;
  }

 private:
  ConcurrentCopying* const cc_;
  ObjPtr<mirror::Object> const holder_;
};

void ConcurrentCopying::VerifyNoMissingCardMarks() {
  auto visitor = [&](mirror::Object* obj)
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_) {
    // Objects not on dirty or aged cards should never have references to newly allocated regions.
    if (heap_->GetCardTable()->GetCard(obj) == gc::accounting::CardTable::kCardClean) {
      VerifyNoMissingCardMarkVisitor internal_visitor(this, /*holder*/ obj);
      obj->VisitReferences</*kVisitNativeRoots*/true, kVerifyNone, kWithoutReadBarrier>(
          internal_visitor, internal_visitor);
    }
  };
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  region_space_->Walk(visitor);
  {
    ReaderMutexLock rmu(Thread::Current(), *Locks::heap_bitmap_lock_);
    heap_->GetLiveBitmap()->Visit(visitor);
  }
}

// Switch threads that from from-space to to-space refs. Forward/mark the thread roots.
void ConcurrentCopying::FlipThreadRoots() {
  TimingLogger::ScopedTiming split("FlipThreadRoots", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
  }
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  gc_barrier_->Init(self, 0);
  ThreadFlipVisitor thread_flip_visitor(this, heap_->use_tlab_);
  FlipCallback flip_callback(this);

  size_t barrier_count = Runtime::Current()->GetThreadList()->FlipThreadRoots(
      &thread_flip_visitor, &flip_callback, this, GetHeap()->GetGcPauseListener());

  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  is_asserting_to_space_invariant_ = true;
  QuasiAtomic::ThreadFenceForConstructor();
  if (kVerboseMode) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
    LOG(INFO) << "GC end of FlipThreadRoots";
  }
}

template <bool kConcurrent>
class ConcurrentCopying::GrayImmuneObjectVisitor {
 public:
  explicit GrayImmuneObjectVisitor(Thread* self) : self_(self) {}

  ALWAYS_INLINE void operator()(mirror::Object* obj) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kUseBakerReadBarrier && obj->GetReadBarrierState() == ReadBarrier::WhiteState()) {
      if (kConcurrent) {
        Locks::mutator_lock_->AssertSharedHeld(self_);
        obj->AtomicSetReadBarrierState(ReadBarrier::WhiteState(), ReadBarrier::GrayState());
        // Mod union table VisitObjects may visit the same object multiple times so we can't check
        // the result of the atomic set.
      } else {
        Locks::mutator_lock_->AssertExclusiveHeld(self_);
        obj->SetReadBarrierState(ReadBarrier::GrayState());
      }
    }
  }

  static void Callback(mirror::Object* obj, void* arg) REQUIRES_SHARED(Locks::mutator_lock_) {
    reinterpret_cast<GrayImmuneObjectVisitor<kConcurrent>*>(arg)->operator()(obj);
  }

 private:
  Thread* const self_;
};

void ConcurrentCopying::GrayAllDirtyImmuneObjects() {
  TimingLogger::ScopedTiming split("GrayAllDirtyImmuneObjects", GetTimings());
  accounting::CardTable* const card_table = heap_->GetCardTable();
  Thread* const self = Thread::Current();
  using VisitorType = GrayImmuneObjectVisitor</* kIsConcurrent */ true>;
  VisitorType visitor(self);
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
    // Mark all the objects on dirty cards since these may point to objects in other space.
    // Once these are marked, the GC will eventually clear them later.
    // Table is non null for boot image and zygote spaces. It is only null for application image
    // spaces.
    if (table != nullptr) {
      table->ProcessCards();
      table->VisitObjects(&VisitorType::Callback, &visitor);
      // Don't clear cards here since we need to rescan in the pause. If we cleared the cards here,
      // there would be races with the mutator marking new cards.
    } else {
      // Keep cards aged if we don't have a mod-union table since we may need to scan them in future
      // GCs. This case is for app images.
      card_table->ModifyCardsAtomic(
          space->Begin(),
          space->End(),
          [](uint8_t card) {
            return (card != gc::accounting::CardTable::kCardClean)
                ? gc::accounting::CardTable::kCardAged
                : card;
          },
          /* card modified visitor */ VoidFunctor());
      card_table->Scan</* kClearCard */ false>(space->GetMarkBitmap(),
                                               space->Begin(),
                                               space->End(),
                                               visitor,
                                               gc::accounting::CardTable::kCardAged);
    }
  }
}

void ConcurrentCopying::GrayAllNewlyDirtyImmuneObjects() {
  TimingLogger::ScopedTiming split("(Paused)GrayAllNewlyDirtyImmuneObjects", GetTimings());
  accounting::CardTable* const card_table = heap_->GetCardTable();
  using VisitorType = GrayImmuneObjectVisitor</* kIsConcurrent */ false>;
  Thread* const self = Thread::Current();
  VisitorType visitor(self);
  WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);

    // Don't need to scan aged cards since we did these before the pause. Note that scanning cards
    // also handles the mod-union table cards.
    card_table->Scan</* kClearCard */ false>(space->GetMarkBitmap(),
                                             space->Begin(),
                                             space->End(),
                                             visitor,
                                             gc::accounting::CardTable::kCardDirty);
    if (table != nullptr) {
      // Add the cards to the mod-union table so that we can clear cards to save RAM.
      table->ProcessCards();
      TimingLogger::ScopedTiming split2("(Paused)ClearCards", GetTimings());
      card_table->ClearCardRange(space->Begin(),
                                 AlignDown(space->End(), accounting::CardTable::kCardSize));
    }
  }
  // Since all of the objects that may point to other spaces are gray, we can avoid all the read
  // barriers in the immune spaces.
  updated_all_immune_objects_.StoreRelaxed(true);
}

void ConcurrentCopying::SwapStacks() {
  heap_->SwapStacks();
}

void ConcurrentCopying::RecordLiveStackFreezeSize(Thread* self) {
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  live_stack_freeze_size_ = heap_->GetLiveStack()->Size();
}

// Used to visit objects in the immune spaces.
inline void ConcurrentCopying::ScanImmuneObject(mirror::Object* obj) {
  DCHECK(obj != nullptr);
  DCHECK(immune_spaces_.ContainsObject(obj));
  // Update the fields without graying it or pushing it onto the mark stack.
  Scan(obj);
}

class ConcurrentCopying::ImmuneSpaceScanObjVisitor {
 public:
  explicit ImmuneSpaceScanObjVisitor(ConcurrentCopying* cc)
      : collector_(cc) {}

  ALWAYS_INLINE void operator()(mirror::Object* obj) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      // Only need to scan gray objects.
      if (obj->GetReadBarrierState() == ReadBarrier::GrayState()) {
        collector_->ScanImmuneObject(obj);
        // Done scanning the object, go back to white.
        bool success = obj->AtomicSetReadBarrierState(ReadBarrier::GrayState(),
                                                      ReadBarrier::WhiteState());
        CHECK(success)
            << Runtime::Current()->GetHeap()->GetVerification()->DumpObjectInfo(obj, "failed CAS");
      }
    } else {
      collector_->ScanImmuneObject(obj);
    }
  }

  static void Callback(mirror::Object* obj, void* arg) REQUIRES_SHARED(Locks::mutator_lock_) {
    reinterpret_cast<ImmuneSpaceScanObjVisitor*>(arg)->operator()(obj);
  }

 private:
  ConcurrentCopying* const collector_;
};

// Concurrently mark roots that are guarded by read barriers and process the mark stack.
void ConcurrentCopying::MarkingPhase() {
  TimingLogger::ScopedTiming split("MarkingPhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC MarkingPhase";
  }
  Thread* self = Thread::Current();
  if (kIsDebugBuild) {
    MutexLock mu(self, *Locks::thread_list_lock_);
    CHECK(weak_ref_access_enabled_);
  }

  // Scan immune spaces.
  // Update all the fields in the immune spaces first without graying the objects so that we
  // minimize dirty pages in the immune spaces. Note mutators can concurrently access and gray some
  // of the objects.
  if (kUseBakerReadBarrier) {
    gc_grays_immune_objects_ = false;
  }
  {
    TimingLogger::ScopedTiming split2("ScanImmuneSpaces", GetTimings());
    for (auto& space : immune_spaces_.GetSpaces()) {
      DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
      accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
      ImmuneSpaceScanObjVisitor visitor(this);
      if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects && table != nullptr) {
        table->VisitObjects(ImmuneSpaceScanObjVisitor::Callback, &visitor);
      } else {
        // TODO: Scan only the aged cards.
        live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(space->Begin()),
                                      reinterpret_cast<uintptr_t>(space->Limit()),
                                      visitor);
      }
    }
  }
  if (kUseBakerReadBarrier) {
    // This release fence makes the field updates in the above loop visible before allowing mutator
    // getting access to immune objects without graying it first.
    updated_all_immune_objects_.StoreRelease(true);
    // Now whiten immune objects concurrently accessed and grayed by mutators. We can't do this in
    // the above loop because we would incorrectly disable the read barrier by whitening an object
    // which may point to an unscanned, white object, breaking the to-space invariant.
    //
    // Make sure no mutators are in the middle of marking an immune object before whitening immune
    // objects.
    IssueEmptyCheckpoint();
    MutexLock mu(Thread::Current(), immune_gray_stack_lock_);
    if (kVerboseMode) {
      LOG(INFO) << "immune gray stack size=" << immune_gray_stack_.size();
    }
    for (mirror::Object* obj : immune_gray_stack_) {
      DCHECK(obj->GetReadBarrierState() == ReadBarrier::GrayState());
      bool success = obj->AtomicSetReadBarrierState(ReadBarrier::GrayState(),
                                                    ReadBarrier::WhiteState());
      DCHECK(success);
    }
    immune_gray_stack_.clear();
  }

  {
    TimingLogger::ScopedTiming split2("VisitConcurrentRoots", GetTimings());
    Runtime::Current()->VisitConcurrentRoots(this, kVisitRootFlagAllRoots);
  }
  {
    // TODO: don't visit the transaction roots if it's not active.
    TimingLogger::ScopedTiming split5("VisitNonThreadRoots", GetTimings());
    Runtime::Current()->VisitNonThreadRoots(this);
  }

  {
    TimingLogger::ScopedTiming split7("ProcessMarkStack", GetTimings());
    // We transition through three mark stack modes (thread-local, shared, GC-exclusive). The
    // primary reasons are the fact that we need to use a checkpoint to process thread-local mark
    // stacks, but after we disable weak refs accesses, we can't use a checkpoint due to a deadlock
    // issue because running threads potentially blocking at WaitHoldingLocks, and that once we
    // reach the point where we process weak references, we can avoid using a lock when accessing
    // the GC mark stack, which makes mark stack processing more efficient.

    // Process the mark stack once in the thread local stack mode. This marks most of the live
    // objects, aside from weak ref accesses with read barriers (Reference::GetReferent() and system
    // weaks) that may happen concurrently while we processing the mark stack and newly mark/gray
    // objects and push refs on the mark stack.
    ProcessMarkStack();
    // Switch to the shared mark stack mode. That is, revoke and process thread-local mark stacks
    // for the last time before transitioning to the shared mark stack mode, which would process new
    // refs that may have been concurrently pushed onto the mark stack during the ProcessMarkStack()
    // call above. At the same time, disable weak ref accesses using a per-thread flag. It's
    // important to do these together in a single checkpoint so that we can ensure that mutators
    // won't newly gray objects and push new refs onto the mark stack due to weak ref accesses and
    // mutators safely transition to the shared mark stack mode (without leaving unprocessed refs on
    // the thread-local mark stacks), without a race. This is why we use a thread-local weak ref
    // access flag Thread::tls32_.weak_ref_access_enabled_ instead of the global ones.
    SwitchToSharedMarkStackMode();
    CHECK(!self->GetWeakRefAccessEnabled());
    // Now that weak refs accesses are disabled, once we exhaust the shared mark stack again here
    // (which may be non-empty if there were refs found on thread-local mark stacks during the above
    // SwitchToSharedMarkStackMode() call), we won't have new refs to process, that is, mutators
    // (via read barriers) have no way to produce any more refs to process. Marking converges once
    // before we process weak refs below.
    ProcessMarkStack();
    CheckEmptyMarkStack();
    // Switch to the GC exclusive mark stack mode so that we can process the mark stack without a
    // lock from this point on.
    SwitchToGcExclusiveMarkStackMode();
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "ProcessReferences";
    }
    // Process weak references. This may produce new refs to process and have them processed via
    // ProcessMarkStack (in the GC exclusive mark stack mode).
    ProcessReferences(self);
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "SweepSystemWeaks";
    }
    SweepSystemWeaks(self);
    if (kVerboseMode) {
      LOG(INFO) << "SweepSystemWeaks done";
    }
    // Process the mark stack here one last time because the above SweepSystemWeaks() call may have
    // marked some objects (strings alive) as hash_set::Erase() can call the hash function for
    // arbitrary elements in the weak intern table in InternTable::Table::SweepWeaks().
    ProcessMarkStack();
    CheckEmptyMarkStack();
    // Re-enable weak ref accesses.
    ReenableWeakRefAccess(self);
    // Free data for class loaders that we unloaded.
    Runtime::Current()->GetClassLinker()->CleanupClassLoaders();
    // Marking is done. Disable marking.
    DisableMarking();
    if (kUseBakerReadBarrier) {
      ProcessFalseGrayStack();
    }
    CheckEmptyMarkStack();
  }

  if (kIsDebugBuild) {
    MutexLock mu(self, *Locks::thread_list_lock_);
    CHECK(weak_ref_access_enabled_);
  }
  if (kVerboseMode) {
    LOG(INFO) << "GC end of MarkingPhase";
  }
}

void ConcurrentCopying::ReenableWeakRefAccess(Thread* self) {
  if (kVerboseMode) {
    LOG(INFO) << "ReenableWeakRefAccess";
  }
  // Iterate all threads (don't need to or can't use a checkpoint) and re-enable weak ref access.
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    weak_ref_access_enabled_ = true;  // This is for new threads.
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      thread->SetWeakRefAccessEnabled(true);
    }
  }
  // Unblock blocking threads.
  GetHeap()->GetReferenceProcessor()->BroadcastForSlowPath(self);
  Runtime::Current()->BroadcastForNewSystemWeaks();
}

class ConcurrentCopying::DisableMarkingCheckpoint : public Closure {
 public:
  explicit DisableMarkingCheckpoint(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* thread) OVERRIDE NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    DCHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Disable the thread-local is_gc_marking flag.
    // Note a thread that has just started right before this checkpoint may have already this flag
    // set to false, which is ok.
    thread->SetIsGcMarkingAndUpdateEntrypoints(false);
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::DisableMarkingCallback : public Closure {
 public:
  explicit DisableMarkingCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* self ATTRIBUTE_UNUSED) OVERRIDE REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a race with ThreadList::Register().
    CHECK(concurrent_copying_->is_marking_);
    concurrent_copying_->is_marking_ = false;
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      CHECK(concurrent_copying_->is_using_read_barrier_entrypoints_);
      concurrent_copying_->is_using_read_barrier_entrypoints_ = false;
    } else {
      CHECK(!concurrent_copying_->is_using_read_barrier_entrypoints_);
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::IssueDisableMarkingCheckpoint() {
  Thread* self = Thread::Current();
  DisableMarkingCheckpoint check_point(this);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  DisableMarkingCallback dmc(this);
  size_t barrier_count = thread_list->RunCheckpoint(&check_point, &dmc);
  // If there are no threads to wait which implies that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  // Release locks then wait for all mutator threads to pass the barrier.
  Locks::mutator_lock_->SharedUnlock(self);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::DisableMarking() {
  // Use a checkpoint to turn off the global is_marking and the thread-local is_gc_marking flags and
  // to ensure no threads are still in the middle of a read barrier which may have a from-space ref
  // cached in a local variable.
  IssueDisableMarkingCheckpoint();
  if (kUseTableLookupReadBarrier) {
    heap_->rb_table_->ClearAll();
    DCHECK(heap_->rb_table_->IsAllCleared());
  }
  is_mark_stack_push_disallowed_.StoreSequentiallyConsistent(1);
  mark_stack_mode_.StoreSequentiallyConsistent(kMarkStackModeOff);
}

void ConcurrentCopying::PushOntoFalseGrayStack(mirror::Object* ref) {
  CHECK(kUseBakerReadBarrier);
  DCHECK(ref != nullptr);
  MutexLock mu(Thread::Current(), mark_stack_lock_);
  false_gray_stack_.push_back(ref);
}

void ConcurrentCopying::ProcessFalseGrayStack() {
  CHECK(kUseBakerReadBarrier);
  // Change the objects on the false gray stack from gray to white.
  MutexLock mu(Thread::Current(), mark_stack_lock_);
  for (mirror::Object* obj : false_gray_stack_) {
    DCHECK(IsMarked(obj));
    // The object could be white here if a thread got preempted after a success at the
    // AtomicSetReadBarrierState in Mark(), GC started marking through it (but not finished so
    // still gray), and the thread ran to register it onto the false gray stack.
    if (obj->GetReadBarrierState() == ReadBarrier::GrayState()) {
      bool success = obj->AtomicSetReadBarrierState(ReadBarrier::GrayState(),
                                                    ReadBarrier::WhiteState());
      DCHECK(success);
    }
  }
  false_gray_stack_.clear();
}

void ConcurrentCopying::IssueEmptyCheckpoint() {
  Thread* self = Thread::Current();
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  // Release locks then wait for all mutator threads to pass the barrier.
  Locks::mutator_lock_->SharedUnlock(self);
  thread_list->RunEmptyCheckpoint();
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::ExpandGcMarkStack() {
  DCHECK(gc_mark_stack_->IsFull());
  const size_t new_size = gc_mark_stack_->Capacity() * 2;
  std::vector<StackReference<mirror::Object>> temp(gc_mark_stack_->Begin(),
                                                   gc_mark_stack_->End());
  gc_mark_stack_->Resize(new_size);
  for (auto& ref : temp) {
    gc_mark_stack_->PushBack(ref.AsMirrorPtr());
  }
  DCHECK(!gc_mark_stack_->IsFull());
}

void ConcurrentCopying::PushOntoMarkStack(mirror::Object* to_ref) {
  CHECK_EQ(is_mark_stack_push_disallowed_.LoadRelaxed(), 0)
      << " " << to_ref << " " << mirror::Object::PrettyTypeOf(to_ref);
  Thread* self = Thread::Current();  // TODO: pass self as an argument from call sites?
  CHECK(thread_running_gc_ != nullptr);
  MarkStackMode mark_stack_mode = mark_stack_mode_.LoadRelaxed();
  if (LIKELY(mark_stack_mode == kMarkStackModeThreadLocal)) {
    if (LIKELY(self == thread_running_gc_)) {
      // If GC-running thread, use the GC mark stack instead of a thread-local mark stack.
      CHECK(self->GetThreadLocalMarkStack() == nullptr);
      if (UNLIKELY(gc_mark_stack_->IsFull())) {
        ExpandGcMarkStack();
      }
      gc_mark_stack_->PushBack(to_ref);
    } else {
      // Otherwise, use a thread-local mark stack.
      accounting::AtomicStack<mirror::Object>* tl_mark_stack = self->GetThreadLocalMarkStack();
      if (UNLIKELY(tl_mark_stack == nullptr || tl_mark_stack->IsFull())) {
        MutexLock mu(self, mark_stack_lock_);
        // Get a new thread local mark stack.
        accounting::AtomicStack<mirror::Object>* new_tl_mark_stack;
        if (!pooled_mark_stacks_.empty()) {
          // Use a pooled mark stack.
          new_tl_mark_stack = pooled_mark_stacks_.back();
          pooled_mark_stacks_.pop_back();
        } else {
          // None pooled. Create a new one.
          new_tl_mark_stack =
              accounting::AtomicStack<mirror::Object>::Create(
                  "thread local mark stack", 4 * KB, 4 * KB);
        }
        DCHECK(new_tl_mark_stack != nullptr);
        DCHECK(new_tl_mark_stack->IsEmpty());
        new_tl_mark_stack->PushBack(to_ref);
        self->SetThreadLocalMarkStack(new_tl_mark_stack);
        if (tl_mark_stack != nullptr) {
          // Store the old full stack into a vector.
          revoked_mark_stacks_.push_back(tl_mark_stack);
        }
      } else {
        tl_mark_stack->PushBack(to_ref);
      }
    }
  } else if (mark_stack_mode == kMarkStackModeShared) {
    // Access the shared GC mark stack with a lock.
    MutexLock mu(self, mark_stack_lock_);
    if (UNLIKELY(gc_mark_stack_->IsFull())) {
      ExpandGcMarkStack();
    }
    gc_mark_stack_->PushBack(to_ref);
  } else {
    CHECK_EQ(static_cast<uint32_t>(mark_stack_mode),
             static_cast<uint32_t>(kMarkStackModeGcExclusive))
        << "ref=" << to_ref
        << " self->gc_marking=" << self->GetIsGcMarking()
        << " cc->is_marking=" << is_marking_;
    CHECK(self == thread_running_gc_)
        << "Only GC-running thread should access the mark stack "
        << "in the GC exclusive mark stack mode";
    // Access the GC mark stack without a lock.
    if (UNLIKELY(gc_mark_stack_->IsFull())) {
      ExpandGcMarkStack();
    }
    gc_mark_stack_->PushBack(to_ref);
  }
}

accounting::ObjectStack* ConcurrentCopying::GetAllocationStack() {
  return heap_->allocation_stack_.get();
}

accounting::ObjectStack* ConcurrentCopying::GetLiveStack() {
  return heap_->live_stack_.get();
}

// The following visitors are used to verify that there's no references to the from-space left after
// marking.
class ConcurrentCopying::VerifyNoFromSpaceRefsVisitor : public SingleRootVisitor {
 public:
  explicit VerifyNoFromSpaceRefsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(mirror::Object* ref,
                  MemberOffset offset = MemberOffset(0),
                  mirror::Object* holder = nullptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    if (ref == nullptr) {
      // OK.
      return;
    }
    collector_->AssertToSpaceInvariant(holder, offset, ref);
    if (kUseBakerReadBarrier) {
      CHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::WhiteState())
          << "Ref " << ref << " " << ref->PrettyTypeOf()
          << " has non-white rb_state ";
    }
  }

  void VisitRoot(mirror::Object* root, const RootInfo& info ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(root != nullptr);
    operator()(root);
  }

 private:
  ConcurrentCopying* const collector_;
};

class ConcurrentCopying::VerifyNoFromSpaceRefsFieldVisitor {
 public:
  explicit VerifyNoFromSpaceRefsFieldVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(offset);
    VerifyNoFromSpaceRefsVisitor visitor(collector_);
    visitor(ref, offset, obj.Ptr());
  }
  void operator()(ObjPtr<mirror::Class> klass,
                  ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    VerifyNoFromSpaceRefsVisitor visitor(collector_);
    visitor(root->AsMirrorPtr());
  }

 private:
  ConcurrentCopying* const collector_;
};

// Verify there's no from-space references left after the marking phase.
void ConcurrentCopying::VerifyNoFromSpaceReferences() {
  Thread* self = Thread::Current();
  DCHECK(Locks::mutator_lock_->IsExclusiveHeld(self));
  // Verify all threads have is_gc_marking to be false
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      CHECK(!thread->GetIsGcMarking());
    }
  }

  auto verify_no_from_space_refs_visitor = [&](mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(obj != nullptr);
    space::RegionSpace* region_space = RegionSpace();
    CHECK(!region_space->IsInFromSpace(obj)) << "Scanning object " << obj << " in from space";
    VerifyNoFromSpaceRefsFieldVisitor visitor(this);
    obj->VisitReferences</*kVisitNativeRoots*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
        visitor,
        visitor);
    if (kUseBakerReadBarrier) {
      CHECK_EQ(obj->GetReadBarrierState(), ReadBarrier::WhiteState())
          << "obj=" << obj << " non-white rb_state " << obj->GetReadBarrierState();
    }
  };
  // Roots.
  {
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    VerifyNoFromSpaceRefsVisitor ref_visitor(this);
    Runtime::Current()->VisitRoots(&ref_visitor);
  }
  // The to-space.
  region_space_->WalkToSpace(verify_no_from_space_refs_visitor);
  // Non-moving spaces.
  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    heap_->GetMarkBitmap()->Visit(verify_no_from_space_refs_visitor);
  }
  // The alloc stack.
  {
    VerifyNoFromSpaceRefsVisitor ref_visitor(this);
    for (auto* it = heap_->allocation_stack_->Begin(), *end = heap_->allocation_stack_->End();
        it < end; ++it) {
      mirror::Object* const obj = it->AsMirrorPtr();
      if (obj != nullptr && obj->GetClass() != nullptr) {
        // TODO: need to call this only if obj is alive?
        ref_visitor(obj);
        verify_no_from_space_refs_visitor(obj);
      }
    }
  }
  // TODO: LOS. But only refs in LOS are classes.
}

// The following visitors are used to assert the to-space invariant.
class ConcurrentCopying::AssertToSpaceInvariantRefsVisitor {
 public:
  explicit AssertToSpaceInvariantRefsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(mirror::Object* ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    if (ref == nullptr) {
      // OK.
      return;
    }
    collector_->AssertToSpaceInvariant(nullptr, MemberOffset(0), ref);
  }

 private:
  ConcurrentCopying* const collector_;
};

class ConcurrentCopying::AssertToSpaceInvariantFieldVisitor {
 public:
  explicit AssertToSpaceInvariantFieldVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(offset);
    AssertToSpaceInvariantRefsVisitor visitor(collector_);
    visitor(ref);
  }
  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    AssertToSpaceInvariantRefsVisitor visitor(collector_);
    visitor(root->AsMirrorPtr());
  }

 private:
  ConcurrentCopying* const collector_;
};

class ConcurrentCopying::RevokeThreadLocalMarkStackCheckpoint : public Closure {
 public:
  RevokeThreadLocalMarkStackCheckpoint(ConcurrentCopying* concurrent_copying,
                                       bool disable_weak_ref_access)
      : concurrent_copying_(concurrent_copying),
        disable_weak_ref_access_(disable_weak_ref_access) {
  }

  virtual void Run(Thread* thread) OVERRIDE NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    CHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Revoke thread local mark stacks.
    accounting::AtomicStack<mirror::Object>* tl_mark_stack = thread->GetThreadLocalMarkStack();
    if (tl_mark_stack != nullptr) {
      MutexLock mu(self, concurrent_copying_->mark_stack_lock_);
      concurrent_copying_->revoked_mark_stacks_.push_back(tl_mark_stack);
      thread->SetThreadLocalMarkStack(nullptr);
    }
    // Disable weak ref access.
    if (disable_weak_ref_access_) {
      thread->SetWeakRefAccessEnabled(false);
    }
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
  const bool disable_weak_ref_access_;
};

void ConcurrentCopying::RevokeThreadLocalMarkStacks(bool disable_weak_ref_access,
                                                    Closure* checkpoint_callback) {
  Thread* self = Thread::Current();
  RevokeThreadLocalMarkStackCheckpoint check_point(this, disable_weak_ref_access);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  size_t barrier_count = thread_list->RunCheckpoint(&check_point, checkpoint_callback);
  // If there are no threads to wait which implys that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  Locks::mutator_lock_->SharedUnlock(self);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::RevokeThreadLocalMarkStack(Thread* thread) {
  Thread* self = Thread::Current();
  CHECK_EQ(self, thread);
  accounting::AtomicStack<mirror::Object>* tl_mark_stack = thread->GetThreadLocalMarkStack();
  if (tl_mark_stack != nullptr) {
    CHECK(is_marking_);
    MutexLock mu(self, mark_stack_lock_);
    revoked_mark_stacks_.push_back(tl_mark_stack);
    thread->SetThreadLocalMarkStack(nullptr);
  }
}

void ConcurrentCopying::ProcessMarkStack() {
  if (kVerboseMode) {
    LOG(INFO) << "ProcessMarkStack. ";
  }
  bool empty_prev = false;
  while (true) {
    bool empty = ProcessMarkStackOnce();
    if (empty_prev && empty) {
      // Saw empty mark stack for a second time, done.
      break;
    }
    empty_prev = empty;
  }
}

bool ConcurrentCopying::ProcessMarkStackOnce() {
  Thread* self = Thread::Current();
  CHECK(thread_running_gc_ != nullptr);
  CHECK(self == thread_running_gc_);
  CHECK(self->GetThreadLocalMarkStack() == nullptr);
  size_t count = 0;
  MarkStackMode mark_stack_mode = mark_stack_mode_.LoadRelaxed();
  if (mark_stack_mode == kMarkStackModeThreadLocal) {
    // Process the thread-local mark stacks and the GC mark stack.
    count += ProcessThreadLocalMarkStacks(/* disable_weak_ref_access */ false,
                                          /* checkpoint_callback */ nullptr);
    while (!gc_mark_stack_->IsEmpty()) {
      mirror::Object* to_ref = gc_mark_stack_->PopBack();
      ProcessMarkStackRef(to_ref);
      ++count;
    }
    gc_mark_stack_->Reset();
  } else if (mark_stack_mode == kMarkStackModeShared) {
    // Do an empty checkpoint to avoid a race with a mutator preempted in the middle of a read
    // barrier but before pushing onto the mark stack. b/32508093. Note the weak ref access is
    // disabled at this point.
    IssueEmptyCheckpoint();
    // Process the shared GC mark stack with a lock.
    {
      MutexLock mu(self, mark_stack_lock_);
      CHECK(revoked_mark_stacks_.empty());
    }
    while (true) {
      std::vector<mirror::Object*> refs;
      {
        // Copy refs with lock. Note the number of refs should be small.
        MutexLock mu(self, mark_stack_lock_);
        if (gc_mark_stack_->IsEmpty()) {
          break;
        }
        for (StackReference<mirror::Object>* p = gc_mark_stack_->Begin();
             p != gc_mark_stack_->End(); ++p) {
          refs.push_back(p->AsMirrorPtr());
        }
        gc_mark_stack_->Reset();
      }
      for (mirror::Object* ref : refs) {
        ProcessMarkStackRef(ref);
        ++count;
      }
    }
  } else {
    CHECK_EQ(static_cast<uint32_t>(mark_stack_mode),
             static_cast<uint32_t>(kMarkStackModeGcExclusive));
    {
      MutexLock mu(self, mark_stack_lock_);
      CHECK(revoked_mark_stacks_.empty());
    }
    // Process the GC mark stack in the exclusive mode. No need to take the lock.
    while (!gc_mark_stack_->IsEmpty()) {
      mirror::Object* to_ref = gc_mark_stack_->PopBack();
      ProcessMarkStackRef(to_ref);
      ++count;
    }
    gc_mark_stack_->Reset();
  }

  // Return true if the stack was empty.
  return count == 0;
}

size_t ConcurrentCopying::ProcessThreadLocalMarkStacks(bool disable_weak_ref_access,
                                                       Closure* checkpoint_callback) {
  // Run a checkpoint to collect all thread local mark stacks and iterate over them all.
  RevokeThreadLocalMarkStacks(disable_weak_ref_access, checkpoint_callback);
  size_t count = 0;
  std::vector<accounting::AtomicStack<mirror::Object>*> mark_stacks;
  {
    MutexLock mu(Thread::Current(), mark_stack_lock_);
    // Make a copy of the mark stack vector.
    mark_stacks = revoked_mark_stacks_;
    revoked_mark_stacks_.clear();
  }
  for (accounting::AtomicStack<mirror::Object>* mark_stack : mark_stacks) {
    for (StackReference<mirror::Object>* p = mark_stack->Begin(); p != mark_stack->End(); ++p) {
      mirror::Object* to_ref = p->AsMirrorPtr();
      ProcessMarkStackRef(to_ref);
      ++count;
    }
    {
      MutexLock mu(Thread::Current(), mark_stack_lock_);
      if (pooled_mark_stacks_.size() >= kMarkStackPoolSize) {
        // The pool has enough. Delete it.
        delete mark_stack;
      } else {
        // Otherwise, put it into the pool for later reuse.
        mark_stack->Reset();
        pooled_mark_stacks_.push_back(mark_stack);
      }
    }
  }
  return count;
}

inline void ConcurrentCopying::ProcessMarkStackRef(mirror::Object* to_ref) {
  DCHECK(!region_space_->IsInFromSpace(to_ref));
  if (kUseBakerReadBarrier) {
    DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState())
        << " " << to_ref << " " << to_ref->GetReadBarrierState()
        << " is_marked=" << IsMarked(to_ref);
  }
  bool add_to_live_bytes = false;
  if (region_space_->IsInUnevacFromSpace(to_ref)) {
    // Mark the bitmap only in the GC thread here so that we don't need a CAS.
    if (!kUseBakerReadBarrier || !region_space_bitmap_->Set(to_ref)) {
      // It may be already marked if we accidentally pushed the same object twice due to the racy
      // bitmap read in MarkUnevacFromSpaceRegion.
      Scan(to_ref);
      // Only add to the live bytes if the object was not already marked.
      add_to_live_bytes = true;
    }
  } else {
    Scan(to_ref);
  }
  if (kUseBakerReadBarrier) {
    DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState())
        << " " << to_ref << " " << to_ref->GetReadBarrierState()
        << " is_marked=" << IsMarked(to_ref);
  }
#ifdef USE_BAKER_OR_BROOKS_READ_BARRIER
  mirror::Object* referent = nullptr;
  if (UNLIKELY((to_ref->GetClass<kVerifyNone, kWithoutReadBarrier>()->IsTypeOfReferenceClass() &&
                (referent = to_ref->AsReference()->GetReferent<kWithoutReadBarrier>()) != nullptr &&
                !IsInToSpace(referent)))) {
    // Leave this reference gray in the queue so that GetReferent() will trigger a read barrier. We
    // will change it to white later in ReferenceQueue::DequeuePendingReference().
    DCHECK(to_ref->AsReference()->GetPendingNext() != nullptr)
        << "Left unenqueued ref gray " << to_ref;
  } else {
    // We may occasionally leave a reference white in the queue if its referent happens to be
    // concurrently marked after the Scan() call above has enqueued the Reference, in which case the
    // above IsInToSpace() evaluates to true and we change the color from gray to white here in this
    // else block.
    if (kUseBakerReadBarrier) {
      bool success = to_ref->AtomicSetReadBarrierState</*kCasRelease*/true>(
          ReadBarrier::GrayState(),
          ReadBarrier::WhiteState());
      DCHECK(success) << "Must succeed as we won the race.";
    }
  }
#else
  DCHECK(!kUseBakerReadBarrier);
#endif

  if (add_to_live_bytes) {
    // Add to the live bytes per unevacuated from-space. Note this code is always run by the
    // GC-running thread (no synchronization required).
    DCHECK(region_space_bitmap_->Test(to_ref));
    size_t obj_size = to_ref->SizeOf<kDefaultVerifyFlags>();
    size_t alloc_size = RoundUp(obj_size, space::RegionSpace::kAlignment);
    region_space_->AddLiveBytes(to_ref, alloc_size);
  }
  if (ReadBarrier::kEnableToSpaceInvariantChecks) {
    CHECK(to_ref != nullptr);
    space::RegionSpace* region_space = RegionSpace();
    CHECK(!region_space->IsInFromSpace(to_ref)) << "Scanning object " << to_ref << " in from space";
    AssertToSpaceInvariant(nullptr, MemberOffset(0), to_ref);
    AssertToSpaceInvariantFieldVisitor visitor(this);
    to_ref->VisitReferences</*kVisitNativeRoots*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
        visitor,
        visitor);
  }
}

class ConcurrentCopying::DisableWeakRefAccessCallback : public Closure {
 public:
  explicit DisableWeakRefAccessCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* self ATTRIBUTE_UNUSED) OVERRIDE REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a deadlock b/31500969.
    CHECK(concurrent_copying_->weak_ref_access_enabled_);
    concurrent_copying_->weak_ref_access_enabled_ = false;
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::SwitchToSharedMarkStackMode() {
  Thread* self = Thread::Current();
  CHECK(thread_running_gc_ != nullptr);
  CHECK_EQ(self, thread_running_gc_);
  CHECK(self->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode before_mark_stack_mode = mark_stack_mode_.LoadRelaxed();
  CHECK_EQ(static_cast<uint32_t>(before_mark_stack_mode),
           static_cast<uint32_t>(kMarkStackModeThreadLocal));
  mark_stack_mode_.StoreRelaxed(kMarkStackModeShared);
  DisableWeakRefAccessCallback dwrac(this);
  // Process the thread local mark stacks one last time after switching to the shared mark stack
  // mode and disable weak ref accesses.
  ProcessThreadLocalMarkStacks(/* disable_weak_ref_access */ true, &dwrac);
  if (kVerboseMode) {
    LOG(INFO) << "Switched to shared mark stack mode and disabled weak ref access";
  }
}

void ConcurrentCopying::SwitchToGcExclusiveMarkStackMode() {
  Thread* self = Thread::Current();
  CHECK(thread_running_gc_ != nullptr);
  CHECK_EQ(self, thread_running_gc_);
  CHECK(self->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode before_mark_stack_mode = mark_stack_mode_.LoadRelaxed();
  CHECK_EQ(static_cast<uint32_t>(before_mark_stack_mode),
           static_cast<uint32_t>(kMarkStackModeShared));
  mark_stack_mode_.StoreRelaxed(kMarkStackModeGcExclusive);
  QuasiAtomic::ThreadFenceForConstructor();
  if (kVerboseMode) {
    LOG(INFO) << "Switched to GC exclusive mark stack mode";
  }
}

void ConcurrentCopying::CheckEmptyMarkStack() {
  Thread* self = Thread::Current();
  CHECK(thread_running_gc_ != nullptr);
  CHECK_EQ(self, thread_running_gc_);
  CHECK(self->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode mark_stack_mode = mark_stack_mode_.LoadRelaxed();
  if (mark_stack_mode == kMarkStackModeThreadLocal) {
    // Thread-local mark stack mode.
    RevokeThreadLocalMarkStacks(false, nullptr);
    MutexLock mu(Thread::Current(), mark_stack_lock_);
    if (!revoked_mark_stacks_.empty()) {
      for (accounting::AtomicStack<mirror::Object>* mark_stack : revoked_mark_stacks_) {
        while (!mark_stack->IsEmpty()) {
          mirror::Object* obj = mark_stack->PopBack();
          if (kUseBakerReadBarrier) {
            uint32_t rb_state = obj->GetReadBarrierState();
            LOG(INFO) << "On mark queue : " << obj << " " << obj->PrettyTypeOf() << " rb_state="
                      << rb_state << " is_marked=" << IsMarked(obj);
          } else {
            LOG(INFO) << "On mark queue : " << obj << " " << obj->PrettyTypeOf()
                      << " is_marked=" << IsMarked(obj);
          }
        }
      }
      LOG(FATAL) << "mark stack is not empty";
    }
  } else {
    // Shared, GC-exclusive, or off.
    MutexLock mu(Thread::Current(), mark_stack_lock_);
    CHECK(gc_mark_stack_->IsEmpty());
    CHECK(revoked_mark_stacks_.empty());
  }
}

void ConcurrentCopying::SweepSystemWeaks(Thread* self) {
  TimingLogger::ScopedTiming split("SweepSystemWeaks", GetTimings());
  ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
  Runtime::Current()->SweepSystemWeaks(this);
}

void ConcurrentCopying::Sweep(bool swap_bitmaps) {
  {
    TimingLogger::ScopedTiming t("MarkStackAsLive", GetTimings());
    accounting::ObjectStack* live_stack = heap_->GetLiveStack();
    if (kEnableFromSpaceAccountingCheck) {
      CHECK_GE(live_stack_freeze_size_, live_stack->Size());
    }
    heap_->MarkAllocStackAsLive(live_stack);
    live_stack->Reset();
  }
  CheckEmptyMarkStack();
  TimingLogger::ScopedTiming split("Sweep", GetTimings());
  for (const auto& space : GetHeap()->GetContinuousSpaces()) {
    if (space->IsContinuousMemMapAllocSpace()) {
      space::ContinuousMemMapAllocSpace* alloc_space = space->AsContinuousMemMapAllocSpace();
      if (space == region_space_ || immune_spaces_.ContainsSpace(space)) {
        continue;
      }
      TimingLogger::ScopedTiming split2(
          alloc_space->IsZygoteSpace() ? "SweepZygoteSpace" : "SweepAllocSpace", GetTimings());
      RecordFree(alloc_space->Sweep(swap_bitmaps));
    }
  }
  SweepLargeObjects(swap_bitmaps);
}

void ConcurrentCopying::MarkZygoteLargeObjects() {
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  Thread* const self = Thread::Current();
  WriterMutexLock rmu(self, *Locks::heap_bitmap_lock_);
  space::LargeObjectSpace* const los = heap_->GetLargeObjectsSpace();
  if (los != nullptr) {
    // Pick the current live bitmap (mark bitmap if swapped).
    accounting::LargeObjectBitmap* const live_bitmap = los->GetLiveBitmap();
    accounting::LargeObjectBitmap* const mark_bitmap = los->GetMarkBitmap();
    // Walk through all of the objects and explicitly mark the zygote ones so they don't get swept.
    std::pair<uint8_t*, uint8_t*> range = los->GetBeginEndAtomic();
    live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(range.first),
                                  reinterpret_cast<uintptr_t>(range.second),
                                  [mark_bitmap, los, self](mirror::Object* obj)
        REQUIRES(Locks::heap_bitmap_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (los->IsZygoteLargeObject(self, obj)) {
        mark_bitmap->Set(obj);
      }
    });
  }
}

void ConcurrentCopying::SweepLargeObjects(bool swap_bitmaps) {
  TimingLogger::ScopedTiming split("SweepLargeObjects", GetTimings());
  if (heap_->GetLargeObjectsSpace() != nullptr) {
    RecordFreeLOS(heap_->GetLargeObjectsSpace()->Sweep(swap_bitmaps));
  }
}

void ConcurrentCopying::ReclaimPhase() {
  TimingLogger::ScopedTiming split("ReclaimPhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC ReclaimPhase";
  }
  Thread* self = Thread::Current();

  {
    // Double-check that the mark stack is empty.
    // Note: need to set this after VerifyNoFromSpaceRef().
    is_asserting_to_space_invariant_ = false;
    QuasiAtomic::ThreadFenceForConstructor();
    if (kVerboseMode) {
      LOG(INFO) << "Issue an empty check point. ";
    }
    IssueEmptyCheckpoint();
    // Disable the check.
    is_mark_stack_push_disallowed_.StoreSequentiallyConsistent(0);
    if (kUseBakerReadBarrier) {
      updated_all_immune_objects_.StoreSequentiallyConsistent(false);
    }
    CheckEmptyMarkStack();
  }

  {
    // Record freed objects.
    TimingLogger::ScopedTiming split2("RecordFree", GetTimings());
    // Don't include thread-locals that are in the to-space.
    const uint64_t from_bytes = region_space_->GetBytesAllocatedInFromSpace();
    const uint64_t from_objects = region_space_->GetObjectsAllocatedInFromSpace();
    const uint64_t unevac_from_bytes = region_space_->GetBytesAllocatedInUnevacFromSpace();
    const uint64_t unevac_from_objects = region_space_->GetObjectsAllocatedInUnevacFromSpace();
    uint64_t to_bytes = bytes_moved_.LoadSequentiallyConsistent();
    cumulative_bytes_moved_.FetchAndAddRelaxed(to_bytes);
    uint64_t to_objects = objects_moved_.LoadSequentiallyConsistent();
    cumulative_objects_moved_.FetchAndAddRelaxed(to_objects);
    if (kEnableFromSpaceAccountingCheck) {
      CHECK_EQ(from_space_num_objects_at_first_pause_, from_objects + unevac_from_objects);
      CHECK_EQ(from_space_num_bytes_at_first_pause_, from_bytes + unevac_from_bytes);
    }
    CHECK_LE(to_objects, from_objects);
    CHECK_LE(to_bytes, from_bytes);
    // Cleared bytes and objects, populated by the call to RegionSpace::ClearFromSpace below.
    uint64_t cleared_bytes;
    uint64_t cleared_objects;
    {
      TimingLogger::ScopedTiming split4("ClearFromSpace", GetTimings());
      region_space_->ClearFromSpace(&cleared_bytes, &cleared_objects);
      // `cleared_bytes` and `cleared_objects` may be greater than the from space equivalents since
      // RegionSpace::ClearFromSpace may clear empty unevac regions.
      CHECK_GE(cleared_bytes, from_bytes);
      CHECK_GE(cleared_objects, from_objects);
    }
    int64_t freed_bytes = cleared_bytes - to_bytes;
    int64_t freed_objects = cleared_objects - to_objects;
    if (kVerboseMode) {
      LOG(INFO) << "RecordFree:"
                << " from_bytes=" << from_bytes << " from_objects=" << from_objects
                << " unevac_from_bytes=" << unevac_from_bytes
                << " unevac_from_objects=" << unevac_from_objects
                << " to_bytes=" << to_bytes << " to_objects=" << to_objects
                << " freed_bytes=" << freed_bytes << " freed_objects=" << freed_objects
                << " from_space size=" << region_space_->FromSpaceSize()
                << " unevac_from_space size=" << region_space_->UnevacFromSpaceSize()
                << " to_space size=" << region_space_->ToSpaceSize();
      LOG(INFO) << "(before) num_bytes_allocated="
                << heap_->num_bytes_allocated_.LoadSequentiallyConsistent();
    }
    RecordFree(ObjectBytePair(freed_objects, freed_bytes));
    if (kVerboseMode) {
      LOG(INFO) << "(after) num_bytes_allocated="
                << heap_->num_bytes_allocated_.LoadSequentiallyConsistent();
    }
  }

  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    Sweep(false);
    SwapBitmaps();
    heap_->UnBindBitmaps();

    // The bitmap was cleared at the start of the GC, there is nothing we need to do here.
    DCHECK(region_space_bitmap_ != nullptr);
    region_space_bitmap_ = nullptr;
  }

  CheckEmptyMarkStack();

  if (kVerboseMode) {
    LOG(INFO) << "GC end of ReclaimPhase";
  }
}

std::string ConcurrentCopying::DumpReferenceInfo(mirror::Object* ref,
                                                 const char* ref_name,
                                                 std::string indent) {
  std::ostringstream oss;
  oss << indent << heap_->GetVerification()->DumpObjectInfo(ref, ref_name) << '\n';
  if (ref != nullptr) {
    if (kUseBakerReadBarrier) {
      oss << indent << ref_name << "->GetMarkBit()=" << ref->GetMarkBit() << '\n';
      oss << indent << ref_name << "->GetReadBarrierState()=" << ref->GetReadBarrierState() << '\n';
    }
  }
  if (region_space_->HasAddress(ref)) {
    oss << indent << "Region containing " << ref_name << ":" << '\n';
    region_space_->DumpRegionForObject(oss, ref);
    if (region_space_bitmap_ != nullptr) {
      oss << indent << "region_space_bitmap_->Test(" << ref_name << ")="
          << std::boolalpha << region_space_bitmap_->Test(ref) << std::noboolalpha;
    }
  }
  return oss.str();
}

std::string ConcurrentCopying::DumpHeapReference(mirror::Object* obj,
                                                 MemberOffset offset,
                                                 mirror::Object* ref) {
  std::ostringstream oss;
  std::string indent = "  ";
  oss << indent << "Invalid reference: ref=" << ref
      << " referenced from: object=" << obj << " offset= " << offset << '\n';
  // Information about `obj`.
  oss << DumpReferenceInfo(obj, "obj", indent) << '\n';
  // Information about `ref`.
  oss << DumpReferenceInfo(ref, "ref", indent);
  return oss.str();
}

void ConcurrentCopying::AssertToSpaceInvariant(mirror::Object* obj,
                                               MemberOffset offset,
                                               mirror::Object* ref) {
  CHECK_EQ(heap_->collector_type_, kCollectorTypeCC) << static_cast<size_t>(heap_->collector_type_);
  if (is_asserting_to_space_invariant_) {
    if (region_space_->HasAddress(ref)) {
      // Check to-space invariant in region space (moving space).
      using RegionType = space::RegionSpace::RegionType;
      space::RegionSpace::RegionType type = region_space_->GetRegionTypeUnsafe(ref);
      if (type == RegionType::kRegionTypeToSpace) {
        // OK.
        return;
      } else if (type == RegionType::kRegionTypeUnevacFromSpace) {
        if (!IsMarkedInUnevacFromSpace(ref)) {
          LOG(FATAL_WITHOUT_ABORT) << "Found unmarked reference in unevac from-space:";
          LOG(FATAL_WITHOUT_ABORT) << DumpHeapReference(obj, offset, ref);
        }
        CHECK(IsMarkedInUnevacFromSpace(ref)) << ref;
     } else {
        // Not OK: either a from-space ref or a reference in an unused region.
        // Do extra logging.
        if (type == RegionType::kRegionTypeFromSpace) {
          LOG(FATAL_WITHOUT_ABORT) << "Found from-space reference:";
        } else {
          LOG(FATAL_WITHOUT_ABORT) << "Found reference in region with type " << type << ":";
        }
        LOG(FATAL_WITHOUT_ABORT) << DumpHeapReference(obj, offset, ref);
        if (obj != nullptr) {
          LogFromSpaceRefHolder(obj, offset);
        }
        ref->GetLockWord(false).Dump(LOG_STREAM(FATAL_WITHOUT_ABORT));
        LOG(FATAL_WITHOUT_ABORT) << "Non-free regions:";
        region_space_->DumpNonFreeRegions(LOG_STREAM(FATAL_WITHOUT_ABORT));
        PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
        MemMap::DumpMaps(LOG_STREAM(FATAL_WITHOUT_ABORT), true);
        LOG(FATAL) << "Invalid reference " << ref
                   << " referenced from object " << obj << " at offset " << offset;
      }
    } else {
      // Check to-space invariant in non-moving space.
      AssertToSpaceInvariantInNonMovingSpace(obj, ref);
    }
  }
}

class RootPrinter {
 public:
  RootPrinter() { }

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<MirrorType>* root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  void VisitRoot(mirror::Object** root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL_WITHOUT_ABORT) << "root=" << root << " ref=" << *root;
  }

  template <class MirrorType>
  void VisitRoot(mirror::CompressedReference<MirrorType>* root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL_WITHOUT_ABORT) << "root=" << root << " ref=" << root->AsMirrorPtr();
  }
};

std::string ConcurrentCopying::DumpGcRoot(mirror::Object* ref) {
  std::ostringstream oss;
  std::string indent = "  ";
  oss << indent << "Invalid GC root: ref=" << ref << '\n';
  // Information about `ref`.
  oss << DumpReferenceInfo(ref, "ref", indent);
  return oss.str();
}

void ConcurrentCopying::AssertToSpaceInvariant(GcRootSource* gc_root_source,
                                               mirror::Object* ref) {
  CHECK_EQ(heap_->collector_type_, kCollectorTypeCC) << static_cast<size_t>(heap_->collector_type_);
  if (is_asserting_to_space_invariant_) {
    if (region_space_->HasAddress(ref)) {
      // Check to-space invariant in region space (moving space).
      using RegionType = space::RegionSpace::RegionType;
      space::RegionSpace::RegionType type = region_space_->GetRegionTypeUnsafe(ref);
      if (type == RegionType::kRegionTypeToSpace) {
        // OK.
        return;
      } else if (type == RegionType::kRegionTypeUnevacFromSpace) {
        if (!IsMarkedInUnevacFromSpace(ref)) {
          LOG(FATAL_WITHOUT_ABORT) << "Found unmarked reference in unevac from-space:";
          LOG(FATAL_WITHOUT_ABORT) << DumpGcRoot(ref);
        }
        CHECK(IsMarkedInUnevacFromSpace(ref)) << ref;
      } else {
        // Not OK: either a from-space ref or a reference in an unused region.
        // Do extra logging.
        if (type == RegionType::kRegionTypeFromSpace) {
          LOG(FATAL_WITHOUT_ABORT) << "Found from-space reference:";
        } else {
          LOG(FATAL_WITHOUT_ABORT) << "Found reference in region with type " << type << ":";
        }
        LOG(FATAL_WITHOUT_ABORT) << DumpGcRoot(ref);
        if (gc_root_source == nullptr) {
          // No info.
        } else if (gc_root_source->HasArtField()) {
          ArtField* field = gc_root_source->GetArtField();
          LOG(FATAL_WITHOUT_ABORT) << "gc root in field " << field << " "
                                   << ArtField::PrettyField(field);
          RootPrinter root_printer;
          field->VisitRoots(root_printer);
        } else if (gc_root_source->HasArtMethod()) {
          ArtMethod* method = gc_root_source->GetArtMethod();
          LOG(FATAL_WITHOUT_ABORT) << "gc root in method " << method << " "
                                   << ArtMethod::PrettyMethod(method);
          RootPrinter root_printer;
          method->VisitRoots(root_printer, kRuntimePointerSize);
        }
        ref->GetLockWord(false).Dump(LOG_STREAM(FATAL_WITHOUT_ABORT));
        LOG(FATAL_WITHOUT_ABORT) << "Non-free regions:";
        region_space_->DumpNonFreeRegions(LOG_STREAM(FATAL_WITHOUT_ABORT));
        PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
        MemMap::DumpMaps(LOG_STREAM(FATAL_WITHOUT_ABORT), true);
        LOG(FATAL) << "Invalid reference " << ref;
      }
    } else {
      // Check to-space invariant in non-moving space.
      AssertToSpaceInvariantInNonMovingSpace(/* obj */ nullptr, ref);
    }
  }
}

void ConcurrentCopying::LogFromSpaceRefHolder(mirror::Object* obj, MemberOffset offset) {
  if (kUseBakerReadBarrier) {
    LOG(INFO) << "holder=" << obj << " " << obj->PrettyTypeOf()
              << " holder rb_state=" << obj->GetReadBarrierState();
  } else {
    LOG(INFO) << "holder=" << obj << " " << obj->PrettyTypeOf();
  }
  if (region_space_->IsInFromSpace(obj)) {
    LOG(INFO) << "holder is in the from-space.";
  } else if (region_space_->IsInToSpace(obj)) {
    LOG(INFO) << "holder is in the to-space.";
  } else if (region_space_->IsInUnevacFromSpace(obj)) {
    LOG(INFO) << "holder is in the unevac from-space.";
    if (IsMarkedInUnevacFromSpace(obj)) {
      LOG(INFO) << "holder is marked in the region space bitmap.";
    } else {
      LOG(INFO) << "holder is not marked in the region space bitmap.";
    }
  } else {
    // In a non-moving space.
    if (immune_spaces_.ContainsObject(obj)) {
      LOG(INFO) << "holder is in an immune image or the zygote space.";
    } else {
      LOG(INFO) << "holder is in a non-immune, non-moving (or main) space.";
      accounting::ContinuousSpaceBitmap* mark_bitmap =
          heap_mark_bitmap_->GetContinuousSpaceBitmap(obj);
      accounting::LargeObjectBitmap* los_bitmap =
          heap_mark_bitmap_->GetLargeObjectBitmap(obj);
      CHECK(los_bitmap != nullptr) << "LOS bitmap covers the entire address range";
      bool is_los = mark_bitmap == nullptr;
      if (!is_los && mark_bitmap->Test(obj)) {
        LOG(INFO) << "holder is marked in the mark bit map.";
      } else if (is_los && los_bitmap->Test(obj)) {
        LOG(INFO) << "holder is marked in the los bit map.";
      } else {
        // If ref is on the allocation stack, then it is considered
        // mark/alive (but not necessarily on the live stack.)
        if (IsOnAllocStack(obj)) {
          LOG(INFO) << "holder is on the alloc stack.";
        } else {
          LOG(INFO) << "holder is not marked or on the alloc stack.";
        }
      }
    }
  }
  LOG(INFO) << "offset=" << offset.SizeValue();
}

void ConcurrentCopying::AssertToSpaceInvariantInNonMovingSpace(mirror::Object* obj,
                                                               mirror::Object* ref) {
  CHECK(!region_space_->HasAddress(ref)) << "obj=" << obj << " ref=" << ref;
  // In a non-moving space. Check that the ref is marked.
  if (immune_spaces_.ContainsObject(ref)) {
    if (kUseBakerReadBarrier) {
      // Immune object may not be gray if called from the GC.
      if (Thread::Current() == thread_running_gc_ && !gc_grays_immune_objects_) {
        return;
      }
      bool updated_all_immune_objects = updated_all_immune_objects_.LoadSequentiallyConsistent();
      CHECK(updated_all_immune_objects || ref->GetReadBarrierState() == ReadBarrier::GrayState())
          << "Unmarked immune space ref. obj=" << obj << " rb_state="
          << (obj != nullptr ? obj->GetReadBarrierState() : 0U)
          << " ref=" << ref << " ref rb_state=" << ref->GetReadBarrierState()
          << " updated_all_immune_objects=" << updated_all_immune_objects;
    }
  } else {
    accounting::ContinuousSpaceBitmap* mark_bitmap =
        heap_mark_bitmap_->GetContinuousSpaceBitmap(ref);
    accounting::LargeObjectBitmap* los_bitmap =
        heap_mark_bitmap_->GetLargeObjectBitmap(ref);
    bool is_los = mark_bitmap == nullptr;
    if ((!is_los && mark_bitmap->Test(ref)) ||
        (is_los && los_bitmap->Test(ref))) {
      // OK.
    } else {
      // If `ref` is on the allocation stack, then it may not be
      // marked live, but considered marked/alive (but not
      // necessarily on the live stack).
      CHECK(IsOnAllocStack(ref)) << "Unmarked ref that's not on the allocation stack."
                                 << " obj=" << obj
                                 << " ref=" << ref
                                 << " is_los=" << std::boolalpha << is_los << std::noboolalpha;
    }
  }
}

// Used to scan ref fields of an object.
class ConcurrentCopying::RefFieldsVisitor {
 public:
  explicit RefFieldsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(mirror::Object* obj, MemberOffset offset, bool /* is_static */)
      const ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_) {
    collector_->Process(obj, offset);
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    collector_->DelayReferenceReferent(klass, ref);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    collector_->MarkRoot</*kGrayImmuneObject*/false>(root);
  }

 private:
  ConcurrentCopying* const collector_;
};

inline void ConcurrentCopying::Scan(mirror::Object* to_ref) {
  if (kDisallowReadBarrierDuringScan && !Runtime::Current()->IsActiveTransaction()) {
    // Avoid all read barriers during visit references to help performance.
    // Don't do this in transaction mode because we may read the old value of an field which may
    // trigger read barriers.
    Thread::Current()->ModifyDebugDisallowReadBarrier(1);
  }
  DCHECK(!region_space_->IsInFromSpace(to_ref));
  DCHECK_EQ(Thread::Current(), thread_running_gc_);
  RefFieldsVisitor visitor(this);
  // Disable the read barrier for a performance reason.
  to_ref->VisitReferences</*kVisitNativeRoots*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
      visitor, visitor);
  if (kDisallowReadBarrierDuringScan && !Runtime::Current()->IsActiveTransaction()) {
    Thread::Current()->ModifyDebugDisallowReadBarrier(-1);
  }
}

inline void ConcurrentCopying::Process(mirror::Object* obj, MemberOffset offset) {
  DCHECK_EQ(Thread::Current(), thread_running_gc_);
  mirror::Object* ref = obj->GetFieldObject<
      mirror::Object, kVerifyNone, kWithoutReadBarrier, false>(offset);
  mirror::Object* to_ref = Mark</*kGrayImmuneObject*/false, /*kFromGCThread*/true>(
      ref,
      /*holder*/ obj,
      offset);
  if (to_ref == ref) {
    return;
  }
  // This may fail if the mutator writes to the field at the same time. But it's ok.
  mirror::Object* expected_ref = ref;
  mirror::Object* new_ref = to_ref;
  do {
    if (expected_ref !=
        obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier, false>(offset)) {
      // It was updated by the mutator.
      break;
    }
    // Use release CAS to make sure threads reading the reference see contents of copied objects.
  } while (!obj->CasFieldWeakReleaseObjectWithoutWriteBarrier<false, false, kVerifyNone>(
      offset,
      expected_ref,
      new_ref));
}

// Process some roots.
inline void ConcurrentCopying::VisitRoots(
    mirror::Object*** roots, size_t count, const RootInfo& info ATTRIBUTE_UNUSED) {
  for (size_t i = 0; i < count; ++i) {
    mirror::Object** root = roots[i];
    mirror::Object* ref = *root;
    mirror::Object* to_ref = Mark(ref);
    if (to_ref == ref) {
      continue;
    }
    Atomic<mirror::Object*>* addr = reinterpret_cast<Atomic<mirror::Object*>*>(root);
    mirror::Object* expected_ref = ref;
    mirror::Object* new_ref = to_ref;
    do {
      if (expected_ref != addr->LoadRelaxed()) {
        // It was updated by the mutator.
        break;
      }
    } while (!addr->CompareAndSetWeakRelaxed(expected_ref, new_ref));
  }
}

template<bool kGrayImmuneObject>
inline void ConcurrentCopying::MarkRoot(mirror::CompressedReference<mirror::Object>* root) {
  DCHECK(!root->IsNull());
  mirror::Object* const ref = root->AsMirrorPtr();
  mirror::Object* to_ref = Mark<kGrayImmuneObject>(ref);
  if (to_ref != ref) {
    auto* addr = reinterpret_cast<Atomic<mirror::CompressedReference<mirror::Object>>*>(root);
    auto expected_ref = mirror::CompressedReference<mirror::Object>::FromMirrorPtr(ref);
    auto new_ref = mirror::CompressedReference<mirror::Object>::FromMirrorPtr(to_ref);
    // If the cas fails, then it was updated by the mutator.
    do {
      if (ref != addr->LoadRelaxed().AsMirrorPtr()) {
        // It was updated by the mutator.
        break;
      }
    } while (!addr->CompareAndSetWeakRelaxed(expected_ref, new_ref));
  }
}

inline void ConcurrentCopying::VisitRoots(
    mirror::CompressedReference<mirror::Object>** roots, size_t count,
    const RootInfo& info ATTRIBUTE_UNUSED) {
  for (size_t i = 0; i < count; ++i) {
    mirror::CompressedReference<mirror::Object>* const root = roots[i];
    if (!root->IsNull()) {
      // kGrayImmuneObject is true because this is used for the thread flip.
      MarkRoot</*kGrayImmuneObject*/true>(root);
    }
  }
}

// Temporary set gc_grays_immune_objects_ to true in a scope if the current thread is GC.
class ConcurrentCopying::ScopedGcGraysImmuneObjects {
 public:
  explicit ScopedGcGraysImmuneObjects(ConcurrentCopying* collector)
      : collector_(collector), enabled_(false) {
    if (kUseBakerReadBarrier &&
        collector_->thread_running_gc_ == Thread::Current() &&
        !collector_->gc_grays_immune_objects_) {
      collector_->gc_grays_immune_objects_ = true;
      enabled_ = true;
    }
  }

  ~ScopedGcGraysImmuneObjects() {
    if (kUseBakerReadBarrier &&
        collector_->thread_running_gc_ == Thread::Current() &&
        enabled_) {
      DCHECK(collector_->gc_grays_immune_objects_);
      collector_->gc_grays_immune_objects_ = false;
    }
  }

 private:
  ConcurrentCopying* const collector_;
  bool enabled_;
};

// Fill the given memory block with a dummy object. Used to fill in a
// copy of objects that was lost in race.
void ConcurrentCopying::FillWithDummyObject(mirror::Object* dummy_obj, size_t byte_size) {
  // GC doesn't gray immune objects while scanning immune objects. But we need to trigger the read
  // barriers here because we need the updated reference to the int array class, etc. Temporary set
  // gc_grays_immune_objects_ to true so that we won't cause a DCHECK failure in MarkImmuneSpace().
  ScopedGcGraysImmuneObjects scoped_gc_gray_immune_objects(this);
  CHECK_ALIGNED(byte_size, kObjectAlignment);
  memset(dummy_obj, 0, byte_size);
  // Avoid going through read barrier for since kDisallowReadBarrierDuringScan may be enabled.
  // Explicitly mark to make sure to get an object in the to-space.
  mirror::Class* int_array_class = down_cast<mirror::Class*>(
      Mark(mirror::IntArray::GetArrayClass<kWithoutReadBarrier>()));
  CHECK(int_array_class != nullptr);
  if (ReadBarrier::kEnableToSpaceInvariantChecks) {
    AssertToSpaceInvariant(nullptr, MemberOffset(0), int_array_class);
  }
  size_t component_size = int_array_class->GetComponentSize<kWithoutReadBarrier>();
  CHECK_EQ(component_size, sizeof(int32_t));
  size_t data_offset = mirror::Array::DataOffset(component_size).SizeValue();
  if (data_offset > byte_size) {
    // An int array is too big. Use java.lang.Object.
    CHECK(java_lang_Object_ != nullptr);
    if (ReadBarrier::kEnableToSpaceInvariantChecks) {
      AssertToSpaceInvariant(nullptr, MemberOffset(0), java_lang_Object_);
    }
    CHECK_EQ(byte_size, (java_lang_Object_->GetObjectSize<kVerifyNone, kWithoutReadBarrier>()));
    dummy_obj->SetClass(java_lang_Object_);
    CHECK_EQ(byte_size, (dummy_obj->SizeOf<kVerifyNone>()));
  } else {
    // Use an int array.
    dummy_obj->SetClass(int_array_class);
    CHECK((dummy_obj->IsArrayInstance<kVerifyNone, kWithoutReadBarrier>()));
    int32_t length = (byte_size - data_offset) / component_size;
    mirror::Array* dummy_arr = dummy_obj->AsArray<kVerifyNone, kWithoutReadBarrier>();
    dummy_arr->SetLength(length);
    CHECK_EQ(dummy_arr->GetLength(), length)
        << "byte_size=" << byte_size << " length=" << length
        << " component_size=" << component_size << " data_offset=" << data_offset;
    CHECK_EQ(byte_size, (dummy_obj->SizeOf<kVerifyNone>()))
        << "byte_size=" << byte_size << " length=" << length
        << " component_size=" << component_size << " data_offset=" << data_offset;
  }
}

// Reuse the memory blocks that were copy of objects that were lost in race.
mirror::Object* ConcurrentCopying::AllocateInSkippedBlock(size_t alloc_size) {
  // Try to reuse the blocks that were unused due to CAS failures.
  CHECK_ALIGNED(alloc_size, space::RegionSpace::kAlignment);
  Thread* self = Thread::Current();
  size_t min_object_size = RoundUp(sizeof(mirror::Object), space::RegionSpace::kAlignment);
  size_t byte_size;
  uint8_t* addr;
  {
    MutexLock mu(self, skipped_blocks_lock_);
    auto it = skipped_blocks_map_.lower_bound(alloc_size);
    if (it == skipped_blocks_map_.end()) {
      // Not found.
      return nullptr;
    }
    byte_size = it->first;
    CHECK_GE(byte_size, alloc_size);
    if (byte_size > alloc_size && byte_size - alloc_size < min_object_size) {
      // If remainder would be too small for a dummy object, retry with a larger request size.
      it = skipped_blocks_map_.lower_bound(alloc_size + min_object_size);
      if (it == skipped_blocks_map_.end()) {
        // Not found.
        return nullptr;
      }
      CHECK_ALIGNED(it->first - alloc_size, space::RegionSpace::kAlignment);
      CHECK_GE(it->first - alloc_size, min_object_size)
          << "byte_size=" << byte_size << " it->first=" << it->first << " alloc_size=" << alloc_size;
    }
    // Found a block.
    CHECK(it != skipped_blocks_map_.end());
    byte_size = it->first;
    addr = it->second;
    CHECK_GE(byte_size, alloc_size);
    CHECK(region_space_->IsInToSpace(reinterpret_cast<mirror::Object*>(addr)));
    CHECK_ALIGNED(byte_size, space::RegionSpace::kAlignment);
    if (kVerboseMode) {
      LOG(INFO) << "Reusing skipped bytes : " << reinterpret_cast<void*>(addr) << ", " << byte_size;
    }
    skipped_blocks_map_.erase(it);
  }
  memset(addr, 0, byte_size);
  if (byte_size > alloc_size) {
    // Return the remainder to the map.
    CHECK_ALIGNED(byte_size - alloc_size, space::RegionSpace::kAlignment);
    CHECK_GE(byte_size - alloc_size, min_object_size);
    // FillWithDummyObject may mark an object, avoid holding skipped_blocks_lock_ to prevent lock
    // violation and possible deadlock. The deadlock case is a recursive case:
    // FillWithDummyObject -> IntArray::GetArrayClass -> Mark -> Copy -> AllocateInSkippedBlock.
    FillWithDummyObject(reinterpret_cast<mirror::Object*>(addr + alloc_size),
                        byte_size - alloc_size);
    CHECK(region_space_->IsInToSpace(reinterpret_cast<mirror::Object*>(addr + alloc_size)));
    {
      MutexLock mu(self, skipped_blocks_lock_);
      skipped_blocks_map_.insert(std::make_pair(byte_size - alloc_size, addr + alloc_size));
    }
  }
  return reinterpret_cast<mirror::Object*>(addr);
}

mirror::Object* ConcurrentCopying::Copy(mirror::Object* from_ref,
                                        mirror::Object* holder,
                                        MemberOffset offset) {
  DCHECK(region_space_->IsInFromSpace(from_ref));
  // If the class pointer is null, the object is invalid. This could occur for a dangling pointer
  // from a previous GC that is either inside or outside the allocated region.
  mirror::Class* klass = from_ref->GetClass<kVerifyNone, kWithoutReadBarrier>();
  if (UNLIKELY(klass == nullptr)) {
    heap_->GetVerification()->LogHeapCorruption(holder, offset, from_ref, /* fatal */ true);
  }
  // There must not be a read barrier to avoid nested RB that might violate the to-space invariant.
  // Note that from_ref is a from space ref so the SizeOf() call will access the from-space meta
  // objects, but it's ok and necessary.
  size_t obj_size = from_ref->SizeOf<kDefaultVerifyFlags>();
  size_t region_space_alloc_size = (obj_size <= space::RegionSpace::kRegionSize)
      ? RoundUp(obj_size, space::RegionSpace::kAlignment)
      : RoundUp(obj_size, space::RegionSpace::kRegionSize);
  size_t region_space_bytes_allocated = 0U;
  size_t non_moving_space_bytes_allocated = 0U;
  size_t bytes_allocated = 0U;
  size_t dummy;
  bool fall_back_to_non_moving = false;
  mirror::Object* to_ref = region_space_->AllocNonvirtual</*kForEvac*/ true>(
      region_space_alloc_size, &region_space_bytes_allocated, nullptr, &dummy);
  bytes_allocated = region_space_bytes_allocated;
  if (LIKELY(to_ref != nullptr)) {
    DCHECK_EQ(region_space_alloc_size, region_space_bytes_allocated);
  } else {
    // Failed to allocate in the region space. Try the skipped blocks.
    to_ref = AllocateInSkippedBlock(region_space_alloc_size);
    if (to_ref != nullptr) {
      // Succeeded to allocate in a skipped block.
      if (heap_->use_tlab_) {
        // This is necessary for the tlab case as it's not accounted in the space.
        region_space_->RecordAlloc(to_ref);
      }
      bytes_allocated = region_space_alloc_size;
      heap_->num_bytes_allocated_.fetch_sub(bytes_allocated, std::memory_order_seq_cst);
      to_space_bytes_skipped_.fetch_sub(bytes_allocated, std::memory_order_seq_cst);
      to_space_objects_skipped_.fetch_sub(1, std::memory_order_seq_cst);
    } else {
      // Fall back to the non-moving space.
      fall_back_to_non_moving = true;
      if (kVerboseMode) {
        LOG(INFO) << "Out of memory in the to-space. Fall back to non-moving. skipped_bytes="
                  << to_space_bytes_skipped_.LoadSequentiallyConsistent()
                  << " skipped_objects=" << to_space_objects_skipped_.LoadSequentiallyConsistent();
      }
      to_ref = heap_->non_moving_space_->Alloc(Thread::Current(), obj_size,
                                               &non_moving_space_bytes_allocated, nullptr, &dummy);
      if (UNLIKELY(to_ref == nullptr)) {
        LOG(FATAL_WITHOUT_ABORT) << "Fall-back non-moving space allocation failed for a "
                                 << obj_size << " byte object in region type "
                                 << region_space_->GetRegionType(from_ref);
        LOG(FATAL) << "Object address=" << from_ref << " type=" << from_ref->PrettyTypeOf();
      }
      bytes_allocated = non_moving_space_bytes_allocated;
      // Mark it in the mark bitmap.
      accounting::ContinuousSpaceBitmap* mark_bitmap =
          heap_mark_bitmap_->GetContinuousSpaceBitmap(to_ref);
      CHECK(mark_bitmap != nullptr);
      CHECK(!mark_bitmap->AtomicTestAndSet(to_ref));
    }
  }
  DCHECK(to_ref != nullptr);

  // Copy the object excluding the lock word since that is handled in the loop.
  to_ref->SetClass(klass);
  const size_t kObjectHeaderSize = sizeof(mirror::Object);
  DCHECK_GE(obj_size, kObjectHeaderSize);
  static_assert(kObjectHeaderSize == sizeof(mirror::HeapReference<mirror::Class>) +
                    sizeof(LockWord),
                "Object header size does not match");
  // Memcpy can tear for words since it may do byte copy. It is only safe to do this since the
  // object in the from space is immutable other than the lock word. b/31423258
  memcpy(reinterpret_cast<uint8_t*>(to_ref) + kObjectHeaderSize,
         reinterpret_cast<const uint8_t*>(from_ref) + kObjectHeaderSize,
         obj_size - kObjectHeaderSize);

  // Attempt to install the forward pointer. This is in a loop as the
  // lock word atomic write can fail.
  while (true) {
    LockWord old_lock_word = from_ref->GetLockWord(false);

    if (old_lock_word.GetState() == LockWord::kForwardingAddress) {
      // Lost the race. Another thread (either GC or mutator) stored
      // the forwarding pointer first. Make the lost copy (to_ref)
      // look like a valid but dead (dummy) object and keep it for
      // future reuse.
      FillWithDummyObject(to_ref, bytes_allocated);
      if (!fall_back_to_non_moving) {
        DCHECK(region_space_->IsInToSpace(to_ref));
        if (bytes_allocated > space::RegionSpace::kRegionSize) {
          // Free the large alloc.
          region_space_->FreeLarge</*kForEvac*/ true>(to_ref, bytes_allocated);
        } else {
          // Record the lost copy for later reuse.
          heap_->num_bytes_allocated_.FetchAndAddSequentiallyConsistent(bytes_allocated);
          to_space_bytes_skipped_.FetchAndAddSequentiallyConsistent(bytes_allocated);
          to_space_objects_skipped_.FetchAndAddSequentiallyConsistent(1);
          MutexLock mu(Thread::Current(), skipped_blocks_lock_);
          skipped_blocks_map_.insert(std::make_pair(bytes_allocated,
                                                    reinterpret_cast<uint8_t*>(to_ref)));
        }
      } else {
        DCHECK(heap_->non_moving_space_->HasAddress(to_ref));
        DCHECK_EQ(bytes_allocated, non_moving_space_bytes_allocated);
        // Free the non-moving-space chunk.
        accounting::ContinuousSpaceBitmap* mark_bitmap =
            heap_mark_bitmap_->GetContinuousSpaceBitmap(to_ref);
        CHECK(mark_bitmap != nullptr);
        CHECK(mark_bitmap->Clear(to_ref));
        heap_->non_moving_space_->Free(Thread::Current(), to_ref);
      }

      // Get the winner's forward ptr.
      mirror::Object* lost_fwd_ptr = to_ref;
      to_ref = reinterpret_cast<mirror::Object*>(old_lock_word.ForwardingAddress());
      CHECK(to_ref != nullptr);
      CHECK_NE(to_ref, lost_fwd_ptr);
      CHECK(region_space_->IsInToSpace(to_ref) || heap_->non_moving_space_->HasAddress(to_ref))
          << "to_ref=" << to_ref << " " << heap_->DumpSpaces();
      CHECK_NE(to_ref->GetLockWord(false).GetState(), LockWord::kForwardingAddress);
      return to_ref;
    }

    // Copy the old lock word over since we did not copy it yet.
    to_ref->SetLockWord(old_lock_word, false);
    // Set the gray ptr.
    if (kUseBakerReadBarrier) {
      to_ref->SetReadBarrierState(ReadBarrier::GrayState());
    }

    // Do a fence to prevent the field CAS in ConcurrentCopying::Process from possibly reordering
    // before the object copy.
    QuasiAtomic::ThreadFenceRelease();

    LockWord new_lock_word = LockWord::FromForwardingAddress(reinterpret_cast<size_t>(to_ref));

    // Try to atomically write the fwd ptr.
    bool success = from_ref->CasLockWordWeakRelaxed(old_lock_word, new_lock_word);
    if (LIKELY(success)) {
      // The CAS succeeded.
      objects_moved_.FetchAndAddRelaxed(1);
      bytes_moved_.FetchAndAddRelaxed(region_space_alloc_size);
      if (LIKELY(!fall_back_to_non_moving)) {
        DCHECK(region_space_->IsInToSpace(to_ref));
      } else {
        DCHECK(heap_->non_moving_space_->HasAddress(to_ref));
        DCHECK_EQ(bytes_allocated, non_moving_space_bytes_allocated);
      }
      if (kUseBakerReadBarrier) {
        DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState());
      }
      DCHECK(GetFwdPtr(from_ref) == to_ref);
      CHECK_NE(to_ref->GetLockWord(false).GetState(), LockWord::kForwardingAddress);
      PushOntoMarkStack(to_ref);
      return to_ref;
    } else {
      // The CAS failed. It may have lost the race or may have failed
      // due to monitor/hashcode ops. Either way, retry.
    }
  }
}

mirror::Object* ConcurrentCopying::IsMarked(mirror::Object* from_ref) {
  DCHECK(from_ref != nullptr);
  space::RegionSpace::RegionType rtype = region_space_->GetRegionType(from_ref);
  if (rtype == space::RegionSpace::RegionType::kRegionTypeToSpace) {
    // It's already marked.
    return from_ref;
  }
  mirror::Object* to_ref;
  if (rtype == space::RegionSpace::RegionType::kRegionTypeFromSpace) {
    to_ref = GetFwdPtr(from_ref);
    DCHECK(to_ref == nullptr || region_space_->IsInToSpace(to_ref) ||
           heap_->non_moving_space_->HasAddress(to_ref))
        << "from_ref=" << from_ref << " to_ref=" << to_ref;
  } else if (rtype == space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace) {
    if (IsMarkedInUnevacFromSpace(from_ref)) {
      to_ref = from_ref;
    } else {
      to_ref = nullptr;
    }
  } else {
    // At this point, `from_ref` should not be in the region space
    // (i.e. within an "unused" region).
    DCHECK(!region_space_->HasAddress(from_ref)) << from_ref;
    // from_ref is in a non-moving space.
    if (immune_spaces_.ContainsObject(from_ref)) {
      // An immune object is alive.
      to_ref = from_ref;
    } else {
      // Non-immune non-moving space. Use the mark bitmap.
      accounting::ContinuousSpaceBitmap* mark_bitmap =
          heap_mark_bitmap_->GetContinuousSpaceBitmap(from_ref);
      bool is_los = mark_bitmap == nullptr;
      if (!is_los && mark_bitmap->Test(from_ref)) {
        // Already marked.
        to_ref = from_ref;
      } else {
        accounting::LargeObjectBitmap* los_bitmap =
            heap_mark_bitmap_->GetLargeObjectBitmap(from_ref);
        // We may not have a large object space for dex2oat, don't assume it exists.
        if (los_bitmap == nullptr) {
          CHECK(heap_->GetLargeObjectsSpace() == nullptr)
              << "LOS bitmap covers the entire address range " << from_ref
              << " " << heap_->DumpSpaces();
        }
        if (los_bitmap != nullptr && is_los && los_bitmap->Test(from_ref)) {
          // Already marked in LOS.
          to_ref = from_ref;
        } else {
          // Not marked.
          if (IsOnAllocStack(from_ref)) {
            // If on the allocation stack, it's considered marked.
            to_ref = from_ref;
          } else {
            // Not marked.
            to_ref = nullptr;
          }
        }
      }
    }
  }
  return to_ref;
}

bool ConcurrentCopying::IsOnAllocStack(mirror::Object* ref) {
  // TODO: Explain why this is here. What release operation does it pair with?
  QuasiAtomic::ThreadFenceAcquire();
  accounting::ObjectStack* alloc_stack = GetAllocationStack();
  return alloc_stack->Contains(ref);
}

mirror::Object* ConcurrentCopying::MarkNonMoving(mirror::Object* ref,
                                                 mirror::Object* holder,
                                                 MemberOffset offset) {
  // ref is in a non-moving space (from_ref == to_ref).
  DCHECK(!region_space_->HasAddress(ref)) << ref;
  DCHECK(!immune_spaces_.ContainsObject(ref));
  // Use the mark bitmap.
  accounting::ContinuousSpaceBitmap* mark_bitmap =
      heap_mark_bitmap_->GetContinuousSpaceBitmap(ref);
  accounting::LargeObjectBitmap* los_bitmap =
      heap_mark_bitmap_->GetLargeObjectBitmap(ref);
  bool is_los = mark_bitmap == nullptr;
  if (!is_los && mark_bitmap->Test(ref)) {
    // Already marked.
    if (kUseBakerReadBarrier) {
      DCHECK(ref->GetReadBarrierState() == ReadBarrier::GrayState() ||
             ref->GetReadBarrierState() == ReadBarrier::WhiteState());
    }
  } else if (is_los && los_bitmap->Test(ref)) {
    // Already marked in LOS.
    if (kUseBakerReadBarrier) {
      DCHECK(ref->GetReadBarrierState() == ReadBarrier::GrayState() ||
             ref->GetReadBarrierState() == ReadBarrier::WhiteState());
    }
  } else {
    // Not marked.
    if (IsOnAllocStack(ref)) {
      // If it's on the allocation stack, it's considered marked. Keep it white.
      // Objects on the allocation stack need not be marked.
      if (!is_los) {
        DCHECK(!mark_bitmap->Test(ref));
      } else {
        DCHECK(!los_bitmap->Test(ref));
      }
      if (kUseBakerReadBarrier) {
        DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::WhiteState());
      }
    } else {
      // For the baker-style RB, we need to handle 'false-gray' cases. See the
      // kRegionTypeUnevacFromSpace-case comment in Mark().
      if (kUseBakerReadBarrier) {
        // Test the bitmap first to reduce the chance of false gray cases.
        if ((!is_los && mark_bitmap->Test(ref)) ||
            (is_los && los_bitmap->Test(ref))) {
          return ref;
        }
      }
      if (is_los && !IsAligned<kPageSize>(ref)) {
        // Ref is a large object that is not aligned, it must be heap corruption. Dump data before
        // AtomicSetReadBarrierState since it will fault if the address is not valid.
        heap_->GetVerification()->LogHeapCorruption(holder, offset, ref, /* fatal */ true);
      }
      // Not marked or on the allocation stack. Try to mark it.
      // This may or may not succeed, which is ok.
      bool cas_success = false;
      if (kUseBakerReadBarrier) {
        cas_success = ref->AtomicSetReadBarrierState(ReadBarrier::WhiteState(),
                                                     ReadBarrier::GrayState());
      }
      if (!is_los && mark_bitmap->AtomicTestAndSet(ref)) {
        // Already marked.
        if (kUseBakerReadBarrier && cas_success &&
            ref->GetReadBarrierState() == ReadBarrier::GrayState()) {
          PushOntoFalseGrayStack(ref);
        }
      } else if (is_los && los_bitmap->AtomicTestAndSet(ref)) {
        // Already marked in LOS.
        if (kUseBakerReadBarrier && cas_success &&
            ref->GetReadBarrierState() == ReadBarrier::GrayState()) {
          PushOntoFalseGrayStack(ref);
        }
      } else {
        // Newly marked.
        if (kUseBakerReadBarrier) {
          DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::GrayState());
        }
        PushOntoMarkStack(ref);
      }
    }
  }
  return ref;
}

void ConcurrentCopying::FinishPhase() {
  Thread* const self = Thread::Current();
  {
    MutexLock mu(self, mark_stack_lock_);
    CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
  }
  // kVerifyNoMissingCardMarks relies on the region space cards not being cleared to avoid false
  // positives.
  if (!kVerifyNoMissingCardMarks) {
    TimingLogger::ScopedTiming split("ClearRegionSpaceCards", GetTimings());
    // We do not currently use the region space cards at all, madvise them away to save ram.
    heap_->GetCardTable()->ClearCardRange(region_space_->Begin(), region_space_->Limit());
  }
  {
    MutexLock mu(self, skipped_blocks_lock_);
    skipped_blocks_map_.clear();
  }
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    {
      WriterMutexLock mu2(self, *Locks::heap_bitmap_lock_);
      heap_->ClearMarkedObjects();
    }
    if (kUseBakerReadBarrier && kFilterModUnionCards) {
      TimingLogger::ScopedTiming split("FilterModUnionCards", GetTimings());
      ReaderMutexLock mu2(self, *Locks::heap_bitmap_lock_);
      for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
        DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
        accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
        // Filter out cards that don't need to be set.
        if (table != nullptr) {
          table->FilterCards();
        }
      }
    }
    if (kUseBakerReadBarrier) {
      TimingLogger::ScopedTiming split("EmptyRBMarkBitStack", GetTimings());
      DCHECK(rb_mark_bit_stack_ != nullptr);
      const auto* limit = rb_mark_bit_stack_->End();
      for (StackReference<mirror::Object>* it = rb_mark_bit_stack_->Begin(); it != limit; ++it) {
        CHECK(it->AsMirrorPtr()->AtomicSetMarkBit(1, 0))
            << "rb_mark_bit_stack_->Begin()" << rb_mark_bit_stack_->Begin() << '\n'
            << "rb_mark_bit_stack_->End()" << rb_mark_bit_stack_->End() << '\n'
            << "rb_mark_bit_stack_->IsFull()"
            << std::boolalpha << rb_mark_bit_stack_->IsFull() << std::noboolalpha << '\n'
            << DumpReferenceInfo(it->AsMirrorPtr(), "*it");
      }
      rb_mark_bit_stack_->Reset();
    }
  }
  if (measure_read_barrier_slow_path_) {
    MutexLock mu(self, rb_slow_path_histogram_lock_);
    rb_slow_path_time_histogram_.AdjustAndAddValue(rb_slow_path_ns_.LoadRelaxed());
    rb_slow_path_count_total_ += rb_slow_path_count_.LoadRelaxed();
    rb_slow_path_count_gc_total_ += rb_slow_path_count_gc_.LoadRelaxed();
  }
}

bool ConcurrentCopying::IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* field,
                                                    bool do_atomic_update) {
  mirror::Object* from_ref = field->AsMirrorPtr();
  if (from_ref == nullptr) {
    return true;
  }
  mirror::Object* to_ref = IsMarked(from_ref);
  if (to_ref == nullptr) {
    return false;
  }
  if (from_ref != to_ref) {
    if (do_atomic_update) {
      do {
        if (field->AsMirrorPtr() != from_ref) {
          // Concurrently overwritten by a mutator.
          break;
        }
      } while (!field->CasWeakRelaxed(from_ref, to_ref));
    } else {
      // TODO: Why is this seq_cst when the above is relaxed? Document memory ordering.
      field->Assign</* kIsVolatile */ true>(to_ref);
    }
  }
  return true;
}

mirror::Object* ConcurrentCopying::MarkObject(mirror::Object* from_ref) {
  return Mark(from_ref);
}

void ConcurrentCopying::DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                                               ObjPtr<mirror::Reference> reference) {
  heap_->GetReferenceProcessor()->DelayReferenceReferent(klass, reference, this);
}

void ConcurrentCopying::ProcessReferences(Thread* self) {
  TimingLogger::ScopedTiming split("ProcessReferences", GetTimings());
  // We don't really need to lock the heap bitmap lock as we use CAS to mark in bitmaps.
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  GetHeap()->GetReferenceProcessor()->ProcessReferences(
      true /*concurrent*/, GetTimings(), GetCurrentIteration()->GetClearSoftReferences(), this);
}

void ConcurrentCopying::RevokeAllThreadLocalBuffers() {
  TimingLogger::ScopedTiming t(__FUNCTION__, GetTimings());
  region_space_->RevokeAllThreadLocalBuffers();
}

mirror::Object* ConcurrentCopying::MarkFromReadBarrierWithMeasurements(mirror::Object* from_ref) {
  if (Thread::Current() != thread_running_gc_) {
    rb_slow_path_count_.FetchAndAddRelaxed(1u);
  } else {
    rb_slow_path_count_gc_.FetchAndAddRelaxed(1u);
  }
  ScopedTrace tr(__FUNCTION__);
  const uint64_t start_time = measure_read_barrier_slow_path_ ? NanoTime() : 0u;
  mirror::Object* ret = Mark(from_ref);
  if (measure_read_barrier_slow_path_) {
    rb_slow_path_ns_.FetchAndAddRelaxed(NanoTime() - start_time);
  }
  return ret;
}

void ConcurrentCopying::DumpPerformanceInfo(std::ostream& os) {
  GarbageCollector::DumpPerformanceInfo(os);
  MutexLock mu(Thread::Current(), rb_slow_path_histogram_lock_);
  if (rb_slow_path_time_histogram_.SampleSize() > 0) {
    Histogram<uint64_t>::CumulativeData cumulative_data;
    rb_slow_path_time_histogram_.CreateHistogram(&cumulative_data);
    rb_slow_path_time_histogram_.PrintConfidenceIntervals(os, 0.99, cumulative_data);
  }
  if (rb_slow_path_count_total_ > 0) {
    os << "Slow path count " << rb_slow_path_count_total_ << "\n";
  }
  if (rb_slow_path_count_gc_total_ > 0) {
    os << "GC slow path count " << rb_slow_path_count_gc_total_ << "\n";
  }
  os << "Cumulative bytes moved " << cumulative_bytes_moved_.LoadRelaxed() << "\n";
  os << "Cumulative objects moved " << cumulative_objects_moved_.LoadRelaxed() << "\n";

  os << "Peak regions allocated "
     << region_space_->GetMaxPeakNumNonFreeRegions() << " ("
     << PrettySize(region_space_->GetMaxPeakNumNonFreeRegions() * space::RegionSpace::kRegionSize)
     << ") / " << region_space_->GetNumRegions() / 2 << " ("
     << PrettySize(region_space_->GetNumRegions() * space::RegionSpace::kRegionSize / 2)
     << ")\n";
}

}  // namespace collector
}  // namespace gc
}  // namespace art
