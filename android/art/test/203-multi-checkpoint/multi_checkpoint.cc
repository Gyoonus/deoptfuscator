/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "art_method-inl.h"
#include "base/mutex-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_pool.h"

namespace art {

struct TestClosure : public Closure {
  bool first_run_start;
  bool first_run_end;
  bool second_run;
  bool second_run_interleaved;

  void Run(Thread* self) OVERRIDE {
    CHECK_EQ(self, Thread::Current()) << "Not running on target thread!";
    if (!first_run_start) {
      CHECK(!second_run);
      first_run_start = true;
      // Suspend ourself so that we will perform the second run.
      {
        ScopedObjectAccess soa(self);
        self->FullSuspendCheck();
      }
      first_run_end = true;
    } else {
      CHECK(!second_run);
      CHECK(first_run_start);
      second_run = true;
      second_run_interleaved = !first_run_end;
    }
  }

  void Check() {
    CHECK(first_run_start);
    CHECK(first_run_end);
    CHECK(second_run);
    CHECK(second_run_interleaved);
  }
};

static TestClosure gTestClosure = {};

extern "C" JNIEXPORT void JNICALL Java_Main_checkCheckpointsRun(JNIEnv*, jclass) {
  gTestClosure.Check();
}

struct SetupClosure : public Closure {
  void Run(Thread* self) OVERRIDE {
    CHECK_EQ(self, Thread::Current()) << "Not running on target thread!";
    ScopedObjectAccess soa(self);
    MutexLock tscl_mu(self, *Locks::thread_suspend_count_lock_);
    // Both should succeed since we are in runnable and have the lock.
    CHECK(self->RequestCheckpoint(&gTestClosure)) << "Could not set first checkpoint.";
    CHECK(self->RequestCheckpoint(&gTestClosure)) << "Could not set second checkpoint.";
  }
};

static SetupClosure gSetupClosure = {};

extern "C" JNIEXPORT void JNICALL Java_Main_pushCheckpoints(JNIEnv*, jclass, jobject thr) {
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);
  MutexLock tll_mu(self, *Locks::thread_list_lock_);
  Thread* target = Thread::FromManagedThread(soa, thr);
  while (true) {
    MutexLock tscl_mu(self, *Locks::thread_suspend_count_lock_);
    if (target->RequestCheckpoint(&gSetupClosure)) {
      break;
    }
  }
}

}  // namespace art
