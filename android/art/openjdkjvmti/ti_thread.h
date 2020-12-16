/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef ART_OPENJDKJVMTI_TI_THREAD_H_
#define ART_OPENJDKJVMTI_TI_THREAD_H_

#include "jni.h"
#include "jvmti.h"

#include "base/macros.h"
#include "base/mutex.h"

namespace art {
class ArtField;
class ScopedObjectAccessAlreadyRunnable;
class Thread;
class Closure;
}  // namespace art

namespace openjdkjvmti {

class EventHandler;

class ThreadUtil {
 public:
  static void Register(EventHandler* event_handler);
  static void Unregister();

  // To be called when it is safe to cache data. This means that we have at least entered the
  // RuntimePhase::kInit but we might or might not have already called VMInit event.
  static void CacheData();

  // Called just after we have sent the VMInit callback so that ThreadUtil can do final setup. This
  // ensures that there are no timing issues between the two callbacks.
  static void VMInitEventSent() REQUIRES_SHARED(art::Locks::mutator_lock_);

  // Handle a jvmtiEnv going away.
  static void RemoveEnvironment(jvmtiEnv* env);

  static jvmtiError GetAllThreads(jvmtiEnv* env, jint* threads_count_ptr, jthread** threads_ptr);

  static jvmtiError GetCurrentThread(jvmtiEnv* env, jthread* thread_ptr);

  static jvmtiError GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr);

  static jvmtiError GetThreadState(jvmtiEnv* env, jthread thread, jint* thread_state_ptr);

  static jvmtiError SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data);
  static jvmtiError GetThreadLocalStorage(jvmtiEnv* env, jthread thread, void** data_ptr);

  static jvmtiError RunAgentThread(jvmtiEnv* env,
                                   jthread thread,
                                   jvmtiStartFunction proc,
                                   const void* arg,
                                   jint priority);

  static jvmtiError SuspendThread(jvmtiEnv* env, jthread thread);
  static jvmtiError ResumeThread(jvmtiEnv* env, jthread thread);

  static jvmtiError SuspendThreadList(jvmtiEnv* env,
                                      jint request_count,
                                      const jthread* threads,
                                      jvmtiError* results);
  static jvmtiError ResumeThreadList(jvmtiEnv* env,
                                     jint request_count,
                                     const jthread* threads,
                                     jvmtiError* results);

  static jvmtiError StopThread(jvmtiEnv* env, jthread thr, jobject exception);
  static jvmtiError InterruptThread(jvmtiEnv* env, jthread thr);

  // Returns true if we decoded the thread and it is alive, false otherwise with an appropriate
  // error placed into 'err'. A thread is alive if it has had it's 'start' function called and has
  // (or at least could have) executed managed code and has not yet returned past it's first managed
  // frame. This means that the thread returned might have IsStillStarting() return true. Code that
  // does not consider that alive should check manually.
  static bool GetAliveNativeThread(jthread thread,
                                   const art::ScopedObjectAccessAlreadyRunnable& soa,
                                   /*out*/ art::Thread** thr,
                                   /*out*/ jvmtiError* err)
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      REQUIRES(art::Locks::thread_list_lock_);

  // Returns true if we decoded the thread, false otherwise with an appropriate error placed into
  // 'err'
  static bool GetNativeThread(jthread thread,
                              const art::ScopedObjectAccessAlreadyRunnable& soa,
                              /*out*/ art::Thread** thr,
                              /*out*/ jvmtiError* err)
      REQUIRES_SHARED(art::Locks::mutator_lock_)
      REQUIRES(art::Locks::thread_list_lock_);

  // Go to sleep if this thread is suspended.
  static void SuspendCheck(art::Thread* self)
    REQUIRES(!art::Locks::mutator_lock_, !art::Locks::user_code_suspension_lock_);

  // Returns true if the thread would be suspended if it locks the mutator-lock or calls
  // SuspendCheck. This function is called with the user_code_suspension_lock already held.
  static bool WouldSuspendForUserCodeLocked(art::Thread* self)
    REQUIRES(art::Locks::user_code_suspension_lock_,
             !art::Locks::thread_suspend_count_lock_);

  // Returns true if this thread would go to sleep if it locks the mutator-lock or calls
  // SuspendCheck.
  static bool WouldSuspendForUserCode(art::Thread* self)
    REQUIRES(!art::Locks::user_code_suspension_lock_,
             !art::Locks::thread_suspend_count_lock_);

 private:
  // We need to make sure only one thread tries to suspend threads at a time so we can get the
  // 'suspend-only-once' behavior the spec requires. Internally, ART considers suspension to be a
  // counted state, allowing a single thread to be suspended multiple times by different users. This
  // makes mapping into the JVMTI idea of thread suspension difficult. We have decided to split the
  // difference and ensure that JVMTI tries to treat suspension as the boolean flag as much as
  // possible with the suspend/resume methods but only do best effort. On the other hand
  // GetThreadState will be totally accurate as much as possible. This means that calling
  // ResumeThread on a thread that has state JVMTI_THREAD_STATE_SUSPENDED will not necessarily
  // cause the thread to wake up if the thread is suspended for the debugger or gc or something.
  static jvmtiError SuspendSelf(art::Thread* self)
      REQUIRES(!art::Locks::mutator_lock_, !art::Locks::user_code_suspension_lock_);
  static jvmtiError SuspendOther(art::Thread* self, jthread target_jthread)
      REQUIRES(!art::Locks::mutator_lock_, !art::Locks::user_code_suspension_lock_);

  static art::ArtField* context_class_loader_;
};

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_TI_THREAD_H_
