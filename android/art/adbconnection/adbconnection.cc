/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <array>

#include "adbconnection.h"

#include "android-base/endian.h"
#include "android-base/stringprintf.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "java_vm_ext.h"
#include "jni_env_ext.h"
#include "mirror/throwable.h"
#include "nativehelper/ScopedLocalRef.h"
#include "runtime-inl.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "well_known_classes.h"

#include "jdwp/jdwp_priv.h"

#include "fd_transport.h"

#include "poll.h"

#ifdef ART_TARGET_ANDROID
#include "cutils/sockets.h"
#endif

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>
#include <jni.h>

namespace adbconnection {

// Messages sent from the transport
using dt_fd_forward::kListenStartMessage;
using dt_fd_forward::kListenEndMessage;
using dt_fd_forward::kAcceptMessage;
using dt_fd_forward::kCloseMessage;

// Messages sent to the transport
using dt_fd_forward::kPerformHandshakeMessage;
using dt_fd_forward::kSkipHandshakeMessage;

using android::base::StringPrintf;

static constexpr const char kJdwpHandshake[14] = {
  'J', 'D', 'W', 'P', '-', 'H', 'a', 'n', 'd', 's', 'h', 'a', 'k', 'e'
};

static constexpr int kEventfdLocked = 0;
static constexpr int kEventfdUnlocked = 1;
static constexpr int kControlSockSendTimeout = 10;

static constexpr size_t kPacketHeaderLen = 11;
static constexpr off_t kPacketSizeOff = 0;
static constexpr off_t kPacketIdOff = 4;
static constexpr off_t kPacketCommandSetOff = 9;
static constexpr off_t kPacketCommandOff = 10;

static constexpr uint8_t kDdmCommandSet = 199;
static constexpr uint8_t kDdmChunkCommand = 1;

static AdbConnectionState* gState;

static bool IsDebuggingPossible() {
  return art::Dbg::IsJdwpAllowed();
}

// Begin running the debugger.
void AdbConnectionDebuggerController::StartDebugger() {
  if (IsDebuggingPossible()) {
    connection_->StartDebuggerThreads();
  } else {
    LOG(ERROR) << "Not starting debugger since process cannot load the jdwp agent.";
  }
}

// The debugger should begin shutting down since the runtime is ending. We don't actually do
// anything here. The real shutdown has already happened as far as the agent is concerned.
void AdbConnectionDebuggerController::StopDebugger() { }

bool AdbConnectionDebuggerController::IsDebuggerConfigured() {
  return IsDebuggingPossible() && !art::Runtime::Current()->GetJdwpOptions().empty();
}

void AdbConnectionDdmCallback::DdmPublishChunk(uint32_t type,
                                               const art::ArrayRef<const uint8_t>& data) {
  connection_->PublishDdmData(type, data);
}

class ScopedEventFdLock {
 public:
  explicit ScopedEventFdLock(int fd) : fd_(fd), data_(0) {
    TEMP_FAILURE_RETRY(read(fd_, &data_, sizeof(data_)));
  }

  ~ScopedEventFdLock() {
    TEMP_FAILURE_RETRY(write(fd_, &data_, sizeof(data_)));
  }

 private:
  int fd_;
  uint64_t data_;
};

AdbConnectionState::AdbConnectionState(const std::string& agent_name)
  : agent_name_(agent_name),
    controller_(this),
    ddm_callback_(this),
    sleep_event_fd_(-1),
    control_sock_(-1),
    local_agent_control_sock_(-1),
    remote_agent_control_sock_(-1),
    adb_connection_socket_(-1),
    adb_write_event_fd_(-1),
    shutting_down_(false),
    agent_loaded_(false),
    agent_listening_(false),
    agent_has_socket_(false),
    sent_agent_fds_(false),
    performed_handshake_(false),
    notified_ddm_active_(false),
    next_ddm_id_(1),
    started_debugger_threads_(false) {
  // Setup the addr.
  control_addr_.controlAddrUn.sun_family = AF_UNIX;
  control_addr_len_ = sizeof(control_addr_.controlAddrUn.sun_family) + sizeof(kJdwpControlName) - 1;
  memcpy(control_addr_.controlAddrUn.sun_path, kJdwpControlName, sizeof(kJdwpControlName) - 1);

  // Add the startup callback.
  art::ScopedObjectAccess soa(art::Thread::Current());
  art::Runtime::Current()->GetRuntimeCallbacks()->AddDebuggerControlCallback(&controller_);
}

static jobject CreateAdbConnectionThread(art::Thread* thr) {
  JNIEnv* env = thr->GetJniEnv();
  // Move to native state to talk with the jnienv api.
  art::ScopedThreadStateChange stsc(thr, art::kNative);
  ScopedLocalRef<jstring> thr_name(env, env->NewStringUTF(kAdbConnectionThreadName));
  ScopedLocalRef<jobject> thr_group(
      env,
      env->GetStaticObjectField(art::WellKnownClasses::java_lang_ThreadGroup,
                                art::WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup));
  return env->NewObject(art::WellKnownClasses::java_lang_Thread,
                        art::WellKnownClasses::java_lang_Thread_init,
                        thr_group.get(),
                        thr_name.get(),
                        /*Priority*/ 0,
                        /*Daemon*/ true);
}

struct CallbackData {
  AdbConnectionState* this_;
  jobject thr_;
};

static void* CallbackFunction(void* vdata) {
  std::unique_ptr<CallbackData> data(reinterpret_cast<CallbackData*>(vdata));
  CHECK(data->this_ == gState);
  art::Thread* self = art::Thread::Attach(kAdbConnectionThreadName,
                                          true,
                                          data->thr_);
  CHECK(self != nullptr) << "threads_being_born_ should have ensured thread could be attached.";
  // The name in Attach() is only for logging. Set the thread name. This is important so
  // that the thread is no longer seen as starting up.
  {
    art::ScopedObjectAccess soa(self);
    self->SetThreadName(kAdbConnectionThreadName);
  }

  // Release the peer.
  JNIEnv* env = self->GetJniEnv();
  env->DeleteGlobalRef(data->thr_);
  data->thr_ = nullptr;
  {
    // The StartThreadBirth was called in the parent thread. We let the runtime know we are up
    // before going into the provided code.
    art::MutexLock mu(self, *art::Locks::runtime_shutdown_lock_);
    art::Runtime::Current()->EndThreadBirth();
  }
  data->this_->RunPollLoop(self);
  int detach_result = art::Runtime::Current()->GetJavaVM()->DetachCurrentThread();
  CHECK_EQ(detach_result, 0);

  // Get rid of the connection
  gState = nullptr;
  delete data->this_;

  return nullptr;
}

void AdbConnectionState::StartDebuggerThreads() {
  // First do all the final setup we need.
  CHECK_EQ(adb_write_event_fd_.get(), -1);
  CHECK_EQ(sleep_event_fd_.get(), -1);
  CHECK_EQ(local_agent_control_sock_.get(), -1);
  CHECK_EQ(remote_agent_control_sock_.get(), -1);

  sleep_event_fd_.reset(eventfd(kEventfdLocked, EFD_CLOEXEC));
  CHECK_NE(sleep_event_fd_.get(), -1) << "Unable to create wakeup eventfd.";
  adb_write_event_fd_.reset(eventfd(kEventfdUnlocked, EFD_CLOEXEC));
  CHECK_NE(adb_write_event_fd_.get(), -1) << "Unable to create write-lock eventfd.";

  {
    art::ScopedObjectAccess soa(art::Thread::Current());
    art::Runtime::Current()->GetRuntimeCallbacks()->AddDdmCallback(&ddm_callback_);
  }
  // Setup the socketpair we use to talk to the agent.
  bool has_sockets;
  do {
    has_sockets = android::base::Socketpair(AF_UNIX,
                                            SOCK_SEQPACKET | SOCK_CLOEXEC,
                                            0,
                                            &local_agent_control_sock_,
                                            &remote_agent_control_sock_);
  } while (!has_sockets && errno == EINTR);
  if (!has_sockets) {
    PLOG(FATAL) << "Unable to create socketpair for agent control!";
  }

  // Next start the threads.
  art::Thread* self = art::Thread::Current();
  art::ScopedObjectAccess soa(self);
  {
    art::Runtime* runtime = art::Runtime::Current();
    art::MutexLock mu(self, *art::Locks::runtime_shutdown_lock_);
    if (runtime->IsShuttingDownLocked()) {
      // The runtime is shutting down so we cannot create new threads. This shouldn't really happen.
      LOG(ERROR) << "The runtime is shutting down when we are trying to start up the debugger!";
      return;
    }
    runtime->StartThreadBirth();
  }
  ScopedLocalRef<jobject> thr(soa.Env(), CreateAdbConnectionThread(soa.Self()));
  pthread_t pthread;
  std::unique_ptr<CallbackData> data(new CallbackData { this, soa.Env()->NewGlobalRef(thr.get()) });
  started_debugger_threads_ = true;
  int pthread_create_result = pthread_create(&pthread,
                                             nullptr,
                                             &CallbackFunction,
                                             data.get());
  if (pthread_create_result != 0) {
    started_debugger_threads_ = false;
    // If the create succeeded the other thread will call EndThreadBirth.
    art::Runtime* runtime = art::Runtime::Current();
    soa.Env()->DeleteGlobalRef(data->thr_);
    LOG(ERROR) << "Failed to create thread for adb-jdwp connection manager!";
    art::MutexLock mu(art::Thread::Current(), *art::Locks::runtime_shutdown_lock_);
    runtime->EndThreadBirth();
    return;
  }
  data.release();
}

static bool FlagsSet(int16_t data, int16_t flags) {
  return (data & flags) == flags;
}

void AdbConnectionState::CloseFds() {
  {
    // Lock the write_event_fd so that concurrent PublishDdms will see that the connection is
    // closed.
    ScopedEventFdLock lk(adb_write_event_fd_);
    // shutdown(adb_connection_socket_, SHUT_RDWR);
    adb_connection_socket_.reset();
  }

  // If we didn't load anything we will need to do the handshake again.
  performed_handshake_ = false;

  // If the agent isn't loaded we might need to tell ddms code the connection is closed.
  if (!agent_loaded_ && notified_ddm_active_) {
    NotifyDdms(/*active*/false);
  }
}

void AdbConnectionState::NotifyDdms(bool active) {
  art::ScopedObjectAccess soa(art::Thread::Current());
  DCHECK_NE(notified_ddm_active_, active);
  notified_ddm_active_ = active;
  if (active) {
    art::Dbg::DdmConnected();
  } else {
    art::Dbg::DdmDisconnected();
  }
}

uint32_t AdbConnectionState::NextDdmId() {
  // Just have a normal counter but always set the sign bit.
  return (next_ddm_id_++) | 0x80000000;
}

void AdbConnectionState::PublishDdmData(uint32_t type, const art::ArrayRef<const uint8_t>& data) {
  SendDdmPacket(NextDdmId(), DdmPacketType::kCmd, type, data);
}

void AdbConnectionState::SendDdmPacket(uint32_t id,
                                       DdmPacketType packet_type,
                                       uint32_t type,
                                       art::ArrayRef<const uint8_t> data) {
  // Get the write_event early to fail fast.
  ScopedEventFdLock lk(adb_write_event_fd_);
  if (adb_connection_socket_ == -1) {
    VLOG(jdwp) << "Not sending ddms data of type "
               << StringPrintf("%c%c%c%c",
                               static_cast<char>(type >> 24),
                               static_cast<char>(type >> 16),
                               static_cast<char>(type >> 8),
                               static_cast<char>(type)) << " due to no connection!";
    // Adb is not connected.
    return;
  }

  // the adb_write_event_fd_ will ensure that the adb_connection_socket_ will not go away until
  // after we have sent our data.
  static constexpr uint32_t kDdmPacketHeaderSize =
      kJDWPHeaderLen       // jdwp command packet size
      + sizeof(uint32_t)   // Type
      + sizeof(uint32_t);  // length
  alignas(sizeof(uint32_t)) std::array<uint8_t, kDdmPacketHeaderSize> pkt;
  uint8_t* pkt_data = pkt.data();

  // Write the length first.
  *reinterpret_cast<uint32_t*>(pkt_data) = htonl(kDdmPacketHeaderSize + data.size());
  pkt_data += sizeof(uint32_t);

  // Write the id next;
  *reinterpret_cast<uint32_t*>(pkt_data) = htonl(id);
  pkt_data += sizeof(uint32_t);

  // next the flags. (0 for cmd packet because DDMS).
  *(pkt_data++) = static_cast<uint8_t>(packet_type);
  switch (packet_type) {
    case DdmPacketType::kCmd: {
      // Now the cmd-set
      *(pkt_data++) = kJDWPDdmCmdSet;
      // Now the command
      *(pkt_data++) = kJDWPDdmCmd;
      break;
    }
    case DdmPacketType::kReply: {
      // This is the error code bytes which are all 0
      *(pkt_data++) = 0;
      *(pkt_data++) = 0;
    }
  }

  // These are at unaligned addresses so we need to do them manually.
  // now the type.
  uint32_t net_type = htonl(type);
  memcpy(pkt_data, &net_type, sizeof(net_type));
  pkt_data += sizeof(uint32_t);

  // Now the data.size()
  uint32_t net_len = htonl(data.size());
  memcpy(pkt_data, &net_len, sizeof(net_len));
  pkt_data += sizeof(uint32_t);

  static uint32_t constexpr kIovSize = 2;
  struct iovec iovs[kIovSize] = {
    { pkt.data(), pkt.size() },
    { const_cast<uint8_t*>(data.data()), data.size() },
  };
  // now pkt_header has the header.
  // use writev to send the actual data.
  ssize_t res = TEMP_FAILURE_RETRY(writev(adb_connection_socket_, iovs, kIovSize));
  if (static_cast<size_t>(res) != (kDdmPacketHeaderSize + data.size())) {
    PLOG(ERROR) << StringPrintf("Failed to send DDMS packet %c%c%c%c to debugger (%zd of %zu)",
                                static_cast<char>(type >> 24),
                                static_cast<char>(type >> 16),
                                static_cast<char>(type >> 8),
                                static_cast<char>(type),
                                res, data.size() + kDdmPacketHeaderSize);
  } else {
    VLOG(jdwp) << StringPrintf("sent DDMS packet %c%c%c%c to debugger %zu",
                               static_cast<char>(type >> 24),
                               static_cast<char>(type >> 16),
                               static_cast<char>(type >> 8),
                               static_cast<char>(type),
                               data.size() + kDdmPacketHeaderSize);
  }
}

void AdbConnectionState::SendAgentFds(bool require_handshake) {
  DCHECK(!sent_agent_fds_);
  const char* message = require_handshake ? kPerformHandshakeMessage : kSkipHandshakeMessage;
  union {
    cmsghdr cm;
    char buffer[CMSG_SPACE(dt_fd_forward::FdSet::kDataLength)];
  } cm_un;
  iovec iov;
  iov.iov_base       = const_cast<char*>(message);
  iov.iov_len        = strlen(message) + 1;

  msghdr msg;
  msg.msg_name       = nullptr;
  msg.msg_namelen    = 0;
  msg.msg_iov        = &iov;
  msg.msg_iovlen     = 1;
  msg.msg_flags      = 0;
  msg.msg_control    = cm_un.buffer;
  msg.msg_controllen = sizeof(cm_un.buffer);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len   = CMSG_LEN(dt_fd_forward::FdSet::kDataLength);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;

  // Duplicate the fds before sending them.
  android::base::unique_fd read_fd(dup(adb_connection_socket_));
  CHECK_NE(read_fd.get(), -1) << "Failed to dup read_fd_: " << strerror(errno);
  android::base::unique_fd write_fd(dup(adb_connection_socket_));
  CHECK_NE(write_fd.get(), -1) << "Failed to dup write_fd: " << strerror(errno);
  android::base::unique_fd write_lock_fd(dup(adb_write_event_fd_));
  CHECK_NE(write_lock_fd.get(), -1) << "Failed to dup write_lock_fd: " << strerror(errno);

  dt_fd_forward::FdSet {
    read_fd.get(), write_fd.get(), write_lock_fd.get()
  }.WriteData(CMSG_DATA(cmsg));

  int res = TEMP_FAILURE_RETRY(sendmsg(local_agent_control_sock_, &msg, MSG_EOR));
  if (res < 0) {
    PLOG(ERROR) << "Failed to send agent adb connection fds.";
  } else {
    sent_agent_fds_ = true;
    VLOG(jdwp) << "Fds have been sent to jdwp agent!";
  }
}

android::base::unique_fd AdbConnectionState::ReadFdFromAdb() {
  // We don't actually care about the data that is sent. We do need to receive something though.
  char dummy = '!';
  union {
    cmsghdr cm;
    char buffer[CMSG_SPACE(sizeof(int))];
  } cm_un;

  iovec iov;
  iov.iov_base       = &dummy;
  iov.iov_len        = 1;

  msghdr msg;
  msg.msg_name       = nullptr;
  msg.msg_namelen    = 0;
  msg.msg_iov        = &iov;
  msg.msg_iovlen     = 1;
  msg.msg_flags      = 0;
  msg.msg_control    = cm_un.buffer;
  msg.msg_controllen = sizeof(cm_un.buffer);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len   = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;
  (reinterpret_cast<int*>(CMSG_DATA(cmsg)))[0] = -1;

  int rc = TEMP_FAILURE_RETRY(recvmsg(control_sock_, &msg, 0));

  if (rc <= 0) {
    PLOG(WARNING) << "Receiving file descriptor from ADB failed (socket " << control_sock_ << ")";
    return android::base::unique_fd(-1);
  } else {
    VLOG(jdwp) << "Fds have been received from ADB!";
  }

  return android::base::unique_fd((reinterpret_cast<int*>(CMSG_DATA(cmsg)))[0]);
}

bool AdbConnectionState::SetupAdbConnection() {
  int        sleep_ms     = 500;
  const int  sleep_max_ms = 2*1000;

  android::base::unique_fd sock(socket(AF_UNIX, SOCK_SEQPACKET, 0));
  if (sock < 0) {
    PLOG(ERROR) << "Could not create ADB control socket";
    return false;
  }
  struct timeval timeout;
  timeout.tv_sec = kControlSockSendTimeout;
  timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  int32_t pid = getpid();

  while (!shutting_down_) {
    // If adbd isn't running, because USB debugging was disabled or
    // perhaps the system is restarting it for "adb root", the
    // connect() will fail.  We loop here forever waiting for it
    // to come back.
    //
    // Waking up and polling every couple of seconds is generally a
    // bad thing to do, but we only do this if the application is
    // debuggable *and* adbd isn't running.  Still, for the sake
    // of battery life, we should consider timing out and giving
    // up after a few minutes in case somebody ships an app with
    // the debuggable flag set.
    int ret = connect(sock, &control_addr_.controlAddrPlain, control_addr_len_);
    if (ret == 0) {
      bool trusted = sock >= 0;
#ifdef ART_TARGET_ANDROID
      // Needed for socket_peer_is_trusted.
      trusted = trusted && socket_peer_is_trusted(sock);
#endif
      if (!trusted) {
        LOG(ERROR) << "adb socket is not trusted. Aborting connection.";
        if (sock >= 0 && shutdown(sock, SHUT_RDWR)) {
          PLOG(ERROR) << "trouble shutting down socket";
        }
        return false;
      }
      /* now try to send our pid to the ADB daemon */
      ret = TEMP_FAILURE_RETRY(send(sock, &pid, sizeof(pid), 0));
      if (ret == sizeof(pid)) {
        VLOG(jdwp) << "PID " << pid << " sent to adb";
        control_sock_ = std::move(sock);
        return true;
      } else {
        PLOG(ERROR) << "Weird, can't send JDWP process pid to ADB. Aborting connection.";
        return false;
      }
    } else {
      if (VLOG_IS_ON(jdwp)) {
        PLOG(ERROR) << "Can't connect to ADB control socket. Will retry.";
      }

      usleep(sleep_ms * 1000);

      sleep_ms += (sleep_ms >> 1);
      if (sleep_ms > sleep_max_ms) {
        sleep_ms = sleep_max_ms;
      }
    }
  }
  return false;
}

void AdbConnectionState::RunPollLoop(art::Thread* self) {
  CHECK_NE(agent_name_, "");
  CHECK_EQ(self->GetState(), art::kNative);
  // TODO: Clang prebuilt for r316199 produces bogus thread safety analysis warning for holding both
  // exclusive and shared lock in the same scope. Remove the assertion as a temporary workaround.
  // http://b/71769596
  // art::Locks::mutator_lock_->AssertNotHeld(self);
  self->SetState(art::kWaitingInMainDebuggerLoop);
  // shutting_down_ set by StopDebuggerThreads
  while (!shutting_down_) {
    // First get the control_sock_ from adb if we don't have one. We only need to do this once.
    if (control_sock_ == -1 && !SetupAdbConnection()) {
      LOG(ERROR) << "Failed to setup adb connection.";
      return;
    }
    while (!shutting_down_ && control_sock_ != -1) {
      bool should_listen_on_connection = !agent_has_socket_ && !sent_agent_fds_;
      struct pollfd pollfds[4] = {
        { sleep_event_fd_, POLLIN, 0 },
        // -1 as an fd causes it to be ignored by poll
        { (agent_loaded_ ? local_agent_control_sock_ : -1), POLLIN, 0 },
        // Check for the control_sock_ actually going away. Only do this if we don't have an active
        // connection.
        { (adb_connection_socket_ == -1 ? control_sock_ : -1), POLLIN | POLLRDHUP, 0 },
        // if we have not loaded the agent either the adb_connection_socket_ is -1 meaning we don't
        // have a real connection yet or the socket through adb needs to be listened to for incoming
        // data that the agent or this plugin can handle.
        { should_listen_on_connection ? adb_connection_socket_ : -1, POLLIN | POLLRDHUP, 0 }
      };
      int res = TEMP_FAILURE_RETRY(poll(pollfds, 4, -1));
      if (res < 0) {
        PLOG(ERROR) << "Failed to poll!";
        return;
      }
      // We don't actually care about doing this we just use it to wake us up.
      // const struct pollfd& sleep_event_poll     = pollfds[0];
      const struct pollfd& agent_control_sock_poll = pollfds[1];
      const struct pollfd& control_sock_poll       = pollfds[2];
      const struct pollfd& adb_socket_poll         = pollfds[3];
      if (FlagsSet(agent_control_sock_poll.revents, POLLIN)) {
        DCHECK(agent_loaded_);
        char buf[257];
        res = TEMP_FAILURE_RETRY(recv(local_agent_control_sock_, buf, sizeof(buf) - 1, 0));
        if (res < 0) {
          PLOG(ERROR) << "Failed to read message from agent control socket! Retrying";
          continue;
        } else {
          buf[res + 1] = '\0';
          VLOG(jdwp) << "Local agent control sock has data: " << static_cast<const char*>(buf);
        }
        if (memcmp(kListenStartMessage, buf, sizeof(kListenStartMessage)) == 0) {
          agent_listening_ = true;
          if (adb_connection_socket_ != -1) {
            SendAgentFds(/*require_handshake*/ !performed_handshake_);
          }
        } else if (memcmp(kListenEndMessage, buf, sizeof(kListenEndMessage)) == 0) {
          agent_listening_ = false;
        } else if (memcmp(kCloseMessage, buf, sizeof(kCloseMessage)) == 0) {
          CloseFds();
          agent_has_socket_ = false;
        } else if (memcmp(kAcceptMessage, buf, sizeof(kAcceptMessage)) == 0) {
          agent_has_socket_ = true;
          sent_agent_fds_ = false;
          // We will only ever do the handshake once so reset this.
          performed_handshake_ = false;
        } else {
          LOG(ERROR) << "Unknown message received from debugger! '" << std::string(buf) << "'";
        }
      } else if (FlagsSet(control_sock_poll.revents, POLLIN)) {
        bool maybe_send_fds = false;
        {
          // Hold onto this lock so that concurrent ddm publishes don't try to use an illegal fd.
          ScopedEventFdLock sefdl(adb_write_event_fd_);
          android::base::unique_fd new_fd(ReadFdFromAdb());
          if (new_fd == -1) {
            // Something went wrong. We need to retry getting the control socket.
            PLOG(ERROR) << "Something went wrong getting fds from adb. Retry!";
            control_sock_.reset();
            break;
          } else if (adb_connection_socket_ != -1) {
            // We already have a connection.
            VLOG(jdwp) << "Ignoring second debugger. Accept then drop!";
            if (new_fd >= 0) {
              new_fd.reset();
            }
          } else {
            VLOG(jdwp) << "Adb connection established with fd " << new_fd;
            adb_connection_socket_ = std::move(new_fd);
            maybe_send_fds = true;
          }
        }
        if (maybe_send_fds && agent_loaded_ && agent_listening_) {
          VLOG(jdwp) << "Sending fds as soon as we received them.";
          // The agent was already loaded so this must be after a disconnection. Therefore have the
          // transport perform the handshake.
          SendAgentFds(/*require_handshake*/ true);
        }
      } else if (FlagsSet(control_sock_poll.revents, POLLRDHUP)) {
        // The other end of the adb connection just dropped it.
        // Reset the connection since we don't have an active socket through the adb server.
        DCHECK(!agent_has_socket_) << "We shouldn't be doing anything if there is already a "
                                   << "connection active";
        control_sock_.reset();
        break;
      } else if (FlagsSet(adb_socket_poll.revents, POLLIN)) {
        DCHECK(!agent_has_socket_);
        if (!agent_loaded_) {
          HandleDataWithoutAgent(self);
        } else if (agent_listening_ && !sent_agent_fds_) {
          VLOG(jdwp) << "Sending agent fds again on data.";
          // Agent was already loaded so it can deal with the handshake.
          SendAgentFds(/*require_handshake*/ true);
        }
      } else if (FlagsSet(adb_socket_poll.revents, POLLRDHUP)) {
        DCHECK(!agent_has_socket_);
        CloseFds();
      } else {
        VLOG(jdwp) << "Woke up poll without anything to do!";
      }
    }
  }
}

static uint32_t ReadUint32AndAdvance(/*in-out*/uint8_t** in) {
  uint32_t res;
  memcpy(&res, *in, sizeof(uint32_t));
  *in = (*in) + sizeof(uint32_t);
  return ntohl(res);
}

void AdbConnectionState::HandleDataWithoutAgent(art::Thread* self) {
  DCHECK(!agent_loaded_);
  DCHECK(!agent_listening_);
  // TODO Should we check in some other way if we are userdebug/eng?
  CHECK(art::Dbg::IsJdwpAllowed());
  // We try to avoid loading the agent which is expensive. First lets just perform the handshake.
  if (!performed_handshake_) {
    PerformHandshake();
    return;
  }
  // Read the packet header to figure out if it is one we can handle. We only 'peek' into the stream
  // to see if it's one we can handle. This doesn't change the state of the socket.
  alignas(sizeof(uint32_t)) uint8_t packet_header[kPacketHeaderLen];
  ssize_t res = TEMP_FAILURE_RETRY(recv(adb_connection_socket_.get(),
                                        packet_header,
                                        sizeof(packet_header),
                                        MSG_PEEK));
  // We want to be very careful not to change the socket state until we know we succeeded. This will
  // let us fall-back to just loading the agent and letting it deal with everything.
  if (res <= 0) {
    // Close the socket. We either hit EOF or an error.
    if (res < 0) {
      PLOG(ERROR) << "Unable to peek into adb socket due to error. Closing socket.";
    }
    CloseFds();
    return;
  } else if (res < static_cast<int>(kPacketHeaderLen)) {
    LOG(ERROR) << "Unable to peek into adb socket. Loading agent to handle this. Only read " << res;
    AttachJdwpAgent(self);
    return;
  }
  uint32_t full_len = ntohl(*reinterpret_cast<uint32_t*>(packet_header + kPacketSizeOff));
  uint32_t pkt_id = ntohl(*reinterpret_cast<uint32_t*>(packet_header + kPacketIdOff));
  uint8_t pkt_cmd_set = packet_header[kPacketCommandSetOff];
  uint8_t pkt_cmd = packet_header[kPacketCommandOff];
  if (pkt_cmd_set != kDdmCommandSet ||
      pkt_cmd != kDdmChunkCommand ||
      full_len < kPacketHeaderLen) {
    VLOG(jdwp) << "Loading agent due to jdwp packet that cannot be handled by adbconnection.";
    AttachJdwpAgent(self);
    return;
  }
  uint32_t avail = -1;
  res = TEMP_FAILURE_RETRY(ioctl(adb_connection_socket_.get(), FIONREAD, &avail));
  if (res < 0) {
    PLOG(ERROR) << "Failed to determine amount of readable data in socket! Closing connection";
    CloseFds();
    return;
  } else if (avail < full_len) {
    LOG(WARNING) << "Unable to handle ddm command in adbconnection due to insufficent data. "
                 << "Expected " << full_len << " bytes but only " << avail << " are readable. "
                 << "Loading jdwp agent to deal with this.";
    AttachJdwpAgent(self);
    return;
  }
  // Actually read the data.
  std::vector<uint8_t> full_pkt;
  full_pkt.resize(full_len);
  res = TEMP_FAILURE_RETRY(recv(adb_connection_socket_.get(), full_pkt.data(), full_len, 0));
  if (res < 0) {
    PLOG(ERROR) << "Failed to recv data from adb connection. Closing connection";
    CloseFds();
    return;
  }
  DCHECK_EQ(memcmp(full_pkt.data(), packet_header, sizeof(packet_header)), 0);
  size_t data_size = full_len - kPacketHeaderLen;
  if (data_size < (sizeof(uint32_t) * 2)) {
    // This is an error (the data isn't long enough) but to match historical behavior we need to
    // ignore it.
    return;
  }
  uint8_t* ddm_data = full_pkt.data() + kPacketHeaderLen;
  uint32_t ddm_type = ReadUint32AndAdvance(&ddm_data);
  uint32_t ddm_len = ReadUint32AndAdvance(&ddm_data);
  if (ddm_len > data_size - (2 * sizeof(uint32_t))) {
    // This is an error (the data isn't long enough) but to match historical behavior we need to
    // ignore it.
    return;
  }

  if (!notified_ddm_active_) {
    NotifyDdms(/*active*/ true);
  }
  uint32_t reply_type;
  std::vector<uint8_t> reply;
  if (!art::Dbg::DdmHandleChunk(self->GetJniEnv(),
                                ddm_type,
                                art::ArrayRef<const jbyte>(reinterpret_cast<const jbyte*>(ddm_data),
                                                           ddm_len),
                                /*out*/&reply_type,
                                /*out*/&reply)) {
    // To match historical behavior we don't send any response when there is no data to reply with.
    return;
  }
  SendDdmPacket(pkt_id,
                DdmPacketType::kReply,
                reply_type,
                art::ArrayRef<const uint8_t>(reply));
}

void AdbConnectionState::PerformHandshake() {
  CHECK(!performed_handshake_);
  // Check to make sure we are able to read the whole handshake.
  uint32_t avail = -1;
  int res = TEMP_FAILURE_RETRY(ioctl(adb_connection_socket_.get(), FIONREAD, &avail));
  if (res < 0 || avail < sizeof(kJdwpHandshake)) {
    if (res < 0) {
      PLOG(ERROR) << "Failed to determine amount of readable data for handshake!";
    }
    LOG(WARNING) << "Closing connection to broken client.";
    CloseFds();
    return;
  }
  // Perform the handshake.
  char handshake_msg[sizeof(kJdwpHandshake)];
  res = TEMP_FAILURE_RETRY(recv(adb_connection_socket_.get(),
                                handshake_msg,
                                sizeof(handshake_msg),
                                MSG_DONTWAIT));
  if (res < static_cast<int>(sizeof(kJdwpHandshake)) ||
      strncmp(handshake_msg, kJdwpHandshake, sizeof(kJdwpHandshake)) != 0) {
    if (res < 0) {
      PLOG(ERROR) << "Failed to read handshake!";
    }
    LOG(WARNING) << "Handshake failed!";
    CloseFds();
    return;
  }
  // Send the handshake back.
  res = TEMP_FAILURE_RETRY(send(adb_connection_socket_.get(),
                                kJdwpHandshake,
                                sizeof(kJdwpHandshake),
                                0));
  if (res < static_cast<int>(sizeof(kJdwpHandshake))) {
    PLOG(ERROR) << "Failed to send jdwp-handshake response.";
    CloseFds();
    return;
  }
  performed_handshake_ = true;
}

void AdbConnectionState::AttachJdwpAgent(art::Thread* self) {
  art::Runtime* runtime = art::Runtime::Current();
  self->AssertNoPendingException();
  runtime->AttachAgent(/* JNIEnv */ nullptr,
                       MakeAgentArg(),
                       /* classloader */ nullptr);
  if (self->IsExceptionPending()) {
    LOG(ERROR) << "Failed to load agent " << agent_name_;
    art::ScopedObjectAccess soa(self);
    self->GetException()->Dump();
    self->ClearException();
    return;
  }
  agent_loaded_ = true;
}

bool ContainsArgument(const std::string& opts, const char* arg) {
  return opts.find(arg) != std::string::npos;
}

bool ValidateJdwpOptions(const std::string& opts) {
  bool res = true;
  // The adbconnection plugin requires that the jdwp agent be configured as a 'server' because that
  // is what adb expects and otherwise we will hit a deadlock as the poll loop thread stops waiting
  // for the fd's to be passed down.
  if (ContainsArgument(opts, "server=n")) {
    res = false;
    LOG(ERROR) << "Cannot start jdwp debugging with server=n from adbconnection.";
  }
  // We don't start the jdwp agent until threads are already running. It is far too late to suspend
  // everything.
  if (ContainsArgument(opts, "suspend=y")) {
    res = false;
    LOG(ERROR) << "Cannot use suspend=y with late-init jdwp.";
  }
  return res;
}

std::string AdbConnectionState::MakeAgentArg() {
  const std::string& opts = art::Runtime::Current()->GetJdwpOptions();
  DCHECK(ValidateJdwpOptions(opts));
  // TODO Get agent_name_ from something user settable?
  return agent_name_ + "=" + opts + (opts.empty() ? "" : ",") +
      "ddm_already_active=" + (notified_ddm_active_ ? "y" : "n") + "," +
      // See the comment above for why we need to be server=y. Since the agent defaults to server=n
      // we will add it if it wasn't already present for the convenience of the user.
      (ContainsArgument(opts, "server=y") ? "" : "server=y,") +
      // See the comment above for why we need to be suspend=n. Since the agent defaults to
      // suspend=y we will add it if it wasn't already present.
      (ContainsArgument(opts, "suspend=n") ? "" : "suspend=n") +
      "transport=dt_fd_forward,address=" + std::to_string(remote_agent_control_sock_);
}

void AdbConnectionState::StopDebuggerThreads() {
  // The regular agent system will take care of unloading the agent (if needed).
  shutting_down_ = true;
  // Wakeup the poll loop.
  uint64_t data = 1;
  if (sleep_event_fd_ != -1) {
    TEMP_FAILURE_RETRY(write(sleep_event_fd_, &data, sizeof(data)));
  }
}

// The plugin initialization function.
extern "C" bool ArtPlugin_Initialize() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  DCHECK(art::Runtime::Current()->GetJdwpProvider() == art::JdwpProvider::kAdbConnection);
  // TODO Provide some way for apps to set this maybe?
  DCHECK(gState == nullptr);
  gState = new AdbConnectionState(kDefaultJdwpAgentName);
  return ValidateJdwpOptions(art::Runtime::Current()->GetJdwpOptions());
}

extern "C" bool ArtPlugin_Deinitialize() {
  gState->StopDebuggerThreads();
  if (!gState->DebuggerThreadsStarted()) {
    // If debugger threads were started then those threads will delete the state once they are done.
    delete gState;
  }
  return true;
}

}  // namespace adbconnection
