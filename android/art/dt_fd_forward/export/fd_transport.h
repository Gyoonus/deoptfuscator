// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef ART_DT_FD_FORWARD_EXPORT_FD_TRANSPORT_H_
#define ART_DT_FD_FORWARD_EXPORT_FD_TRANSPORT_H_

#include <stdint.h>

namespace dt_fd_forward {

// The file-descriptors sent over a socket to the dt_fd_forward transport.
struct FdSet {
  // A fd that can be read from which provides the JDWP data.
  int read_fd_;

  // A fd that can be written to in order to provide JDWP responses and events.
  int write_fd_;

  // A eventfd that can be locked to ensure that writes to write_fd_ are atomic. This must be held
  // when writing to write_fd_. This allows the proxy to insert packets into the response stream
  // without having to parse it.
  int write_lock_fd_;

  static constexpr size_t kDataLength = sizeof(int) * 3;
  void WriteData(void* buf) {
    int* ibuf = reinterpret_cast<int*>(buf);
    ibuf[0] = read_fd_;
    ibuf[1] = write_fd_;
    ibuf[2] = write_lock_fd_;
  }

  static FdSet ReadData(void* buf) {
    int* ibuf = reinterpret_cast<int*>(buf);
    return FdSet { ibuf[0], ibuf[1], ibuf[2] };
  }
};

// Sent with the file descriptors if the transport should not skip waiting for the handshake.
static constexpr char kPerformHandshakeMessage[] = "HANDSHAKE:REQD";

// Sent with the file descriptors if the transport can skip waiting for the handshake.
static constexpr char kSkipHandshakeMessage[] = "HANDSHAKE:SKIP";

// This message is sent over the fd associated with the transport when we are listening for fds.
static constexpr char kListenStartMessage[] = "dt_fd_forward:START-LISTEN";

// This message is sent over the fd associated with the transport when we stop listening for fds.
static constexpr char kListenEndMessage[] = "dt_fd_forward:END-LISTEN";

// This message is sent over the fd associated with the transport when we have accepted a
// connection. This is sent before any handshaking has occurred. It is simply an acknowledgment
// that the FdSet has been received. This will be paired with a single CLOSING message when these
// fds are closed.
static constexpr char kAcceptMessage[] = "dt_fd_forward:ACCEPTED";

// This message is sent over the fd associated with the transport when we are closing the fds. This
// can be used by the proxy to send additional data on a dup'd fd. The write_lock_fd_ will be held
// until the other two fds are closed and then it will be released and closed.
static constexpr char kCloseMessage[] = "dt_fd_forward:CLOSING";

}  // namespace dt_fd_forward

#endif  // ART_DT_FD_FORWARD_EXPORT_FD_TRANSPORT_H_
