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

#include "ti_threadgroup.h"

#include "art_field-inl.h"
#include "art_jvmti.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "handle_scope-inl.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "obj_ptr.h"
#include "object_lock.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "well_known_classes.h"

namespace openjdkjvmti {


jvmtiError ThreadGroupUtil::GetTopThreadGroups(jvmtiEnv* env,
                                               jint* group_count_ptr,
                                               jthreadGroup** groups_ptr) {
  // We only have a single top group. So we can take the current thread and move upwards.
  if (group_count_ptr == nullptr || groups_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }

  art::Runtime* runtime = art::Runtime::Current();
  if (runtime == nullptr) {
    // Must be starting the runtime, or dying.
    return ERR(WRONG_PHASE);
  }

  jobject sys_thread_group = runtime->GetSystemThreadGroup();
  if (sys_thread_group == nullptr) {
    // Seems we're still starting up.
    return ERR(WRONG_PHASE);
  }

  unsigned char* data;
  jvmtiError result = env->Allocate(sizeof(jthreadGroup), &data);
  if (result != ERR(NONE)) {
    return result;
  }

  jthreadGroup* groups = reinterpret_cast<jthreadGroup*>(data);
  *groups =
      reinterpret_cast<JNIEnv*>(art::Thread::Current()->GetJniEnv())->NewLocalRef(sys_thread_group);
  *groups_ptr = groups;
  *group_count_ptr = 1;

  return ERR(NONE);
}

jvmtiError ThreadGroupUtil::GetThreadGroupInfo(jvmtiEnv* env,
                                               jthreadGroup group,
                                               jvmtiThreadGroupInfo* info_ptr) {
  if (group == nullptr) {
    return ERR(INVALID_THREAD_GROUP);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());
  if (soa.Env()->IsInstanceOf(group, art::WellKnownClasses::java_lang_ThreadGroup) == JNI_FALSE) {
    return ERR(INVALID_THREAD_GROUP);
  }

  art::ObjPtr<art::mirror::Object> obj = soa.Decode<art::mirror::Object>(group);

  // Do the name first. It's the only thing that can fail.
  {
    art::ArtField* name_field =
        art::jni::DecodeArtField(art::WellKnownClasses::java_lang_ThreadGroup_name);
    CHECK(name_field != nullptr);
    art::ObjPtr<art::mirror::String> name_obj =
        art::ObjPtr<art::mirror::String>::DownCast(name_field->GetObject(obj));
    std::string tmp_str;
    const char* tmp_cstr;
    if (name_obj == nullptr) {
      tmp_cstr = "";
    } else {
      tmp_str = name_obj->ToModifiedUtf8();
      tmp_cstr = tmp_str.c_str();
    }
    jvmtiError result;
    JvmtiUniquePtr<char[]> copy = CopyString(env, tmp_cstr, &result);
    if (copy == nullptr) {
      return result;
    }
    info_ptr->name = copy.release();
  }

  // Parent.
  {
    art::ArtField* parent_field =
        art::jni::DecodeArtField(art::WellKnownClasses::java_lang_ThreadGroup_parent);
    CHECK(parent_field != nullptr);
    art::ObjPtr<art::mirror::Object> parent_group = parent_field->GetObject(obj);
    info_ptr->parent = parent_group == nullptr
                           ? nullptr
                           : soa.AddLocalReference<jthreadGroup>(parent_group);
  }

  // Max priority.
  {
    art::ArtField* prio_field = obj->GetClass()->FindDeclaredInstanceField("maxPriority", "I");
    CHECK(prio_field != nullptr);
    info_ptr->max_priority = static_cast<jint>(prio_field->GetInt(obj));
  }

  // Daemon.
  {
    art::ArtField* daemon_field = obj->GetClass()->FindDeclaredInstanceField("daemon", "Z");
    CHECK(daemon_field != nullptr);
    info_ptr->is_daemon = daemon_field->GetBoolean(obj) == 0 ? JNI_FALSE : JNI_TRUE;
  }

  return ERR(NONE);
}


static bool IsInDesiredThreadGroup(art::Handle<art::mirror::Object> desired_thread_group,
                                   art::ObjPtr<art::mirror::Object> peer)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  CHECK(desired_thread_group != nullptr);

  art::ArtField* thread_group_field =
      art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_group);
  DCHECK(thread_group_field != nullptr);
  art::ObjPtr<art::mirror::Object> group = thread_group_field->GetObject(peer);
  return (group == desired_thread_group.Get());
}

static void GetThreads(art::Handle<art::mirror::Object> thread_group,
                       std::vector<art::ObjPtr<art::mirror::Object>>* thread_peers)
    REQUIRES_SHARED(art::Locks::mutator_lock_) REQUIRES(!art::Locks::thread_list_lock_) {
  CHECK(thread_group != nullptr);

  art::MutexLock mu(art::Thread::Current(), *art::Locks::thread_list_lock_);
  for (art::Thread* t : art::Runtime::Current()->GetThreadList()->GetList()) {
    if (t->IsStillStarting()) {
      continue;
    }
    art::ObjPtr<art::mirror::Object> peer = t->GetPeerFromOtherThread();
    if (peer == nullptr) {
      continue;
    }
    if (IsInDesiredThreadGroup(thread_group, peer)) {
      thread_peers->push_back(peer);
    }
  }
}

static void GetChildThreadGroups(art::Handle<art::mirror::Object> thread_group,
                                 std::vector<art::ObjPtr<art::mirror::Object>>* thread_groups)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  CHECK(thread_group != nullptr);

  // Get the ThreadGroup[] "groups" out of this thread group...
  art::ArtField* groups_field =
      art::jni::DecodeArtField(art::WellKnownClasses::java_lang_ThreadGroup_groups);
  art::ObjPtr<art::mirror::Object> groups_array = groups_field->GetObject(thread_group.Get());

  if (groups_array == nullptr) {
    return;
  }
  CHECK(groups_array->IsObjectArray());

  art::ObjPtr<art::mirror::ObjectArray<art::mirror::Object>> groups_array_as_array =
      groups_array->AsObjectArray<art::mirror::Object>();

  // Copy all non-null elements.
  for (int32_t i = 0; i < groups_array_as_array->GetLength(); ++i) {
    art::ObjPtr<art::mirror::Object> entry = groups_array_as_array->Get(i);
    if (entry != nullptr) {
      thread_groups->push_back(entry);
    }
  }
}

jvmtiError ThreadGroupUtil::GetThreadGroupChildren(jvmtiEnv* env,
                                                   jthreadGroup group,
                                                   jint* thread_count_ptr,
                                                   jthread** threads_ptr,
                                                   jint* group_count_ptr,
                                                   jthreadGroup** groups_ptr) {
  if (group == nullptr) {
    return ERR(INVALID_THREAD_GROUP);
  }

  art::ScopedObjectAccess soa(art::Thread::Current());

  if (!soa.Env()->IsInstanceOf(group, art::WellKnownClasses::java_lang_ThreadGroup)) {
    return ERR(INVALID_THREAD_GROUP);
  }

  art::StackHandleScope<1> hs(soa.Self());
  art::Handle<art::mirror::Object> thread_group = hs.NewHandle(
      soa.Decode<art::mirror::Object>(group));

  art::ObjectLock<art::mirror::Object> thread_group_lock(soa.Self(), thread_group);

  std::vector<art::ObjPtr<art::mirror::Object>> thread_peers;
  GetThreads(thread_group, &thread_peers);

  std::vector<art::ObjPtr<art::mirror::Object>> thread_groups;
  GetChildThreadGroups(thread_group, &thread_groups);

  JvmtiUniquePtr<jthread[]> peers_uptr;
  if (!thread_peers.empty()) {
    jvmtiError res;
    peers_uptr = AllocJvmtiUniquePtr<jthread[]>(env, thread_peers.size(), &res);
    if (peers_uptr == nullptr) {
      return res;
    }
  }

  JvmtiUniquePtr<jthreadGroup[]> group_uptr;
  if (!thread_groups.empty()) {
    jvmtiError res;
    group_uptr = AllocJvmtiUniquePtr<jthreadGroup[]>(env, thread_groups.size(), &res);
    if (group_uptr == nullptr) {
      return res;
    }
  }

  // Can't fail anymore from here on.

  // Copy data into out buffers.
  for (size_t i = 0; i != thread_peers.size(); ++i) {
    peers_uptr[i] = soa.AddLocalReference<jthread>(thread_peers[i]);
  }
  for (size_t i = 0; i != thread_groups.size(); ++i) {
    group_uptr[i] = soa.AddLocalReference<jthreadGroup>(thread_groups[i]);
  }

  *thread_count_ptr = static_cast<jint>(thread_peers.size());
  *threads_ptr = peers_uptr.release();
  *group_count_ptr = static_cast<jint>(thread_groups.size());
  *groups_ptr = group_uptr.release();

  return ERR(NONE);
}

}  // namespace openjdkjvmti
