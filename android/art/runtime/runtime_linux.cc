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

#include <iostream>

#include "base/memory_tool.h"
#include "runtime_common.h"

namespace art {

void HandleUnexpectedSignalLinux(int signal_number, siginfo_t* info, void* raw_context) {
  // Linux is mainly used for host testing. Under those conditions, react to the timeout signal,
  // and dump to stderr to avoid missing output on double-faults.
  HandleUnexpectedSignalCommon(signal_number,
                               info,
                               raw_context,
                               /* handle_timeout_signal */ true,
                               /* dump_on_stderr */ true);

  if (getenv("debug_db_uid") != nullptr || getenv("art_wait_for_gdb_on_crash") != nullptr) {
    pid_t tid = GetTid();
    std::string thread_name(GetThreadName(tid));
    std::cerr << "********************************************************\n"
              << "* Process " << getpid() << " thread " << tid << " \"" << thread_name
              << "\""
              << " has been suspended while crashing.\n"
              << "* Attach gdb:\n"
              << "*     gdb -p " << tid << "\n"
              << "********************************************************"
              << std::endl;
    // Wait for debugger to attach.
    while (true) {
    }
  }
#ifdef __linux__
  // Remove our signal handler for this signal...
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_DFL;
  sigaction(signal_number, &action, nullptr);
  // ...and re-raise so we die with the appropriate status.
  kill(getpid(), signal_number);
#else
  exit(EXIT_FAILURE);
#endif
}

void Runtime::InitPlatformSignalHandlers() {
  constexpr bool kIsASAN =
#ifdef ADDRESS_SANITIZER
      true;
#else
      false;
#endif
  if (!kIsTargetBuild && kIsASAN) {
    // (Temporarily) try and let ASAN print abort stacks, as our code sometimes fails. b/31098551
    return;
  }
  // On the host, we don't have debuggerd to dump a stack for us when something unexpected happens.
  InitPlatformSignalHandlersCommon(HandleUnexpectedSignalLinux,
                                   nullptr,
                                   /* handle_timeout_signal */ true);
}

}  // namespace art
