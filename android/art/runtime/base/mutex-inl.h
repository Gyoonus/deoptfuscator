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

#ifndef ART_RUNTIME_BASE_MUTEX_INL_H_
#define ART_RUNTIME_BASE_MUTEX_INL_H_

#include <inttypes.h>

#include "mutex.h"

#include "base/utils.h"
#include "base/value_object.h"
#include "thread.h"

#if ART_USE_FUTEXES
#include "linux/futex.h"
#include "sys/syscall.h"
#ifndef SYS_futex
#define SYS_futex __NR_futex
#endif
#endif  // ART_USE_FUTEXES

#define CHECK_MUTEX_CALL(call, args) CHECK_PTHREAD_CALL(call, args, name_)

namespace art {

#if ART_USE_FUTEXES
static inline int futex(volatile int *uaddr, int op, int val, const struct timespec *timeout,
                        volatile int *uaddr2, int val3) {
  return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}
#endif  // ART_USE_FUTEXES

// The following isn't strictly necessary, but we want updates on Atomic<pid_t> to be lock-free.
// TODO: Use std::atomic::is_always_lock_free after switching to C++17 atomics.
static_assert(sizeof(pid_t) <= sizeof(int32_t), "pid_t should fit in 32 bits");

static inline pid_t SafeGetTid(const Thread* self) {
  if (self != nullptr) {
    return self->GetTid();
  } else {
    return GetTid();
  }
}

static inline void CheckUnattachedThread(LockLevel level) NO_THREAD_SAFETY_ANALYSIS {
  // The check below enumerates the cases where we expect not to be able to sanity check locks
  // on a thread. Lock checking is disabled to avoid deadlock when checking shutdown lock.
  // TODO: tighten this check.
  if (kDebugLocking) {
    CHECK(!Locks::IsSafeToCallAbortRacy() ||
          // Used during thread creation to avoid races with runtime shutdown. Thread::Current not
          // yet established.
          level == kRuntimeShutdownLock ||
          // Thread Ids are allocated/released before threads are established.
          level == kAllocatedThreadIdsLock ||
          // Thread LDT's are initialized without Thread::Current established.
          level == kModifyLdtLock ||
          // Threads are unregistered while holding the thread list lock, during this process they
          // no longer exist and so we expect an unlock with no self.
          level == kThreadListLock ||
          // Ignore logging which may or may not have set up thread data structures.
          level == kLoggingLock ||
          // When transitioning from suspended to runnable, a daemon thread might be in
          // a situation where the runtime is shutting down. To not crash our debug locking
          // mechanism we just pass null Thread* to the MutexLock during that transition
          // (see Thread::TransitionFromSuspendedToRunnable).
          level == kThreadSuspendCountLock ||
          // Avoid recursive death.
          level == kAbortLock ||
          // Locks at the absolute top of the stack can be locked at any time.
          level == kTopLockLevel) << level;
  }
}

inline void BaseMutex::RegisterAsLocked(Thread* self) {
  if (UNLIKELY(self == nullptr)) {
    CheckUnattachedThread(level_);
    return;
  }
  if (kDebugLocking) {
    // Check if a bad Mutex of this level or lower is held.
    bool bad_mutexes_held = false;
    // Specifically allow a kTopLockLevel lock to be gained when the current thread holds the
    // mutator_lock_ exclusive. This is because we suspending when holding locks at this level is
    // not allowed and if we hold the mutator_lock_ exclusive we must unsuspend stuff eventually
    // so there are no deadlocks.
    if (level_ == kTopLockLevel &&
        Locks::mutator_lock_->IsSharedHeld(self) &&
        !Locks::mutator_lock_->IsExclusiveHeld(self)) {
      LOG(ERROR) << "Lock level violation: holding \"" << Locks::mutator_lock_->name_ << "\" "
                  << "(level " << kMutatorLock << " - " << static_cast<int>(kMutatorLock)
                  << ") non-exclusive while locking \"" << name_ << "\" "
                  << "(level " << level_ << " - " << static_cast<int>(level_) << ") a top level"
                  << "mutex. This is not allowed.";
      bad_mutexes_held = true;
    } else if (this == Locks::mutator_lock_ && self->GetHeldMutex(kTopLockLevel) != nullptr) {
      LOG(ERROR) << "Lock level violation. Locking mutator_lock_ while already having a "
                 << "kTopLevelLock (" << self->GetHeldMutex(kTopLockLevel)->name_ << "held is "
                 << "not allowed.";
      bad_mutexes_held = true;
    }
    for (int i = level_; i >= 0; --i) {
      LockLevel lock_level_i = static_cast<LockLevel>(i);
      BaseMutex* held_mutex = self->GetHeldMutex(lock_level_i);
      if (level_ == kTopLockLevel &&
          lock_level_i == kMutatorLock &&
          Locks::mutator_lock_->IsExclusiveHeld(self)) {
        // This is checked above.
        continue;
      } else if (UNLIKELY(held_mutex != nullptr) && lock_level_i != kAbortLock) {
        LOG(ERROR) << "Lock level violation: holding \"" << held_mutex->name_ << "\" "
                   << "(level " << lock_level_i << " - " << i
                   << ") while locking \"" << name_ << "\" "
                   << "(level " << level_ << " - " << static_cast<int>(level_) << ")";
        if (lock_level_i > kAbortLock) {
          // Only abort in the check below if this is more than abort level lock.
          bad_mutexes_held = true;
        }
      }
    }
    if (gAborting == 0) {  // Avoid recursive aborts.
      CHECK(!bad_mutexes_held);
    }
  }
  // Don't record monitors as they are outside the scope of analysis. They may be inspected off of
  // the monitor list.
  if (level_ != kMonitorLock) {
    self->SetHeldMutex(level_, this);
  }
}

inline void BaseMutex::RegisterAsUnlocked(Thread* self) {
  if (UNLIKELY(self == nullptr)) {
    CheckUnattachedThread(level_);
    return;
  }
  if (level_ != kMonitorLock) {
    if (kDebugLocking && gAborting == 0) {  // Avoid recursive aborts.
      CHECK(self->GetHeldMutex(level_) == this) << "Unlocking on unacquired mutex: " << name_;
    }
    self->SetHeldMutex(level_, nullptr);
  }
}

inline void ReaderWriterMutex::SharedLock(Thread* self) {
  DCHECK(self == nullptr || self == Thread::Current());
#if ART_USE_FUTEXES
  bool done = false;
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (LIKELY(cur_state >= 0)) {
      // Add as an extra reader.
      done = state_.CompareAndSetWeakAcquire(cur_state, cur_state + 1);
    } else {
      HandleSharedLockContention(self, cur_state);
    }
  } while (!done);
#else
  CHECK_MUTEX_CALL(pthread_rwlock_rdlock, (&rwlock_));
#endif
  DCHECK(GetExclusiveOwnerTid() == 0 || GetExclusiveOwnerTid() == -1);
  RegisterAsLocked(self);
  AssertSharedHeld(self);
}

inline void ReaderWriterMutex::SharedUnlock(Thread* self) {
  DCHECK(self == nullptr || self == Thread::Current());
  DCHECK(GetExclusiveOwnerTid() == 0 || GetExclusiveOwnerTid() == -1);
  AssertSharedHeld(self);
  RegisterAsUnlocked(self);
#if ART_USE_FUTEXES
  bool done = false;
  do {
    int32_t cur_state = state_.LoadRelaxed();
    if (LIKELY(cur_state > 0)) {
      // Reduce state by 1 and impose lock release load/store ordering.
      // Note, the relaxed loads below musn't reorder before the CompareAndSet.
      // TODO: the ordering here is non-trivial as state is split across 3 fields, fix by placing
      // a status bit into the state on contention.
      done = state_.CompareAndSetWeakSequentiallyConsistent(cur_state, cur_state - 1);
      if (done && (cur_state - 1) == 0) {  // Weak CAS may fail spuriously.
        if (num_pending_writers_.LoadRelaxed() > 0 ||
            num_pending_readers_.LoadRelaxed() > 0) {
          // Wake any exclusive waiters as there are now no readers.
          futex(state_.Address(), FUTEX_WAKE, -1, nullptr, nullptr, 0);
        }
      }
    } else {
      LOG(FATAL) << "Unexpected state_:" << cur_state << " for " << name_;
    }
  } while (!done);
#else
  CHECK_MUTEX_CALL(pthread_rwlock_unlock, (&rwlock_));
#endif
}

inline bool Mutex::IsExclusiveHeld(const Thread* self) const {
  DCHECK(self == nullptr || self == Thread::Current());
  bool result = (GetExclusiveOwnerTid() == SafeGetTid(self));
  if (kDebugLocking) {
    // Sanity debug check that if we think it is locked we have it in our held mutexes.
    if (result && self != nullptr && level_ != kMonitorLock && !gAborting) {
      CHECK_EQ(self->GetHeldMutex(level_), this);
    }
  }
  return result;
}

inline pid_t Mutex::GetExclusiveOwnerTid() const {
  return exclusive_owner_.LoadRelaxed();
}

inline void Mutex::AssertExclusiveHeld(const Thread* self) const {
  if (kDebugLocking && (gAborting == 0)) {
    CHECK(IsExclusiveHeld(self)) << *this;
  }
}

inline void Mutex::AssertHeld(const Thread* self) const {
  AssertExclusiveHeld(self);
}

inline bool ReaderWriterMutex::IsExclusiveHeld(const Thread* self) const {
  DCHECK(self == nullptr || self == Thread::Current());
  bool result = (GetExclusiveOwnerTid() == SafeGetTid(self));
  if (kDebugLocking) {
    // Sanity that if the pthread thinks we own the lock the Thread agrees.
    if (self != nullptr && result)  {
      CHECK_EQ(self->GetHeldMutex(level_), this);
    }
  }
  return result;
}

inline pid_t ReaderWriterMutex::GetExclusiveOwnerTid() const {
#if ART_USE_FUTEXES
  int32_t state = state_.LoadRelaxed();
  if (state == 0) {
    return 0;  // No owner.
  } else if (state > 0) {
    return -1;  // Shared.
  } else {
    return exclusive_owner_.LoadRelaxed();
  }
#else
  return exclusive_owner_.LoadRelaxed();
#endif
}

inline void ReaderWriterMutex::AssertExclusiveHeld(const Thread* self) const {
  if (kDebugLocking && (gAborting == 0)) {
    CHECK(IsExclusiveHeld(self)) << *this;
  }
}

inline void ReaderWriterMutex::AssertWriterHeld(const Thread* self) const {
  AssertExclusiveHeld(self);
}

inline void MutatorMutex::TransitionFromRunnableToSuspended(Thread* self) {
  AssertSharedHeld(self);
  RegisterAsUnlocked(self);
}

inline void MutatorMutex::TransitionFromSuspendedToRunnable(Thread* self) {
  RegisterAsLocked(self);
  AssertSharedHeld(self);
}

inline ReaderMutexLock::ReaderMutexLock(Thread* self, ReaderWriterMutex& mu)
    : self_(self), mu_(mu) {
  mu_.SharedLock(self_);
}

inline ReaderMutexLock::~ReaderMutexLock() {
  mu_.SharedUnlock(self_);
}

// Catch bug where variable name is omitted. "ReaderMutexLock (lock);" instead of
// "ReaderMutexLock mu(lock)".
#define ReaderMutexLock(x) static_assert(0, "ReaderMutexLock declaration missing variable name")

}  // namespace art

#endif  // ART_RUNTIME_BASE_MUTEX_INL_H_
