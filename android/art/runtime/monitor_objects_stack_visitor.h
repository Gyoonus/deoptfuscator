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

#ifndef ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_
#define ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_

#include <android-base/logging.h>

#include "art_method.h"
#include "base/mutex.h"
#include "monitor.h"
#include "stack.h"
#include "thread.h"
#include "thread_state.h"

namespace art {

namespace mirror {
class Object;
}

class Context;

class MonitorObjectsStackVisitor : public StackVisitor {
 public:
  MonitorObjectsStackVisitor(Thread* thread_in,
                             Context* context,
                             bool check_suspended = true,
                             bool dump_locks_in = true)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread_in,
                     context,
                     StackVisitor::StackWalkKind::kIncludeInlinedFrames,
                     check_suspended),
        frame_count(0u),
        dump_locks(dump_locks_in) {}

  enum class VisitMethodResult {
    kContinueMethod,
    kSkipMethod,
    kEndStackWalk,
  };

  bool VisitFrame() FINAL REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    if (m->IsRuntimeMethod()) {
      return true;
    }

    VisitMethodResult vmrEntry = StartMethod(m, frame_count);
    switch (vmrEntry) {
      case VisitMethodResult::kContinueMethod:
        break;
      case VisitMethodResult::kSkipMethod:
        return true;
      case VisitMethodResult::kEndStackWalk:
        return false;
    }

    if (frame_count == 0) {
      // Top frame, check for blocked state.

      mirror::Object* monitor_object;
      uint32_t lock_owner_tid;
      ThreadState state = Monitor::FetchState(GetThread(),
                                              &monitor_object,
                                              &lock_owner_tid);
      switch (state) {
        case kWaiting:
        case kTimedWaiting:
          VisitWaitingObject(monitor_object, state);
          break;
        case kSleeping:
          VisitSleepingObject(monitor_object);
          break;

        case kBlocked:
        case kWaitingForLockInflation:
          VisitBlockedOnObject(monitor_object, state, lock_owner_tid);
          break;

        default:
          break;
      }
    }

    if (dump_locks) {
      // Visit locks, but do not abort on errors. This could trigger a nested abort.
      // Skip visiting locks if dump_locks is false as it would cause a bad_mutexes_held in
      // RegTypeCache::RegTypeCache due to thread_list_lock.
      Monitor::VisitLocks(this, VisitLockedObject, this, false);
    }

    ++frame_count;

    VisitMethodResult vmrExit = EndMethod(m);
    switch (vmrExit) {
      case VisitMethodResult::kContinueMethod:
      case VisitMethodResult::kSkipMethod:
        return true;

      case VisitMethodResult::kEndStackWalk:
        return false;
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

 protected:
  virtual VisitMethodResult StartMethod(ArtMethod* m, size_t frame_nr)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual VisitMethodResult EndMethod(ArtMethod* m)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  virtual void VisitWaitingObject(mirror::Object* obj, ThreadState state)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitSleepingObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitBlockedOnObject(mirror::Object* obj, ThreadState state, uint32_t owner_tid)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;
  virtual void VisitLockedObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  size_t frame_count;

 private:
  static void VisitLockedObject(mirror::Object* o, void* context)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    MonitorObjectsStackVisitor* self = reinterpret_cast<MonitorObjectsStackVisitor*>(context);
    if (o != nullptr) {
      if (kUseReadBarrier && Thread::Current()->GetIsGcMarking()) {
        // We may call Thread::Dump() in the middle of the CC thread flip and this thread's stack
        // may have not been flipped yet and "o" may be a from-space (stale) ref, in which case the
        // IdentityHashCode call below will crash. So explicitly mark/forward it here.
        o = ReadBarrier::Mark(o);
      }
    }
    self->VisitLockedObject(o);
  }

  const bool dump_locks;
};

}  // namespace art

#endif  // ART_RUNTIME_MONITOR_OBJECTS_STACK_VISITOR_H_
