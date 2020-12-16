#!/usr/bin/env python3
#
# Copyright 2017, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
A python program that simulates the plugin side of the dt_fd_forward transport for testing.

This program will invoke a given java language runtime program and send down debugging arguments
that cause it to use the dt_fd_forward transport. This will then create a normal server-port that
debuggers can attach to.
"""

import argparse
import array
from multiprocessing import Process
import contextlib
import ctypes
import os
import select
import socket
import subprocess
import sys
import time

NEED_HANDSHAKE_MESSAGE = b"HANDSHAKE:REQD\x00"
LISTEN_START_MESSAGE   = b"dt_fd_forward:START-LISTEN\x00"
LISTEN_END_MESSAGE     = b"dt_fd_forward:END-LISTEN\x00"
ACCEPTED_MESSAGE       = b"dt_fd_forward:ACCEPTED\x00"
CLOSE_MESSAGE          = b"dt_fd_forward:CLOSING\x00"

libc = ctypes.cdll.LoadLibrary("libc.so.6")
def eventfd(init_val, flags):
  """
  Creates an eventfd. See 'man 2 eventfd' for more information.
  """
  return libc.eventfd(init_val, flags)

@contextlib.contextmanager
def make_eventfd(init):
  """
  Creates an eventfd with given initial value that is closed after the manager finishes.
  """
  fd = eventfd(init, 0)
  yield fd
  os.close(fd)

@contextlib.contextmanager
def make_sockets():
  """
  Make a (remote,local) socket pair. The remote socket is inheritable by forked processes. They are
  both linked together.
  """
  (rfd, lfd) = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
  yield (rfd, lfd)
  rfd.close()
  lfd.close()

def send_fds(sock, remote_read, remote_write, remote_event):
  """
  Send the three fds over the given socket.
  """
  sock.sendmsg([NEED_HANDSHAKE_MESSAGE],  # We want the transport to handle the handshake.
               [(socket.SOL_SOCKET,  # Send over socket.
                 socket.SCM_RIGHTS,  # Payload is file-descriptor array
                 array.array('i', [remote_read, remote_write, remote_event]))])


def HandleSockets(host, port, local_sock, finish_event):
  """
  Handle the IO between the network and the runtime.

  This is similar to what we will do with the plugin that controls the jdwp connection.

  The main difference is it will keep around the connection and event-fd in order to let it send
  ddms packets directly.
  """
  listening = False
  with socket.socket() as sock:
    sock.bind((host, port))
    sock.listen()
    while True:
      sources = [local_sock, finish_event, sock]
      print("Starting select on " + str(sources))
      (rf, _, _) = select.select(sources, [], [])
      if local_sock in rf:
        buf = local_sock.recv(1024)
        print("Local_sock has data: " + str(buf))
        if buf == LISTEN_START_MESSAGE:
          print("listening on " + str(sock))
          listening = True
        elif buf == LISTEN_END_MESSAGE:
          print("End listening")
          listening = False
        elif buf == ACCEPTED_MESSAGE:
          print("Fds were accepted.")
        elif buf == CLOSE_MESSAGE:
          # TODO Dup the fds and send a fake DDMS message like the actual plugin would.
          print("Fds were closed")
        else:
          print("Unknown data received from socket " + str(buf))
          return
      elif sock in rf:
        (conn, addr) = sock.accept()
        with conn:
          print("connection accepted from " + str(addr))
          if listening:
            with make_eventfd(1) as efd:
              print("sending fds ({}, {}, {}) to target.".format(conn.fileno(), conn.fileno(), efd))
              send_fds(local_sock, conn.fileno(), conn.fileno(), efd)
          else:
            print("Closing fds since we cannot accept them.")
      if finish_event in rf:
        print("woke up from finish_event")
        return

def StartChildProcess(cmd_pre, cmd_post, jdwp_lib, jdwp_ops, remote_sock, can_be_runtest):
  """
  Open the child java-language runtime process.
  """
  full_cmd = list(cmd_pre)
  os.set_inheritable(remote_sock.fileno(), True)
  jdwp_arg = jdwp_lib + "=" + \
             jdwp_ops + "transport=dt_fd_forward,address=" + str(remote_sock.fileno())
  if can_be_runtest and cmd_pre[0].endswith("run-test"):
    print("Assuming run-test. Pass --no-run-test if this isn't true")
    full_cmd += ["--with-agent", jdwp_arg]
  else:
    full_cmd.append("-agentpath:" + jdwp_arg)
  full_cmd += cmd_post
  print("Running " + str(full_cmd))
  # Start the actual process with the fd being passed down.
  proc = subprocess.Popen(full_cmd, close_fds=False)
  # Get rid of the extra socket.
  remote_sock.close()
  proc.wait()

def main():
  parser = argparse.ArgumentParser(description="""
                                   Runs a socket that forwards to dt_fds.

                                   Pass '--' to start passing in the program we will pass the debug
                                   options down to.
                                   """)
  parser.add_argument("--host", type=str, default="localhost",
                      help="Host we will listen for traffic on. Defaults to 'localhost'.")
  parser.add_argument("--debug-lib", type=str, default="libjdwp.so",
                      help="jdwp library we pass to -agentpath:. Default is 'libjdwp.so'")
  parser.add_argument("--debug-options", type=str, default="server=y,suspend=y,",
                      help="non-address options we pass to jdwp agent, default is " +
                           "'server=y,suspend=y,'")
  parser.add_argument("--port", type=int, default=12345,
                      help="port we will expose the traffic on. Defaults to 12345.")
  parser.add_argument("--no-run-test", default=False, action="store_true",
                      help="don't pass in arguments for run-test even if it looks like that is " +
                           "the program")
  parser.add_argument("--pre-end", type=int, default=1,
                      help="number of 'rest' arguments to put before passing in the debug options")
  end_idx = 0 if '--' not in sys.argv else sys.argv.index('--')
  if end_idx == 0 and ('--help' in sys.argv or '-h' in sys.argv):
    parser.print_help()
    return
  args = parser.parse_args(sys.argv[:end_idx][1:])
  rest = sys.argv[1 + end_idx:]

  with make_eventfd(0) as wakeup_event:
    with make_sockets() as (remote_sock, local_sock):
      invoker = Process(target=StartChildProcess,
                        args=(rest[:args.pre_end],
                              rest[args.pre_end:],
                              args.debug_lib,
                              args.debug_options,
                              remote_sock,
                              not args.no_run_test))
      socket_handler = Process(target=HandleSockets,
                               args=(args.host, args.port, local_sock, wakeup_event))
      socket_handler.start()
      invoker.start()
    invoker.join()
    # Write any 64 bit value to the wakeup_event to make sure that the socket handler will wake
    # up and exit.
    os.write(wakeup_event, b'\x00\x00\x00\x00\x00\x00\x01\x00')
    socket_handler.join()

if __name__ == '__main__':
  main()
