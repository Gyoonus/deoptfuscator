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

#include "runtime.h"

#include <signal.h>

#include <cstring>

#include "runtime_common.h"

namespace art {

struct sigaction old_action;

void HandleUnexpectedSignalAndroid(int signal_number, siginfo_t* info, void* raw_context) {
  HandleUnexpectedSignalCommon(signal_number,
                               info,
                               raw_context,
                               /* handle_timeout_signal */ false,
                               /* dump_on_stderr */ false);

  // Run the old signal handler.
  old_action.sa_sigaction(signal_number, info, raw_context);
}

void Runtime::InitPlatformSignalHandlers() {
  // Enable the signal handler dumping crash information to the logcat
  // when the Android root is not "/system".
  const char* android_root = getenv("ANDROID_ROOT");
  if (android_root != nullptr && strcmp(android_root, "/system") != 0) {
    InitPlatformSignalHandlersCommon(HandleUnexpectedSignalAndroid,
                                     &old_action,
                                     /* handle_timeout_signal */ false);
  }
}

}  // namespace art
