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

#include "jvmti_helper.h"
#include "test_env.h"

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "android-base/logging.h"
#include "scoped_local_ref.h"

namespace art {

void CheckJvmtiError(jvmtiEnv* env, jvmtiError error) {
  if (error != JVMTI_ERROR_NONE) {
    char* error_name;
    jvmtiError name_error = env->GetErrorName(error, &error_name);
    if (name_error != JVMTI_ERROR_NONE) {
      LOG(FATAL) << "Unable to get error name for " << error;
    }
    LOG(FATAL) << "Unexpected error: " << error_name;
  }
}

// These are a set of capabilities we will enable in all situations. These are chosen since they
// will not affect the runtime in any significant way if they are enabled.
static const jvmtiCapabilities standard_caps = {
    .can_tag_objects                                 = 1,
    .can_generate_field_modification_events          = 1,
    .can_generate_field_access_events                = 1,
    .can_get_bytecodes                               = 1,
    .can_get_synthetic_attribute                     = 1,
    .can_get_owned_monitor_info                      = 0,
    .can_get_current_contended_monitor               = 1,
    .can_get_monitor_info                            = 1,
    .can_pop_frame                                   = 0,
    .can_redefine_classes                            = 1,
    .can_signal_thread                               = 1,
    .can_get_source_file_name                        = 1,
    .can_get_line_numbers                            = 1,
    .can_get_source_debug_extension                  = 1,
    .can_access_local_variables                      = 0,
    .can_maintain_original_method_order              = 1,
    .can_generate_single_step_events                 = 1,
    .can_generate_exception_events                   = 0,
    .can_generate_frame_pop_events                   = 0,
    .can_generate_breakpoint_events                  = 1,
    .can_suspend                                     = 1,
    .can_redefine_any_class                          = 0,
    .can_get_current_thread_cpu_time                 = 0,
    .can_get_thread_cpu_time                         = 0,
    .can_generate_method_entry_events                = 1,
    .can_generate_method_exit_events                 = 1,
    .can_generate_all_class_hook_events              = 0,
    .can_generate_compiled_method_load_events        = 0,
    .can_generate_monitor_events                     = 0,
    .can_generate_vm_object_alloc_events             = 1,
    .can_generate_native_method_bind_events          = 1,
    .can_generate_garbage_collection_events          = 1,
    .can_generate_object_free_events                 = 1,
    .can_force_early_return                          = 0,
    .can_get_owned_monitor_stack_depth_info          = 0,
    .can_get_constant_pool                           = 0,
    .can_set_native_method_prefix                    = 0,
    .can_retransform_classes                         = 1,
    .can_retransform_any_class                       = 0,
    .can_generate_resource_exhaustion_heap_events    = 0,
    .can_generate_resource_exhaustion_threads_events = 0,
};

jvmtiCapabilities GetStandardCapabilities() {
  return standard_caps;
}

void SetStandardCapabilities(jvmtiEnv* env) {
  if (IsJVM()) {
    // RI is more strict about adding capabilities at runtime then ART so just give it everything.
    SetAllCapabilities(env);
    return;
  }
  jvmtiCapabilities caps = GetStandardCapabilities();
  CheckJvmtiError(env, env->AddCapabilities(&caps));
}

void SetAllCapabilities(jvmtiEnv* env) {
  jvmtiCapabilities caps;
  CheckJvmtiError(env, env->GetPotentialCapabilities(&caps));
  CheckJvmtiError(env, env->AddCapabilities(&caps));
}

bool JvmtiErrorToException(JNIEnv* env, jvmtiEnv* jvmtienv, jvmtiError error) {
  if (error == JVMTI_ERROR_NONE) {
    return false;
  }

  ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
  if (rt_exception.get() == nullptr) {
    // CNFE should be pending.
    return true;
  }

  char* err;
  CheckJvmtiError(jvmtienv, jvmtienv->GetErrorName(error, &err));

  env->ThrowNew(rt_exception.get(), err);

  Deallocate(jvmtienv, err);
  return true;
}

std::ostream& operator<<(std::ostream& os, const jvmtiError& rhs) {
  switch (rhs) {
    case JVMTI_ERROR_NONE:
      return os << "NONE";
    case JVMTI_ERROR_INVALID_THREAD:
      return os << "INVALID_THREAD";
    case JVMTI_ERROR_INVALID_THREAD_GROUP:
      return os << "INVALID_THREAD_GROUP";
    case JVMTI_ERROR_INVALID_PRIORITY:
      return os << "INVALID_PRIORITY";
    case JVMTI_ERROR_THREAD_NOT_SUSPENDED:
      return os << "THREAD_NOT_SUSPENDED";
    case JVMTI_ERROR_THREAD_SUSPENDED:
      return os << "THREAD_SUSPENDED";
    case JVMTI_ERROR_THREAD_NOT_ALIVE:
      return os << "THREAD_NOT_ALIVE";
    case JVMTI_ERROR_INVALID_OBJECT:
      return os << "INVALID_OBJECT";
    case JVMTI_ERROR_INVALID_CLASS:
      return os << "INVALID_CLASS";
    case JVMTI_ERROR_CLASS_NOT_PREPARED:
      return os << "CLASS_NOT_PREPARED";
    case JVMTI_ERROR_INVALID_METHODID:
      return os << "INVALID_METHODID";
    case JVMTI_ERROR_INVALID_LOCATION:
      return os << "INVALID_LOCATION";
    case JVMTI_ERROR_INVALID_FIELDID:
      return os << "INVALID_FIELDID";
    case JVMTI_ERROR_NO_MORE_FRAMES:
      return os << "NO_MORE_FRAMES";
    case JVMTI_ERROR_OPAQUE_FRAME:
      return os << "OPAQUE_FRAME";
    case JVMTI_ERROR_TYPE_MISMATCH:
      return os << "TYPE_MISMATCH";
    case JVMTI_ERROR_INVALID_SLOT:
      return os << "INVALID_SLOT";
    case JVMTI_ERROR_DUPLICATE:
      return os << "DUPLICATE";
    case JVMTI_ERROR_NOT_FOUND:
      return os << "NOT_FOUND";
    case JVMTI_ERROR_INVALID_MONITOR:
      return os << "INVALID_MONITOR";
    case JVMTI_ERROR_NOT_MONITOR_OWNER:
      return os << "NOT_MONITOR_OWNER";
    case JVMTI_ERROR_INTERRUPT:
      return os << "INTERRUPT";
    case JVMTI_ERROR_INVALID_CLASS_FORMAT:
      return os << "INVALID_CLASS_FORMAT";
    case JVMTI_ERROR_CIRCULAR_CLASS_DEFINITION:
      return os << "CIRCULAR_CLASS_DEFINITION";
    case JVMTI_ERROR_FAILS_VERIFICATION:
      return os << "FAILS_VERIFICATION";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_ADDED:
      return os << "UNSUPPORTED_REDEFINITION_METHOD_ADDED";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED:
      return os << "UNSUPPORTED_REDEFINITION_SCHEMA_CHANGED";
    case JVMTI_ERROR_INVALID_TYPESTATE:
      return os << "INVALID_TYPESTATE";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED:
      return os << "UNSUPPORTED_REDEFINITION_HIERARCHY_CHANGED";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_DELETED:
      return os << "UNSUPPORTED_REDEFINITION_METHOD_DELETED";
    case JVMTI_ERROR_UNSUPPORTED_VERSION:
      return os << "UNSUPPORTED_VERSION";
    case JVMTI_ERROR_NAMES_DONT_MATCH:
      return os << "NAMES_DONT_MATCH";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED:
      return os << "UNSUPPORTED_REDEFINITION_CLASS_MODIFIERS_CHANGED";
    case JVMTI_ERROR_UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED:
      return os << "UNSUPPORTED_REDEFINITION_METHOD_MODIFIERS_CHANGED";
    case JVMTI_ERROR_UNMODIFIABLE_CLASS:
      return os << "JVMTI_ERROR_UNMODIFIABLE_CLASS";
    case JVMTI_ERROR_NOT_AVAILABLE:
      return os << "NOT_AVAILABLE";
    case JVMTI_ERROR_MUST_POSSESS_CAPABILITY:
      return os << "MUST_POSSESS_CAPABILITY";
    case JVMTI_ERROR_NULL_POINTER:
      return os << "NULL_POINTER";
    case JVMTI_ERROR_ABSENT_INFORMATION:
      return os << "ABSENT_INFORMATION";
    case JVMTI_ERROR_INVALID_EVENT_TYPE:
      return os << "INVALID_EVENT_TYPE";
    case JVMTI_ERROR_ILLEGAL_ARGUMENT:
      return os << "ILLEGAL_ARGUMENT";
    case JVMTI_ERROR_NATIVE_METHOD:
      return os << "NATIVE_METHOD";
    case JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED:
      return os << "CLASS_LOADER_UNSUPPORTED";
    case JVMTI_ERROR_OUT_OF_MEMORY:
      return os << "OUT_OF_MEMORY";
    case JVMTI_ERROR_ACCESS_DENIED:
      return os << "ACCESS_DENIED";
    case JVMTI_ERROR_WRONG_PHASE:
      return os << "WRONG_PHASE";
    case JVMTI_ERROR_INTERNAL:
      return os << "INTERNAL";
    case JVMTI_ERROR_UNATTACHED_THREAD:
      return os << "UNATTACHED_THREAD";
    case JVMTI_ERROR_INVALID_ENVIRONMENT:
      return os << "INVALID_ENVIRONMENT";
  }
  LOG(FATAL) << "Unexpected error type " << static_cast<int>(rhs);
  __builtin_unreachable();
}

}  // namespace art
