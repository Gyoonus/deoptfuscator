/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
#define ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_

#include <inttypes.h>
#include <memory>
#include <vector>

#include "base/array_ref.h"
#include "base/mutex.h"

namespace art {

// Notify native tools (e.g. libunwind) that DEX file has been opened.
// It takes the lock itself. The parameter must point to dex data (not the DexFile* object).
void AddNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile);

// Notify native tools (e.g. libunwind) that DEX file has been closed.
// It takes the lock itself. The parameter must point to dex data (not the DexFile* object).
void RemoveNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile);

// Notify native tools about new JITed code by passing in-memory ELF.
// The handle is the object that is being described (needed to be able to remove the entry).
// The method will make copy of the passed ELF file (to shrink it to the minimum size).
void AddNativeDebugInfoForJit(const void* handle, const std::vector<uint8_t>& symfile)
    REQUIRES(Locks::native_debug_interface_lock_);

// Notify native debugger that JITed code has been removed and free the debug info.
void RemoveNativeDebugInfoForJit(const void* handle)
    REQUIRES(Locks::native_debug_interface_lock_);

// Returns approximate memory used by all JITCodeEntries.
size_t GetJitNativeDebugInfoMemUsage()
    REQUIRES(Locks::native_debug_interface_lock_);

}  // namespace art

#endif  // ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
