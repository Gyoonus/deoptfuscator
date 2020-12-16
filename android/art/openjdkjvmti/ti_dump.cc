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

#include "ti_dump.h"

#include <limits>

#include "art_jvmti.h"
#include "base/mutex.h"
#include "events-inl.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"

namespace openjdkjvmti {

struct DumpCallback : public art::RuntimeSigQuitCallback {
  void SigQuit() OVERRIDE REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::Thread* thread = art::Thread::Current();
    art::ScopedThreadSuspension sts(thread, art::ThreadState::kNative);
    event_handler->DispatchEvent<ArtJvmtiEvent::kDataDumpRequest>(art::Thread::Current());
  }

  EventHandler* event_handler = nullptr;
};

static DumpCallback gDumpCallback;

void DumpUtil::Register(EventHandler* handler) {
  gDumpCallback.event_handler = handler;
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add sigquit callback");
  art::Runtime::Current()->GetRuntimeCallbacks()->AddRuntimeSigQuitCallback(&gDumpCallback);
}

void DumpUtil::Unregister() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Remove sigquit callback");
  art::Runtime::Current()->GetRuntimeCallbacks()->RemoveRuntimeSigQuitCallback(&gDumpCallback);
}

}  // namespace openjdkjvmti
