/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "monitor.h"

#include <vector>

#include "android-base/stringprintf.h"

#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG.
#include "base/mutex.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "class_linker.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction-inl.h"
#include "lock_word-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "object_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "stack.h"
#include "thread.h"
#include "thread_list.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

static constexpr uint64_t kDebugThresholdFudgeFactor = kIsDebugBuild ? 10 : 1;
static constexpr uint64_t kLongWaitMs = 100 * kDebugThresholdFudgeFactor;

/*
 * Every Object has a monitor associated with it, but not every Object is actually locked.  Even
 * the ones that are locked do not need a full-fledged monitor until a) there is actual contention
 * or b) wait() is called on the Object.
 *
 * For Android, we have implemented a scheme similar to the one described in Bacon et al.'s
 * "Thin locks: featherweight synchronization for Java" (ACM 1998).  Things are even easier for us,
 * though, because we have a full 32 bits to work with.
 *
 * The two states of an Object's lock are referred to as "thin" and "fat".  A lock may transition
 * from the "thin" state to the "fat" state and this transition is referred to as inflation. Once
 * a lock has been inflated it remains in the "fat" state indefinitely.
 *
 * The lock value itself is stored in mirror::Object::monitor_ and the representation is described
 * in the LockWord value type.
 *
 * Monitors provide:
 *  - mutually exclusive access to resources
 *  - a way for multiple threads to wait for notification
 *
 * In effect, they fill the role of both mutexes and condition variables.
 *
 * Only one thread can own the monitor at any time.  There may be several threads waiting on it
 * (the wait call unlocks it).  One or more waiting threads may be getting interrupted or notified
 * at any given time.
 */

uint32_t Monitor::lock_profiling_threshold_ = 0;
uint32_t Monitor::stack_dump_lock_profiling_threshold_ = 0;

void Monitor::Init(uint32_t lock_profiling_threshold,
                   uint32_t stack_dump_lock_profiling_threshold) {
  // It isn't great to always include the debug build fudge factor for command-
  // line driven arguments, but it's easier to adjust here than in the build.
  lock_profiling_threshold_ =
      lock_profiling_threshold * kDebugThresholdFudgeFactor;
  stack_dump_lock_profiling_threshold_ =
      stack_dump_lock_profiling_threshold * kDebugThresholdFudgeFactor;
}

Monitor::Monitor(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code)
    : monitor_lock_("a monitor lock", kMonitorLock),
      monitor_contenders_("monitor contenders", monitor_lock_),
      num_waiters_(0),
      owner_(owner),
      lock_count_(0),
      obj_(GcRoot<mirror::Object>(obj)),
      wait_set_(nullptr),
      hash_code_(hash_code),
      locking_method_(nullptr),
      locking_dex_pc_(0),
      monitor_id_(MonitorPool::ComputeMonitorId(this, self)) {
#ifdef __LP64__
  DCHECK(false) << "Should not be reached in 64b";
  next_free_ = nullptr;
#endif
  // We should only inflate a lock if the owner is ourselves or suspended. This avoids a race
  // with the owner unlocking the thin-lock.
  CHECK(owner == nullptr || owner == self || owner->IsSuspended());
  // The identity hash code is set for the life time of the monitor.
}

Monitor::Monitor(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code,
                 MonitorId id)
    : monitor_lock_("a monitor lock", kMonitorLock),
      monitor_contenders_("monitor contenders", monitor_lock_),
      num_waiters_(0),
      owner_(owner),
      lock_count_(0),
      obj_(GcRoot<mirror::Object>(obj)),
      wait_set_(nullptr),
      hash_code_(hash_code),
      locking_method_(nullptr),
      locking_dex_pc_(0),
      monitor_id_(id) {
#ifdef __LP64__
  next_free_ = nullptr;
#endif
  // We should only inflate a lock if the owner is ourselves or suspended. This avoids a race
  // with the owner unlocking the thin-lock.
  CHECK(owner == nullptr || owner == self || owner->IsSuspended());
  // The identity hash code is set for the life time of the monitor.
}

int32_t Monitor::GetHashCode() {
  while (!HasHashCode()) {
    if (hash_code_.CompareAndSetWeakRelaxed(0, mirror::Object::GenerateIdentityHashCode())) {
      break;
    }
  }
  DCHECK(HasHashCode());
  return hash_code_.LoadRelaxed();
}

bool Monitor::Install(Thread* self) {
  MutexLock mu(self, monitor_lock_);  // Uncontended mutex acquisition as monitor isn't yet public.
  CHECK(owner_ == nullptr || owner_ == self || owner_->IsSuspended());
  // Propagate the lock state.
  LockWord lw(GetObject()->GetLockWord(false));
  switch (lw.GetState()) {
    case LockWord::kThinLocked: {
      CHECK_EQ(owner_->GetThreadId(), lw.ThinLockOwner());
      lock_count_ = lw.ThinLockCount();
      break;
    }
    case LockWord::kHashCode: {
      CHECK_EQ(hash_code_.LoadRelaxed(), static_cast<int32_t>(lw.GetHashCode()));
      break;
    }
    case LockWord::kFatLocked: {
      // The owner_ is suspended but another thread beat us to install a monitor.
      return false;
    }
    case LockWord::kUnlocked: {
      LOG(FATAL) << "Inflating unlocked lock word";
      break;
    }
    default: {
      LOG(FATAL) << "Invalid monitor state " << lw.GetState();
      return false;
    }
  }
  LockWord fat(this, lw.GCState());
  // Publish the updated lock word, which may race with other threads.
  bool success = GetObject()->CasLockWordWeakRelease(lw, fat);
  // Lock profiling.
  if (success && owner_ != nullptr && lock_profiling_threshold_ != 0) {
    // Do not abort on dex pc errors. This can easily happen when we want to dump a stack trace on
    // abort.
    locking_method_ = owner_->GetCurrentMethod(&locking_dex_pc_, false);
    if (locking_method_ != nullptr && UNLIKELY(locking_method_->IsProxyMethod())) {
      // Grab another frame. Proxy methods are not helpful for lock profiling. This should be rare
      // enough that it's OK to walk the stack twice.
      struct NextMethodVisitor FINAL : public StackVisitor {
        explicit NextMethodVisitor(Thread* thread) REQUIRES_SHARED(Locks::mutator_lock_)
            : StackVisitor(thread,
                           nullptr,
                           StackVisitor::StackWalkKind::kIncludeInlinedFrames,
                           false),
              count_(0),
              method_(nullptr),
              dex_pc_(0) {}
        bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
          ArtMethod* m = GetMethod();
          if (m->IsRuntimeMethod()) {
            // Continue if this is a runtime method.
            return true;
          }
          count_++;
          if (count_ == 2u) {
            method_ = m;
            dex_pc_ = GetDexPc(false);
            return false;
          }
          return true;
        }
        size_t count_;
        ArtMethod* method_;
        uint32_t dex_pc_;
      };
      NextMethodVisitor nmv(owner_);
      nmv.WalkStack();
      locking_method_ = nmv.method_;
      locking_dex_pc_ = nmv.dex_pc_;
    }
    DCHECK(locking_method_ == nullptr || !locking_method_->IsProxyMethod());
  }
  return success;
}

Monitor::~Monitor() {
  // Deflated monitors have a null object.
}

void Monitor::AppendToWaitSet(Thread* thread) {
  DCHECK(owner_ == Thread::Current());
  DCHECK(thread != nullptr);
  DCHECK(thread->GetWaitNext() == nullptr) << thread->GetWaitNext();
  if (wait_set_ == nullptr) {
    wait_set_ = thread;
    return;
  }

  // push_back.
  Thread* t = wait_set_;
  while (t->GetWaitNext() != nullptr) {
    t = t->GetWaitNext();
  }
  t->SetWaitNext(thread);
}

void Monitor::RemoveFromWaitSet(Thread *thread) {
  DCHECK(owner_ == Thread::Current());
  DCHECK(thread != nullptr);
  if (wait_set_ == nullptr) {
    return;
  }
  if (wait_set_ == thread) {
    wait_set_ = thread->GetWaitNext();
    thread->SetWaitNext(nullptr);
    return;
  }

  Thread* t = wait_set_;
  while (t->GetWaitNext() != nullptr) {
    if (t->GetWaitNext() == thread) {
      t->SetWaitNext(thread->GetWaitNext());
      thread->SetWaitNext(nullptr);
      return;
    }
    t = t->GetWaitNext();
  }
}

void Monitor::SetObject(mirror::Object* object) {
  obj_ = GcRoot<mirror::Object>(object);
}

// Note: Adapted from CurrentMethodVisitor in thread.cc. We must not resolve here.

struct NthCallerWithDexPcVisitor FINAL : public StackVisitor {
  explicit NthCallerWithDexPcVisitor(Thread* thread, size_t frame)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        method_(nullptr),
        dex_pc_(0),
        current_frame_number_(0),
        wanted_frame_number_(frame) {}
  bool VisitFrame() OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    if (m == nullptr || m->IsRuntimeMethod()) {
      // Runtime method, upcall, or resolution issue. Skip.
      return true;
    }

    // Is this the requested frame?
    if (current_frame_number_ == wanted_frame_number_) {
      method_ = m;
      dex_pc_ = GetDexPc(false /* abort_on_error*/);
      return false;
    }

    // Look for more.
    current_frame_number_++;
    return true;
  }

  ArtMethod* method_;
  uint32_t dex_pc_;

 private:
  size_t current_frame_number_;
  const size_t wanted_frame_number_;
};

// This function is inlined and just helps to not have the VLOG and ATRACE check at all the
// potential tracing points.
void Monitor::AtraceMonitorLock(Thread* self, mirror::Object* obj, bool is_wait) {
  if (UNLIKELY(VLOG_IS_ON(systrace_lock_logging) && ATRACE_ENABLED())) {
    AtraceMonitorLockImpl(self, obj, is_wait);
  }
}

void Monitor::AtraceMonitorLockImpl(Thread* self, mirror::Object* obj, bool is_wait) {
  // Wait() requires a deeper call stack to be useful. Otherwise you'll see "Waiting at
  // Object.java". Assume that we'll wait a nontrivial amount, so it's OK to do a longer
  // stack walk than if !is_wait.
  NthCallerWithDexPcVisitor visitor(self, is_wait ? 1U : 0U);
  visitor.WalkStack(false);
  const char* prefix = is_wait ? "Waiting on " : "Locking ";

  const char* filename;
  int32_t line_number;
  TranslateLocation(visitor.method_, visitor.dex_pc_, &filename, &line_number);

  // It would be nice to have a stable "ID" for the object here. However, the only stable thing
  // would be the identity hashcode. But we cannot use IdentityHashcode here: For one, there are
  // times when it is unsafe to make that call (see stack dumping for an explanation). More
  // importantly, we would have to give up on thin-locking when adding systrace locks, as the
  // identity hashcode is stored in the lockword normally (so can't be used with thin-locks).
  //
  // Because of thin-locks we also cannot use the monitor id (as there is no monitor). Monitor ids
  // also do not have to be stable, as the monitor may be deflated.
  std::string tmp = StringPrintf("%s %d at %s:%d",
      prefix,
      (obj == nullptr ? -1 : static_cast<int32_t>(reinterpret_cast<uintptr_t>(obj))),
      (filename != nullptr ? filename : "null"),
      line_number);
  ATRACE_BEGIN(tmp.c_str());
}

void Monitor::AtraceMonitorUnlock() {
  if (UNLIKELY(VLOG_IS_ON(systrace_lock_logging))) {
    ATRACE_END();
  }
}

std::string Monitor::PrettyContentionInfo(const std::string& owner_name,
                                          pid_t owner_tid,
                                          ArtMethod* owners_method,
                                          uint32_t owners_dex_pc,
                                          size_t num_waiters) {
  Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  const char* owners_filename;
  int32_t owners_line_number = 0;
  if (owners_method != nullptr) {
    TranslateLocation(owners_method, owners_dex_pc, &owners_filename, &owners_line_number);
  }
  std::ostringstream oss;
  oss << "monitor contention with owner " << owner_name << " (" << owner_tid << ")";
  if (owners_method != nullptr) {
    oss << " at " << owners_method->PrettyMethod();
    oss << "(" << owners_filename << ":" << owners_line_number << ")";
  }
  oss << " waiters=" << num_waiters;
  return oss.str();
}

bool Monitor::TryLockLocked(Thread* self) {
  if (owner_ == nullptr) {  // Unowned.
    owner_ = self;
    CHECK_EQ(lock_count_, 0);
    // When debugging, save the current monitor holder for future
    // acquisition failures to use in sampled logging.
    if (lock_profiling_threshold_ != 0) {
      locking_method_ = self->GetCurrentMethod(&locking_dex_pc_);
      // We don't expect a proxy method here.
      DCHECK(locking_method_ == nullptr || !locking_method_->IsProxyMethod());
    }
  } else if (owner_ == self) {  // Recursive.
    lock_count_++;
  } else {
    return false;
  }
  AtraceMonitorLock(self, GetObject(), false /* is_wait */);
  return true;
}

bool Monitor::TryLock(Thread* self) {
  MutexLock mu(self, monitor_lock_);
  return TryLockLocked(self);
}

// Asserts that a mutex isn't held when the class comes into and out of scope.
class ScopedAssertNotHeld {
 public:
  ScopedAssertNotHeld(Thread* self, Mutex& mu) : self_(self), mu_(mu) {
    mu_.AssertNotHeld(self_);
  }

  ~ScopedAssertNotHeld() {
    mu_.AssertNotHeld(self_);
  }

 private:
  Thread* const self_;
  Mutex& mu_;
  DISALLOW_COPY_AND_ASSIGN(ScopedAssertNotHeld);
};

template <LockReason reason>
void Monitor::Lock(Thread* self) {
  ScopedAssertNotHeld sanh(self, monitor_lock_);
  bool called_monitors_callback = false;
  monitor_lock_.Lock(self);
  while (true) {
    if (TryLockLocked(self)) {
      break;
    }
    // Contended.
    const bool log_contention = (lock_profiling_threshold_ != 0);
    uint64_t wait_start_ms = log_contention ? MilliTime() : 0;
    ArtMethod* owners_method = locking_method_;
    uint32_t owners_dex_pc = locking_dex_pc_;
    // Do this before releasing the lock so that we don't get deflated.
    size_t num_waiters = num_waiters_;
    ++num_waiters_;

    // If systrace logging is enabled, first look at the lock owner. Acquiring the monitor's
    // lock and then re-acquiring the mutator lock can deadlock.
    bool started_trace = false;
    if (ATRACE_ENABLED()) {
      if (owner_ != nullptr) {  // Did the owner_ give the lock up?
        std::ostringstream oss;
        std::string name;
        owner_->GetThreadName(name);
        oss << PrettyContentionInfo(name,
                                    owner_->GetTid(),
                                    owners_method,
                                    owners_dex_pc,
                                    num_waiters);
        // Add info for contending thread.
        uint32_t pc;
        ArtMethod* m = self->GetCurrentMethod(&pc);
        const char* filename;
        int32_t line_number;
        TranslateLocation(m, pc, &filename, &line_number);
        oss << " blocking from "
            << ArtMethod::PrettyMethod(m) << "(" << (filename != nullptr ? filename : "null")
            << ":" << line_number << ")";
        ATRACE_BEGIN(oss.str().c_str());
        started_trace = true;
      }
    }

    monitor_lock_.Unlock(self);  // Let go of locks in order.
    // Call the contended locking cb once and only once. Also only call it if we are locking for
    // the first time, not during a Wait wakeup.
    if (reason == LockReason::kForLock && !called_monitors_callback) {
      called_monitors_callback = true;
      Runtime::Current()->GetRuntimeCallbacks()->MonitorContendedLocking(this);
    }
    self->SetMonitorEnterObject(GetObject());
    {
      ScopedThreadSuspension tsc(self, kBlocked);  // Change to blocked and give up mutator_lock_.
      uint32_t original_owner_thread_id = 0u;
      {
        // Reacquire monitor_lock_ without mutator_lock_ for Wait.
        MutexLock mu2(self, monitor_lock_);
        if (owner_ != nullptr) {  // Did the owner_ give the lock up?
          original_owner_thread_id = owner_->GetThreadId();
          monitor_contenders_.Wait(self);  // Still contended so wait.
        }
      }
      if (original_owner_thread_id != 0u) {
        // Woken from contention.
        if (log_contention) {
          uint64_t wait_ms = MilliTime() - wait_start_ms;
          uint32_t sample_percent;
          if (wait_ms >= lock_profiling_threshold_) {
            sample_percent = 100;
          } else {
            sample_percent = 100 * wait_ms / lock_profiling_threshold_;
          }
          if (sample_percent != 0 && (static_cast<uint32_t>(rand() % 100) < sample_percent)) {
            // Reacquire mutator_lock_ for logging.
            ScopedObjectAccess soa(self);

            bool owner_alive = false;
            pid_t original_owner_tid = 0;
            std::string original_owner_name;

            const bool should_dump_stacks = stack_dump_lock_profiling_threshold_ > 0 &&
                wait_ms > stack_dump_lock_profiling_threshold_;
            std::string owner_stack_dump;

            // Acquire thread-list lock to find thread and keep it from dying until we've got all
            // the info we need.
            {
              Locks::thread_list_lock_->ExclusiveLock(Thread::Current());

              // Re-find the owner in case the thread got killed.
              Thread* original_owner = Runtime::Current()->GetThreadList()->FindThreadByThreadId(
                  original_owner_thread_id);

              if (original_owner != nullptr) {
                owner_alive = true;
                original_owner_tid = original_owner->GetTid();
                original_owner->GetThreadName(original_owner_name);

                if (should_dump_stacks) {
                  // Very long contention. Dump stacks.
                  struct CollectStackTrace : public Closure {
                    void Run(art::Thread* thread) OVERRIDE
                        REQUIRES_SHARED(art::Locks::mutator_lock_) {
                      thread->DumpJavaStack(oss);
                    }

                    std::ostringstream oss;
                  };
                  CollectStackTrace owner_trace;
                  // RequestSynchronousCheckpoint releases the thread_list_lock_ as a part of its
                  // execution.
                  original_owner->RequestSynchronousCheckpoint(&owner_trace);
                  owner_stack_dump = owner_trace.oss.str();
                } else {
                  Locks::thread_list_lock_->ExclusiveUnlock(Thread::Current());
                }
              } else {
                Locks::thread_list_lock_->ExclusiveUnlock(Thread::Current());
              }
              // This is all the data we need. Now drop the thread-list lock, it's OK for the
              // owner to go away now.
            }

            // If we found the owner (and thus have owner data), go and log now.
            if (owner_alive) {
              // Give the detailed traces for really long contention.
              if (should_dump_stacks) {
                // This must be here (and not above) because we cannot hold the thread-list lock
                // while running the checkpoint.
                std::ostringstream self_trace_oss;
                self->DumpJavaStack(self_trace_oss);

                uint32_t pc;
                ArtMethod* m = self->GetCurrentMethod(&pc);

                LOG(WARNING) << "Long "
                    << PrettyContentionInfo(original_owner_name,
                                            original_owner_tid,
                                            owners_method,
                                            owners_dex_pc,
                                            num_waiters)
                    << " in " << ArtMethod::PrettyMethod(m) << " for "
                    << PrettyDuration(MsToNs(wait_ms)) << "\n"
                    << "Current owner stack:\n" << owner_stack_dump
                    << "Contender stack:\n" << self_trace_oss.str();
              } else if (wait_ms > kLongWaitMs && owners_method != nullptr) {
                uint32_t pc;
                ArtMethod* m = self->GetCurrentMethod(&pc);
                // TODO: We should maybe check that original_owner is still a live thread.
                LOG(WARNING) << "Long "
                    << PrettyContentionInfo(original_owner_name,
                                            original_owner_tid,
                                            owners_method,
                                            owners_dex_pc,
                                            num_waiters)
                    << " in " << ArtMethod::PrettyMethod(m) << " for "
                    << PrettyDuration(MsToNs(wait_ms));
              }
              LogContentionEvent(self,
                                wait_ms,
                                sample_percent,
                                owners_method,
                                owners_dex_pc);
            }
          }
        }
      }
    }
    if (started_trace) {
      ATRACE_END();
    }
    self->SetMonitorEnterObject(nullptr);
    monitor_lock_.Lock(self);  // Reacquire locks in order.
    --num_waiters_;
  }
  monitor_lock_.Unlock(self);
  // We need to pair this with a single contended locking call. NB we match the RI behavior and call
  // this even if MonitorEnter failed.
  if (called_monitors_callback) {
    CHECK(reason == LockReason::kForLock);
    Runtime::Current()->GetRuntimeCallbacks()->MonitorContendedLocked(this);
  }
}

template void Monitor::Lock<LockReason::kForLock>(Thread* self);
template void Monitor::Lock<LockReason::kForWait>(Thread* self);

static void ThrowIllegalMonitorStateExceptionF(const char* fmt, ...)
                                              __attribute__((format(printf, 1, 2)));

static void ThrowIllegalMonitorStateExceptionF(const char* fmt, ...)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  va_list args;
  va_start(args, fmt);
  Thread* self = Thread::Current();
  self->ThrowNewExceptionV("Ljava/lang/IllegalMonitorStateException;", fmt, args);
  if (!Runtime::Current()->IsStarted() || VLOG_IS_ON(monitor)) {
    std::ostringstream ss;
    self->Dump(ss);
    LOG(Runtime::Current()->IsStarted() ? ::android::base::INFO : ::android::base::ERROR)
        << self->GetException()->Dump() << "\n" << ss.str();
  }
  va_end(args);
}

static std::string ThreadToString(Thread* thread) {
  if (thread == nullptr) {
    return "nullptr";
  }
  std::ostringstream oss;
  // TODO: alternatively, we could just return the thread's name.
  oss << *thread;
  return oss.str();
}

void Monitor::FailedUnlock(mirror::Object* o,
                           uint32_t expected_owner_thread_id,
                           uint32_t found_owner_thread_id,
                           Monitor* monitor) {
  // Acquire thread list lock so threads won't disappear from under us.
  std::string current_owner_string;
  std::string expected_owner_string;
  std::string found_owner_string;
  uint32_t current_owner_thread_id = 0u;
  {
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    ThreadList* const thread_list = Runtime::Current()->GetThreadList();
    Thread* expected_owner = thread_list->FindThreadByThreadId(expected_owner_thread_id);
    Thread* found_owner = thread_list->FindThreadByThreadId(found_owner_thread_id);

    // Re-read owner now that we hold lock.
    Thread* current_owner = (monitor != nullptr) ? monitor->GetOwner() : nullptr;
    if (current_owner != nullptr) {
      current_owner_thread_id = current_owner->GetThreadId();
    }
    // Get short descriptions of the threads involved.
    current_owner_string = ThreadToString(current_owner);
    expected_owner_string = expected_owner != nullptr ? ThreadToString(expected_owner) : "unnamed";
    found_owner_string = found_owner != nullptr ? ThreadToString(found_owner) : "unnamed";
  }

  if (current_owner_thread_id == 0u) {
    if (found_owner_thread_id == 0u) {
      ThrowIllegalMonitorStateExceptionF("unlock of unowned monitor on object of type '%s'"
                                         " on thread '%s'",
                                         mirror::Object::PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    } else {
      // Race: the original read found an owner but now there is none
      ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                         " (where now the monitor appears unowned) on thread '%s'",
                                         found_owner_string.c_str(),
                                         mirror::Object::PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    }
  } else {
    if (found_owner_thread_id == 0u) {
      // Race: originally there was no owner, there is now
      ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                         " (originally believed to be unowned) on thread '%s'",
                                         current_owner_string.c_str(),
                                         mirror::Object::PrettyTypeOf(o).c_str(),
                                         expected_owner_string.c_str());
    } else {
      if (found_owner_thread_id != current_owner_thread_id) {
        // Race: originally found and current owner have changed
        ThrowIllegalMonitorStateExceptionF("unlock of monitor originally owned by '%s' (now"
                                           " owned by '%s') on object of type '%s' on thread '%s'",
                                           found_owner_string.c_str(),
                                           current_owner_string.c_str(),
                                           mirror::Object::PrettyTypeOf(o).c_str(),
                                           expected_owner_string.c_str());
      } else {
        ThrowIllegalMonitorStateExceptionF("unlock of monitor owned by '%s' on object of type '%s'"
                                           " on thread '%s",
                                           current_owner_string.c_str(),
                                           mirror::Object::PrettyTypeOf(o).c_str(),
                                           expected_owner_string.c_str());
      }
    }
  }
}

bool Monitor::Unlock(Thread* self) {
  DCHECK(self != nullptr);
  uint32_t owner_thread_id = 0u;
  {
    MutexLock mu(self, monitor_lock_);
    Thread* owner = owner_;
    if (owner != nullptr) {
      owner_thread_id = owner->GetThreadId();
    }
    if (owner == self) {
      // We own the monitor, so nobody else can be in here.
      AtraceMonitorUnlock();
      if (lock_count_ == 0) {
        owner_ = nullptr;
        locking_method_ = nullptr;
        locking_dex_pc_ = 0;
        // Wake a contender.
        monitor_contenders_.Signal(self);
      } else {
        --lock_count_;
      }
      return true;
    }
  }
  // We don't own this, so we're not allowed to unlock it.
  // The JNI spec says that we should throw IllegalMonitorStateException in this case.
  FailedUnlock(GetObject(), self->GetThreadId(), owner_thread_id, this);
  return false;
}

void Monitor::Wait(Thread* self, int64_t ms, int32_t ns,
                   bool interruptShouldThrow, ThreadState why) {
  DCHECK(self != nullptr);
  DCHECK(why == kTimedWaiting || why == kWaiting || why == kSleeping);

  monitor_lock_.Lock(self);

  // Make sure that we hold the lock.
  if (owner_ != self) {
    monitor_lock_.Unlock(self);
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before wait()");
    return;
  }

  // We need to turn a zero-length timed wait into a regular wait because
  // Object.wait(0, 0) is defined as Object.wait(0), which is defined as Object.wait().
  if (why == kTimedWaiting && (ms == 0 && ns == 0)) {
    why = kWaiting;
  }

  // Enforce the timeout range.
  if (ms < 0 || ns < 0 || ns > 999999) {
    monitor_lock_.Unlock(self);
    self->ThrowNewExceptionF("Ljava/lang/IllegalArgumentException;",
                             "timeout arguments out of range: ms=%" PRId64 " ns=%d", ms, ns);
    return;
  }

  /*
   * Add ourselves to the set of threads waiting on this monitor, and
   * release our hold.  We need to let it go even if we're a few levels
   * deep in a recursive lock, and we need to restore that later.
   *
   * We append to the wait set ahead of clearing the count and owner
   * fields so the subroutine can check that the calling thread owns
   * the monitor.  Aside from that, the order of member updates is
   * not order sensitive as we hold the pthread mutex.
   */
  AppendToWaitSet(self);
  ++num_waiters_;
  int prev_lock_count = lock_count_;
  lock_count_ = 0;
  owner_ = nullptr;
  ArtMethod* saved_method = locking_method_;
  locking_method_ = nullptr;
  uintptr_t saved_dex_pc = locking_dex_pc_;
  locking_dex_pc_ = 0;

  AtraceMonitorUnlock();  // For the implict Unlock() just above. This will only end the deepest
                          // nesting, but that is enough for the visualization, and corresponds to
                          // the single Lock() we do afterwards.
  AtraceMonitorLock(self, GetObject(), true /* is_wait */);

  bool was_interrupted = false;
  bool timed_out = false;
  {
    // Update thread state. If the GC wakes up, it'll ignore us, knowing
    // that we won't touch any references in this state, and we'll check
    // our suspend mode before we transition out.
    ScopedThreadSuspension sts(self, why);

    // Pseudo-atomically wait on self's wait_cond_ and release the monitor lock.
    MutexLock mu(self, *self->GetWaitMutex());

    // Set wait_monitor_ to the monitor object we will be waiting on. When wait_monitor_ is
    // non-null a notifying or interrupting thread must signal the thread's wait_cond_ to wake it
    // up.
    DCHECK(self->GetWaitMonitor() == nullptr);
    self->SetWaitMonitor(this);

    // Release the monitor lock.
    monitor_contenders_.Signal(self);
    monitor_lock_.Unlock(self);

    // Handle the case where the thread was interrupted before we called wait().
    if (self->IsInterrupted()) {
      was_interrupted = true;
    } else {
      // Wait for a notification or a timeout to occur.
      if (why == kWaiting) {
        self->GetWaitConditionVariable()->Wait(self);
      } else {
        DCHECK(why == kTimedWaiting || why == kSleeping) << why;
        timed_out = self->GetWaitConditionVariable()->TimedWait(self, ms, ns);
      }
      was_interrupted = self->IsInterrupted();
    }
  }

  {
    // We reset the thread's wait_monitor_ field after transitioning back to runnable so
    // that a thread in a waiting/sleeping state has a non-null wait_monitor_ for debugging
    // and diagnostic purposes. (If you reset this earlier, stack dumps will claim that threads
    // are waiting on "null".)
    MutexLock mu(self, *self->GetWaitMutex());
    DCHECK(self->GetWaitMonitor() != nullptr);
    self->SetWaitMonitor(nullptr);
  }

  // Allocate the interrupted exception not holding the monitor lock since it may cause a GC.
  // If the GC requires acquiring the monitor for enqueuing cleared references, this would
  // cause a deadlock if the monitor is held.
  if (was_interrupted && interruptShouldThrow) {
    /*
     * We were interrupted while waiting, or somebody interrupted an
     * un-interruptible thread earlier and we're bailing out immediately.
     *
     * The doc sayeth: "The interrupted status of the current thread is
     * cleared when this exception is thrown."
     */
    self->SetInterrupted(false);
    self->ThrowNewException("Ljava/lang/InterruptedException;", nullptr);
  }

  AtraceMonitorUnlock();  // End Wait().

  // We just slept, tell the runtime callbacks about this.
  Runtime::Current()->GetRuntimeCallbacks()->MonitorWaitFinished(this, timed_out);

  // Re-acquire the monitor and lock.
  Lock<LockReason::kForWait>(self);
  monitor_lock_.Lock(self);
  self->GetWaitMutex()->AssertNotHeld(self);

  /*
   * We remove our thread from wait set after restoring the count
   * and owner fields so the subroutine can check that the calling
   * thread owns the monitor. Aside from that, the order of member
   * updates is not order sensitive as we hold the pthread mutex.
   */
  owner_ = self;
  lock_count_ = prev_lock_count;
  locking_method_ = saved_method;
  locking_dex_pc_ = saved_dex_pc;
  --num_waiters_;
  RemoveFromWaitSet(self);

  monitor_lock_.Unlock(self);
}

void Monitor::Notify(Thread* self) {
  DCHECK(self != nullptr);
  MutexLock mu(self, monitor_lock_);
  // Make sure that we hold the lock.
  if (owner_ != self) {
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before notify()");
    return;
  }
  // Signal the first waiting thread in the wait set.
  while (wait_set_ != nullptr) {
    Thread* thread = wait_set_;
    wait_set_ = thread->GetWaitNext();
    thread->SetWaitNext(nullptr);

    // Check to see if the thread is still waiting.
    MutexLock wait_mu(self, *thread->GetWaitMutex());
    if (thread->GetWaitMonitor() != nullptr) {
      thread->GetWaitConditionVariable()->Signal(self);
      return;
    }
  }
}

void Monitor::NotifyAll(Thread* self) {
  DCHECK(self != nullptr);
  MutexLock mu(self, monitor_lock_);
  // Make sure that we hold the lock.
  if (owner_ != self) {
    ThrowIllegalMonitorStateExceptionF("object not locked by thread before notifyAll()");
    return;
  }
  // Signal all threads in the wait set.
  while (wait_set_ != nullptr) {
    Thread* thread = wait_set_;
    wait_set_ = thread->GetWaitNext();
    thread->SetWaitNext(nullptr);
    thread->Notify();
  }
}

bool Monitor::Deflate(Thread* self, mirror::Object* obj) {
  DCHECK(obj != nullptr);
  // Don't need volatile since we only deflate with mutators suspended.
  LockWord lw(obj->GetLockWord(false));
  // If the lock isn't an inflated monitor, then we don't need to deflate anything.
  if (lw.GetState() == LockWord::kFatLocked) {
    Monitor* monitor = lw.FatLockMonitor();
    DCHECK(monitor != nullptr);
    MutexLock mu(self, monitor->monitor_lock_);
    // Can't deflate if we have anybody waiting on the CV.
    if (monitor->num_waiters_ > 0) {
      return false;
    }
    Thread* owner = monitor->owner_;
    if (owner != nullptr) {
      // Can't deflate if we are locked and have a hash code.
      if (monitor->HasHashCode()) {
        return false;
      }
      // Can't deflate if our lock count is too high.
      if (static_cast<uint32_t>(monitor->lock_count_) > LockWord::kThinLockMaxCount) {
        return false;
      }
      // Deflate to a thin lock.
      LockWord new_lw = LockWord::FromThinLockId(owner->GetThreadId(),
                                                 monitor->lock_count_,
                                                 lw.GCState());
      // Assume no concurrent read barrier state changes as mutators are suspended.
      obj->SetLockWord(new_lw, false);
      VLOG(monitor) << "Deflated " << obj << " to thin lock " << owner->GetTid() << " / "
          << monitor->lock_count_;
    } else if (monitor->HasHashCode()) {
      LockWord new_lw = LockWord::FromHashCode(monitor->GetHashCode(), lw.GCState());
      // Assume no concurrent read barrier state changes as mutators are suspended.
      obj->SetLockWord(new_lw, false);
      VLOG(monitor) << "Deflated " << obj << " to hash monitor " << monitor->GetHashCode();
    } else {
      // No lock and no hash, just put an empty lock word inside the object.
      LockWord new_lw = LockWord::FromDefault(lw.GCState());
      // Assume no concurrent read barrier state changes as mutators are suspended.
      obj->SetLockWord(new_lw, false);
      VLOG(monitor) << "Deflated" << obj << " to empty lock word";
    }
    // The monitor is deflated, mark the object as null so that we know to delete it during the
    // next GC.
    monitor->obj_ = GcRoot<mirror::Object>(nullptr);
  }
  return true;
}

void Monitor::Inflate(Thread* self, Thread* owner, mirror::Object* obj, int32_t hash_code) {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  // Allocate and acquire a new monitor.
  Monitor* m = MonitorPool::CreateMonitor(self, owner, obj, hash_code);
  DCHECK(m != nullptr);
  if (m->Install(self)) {
    if (owner != nullptr) {
      VLOG(monitor) << "monitor: thread" << owner->GetThreadId()
          << " created monitor " << m << " for object " << obj;
    } else {
      VLOG(monitor) << "monitor: Inflate with hashcode " << hash_code
          << " created monitor " << m << " for object " << obj;
    }
    Runtime::Current()->GetMonitorList()->Add(m);
    CHECK_EQ(obj->GetLockWord(true).GetState(), LockWord::kFatLocked);
  } else {
    MonitorPool::ReleaseMonitor(self, m);
  }
}

void Monitor::InflateThinLocked(Thread* self, Handle<mirror::Object> obj, LockWord lock_word,
                                uint32_t hash_code) {
  DCHECK_EQ(lock_word.GetState(), LockWord::kThinLocked);
  uint32_t owner_thread_id = lock_word.ThinLockOwner();
  if (owner_thread_id == self->GetThreadId()) {
    // We own the monitor, we can easily inflate it.
    Inflate(self, self, obj.Get(), hash_code);
  } else {
    ThreadList* thread_list = Runtime::Current()->GetThreadList();
    // Suspend the owner, inflate. First change to blocked and give up mutator_lock_.
    self->SetMonitorEnterObject(obj.Get());
    bool timed_out;
    Thread* owner;
    {
      ScopedThreadSuspension sts(self, kWaitingForLockInflation);
      owner = thread_list->SuspendThreadByThreadId(owner_thread_id,
                                                   SuspendReason::kInternal,
                                                   &timed_out);
    }
    if (owner != nullptr) {
      // We succeeded in suspending the thread, check the lock's status didn't change.
      lock_word = obj->GetLockWord(true);
      if (lock_word.GetState() == LockWord::kThinLocked &&
          lock_word.ThinLockOwner() == owner_thread_id) {
        // Go ahead and inflate the lock.
        Inflate(self, owner, obj.Get(), hash_code);
      }
      bool resumed = thread_list->Resume(owner, SuspendReason::kInternal);
      DCHECK(resumed);
    }
    self->SetMonitorEnterObject(nullptr);
  }
}

// Fool annotalysis into thinking that the lock on obj is acquired.
static mirror::Object* FakeLock(mirror::Object* obj)
    EXCLUSIVE_LOCK_FUNCTION(obj) NO_THREAD_SAFETY_ANALYSIS {
  return obj;
}

// Fool annotalysis into thinking that the lock on obj is release.
static mirror::Object* FakeUnlock(mirror::Object* obj)
    UNLOCK_FUNCTION(obj) NO_THREAD_SAFETY_ANALYSIS {
  return obj;
}

mirror::Object* Monitor::MonitorEnter(Thread* self, mirror::Object* obj, bool trylock) {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  self->AssertThreadSuspensionIsAllowable();
  obj = FakeLock(obj);
  uint32_t thread_id = self->GetThreadId();
  size_t contention_count = 0;
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_obj(hs.NewHandle(obj));
  while (true) {
    // We initially read the lockword with ordinary Java/relaxed semantics. When stronger
    // semantics are needed, we address it below. Since GetLockWord bottoms out to a relaxed load,
    // we can fix it later, in an infrequently executed case, with a fence.
    LockWord lock_word = h_obj->GetLockWord(false);
    switch (lock_word.GetState()) {
      case LockWord::kUnlocked: {
        // No ordering required for preceding lockword read, since we retest.
        LockWord thin_locked(LockWord::FromThinLockId(thread_id, 0, lock_word.GCState()));
        if (h_obj->CasLockWordWeakAcquire(lock_word, thin_locked)) {
          AtraceMonitorLock(self, h_obj.Get(), false /* is_wait */);
          return h_obj.Get();  // Success!
        }
        continue;  // Go again.
      }
      case LockWord::kThinLocked: {
        uint32_t owner_thread_id = lock_word.ThinLockOwner();
        if (owner_thread_id == thread_id) {
          // No ordering required for initial lockword read.
          // We own the lock, increase the recursion count.
          uint32_t new_count = lock_word.ThinLockCount() + 1;
          if (LIKELY(new_count <= LockWord::kThinLockMaxCount)) {
            LockWord thin_locked(LockWord::FromThinLockId(thread_id,
                                                          new_count,
                                                          lock_word.GCState()));
            // Only this thread pays attention to the count. Thus there is no need for stronger
            // than relaxed memory ordering.
            if (!kUseReadBarrier) {
              h_obj->SetLockWord(thin_locked, false /* volatile */);
              AtraceMonitorLock(self, h_obj.Get(), false /* is_wait */);
              return h_obj.Get();  // Success!
            } else {
              // Use CAS to preserve the read barrier state.
              if (h_obj->CasLockWordWeakRelaxed(lock_word, thin_locked)) {
                AtraceMonitorLock(self, h_obj.Get(), false /* is_wait */);
                return h_obj.Get();  // Success!
              }
            }
            continue;  // Go again.
          } else {
            // We'd overflow the recursion count, so inflate the monitor.
            InflateThinLocked(self, h_obj, lock_word, 0);
          }
        } else {
          if (trylock) {
            return nullptr;
          }
          // Contention.
          contention_count++;
          Runtime* runtime = Runtime::Current();
          if (contention_count <= runtime->GetMaxSpinsBeforeThinLockInflation()) {
            // TODO: Consider switching the thread state to kWaitingForLockInflation when we are
            // yielding.  Use sched_yield instead of NanoSleep since NanoSleep can wait much longer
            // than the parameter you pass in. This can cause thread suspension to take excessively
            // long and make long pauses. See b/16307460.
            // TODO: We should literally spin first, without sched_yield. Sched_yield either does
            // nothing (at significant expense), or guarantees that we wait at least microseconds.
            // If the owner is running, I would expect the median lock hold time to be hundreds
            // of nanoseconds or less.
            sched_yield();
          } else {
            contention_count = 0;
            // No ordering required for initial lockword read. Install rereads it anyway.
            InflateThinLocked(self, h_obj, lock_word, 0);
          }
        }
        continue;  // Start from the beginning.
      }
      case LockWord::kFatLocked: {
        // We should have done an acquire read of the lockword initially, to ensure
        // visibility of the monitor data structure. Use an explicit fence instead.
        QuasiAtomic::ThreadFenceAcquire();
        Monitor* mon = lock_word.FatLockMonitor();
        if (trylock) {
          return mon->TryLock(self) ? h_obj.Get() : nullptr;
        } else {
          mon->Lock(self);
          return h_obj.Get();  // Success!
        }
      }
      case LockWord::kHashCode:
        // Inflate with the existing hashcode.
        // Again no ordering required for initial lockword read, since we don't rely
        // on the visibility of any prior computation.
        Inflate(self, nullptr, h_obj.Get(), lock_word.GetHashCode());
        continue;  // Start from the beginning.
      default: {
        LOG(FATAL) << "Invalid monitor state " << lock_word.GetState();
        UNREACHABLE();
      }
    }
  }
}

bool Monitor::MonitorExit(Thread* self, mirror::Object* obj) {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  self->AssertThreadSuspensionIsAllowable();
  obj = FakeUnlock(obj);
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_obj(hs.NewHandle(obj));
  while (true) {
    LockWord lock_word = obj->GetLockWord(true);
    switch (lock_word.GetState()) {
      case LockWord::kHashCode:
        // Fall-through.
      case LockWord::kUnlocked:
        FailedUnlock(h_obj.Get(), self->GetThreadId(), 0u, nullptr);
        return false;  // Failure.
      case LockWord::kThinLocked: {
        uint32_t thread_id = self->GetThreadId();
        uint32_t owner_thread_id = lock_word.ThinLockOwner();
        if (owner_thread_id != thread_id) {
          FailedUnlock(h_obj.Get(), thread_id, owner_thread_id, nullptr);
          return false;  // Failure.
        } else {
          // We own the lock, decrease the recursion count.
          LockWord new_lw = LockWord::Default();
          if (lock_word.ThinLockCount() != 0) {
            uint32_t new_count = lock_word.ThinLockCount() - 1;
            new_lw = LockWord::FromThinLockId(thread_id, new_count, lock_word.GCState());
          } else {
            new_lw = LockWord::FromDefault(lock_word.GCState());
          }
          if (!kUseReadBarrier) {
            DCHECK_EQ(new_lw.ReadBarrierState(), 0U);
            // TODO: This really only needs memory_order_release, but we currently have
            // no way to specify that. In fact there seem to be no legitimate uses of SetLockWord
            // with a final argument of true. This slows down x86 and ARMv7, but probably not v8.
            h_obj->SetLockWord(new_lw, true);
            AtraceMonitorUnlock();
            // Success!
            return true;
          } else {
            // Use CAS to preserve the read barrier state.
            if (h_obj->CasLockWordWeakRelease(lock_word, new_lw)) {
              AtraceMonitorUnlock();
              // Success!
              return true;
            }
          }
          continue;  // Go again.
        }
      }
      case LockWord::kFatLocked: {
        Monitor* mon = lock_word.FatLockMonitor();
        return mon->Unlock(self);
      }
      default: {
        LOG(FATAL) << "Invalid monitor state " << lock_word.GetState();
        return false;
      }
    }
  }
}

void Monitor::Wait(Thread* self, mirror::Object *obj, int64_t ms, int32_t ns,
                   bool interruptShouldThrow, ThreadState why) {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_obj(hs.NewHandle(obj));

  Runtime::Current()->GetRuntimeCallbacks()->ObjectWaitStart(h_obj, ms);
  if (UNLIKELY(self->ObserveAsyncException() || self->IsExceptionPending())) {
    // See b/65558434 for information on handling of exceptions here.
    return;
  }

  LockWord lock_word = h_obj->GetLockWord(true);
  while (lock_word.GetState() != LockWord::kFatLocked) {
    switch (lock_word.GetState()) {
      case LockWord::kHashCode:
        // Fall-through.
      case LockWord::kUnlocked:
        ThrowIllegalMonitorStateExceptionF("object not locked by thread before wait()");
        return;  // Failure.
      case LockWord::kThinLocked: {
        uint32_t thread_id = self->GetThreadId();
        uint32_t owner_thread_id = lock_word.ThinLockOwner();
        if (owner_thread_id != thread_id) {
          ThrowIllegalMonitorStateExceptionF("object not locked by thread before wait()");
          return;  // Failure.
        } else {
          // We own the lock, inflate to enqueue ourself on the Monitor. May fail spuriously so
          // re-load.
          Inflate(self, self, h_obj.Get(), 0);
          lock_word = h_obj->GetLockWord(true);
        }
        break;
      }
      case LockWord::kFatLocked:  // Unreachable given the loop condition above. Fall-through.
      default: {
        LOG(FATAL) << "Invalid monitor state " << lock_word.GetState();
        return;
      }
    }
  }
  Monitor* mon = lock_word.FatLockMonitor();
  mon->Wait(self, ms, ns, interruptShouldThrow, why);
}

void Monitor::DoNotify(Thread* self, mirror::Object* obj, bool notify_all) {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  LockWord lock_word = obj->GetLockWord(true);
  switch (lock_word.GetState()) {
    case LockWord::kHashCode:
      // Fall-through.
    case LockWord::kUnlocked:
      ThrowIllegalMonitorStateExceptionF("object not locked by thread before notify()");
      return;  // Failure.
    case LockWord::kThinLocked: {
      uint32_t thread_id = self->GetThreadId();
      uint32_t owner_thread_id = lock_word.ThinLockOwner();
      if (owner_thread_id != thread_id) {
        ThrowIllegalMonitorStateExceptionF("object not locked by thread before notify()");
        return;  // Failure.
      } else {
        // We own the lock but there's no Monitor and therefore no waiters.
        return;  // Success.
      }
    }
    case LockWord::kFatLocked: {
      Monitor* mon = lock_word.FatLockMonitor();
      if (notify_all) {
        mon->NotifyAll(self);
      } else {
        mon->Notify(self);
      }
      return;  // Success.
    }
    default: {
      LOG(FATAL) << "Invalid monitor state " << lock_word.GetState();
      return;
    }
  }
}

uint32_t Monitor::GetLockOwnerThreadId(mirror::Object* obj) {
  DCHECK(obj != nullptr);
  LockWord lock_word = obj->GetLockWord(true);
  switch (lock_word.GetState()) {
    case LockWord::kHashCode:
      // Fall-through.
    case LockWord::kUnlocked:
      return ThreadList::kInvalidThreadId;
    case LockWord::kThinLocked:
      return lock_word.ThinLockOwner();
    case LockWord::kFatLocked: {
      Monitor* mon = lock_word.FatLockMonitor();
      return mon->GetOwnerThreadId();
    }
    default: {
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
    }
  }
}

ThreadState Monitor::FetchState(const Thread* thread,
                                /* out */ mirror::Object** monitor_object,
                                /* out */ uint32_t* lock_owner_tid) {
  DCHECK(monitor_object != nullptr);
  DCHECK(lock_owner_tid != nullptr);

  *monitor_object = nullptr;
  *lock_owner_tid = ThreadList::kInvalidThreadId;

  ThreadState state = thread->GetState();

  switch (state) {
    case kWaiting:
    case kTimedWaiting:
    case kSleeping:
    {
      Thread* self = Thread::Current();
      MutexLock mu(self, *thread->GetWaitMutex());
      Monitor* monitor = thread->GetWaitMonitor();
      if (monitor != nullptr) {
        *monitor_object = monitor->GetObject();
      }
    }
    break;

    case kBlocked:
    case kWaitingForLockInflation:
    {
      mirror::Object* lock_object = thread->GetMonitorEnterObject();
      if (lock_object != nullptr) {
        if (kUseReadBarrier && Thread::Current()->GetIsGcMarking()) {
          // We may call Thread::Dump() in the middle of the CC thread flip and this thread's stack
          // may have not been flipped yet and "pretty_object" may be a from-space (stale) ref, in
          // which case the GetLockOwnerThreadId() call below will crash. So explicitly mark/forward
          // it here.
          lock_object = ReadBarrier::Mark(lock_object);
        }
        *monitor_object = lock_object;
        *lock_owner_tid = lock_object->GetLockOwnerThreadId();
      }
    }
    break;

    default:
      break;
  }

  return state;
}

mirror::Object* Monitor::GetContendedMonitor(Thread* thread) {
  // This is used to implement JDWP's ThreadReference.CurrentContendedMonitor, and has a bizarre
  // definition of contended that includes a monitor a thread is trying to enter...
  mirror::Object* result = thread->GetMonitorEnterObject();
  if (result == nullptr) {
    // ...but also a monitor that the thread is waiting on.
    MutexLock mu(Thread::Current(), *thread->GetWaitMutex());
    Monitor* monitor = thread->GetWaitMonitor();
    if (monitor != nullptr) {
      result = monitor->GetObject();
    }
  }
  return result;
}

void Monitor::VisitLocks(StackVisitor* stack_visitor, void (*callback)(mirror::Object*, void*),
                         void* callback_context, bool abort_on_failure) {
  ArtMethod* m = stack_visitor->GetMethod();
  CHECK(m != nullptr);

  // Native methods are an easy special case.
  // TODO: use the JNI implementation's table of explicit MonitorEnter calls and dump those too.
  if (m->IsNative()) {
    if (m->IsSynchronized()) {
      mirror::Object* jni_this =
          stack_visitor->GetCurrentHandleScope(sizeof(void*))->GetReference(0);
      callback(jni_this, callback_context);
    }
    return;
  }

  // Proxy methods should not be synchronized.
  if (m->IsProxyMethod()) {
    CHECK(!m->IsSynchronized());
    return;
  }

  // Is there any reason to believe there's any synchronization in this method?
  CHECK(m->GetCodeItem() != nullptr) << m->PrettyMethod();
  CodeItemDataAccessor accessor(m->DexInstructionData());
  if (accessor.TriesSize() == 0) {
    return;  // No "tries" implies no synchronization, so no held locks to report.
  }

  // Get the dex pc. If abort_on_failure is false, GetDexPc will not abort in the case it cannot
  // find the dex pc, and instead return kDexNoIndex. Then bail out, as it indicates we have an
  // inconsistent stack anyways.
  uint32_t dex_pc = stack_visitor->GetDexPc(abort_on_failure);
  if (!abort_on_failure && dex_pc == dex::kDexNoIndex) {
    LOG(ERROR) << "Could not find dex_pc for " << m->PrettyMethod();
    return;
  }

  // Ask the verifier for the dex pcs of all the monitor-enter instructions corresponding to
  // the locks held in this stack frame.
  std::vector<verifier::MethodVerifier::DexLockInfo> monitor_enter_dex_pcs;
  verifier::MethodVerifier::FindLocksAtDexPc(m, dex_pc, &monitor_enter_dex_pcs);
  for (verifier::MethodVerifier::DexLockInfo& dex_lock_info : monitor_enter_dex_pcs) {
    // As a debug check, check that dex PC corresponds to a monitor-enter.
    if (kIsDebugBuild) {
      const Instruction& monitor_enter_instruction = accessor.InstructionAt(dex_lock_info.dex_pc);
      CHECK_EQ(monitor_enter_instruction.Opcode(), Instruction::MONITOR_ENTER)
          << "expected monitor-enter @" << dex_lock_info.dex_pc << "; was "
          << reinterpret_cast<const void*>(&monitor_enter_instruction);
    }

    // Iterate through the set of dex registers, as the compiler may not have held all of them
    // live.
    bool success = false;
    for (uint32_t dex_reg : dex_lock_info.dex_registers) {
      uint32_t value;
      success = stack_visitor->GetVReg(m, dex_reg, kReferenceVReg, &value);
      if (success) {
        mirror::Object* o = reinterpret_cast<mirror::Object*>(value);
        callback(o, callback_context);
        break;
      }
    }
    DCHECK(success) << "Failed to find/read reference for monitor-enter at dex pc "
                    << dex_lock_info.dex_pc
                    << " in method "
                    << m->PrettyMethod();
    if (!success) {
      LOG(WARNING) << "Had a lock reported for dex pc " << dex_lock_info.dex_pc
                   << " but was not able to fetch a corresponding object!";
    }
  }
}

bool Monitor::IsValidLockWord(LockWord lock_word) {
  switch (lock_word.GetState()) {
    case LockWord::kUnlocked:
      // Nothing to check.
      return true;
    case LockWord::kThinLocked:
      // Basic sanity check of owner.
      return lock_word.ThinLockOwner() != ThreadList::kInvalidThreadId;
    case LockWord::kFatLocked: {
      // Check the  monitor appears in the monitor list.
      Monitor* mon = lock_word.FatLockMonitor();
      MonitorList* list = Runtime::Current()->GetMonitorList();
      MutexLock mu(Thread::Current(), list->monitor_list_lock_);
      for (Monitor* list_mon : list->list_) {
        if (mon == list_mon) {
          return true;  // Found our monitor.
        }
      }
      return false;  // Fail - unowned monitor in an object.
    }
    case LockWord::kHashCode:
      return true;
    default:
      LOG(FATAL) << "Unreachable";
      UNREACHABLE();
  }
}

bool Monitor::IsLocked() REQUIRES_SHARED(Locks::mutator_lock_) {
  MutexLock mu(Thread::Current(), monitor_lock_);
  return owner_ != nullptr;
}

void Monitor::TranslateLocation(ArtMethod* method,
                                uint32_t dex_pc,
                                const char** source_file,
                                int32_t* line_number) {
  // If method is null, location is unknown
  if (method == nullptr) {
    *source_file = "";
    *line_number = 0;
    return;
  }
  *source_file = method->GetDeclaringClassSourceFile();
  if (*source_file == nullptr) {
    *source_file = "";
  }
  *line_number = method->GetLineNumFromDexPC(dex_pc);
}

uint32_t Monitor::GetOwnerThreadId() {
  MutexLock mu(Thread::Current(), monitor_lock_);
  Thread* owner = owner_;
  if (owner != nullptr) {
    return owner->GetThreadId();
  } else {
    return ThreadList::kInvalidThreadId;
  }
}

MonitorList::MonitorList()
    : allow_new_monitors_(true), monitor_list_lock_("MonitorList lock", kMonitorListLock),
      monitor_add_condition_("MonitorList disallow condition", monitor_list_lock_) {
}

MonitorList::~MonitorList() {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  // Release all monitors to the pool.
  // TODO: Is it an invariant that *all* open monitors are in the list? Then we could
  // clear faster in the pool.
  MonitorPool::ReleaseMonitors(self, &list_);
}

void MonitorList::DisallowNewMonitors() {
  CHECK(!kUseReadBarrier);
  MutexLock mu(Thread::Current(), monitor_list_lock_);
  allow_new_monitors_ = false;
}

void MonitorList::AllowNewMonitors() {
  CHECK(!kUseReadBarrier);
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  allow_new_monitors_ = true;
  monitor_add_condition_.Broadcast(self);
}

void MonitorList::BroadcastForNewMonitors() {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  monitor_add_condition_.Broadcast(self);
}

void MonitorList::Add(Monitor* m) {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  // CMS needs this to block for concurrent reference processing because an object allocated during
  // the GC won't be marked and concurrent reference processing would incorrectly clear the JNI weak
  // ref. But CC (kUseReadBarrier == true) doesn't because of the to-space invariant.
  while (!kUseReadBarrier && UNLIKELY(!allow_new_monitors_)) {
    // Check and run the empty checkpoint before blocking so the empty checkpoint will work in the
    // presence of threads blocking for weak ref access.
    self->CheckEmptyCheckpointFromWeakRefAccess(&monitor_list_lock_);
    monitor_add_condition_.WaitHoldingLocks(self);
  }
  list_.push_front(m);
}

void MonitorList::SweepMonitorList(IsMarkedVisitor* visitor) {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  for (auto it = list_.begin(); it != list_.end(); ) {
    Monitor* m = *it;
    // Disable the read barrier in GetObject() as this is called by GC.
    mirror::Object* obj = m->GetObject<kWithoutReadBarrier>();
    // The object of a monitor can be null if we have deflated it.
    mirror::Object* new_obj = obj != nullptr ? visitor->IsMarked(obj) : nullptr;
    if (new_obj == nullptr) {
      VLOG(monitor) << "freeing monitor " << m << " belonging to unmarked object "
                    << obj;
      MonitorPool::ReleaseMonitor(self, m);
      it = list_.erase(it);
    } else {
      m->SetObject(new_obj);
      ++it;
    }
  }
}

size_t MonitorList::Size() {
  Thread* self = Thread::Current();
  MutexLock mu(self, monitor_list_lock_);
  return list_.size();
}

class MonitorDeflateVisitor : public IsMarkedVisitor {
 public:
  MonitorDeflateVisitor() : self_(Thread::Current()), deflate_count_(0) {}

  virtual mirror::Object* IsMarked(mirror::Object* object) OVERRIDE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Monitor::Deflate(self_, object)) {
      DCHECK_NE(object->GetLockWord(true).GetState(), LockWord::kFatLocked);
      ++deflate_count_;
      // If we deflated, return null so that the monitor gets removed from the array.
      return nullptr;
    }
    return object;  // Monitor was not deflated.
  }

  Thread* const self_;
  size_t deflate_count_;
};

size_t MonitorList::DeflateMonitors() {
  MonitorDeflateVisitor visitor;
  Locks::mutator_lock_->AssertExclusiveHeld(visitor.self_);
  SweepMonitorList(&visitor);
  return visitor.deflate_count_;
}

MonitorInfo::MonitorInfo(mirror::Object* obj) : owner_(nullptr), entry_count_(0) {
  DCHECK(obj != nullptr);
  LockWord lock_word = obj->GetLockWord(true);
  switch (lock_word.GetState()) {
    case LockWord::kUnlocked:
      // Fall-through.
    case LockWord::kForwardingAddress:
      // Fall-through.
    case LockWord::kHashCode:
      break;
    case LockWord::kThinLocked:
      owner_ = Runtime::Current()->GetThreadList()->FindThreadByThreadId(lock_word.ThinLockOwner());
      DCHECK(owner_ != nullptr) << "Thin-locked without owner!";
      entry_count_ = 1 + lock_word.ThinLockCount();
      // Thin locks have no waiters.
      break;
    case LockWord::kFatLocked: {
      Monitor* mon = lock_word.FatLockMonitor();
      owner_ = mon->owner_;
      // Here it is okay for the owner to be null since we don't reset the LockWord back to
      // kUnlocked until we get a GC. In cases where this hasn't happened yet we will have a fat
      // lock without an owner.
      if (owner_ != nullptr) {
        entry_count_ = 1 + mon->lock_count_;
      } else {
        DCHECK_EQ(mon->lock_count_, 0) << "Monitor is fat-locked without any owner!";
      }
      for (Thread* waiter = mon->wait_set_; waiter != nullptr; waiter = waiter->GetWaitNext()) {
        waiters_.push_back(waiter);
      }
      break;
    }
  }
}

}  // namespace art
