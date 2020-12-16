/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_RUNTIME_DEBUG_H_
#define ART_LIBARTBASE_BASE_RUNTIME_DEBUG_H_

namespace art {

// Runtime debug flags are flags that have a runtime component, that is, their value can be changed.
// This is meant to implement fast vs slow debug builds, in that certain debug flags can be turned
// on and off. To that effect, expose two macros to help implement and globally drive these flags:
//
// In the header, declare a (class) flag like this:
//
//   class C {
//     DECLARE_RUNTIME_DEBUG_FLAG(kFlag);
//   };
//
// This will declare a flag kFlag that is a constexpr false in release builds, and a static field
// in debug builds. Usage is than uniform as C::kFlag.
//
// In the cc file, define the flag like this:
//
//   DEFINE_RUNTIME_DEBUG_FLAG(C, kFlag);
//
// This will define the static storage, as necessary, and register the flag with the runtime
// infrastructure to toggle the value.

#ifdef NDEBUG
#define DECLARE_RUNTIME_DEBUG_FLAG(x) \
  static constexpr bool x = false;
// Note: the static_assert in the following only works for public flags. Fix this when we cross
//       the line at some point.
#define DEFINE_RUNTIME_DEBUG_FLAG(C, x) \
  static_assert(!C::x, "Unexpected enabled flag in release build");
#else
#define DECLARE_RUNTIME_DEBUG_FLAG(x) \
  static bool x;
#define DEFINE_RUNTIME_DEBUG_FLAG(C, x) \
  bool C::x = RegisterRuntimeDebugFlag(&C::x);
#endif  // NDEBUG

bool RegisterRuntimeDebugFlag(bool* runtime_debug_flag);
void SetRuntimeDebugFlagsEnabled(bool enabled);

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_RUNTIME_DEBUG_H_
