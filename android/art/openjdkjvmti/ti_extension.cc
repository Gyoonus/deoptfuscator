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

#include <vector>

#include "ti_extension.h"

#include "art_jvmti.h"
#include "events.h"
#include "ti_allocator.h"
#include "ti_class.h"
#include "ti_ddms.h"
#include "ti_heap.h"
#include "thread-inl.h"

namespace openjdkjvmti {

struct CParamInfo {
  const char* name;
  jvmtiParamKind kind;
  jvmtiParamTypes base_type;
  jboolean null_ok;

  jvmtiParamInfo ToParamInfo(jvmtiEnv* env,
                             /*out*/std::vector<JvmtiUniquePtr<char[]>>* char_buffers,
                             /*out*/jvmtiError* err) const {
    JvmtiUniquePtr<char[]> param_name = CopyString(env, name, err);
    char* name_ptr = param_name.get();
    char_buffers->push_back(std::move(param_name));
    return jvmtiParamInfo{ name_ptr, kind, base_type, null_ok };
  }
};

jvmtiError ExtensionUtil::GetExtensionFunctions(jvmtiEnv* env,
                                                jint* extension_count_ptr,
                                                jvmtiExtensionFunctionInfo** extensions) {
  if (extension_count_ptr == nullptr || extensions == nullptr) {
    return ERR(NULL_POINTER);
  }

  std::vector<jvmtiExtensionFunctionInfo> ext_vector;

  // Holders for allocated values.
  std::vector<JvmtiUniquePtr<char[]>> char_buffers;
  std::vector<JvmtiUniquePtr<jvmtiParamInfo[]>> param_buffers;
  std::vector<JvmtiUniquePtr<jvmtiError[]>> error_buffers;

  auto add_extension = [&](jvmtiExtensionFunction func,
                           const char* id,
                           const char* short_description,
                           const std::vector<CParamInfo>& params,
                           const std::vector<jvmtiError>& errors) {
    jvmtiExtensionFunctionInfo func_info;
    jvmtiError error;

    func_info.func = func;

    JvmtiUniquePtr<char[]> id_ptr = CopyString(env, id, &error);
    if (id_ptr == nullptr) {
      return error;
    }
    func_info.id = id_ptr.get();
    char_buffers.push_back(std::move(id_ptr));

    JvmtiUniquePtr<char[]> descr = CopyString(env, short_description, &error);
    if (descr == nullptr) {
      return error;
    }
    func_info.short_description = descr.get();
    char_buffers.push_back(std::move(descr));

    func_info.param_count = params.size();
    if (!params.empty()) {
      JvmtiUniquePtr<jvmtiParamInfo[]> params_ptr =
          AllocJvmtiUniquePtr<jvmtiParamInfo[]>(env, params.size(), &error);
      if (params_ptr == nullptr) {
        return error;
      }
      func_info.params = params_ptr.get();
      param_buffers.push_back(std::move(params_ptr));

      for (jint i = 0; i != func_info.param_count; ++i) {
        func_info.params[i] = params[i].ToParamInfo(env, &char_buffers, &error);
        if (error != OK) {
          return error;
        }
      }
    } else {
      func_info.params = nullptr;
    }

    func_info.error_count = errors.size();
    if (!errors.empty()) {
      JvmtiUniquePtr<jvmtiError[]> errors_ptr =
          AllocJvmtiUniquePtr<jvmtiError[]>(env, errors.size(), &error);
      if (errors_ptr == nullptr) {
        return error;
      }
      func_info.errors = errors_ptr.get();
      error_buffers.push_back(std::move(errors_ptr));

      for (jint i = 0; i != func_info.error_count; ++i) {
        func_info.errors[i] = errors[i];
      }
    } else {
      func_info.errors = nullptr;
    }

    ext_vector.push_back(func_info);

    return ERR(NONE);
  };

  jvmtiError error;

  // Heap extensions.
  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::GetObjectHeapId),
      "com.android.art.heap.get_object_heap_id",
      "Retrieve the heap id of the the object tagged with the given argument. An "
          "arbitrary object is chosen if multiple objects exist with the same tag.",
      {
          { "tag", JVMTI_KIND_IN, JVMTI_TYPE_JLONG, false},
          { "heap_id", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, false}
      },
      { JVMTI_ERROR_NOT_FOUND });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::GetHeapName),
      "com.android.art.heap.get_heap_name",
      "Retrieve the name of the heap with the given id.",
      {
          { "heap_id", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false},
          { "heap_name", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_CCHAR, false}
      },
      { JVMTI_ERROR_ILLEGAL_ARGUMENT });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(HeapExtensions::IterateThroughHeapExt),
      "com.android.art.heap.iterate_through_heap_ext",
      "Iterate through a heap. This is equivalent to the standard IterateThroughHeap function,"
      " except for additionally passing the heap id of the current object. The jvmtiHeapCallbacks"
      " structure is reused, with the callbacks field overloaded to a signature of "
      "jint (*)(jlong, jlong, jlong*, jint length, void*, jint).",
      {
          { "heap_filter", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false},
          { "klass", JVMTI_KIND_IN, JVMTI_TYPE_JCLASS, true},
          { "callbacks", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, false},
          { "user_data", JVMTI_KIND_IN_PTR, JVMTI_TYPE_CVOID, true}
      },
      {
          ERR(MUST_POSSESS_CAPABILITY),
          ERR(INVALID_CLASS),
          ERR(NULL_POINTER),
      });
  if (error != ERR(NONE)) {
    return error;
  }

  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(AllocUtil::GetGlobalJvmtiAllocationState),
      "com.android.art.alloc.get_global_jvmti_allocation_state",
      "Returns the total amount of memory currently allocated by all jvmtiEnvs through the"
      " 'Allocate' jvmti function. This does not include any memory that has been deallocated"
      " through the 'Deallocate' function. This number is approximate and might not correspond"
      " exactly to the sum of the sizes of all not freed allocations.",
      {
          { "currently_allocated", JVMTI_KIND_OUT, JVMTI_TYPE_JLONG, false},
      },
      { ERR(NULL_POINTER) });
  if (error != ERR(NONE)) {
    return error;
  }

  // DDMS extension
  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(DDMSUtil::HandleChunk),
      "com.android.art.internal.ddm.process_chunk",
      "Handles a single ddms chunk request and returns a response. The reply data is in the ddms"
      " chunk format. It returns the processed chunk. This is provided for backwards compatibility"
      " reasons only. Agents should avoid making use of this extension when possible and instead"
      " use the other JVMTI entrypoints explicitly.",
      {
        { "type_in", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false },
        { "length_in", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false },
        { "data_in", JVMTI_KIND_IN_BUF, JVMTI_TYPE_JBYTE, true },
        { "type_out", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, false },
        { "data_len_out", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, false },
        { "data_out", JVMTI_KIND_ALLOC_BUF, JVMTI_TYPE_JBYTE, false }
      },
      { ERR(NULL_POINTER), ERR(ILLEGAL_ARGUMENT), ERR(OUT_OF_MEMORY) });
  if (error != ERR(NONE)) {
    return error;
  }

  // GetClassLoaderClassDescriptors extension
  error = add_extension(
      reinterpret_cast<jvmtiExtensionFunction>(ClassUtil::GetClassLoaderClassDescriptors),
      "com.android.art.class.get_class_loader_class_descriptors",
      "Retrieves a list of all the classes (as class descriptors) that the given class loader is"
      " capable of being the defining class loader for. The return format is a list of"
      " null-terminated descriptor strings of the form \"L/java/lang/Object;\". Each descriptor"
      " will be in the list at most once. If the class_loader is null the bootclassloader will be"
      " used. If the class_loader is not null it must either be a java.lang.BootClassLoader, a"
      " dalvik.system.BaseDexClassLoader or a derived type. The data_out list and all elements"
      " must be deallocated by the caller.",
      {
        { "class_loader", JVMTI_KIND_IN, JVMTI_TYPE_JOBJECT, true },
        { "class_descriptor_count_out", JVMTI_KIND_OUT, JVMTI_TYPE_JINT, false },
        { "data_out", JVMTI_KIND_ALLOC_ALLOC_BUF, JVMTI_TYPE_CCHAR, false },
      },
      {
        ERR(NULL_POINTER),
        ERR(ILLEGAL_ARGUMENT),
        ERR(OUT_OF_MEMORY),
        ERR(NOT_IMPLEMENTED),
      });
  if (error != ERR(NONE)) {
    return error;
  }
  // Copy into output buffer.

  *extension_count_ptr = ext_vector.size();
  JvmtiUniquePtr<jvmtiExtensionFunctionInfo[]> out_data =
      AllocJvmtiUniquePtr<jvmtiExtensionFunctionInfo[]>(env, ext_vector.size(), &error);
  if (out_data == nullptr) {
    return error;
  }
  memcpy(out_data.get(),
          ext_vector.data(),
          ext_vector.size() * sizeof(jvmtiExtensionFunctionInfo));
  *extensions = out_data.release();

  // Release all the buffer holders, we're OK now.
  for (auto& holder : char_buffers) {
    holder.release();
  }
  for (auto& holder : param_buffers) {
    holder.release();
  }
  for (auto& holder : error_buffers) {
    holder.release();
  }

  return OK;
}


jvmtiError ExtensionUtil::GetExtensionEvents(jvmtiEnv* env,
                                             jint* extension_count_ptr,
                                             jvmtiExtensionEventInfo** extensions) {
  std::vector<jvmtiExtensionEventInfo> ext_vector;

  // Holders for allocated values.
  std::vector<JvmtiUniquePtr<char[]>> char_buffers;
  std::vector<JvmtiUniquePtr<jvmtiParamInfo[]>> param_buffers;

  auto add_extension = [&](ArtJvmtiEvent extension_event_index,
                           const char* id,
                           const char* short_description,
                           const std::vector<CParamInfo>& params) {
    DCHECK(IsExtensionEvent(extension_event_index));
    jvmtiExtensionEventInfo event_info;
    jvmtiError error;

    event_info.extension_event_index = static_cast<jint>(extension_event_index);

    JvmtiUniquePtr<char[]> id_ptr = CopyString(env, id, &error);
    if (id_ptr == nullptr) {
      return error;
    }
    event_info.id = id_ptr.get();
    char_buffers.push_back(std::move(id_ptr));

    JvmtiUniquePtr<char[]> descr = CopyString(env, short_description, &error);
    if (descr == nullptr) {
      return error;
    }
    event_info.short_description = descr.get();
    char_buffers.push_back(std::move(descr));

    event_info.param_count = params.size();
    if (!params.empty()) {
      JvmtiUniquePtr<jvmtiParamInfo[]> params_ptr =
          AllocJvmtiUniquePtr<jvmtiParamInfo[]>(env, params.size(), &error);
      if (params_ptr == nullptr) {
        return error;
      }
      event_info.params = params_ptr.get();
      param_buffers.push_back(std::move(params_ptr));

      for (jint i = 0; i != event_info.param_count; ++i) {
        event_info.params[i] = params[i].ToParamInfo(env, &char_buffers, &error);
        if (error != OK) {
          return error;
        }
      }
    } else {
      event_info.params = nullptr;
    }

    ext_vector.push_back(event_info);

    return ERR(NONE);
  };

  jvmtiError error;
  error = add_extension(
      ArtJvmtiEvent::kDdmPublishChunk,
      "com.android.art.internal.ddm.publish_chunk",
      "Called when there is new ddms information that the agent or other clients can use. The"
      " agent is given the 'type' of the ddms chunk and a 'data_size' byte-buffer in 'data'."
      " The 'data' pointer is only valid for the duration of the publish_chunk event. The agent"
      " is responsible for interpreting the information present in the 'data' buffer. This is"
      " provided for backwards-compatibility support only. Agents should prefer to use relevant"
      " JVMTI events and functions above listening for this event.",
      {
        { "jni_env", JVMTI_KIND_IN_PTR, JVMTI_TYPE_JNIENV, false },
        { "type", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false },
        { "data_size", JVMTI_KIND_IN, JVMTI_TYPE_JINT, false },
        { "data",  JVMTI_KIND_IN_BUF, JVMTI_TYPE_JBYTE, false },
      });
  if (error != OK) {
    return error;
  }

  // Copy into output buffer.

  *extension_count_ptr = ext_vector.size();
  JvmtiUniquePtr<jvmtiExtensionEventInfo[]> out_data =
      AllocJvmtiUniquePtr<jvmtiExtensionEventInfo[]>(env, ext_vector.size(), &error);
  if (out_data == nullptr) {
    return error;
  }
  memcpy(out_data.get(),
         ext_vector.data(),
         ext_vector.size() * sizeof(jvmtiExtensionEventInfo));
  *extensions = out_data.release();

  // Release all the buffer holders, we're OK now.
  for (auto& holder : char_buffers) {
    holder.release();
  }
  for (auto& holder : param_buffers) {
    holder.release();
  }

  return OK;
}

jvmtiError ExtensionUtil::SetExtensionEventCallback(jvmtiEnv* env,
                                                    jint extension_event_index,
                                                    jvmtiExtensionEvent callback,
                                                    EventHandler* event_handler) {
  if (!IsExtensionEvent(extension_event_index)) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  ArtJvmTiEnv* art_env = ArtJvmTiEnv::AsArtJvmTiEnv(env);
  jvmtiEventMode mode = callback == nullptr ? JVMTI_DISABLE : JVMTI_ENABLE;
  // Lock the event_info_mutex_ while we set the event to make sure it isn't lost by a concurrent
  // change to the normal callbacks.
  {
    art::WriterMutexLock lk(art::Thread::Current(), art_env->event_info_mutex_);
    if (art_env->event_callbacks.get() == nullptr) {
      art_env->event_callbacks.reset(new ArtJvmtiEventCallbacks());
    }
    jvmtiError err = art_env->event_callbacks->Set(extension_event_index, callback);
    if (err != OK) {
      return err;
    }
  }
  return event_handler->SetEvent(art_env,
                                 /*event_thread*/nullptr,
                                 static_cast<ArtJvmtiEvent>(extension_event_index),
                                 mode);
}

}  // namespace openjdkjvmti
