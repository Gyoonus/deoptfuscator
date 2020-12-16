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

#ifndef ART_OPENJDKJVMTI_DEOPT_MANAGER_H_
#define ART_OPENJDKJVMTI_DEOPT_MANAGER_H_

#include <atomic>
#include <unordered_map>

#include "jni.h"
#include "jvmti.h"

#include "base/mutex.h"
#include "runtime_callbacks.h"
#include "ti_breakpoint.h"

namespace art {
class ArtMethod;
namespace mirror {
class Class;
}  // namespace mirror
}  // namespace art

namespace openjdkjvmti {

class DeoptManager;

struct JvmtiMethodInspectionCallback : public art::MethodInspectionCallback {
 public:
  explicit JvmtiMethodInspectionCallback(DeoptManager* manager) : manager_(manager) {}

  bool IsMethodBeingInspected(art::ArtMethod* method)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_);

  bool IsMethodSafeToJit(art::ArtMethod* method)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_);

  bool MethodNeedsDebugVersion(art::ArtMethod* method)
      OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_);

 private:
  DeoptManager* manager_;
};

class ScopedDeoptimizationContext;

class DeoptManager {
 public:
  DeoptManager();

  void Setup();
  void Shutdown();

  void RemoveDeoptimizationRequester() REQUIRES(!deoptimization_status_lock_,
                                                !art::Roles::uninterruptible_);
  void AddDeoptimizationRequester() REQUIRES(!deoptimization_status_lock_,
                                             !art::Roles::uninterruptible_);
  bool MethodHasBreakpoints(art::ArtMethod* method)
      REQUIRES(!deoptimization_status_lock_);

  void RemoveMethodBreakpoint(art::ArtMethod* method)
      REQUIRES(!deoptimization_status_lock_, !art::Roles::uninterruptible_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  void AddMethodBreakpoint(art::ArtMethod* method)
      REQUIRES(!deoptimization_status_lock_, !art::Roles::uninterruptible_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  void AddDeoptimizeAllMethods()
      REQUIRES(!deoptimization_status_lock_, !art::Roles::uninterruptible_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  void RemoveDeoptimizeAllMethods()
      REQUIRES(!deoptimization_status_lock_, !art::Roles::uninterruptible_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  void DeoptimizeThread(art::Thread* target) REQUIRES_SHARED(art::Locks::mutator_lock_);
  void DeoptimizeAllThreads() REQUIRES_SHARED(art::Locks::mutator_lock_);

  void FinishSetup()
      REQUIRES(!deoptimization_status_lock_, !art::Roles::uninterruptible_)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

  static DeoptManager* Get();

  bool HaveLocalsChanged() const {
    return set_local_variable_called_.load();
  }

  void SetLocalsUpdated() {
    set_local_variable_called_.store(true);
  }

 private:
  bool MethodHasBreakpointsLocked(art::ArtMethod* method)
      REQUIRES(breakpoint_status_lock_);

  // Wait until nothing is currently in the middle of deoptimizing/undeoptimizing something. This is
  // needed to ensure that everything is synchronized since threads need to drop the
  // deoptimization_status_lock_ while deoptimizing methods.
  void WaitForDeoptimizationToFinish(art::Thread* self)
      RELEASE(deoptimization_status_lock_) REQUIRES(!art::Locks::mutator_lock_);

  void WaitForDeoptimizationToFinishLocked(art::Thread* self)
      REQUIRES(deoptimization_status_lock_, !art::Locks::mutator_lock_);

  void AddDeoptimizeAllMethodsLocked(art::Thread* self)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  void RemoveDeoptimizeAllMethodsLocked(art::Thread* self)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  void PerformGlobalDeoptimization(art::Thread* self)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  void PerformGlobalUndeoptimization(art::Thread* self)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  void PerformLimitedDeoptimization(art::Thread* self, art::ArtMethod* method)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  void PerformLimitedUndeoptimization(art::Thread* self, art::ArtMethod* method)
      RELEASE(deoptimization_status_lock_)
      REQUIRES(!art::Roles::uninterruptible_, !art::Locks::mutator_lock_);

  static constexpr const char* kDeoptManagerInstrumentationKey = "JVMTI_DeoptManager";

  art::Mutex deoptimization_status_lock_ ACQUIRED_BEFORE(art::Locks::classlinker_classes_lock_);
  art::ConditionVariable deoptimization_condition_ GUARDED_BY(deoptimization_status_lock_);
  bool performing_deoptimization_ GUARDED_BY(deoptimization_status_lock_);

  // Number of times we have gotten requests to deopt everything.
  uint32_t global_deopt_count_ GUARDED_BY(deoptimization_status_lock_);

  // Number of users of deoptimization there currently are.
  uint32_t deopter_count_ GUARDED_BY(deoptimization_status_lock_);

  // A mutex that just protects the breakpoint-status map. This mutex should always be at the
  // bottom of the lock hierarchy. Nothing more should be locked if we hold this.
  art::Mutex breakpoint_status_lock_ ACQUIRED_BEFORE(art::Locks::abort_lock_);
  // A map from methods to the number of breakpoints in them from all envs.
  std::unordered_map<art::ArtMethod*, uint32_t> breakpoint_status_
      GUARDED_BY(breakpoint_status_lock_);

  // The MethodInspectionCallback we use to tell the runtime if we care about particular methods.
  JvmtiMethodInspectionCallback inspection_callback_;

  // Set to true if anything calls SetLocalVariables on any thread since we need to be careful about
  // OSR after this.
  std::atomic<bool> set_local_variable_called_;

  // Helper for setting up/tearing-down for deoptimization.
  friend class ScopedDeoptimizationContext;
};

}  // namespace openjdkjvmti
#endif  // ART_OPENJDKJVMTI_DEOPT_MANAGER_H_
