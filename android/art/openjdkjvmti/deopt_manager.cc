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

#include <functional>

#include "deopt_manager.h"

#include "art_jvmti.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/mutex-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/modifiers.h"
#include "events-inl.h"
#include "jit/jit.h"
#include "jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_phase.h"

namespace openjdkjvmti {

// TODO We should make this much more selective in the future so we only return true when we
// actually care about the method at this time (ie active frames had locals changed). For now we
// just assume that if anything has changed any frame's locals we care about all methods. If nothing
// has we only care about methods with active breakpoints on them. In the future we should probably
// rewrite all of this to instead do this at the ShadowFrame or thread granularity.
bool JvmtiMethodInspectionCallback::IsMethodBeingInspected(art::ArtMethod* method) {
  // Non-java-debuggable runtimes we need to assume that any method might not be debuggable and
  // therefore potentially being inspected (due to inlines). If we are debuggable we rely hard on
  // inlining not being done since we don't keep track of which methods get inlined where and simply
  // look to see if the method is breakpointed.
  return !art::Runtime::Current()->IsJavaDebuggable() ||
      manager_->HaveLocalsChanged() ||
      manager_->MethodHasBreakpoints(method);
}

bool JvmtiMethodInspectionCallback::IsMethodSafeToJit(art::ArtMethod* method) {
  return !manager_->MethodHasBreakpoints(method);
}

bool JvmtiMethodInspectionCallback::MethodNeedsDebugVersion(
    art::ArtMethod* method ATTRIBUTE_UNUSED) {
  return true;
}

DeoptManager::DeoptManager()
  : deoptimization_status_lock_("JVMTI_DeoptimizationStatusLock",
                                static_cast<art::LockLevel>(
                                    art::LockLevel::kClassLinkerClassesLock + 1)),
    deoptimization_condition_("JVMTI_DeoptimizationCondition", deoptimization_status_lock_),
    performing_deoptimization_(false),
    global_deopt_count_(0),
    deopter_count_(0),
    breakpoint_status_lock_("JVMTI_BreakpointStatusLock",
                            static_cast<art::LockLevel>(art::LockLevel::kAbortLock + 1)),
    inspection_callback_(this),
    set_local_variable_called_(false) { }

void DeoptManager::Setup() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add method Inspection Callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->AddMethodInspectionCallback(&inspection_callback_);
}

void DeoptManager::Shutdown() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("remove method Inspection Callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->RemoveMethodInspectionCallback(&inspection_callback_);
}

void DeoptManager::FinishSetup() {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, deoptimization_status_lock_);

  art::Runtime* runtime = art::Runtime::Current();
  // See if we need to do anything.
  if (!runtime->IsJavaDebuggable()) {
    // See if we can enable all JVMTI functions. If this is false, only kArtTiVersion agents can be
    // retrieved and they will all be best-effort.
    if (PhaseUtil::GetPhaseUnchecked() == JVMTI_PHASE_ONLOAD) {
      // We are still early enough to change the compiler options and get full JVMTI support.
      LOG(INFO) << "Openjdkjvmti plugin loaded on a non-debuggable runtime. Changing runtime to "
                << "debuggable state. Please pass '--debuggable' to dex2oat and "
                << "'-Xcompiler-option --debuggable' to dalvikvm in the future.";
      DCHECK(runtime->GetJit() == nullptr) << "Jit should not be running yet!";
      runtime->AddCompilerOption("--debuggable");
      runtime->SetJavaDebuggable(true);
    } else {
      LOG(WARNING) << "Openjdkjvmti plugin was loaded on a non-debuggable Runtime. Plugin was "
                   << "loaded too late to change runtime state to DEBUGGABLE. Only kArtTiVersion "
                   << "(0x" << std::hex << kArtTiVersion << ") environments are available. Some "
                   << "functionality might not work properly.";
      if (runtime->GetJit() == nullptr &&
          runtime->GetJITOptions()->UseJitCompilation() &&
          !runtime->GetInstrumentation()->IsForcedInterpretOnly()) {
        // If we don't have a jit we should try to start the jit for performance reasons. We only
        // need to do this for late attach on non-debuggable processes because for debuggable
        // processes we already rely on jit and we cannot force this jit to start if we are still in
        // OnLoad since the runtime hasn't started up sufficiently. This is only expected to happen
        // on userdebug/eng builds.
        LOG(INFO) << "Attempting to start jit for openjdkjvmti plugin.";
        runtime->CreateJit();
        if (runtime->GetJit() == nullptr) {
          LOG(WARNING) << "Could not start jit for openjdkjvmti plugin. This process might be "
                       << "quite slow as it is running entirely in the interpreter. Try running "
                       << "'setenforce 0' and restarting this process.";
        }
      }
    }
    runtime->DeoptimizeBootImage();
  }
}

bool DeoptManager::MethodHasBreakpoints(art::ArtMethod* method) {
  art::MutexLock lk(art::Thread::Current(), breakpoint_status_lock_);
  return MethodHasBreakpointsLocked(method);
}

bool DeoptManager::MethodHasBreakpointsLocked(art::ArtMethod* method) {
  auto elem = breakpoint_status_.find(method);
  return elem != breakpoint_status_.end() && elem->second != 0;
}

void DeoptManager::RemoveDeoptimizeAllMethods() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  RemoveDeoptimizeAllMethodsLocked(self);
}

void DeoptManager::AddDeoptimizeAllMethods() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  AddDeoptimizeAllMethodsLocked(self);
}

void DeoptManager::AddMethodBreakpoint(art::ArtMethod* method) {
  DCHECK(method->IsInvokable());
  DCHECK(!method->IsProxyMethod()) << method->PrettyMethod();
  DCHECK(!method->IsNative()) << method->PrettyMethod();

  art::Thread* self = art::Thread::Current();
  method = method->GetCanonicalMethod();
  bool is_default = method->IsDefault();

  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  {
    breakpoint_status_lock_.ExclusiveLock(self);

    DCHECK_GT(deopter_count_, 0u) << "unexpected deotpimization request";

    if (MethodHasBreakpointsLocked(method)) {
      // Don't need to do anything extra.
      breakpoint_status_[method]++;
      // Another thread might be deoptimizing the very method we just added new breakpoints for.
      // Wait for any deopts to finish before moving on.
      breakpoint_status_lock_.ExclusiveUnlock(self);
      WaitForDeoptimizationToFinish(self);
      return;
    }
    breakpoint_status_[method] = 1;
    breakpoint_status_lock_.ExclusiveUnlock(self);
  }
  auto instrumentation = art::Runtime::Current()->GetInstrumentation();
  if (instrumentation->IsForcedInterpretOnly()) {
    // We are already interpreting everything so no need to do anything.
    deoptimization_status_lock_.ExclusiveUnlock(self);
    return;
  } else if (is_default) {
    AddDeoptimizeAllMethodsLocked(self);
  } else {
    PerformLimitedDeoptimization(self, method);
  }
}

void DeoptManager::RemoveMethodBreakpoint(art::ArtMethod* method) {
  DCHECK(method->IsInvokable()) << method->PrettyMethod();
  DCHECK(!method->IsProxyMethod()) << method->PrettyMethod();
  DCHECK(!method->IsNative()) << method->PrettyMethod();

  art::Thread* self = art::Thread::Current();
  method = method->GetCanonicalMethod();
  bool is_default = method->IsDefault();

  art::ScopedThreadSuspension sts(self, art::kSuspended);
  // Ideally we should do a ScopedSuspendAll right here to get the full mutator_lock_ that we might
  // need but since that is very heavy we will instead just use a condition variable to make sure we
  // don't race with ourselves.
  deoptimization_status_lock_.ExclusiveLock(self);
  bool is_last_breakpoint;
  {
    art::MutexLock mu(self, breakpoint_status_lock_);

    DCHECK_GT(deopter_count_, 0u) << "unexpected deotpimization request";
    DCHECK(MethodHasBreakpointsLocked(method)) << "Breakpoint on a method was removed without "
                                              << "breakpoints present!";
    breakpoint_status_[method] -= 1;
    is_last_breakpoint = (breakpoint_status_[method] == 0);
  }
  auto instrumentation = art::Runtime::Current()->GetInstrumentation();
  if (UNLIKELY(instrumentation->IsForcedInterpretOnly())) {
    // We don't need to do anything since we are interpreting everything anyway.
    deoptimization_status_lock_.ExclusiveUnlock(self);
    return;
  } else if (is_last_breakpoint) {
    if (UNLIKELY(is_default)) {
      RemoveDeoptimizeAllMethodsLocked(self);
    } else {
      PerformLimitedUndeoptimization(self, method);
    }
  } else {
    // Another thread might be deoptimizing the very methods we just removed breakpoints from. Wait
    // for any deopts to finish before moving on.
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::WaitForDeoptimizationToFinishLocked(art::Thread* self) {
  while (performing_deoptimization_) {
    deoptimization_condition_.Wait(self);
  }
}

void DeoptManager::WaitForDeoptimizationToFinish(art::Thread* self) {
  WaitForDeoptimizationToFinishLocked(self);
  deoptimization_status_lock_.ExclusiveUnlock(self);
}

class ScopedDeoptimizationContext : public art::ValueObject {
 public:
  ScopedDeoptimizationContext(art::Thread* self, DeoptManager* deopt)
      RELEASE(deopt->deoptimization_status_lock_)
      ACQUIRE(art::Locks::mutator_lock_)
      ACQUIRE(art::Roles::uninterruptible_)
      : self_(self), deopt_(deopt), uninterruptible_cause_(nullptr) {
    deopt_->WaitForDeoptimizationToFinishLocked(self_);
    DCHECK(!deopt->performing_deoptimization_)
        << "Already performing deoptimization on another thread!";
    // Use performing_deoptimization_ to keep track of the lock.
    deopt_->performing_deoptimization_ = true;
    deopt_->deoptimization_status_lock_.Unlock(self_);
    art::Runtime::Current()->GetThreadList()->SuspendAll("JMVTI Deoptimizing methods",
                                                         /*long_suspend*/ false);
    uninterruptible_cause_ = self_->StartAssertNoThreadSuspension("JVMTI deoptimizing methods");
  }

  ~ScopedDeoptimizationContext()
      RELEASE(art::Locks::mutator_lock_)
      RELEASE(art::Roles::uninterruptible_) {
    // Can be suspended again.
    self_->EndAssertNoThreadSuspension(uninterruptible_cause_);
    // Release the mutator lock.
    art::Runtime::Current()->GetThreadList()->ResumeAll();
    // Let other threads know it's fine to proceed.
    art::MutexLock lk(self_, deopt_->deoptimization_status_lock_);
    deopt_->performing_deoptimization_ = false;
    deopt_->deoptimization_condition_.Broadcast(self_);
  }

 private:
  art::Thread* self_;
  DeoptManager* deopt_;
  const char* uninterruptible_cause_;
};

void DeoptManager::AddDeoptimizeAllMethodsLocked(art::Thread* self) {
  global_deopt_count_++;
  if (global_deopt_count_ == 1) {
    PerformGlobalDeoptimization(self);
  } else {
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::RemoveDeoptimizeAllMethodsLocked(art::Thread* self) {
  DCHECK_GT(global_deopt_count_, 0u) << "Request to remove non-existent global deoptimization!";
  global_deopt_count_--;
  if (global_deopt_count_ == 0) {
    PerformGlobalUndeoptimization(self);
  } else {
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::PerformLimitedDeoptimization(art::Thread* self, art::ArtMethod* method) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->Deoptimize(method);
}

void DeoptManager::PerformLimitedUndeoptimization(art::Thread* self, art::ArtMethod* method) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->Undeoptimize(method);
}

void DeoptManager::PerformGlobalDeoptimization(art::Thread* self) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->DeoptimizeEverything(
      kDeoptManagerInstrumentationKey);
}

void DeoptManager::PerformGlobalUndeoptimization(art::Thread* self) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->UndeoptimizeEverything(
      kDeoptManagerInstrumentationKey);
}


void DeoptManager::RemoveDeoptimizationRequester() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  DCHECK_GT(deopter_count_, 0u) << "Removing deoptimization requester without any being present";
  deopter_count_--;
  if (deopter_count_ == 0) {
    ScopedDeoptimizationContext sdc(self, this);
    // TODO Give this a real key.
    art::Runtime::Current()->GetInstrumentation()->DisableDeoptimization("");
    return;
  } else {
    deoptimization_status_lock_.ExclusiveUnlock(self);
  }
}

void DeoptManager::AddDeoptimizationRequester() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange stsc(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  deopter_count_++;
  if (deopter_count_ == 1) {
    ScopedDeoptimizationContext sdc(self, this);
    art::Runtime::Current()->GetInstrumentation()->EnableDeoptimization();
    return;
  } else {
    deoptimization_status_lock_.ExclusiveUnlock(self);
  }
}

void DeoptManager::DeoptimizeThread(art::Thread* target) {
  art::Runtime::Current()->GetInstrumentation()->InstrumentThreadStack(target);
}

extern DeoptManager* gDeoptManager;
DeoptManager* DeoptManager::Get() {
  return gDeoptManager;
}

}  // namespace openjdkjvmti
