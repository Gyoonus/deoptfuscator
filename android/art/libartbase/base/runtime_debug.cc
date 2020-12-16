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

#include "runtime_debug.h"

#include <vector>

#include "globals.h"

namespace art {

// We test here that the runtime-debug-checks are actually a no-op constexpr false in release
// builds, as we can't check that in gtests (which are always debug).

#ifdef NDEBUG
namespace {
DECLARE_RUNTIME_DEBUG_FLAG(kTestForConstexpr);
static_assert(!kTestForConstexpr, "Issue with DECLARE_RUNTIME_DEBUG_FLAG in NDEBUG.");
}
#endif

// Implementation of runtime debug flags. This should be compile-time optimized away in release
// builds.
namespace {
bool gSlowEnabled = false;  // Default for slow flags is "off."

// Use a function with a static to ensure our vector storage doesn't have initialization order
// issues.
std::vector<bool*>& GetFlagPtrs() {
  static std::vector<bool*> g_flag_ptrs;
  return g_flag_ptrs;
}

bool RegisterRuntimeDebugFlagImpl(bool* flag_ptr) {
  GetFlagPtrs().push_back(flag_ptr);
  return gSlowEnabled;
}

void SetRuntimeDebugFlagsEnabledImpl(bool enabled) {
  gSlowEnabled = enabled;
  for (bool* flag_ptr : GetFlagPtrs()) {
    *flag_ptr = enabled;
  }
}

}  // namespace

bool RegisterRuntimeDebugFlag(bool* flag_ptr) {
  if (kIsDebugBuild) {
    return RegisterRuntimeDebugFlagImpl(flag_ptr);
  }
  return false;
}

void SetRuntimeDebugFlagsEnabled(bool enabled) {
  if (kIsDebugBuild) {
    SetRuntimeDebugFlagsEnabledImpl(enabled);
  }
}

}  // namespace art
