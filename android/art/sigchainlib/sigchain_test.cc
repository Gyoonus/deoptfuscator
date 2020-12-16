/*
 * Copyright (C) 2018 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <signal.h>
#include <sys/syscall.h>

#include <functional>

#include <gtest/gtest.h>

#include "sigchain.h"

#if !defined(__BIONIC__)
typedef sigset_t sigset64_t;

static int sigemptyset64(sigset64_t* set) {
  return sigemptyset(set);
}

static int sigismember64(sigset64_t* set, int member) {
  return sigismember(set, member);
}
#endif

static int RealSigprocmask(int how, const sigset64_t* new_sigset, sigset64_t* old_sigset) {
  // glibc's sigset_t is overly large, so sizeof(*new_sigset) doesn't work.
  return syscall(__NR_rt_sigprocmask, how, new_sigset, old_sigset, 8);
}

class SigchainTest : public ::testing::Test {
  void SetUp() final {
    art::AddSpecialSignalHandlerFn(SIGSEGV, &action);
  }

  void TearDown() final {
    art::RemoveSpecialSignalHandlerFn(SIGSEGV, action.sc_sigaction);
  }

  art::SigchainAction action = {
      .sc_sigaction = [](int, siginfo_t*, void*) { return true; },
      .sc_mask = {},
      .sc_flags = 0,
  };
};


static void TestSignalBlocking(std::function<void()> fn) {
  // Unblock SIGSEGV, make sure it stays unblocked.
  sigset64_t mask;
  sigemptyset64(&mask);
  ASSERT_EQ(0, RealSigprocmask(SIG_SETMASK, &mask, nullptr)) << strerror(errno);

  fn();

  if (testing::Test::HasFatalFailure()) return;
  ASSERT_EQ(0, RealSigprocmask(SIG_SETMASK, nullptr, &mask));
  ASSERT_FALSE(sigismember64(&mask, SIGSEGV));
}

TEST_F(SigchainTest, sigprocmask_setmask) {
  TestSignalBlocking([]() {
    sigset_t mask;
    sigfillset(&mask);
    ASSERT_EQ(0, sigprocmask(SIG_SETMASK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, sigprocmask_block) {
  TestSignalBlocking([]() {
    sigset_t mask;
    sigfillset(&mask);
    ASSERT_EQ(0, sigprocmask(SIG_BLOCK, &mask, nullptr));
  });
}

// bionic-only wide variants for LP32.
#if defined(__BIONIC__)
TEST_F(SigchainTest, sigprocmask64_setmask) {
  TestSignalBlocking([]() {
    sigset64_t mask;
    sigfillset64(&mask);
    ASSERT_EQ(0, sigprocmask64(SIG_SETMASK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, sigprocmask64_block) {
  TestSignalBlocking([]() {
    sigset64_t mask;
    sigfillset64(&mask);
    ASSERT_EQ(0, sigprocmask64(SIG_BLOCK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, pthread_sigmask64_setmask) {
  TestSignalBlocking([]() {
    sigset64_t mask;
    sigfillset64(&mask);
    ASSERT_EQ(0, pthread_sigmask64(SIG_SETMASK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, pthread_sigmask64_block) {
  TestSignalBlocking([]() {
    sigset64_t mask;
    sigfillset64(&mask);
    ASSERT_EQ(0, pthread_sigmask64(SIG_BLOCK, &mask, nullptr));
  });
}
#endif

// glibc doesn't implement most of these in terms of sigprocmask, which we rely on.
#if defined(__BIONIC__)
TEST_F(SigchainTest, pthread_sigmask_setmask) {
  TestSignalBlocking([]() {
    sigset_t mask;
    sigfillset(&mask);
    ASSERT_EQ(0, pthread_sigmask(SIG_SETMASK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, pthread_sigmask_block) {
  TestSignalBlocking([]() {
    sigset_t mask;
    sigfillset(&mask);
    ASSERT_EQ(0, pthread_sigmask(SIG_BLOCK, &mask, nullptr));
  });
}

TEST_F(SigchainTest, sigset_mask) {
  TestSignalBlocking([]() {
    sigset(SIGSEGV, SIG_HOLD);
  });
}

TEST_F(SigchainTest, sighold) {
  TestSignalBlocking([]() {
    sighold(SIGSEGV);
  });
}

#if defined(__BIONIC__)
// Not exposed via headers, but the symbols are available if you declare them yourself.
extern "C" int sigblock(int);
extern "C" int sigsetmask(int);
#endif

TEST_F(SigchainTest, sigblock) {
  TestSignalBlocking([]() {
    int mask = ~0U;
    ASSERT_EQ(0, sigblock(mask));
  });
}

TEST_F(SigchainTest, sigsetmask) {
  TestSignalBlocking([]() {
    int mask = ~0U;
    ASSERT_EQ(0, sigsetmask(mask));
  });
}

#endif
