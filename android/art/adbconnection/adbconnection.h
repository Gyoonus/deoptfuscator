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

#ifndef ART_ADBCONNECTION_ADBCONNECTION_H_
#define ART_ADBCONNECTION_ADBCONNECTION_H_

#include <stdint.h>
#include <vector>
#include <limits>

#include "android-base/unique_fd.h"

#include "base/mutex.h"
#include "base/array_ref.h"
#include "runtime_callbacks.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <jni.h>

namespace adbconnection {

static constexpr char kJdwpControlName[] = "\0jdwp-control";
static constexpr char kAdbConnectionThreadName[] = "ADB-JDWP Connection Control Thread";

// The default jdwp agent name.
static constexpr char kDefaultJdwpAgentName[] = "libjdwp.so";

class AdbConnectionState;

struct AdbConnectionDebuggerController : public art::DebuggerControlCallback {
  explicit AdbConnectionDebuggerController(AdbConnectionState* connection)
      : connection_(connection) {}

  // Begin running the debugger.
  void StartDebugger() OVERRIDE;

  // The debugger should begin shutting down since the runtime is ending.
  void StopDebugger() OVERRIDE;

  bool IsDebuggerConfigured() OVERRIDE;

 private:
  AdbConnectionState* connection_;
};

enum class DdmPacketType : uint8_t { kReply = 0x80, kCmd = 0x00, };

struct AdbConnectionDdmCallback : public art::DdmCallback {
  explicit AdbConnectionDdmCallback(AdbConnectionState* connection) : connection_(connection) {}

  void DdmPublishChunk(uint32_t type,
                       const art::ArrayRef<const uint8_t>& data)
      REQUIRES_SHARED(art::Locks::mutator_lock_);

 private:
  AdbConnectionState* connection_;
};

class AdbConnectionState {
 public:
  explicit AdbConnectionState(const std::string& name);

  // Called on the listening thread to start dealing with new input. thr is used to attach the new
  // thread to the runtime.
  void RunPollLoop(art::Thread* self);

  // Sends ddms data over the socket, if there is one. This data is sent even if we haven't finished
  // hand-shaking yet.
  void PublishDdmData(uint32_t type, const art::ArrayRef<const uint8_t>& data);

  // Stops debugger threads during shutdown.
  void StopDebuggerThreads();

  // If StartDebuggerThreads was called successfully.
  bool DebuggerThreadsStarted() {
    return started_debugger_threads_;
  }

 private:
  uint32_t NextDdmId();

  void StartDebuggerThreads();

  // Tell adbd about the new runtime.
  bool SetupAdbConnection();

  std::string MakeAgentArg();

  android::base::unique_fd ReadFdFromAdb();

  void SendAgentFds(bool require_handshake);

  void CloseFds();

  void HandleDataWithoutAgent(art::Thread* self);

  void PerformHandshake();

  void AttachJdwpAgent(art::Thread* self);

  void NotifyDdms(bool active);

  void SendDdmPacket(uint32_t id,
                     DdmPacketType type,
                     uint32_t ddm_type,
                     art::ArrayRef<const uint8_t> data);

  std::string agent_name_;

  AdbConnectionDebuggerController controller_;
  AdbConnectionDdmCallback ddm_callback_;

  // Eventfd used to allow the StopDebuggerThreads function to wake up sleeping threads
  android::base::unique_fd sleep_event_fd_;

  // Socket that we use to talk to adbd.
  android::base::unique_fd control_sock_;

  // Socket that we use to talk to the agent (if it's loaded).
  android::base::unique_fd local_agent_control_sock_;

  // The fd of the socket the agent uses to talk to us. We need to keep it around in order to clean
  // it up when the runtime goes away.
  android::base::unique_fd remote_agent_control_sock_;

  // The fd that is forwarded through adb to the client. This is guarded by the
  // adb_write_event_fd_.
  android::base::unique_fd adb_connection_socket_;

  // The fd we send to the agent to let us synchronize access to the shared adb_connection_socket_.
  // This is also used as a general lock for the adb_connection_socket_ on any threads other than
  // the poll thread.
  android::base::unique_fd adb_write_event_fd_;

  std::atomic<bool> shutting_down_;

  // True if we have loaded the agent library.
  std::atomic<bool> agent_loaded_;

  // True if the dt_fd_forward transport is listening for a new communication channel.
  std::atomic<bool> agent_listening_;

  // True if the dt_fd_forward transport has the socket. If so we don't do anything to the agent or
  // the adb connection socket until connection goes away.
  std::atomic<bool> agent_has_socket_;

  std::atomic<bool> sent_agent_fds_;

  bool performed_handshake_;

  bool notified_ddm_active_;

  std::atomic<uint32_t> next_ddm_id_;

  bool started_debugger_threads_;

  socklen_t control_addr_len_;
  union {
    sockaddr_un controlAddrUn;
    sockaddr controlAddrPlain;
  } control_addr_;

  friend struct AdbConnectionDebuggerController;
};

}  // namespace adbconnection

#endif  // ART_ADBCONNECTION_ADBCONNECTION_H_
