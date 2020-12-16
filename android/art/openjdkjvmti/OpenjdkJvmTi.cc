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

#include <string>
#include <type_traits>
#include <vector>

#include <android-base/logging.h>

#include <jni.h>

#include "jvmti.h"

#include "art_jvmti.h"
#include "base/logging.h"  // For gLogVerbosity.
#include "base/mutex.h"
#include "events-inl.h"
#include "jni_env_ext-inl.h"
#include "obj_ptr-inl.h"
#include "object_tagging.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_allocator.h"
#include "ti_breakpoint.h"
#include "ti_class.h"
#include "ti_dump.h"
#include "ti_extension.h"
#include "ti_field.h"
#include "ti_heap.h"
#include "ti_jni.h"
#include "ti_method.h"
#include "ti_monitor.h"
#include "ti_object.h"
#include "ti_phase.h"
#include "ti_properties.h"
#include "ti_redefine.h"
#include "ti_search.h"
#include "ti_stack.h"
#include "ti_thread.h"
#include "ti_threadgroup.h"
#include "ti_timers.h"
#include "transform.h"

namespace openjdkjvmti {

// NB These are heap allocated to avoid the static destructors being run if an agent calls exit(3).
// These should never be null.
EventHandler* gEventHandler;
DeoptManager* gDeoptManager;

#define ENSURE_NON_NULL(n)      \
  do {                          \
    if ((n) == nullptr) {       \
      return ERR(NULL_POINTER); \
    }                           \
  } while (false)

// Returns whether we are able to use all jvmti features.
static bool IsFullJvmtiAvailable() {
  art::Runtime* runtime = art::Runtime::Current();
  return runtime->GetInstrumentation()->IsForcedInterpretOnly() || runtime->IsJavaDebuggable();
}

class JvmtiFunctions {
 private:
  static jvmtiError getEnvironmentError(jvmtiEnv* env) {
    if (env == nullptr) {
      return ERR(INVALID_ENVIRONMENT);
    } else if (art::Thread::Current() == nullptr) {
      return ERR(UNATTACHED_THREAD);
    } else {
      return OK;
    }
  }

#define ENSURE_VALID_ENV(env)                                            \
  do {                                                                   \
    jvmtiError ensure_valid_env_ ## __LINE__ = getEnvironmentError(env); \
    if (ensure_valid_env_ ## __LINE__ != OK) {                           \
      return ensure_valid_env_ ## __LINE__ ;                             \
    }                                                                    \
  } while (false)

#define ENSURE_HAS_CAP(env, cap) \
  do { \
    if (ArtJvmTiEnv::AsArtJvmTiEnv(env)->capabilities.cap != 1) { \
      return ERR(MUST_POSSESS_CAPABILITY); \
    } \
  } while (false)

 public:
  static jvmtiError Allocate(jvmtiEnv* env, jlong size, unsigned char** mem_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(mem_ptr);
    return AllocUtil::Allocate(env, size, mem_ptr);
  }

  static jvmtiError Deallocate(jvmtiEnv* env, unsigned char* mem) {
    ENSURE_VALID_ENV(env);
    return AllocUtil::Deallocate(env, mem);
  }

  static jvmtiError GetThreadState(jvmtiEnv* env, jthread thread, jint* thread_state_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::GetThreadState(env, thread, thread_state_ptr);
  }

  static jvmtiError GetCurrentThread(jvmtiEnv* env, jthread* thread_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::GetCurrentThread(env, thread_ptr);
  }

  static jvmtiError GetAllThreads(jvmtiEnv* env, jint* threads_count_ptr, jthread** threads_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::GetAllThreads(env, threads_count_ptr, threads_ptr);
  }

  static jvmtiError SuspendThread(jvmtiEnv* env, jthread thread) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_suspend);
    return ThreadUtil::SuspendThread(env, thread);
  }

  static jvmtiError SuspendThreadList(jvmtiEnv* env,
                                      jint request_count,
                                      const jthread* request_list,
                                      jvmtiError* results) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_suspend);
    return ThreadUtil::SuspendThreadList(env, request_count, request_list, results);
  }

  static jvmtiError ResumeThread(jvmtiEnv* env, jthread thread) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_suspend);
    return ThreadUtil::ResumeThread(env, thread);
  }

  static jvmtiError ResumeThreadList(jvmtiEnv* env,
                                     jint request_count,
                                     const jthread* request_list,
                                     jvmtiError* results) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_suspend);
    return ThreadUtil::ResumeThreadList(env, request_count, request_list, results);
  }

  static jvmtiError StopThread(jvmtiEnv* env, jthread thread, jobject exception) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_signal_thread);
    return ThreadUtil::StopThread(env, thread, exception);
  }

  static jvmtiError InterruptThread(jvmtiEnv* env, jthread thread) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_signal_thread);
    return ThreadUtil::InterruptThread(env, thread);
  }

  static jvmtiError GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::GetThreadInfo(env, thread, info_ptr);
  }

  static jvmtiError GetOwnedMonitorInfo(jvmtiEnv* env,
                                        jthread thread,
                                        jint* owned_monitor_count_ptr,
                                        jobject** owned_monitors_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_owned_monitor_info);
    return StackUtil::GetOwnedMonitorInfo(env,
                                          thread,
                                          owned_monitor_count_ptr,
                                          owned_monitors_ptr);
  }

  static jvmtiError GetOwnedMonitorStackDepthInfo(jvmtiEnv* env,
                                                  jthread thread,
                                                  jint* monitor_info_count_ptr,
                                                  jvmtiMonitorStackDepthInfo** monitor_info_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_owned_monitor_stack_depth_info);
    return StackUtil::GetOwnedMonitorStackDepthInfo(env,
                                                    thread,
                                                    monitor_info_count_ptr,
                                                    monitor_info_ptr);
  }

  static jvmtiError GetCurrentContendedMonitor(jvmtiEnv* env,
                                               jthread thread,
                                               jobject* monitor_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_current_contended_monitor);
    return MonitorUtil::GetCurrentContendedMonitor(env, thread, monitor_ptr);
  }

  static jvmtiError RunAgentThread(jvmtiEnv* env,
                                   jthread thread,
                                   jvmtiStartFunction proc,
                                   const void* arg,
                                   jint priority) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::RunAgentThread(env, thread, proc, arg, priority);
  }

  static jvmtiError SetThreadLocalStorage(jvmtiEnv* env, jthread thread, const void* data) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::SetThreadLocalStorage(env, thread, data);
  }

  static jvmtiError GetThreadLocalStorage(jvmtiEnv* env, jthread thread, void** data_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadUtil::GetThreadLocalStorage(env, thread, data_ptr);
  }

  static jvmtiError GetTopThreadGroups(jvmtiEnv* env,
                                       jint* group_count_ptr,
                                       jthreadGroup** groups_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadGroupUtil::GetTopThreadGroups(env, group_count_ptr, groups_ptr);
  }

  static jvmtiError GetThreadGroupInfo(jvmtiEnv* env,
                                       jthreadGroup group,
                                       jvmtiThreadGroupInfo* info_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadGroupUtil::GetThreadGroupInfo(env, group, info_ptr);
  }

  static jvmtiError GetThreadGroupChildren(jvmtiEnv* env,
                                           jthreadGroup group,
                                           jint* thread_count_ptr,
                                           jthread** threads_ptr,
                                           jint* group_count_ptr,
                                           jthreadGroup** groups_ptr) {
    ENSURE_VALID_ENV(env);
    return ThreadGroupUtil::GetThreadGroupChildren(env,
                                                   group,
                                                   thread_count_ptr,
                                                   threads_ptr,
                                                   group_count_ptr,
                                                   groups_ptr);
  }

  static jvmtiError GetStackTrace(jvmtiEnv* env,
                                  jthread thread,
                                  jint start_depth,
                                  jint max_frame_count,
                                  jvmtiFrameInfo* frame_buffer,
                                  jint* count_ptr) {
    ENSURE_VALID_ENV(env);
    return StackUtil::GetStackTrace(env,
                                    thread,
                                    start_depth,
                                    max_frame_count,
                                    frame_buffer,
                                    count_ptr);
  }

  static jvmtiError GetAllStackTraces(jvmtiEnv* env,
                                      jint max_frame_count,
                                      jvmtiStackInfo** stack_info_ptr,
                                      jint* thread_count_ptr) {
    ENSURE_VALID_ENV(env);
    return StackUtil::GetAllStackTraces(env, max_frame_count, stack_info_ptr, thread_count_ptr);
  }

  static jvmtiError GetThreadListStackTraces(jvmtiEnv* env,
                                             jint thread_count,
                                             const jthread* thread_list,
                                             jint max_frame_count,
                                             jvmtiStackInfo** stack_info_ptr) {
    ENSURE_VALID_ENV(env);
    return StackUtil::GetThreadListStackTraces(env,
                                               thread_count,
                                               thread_list,
                                               max_frame_count,
                                               stack_info_ptr);
  }

  static jvmtiError GetFrameCount(jvmtiEnv* env, jthread thread, jint* count_ptr) {
    ENSURE_VALID_ENV(env);
    return StackUtil::GetFrameCount(env, thread, count_ptr);
  }

  static jvmtiError PopFrame(jvmtiEnv* env, jthread thread ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_pop_frame);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetFrameLocation(jvmtiEnv* env,
                                     jthread thread,
                                     jint depth,
                                     jmethodID* method_ptr,
                                     jlocation* location_ptr) {
    ENSURE_VALID_ENV(env);
    return StackUtil::GetFrameLocation(env, thread, depth, method_ptr, location_ptr);
  }

  static jvmtiError NotifyFramePop(jvmtiEnv* env, jthread thread, jint depth) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_frame_pop_events);
    return StackUtil::NotifyFramePop(env, thread, depth);
  }

  static jvmtiError ForceEarlyReturnObject(jvmtiEnv* env,
                                           jthread thread ATTRIBUTE_UNUSED,
                                           jobject value ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnInt(jvmtiEnv* env,
                                        jthread thread ATTRIBUTE_UNUSED,
                                        jint value ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnLong(jvmtiEnv* env,
                                         jthread thread ATTRIBUTE_UNUSED,
                                         jlong value ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnFloat(jvmtiEnv* env,
                                          jthread thread ATTRIBUTE_UNUSED,
                                          jfloat value ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnDouble(jvmtiEnv* env,
                                           jthread thread ATTRIBUTE_UNUSED,
                                           jdouble value ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError ForceEarlyReturnVoid(jvmtiEnv* env, jthread thread ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_force_early_return);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError FollowReferences(jvmtiEnv* env,
                                     jint heap_filter,
                                     jclass klass,
                                     jobject initial_object,
                                     const jvmtiHeapCallbacks* callbacks,
                                     const void* user_data) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    HeapUtil heap_util(ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table.get());
    return heap_util.FollowReferences(env,
                                      heap_filter,
                                      klass,
                                      initial_object,
                                      callbacks,
                                      user_data);
  }

  static jvmtiError IterateThroughHeap(jvmtiEnv* env,
                                       jint heap_filter,
                                       jclass klass,
                                       const jvmtiHeapCallbacks* callbacks,
                                       const void* user_data) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    HeapUtil heap_util(ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table.get());
    return heap_util.IterateThroughHeap(env, heap_filter, klass, callbacks, user_data);
  }

  static jvmtiError GetTag(jvmtiEnv* env, jobject object, jlong* tag_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);

    JNIEnv* jni_env = GetJniEnv(env);
    if (jni_env == nullptr) {
      return ERR(INTERNAL);
    }

    art::ScopedObjectAccess soa(jni_env);
    art::ObjPtr<art::mirror::Object> obj = soa.Decode<art::mirror::Object>(object);
    if (!ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table->GetTag(obj.Ptr(), tag_ptr)) {
      *tag_ptr = 0;
    }

    return ERR(NONE);
  }

  static jvmtiError SetTag(jvmtiEnv* env, jobject object, jlong tag) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);

    if (object == nullptr) {
      return ERR(NULL_POINTER);
    }

    JNIEnv* jni_env = GetJniEnv(env);
    if (jni_env == nullptr) {
      return ERR(INTERNAL);
    }

    art::ScopedObjectAccess soa(jni_env);
    art::ObjPtr<art::mirror::Object> obj = soa.Decode<art::mirror::Object>(object);
    ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table->Set(obj.Ptr(), tag);

    return ERR(NONE);
  }

  static jvmtiError GetObjectsWithTags(jvmtiEnv* env,
                                       jint tag_count,
                                       const jlong* tags,
                                       jint* count_ptr,
                                       jobject** object_result_ptr,
                                       jlong** tag_result_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);

    JNIEnv* jni_env = GetJniEnv(env);
    if (jni_env == nullptr) {
      return ERR(INTERNAL);
    }

    art::ScopedObjectAccess soa(jni_env);
    return ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table->GetTaggedObjects(env,
                                                                               tag_count,
                                                                               tags,
                                                                               count_ptr,
                                                                               object_result_ptr,
                                                                               tag_result_ptr);
  }

  static jvmtiError ForceGarbageCollection(jvmtiEnv* env) {
    ENSURE_VALID_ENV(env);
    return HeapUtil::ForceGarbageCollection(env);
  }

  static jvmtiError IterateOverObjectsReachableFromObject(
      jvmtiEnv* env,
      jobject object ATTRIBUTE_UNUSED,
      jvmtiObjectReferenceCallback object_reference_callback ATTRIBUTE_UNUSED,
      const void* user_data ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverReachableObjects(
      jvmtiEnv* env,
      jvmtiHeapRootCallback heap_root_callback ATTRIBUTE_UNUSED,
      jvmtiStackReferenceCallback stack_ref_callback ATTRIBUTE_UNUSED,
      jvmtiObjectReferenceCallback object_ref_callback ATTRIBUTE_UNUSED,
      const void* user_data ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverHeap(jvmtiEnv* env,
                                    jvmtiHeapObjectFilter object_filter ATTRIBUTE_UNUSED,
                                    jvmtiHeapObjectCallback heap_object_callback ATTRIBUTE_UNUSED,
                                    const void* user_data ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IterateOverInstancesOfClass(
      jvmtiEnv* env,
      jclass klass ATTRIBUTE_UNUSED,
      jvmtiHeapObjectFilter object_filter ATTRIBUTE_UNUSED,
      jvmtiHeapObjectCallback heap_object_callback ATTRIBUTE_UNUSED,
      const void* user_data ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_tag_objects);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetLocalObject(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jobject* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariable(env, thread, depth, slot, value_ptr);
  }

  static jvmtiError GetLocalInstance(jvmtiEnv* env,
                                     jthread thread,
                                     jint depth,
                                     jobject* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalInstance(env, thread, depth, value_ptr);
  }

  static jvmtiError GetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariable(env, thread, depth, slot, value_ptr);
  }

  static jvmtiError GetLocalLong(jvmtiEnv* env,
                                 jthread thread,
                                 jint depth,
                                 jint slot,
                                 jlong* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariable(env, thread, depth, slot, value_ptr);
  }

  static jvmtiError GetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariable(env, thread, depth, slot, value_ptr);
  }

  static jvmtiError GetLocalDouble(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jdouble* value_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariable(env, thread, depth, slot, value_ptr);
  }

  static jvmtiError SetLocalObject(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jobject value) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::SetLocalVariable(env, thread, depth, slot, value);
  }

  static jvmtiError SetLocalInt(jvmtiEnv* env,
                                jthread thread,
                                jint depth,
                                jint slot,
                                jint value) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::SetLocalVariable(env, thread, depth, slot, value);
  }

  static jvmtiError SetLocalLong(jvmtiEnv* env,
                                 jthread thread,
                                 jint depth,
                                 jint slot,
                                 jlong value) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::SetLocalVariable(env, thread, depth, slot, value);
  }

  static jvmtiError SetLocalFloat(jvmtiEnv* env,
                                  jthread thread,
                                  jint depth,
                                  jint slot,
                                  jfloat value) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::SetLocalVariable(env, thread, depth, slot, value);
  }

  static jvmtiError SetLocalDouble(jvmtiEnv* env,
                                   jthread thread,
                                   jint depth,
                                   jint slot,
                                   jdouble value) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::SetLocalVariable(env, thread, depth, slot, value);
  }


  static jvmtiError SetBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_breakpoint_events);
    return BreakpointUtil::SetBreakpoint(env, method, location);
  }

  static jvmtiError ClearBreakpoint(jvmtiEnv* env, jmethodID method, jlocation location) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_breakpoint_events);
    return BreakpointUtil::ClearBreakpoint(env, method, location);
  }

  static jvmtiError SetFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_field_access_events);
    return FieldUtil::SetFieldAccessWatch(env, klass, field);
  }

  static jvmtiError ClearFieldAccessWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_field_access_events);
    return FieldUtil::ClearFieldAccessWatch(env, klass, field);
  }

  static jvmtiError SetFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_field_modification_events);
    return FieldUtil::SetFieldModificationWatch(env, klass, field);
  }

  static jvmtiError ClearFieldModificationWatch(jvmtiEnv* env, jclass klass, jfieldID field) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_generate_field_modification_events);
    return FieldUtil::ClearFieldModificationWatch(env, klass, field);
  }

  static jvmtiError GetLoadedClasses(jvmtiEnv* env, jint* class_count_ptr, jclass** classes_ptr) {
    ENSURE_VALID_ENV(env);
    HeapUtil heap_util(ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table.get());
    return heap_util.GetLoadedClasses(env, class_count_ptr, classes_ptr);
  }

  static jvmtiError GetClassLoaderClasses(jvmtiEnv* env,
                                          jobject initiating_loader,
                                          jint* class_count_ptr,
                                          jclass** classes_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassLoaderClasses(env, initiating_loader, class_count_ptr, classes_ptr);
  }

  static jvmtiError GetClassSignature(jvmtiEnv* env,
                                      jclass klass,
                                      char** signature_ptr,
                                      char** generic_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassSignature(env, klass, signature_ptr, generic_ptr);
  }

  static jvmtiError GetClassStatus(jvmtiEnv* env, jclass klass, jint* status_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassStatus(env, klass, status_ptr);
  }

  static jvmtiError GetSourceFileName(jvmtiEnv* env, jclass klass, char** source_name_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_source_file_name);
    return ClassUtil::GetSourceFileName(env, klass, source_name_ptr);
  }

  static jvmtiError GetClassModifiers(jvmtiEnv* env, jclass klass, jint* modifiers_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassModifiers(env, klass, modifiers_ptr);
  }

  static jvmtiError GetClassMethods(jvmtiEnv* env,
                                    jclass klass,
                                    jint* method_count_ptr,
                                    jmethodID** methods_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassMethods(env, klass, method_count_ptr, methods_ptr);
  }

  static jvmtiError GetClassFields(jvmtiEnv* env,
                                   jclass klass,
                                   jint* field_count_ptr,
                                   jfieldID** fields_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassFields(env, klass, field_count_ptr, fields_ptr);
  }

  static jvmtiError GetImplementedInterfaces(jvmtiEnv* env,
                                             jclass klass,
                                             jint* interface_count_ptr,
                                             jclass** interfaces_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetImplementedInterfaces(env, klass, interface_count_ptr, interfaces_ptr);
  }

  static jvmtiError GetClassVersionNumbers(jvmtiEnv* env,
                                           jclass klass,
                                           jint* minor_version_ptr,
                                           jint* major_version_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassVersionNumbers(env, klass, minor_version_ptr, major_version_ptr);
  }

  static jvmtiError GetConstantPool(jvmtiEnv* env,
                                    jclass klass ATTRIBUTE_UNUSED,
                                    jint* constant_pool_count_ptr ATTRIBUTE_UNUSED,
                                    jint* constant_pool_byte_count_ptr ATTRIBUTE_UNUSED,
                                    unsigned char** constant_pool_bytes_ptr ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_constant_pool);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError IsInterface(jvmtiEnv* env, jclass klass, jboolean* is_interface_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::IsInterface(env, klass, is_interface_ptr);
  }

  static jvmtiError IsArrayClass(jvmtiEnv* env,
                                 jclass klass,
                                 jboolean* is_array_class_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::IsArrayClass(env, klass, is_array_class_ptr);
  }

  static jvmtiError IsModifiableClass(jvmtiEnv* env,
                                      jclass klass,
                                      jboolean* is_modifiable_class_ptr) {
    ENSURE_VALID_ENV(env);
    return Redefiner::IsModifiableClass(env, klass, is_modifiable_class_ptr);
  }

  static jvmtiError GetClassLoader(jvmtiEnv* env, jclass klass, jobject* classloader_ptr) {
    ENSURE_VALID_ENV(env);
    return ClassUtil::GetClassLoader(env, klass, classloader_ptr);
  }

  static jvmtiError GetSourceDebugExtension(jvmtiEnv* env,
                                            jclass klass,
                                            char** source_debug_extension_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_source_debug_extension);
    return ClassUtil::GetSourceDebugExtension(env, klass, source_debug_extension_ptr);
  }

  static jvmtiError RetransformClasses(jvmtiEnv* env, jint class_count, const jclass* classes) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_retransform_classes);
    std::string error_msg;
    jvmtiError res = Transformer::RetransformClasses(ArtJvmTiEnv::AsArtJvmTiEnv(env),
                                                     gEventHandler,
                                                     art::Runtime::Current(),
                                                     art::Thread::Current(),
                                                     class_count,
                                                     classes,
                                                     &error_msg);
    if (res != OK) {
      LOG(WARNING) << "FAILURE TO RETRANFORM " << error_msg;
    }
    return res;
  }

  static jvmtiError RedefineClasses(jvmtiEnv* env,
                                    jint class_count,
                                    const jvmtiClassDefinition* class_definitions) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_redefine_classes);
    std::string error_msg;
    jvmtiError res = Redefiner::RedefineClasses(ArtJvmTiEnv::AsArtJvmTiEnv(env),
                                                gEventHandler,
                                                art::Runtime::Current(),
                                                art::Thread::Current(),
                                                class_count,
                                                class_definitions,
                                                &error_msg);
    if (res != OK) {
      LOG(WARNING) << "FAILURE TO REDEFINE " << error_msg;
    }
    return res;
  }

  static jvmtiError GetObjectSize(jvmtiEnv* env, jobject object, jlong* size_ptr) {
    ENSURE_VALID_ENV(env);
    return ObjectUtil::GetObjectSize(env, object, size_ptr);
  }

  static jvmtiError GetObjectHashCode(jvmtiEnv* env, jobject object, jint* hash_code_ptr) {
    ENSURE_VALID_ENV(env);
    return ObjectUtil::GetObjectHashCode(env, object, hash_code_ptr);
  }

  static jvmtiError GetObjectMonitorUsage(jvmtiEnv* env,
                                          jobject object,
                                          jvmtiMonitorUsage* info_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_monitor_info);
    return ObjectUtil::GetObjectMonitorUsage(env, object, info_ptr);
  }

  static jvmtiError GetFieldName(jvmtiEnv* env,
                                 jclass klass,
                                 jfieldID field,
                                 char** name_ptr,
                                 char** signature_ptr,
                                 char** generic_ptr) {
    ENSURE_VALID_ENV(env);
    return FieldUtil::GetFieldName(env, klass, field, name_ptr, signature_ptr, generic_ptr);
  }

  static jvmtiError GetFieldDeclaringClass(jvmtiEnv* env,
                                           jclass klass,
                                           jfieldID field,
                                           jclass* declaring_class_ptr) {
    ENSURE_VALID_ENV(env);
    return FieldUtil::GetFieldDeclaringClass(env, klass, field, declaring_class_ptr);
  }

  static jvmtiError GetFieldModifiers(jvmtiEnv* env,
                                      jclass klass,
                                      jfieldID field,
                                      jint* modifiers_ptr) {
    ENSURE_VALID_ENV(env);
    return FieldUtil::GetFieldModifiers(env, klass, field, modifiers_ptr);
  }

  static jvmtiError IsFieldSynthetic(jvmtiEnv* env,
                                     jclass klass,
                                     jfieldID field,
                                     jboolean* is_synthetic_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_synthetic_attribute);
    return FieldUtil::IsFieldSynthetic(env, klass, field, is_synthetic_ptr);
  }

  static jvmtiError GetMethodName(jvmtiEnv* env,
                                  jmethodID method,
                                  char** name_ptr,
                                  char** signature_ptr,
                                  char** generic_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetMethodName(env, method, name_ptr, signature_ptr, generic_ptr);
  }

  static jvmtiError GetMethodDeclaringClass(jvmtiEnv* env,
                                            jmethodID method,
                                            jclass* declaring_class_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetMethodDeclaringClass(env, method, declaring_class_ptr);
  }

  static jvmtiError GetMethodModifiers(jvmtiEnv* env,
                                       jmethodID method,
                                       jint* modifiers_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetMethodModifiers(env, method, modifiers_ptr);
  }

  static jvmtiError GetMaxLocals(jvmtiEnv* env,
                                 jmethodID method,
                                 jint* max_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetMaxLocals(env, method, max_ptr);
  }

  static jvmtiError GetArgumentsSize(jvmtiEnv* env,
                                     jmethodID method,
                                     jint* size_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetArgumentsSize(env, method, size_ptr);
  }

  static jvmtiError GetLineNumberTable(jvmtiEnv* env,
                                       jmethodID method,
                                       jint* entry_count_ptr,
                                       jvmtiLineNumberEntry** table_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_line_numbers);
    return MethodUtil::GetLineNumberTable(env, method, entry_count_ptr, table_ptr);
  }

  static jvmtiError GetMethodLocation(jvmtiEnv* env,
                                      jmethodID method,
                                      jlocation* start_location_ptr,
                                      jlocation* end_location_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::GetMethodLocation(env, method, start_location_ptr, end_location_ptr);
  }

  static jvmtiError GetLocalVariableTable(jvmtiEnv* env,
                                          jmethodID method,
                                          jint* entry_count_ptr,
                                          jvmtiLocalVariableEntry** table_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_access_local_variables);
    return MethodUtil::GetLocalVariableTable(env, method, entry_count_ptr, table_ptr);
  }

  static jvmtiError GetBytecodes(jvmtiEnv* env,
                                 jmethodID method,
                                 jint* bytecode_count_ptr,
                                 unsigned char** bytecodes_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_bytecodes);
    return MethodUtil::GetBytecodes(env, method, bytecode_count_ptr, bytecodes_ptr);
  }

  static jvmtiError IsMethodNative(jvmtiEnv* env, jmethodID method, jboolean* is_native_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::IsMethodNative(env, method, is_native_ptr);
  }

  static jvmtiError IsMethodSynthetic(jvmtiEnv* env, jmethodID method, jboolean* is_synthetic_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_synthetic_attribute);
    return MethodUtil::IsMethodSynthetic(env, method, is_synthetic_ptr);
  }

  static jvmtiError IsMethodObsolete(jvmtiEnv* env, jmethodID method, jboolean* is_obsolete_ptr) {
    ENSURE_VALID_ENV(env);
    return MethodUtil::IsMethodObsolete(env, method, is_obsolete_ptr);
  }

  static jvmtiError SetNativeMethodPrefix(jvmtiEnv* env, const char* prefix ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_set_native_method_prefix);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError SetNativeMethodPrefixes(jvmtiEnv* env,
                                            jint prefix_count ATTRIBUTE_UNUSED,
                                            char** prefixes ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_set_native_method_prefix);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError CreateRawMonitor(jvmtiEnv* env, const char* name, jrawMonitorID* monitor_ptr) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::CreateRawMonitor(env, name, monitor_ptr);
  }

  static jvmtiError DestroyRawMonitor(jvmtiEnv* env, jrawMonitorID monitor) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::DestroyRawMonitor(env, monitor);
  }

  static jvmtiError RawMonitorEnter(jvmtiEnv* env, jrawMonitorID monitor) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::RawMonitorEnter(env, monitor);
  }

  static jvmtiError RawMonitorExit(jvmtiEnv* env, jrawMonitorID monitor) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::RawMonitorExit(env, monitor);
  }

  static jvmtiError RawMonitorWait(jvmtiEnv* env, jrawMonitorID monitor, jlong millis) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::RawMonitorWait(env, monitor, millis);
  }

  static jvmtiError RawMonitorNotify(jvmtiEnv* env, jrawMonitorID monitor) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::RawMonitorNotify(env, monitor);
  }

  static jvmtiError RawMonitorNotifyAll(jvmtiEnv* env, jrawMonitorID monitor) {
    ENSURE_VALID_ENV(env);
    return MonitorUtil::RawMonitorNotifyAll(env, monitor);
  }

  static jvmtiError SetJNIFunctionTable(jvmtiEnv* env, const jniNativeInterface* function_table) {
    ENSURE_VALID_ENV(env);
    return JNIUtil::SetJNIFunctionTable(env, function_table);
  }

  static jvmtiError GetJNIFunctionTable(jvmtiEnv* env, jniNativeInterface** function_table) {
    ENSURE_VALID_ENV(env);
    return JNIUtil::GetJNIFunctionTable(env, function_table);
  }

  // TODO: This will require locking, so that an agent can't remove callbacks when we're dispatching
  //       an event.
  static jvmtiError SetEventCallbacks(jvmtiEnv* env,
                                      const jvmtiEventCallbacks* callbacks,
                                      jint size_of_callbacks) {
    ENSURE_VALID_ENV(env);
    if (size_of_callbacks < 0) {
      return ERR(ILLEGAL_ARGUMENT);
    }

    if (callbacks == nullptr) {
      ArtJvmTiEnv::AsArtJvmTiEnv(env)->event_callbacks.reset();
      return ERR(NONE);
    }

    // Lock the event_info_mutex_ while we replace the callbacks.
    ArtJvmTiEnv* art_env = ArtJvmTiEnv::AsArtJvmTiEnv(env);
    art::WriterMutexLock lk(art::Thread::Current(), art_env->event_info_mutex_);
    std::unique_ptr<ArtJvmtiEventCallbacks> tmp(new ArtJvmtiEventCallbacks());
    // Copy over the extension events.
    tmp->CopyExtensionsFrom(art_env->event_callbacks.get());
    // Never overwrite the extension events.
    size_t copy_size = std::min(sizeof(jvmtiEventCallbacks),
                                static_cast<size_t>(size_of_callbacks));
    copy_size = art::RoundDown(copy_size, sizeof(void*));
    // Copy non-extension events.
    memcpy(tmp.get(), callbacks, copy_size);

    // replace the event table.
    art_env->event_callbacks = std::move(tmp);

    return ERR(NONE);
  }

  static jvmtiError SetEventNotificationMode(jvmtiEnv* env,
                                             jvmtiEventMode mode,
                                             jvmtiEvent event_type,
                                             jthread event_thread,
                                             ...) {
    ENSURE_VALID_ENV(env);
    art::Thread* art_thread = nullptr;
    if (event_thread != nullptr) {
      // TODO The locking around this call is less then what we really want.
      art::ScopedObjectAccess soa(art::Thread::Current());
      art::MutexLock mu(soa.Self(), *art::Locks::thread_list_lock_);
      jvmtiError err = ERR(INTERNAL);
      if (!ThreadUtil::GetAliveNativeThread(event_thread, soa, &art_thread, &err)) {
        return err;
      } else if (art_thread->IsStillStarting()) {
        return ERR(THREAD_NOT_ALIVE);
      }
    }

    ArtJvmTiEnv* art_env = ArtJvmTiEnv::AsArtJvmTiEnv(env);
    return gEventHandler->SetEvent(art_env,
                                   art_thread,
                                   GetArtJvmtiEvent(art_env, event_type),
                                   mode);
  }

  static jvmtiError GenerateEvents(jvmtiEnv* env,
                                   jvmtiEvent event_type ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    return OK;
  }

  static jvmtiError GetExtensionFunctions(jvmtiEnv* env,
                                          jint* extension_count_ptr,
                                          jvmtiExtensionFunctionInfo** extensions) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(extension_count_ptr);
    ENSURE_NON_NULL(extensions);
    return ExtensionUtil::GetExtensionFunctions(env, extension_count_ptr, extensions);
  }

  static jvmtiError GetExtensionEvents(jvmtiEnv* env,
                                       jint* extension_count_ptr,
                                       jvmtiExtensionEventInfo** extensions) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(extension_count_ptr);
    ENSURE_NON_NULL(extensions);
    return ExtensionUtil::GetExtensionEvents(env, extension_count_ptr, extensions);
  }

  static jvmtiError SetExtensionEventCallback(jvmtiEnv* env,
                                              jint extension_event_index,
                                              jvmtiExtensionEvent callback) {
    ENSURE_VALID_ENV(env);
    return ExtensionUtil::SetExtensionEventCallback(env,
                                                    extension_event_index,
                                                    callback,
                                                    gEventHandler);
  }

#define FOR_ALL_CAPABILITIES(FUN)                        \
    FUN(can_tag_objects)                                 \
    FUN(can_generate_field_modification_events)          \
    FUN(can_generate_field_access_events)                \
    FUN(can_get_bytecodes)                               \
    FUN(can_get_synthetic_attribute)                     \
    FUN(can_get_owned_monitor_info)                      \
    FUN(can_get_current_contended_monitor)               \
    FUN(can_get_monitor_info)                            \
    FUN(can_pop_frame)                                   \
    FUN(can_redefine_classes)                            \
    FUN(can_signal_thread)                               \
    FUN(can_get_source_file_name)                        \
    FUN(can_get_line_numbers)                            \
    FUN(can_get_source_debug_extension)                  \
    FUN(can_access_local_variables)                      \
    FUN(can_maintain_original_method_order)              \
    FUN(can_generate_single_step_events)                 \
    FUN(can_generate_exception_events)                   \
    FUN(can_generate_frame_pop_events)                   \
    FUN(can_generate_breakpoint_events)                  \
    FUN(can_suspend)                                     \
    FUN(can_redefine_any_class)                          \
    FUN(can_get_current_thread_cpu_time)                 \
    FUN(can_get_thread_cpu_time)                         \
    FUN(can_generate_method_entry_events)                \
    FUN(can_generate_method_exit_events)                 \
    FUN(can_generate_all_class_hook_events)              \
    FUN(can_generate_compiled_method_load_events)        \
    FUN(can_generate_monitor_events)                     \
    FUN(can_generate_vm_object_alloc_events)             \
    FUN(can_generate_native_method_bind_events)          \
    FUN(can_generate_garbage_collection_events)          \
    FUN(can_generate_object_free_events)                 \
    FUN(can_force_early_return)                          \
    FUN(can_get_owned_monitor_stack_depth_info)          \
    FUN(can_get_constant_pool)                           \
    FUN(can_set_native_method_prefix)                    \
    FUN(can_retransform_classes)                         \
    FUN(can_retransform_any_class)                       \
    FUN(can_generate_resource_exhaustion_heap_events)    \
    FUN(can_generate_resource_exhaustion_threads_events)

  static jvmtiError GetPotentialCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(capabilities_ptr);
    *capabilities_ptr = kPotentialCapabilities;
    if (UNLIKELY(!IsFullJvmtiAvailable())) {
#define REMOVE_NONDEBUGGABLE_UNSUPPORTED(e)                 \
      do {                                                  \
        if (kNonDebuggableUnsupportedCapabilities.e == 1) { \
          capabilities_ptr->e = 0;                          \
        }                                                   \
      } while (false);

      FOR_ALL_CAPABILITIES(REMOVE_NONDEBUGGABLE_UNSUPPORTED);
#undef REMOVE_NONDEBUGGABLE_UNSUPPORTED
    }
    return OK;
  }

  static jvmtiError AddCapabilities(jvmtiEnv* env, const jvmtiCapabilities* capabilities_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(capabilities_ptr);
    ArtJvmTiEnv* art_env = static_cast<ArtJvmTiEnv*>(env);
    jvmtiError ret = OK;
    jvmtiCapabilities changed = {};
    jvmtiCapabilities potential_capabilities = {};
    ret = env->GetPotentialCapabilities(&potential_capabilities);
    if (ret != OK) {
      return ret;
    }
#define ADD_CAPABILITY(e) \
    do { \
      if (capabilities_ptr->e == 1) { \
        if (potential_capabilities.e == 1) { \
          if (art_env->capabilities.e != 1) { \
            art_env->capabilities.e = 1; \
            changed.e = 1; \
          }\
        } else { \
          ret = ERR(NOT_AVAILABLE); \
        } \
      } \
    } while (false);

    FOR_ALL_CAPABILITIES(ADD_CAPABILITY);
#undef ADD_CAPABILITY
    gEventHandler->HandleChangedCapabilities(ArtJvmTiEnv::AsArtJvmTiEnv(env),
                                             changed,
                                             /*added*/true);
    return ret;
  }

  static jvmtiError RelinquishCapabilities(jvmtiEnv* env,
                                           const jvmtiCapabilities* capabilities_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(capabilities_ptr);
    ArtJvmTiEnv* art_env = reinterpret_cast<ArtJvmTiEnv*>(env);
    jvmtiCapabilities changed = {};
#define DEL_CAPABILITY(e) \
    do { \
      if (capabilities_ptr->e == 1) { \
        if (art_env->capabilities.e == 1) { \
          art_env->capabilities.e = 0;\
          changed.e = 1; \
        } \
      } \
    } while (false);

    FOR_ALL_CAPABILITIES(DEL_CAPABILITY);
#undef DEL_CAPABILITY
    gEventHandler->HandleChangedCapabilities(ArtJvmTiEnv::AsArtJvmTiEnv(env),
                                             changed,
                                             /*added*/false);
    return OK;
  }

#undef FOR_ALL_CAPABILITIES

  static jvmtiError GetCapabilities(jvmtiEnv* env, jvmtiCapabilities* capabilities_ptr) {
    ENSURE_VALID_ENV(env);
    ENSURE_NON_NULL(capabilities_ptr);
    ArtJvmTiEnv* artenv = reinterpret_cast<ArtJvmTiEnv*>(env);
    *capabilities_ptr = artenv->capabilities;
    return OK;
  }

  static jvmtiError GetCurrentThreadCpuTimerInfo(jvmtiEnv* env,
                                                 jvmtiTimerInfo* info_ptr ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_current_thread_cpu_time);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetCurrentThreadCpuTime(jvmtiEnv* env, jlong* nanos_ptr ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_current_thread_cpu_time);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTimerInfo(jvmtiEnv* env,
                                          jvmtiTimerInfo* info_ptr ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_thread_cpu_time);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetThreadCpuTime(jvmtiEnv* env,
                                     jthread thread ATTRIBUTE_UNUSED,
                                     jlong* nanos_ptr ATTRIBUTE_UNUSED) {
    ENSURE_VALID_ENV(env);
    ENSURE_HAS_CAP(env, can_get_thread_cpu_time);
    return ERR(NOT_IMPLEMENTED);
  }

  static jvmtiError GetTimerInfo(jvmtiEnv* env, jvmtiTimerInfo* info_ptr) {
    ENSURE_VALID_ENV(env);
    return TimerUtil::GetTimerInfo(env, info_ptr);
  }

  static jvmtiError GetTime(jvmtiEnv* env, jlong* nanos_ptr) {
    ENSURE_VALID_ENV(env);
    return TimerUtil::GetTime(env, nanos_ptr);
  }

  static jvmtiError GetAvailableProcessors(jvmtiEnv* env, jint* processor_count_ptr) {
    ENSURE_VALID_ENV(env);
    return TimerUtil::GetAvailableProcessors(env, processor_count_ptr);
  }

  static jvmtiError AddToBootstrapClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    ENSURE_VALID_ENV(env);
    return SearchUtil::AddToBootstrapClassLoaderSearch(env, segment);
  }

  static jvmtiError AddToSystemClassLoaderSearch(jvmtiEnv* env, const char* segment) {
    ENSURE_VALID_ENV(env);
    return SearchUtil::AddToSystemClassLoaderSearch(env, segment);
  }

  static jvmtiError GetSystemProperties(jvmtiEnv* env, jint* count_ptr, char*** property_ptr) {
    ENSURE_VALID_ENV(env);
    return PropertiesUtil::GetSystemProperties(env, count_ptr, property_ptr);
  }

  static jvmtiError GetSystemProperty(jvmtiEnv* env, const char* property, char** value_ptr) {
    ENSURE_VALID_ENV(env);
    return PropertiesUtil::GetSystemProperty(env, property, value_ptr);
  }

  static jvmtiError SetSystemProperty(jvmtiEnv* env, const char* property, const char* value) {
    ENSURE_VALID_ENV(env);
    return PropertiesUtil::SetSystemProperty(env, property, value);
  }

  static jvmtiError GetPhase(jvmtiEnv* env, jvmtiPhase* phase_ptr) {
    ENSURE_VALID_ENV(env);
    return PhaseUtil::GetPhase(env, phase_ptr);
  }

  static jvmtiError DisposeEnvironment(jvmtiEnv* env) {
    ENSURE_VALID_ENV(env);
    ArtJvmTiEnv* tienv = ArtJvmTiEnv::AsArtJvmTiEnv(env);
    gEventHandler->RemoveArtJvmTiEnv(tienv);
    art::Runtime::Current()->RemoveSystemWeakHolder(tienv->object_tag_table.get());
    ThreadUtil::RemoveEnvironment(tienv);
    delete tienv;
    return OK;
  }

  static jvmtiError SetEnvironmentLocalStorage(jvmtiEnv* env, const void* data) {
    ENSURE_VALID_ENV(env);
    reinterpret_cast<ArtJvmTiEnv*>(env)->local_data = const_cast<void*>(data);
    return OK;
  }

  static jvmtiError GetEnvironmentLocalStorage(jvmtiEnv* env, void** data_ptr) {
    ENSURE_VALID_ENV(env);
    *data_ptr = reinterpret_cast<ArtJvmTiEnv*>(env)->local_data;
    return OK;
  }

  static jvmtiError GetVersionNumber(jvmtiEnv* env, jint* version_ptr) {
    ENSURE_VALID_ENV(env);
    *version_ptr = ArtJvmTiEnv::AsArtJvmTiEnv(env)->ti_version;
    return OK;
  }

  static jvmtiError GetErrorName(jvmtiEnv* env, jvmtiError error,  char** name_ptr) {
    ENSURE_NON_NULL(name_ptr);
    auto copy_fn = [&](const char* name_cstr) {
      jvmtiError res;
      JvmtiUniquePtr<char[]> copy = CopyString(env, name_cstr, &res);
      if (copy == nullptr) {
        *name_ptr = nullptr;
        return res;
      } else {
        *name_ptr = copy.release();
        return OK;
      }
    };
    switch (error) {
#define ERROR_CASE(e) case (JVMTI_ERROR_ ## e) : \
        return copy_fn("JVMTI_ERROR_"#e);
      ERROR_CASE(NONE);
      ERROR_CASE(INVALID_THREAD);
      ERROR_CASE(INVALID_THREAD_GROUP);
      ERROR_CASE(INVALID_PRIORITY);
      ERROR_CASE(THREAD_NOT_SUSPENDED);
      ERROR_CASE(THREAD_SUSPENDED);
      ERROR_CASE(THREAD_NOT_ALIVE);
      ERROR_CASE(INVALID_OBJECT);
      ERROR_CASE(INVALID_CLASS);
      ERROR_CASE(CLASS_NOT_PREPARED);
      ERROR_CASE(INVALID_METHODID);
      ERROR_CASE(INVALID_LOCATION);
      ERROR_CASE(INVALID_FIELDID);
      ERROR_CASE(NO_MORE_FRAMES);
      ERROR_CASE(OPAQUE_FRAME);
      ERROR_CASE(TYPE_MISMATCH);
      ERROR_CASE(INVALID_SLOT);
      ERROR_CASE(DUPLICATE);
      ERROR_CASE(NOT_FOUND);
      ERROR_CASE(INVALID_MONITOR);
      ERROR_CASE(NOT_MONITOR_OWNER);
      ERROR_CASE(INTERRUPT);
      ERROR_CASE(INVALID_CLASS_FORMAT);
      ERROR_CASE(CIRCULAR_CLASS_DEFINITION);
      ERROR_CASE(FAILS_VERIFICATION);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_ADDED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED);
      ERROR_CASE(INVALID_TYPESTATE);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_DELETED);
      ERROR_CASE(UNSUPPORTED_VERSION);
      ERROR_CASE(NAMES_DONT_MATCH);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED);
      ERROR_CASE(UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED);
      ERROR_CASE(UNMODIFIABLE_CLASS);
      ERROR_CASE(NOT_AVAILABLE);
      ERROR_CASE(MUST_POSSESS_CAPABILITY);
      ERROR_CASE(NULL_POINTER);
      ERROR_CASE(ABSENT_INFORMATION);
      ERROR_CASE(INVALID_EVENT_TYPE);
      ERROR_CASE(ILLEGAL_ARGUMENT);
      ERROR_CASE(NATIVE_METHOD);
      ERROR_CASE(CLASS_LOADER_UNSUPPORTED);
      ERROR_CASE(OUT_OF_MEMORY);
      ERROR_CASE(ACCESS_DENIED);
      ERROR_CASE(WRONG_PHASE);
      ERROR_CASE(INTERNAL);
      ERROR_CASE(UNATTACHED_THREAD);
      ERROR_CASE(INVALID_ENVIRONMENT);
#undef ERROR_CASE
    }

    return ERR(ILLEGAL_ARGUMENT);
  }

  static jvmtiError SetVerboseFlag(jvmtiEnv* env,
                                   jvmtiVerboseFlag flag,
                                   jboolean value) {
    ENSURE_VALID_ENV(env);
    if (flag == jvmtiVerboseFlag::JVMTI_VERBOSE_OTHER) {
      // OTHER is special, as it's 0, so can't do a bit check.
      bool val = (value == JNI_TRUE) ? true : false;

      art::gLogVerbosity.collector = val;
      art::gLogVerbosity.compiler = val;
      art::gLogVerbosity.deopt = val;
      art::gLogVerbosity.heap = val;
      art::gLogVerbosity.jdwp = val;
      art::gLogVerbosity.jit = val;
      art::gLogVerbosity.monitor = val;
      art::gLogVerbosity.oat = val;
      art::gLogVerbosity.profiler = val;
      art::gLogVerbosity.signals = val;
      art::gLogVerbosity.simulator = val;
      art::gLogVerbosity.startup = val;
      art::gLogVerbosity.third_party_jni = val;
      art::gLogVerbosity.threads = val;
      art::gLogVerbosity.verifier = val;
      // Do not set verifier-debug.
      art::gLogVerbosity.image = val;

      // Note: can't switch systrace_lock_logging. That requires changing entrypoints.

      art::gLogVerbosity.agents = val;
    } else {
      // Spec isn't clear whether "flag" is a mask or supposed to be single. We implement the mask
      // semantics.
      constexpr std::underlying_type<jvmtiVerboseFlag>::type kMask =
          jvmtiVerboseFlag::JVMTI_VERBOSE_GC |
          jvmtiVerboseFlag::JVMTI_VERBOSE_CLASS |
          jvmtiVerboseFlag::JVMTI_VERBOSE_JNI;
      if ((flag & ~kMask) != 0) {
        return ERR(ILLEGAL_ARGUMENT);
      }

      bool val = (value == JNI_TRUE) ? true : false;

      if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_GC) != 0) {
        art::gLogVerbosity.gc = val;
      }

      if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_CLASS) != 0) {
        art::gLogVerbosity.class_linker = val;
      }

      if ((flag & jvmtiVerboseFlag::JVMTI_VERBOSE_JNI) != 0) {
        art::gLogVerbosity.jni = val;
      }
    }

    return ERR(NONE);
  }

  static jvmtiError GetJLocationFormat(jvmtiEnv* env, jvmtiJlocationFormat* format_ptr) {
    ENSURE_VALID_ENV(env);
    // Report BCI as jlocation format. We report dex bytecode indices.
    if (format_ptr == nullptr) {
      return ERR(NULL_POINTER);
    }
    *format_ptr = jvmtiJlocationFormat::JVMTI_JLOCATION_JVMBCI;
    return ERR(NONE);
  }
};

static bool IsJvmtiVersion(jint version) {
  return version ==  JVMTI_VERSION_1 ||
         version == JVMTI_VERSION_1_0 ||
         version == JVMTI_VERSION_1_1 ||
         version == JVMTI_VERSION_1_2 ||
         version == JVMTI_VERSION;
}

extern const jvmtiInterface_1 gJvmtiInterface;

ArtJvmTiEnv::ArtJvmTiEnv(art::JavaVMExt* runtime, EventHandler* event_handler, jint version)
    : art_vm(runtime),
      local_data(nullptr),
      ti_version(version),
      capabilities(),
      event_info_mutex_("jvmtiEnv_EventInfoMutex") {
  object_tag_table = std::unique_ptr<ObjectTagTable>(new ObjectTagTable(event_handler, this));
  functions = &gJvmtiInterface;
}

// Creates a jvmtiEnv and returns it with the art::ti::Env that is associated with it. new_art_ti
// is a pointer to the uninitialized memory for an art::ti::Env.
static void CreateArtJvmTiEnv(art::JavaVMExt* vm, jint version, /*out*/void** new_jvmtiEnv) {
  struct ArtJvmTiEnv* env = new ArtJvmTiEnv(vm, gEventHandler, version);
  *new_jvmtiEnv = env;

  gEventHandler->RegisterArtJvmTiEnv(env);

  art::Runtime::Current()->AddSystemWeakHolder(
      ArtJvmTiEnv::AsArtJvmTiEnv(env)->object_tag_table.get());
}

// A hook that the runtime uses to allow plugins to handle GetEnv calls. It returns true and
// places the return value in 'env' if this library can handle the GetEnv request. Otherwise
// returns false and does not modify the 'env' pointer.
static jint GetEnvHandler(art::JavaVMExt* vm, /*out*/void** env, jint version) {
  // JavaDebuggable will either be set by the runtime as it is starting up or the plugin if it's
  // loaded early enough. If this is false we cannot guarantee conformance to all JVMTI behaviors
  // due to optimizations. We will only allow agents to get ArtTiEnvs using the kArtTiVersion.
  if (IsFullJvmtiAvailable() && IsJvmtiVersion(version)) {
    CreateArtJvmTiEnv(vm, JVMTI_VERSION, env);
    return JNI_OK;
  } else if (version == kArtTiVersion) {
    CreateArtJvmTiEnv(vm, kArtTiVersion, env);
    return JNI_OK;
  } else {
    printf("version 0x%x is not valid!", version);
    return JNI_EVERSION;
  }
}

// The plugin initialization function. This adds the jvmti environment.
extern "C" bool ArtPlugin_Initialize() {
  art::Runtime* runtime = art::Runtime::Current();

  gDeoptManager = new DeoptManager;
  gEventHandler = new EventHandler;

  gDeoptManager->Setup();
  if (runtime->IsStarted()) {
    PhaseUtil::SetToLive();
  } else {
    PhaseUtil::SetToOnLoad();
  }
  PhaseUtil::Register(gEventHandler);
  ThreadUtil::Register(gEventHandler);
  ClassUtil::Register(gEventHandler);
  DumpUtil::Register(gEventHandler);
  MethodUtil::Register(gEventHandler);
  SearchUtil::Register();
  HeapUtil::Register();
  Transformer::Setup();

  {
    // Make sure we can deopt anything we need to.
    art::ScopedObjectAccess soa(art::Thread::Current());
    gDeoptManager->FinishSetup();
  }

  runtime->GetJavaVM()->AddEnvironmentHook(GetEnvHandler);

  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  gEventHandler->Shutdown();
  gDeoptManager->Shutdown();
  PhaseUtil::Unregister();
  ThreadUtil::Unregister();
  ClassUtil::Unregister();
  DumpUtil::Unregister();
  MethodUtil::Unregister();
  SearchUtil::Unregister();
  HeapUtil::Unregister();

  // TODO It would be good to delete the gEventHandler and gDeoptManager here but we cannot since
  // daemon threads might be suspended and we want to make sure that even if they wake up briefly
  // they won't hit deallocated memory. By this point none of the functions will do anything since
  // they have already shutdown.

  return true;
}

// The actual struct holding all of the entrypoints into the jvmti interface.
const jvmtiInterface_1 gJvmtiInterface = {
  nullptr,  // reserved1
  JvmtiFunctions::SetEventNotificationMode,
  nullptr,  // reserved3
  JvmtiFunctions::GetAllThreads,
  JvmtiFunctions::SuspendThread,
  JvmtiFunctions::ResumeThread,
  JvmtiFunctions::StopThread,
  JvmtiFunctions::InterruptThread,
  JvmtiFunctions::GetThreadInfo,
  JvmtiFunctions::GetOwnedMonitorInfo,  // 10
  JvmtiFunctions::GetCurrentContendedMonitor,
  JvmtiFunctions::RunAgentThread,
  JvmtiFunctions::GetTopThreadGroups,
  JvmtiFunctions::GetThreadGroupInfo,
  JvmtiFunctions::GetThreadGroupChildren,
  JvmtiFunctions::GetFrameCount,
  JvmtiFunctions::GetThreadState,
  JvmtiFunctions::GetCurrentThread,
  JvmtiFunctions::GetFrameLocation,
  JvmtiFunctions::NotifyFramePop,  // 20
  JvmtiFunctions::GetLocalObject,
  JvmtiFunctions::GetLocalInt,
  JvmtiFunctions::GetLocalLong,
  JvmtiFunctions::GetLocalFloat,
  JvmtiFunctions::GetLocalDouble,
  JvmtiFunctions::SetLocalObject,
  JvmtiFunctions::SetLocalInt,
  JvmtiFunctions::SetLocalLong,
  JvmtiFunctions::SetLocalFloat,
  JvmtiFunctions::SetLocalDouble,  // 30
  JvmtiFunctions::CreateRawMonitor,
  JvmtiFunctions::DestroyRawMonitor,
  JvmtiFunctions::RawMonitorEnter,
  JvmtiFunctions::RawMonitorExit,
  JvmtiFunctions::RawMonitorWait,
  JvmtiFunctions::RawMonitorNotify,
  JvmtiFunctions::RawMonitorNotifyAll,
  JvmtiFunctions::SetBreakpoint,
  JvmtiFunctions::ClearBreakpoint,
  nullptr,  // reserved40
  JvmtiFunctions::SetFieldAccessWatch,
  JvmtiFunctions::ClearFieldAccessWatch,
  JvmtiFunctions::SetFieldModificationWatch,
  JvmtiFunctions::ClearFieldModificationWatch,
  JvmtiFunctions::IsModifiableClass,
  JvmtiFunctions::Allocate,
  JvmtiFunctions::Deallocate,
  JvmtiFunctions::GetClassSignature,
  JvmtiFunctions::GetClassStatus,
  JvmtiFunctions::GetSourceFileName,  // 50
  JvmtiFunctions::GetClassModifiers,
  JvmtiFunctions::GetClassMethods,
  JvmtiFunctions::GetClassFields,
  JvmtiFunctions::GetImplementedInterfaces,
  JvmtiFunctions::IsInterface,
  JvmtiFunctions::IsArrayClass,
  JvmtiFunctions::GetClassLoader,
  JvmtiFunctions::GetObjectHashCode,
  JvmtiFunctions::GetObjectMonitorUsage,
  JvmtiFunctions::GetFieldName,  // 60
  JvmtiFunctions::GetFieldDeclaringClass,
  JvmtiFunctions::GetFieldModifiers,
  JvmtiFunctions::IsFieldSynthetic,
  JvmtiFunctions::GetMethodName,
  JvmtiFunctions::GetMethodDeclaringClass,
  JvmtiFunctions::GetMethodModifiers,
  nullptr,  // reserved67
  JvmtiFunctions::GetMaxLocals,
  JvmtiFunctions::GetArgumentsSize,
  JvmtiFunctions::GetLineNumberTable,  // 70
  JvmtiFunctions::GetMethodLocation,
  JvmtiFunctions::GetLocalVariableTable,
  JvmtiFunctions::SetNativeMethodPrefix,
  JvmtiFunctions::SetNativeMethodPrefixes,
  JvmtiFunctions::GetBytecodes,
  JvmtiFunctions::IsMethodNative,
  JvmtiFunctions::IsMethodSynthetic,
  JvmtiFunctions::GetLoadedClasses,
  JvmtiFunctions::GetClassLoaderClasses,
  JvmtiFunctions::PopFrame,  // 80
  JvmtiFunctions::ForceEarlyReturnObject,
  JvmtiFunctions::ForceEarlyReturnInt,
  JvmtiFunctions::ForceEarlyReturnLong,
  JvmtiFunctions::ForceEarlyReturnFloat,
  JvmtiFunctions::ForceEarlyReturnDouble,
  JvmtiFunctions::ForceEarlyReturnVoid,
  JvmtiFunctions::RedefineClasses,
  JvmtiFunctions::GetVersionNumber,
  JvmtiFunctions::GetCapabilities,
  JvmtiFunctions::GetSourceDebugExtension,  // 90
  JvmtiFunctions::IsMethodObsolete,
  JvmtiFunctions::SuspendThreadList,
  JvmtiFunctions::ResumeThreadList,
  nullptr,  // reserved94
  nullptr,  // reserved95
  nullptr,  // reserved96
  nullptr,  // reserved97
  nullptr,  // reserved98
  nullptr,  // reserved99
  JvmtiFunctions::GetAllStackTraces,  // 100
  JvmtiFunctions::GetThreadListStackTraces,
  JvmtiFunctions::GetThreadLocalStorage,
  JvmtiFunctions::SetThreadLocalStorage,
  JvmtiFunctions::GetStackTrace,
  nullptr,  // reserved105
  JvmtiFunctions::GetTag,
  JvmtiFunctions::SetTag,
  JvmtiFunctions::ForceGarbageCollection,
  JvmtiFunctions::IterateOverObjectsReachableFromObject,
  JvmtiFunctions::IterateOverReachableObjects,  // 110
  JvmtiFunctions::IterateOverHeap,
  JvmtiFunctions::IterateOverInstancesOfClass,
  nullptr,  // reserved113
  JvmtiFunctions::GetObjectsWithTags,
  JvmtiFunctions::FollowReferences,
  JvmtiFunctions::IterateThroughHeap,
  nullptr,  // reserved117
  nullptr,  // reserved118
  nullptr,  // reserved119
  JvmtiFunctions::SetJNIFunctionTable,  // 120
  JvmtiFunctions::GetJNIFunctionTable,
  JvmtiFunctions::SetEventCallbacks,
  JvmtiFunctions::GenerateEvents,
  JvmtiFunctions::GetExtensionFunctions,
  JvmtiFunctions::GetExtensionEvents,
  JvmtiFunctions::SetExtensionEventCallback,
  JvmtiFunctions::DisposeEnvironment,
  JvmtiFunctions::GetErrorName,
  JvmtiFunctions::GetJLocationFormat,
  JvmtiFunctions::GetSystemProperties,  // 130
  JvmtiFunctions::GetSystemProperty,
  JvmtiFunctions::SetSystemProperty,
  JvmtiFunctions::GetPhase,
  JvmtiFunctions::GetCurrentThreadCpuTimerInfo,
  JvmtiFunctions::GetCurrentThreadCpuTime,
  JvmtiFunctions::GetThreadCpuTimerInfo,
  JvmtiFunctions::GetThreadCpuTime,
  JvmtiFunctions::GetTimerInfo,
  JvmtiFunctions::GetTime,
  JvmtiFunctions::GetPotentialCapabilities,  // 140
  nullptr,  // reserved141
  JvmtiFunctions::AddCapabilities,
  JvmtiFunctions::RelinquishCapabilities,
  JvmtiFunctions::GetAvailableProcessors,
  JvmtiFunctions::GetClassVersionNumbers,
  JvmtiFunctions::GetConstantPool,
  JvmtiFunctions::GetEnvironmentLocalStorage,
  JvmtiFunctions::SetEnvironmentLocalStorage,
  JvmtiFunctions::AddToBootstrapClassLoaderSearch,
  JvmtiFunctions::SetVerboseFlag,  // 150
  JvmtiFunctions::AddToSystemClassLoaderSearch,
  JvmtiFunctions::RetransformClasses,
  JvmtiFunctions::GetOwnedMonitorStackDepthInfo,
  JvmtiFunctions::GetObjectSize,
  JvmtiFunctions::GetLocalInstance,
};

};  // namespace openjdkjvmti
