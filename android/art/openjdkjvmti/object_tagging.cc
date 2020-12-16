/* Copyright (C) 2016 The Android Open Source Project
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

#include "object_tagging.h"

#include <limits>

#include "art_jvmti.h"
#include "events-inl.h"
#include "jvmti_weak_table-inl.h"

namespace openjdkjvmti {

// Instantiate for jlong = JVMTI tags.
template class JvmtiWeakTable<jlong>;

bool ObjectTagTable::Set(art::mirror::Object* obj, jlong new_tag) {
  if (new_tag == 0) {
    jlong tmp;
    return Remove(obj, &tmp);
  }
  return JvmtiWeakTable<jlong>::Set(obj, new_tag);
}
bool ObjectTagTable::SetLocked(art::mirror::Object* obj, jlong new_tag) {
  if (new_tag == 0) {
    jlong tmp;
    return RemoveLocked(obj, &tmp);
  }
  return JvmtiWeakTable<jlong>::SetLocked(obj, new_tag);
}

bool ObjectTagTable::DoesHandleNullOnSweep() {
  return event_handler_->IsEventEnabledAnywhere(ArtJvmtiEvent::kObjectFree);
}
void ObjectTagTable::HandleNullSweep(jlong tag) {
  event_handler_->DispatchEventOnEnv<ArtJvmtiEvent::kObjectFree>(
      jvmti_env_, art::Thread::Current(), tag);
}

}  // namespace openjdkjvmti
