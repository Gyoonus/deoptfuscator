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

#include "ti_allocator.h"

#if defined(__APPLE__)
// Apple doesn't have malloc.h. Just give this function a non-functional definition.
#define malloc_usable_size(P) 0
#else
#include <malloc.h>
#endif

#include <atomic>

#include "art_jvmti.h"
#include "base/enums.h"

namespace openjdkjvmti {

std::atomic<jlong> AllocUtil::allocated;

jvmtiError AllocUtil::GetGlobalJvmtiAllocationState(jvmtiEnv* env ATTRIBUTE_UNUSED,
                                                    jlong* allocated_ptr) {
  if (allocated_ptr == nullptr) {
    return ERR(NULL_POINTER);
  }
  *allocated_ptr = allocated.load();
  return OK;
}

jvmtiError AllocUtil::Allocate(jvmtiEnv* env ATTRIBUTE_UNUSED,
                               jlong size,
                               unsigned char** mem_ptr) {
  if (size < 0) {
    return ERR(ILLEGAL_ARGUMENT);
  } else if (size == 0) {
    *mem_ptr = nullptr;
    return OK;
  }
  *mem_ptr = AllocateImpl(size);
  if (UNLIKELY(*mem_ptr == nullptr)) {
    return ERR(OUT_OF_MEMORY);
  }
  return OK;
}

unsigned char* AllocUtil::AllocateImpl(jlong size) {
  unsigned char* ret = size != 0 ? reinterpret_cast<unsigned char*>(malloc(size)) : nullptr;
  if (LIKELY(ret != nullptr)) {
    allocated += malloc_usable_size(ret);
  }
  return ret;
}

jvmtiError AllocUtil::Deallocate(jvmtiEnv* env ATTRIBUTE_UNUSED, unsigned char* mem) {
  DeallocateImpl(mem);
  return OK;
}

void AllocUtil::DeallocateImpl(unsigned char* mem) {
  if (mem != nullptr) {
    allocated -= malloc_usable_size(mem);
    free(mem);
  }
}

}  // namespace openjdkjvmti
