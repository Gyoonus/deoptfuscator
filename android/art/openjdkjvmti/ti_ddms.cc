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
#include <vector>

#include "ti_ddms.h"

#include "art_jvmti.h"
#include "base/array_ref.h"
#include "debugger.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"

namespace openjdkjvmti {

jvmtiError DDMSUtil::HandleChunk(jvmtiEnv* env,
                                 jint type_in,
                                 jint length_in,
                                 const jbyte* data_in,
                                 /*out*/jint* type_out,
                                 /*out*/jint* data_length_out,
                                 /*out*/jbyte** data_out) {
  if (env == nullptr || type_out == nullptr || data_out == nullptr || data_length_out == nullptr) {
    return ERR(NULL_POINTER);
  } else if (data_in == nullptr && length_in != 0) {
    // Data-in shouldn't be null if we have data.
    return ERR(ILLEGAL_ARGUMENT);
  }

  *data_length_out = 0;
  *data_out = nullptr;

  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange(self, art::ThreadState::kNative);

  art::ArrayRef<const jbyte> data_arr(data_in, length_in);
  std::vector<uint8_t> out_data;
  if (!art::Dbg::DdmHandleChunk(self->GetJniEnv(),
                                type_in,
                                data_arr,
                                /*out*/reinterpret_cast<uint32_t*>(type_out),
                                /*out*/&out_data)) {
    LOG(WARNING) << "Something went wrong with handling the ddm chunk.";
    return ERR(INTERNAL);
  } else {
    jvmtiError error = OK;
    if (!out_data.empty()) {
      JvmtiUniquePtr<jbyte[]> ret = AllocJvmtiUniquePtr<jbyte[]>(env, out_data.size(), &error);
      if (error != OK) {
        return error;
      }
      memcpy(ret.get(), out_data.data(), out_data.size());
      *data_out = ret.release();
      *data_length_out = static_cast<jint>(out_data.size());
    }
    return OK;
  }
}

}  // namespace openjdkjvmti
