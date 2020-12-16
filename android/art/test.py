#!/usr/bin/env python
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

# --run-test : To run run-test
# --gtest : To run gtest
# -j : Number of jobs
# --host: for host tests
# --target: for target tests
# All the other arguments will be passed to the run-test testrunner.
import sys
import subprocess
import os
import argparse

ANDROID_BUILD_TOP = os.environ.get('ANDROID_BUILD_TOP', os.getcwd())

parser = argparse.ArgumentParser()
parser.add_argument('-j', default='', dest='n_threads', help='specify number of concurrent tests')
parser.add_argument('--run-test', '-r', action='store_true', dest='run_test', help='execute run tests')
parser.add_argument('--gtest', '-g', action='store_true', dest='gtest', help='execute gtest tests')
parser.add_argument('--target', action='store_true', dest='target', help='test on target system')
parser.add_argument('--host', action='store_true', dest='host', help='test on build host system')
parser.add_argument('--help-runner', action='store_true', dest='help_runner', help='show help for optional run test arguments')
options, unknown = parser.parse_known_args()

if options.run_test or options.help_runner or not options.gtest:
  testrunner = os.path.join('./',
                          ANDROID_BUILD_TOP,
                            'art/test/testrunner/testrunner.py')
  run_test_args = []
  for arg in sys.argv[1:]:
    if arg == '--run-test' or arg == '--gtest' \
    or arg == '-r' or arg == '-g':
      continue
    if arg == '--help-runner':
      run_test_args = ['--help']
      break
    run_test_args.append(arg)

  test_runner_cmd = [testrunner] + run_test_args
  print test_runner_cmd
  if subprocess.call(test_runner_cmd) or options.help_runner:
    sys.exit(1)

if options.gtest or not options.run_test:
  build_target = ''
  if options.host or not options.target:
    build_target += ' test-art-host-gtest'
  if options.target or not options.host:
    build_target += ' test-art-target-gtest'

  build_command = 'make'
  build_command += ' -j' + str(options.n_threads)

  build_command += ' -C ' + ANDROID_BUILD_TOP
  build_command += ' ' + build_target
  # Add 'dist' to avoid Jack issues b/36169180.
  build_command += ' dist'

  print build_command

  if subprocess.call(build_command.split()):
      sys.exit(1)

sys.exit(0)
