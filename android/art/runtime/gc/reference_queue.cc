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

#include "reference_queue.h"

#include "accounting/card_table-inl.h"
#include "collector/concurrent_copying.h"
#include "heap.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/reference-inl.h"
#include "object_callbacks.h"

namespace art {
namespace gc {

ReferenceQueue::ReferenceQueue(Mutex* lock) : lock_(lock), list_(nullptr) {
}

void ReferenceQueue::AtomicEnqueueIfNotEnqueued(Thread* self, ObjPtr<mirror::Reference> ref) {
  DCHECK(ref != nullptr);
  MutexLock mu(self, *lock_);
  if (ref->IsUnprocessed()) {
    EnqueueReference(ref);
  }
}

void ReferenceQueue::EnqueueReference(ObjPtr<mirror::Reference> ref) {
  DCHECK(ref != nullptr);
  CHECK(ref->IsUnprocessed());
  if (IsEmpty()) {
    // 1 element cyclic queue, ie: Reference ref = ..; ref.pendingNext = ref;
    list_ = ref.Ptr();
  } else {
    // The list is owned by the GC, everything that has been inserted must already be at least
    // gray.
    ObjPtr<mirror::Reference> head = list_->GetPendingNext<kWithoutReadBarrier>();
    DCHECK(head != nullptr);
    ref->SetPendingNext(head);
  }
  // Add the reference in the middle to preserve the cycle.
  list_->SetPendingNext(ref);
}

ObjPtr<mirror::Reference> ReferenceQueue::DequeuePendingReference() {
  DCHECK(!IsEmpty());
  ObjPtr<mirror::Reference> ref = list_->GetPendingNext<kWithoutReadBarrier>();
  DCHECK(ref != nullptr);
  // Note: the following code is thread-safe because it is only called from ProcessReferences which
  // is single threaded.
  if (list_ == ref) {
    list_ = nullptr;
  } else {
    ObjPtr<mirror::Reference> next = ref->GetPendingNext<kWithoutReadBarrier>();
    list_->SetPendingNext(next);
  }
  ref->SetPendingNext(nullptr);
  return ref;
}

// This must be called whenever DequeuePendingReference is called.
void ReferenceQueue::DisableReadBarrierForReference(ObjPtr<mirror::Reference> ref) {
  Heap* heap = Runtime::Current()->GetHeap();
  if (kUseBakerOrBrooksReadBarrier && heap->CurrentCollectorType() == kCollectorTypeCC &&
      heap->ConcurrentCopyingCollector()->IsActive()) {
    // Change the gray ptr we left in ConcurrentCopying::ProcessMarkStackRef() to white.
    // We check IsActive() above because we don't want to do this when the zygote compaction
    // collector (SemiSpace) is running.
    CHECK(ref != nullptr);
    collector::ConcurrentCopying* concurrent_copying = heap->ConcurrentCopyingCollector();
    uint32_t rb_state = ref->GetReadBarrierState();
    if (rb_state == ReadBarrier::GrayState()) {
      ref->AtomicSetReadBarrierState(ReadBarrier::GrayState(), ReadBarrier::WhiteState());
      CHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::WhiteState());
    } else {
      // In ConcurrentCopying::ProcessMarkStackRef() we may leave a white reference in the queue and
      // find it here, which is OK.
      CHECK_EQ(rb_state, ReadBarrier::WhiteState()) << "ref=" << ref << " rb_state=" << rb_state;
      ObjPtr<mirror::Object> referent = ref->GetReferent<kWithoutReadBarrier>();
      // The referent could be null if it's cleared by a mutator (Reference.clear()).
      if (referent != nullptr) {
        CHECK(concurrent_copying->IsInToSpace(referent.Ptr()))
            << "ref=" << ref << " rb_state=" << ref->GetReadBarrierState()
            << " referent=" << referent;
      }
    }
  }
}

void ReferenceQueue::Dump(std::ostream& os) const {
  ObjPtr<mirror::Reference> cur = list_;
  os << "Reference starting at list_=" << list_ << "\n";
  if (cur == nullptr) {
    return;
  }
  do {
    ObjPtr<mirror::Reference> pending_next = cur->GetPendingNext();
    os << "Reference= " << cur << " PendingNext=" << pending_next;
    if (cur->IsFinalizerReferenceInstance()) {
      os << " Zombie=" << cur->AsFinalizerReference()->GetZombie();
    }
    os << "\n";
    cur = pending_next;
  } while (cur != list_);
}

size_t ReferenceQueue::GetLength() const {
  size_t count = 0;
  ObjPtr<mirror::Reference> cur = list_;
  if (cur != nullptr) {
    do {
      ++count;
      cur = cur->GetPendingNext();
    } while (cur != list_);
  }
  return count;
}

void ReferenceQueue::ClearWhiteReferences(ReferenceQueue* cleared_references,
                                          collector::GarbageCollector* collector) {
  while (!IsEmpty()) {
    ObjPtr<mirror::Reference> ref = DequeuePendingReference();
    mirror::HeapReference<mirror::Object>* referent_addr = ref->GetReferentReferenceAddr();
    // do_atomic_update is false because this happens during the reference processing phase where
    // Reference.clear() would block.
    if (!collector->IsNullOrMarkedHeapReference(referent_addr, /*do_atomic_update*/false)) {
      // Referent is white, clear it.
      if (Runtime::Current()->IsActiveTransaction()) {
        ref->ClearReferent<true>();
      } else {
        ref->ClearReferent<false>();
      }
      cleared_references->EnqueueReference(ref);
    }
    // Delay disabling the read barrier until here so that the ClearReferent call above in
    // transaction mode will trigger the read barrier.
    DisableReadBarrierForReference(ref);
  }
}

void ReferenceQueue::EnqueueFinalizerReferences(ReferenceQueue* cleared_references,
                                                collector::GarbageCollector* collector) {
  while (!IsEmpty()) {
    ObjPtr<mirror::FinalizerReference> ref = DequeuePendingReference()->AsFinalizerReference();
    mirror::HeapReference<mirror::Object>* referent_addr = ref->GetReferentReferenceAddr();
    // do_atomic_update is false because this happens during the reference processing phase where
    // Reference.clear() would block.
    if (!collector->IsNullOrMarkedHeapReference(referent_addr, /*do_atomic_update*/false)) {
      ObjPtr<mirror::Object> forward_address = collector->MarkObject(referent_addr->AsMirrorPtr());
      // Move the updated referent to the zombie field.
      if (Runtime::Current()->IsActiveTransaction()) {
        ref->SetZombie<true>(forward_address);
        ref->ClearReferent<true>();
      } else {
        ref->SetZombie<false>(forward_address);
        ref->ClearReferent<false>();
      }
      cleared_references->EnqueueReference(ref);
    }
    // Delay disabling the read barrier until here so that the ClearReferent call above in
    // transaction mode will trigger the read barrier.
    DisableReadBarrierForReference(ref->AsReference());
  }
}

void ReferenceQueue::ForwardSoftReferences(MarkObjectVisitor* visitor) {
  if (UNLIKELY(IsEmpty())) {
    return;
  }
  ObjPtr<mirror::Reference> const head = list_;
  ObjPtr<mirror::Reference> ref = head;
  do {
    mirror::HeapReference<mirror::Object>* referent_addr = ref->GetReferentReferenceAddr();
    if (referent_addr->AsMirrorPtr() != nullptr) {
      // do_atomic_update is false because mutators can't access the referent due to the weak ref
      // access blocking.
      visitor->MarkHeapReference(referent_addr, /*do_atomic_update*/ false);
    }
    ref = ref->GetPendingNext();
  } while (LIKELY(ref != head));
}

void ReferenceQueue::UpdateRoots(IsMarkedVisitor* visitor) {
  if (list_ != nullptr) {
    list_ = down_cast<mirror::Reference*>(visitor->IsMarked(list_));
  }
}

}  // namespace gc
}  // namespace art
