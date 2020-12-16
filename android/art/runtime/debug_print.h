/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEBUG_PRINT_H_
#define ART_RUNTIME_DEBUG_PRINT_H_

#include "base/mutex.h"
#include "mirror/object.h"

// Helper functions for printing extra information for certain hard to diagnose bugs.

namespace art {

std::string DescribeSpace(ObjPtr<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;
std::string DescribeLoaders(ObjPtr<mirror::ClassLoader> loader, const char* class_descriptor)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

void DumpB77342775DebugData(ObjPtr<mirror::Class> target_class, ObjPtr<mirror::Class> src_class)
    REQUIRES_SHARED(Locks::mutator_lock_) COLD_ATTR;

}  // namespace art

#endif  // ART_RUNTIME_DEBUG_PRINT_H_
