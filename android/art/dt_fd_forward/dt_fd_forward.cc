/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jdwpTransport.h. This
 * implementation is licensed under the same terms as the file
 * jdwpTransport.h. The copyright and license information for the file
 * jdwpTransport.h follows.
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

#include "dt_fd_forward.h"

#include <string>
#include <vector>

#include <android-base/endian.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>

#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include <jni.h>
#include <jdwpTransport.h>

namespace dt_fd_forward {

// Helper that puts line-number in error message.
#define DT_IO_ERROR(f) \
    SetLastError(::android::base::StringPrintf("%s:%d - %s: %s", \
                                               __FILE__, __LINE__, f, strerror(errno)))

extern const jdwpTransportNativeInterface_ gTransportInterface;

template <typename T> static T HostToNetwork(T in);
template <typename T> static T NetworkToHost(T in);

template<> int8_t HostToNetwork(int8_t in) { return in; }
template<> int8_t NetworkToHost(int8_t in) { return in; }
template<> int16_t HostToNetwork(int16_t in) { return htons(in); }
template<> int16_t NetworkToHost(int16_t in) { return ntohs(in); }
template<> int32_t HostToNetwork(int32_t in) { return htonl(in); }
template<> int32_t NetworkToHost(int32_t in) { return ntohl(in); }

FdForwardTransport::FdForwardTransport(jdwpTransportCallback* cb)
    : mem_(*cb),
      read_fd_(-1),
      write_fd_(-1),
      wakeup_fd_(eventfd(0, EFD_NONBLOCK)),
      listen_fd_(-1),
      close_notify_fd_(-1),
      state_(TransportState::kClosed),
      current_seq_num_(0) {}

FdForwardTransport::~FdForwardTransport() { }

bool FdForwardTransport::ChangeState(TransportState old_state, TransportState new_state) {
  if (old_state == state_) {
    state_ = new_state;
    state_cv_.notify_all();
    return true;
  } else {
    return false;
  }
}

jdwpTransportError FdForwardTransport::PerformAttach(int listen_fd) {
  jdwpTransportError err = SetupListen(listen_fd);
  if (err != OK) {
    return OK;
  }
  err = Accept();
  StopListening();
  return err;
}

static void SendListenMessage(const android::base::unique_fd& fd) {
  TEMP_FAILURE_RETRY(send(fd, kListenStartMessage, sizeof(kListenStartMessage), MSG_EOR));
}

jdwpTransportError FdForwardTransport::SetupListen(int listen_fd) {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (!ChangeState(TransportState::kClosed, TransportState::kListenSetup)) {
    return ERR(ILLEGAL_STATE);
  } else {
    listen_fd_.reset(dup(listen_fd));
    SendListenMessage(listen_fd_);
    CHECK(ChangeState(TransportState::kListenSetup, TransportState::kListening));
    return OK;
  }
}

static void SendListenEndMessage(const android::base::unique_fd& fd) {
  TEMP_FAILURE_RETRY(send(fd, kListenEndMessage, sizeof(kListenEndMessage), MSG_EOR));
}

jdwpTransportError FdForwardTransport::StopListening() {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (listen_fd_ != -1) {
    SendListenEndMessage(listen_fd_);
  }
  // Don't close the listen_fd_ since we might need it for later calls to listen.
  if (ChangeState(TransportState::kListening, TransportState::kClosed) ||
      state_ == TransportState::kOpen) {
    listen_fd_.reset();
  }
  return OK;
}

// Last error message.
thread_local std::string global_last_error_;

void FdForwardTransport::SetLastError(const std::string& desc) {
  LOG(ERROR) << desc;
  global_last_error_ = desc;
}

IOResult FdForwardTransport::ReadFullyWithoutChecks(void* data, size_t ndata) {
  uint8_t* bdata = reinterpret_cast<uint8_t*>(data);
  size_t nbytes = 0;
  while (nbytes < ndata) {
    int res = TEMP_FAILURE_RETRY(read(read_fd_, bdata + nbytes, ndata - nbytes));
    if (res < 0) {
      DT_IO_ERROR("Failed read()");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

IOResult FdForwardTransport::ReadUpToMax(void* data, size_t ndata, /*out*/size_t* read_amount) {
  CHECK_GE(read_fd_.get(), 0);
  int avail;
  int res = TEMP_FAILURE_RETRY(ioctl(read_fd_, FIONREAD, &avail));
  if (res < 0) {
    DT_IO_ERROR("Failed ioctl(read_fd_, FIONREAD, &avail)");
    return IOResult::kError;
  }
  size_t to_read = std::min(static_cast<size_t>(avail), ndata);
  *read_amount = to_read;
  if (*read_amount == 0) {
    // Check if the read would cause an EOF.
    struct pollfd pollfd = { read_fd_, POLLRDHUP, 0 };
    res = TEMP_FAILURE_RETRY(poll(&pollfd, /*nfds*/1, /*timeout*/0));
    if (res < 0 || (pollfd.revents & POLLERR) == POLLERR) {
      DT_IO_ERROR("Failed poll on read fd.");
      return IOResult::kError;
    }
    return ((pollfd.revents & (POLLRDHUP | POLLHUP)) == 0) ? IOResult::kOk : IOResult::kEOF;
  }

  return ReadFullyWithoutChecks(data, to_read);
}

IOResult FdForwardTransport::ReadFully(void* data, size_t ndata) {
  uint64_t seq_num = current_seq_num_;
  size_t nbytes = 0;
  while (nbytes < ndata) {
    size_t read_len;
    struct pollfd pollfds[2];
    {
      std::lock_guard<std::mutex> lk(state_mutex_);
      // Operations in this block must not cause an unbounded pause.
      if (state_ != TransportState::kOpen || seq_num != current_seq_num_) {
        // Async-close occurred!
        return IOResult::kInterrupt;
      } else {
        CHECK_GE(read_fd_.get(), 0);
      }
      IOResult res = ReadUpToMax(reinterpret_cast<uint8_t*>(data) + nbytes,
                                 ndata - nbytes,
                                 /*out*/&read_len);
      if (res != IOResult::kOk) {
        return res;
      } else {
        nbytes += read_len;
      }

      pollfds[0] = { read_fd_, POLLRDHUP | POLLIN, 0 };
      pollfds[1] = { wakeup_fd_, POLLIN, 0 };
    }
    if (read_len == 0) {
      // No more data. Sleep without locks until more is available. We don't actually check for any
      // errors since possible ones are (1) the read_fd_ is closed or wakeup happens which are both
      // fine since the wakeup_fd_ or the poll failing will wake us up.
      int poll_res = TEMP_FAILURE_RETRY(poll(pollfds, 2, -1));
      if (poll_res < 0) {
        DT_IO_ERROR("Failed to poll!");
      }
      // Clear the wakeup_fd regardless.
      uint64_t val;
      int unused = TEMP_FAILURE_RETRY(read(wakeup_fd_, &val, sizeof(val)));
      DCHECK(unused == sizeof(val) || errno == EAGAIN);
      if (poll_res < 0) {
        return IOResult::kError;
      }
    }
  }
  return IOResult::kOk;
}

// A helper that allows us to lock the eventfd 'fd'.
class ScopedEventFdLock {
 public:
  explicit ScopedEventFdLock(const android::base::unique_fd& fd) : fd_(fd), data_(0) {
    TEMP_FAILURE_RETRY(read(fd_, &data_, sizeof(data_)));
  }

  ~ScopedEventFdLock() {
    TEMP_FAILURE_RETRY(write(fd_, &data_, sizeof(data_)));
  }

 private:
  const android::base::unique_fd& fd_;
  uint64_t data_;
};

IOResult FdForwardTransport::WriteFullyWithoutChecks(const void* data, size_t ndata) {
  ScopedEventFdLock sefdl(write_lock_fd_);
  const uint8_t* bdata = static_cast<const uint8_t*>(data);
  size_t nbytes = 0;
  while (nbytes < ndata) {
    int res = TEMP_FAILURE_RETRY(write(write_fd_, bdata + nbytes, ndata - nbytes));
    if (res < 0) {
      DT_IO_ERROR("Failed write()");
      return IOResult::kError;
    } else if (res == 0) {
      return IOResult::kEOF;
    } else {
      nbytes += res;
    }
  }
  return IOResult::kOk;
}

IOResult FdForwardTransport::WriteFully(const void* data, size_t ndata) {
  std::lock_guard<std::mutex> lk(state_mutex_);
  if (state_ != TransportState::kOpen) {
    return IOResult::kInterrupt;
  }
  return WriteFullyWithoutChecks(data, ndata);
}

static void SendAcceptMessage(int fd) {
  TEMP_FAILURE_RETRY(send(fd, kAcceptMessage, sizeof(kAcceptMessage), MSG_EOR));
}

IOResult FdForwardTransport::ReceiveFdsFromSocket(bool* do_handshake) {
  union {
    cmsghdr cm;
    uint8_t buffer[CMSG_SPACE(sizeof(FdSet))];
  } msg_union;
  // This lets us know if we need to do a handshake or not.
  char message[128];
  iovec iov;
  iov.iov_base = message;
  iov.iov_len  = sizeof(message);

  msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = msg_union.buffer;
  msg.msg_controllen = sizeof(msg_union.buffer);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len   = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;
  memset(reinterpret_cast<int*>(CMSG_DATA(cmsg)), -1, FdSet::kDataLength);

  int res = TEMP_FAILURE_RETRY(recvmsg(listen_fd_, &msg, 0));
  if (res <= 0) {
    DT_IO_ERROR("Failed to receive fds!");
    return IOResult::kError;
  }
  FdSet out_fds = FdSet::ReadData(CMSG_DATA(cmsg));
  bool failed = false;
  if (out_fds.read_fd_ < 0 ||
      out_fds.write_fd_ < 0 ||
      out_fds.write_lock_fd_ < 0) {
    DT_IO_ERROR("Received fds were invalid!");
    failed = true;
  } else if (strcmp(kPerformHandshakeMessage, message) == 0) {
    *do_handshake = true;
  } else if (strcmp(kSkipHandshakeMessage, message) == 0) {
    *do_handshake = false;
  } else {
    DT_IO_ERROR("Unknown message sent with fds.");
    failed = true;
  }

  if (failed) {
    if (out_fds.read_fd_ >= 0) {
      close(out_fds.read_fd_);
    }
    if (out_fds.write_fd_ >= 0) {
      close(out_fds.write_fd_);
    }
    if (out_fds.write_lock_fd_ >= 0) {
      close(out_fds.write_lock_fd_);
    }
    return IOResult::kError;
  }

  read_fd_.reset(out_fds.read_fd_);
  write_fd_.reset(out_fds.write_fd_);
  write_lock_fd_.reset(out_fds.write_lock_fd_);

  // We got the fds. Send ack.
  close_notify_fd_.reset(dup(listen_fd_));
  SendAcceptMessage(close_notify_fd_);

  return IOResult::kOk;
}

// Accept the connection. Note that we match the behavior of other transports which is to just close
// the connection and try again if we get a bad handshake.
jdwpTransportError FdForwardTransport::Accept() {
  // TODO Work with timeouts.
  while (true) {
    std::unique_lock<std::mutex> lk(state_mutex_);
    while (!ChangeState(TransportState::kListening, TransportState::kOpening)) {
      if (state_ == TransportState::kClosed ||
          state_ == TransportState::kOpen) {
        return ERR(ILLEGAL_STATE);
      }
      state_cv_.wait(lk);
    }

    bool do_handshake = false;
    DCHECK_NE(listen_fd_.get(), -1);
    if (ReceiveFdsFromSocket(&do_handshake) != IOResult::kOk) {
      CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
      return ERR(IO_ERROR);
    }

    current_seq_num_++;

    // Moved to the opening state.
    if (do_handshake) {
      // Perform the handshake
      char handshake_recv[sizeof(kJdwpHandshake)];
      memset(handshake_recv, 0, sizeof(handshake_recv));
      IOResult res = ReadFullyWithoutChecks(handshake_recv, sizeof(handshake_recv));
      if (res != IOResult::kOk ||
          strncmp(handshake_recv, kJdwpHandshake, sizeof(kJdwpHandshake)) != 0) {
        DT_IO_ERROR("Failed to read handshake");
        CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
        CloseFdsLocked();
        // Retry.
        continue;
      }
      res = WriteFullyWithoutChecks(kJdwpHandshake, sizeof(kJdwpHandshake));
      if (res != IOResult::kOk) {
        DT_IO_ERROR("Failed to write handshake");
        CHECK(ChangeState(TransportState::kOpening, TransportState::kListening));
        CloseFdsLocked();
        // Retry.
        continue;
      }
    }
    break;
  }
  CHECK(ChangeState(TransportState::kOpening, TransportState::kOpen));
  return OK;
}

void SendClosingMessage(int fd) {
  if (fd >= 0) {
    TEMP_FAILURE_RETRY(send(fd, kCloseMessage, sizeof(kCloseMessage), MSG_EOR));
  }
}

// Actually close the fds associated with this transport.
void FdForwardTransport::CloseFdsLocked() {
  // We have a different set of fd's now. Increase the seq number.
  current_seq_num_++;

  // All access to these is locked under the state_mutex_ so we are safe to close these.
  {
    ScopedEventFdLock sefdl(write_lock_fd_);
    if (close_notify_fd_ >= 0) {
      SendClosingMessage(close_notify_fd_);
    }
    close_notify_fd_.reset();
    read_fd_.reset();
    write_fd_.reset();
    close_notify_fd_.reset();
  }
  write_lock_fd_.reset();

  // Send a wakeup in case we have any in-progress reads/writes.
  uint64_t data = 1;
  TEMP_FAILURE_RETRY(write(wakeup_fd_, &data, sizeof(data)));
}

jdwpTransportError FdForwardTransport::Close() {
  std::lock_guard<std::mutex> lk(state_mutex_);
  jdwpTransportError res =
      ChangeState(TransportState::kOpen, TransportState::kClosed) ? OK : ERR(ILLEGAL_STATE);
  // Send a wakeup after changing the state even if nothing actually happened.
  uint64_t data = 1;
  TEMP_FAILURE_RETRY(write(wakeup_fd_, &data, sizeof(data)));
  if (res == OK) {
    CloseFdsLocked();
  }
  return res;
}

// A helper class to read and parse the JDWP packet.
class PacketReader {
 public:
  PacketReader(FdForwardTransport* transport, jdwpPacket* pkt)
      : transport_(transport),
        pkt_(pkt),
        is_eof_(false),
        is_err_(false) {}
  bool ReadFully() {
    // Zero out.
    memset(pkt_, 0, sizeof(jdwpPacket));
    int32_t len = ReadInt32();         // read len
    if (is_err_) {
      return false;
    } else if (is_eof_) {
      return true;
    } else if (len < 11) {
      transport_->DT_IO_ERROR("Packet with len < 11 received!");
      return false;
    }
    pkt_->type.cmd.len = len;
    pkt_->type.cmd.id = ReadInt32();
    pkt_->type.cmd.flags = ReadByte();
    if (is_err_) {
      return false;
    } else if (is_eof_) {
      return true;
    } else if ((pkt_->type.reply.flags & JDWPTRANSPORT_FLAGS_REPLY) == JDWPTRANSPORT_FLAGS_REPLY) {
      ReadReplyPacket();
    } else {
      ReadCmdPacket();
    }
    return !is_err_;
  }

 private:
  void ReadReplyPacket() {
    pkt_->type.reply.errorCode = ReadInt16();
    pkt_->type.reply.data = ReadRemaining();
  }

  void ReadCmdPacket() {
    pkt_->type.cmd.cmdSet = ReadByte();
    pkt_->type.cmd.cmd = ReadByte();
    pkt_->type.cmd.data = ReadRemaining();
  }

  template <typename T>
  T HandleResult(IOResult res, T val, T fail) {
    switch (res) {
      case IOResult::kError:
        is_err_ = true;
        return fail;
      case IOResult::kOk:
        return val;
      case IOResult::kEOF:
        is_eof_ = true;
        pkt_->type.cmd.len = 0;
        return fail;
      case IOResult::kInterrupt:
        transport_->DT_IO_ERROR("Failed to read, concurrent close!");
        is_err_ = true;
        return fail;
    }
  }

  jbyte* ReadRemaining() {
    if (is_eof_ || is_err_) {
      return nullptr;
    }
    jbyte* out = nullptr;
    jint rem = pkt_->type.cmd.len - 11;
    CHECK_GE(rem, 0);
    if (rem == 0) {
      return nullptr;
    } else {
      out = reinterpret_cast<jbyte*>(transport_->Alloc(rem));
      IOResult res = transport_->ReadFully(out, rem);
      jbyte* ret = HandleResult(res, out, static_cast<jbyte*>(nullptr));
      if (ret != out) {
        transport_->Free(out);
      }
      return ret;
    }
  }

  jbyte ReadByte() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jbyte out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
    return HandleResult(res, NetworkToHost(out), static_cast<jbyte>(-1));
  }

  jshort ReadInt16() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jshort out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
    return HandleResult(res, NetworkToHost(out), static_cast<jshort>(-1));
  }

  jint ReadInt32() {
    if (is_eof_ || is_err_) {
      return -1;
    }
    jint out;
    IOResult res = transport_->ReadFully(&out, sizeof(out));
    return HandleResult(res, NetworkToHost(out), -1);
  }

  FdForwardTransport* transport_;
  jdwpPacket* pkt_;
  bool is_eof_;
  bool is_err_;
};

jdwpTransportError FdForwardTransport::ReadPacket(jdwpPacket* pkt) {
  if (pkt == nullptr) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  PacketReader reader(this, pkt);
  if (reader.ReadFully()) {
    return OK;
  } else {
    return ERR(IO_ERROR);
  }
}

// A class that writes a packet to the transport.
class PacketWriter {
 public:
  PacketWriter(FdForwardTransport* transport, const jdwpPacket* pkt)
      : transport_(transport), pkt_(pkt), data_() {}

  bool WriteFully() {
    PushInt32(pkt_->type.cmd.len);
    PushInt32(pkt_->type.cmd.id);
    PushByte(pkt_->type.cmd.flags);
    if ((pkt_->type.reply.flags & JDWPTRANSPORT_FLAGS_REPLY) == JDWPTRANSPORT_FLAGS_REPLY) {
      PushInt16(pkt_->type.reply.errorCode);
      PushData(pkt_->type.reply.data, pkt_->type.reply.len - 11);
    } else {
      PushByte(pkt_->type.cmd.cmdSet);
      PushByte(pkt_->type.cmd.cmd);
      PushData(pkt_->type.cmd.data, pkt_->type.cmd.len - 11);
    }
    IOResult res = transport_->WriteFully(data_.data(), data_.size());
    return res == IOResult::kOk;
  }

 private:
  void PushInt32(int32_t data) {
    data = HostToNetwork(data);
    PushData(&data, sizeof(data));
  }
  void PushInt16(int16_t data) {
    data = HostToNetwork(data);
    PushData(&data, sizeof(data));
  }
  void PushByte(jbyte data) {
    data_.push_back(HostToNetwork(data));
  }

  void PushData(void* d, size_t size) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(d);
    data_.insert(data_.end(), bytes, bytes + size);
  }

  FdForwardTransport* transport_;
  const jdwpPacket* pkt_;
  std::vector<uint8_t> data_;
};

jdwpTransportError FdForwardTransport::WritePacket(const jdwpPacket* pkt) {
  if (pkt == nullptr) {
    return ERR(ILLEGAL_ARGUMENT);
  }
  PacketWriter writer(this, pkt);
  if (writer.WriteFully()) {
    return OK;
  } else {
    return ERR(IO_ERROR);
  }
}

jboolean FdForwardTransport::IsOpen() {
  return state_ == TransportState::kOpen;
}

void* FdForwardTransport::Alloc(size_t s) {
  return mem_.alloc(s);
}

void FdForwardTransport::Free(void* data) {
  mem_.free(data);
}

jdwpTransportError FdForwardTransport::GetLastError(/*out*/char** err) {
  std::string data = global_last_error_;
  *err = reinterpret_cast<char*>(Alloc(data.size() + 1));
  strcpy(*err, data.c_str());
  return OK;
}

static FdForwardTransport* AsFdForward(jdwpTransportEnv* env) {
  return reinterpret_cast<FdForwardTransport*>(env);
}

static jdwpTransportError ParseAddress(const std::string& addr,
                                       /*out*/int* listen_sock) {
  if (!android::base::ParseInt(addr.c_str(), listen_sock) || *listen_sock < 0) {
    LOG(ERROR) << "address format is <fd_num> not " << addr;
    return ERR(ILLEGAL_ARGUMENT);
  }
  return OK;
}

class JdwpTransportFunctions {
 public:
  static jdwpTransportError GetCapabilities(jdwpTransportEnv* env ATTRIBUTE_UNUSED,
                                            /*out*/ JDWPTransportCapabilities* capabilities_ptr) {
    // We don't support any of the optional capabilities (can_timeout_attach, can_timeout_accept,
    // can_timeout_handshake) so just return a zeroed capabilities ptr.
    // TODO We should maybe support these timeout options.
    memset(capabilities_ptr, 0, sizeof(JDWPTransportCapabilities));
    return OK;
  }

  // Address is <sock_fd>
  static jdwpTransportError Attach(jdwpTransportEnv* env,
                                   const char* address,
                                   jlong attach_timeout ATTRIBUTE_UNUSED,
                                   jlong handshake_timeout ATTRIBUTE_UNUSED) {
    if (address == nullptr || *address == '\0') {
      return ERR(ILLEGAL_ARGUMENT);
    }
    int listen_fd;
    jdwpTransportError err = ParseAddress(address, &listen_fd);
    if (err != OK) {
      return err;
    }
    return AsFdForward(env)->PerformAttach(listen_fd);
  }

  static jdwpTransportError StartListening(jdwpTransportEnv* env,
                                           const char* address,
                                           /*out*/ char** actual_address) {
    if (address == nullptr || *address == '\0') {
      return ERR(ILLEGAL_ARGUMENT);
    }
    int listen_fd;
    jdwpTransportError err = ParseAddress(address, &listen_fd);
    if (err != OK) {
      return err;
    }
    err = AsFdForward(env)->SetupListen(listen_fd);
    if (err != OK) {
      return err;
    }
    if (actual_address != nullptr) {
      *actual_address = reinterpret_cast<char*>(AsFdForward(env)->Alloc(strlen(address) + 1));
      memcpy(*actual_address, address, strlen(address) + 1);
    }
    return OK;
  }

  static jdwpTransportError StopListening(jdwpTransportEnv* env) {
    return AsFdForward(env)->StopListening();
  }

  static jdwpTransportError Accept(jdwpTransportEnv* env,
                                   jlong accept_timeout ATTRIBUTE_UNUSED,
                                   jlong handshake_timeout ATTRIBUTE_UNUSED) {
    return AsFdForward(env)->Accept();
  }

  static jboolean IsOpen(jdwpTransportEnv* env) {
    return AsFdForward(env)->IsOpen();
  }

  static jdwpTransportError Close(jdwpTransportEnv* env) {
    return AsFdForward(env)->Close();
  }

  static jdwpTransportError ReadPacket(jdwpTransportEnv* env, jdwpPacket *pkt) {
    return AsFdForward(env)->ReadPacket(pkt);
  }

  static jdwpTransportError WritePacket(jdwpTransportEnv* env, const jdwpPacket* pkt) {
    return AsFdForward(env)->WritePacket(pkt);
  }

  static jdwpTransportError GetLastError(jdwpTransportEnv* env, char** error) {
    return AsFdForward(env)->GetLastError(error);
  }
};

// The actual struct holding all the entrypoints into the jdwpTransport interface.
const jdwpTransportNativeInterface_ gTransportInterface = {
  nullptr,  // reserved1
  JdwpTransportFunctions::GetCapabilities,
  JdwpTransportFunctions::Attach,
  JdwpTransportFunctions::StartListening,
  JdwpTransportFunctions::StopListening,
  JdwpTransportFunctions::Accept,
  JdwpTransportFunctions::IsOpen,
  JdwpTransportFunctions::Close,
  JdwpTransportFunctions::ReadPacket,
  JdwpTransportFunctions::WritePacket,
  JdwpTransportFunctions::GetLastError,
};

extern "C"
JNIEXPORT jint JNICALL jdwpTransport_OnLoad(JavaVM* vm ATTRIBUTE_UNUSED,
                                            jdwpTransportCallback* cb,
                                            jint version,
                                            jdwpTransportEnv** /*out*/env) {
  if (version != JDWPTRANSPORT_VERSION_1_0) {
    LOG(ERROR) << "unknown version " << version;
    return JNI_EVERSION;
  }
  void* data = cb->alloc(sizeof(FdForwardTransport));
  if (data == nullptr) {
    LOG(ERROR) << "Failed to allocate data for transport!";
    return JNI_ENOMEM;
  }
  FdForwardTransport* transport =
      new (data) FdForwardTransport(cb);
  transport->functions = &gTransportInterface;
  *env = transport;
  return JNI_OK;
}

}  // namespace dt_fd_forward
