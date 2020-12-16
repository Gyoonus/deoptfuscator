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

#ifndef ART_SIGCHAINLIB_SIGCHAIN_H_
#define ART_SIGCHAINLIB_SIGCHAIN_H_

#include <signal.h>
#include <stdint.h>

namespace art {

// Handlers that exit without returning to their caller (e.g. via siglongjmp) must pass this flag.
static constexpr uint64_t SIGCHAIN_ALLOW_NORETURN = 0x1UL;

struct SigchainAction {
  bool (*sc_sigaction)(int, siginfo_t*, void*);
  sigset_t sc_mask;
  uint64_t sc_flags;
};

extern "C" void AddSpecialSignalHandlerFn(int signal, SigchainAction* sa);
extern "C" void RemoveSpecialSignalHandlerFn(int signal, bool (*fn)(int, siginfo_t*, void*));

extern "C" void EnsureFrontOfChain(int signal);

}  // namespace art

#endif  // ART_SIGCHAINLIB_SIGCHAIN_H_
