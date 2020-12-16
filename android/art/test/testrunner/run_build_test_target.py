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

"""Build and run go/ab/git_master-art-host target

This script is executed by the android build server and must not be moved,
or changed in an otherwise backwards-incompatible manner.

Provided with a target name, the script setup the environment for
building the test target by taking config information from
from target_config.py.

See target_config.py for the configuration syntax.
"""

import argparse
import os
import subprocess
import sys

from target_config import target_config
import env

parser = argparse.ArgumentParser()
parser.add_argument('-j', default='1', dest='n_threads')
# either -l/--list OR build-target is required (but not both).
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument('build_target', nargs='?')
group.add_argument('-l', '--list', action='store_true', help='List all possible run-build targets.')
options = parser.parse_args()

##########

if options.list:
  print "List of all known build_target: "
  for k in sorted(target_config.iterkeys()):
    print " * " + k
  # TODO: would be nice if this was the same order as the target config file.
  sys.exit(1)

if not target_config.get(options.build_target):
  sys.stderr.write("error: invalid build_target, see -l/--list.\n")
  sys.exit(1)

target = target_config[options.build_target]
n_threads = options.n_threads
custom_env = target.get('env', {})
custom_env['SOONG_ALLOW_MISSING_DEPENDENCIES'] = 'true'
print custom_env
os.environ.update(custom_env)

if target.has_key('make'):
  build_command = 'make'
  build_command += ' DX='
  build_command += ' -j' + str(n_threads)
  build_command += ' -C ' + env.ANDROID_BUILD_TOP
  build_command += ' ' + target.get('make')
  # Add 'dist' to avoid Jack issues b/36169180.
  build_command += ' dist'
  sys.stdout.write(str(build_command) + '\n')
  sys.stdout.flush()
  if subprocess.call(build_command.split()):
    sys.exit(1)

if target.has_key('golem'):
  machine_type = target.get('golem')
  # use art-opt-cc by default since it mimics the default preopt config.
  default_golem_config = 'art-opt-cc'

  os.chdir(env.ANDROID_BUILD_TOP)
  cmd =  ['art/tools/golem/build-target.sh']
  cmd += ['-j' + str(n_threads)]
  cmd += ['--showcommands']
  cmd += ['--machine-type=%s' %(machine_type)]
  cmd += ['--golem=%s' %(default_golem_config)]
  cmd += ['--tarball']
  sys.stdout.write(str(cmd) + '\n')
  sys.stdout.flush()

  if subprocess.call(cmd):
    sys.exit(1)

if target.has_key('run-test'):
  run_test_command = [os.path.join(env.ANDROID_BUILD_TOP,
                                   'art/test/testrunner/testrunner.py')]
  test_flags = target.get('run-test', [])
  run_test_command += test_flags
  # Let testrunner compute concurrency based on #cpus.
  # b/65822340
  # run_test_command += ['-j', str(n_threads)]

  # In the config assume everything will run with --host and on ART.
  # However for only [--jvm] this is undesirable, so don't pass in ART-specific flags.
  if ['--jvm'] != test_flags:
    run_test_command += ['--host']
    run_test_command += ['--dex2oat-jobs']
    run_test_command += ['4']
  run_test_command += ['-b']
  run_test_command += ['--verbose']

  sys.stdout.write(str(run_test_command) + '\n')
  sys.stdout.flush()
  if subprocess.call(run_test_command):
    sys.exit(1)

sys.exit(0)
