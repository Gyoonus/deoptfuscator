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

#include "lock_count_data.h"

#include <algorithm>
#include <string>

#include "android-base/logging.h"
#include "mirror/object-inl.h"
#include "thread.h"

namespace art {

void LockCountData::AddMonitor(Thread* self, mirror::Object* obj) {
  if (obj == nullptr) {
    return;
  }

  // If there's an error during enter, we won't have locked the monitor. So check there's no
  // exception.
  if (self->IsExceptionPending()) {
    return;
  }

  if (monitors_ == nullptr) {
    monitors_.reset(new std::vector<mirror::Object*>());
  }
  monitors_->push_back(obj);
}

void LockCountData::RemoveMonitorOrThrow(Thread* self, const mirror::Object* obj) {
  if (obj == nullptr) {
    return;
  }
  bool found_object = false;
  if (monitors_ != nullptr) {
    // We need to remove one pointer to ref, as duplicates are used for counting recursive locks.
    // We arbitrarily choose the first one.
    auto it = std::find(monitors_->begin(), monitors_->end(), obj);
    if (it != monitors_->end()) {
      monitors_->erase(it);
      found_object = true;
    }
  }
  if (!found_object) {
    // The object wasn't found. Time for an IllegalMonitorStateException.
    // The order here isn't fully clear. Assume that any other pending exception is swallowed.
    // TODO: Maybe make already pending exception a suppressed exception.
    self->ClearException();
    self->ThrowNewExceptionF("Ljava/lang/IllegalMonitorStateException;",
                             "did not lock monitor on object of type '%s' before unlocking",
                             const_cast<mirror::Object*>(obj)->PrettyTypeOf().c_str());
  }
}

// Helper to unlock a monitor. Must be NO_THREAD_SAFETY_ANALYSIS, as we can't statically show
// that the object was locked.
void MonitorExitHelper(Thread* self, mirror::Object* obj) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(self != nullptr);
  DCHECK(obj != nullptr);
  obj->MonitorExit(self);
}

bool LockCountData::CheckAllMonitorsReleasedOrThrow(Thread* self) {
  DCHECK(self != nullptr);
  if (monitors_ != nullptr) {
    if (!monitors_->empty()) {
      // There may be an exception pending, if the method is terminating abruptly. Clear it.
      // TODO: Should we add this as a suppressed exception?
      self->ClearException();

      // OK, there are monitors that are still locked. To enforce structured locking (and avoid
      // deadlocks) we unlock all of them before we raise the IllegalMonitorState exception.
      for (mirror::Object* obj : *monitors_) {
        MonitorExitHelper(self, obj);
        // If this raised an exception, ignore. TODO: Should we add this as suppressed
        // exceptions?
        if (self->IsExceptionPending()) {
          self->ClearException();
        }
      }
      // Raise an exception, just give the first object as the sample.
      mirror::Object* first = (*monitors_)[0];
      self->ThrowNewExceptionF("Ljava/lang/IllegalMonitorStateException;",
                               "did not unlock monitor on object of type '%s'",
                               mirror::Object::PrettyTypeOf(first).c_str());

      // To make sure this path is not triggered again, clean out the monitors.
      monitors_->clear();

      return false;
    }
  }
  return true;
}

}  // namespace art
