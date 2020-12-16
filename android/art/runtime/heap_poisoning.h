/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_HEAP_POISONING_H_
#define ART_RUNTIME_HEAP_POISONING_H_

// This is a C and C++ header used for both assembly code and mainline code.

#ifdef ART_HEAP_POISONING
#define USE_HEAP_POISONING
#endif

#ifdef __cplusplus

namespace art {

// If true, references within the heap are poisoned (negated).
#ifdef USE_HEAP_POISONING
static constexpr bool kPoisonHeapReferences = true;
#else
static constexpr bool kPoisonHeapReferences = false;
#endif

}  // namespace art

#endif  // __cplusplus

#endif  // ART_RUNTIME_HEAP_POISONING_H_
