/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jdwpTransport.h. This implementation
 * is licensed under the same terms as the file jdwpTransport.h.  The
 * copyright and license information for the file jdwpTranport.h follows.
 *
 * Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef ART_DT_FD_FORWARD_DT_FD_FORWARD_H_
#define ART_DT_FD_FORWARD_DT_FD_FORWARD_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <android-base/logging.h>
#include <android-base/thread_annotations.h>
#include <android-base/unique_fd.h>

#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jvmti.h>
#include <jdwpTransport.h>

#include "fd_transport.h"

namespace dt_fd_forward {

static constexpr uint8_t kReplyFlag = 0x80;
// Macro and constexpr to make error values less annoying to write.
#define ERR(e) JDWPTRANSPORT_ERROR_ ## e
static constexpr jdwpTransportError OK = ERR(NONE);

static constexpr const char kJdwpHandshake[14] = {
    'J', 'D', 'W', 'P', '-', 'H', 'a', 'n', 'd', 's', 'h', 'a', 'k', 'e'
};  // "JDWP-Handshake"

enum class TransportState {
  kClosed,       // Main state.
  kListenSetup,  // Transient, wait for the state to change before proceeding.
  kListening,    // Main state.
  kOpening,      // Transient, wait for the state to change before proceeding.
  kOpen,         // Main state.
};

enum class IOResult {
  kOk, kInterrupt, kError, kEOF,
};

class PacketReader;
class PacketWriter;

// TODO It would be good to get the thread-safety analysis checks working but first we would need to
// use something other than std::mutex which does not have the annotations required.
class FdForwardTransport : public jdwpTransportEnv {
 public:
  explicit FdForwardTransport(jdwpTransportCallback* cb);
  ~FdForwardTransport();

  jdwpTransportError PerformAttach(int listen_fd);
  jdwpTransportError SetupListen(int listen_fd);
  jdwpTransportError StopListening();

  jboolean IsOpen();

  jdwpTransportError WritePacket(const jdwpPacket* pkt);
  jdwpTransportError ReadPacket(jdwpPacket* pkt);
  jdwpTransportError Close();
  jdwpTransportError Accept();
  jdwpTransportError GetLastError(/*out*/char** description);

  void* Alloc(size_t data);
  void Free(void* data);

 private:
  void SetLastError(const std::string& desc);

  bool ChangeState(TransportState old_state, TransportState new_state);  // REQUIRES(state_mutex_);

  // Gets the fds from the server side. do_handshake returns whether the transport can skip the
  // jdwp handshake.
  IOResult ReceiveFdsFromSocket(/*out*/bool* do_handshake);

  IOResult WriteFully(const void* data, size_t ndata);  // REQUIRES(!state_mutex_);
  IOResult WriteFullyWithoutChecks(const void* data, size_t ndata);  // REQUIRES(state_mutex_);
  IOResult ReadFully(void* data, size_t ndata);  // REQUIRES(!state_mutex_);
  IOResult ReadUpToMax(void* data, size_t ndata, /*out*/size_t* amount_read);
      // REQUIRES(state_mutex_);
  IOResult ReadFullyWithoutChecks(void* data, size_t ndata);  // REQUIRES(state_mutex_);

  void CloseFdsLocked();  // REQUIRES(state_mutex_)

  // The allocation/deallocation functions.
  jdwpTransportCallback mem_;

  // Input from the server;
  android::base::unique_fd read_fd_;  // GUARDED_BY(state_mutex_);
  // Output to the server;
  android::base::unique_fd write_fd_;  // GUARDED_BY(state_mutex_);

  // an eventfd passed with the write_fd to the transport that we will 'read' from to get a lock on
  // the write_fd_. The other side must not hold it for unbounded time.
  android::base::unique_fd write_lock_fd_;  // GUARDED_BY(state_mutex_);

  // Eventfd we will use to wake-up paused reads for close().
  android::base::unique_fd wakeup_fd_;

  // Socket we will get the read/write fd's from.
  android::base::unique_fd listen_fd_;

  // Fd we will write close notification to. This is a dup of listen_fd_.
  android::base::unique_fd close_notify_fd_;

  TransportState state_;  // GUARDED_BY(state_mutex_);

  std::mutex state_mutex_;
  std::condition_variable state_cv_;

  // A counter that we use to make sure we don't do half a read on one and half on another fd.
  std::atomic<uint64_t> current_seq_num_;

  friend class PacketReader;  // For ReadFullyWithInterrupt
  friend class PacketWriter;  // For WriteFullyWithInterrupt
};

}  // namespace dt_fd_forward

#endif  // ART_DT_FD_FORWARD_DT_FD_FORWARD_H_
