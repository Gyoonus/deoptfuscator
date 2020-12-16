/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "callee_save_frame.h"
#include "common_throws.h"
#include "mirror/object-inl.h"

namespace art {

extern "C" int artLockObjectFromCode(mirror::Object* obj, Thread* self)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_)
    REQUIRES_SHARED(Locks::mutator_lock_) /* EXCLUSIVE_LOCK_FUNCTION(Monitor::monitor_lock_) */ {
  ScopedQuickEntrypointChecks sqec(self);
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerException("Null reference used for synchronization (monitor-enter)");
    return -1;  // Failure.
  } else {
    obj = obj->MonitorEnter(self);  // May block
    DCHECK(self->HoldsLock(obj));
    // Exceptions can be thrown by monitor event listeners. This is expected to be rare however.
    if (UNLIKELY(self->IsExceptionPending())) {
      // TODO Remove this DCHECK if we expand the use of monitor callbacks.
      DCHECK(Runtime::Current()->HasLoadedPlugins())
          << "Exceptions are only expected to be thrown by plugin code which doesn't seem to be "
          << "loaded.";
      // We need to get rid of the lock
      bool unlocked = obj->MonitorExit(self);
      DCHECK(unlocked);
      return -1;  // Failure.
    } else {
      DCHECK(self->HoldsLock(obj));
      return 0;  // Success.
    }
  }
}

extern "C" int artUnlockObjectFromCode(mirror::Object* obj, Thread* self)
    NO_THREAD_SAFETY_ANALYSIS
    REQUIRES(!Roles::uninterruptible_)
    REQUIRES_SHARED(Locks::mutator_lock_) /* UNLOCK_FUNCTION(Monitor::monitor_lock_) */ {
  ScopedQuickEntrypointChecks sqec(self);
  if (UNLIKELY(obj == nullptr)) {
    ThrowNullPointerException("Null reference used for synchronization (monitor-exit)");
    return -1;  // Failure.
  } else {
    // MonitorExit may throw exception.
    return obj->MonitorExit(self) ? 0 /* Success */ : -1 /* Failure */;
  }
}

}  // namespace art
