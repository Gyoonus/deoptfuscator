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

"""ART Run-Test TestRunner

The testrunner runs the ART run-tests by simply invoking the script.
It fetches the list of eligible tests from art/test directory, and list of
disabled tests from art/test/knownfailures.json. It runs the tests by
invoking art/test/run-test script and checks the exit value to decide if the
test passed or failed.

Before invoking the script, first build all the tests dependencies.
There are two major build targets for building target and host tests
dependencies:
1) test-art-host-run-test
2) test-art-target-run-test

There are various options to invoke the script which are:
-t: Either the test name as in art/test or the test name including the variant
    information. Eg, "-t 001-HelloWorld",
    "-t test-art-host-run-test-debug-prebuild-optimizing-relocate-ntrace-cms-checkjni-picimage-npictest-ndebuggable-001-HelloWorld32"
-j: Number of thread workers to be used. Eg - "-j64"
--dry-run: Instead of running the test name, just print its name.
--verbose
-b / --build-dependencies: to build the dependencies before running the test

To specify any specific variants for the test, use --<<variant-name>>.
For eg, for compiler type as optimizing, use --optimizing.


In the end, the script will print the failed and skipped tests if any.

"""
import argparse
import collections
import fnmatch
import itertools
import json
import multiprocessing
import os
import re
import subprocess
import sys
import tempfile
import threading
import time

import env
from target_config import target_config
from device_config import device_config

# timeout for individual tests.
# TODO: make it adjustable per tests and for buildbots
timeout = 3000 # 50 minutes

# DISABLED_TEST_CONTAINER holds information about the disabled tests. It is a map
# that has key as the test name (like 001-HelloWorld), and value as set of
# variants that the test is disabled for.
DISABLED_TEST_CONTAINER = {}

# The Dict contains the list of all possible variants for a given type. For example,
# for key TARGET, the value would be target and host. The list is used to parse
# the test name given as the argument to run.
VARIANT_TYPE_DICT = {}

# The set contains all the variants of each time.
TOTAL_VARIANTS_SET = set()

# The colors are used in the output. When a test passes, COLOR_PASS is used,
# and so on.
COLOR_ERROR = '\033[91m'
COLOR_PASS = '\033[92m'
COLOR_SKIP = '\033[93m'
COLOR_NORMAL = '\033[0m'

# The mutex object is used by the threads for exclusive access of test_count
# to make any changes in its value.
test_count_mutex = threading.Lock()

# The set contains the list of all the possible run tests that are in art/test
# directory.
RUN_TEST_SET = set()

# The semaphore object is used by the testrunner to limit the number of
# threads to the user requested concurrency value.
semaphore = threading.Semaphore(1)

# The mutex object is used to provide exclusive access to a thread to print
# its output.
print_mutex = threading.Lock()
failed_tests = []
skipped_tests = []

# Flags
n_thread = -1
test_count = 0
total_test_count = 0
verbose = False
dry_run = False
ignore_skips = False
build = False
gdb = False
gdb_arg = ''
stop_testrunner = False
dex2oat_jobs = -1   # -1 corresponds to default threads for dex2oat
run_all_configs = False

# Dict containing extra arguments
extra_arguments = { "host" : [], "target" : [] }

# Dict to store user requested test variants.
# key: variant_type.
# value: set of variants user wants to run of type <key>.
_user_input_variants = collections.defaultdict(set)

def gather_test_info():
  """The method gathers test information about the test to be run which includes
  generating the list of total tests from the art/test directory and the list
  of disabled test. It also maps various variants to types.
  """
  global TOTAL_VARIANTS_SET
  global DISABLED_TEST_CONTAINER
  # TODO: Avoid duplication of the variant names in different lists.
  VARIANT_TYPE_DICT['pictest'] = {'pictest', 'npictest'}
  VARIANT_TYPE_DICT['run'] = {'ndebug', 'debug'}
  VARIANT_TYPE_DICT['target'] = {'target', 'host', 'jvm'}
  VARIANT_TYPE_DICT['trace'] = {'trace', 'ntrace', 'stream'}
  VARIANT_TYPE_DICT['image'] = {'picimage', 'no-image', 'multipicimage'}
  VARIANT_TYPE_DICT['debuggable'] = {'ndebuggable', 'debuggable'}
  VARIANT_TYPE_DICT['gc'] = {'gcstress', 'gcverify', 'cms'}
  VARIANT_TYPE_DICT['prebuild'] = {'no-prebuild', 'no-dex2oat', 'prebuild'}
  VARIANT_TYPE_DICT['cdex_level'] = {'cdex-none', 'cdex-fast'}
  VARIANT_TYPE_DICT['relocate'] = {'relocate-npatchoat', 'relocate', 'no-relocate'}
  VARIANT_TYPE_DICT['jni'] = {'jni', 'forcecopy', 'checkjni'}
  VARIANT_TYPE_DICT['address_sizes'] = {'64', '32'}
  VARIANT_TYPE_DICT['jvmti'] = {'no-jvmti', 'jvmti-stress', 'redefine-stress', 'trace-stress',
                                'field-stress', 'step-stress'}
  VARIANT_TYPE_DICT['compiler'] = {'interp-ac', 'interpreter', 'jit', 'optimizing',
                                   'regalloc_gc', 'speed-profile'}

  for v_type in VARIANT_TYPE_DICT:
    TOTAL_VARIANTS_SET = TOTAL_VARIANTS_SET.union(VARIANT_TYPE_DICT.get(v_type))

  test_dir = env.ANDROID_BUILD_TOP + '/art/test'
  for f in os.listdir(test_dir):
    if fnmatch.fnmatch(f, '[0-9]*'):
      RUN_TEST_SET.add(f)
  DISABLED_TEST_CONTAINER = get_disabled_test_info()


def setup_test_env():
  """The method sets default value for the various variants of the tests if they
  are already not set.
  """
  if env.ART_TEST_BISECTION:
    env.ART_TEST_RUN_TEST_NO_PREBUILD = True
    env.ART_TEST_RUN_TEST_PREBUILD = False
    # Bisection search writes to standard output.
    env.ART_TEST_QUIET = False

  global _user_input_variants
  global run_all_configs
  if run_all_configs:
    target_types = _user_input_variants['target']
    _user_input_variants = VARIANT_TYPE_DICT
    _user_input_variants['target'] = target_types

  if not _user_input_variants['target']:
    _user_input_variants['target'].add('host')
    _user_input_variants['target'].add('target')

  if not _user_input_variants['prebuild']: # Default
    _user_input_variants['prebuild'].add('prebuild')

  if not _user_input_variants['cdex_level']: # Default
    _user_input_variants['cdex_level'].add('cdex-fast')

  # By default only run without jvmti
  if not _user_input_variants['jvmti']:
    _user_input_variants['jvmti'].add('no-jvmti')

  # By default we run all 'compiler' variants.
  if not _user_input_variants['compiler'] and _user_input_variants['target'] != 'jvm':
    _user_input_variants['compiler'].add('optimizing')
    _user_input_variants['compiler'].add('jit')
    _user_input_variants['compiler'].add('interpreter')
    _user_input_variants['compiler'].add('interp-ac')
    _user_input_variants['compiler'].add('speed-profile')

  if not _user_input_variants['relocate']: # Default
    _user_input_variants['relocate'].add('no-relocate')

  if not _user_input_variants['trace']: # Default
    _user_input_variants['trace'].add('ntrace')

  if not _user_input_variants['gc']: # Default
    _user_input_variants['gc'].add('cms')

  if not _user_input_variants['jni']: # Default
    _user_input_variants['jni'].add('checkjni')

  if not _user_input_variants['image']: # Default
    _user_input_variants['image'].add('picimage')

  if not _user_input_variants['pictest']: # Default
    _user_input_variants['pictest'].add('npictest')

  if not _user_input_variants['debuggable']: # Default
    _user_input_variants['debuggable'].add('ndebuggable')

  if not _user_input_variants['run']: # Default
    _user_input_variants['run'].add('debug')

  _user_input_variants['address_sizes_target'] = collections.defaultdict(set)
  if not _user_input_variants['address_sizes']:
    _user_input_variants['address_sizes_target']['target'].add(
        env.ART_PHONY_TEST_TARGET_SUFFIX)
    _user_input_variants['address_sizes_target']['host'].add(
        env.ART_PHONY_TEST_HOST_SUFFIX)
    if env.ART_TEST_RUN_TEST_2ND_ARCH:
      _user_input_variants['address_sizes_target']['host'].add(
          env.ART_2ND_PHONY_TEST_HOST_SUFFIX)
      _user_input_variants['address_sizes_target']['target'].add(
          env.ART_2ND_PHONY_TEST_TARGET_SUFFIX)
  else:
    _user_input_variants['address_sizes_target']['host'] = _user_input_variants['address_sizes']
    _user_input_variants['address_sizes_target']['target'] = _user_input_variants['address_sizes']

  global n_thread
  if n_thread is -1:
    if 'target' in _user_input_variants['target']:
      n_thread = get_default_threads('target')
    else:
      n_thread = get_default_threads('host')
    print_text("Concurrency: " + str(n_thread) + "\n")

  global extra_arguments
  for target in _user_input_variants['target']:
    extra_arguments[target] = find_extra_device_arguments(target)

  global semaphore
  semaphore = threading.Semaphore(n_thread)

  if not sys.stdout.isatty():
    global COLOR_ERROR
    global COLOR_PASS
    global COLOR_SKIP
    global COLOR_NORMAL
    COLOR_ERROR = ''
    COLOR_PASS = ''
    COLOR_SKIP = ''
    COLOR_NORMAL = ''

def find_extra_device_arguments(target):
  """
  Gets any extra arguments from the device_config.
  """
  device_name = target
  if target == 'target':
    device_name = get_device_name()
  return device_config.get(device_name, { 'run-test-args' : [] })['run-test-args']

def get_device_name():
  """
  Gets the value of ro.product.name from remote device.
  """
  proc = subprocess.Popen(['adb', 'shell', 'getprop', 'ro.product.name'],
                          stderr=subprocess.STDOUT,
                          stdout = subprocess.PIPE,
                          universal_newlines=True)
  # only wait 2 seconds.
  output = proc.communicate(timeout = 2)[0]
  success = not proc.wait()
  if success:
    return output.strip()
  else:
    print_text("Unable to determine device type!\n")
    print_text("Continuing anyway.\n")
    return "UNKNOWN_TARGET"

def run_tests(tests):
  """Creates thread workers to run the tests.

  The method generates command and thread worker to run the tests. Depending on
  the user input for the number of threads to be used, the method uses a
  semaphore object to keep a count in control for the thread workers. When a new
  worker is created, it acquires the semaphore object, and when the number of
  workers reaches the maximum allowed concurrency, the method wait for an
  existing thread worker to release the semaphore object. Worker releases the
  semaphore object when they finish printing the output.

  Args:
    tests: The set of tests to be run.
  """
  options_all = ''

  # jvm does not run with all these combinations,
  # or at least it doesn't make sense for most of them.
  # TODO: support some jvm variants like jvmti ?
  target_input_variants = _user_input_variants['target']
  uncombinated_target_input_variants = []
  if 'jvm' in target_input_variants:
    _user_input_variants['target'].remove('jvm')
    uncombinated_target_input_variants.append('jvm')

  global total_test_count
  total_test_count = len(tests)
  if target_input_variants:
    for variant_type in VARIANT_TYPE_DICT:
      if not (variant_type == 'target' or 'address_sizes' in variant_type):
        total_test_count *= len(_user_input_variants[variant_type])
  target_address_combinations = 0
  for target in target_input_variants:
    for address_size in _user_input_variants['address_sizes_target'][target]:
      target_address_combinations += 1
  target_address_combinations += len(uncombinated_target_input_variants)
  total_test_count *= target_address_combinations

  if env.ART_TEST_WITH_STRACE:
    options_all += ' --strace'

  if env.ART_TEST_RUN_TEST_ALWAYS_CLEAN:
    options_all += ' --always-clean'

  if env.ART_TEST_BISECTION:
    options_all += ' --bisection-search'

  if env.ART_TEST_ANDROID_ROOT:
    options_all += ' --android-root ' + env.ART_TEST_ANDROID_ROOT

  if gdb:
    options_all += ' --gdb'
    if gdb_arg:
      options_all += ' --gdb-arg ' + gdb_arg

  if dex2oat_jobs != -1:
    options_all += ' --dex2oat-jobs ' + str(dex2oat_jobs)

  def iter_config(tests, input_variants, user_input_variants):
    config = itertools.product(tests, input_variants, user_input_variants['run'],
                                 user_input_variants['prebuild'], user_input_variants['compiler'],
                                 user_input_variants['relocate'], user_input_variants['trace'],
                                 user_input_variants['gc'], user_input_variants['jni'],
                                 user_input_variants['image'], user_input_variants['pictest'],
                                 user_input_variants['debuggable'], user_input_variants['jvmti'],
                                 user_input_variants['cdex_level'])
    return config

  # [--host, --target] combines with all the other user input variants.
  config = iter_config(tests, target_input_variants, _user_input_variants)
  # [--jvm] currently combines with nothing else. most of the extra flags we'd insert
  # would be unrecognizable by the 'java' binary, so avoid inserting any extra flags for now.
  uncombinated_config = iter_config(tests, uncombinated_target_input_variants, { 'run': [''],
      'prebuild': [''], 'compiler': [''],
      'relocate': [''], 'trace': [''],
      'gc': [''], 'jni': [''],
      'image': [''], 'pictest': [''],
      'debuggable': [''], 'jvmti': [''],
      'cdex_level': ['']})

  def start_combination(config_tuple, address_size):
      test, target, run, prebuild, compiler, relocate, trace, gc, \
      jni, image, pictest, debuggable, jvmti, cdex_level = config_tuple

      if stop_testrunner:
        # When ART_TEST_KEEP_GOING is set to false, then as soon as a test
        # fails, stop_testrunner is set to True. When this happens, the method
        # stops creating any any thread and wait for all the exising threads
        # to end.
        while threading.active_count() > 2:
          time.sleep(0.1)
          return
      # NB The order of components here should match the order of
      # components in the regex parser in parse_test_name.
      test_name = 'test-art-'
      test_name += target + '-run-test-'
      test_name += run + '-'
      test_name += prebuild + '-'
      test_name += compiler + '-'
      test_name += relocate + '-'
      test_name += trace + '-'
      test_name += gc + '-'
      test_name += jni + '-'
      test_name += image + '-'
      test_name += pictest + '-'
      test_name += debuggable + '-'
      test_name += jvmti + '-'
      test_name += cdex_level + '-'
      test_name += test
      test_name += address_size

      variant_set = {target, run, prebuild, compiler, relocate, trace, gc, jni,
                     image, pictest, debuggable, jvmti, cdex_level, address_size}

      options_test = options_all

      if target == 'host':
        options_test += ' --host'
      elif target == 'jvm':
        options_test += ' --jvm'

      if run == 'ndebug':
        options_test += ' -O'

      if prebuild == 'prebuild':
        options_test += ' --prebuild'
      elif prebuild == 'no-prebuild':
        options_test += ' --no-prebuild'
      elif prebuild == 'no-dex2oat':
        options_test += ' --no-prebuild --no-dex2oat'

      # Add option and remove the cdex- prefix.
      options_test += ' --compact-dex-level ' + cdex_level.replace('cdex-','')

      if compiler == 'optimizing':
        options_test += ' --optimizing'
      elif compiler == 'regalloc_gc':
        options_test += ' --optimizing -Xcompiler-option --register-allocation-strategy=graph-color'
      elif compiler == 'interpreter':
        options_test += ' --interpreter'
      elif compiler == 'interp-ac':
        options_test += ' --interpreter --verify-soft-fail'
      elif compiler == 'jit':
        options_test += ' --jit'
      elif compiler == 'speed-profile':
        options_test += ' --random-profile'

      if relocate == 'relocate':
        options_test += ' --relocate'
      elif relocate == 'no-relocate':
        options_test += ' --no-relocate'
      elif relocate == 'relocate-npatchoat':
        options_test += ' --relocate --no-patchoat'

      if trace == 'trace':
        options_test += ' --trace'
      elif trace == 'stream':
        options_test += ' --trace --stream'

      if gc == 'gcverify':
        options_test += ' --gcverify'
      elif gc == 'gcstress':
        options_test += ' --gcstress'

      if jni == 'forcecopy':
        options_test += ' --runtime-option -Xjniopts:forcecopy'
      elif jni == 'checkjni':
        options_test += ' --runtime-option -Xcheck:jni'

      if image == 'no-image':
        options_test += ' --no-image'
      elif image == 'multipicimage':
        options_test += ' --multi-image'

      if pictest == 'pictest':
        options_test += ' --pic-test'

      if debuggable == 'debuggable':
        options_test += ' --debuggable'

      if jvmti == 'jvmti-stress':
        options_test += ' --jvmti-trace-stress --jvmti-redefine-stress --jvmti-field-stress'
      elif jvmti == 'field-stress':
        options_test += ' --jvmti-field-stress'
      elif jvmti == 'trace-stress':
        options_test += ' --jvmti-trace-stress'
      elif jvmti == 'redefine-stress':
        options_test += ' --jvmti-redefine-stress'
      elif jvmti == 'step-stress':
        options_test += ' --jvmti-step-stress'

      if address_size == '64':
        options_test += ' --64'

        if env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
          options_test += ' --instruction-set-features' + env.DEX2OAT_HOST_INSTRUCTION_SET_FEATURES

      elif address_size == '32':
        if env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES:
          options_test += ' --instruction-set-features ' + \
                          env.HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES

      # Use the default run-test behavior unless ANDROID_COMPILE_WITH_JACK is explicitly set.
      if env.ANDROID_COMPILE_WITH_JACK == True:
        options_test += ' --build-with-jack'
      elif env.ANDROID_COMPILE_WITH_JACK == False:
        options_test += ' --build-with-javac-dx'

      if env.USE_D8_BY_DEFAULT == True:
        options_test += ' --build-with-d8'

      # TODO(http://36039166): This is a temporary solution to
      # fix build breakages.
      options_test = (' --output-path %s') % (
          tempfile.mkdtemp(dir=env.ART_HOST_TEST_DIR)) + options_test

      run_test_sh = env.ANDROID_BUILD_TOP + '/art/test/run-test'
      command = ' '.join((run_test_sh, options_test, ' '.join(extra_arguments[target]), test))

      semaphore.acquire()
      worker = threading.Thread(target=run_test, args=(command, test, variant_set, test_name))
      worker.daemon = True
      worker.start()

  for config_tuple in config:
    target = config_tuple[1]
    for address_size in _user_input_variants['address_sizes_target'][target]:
      start_combination(config_tuple, address_size)

  for config_tuple in uncombinated_config:
      start_combination(config_tuple, "")  # no address size

  while threading.active_count() > 2:
    time.sleep(0.1)


def run_test(command, test, test_variant, test_name):
  """Runs the test.

  It invokes art/test/run-test script to run the test. The output of the script
  is checked, and if it ends with "Succeeded!", it assumes that the tests
  passed, otherwise, put it in the list of failed test. Before actually running
  the test, it also checks if the test is placed in the list of disabled tests,
  and if yes, it skips running it, and adds the test in the list of skipped
  tests. The method uses print_text method to actually print the output. After
  successfully running and capturing the output for the test, it releases the
  semaphore object.

  Args:
    command: The command to be used to invoke the script
    test: The name of the test without the variant information.
    test_variant: The set of variant for the test.
    test_name: The name of the test along with the variants.
  """
  global stop_testrunner
  try:
    if is_test_disabled(test, test_variant):
      test_skipped = True
    else:
      test_skipped = False
      if gdb:
        proc = subprocess.Popen(command.split(), stderr=subprocess.STDOUT, universal_newlines=True)
      else:
        proc = subprocess.Popen(command.split(), stderr=subprocess.STDOUT, stdout = subprocess.PIPE,
                                universal_newlines=True)
      script_output = proc.communicate(timeout=timeout)[0]
      test_passed = not proc.wait()

    if not test_skipped:
      if test_passed:
        print_test_info(test_name, 'PASS')
      else:
        failed_tests.append((test_name, str(command) + "\n" + script_output))
        if not env.ART_TEST_KEEP_GOING:
          stop_testrunner = True
        print_test_info(test_name, 'FAIL', ('%s\n%s') % (
          command, script_output))
    elif not dry_run:
      print_test_info(test_name, 'SKIP')
      skipped_tests.append(test_name)
    else:
      print_test_info(test_name, '')
  except subprocess.TimeoutExpired as e:
    failed_tests.append((test_name, 'Timed out in %d seconds' % timeout))
    print_test_info(test_name, 'TIMEOUT', 'Timed out in %d seconds\n%s' % (
        timeout, command))
  except Exception as e:
    failed_tests.append((test_name, str(e)))
    print_test_info(test_name, 'FAIL',
    ('%s\n%s\n\n') % (command, str(e)))
  finally:
    semaphore.release()


def print_test_info(test_name, result, failed_test_info=""):
  """Print the continous test information

  If verbose is set to True, it continuously prints test status information
  on a new line.
  If verbose is set to False, it keeps on erasing test
  information by overriding it with the latest test information. Also,
  in this case it stictly makes sure that the information length doesn't
  exceed the console width. It does so by shortening the test_name.

  When a test fails, it prints the output of the run-test script and
  command used to invoke the script. It doesn't override the failing
  test information in either of the cases.
  """

  global test_count
  info = ''
  if not verbose:
    # Without --verbose, the testrunner erases passing test info. It
    # does that by overriding the printed text with white spaces all across
    # the console width.
    console_width = int(os.popen('stty size', 'r').read().split()[1])
    info = '\r' + ' ' * console_width + '\r'
  try:
    print_mutex.acquire()
    test_count += 1
    percent = (test_count * 100) / total_test_count
    progress_info = ('[ %d%% %d/%d ]') % (
      percent,
      test_count,
      total_test_count)

    if result == 'FAIL' or result == 'TIMEOUT':
      if not verbose:
        info += ('%s %s %s\n') % (
          progress_info,
          test_name,
          COLOR_ERROR + result + COLOR_NORMAL)
      else:
        info += ('%s %s %s\n%s\n') % (
          progress_info,
          test_name,
          COLOR_ERROR + result + COLOR_NORMAL,
          failed_test_info)
    else:
      result_text = ''
      if result == 'PASS':
        result_text += COLOR_PASS + 'PASS' + COLOR_NORMAL
      elif result == 'SKIP':
        result_text += COLOR_SKIP + 'SKIP' + COLOR_NORMAL

      if verbose:
        info += ('%s %s %s\n') % (
          progress_info,
          test_name,
          result_text)
      else:
        total_output_length = 2 # Two spaces
        total_output_length += len(progress_info)
        total_output_length += len(result)
        allowed_test_length = console_width - total_output_length
        test_name_len = len(test_name)
        if allowed_test_length < test_name_len:
          test_name = ('...%s') % (
            test_name[-(allowed_test_length - 3):])
        info += ('%s %s %s') % (
          progress_info,
          test_name,
          result_text)
    print_text(info)
  except Exception as e:
    print_text(('%s\n%s\n') % (test_name, str(e)))
    failed_tests.append(test_name)
  finally:
    print_mutex.release()

def verify_knownfailure_entry(entry):
  supported_field = {
      'tests' : (list, str),
      'test_patterns' : (list,),
      'description' : (list, str),
      'bug' : (str,),
      'variant' : (str,),
      'env_vars' : (dict,),
  }
  for field in entry:
    field_type = type(entry[field])
    if field_type not in supported_field[field]:
      raise ValueError('%s is not supported type for %s\n%s' % (
          str(field_type),
          field,
          str(entry)))

def get_disabled_test_info():
  """Generate set of known failures.

  It parses the art/test/knownfailures.json file to generate the list of
  disabled tests.

  Returns:
    The method returns a dict of tests mapped to the variants list
    for which the test should not be run.
  """
  known_failures_file = env.ANDROID_BUILD_TOP + '/art/test/knownfailures.json'
  with open(known_failures_file) as known_failures_json:
    known_failures_info = json.loads(known_failures_json.read())

  disabled_test_info = {}
  for failure in known_failures_info:
    verify_knownfailure_entry(failure)
    tests = failure.get('tests', [])
    if isinstance(tests, str):
      tests = [tests]
    patterns = failure.get("test_patterns", [])
    if (not isinstance(patterns, list)):
      raise ValueError("test_patters is not a list in %s" % failure)

    tests += [f for f in RUN_TEST_SET if any(re.match(pat, f) is not None for pat in patterns)]
    variants = parse_variants(failure.get('variant'))
    env_vars = failure.get('env_vars')

    if check_env_vars(env_vars):
      for test in tests:
        if test not in RUN_TEST_SET:
          raise ValueError('%s is not a valid run-test' % (
              test))
        if test in disabled_test_info:
          disabled_test_info[test] = disabled_test_info[test].union(variants)
        else:
          disabled_test_info[test] = variants
  return disabled_test_info


def check_env_vars(env_vars):
  """Checks if the env variables are set as required to run the test.

  Returns:
    True if all the env variables are set as required, otherwise False.
  """

  if not env_vars:
    return True
  for key in env_vars:
    if env.get_env(key) != env_vars.get(key):
      return False
  return True


def is_test_disabled(test, variant_set):
  """Checks if the test along with the variant_set is disabled.

  Args:
    test: The name of the test as in art/test directory.
    variant_set: Variants to be used for the test.
  Returns:
    True, if the test is disabled.
  """
  if dry_run:
    return True
  if test in env.EXTRA_DISABLED_TESTS:
    return True
  if ignore_skips:
    return False
  variants_list = DISABLED_TEST_CONTAINER.get(test, {})
  for variants in variants_list:
    variants_present = True
    for variant in variants:
      if variant not in variant_set:
        variants_present = False
        break
    if variants_present:
      return True
  return False


def parse_variants(variants):
  """Parse variants fetched from art/test/knownfailures.json.
  """
  if not variants:
    variants = ''
    for variant in TOTAL_VARIANTS_SET:
      variants += variant
      variants += '|'
    variants = variants[:-1]
  variant_list = set()
  or_variants = variants.split('|')
  for or_variant in or_variants:
    and_variants = or_variant.split('&')
    variant = set()
    for and_variant in and_variants:
      and_variant = and_variant.strip()
      if and_variant not in TOTAL_VARIANTS_SET:
        raise ValueError('%s is not a valid variant' % (
            and_variant))
      variant.add(and_variant)
    variant_list.add(frozenset(variant))
  return variant_list

def print_text(output):
  sys.stdout.write(output)
  sys.stdout.flush()

def print_analysis():
  if not verbose:
    # Without --verbose, the testrunner erases passing test info. It
    # does that by overriding the printed text with white spaces all across
    # the console width.
    console_width = int(os.popen('stty size', 'r').read().split()[1])
    eraser_text = '\r' + ' ' * console_width + '\r'
    print_text(eraser_text)

  # Prints information about the total tests run.
  # E.g., "2/38 (5%) tests passed".
  passed_test_count = total_test_count - len(skipped_tests) - len(failed_tests)
  passed_test_information = ('%d/%d (%d%%) %s passed.\n') % (
      passed_test_count,
      total_test_count,
      (passed_test_count*100)/total_test_count,
      'tests' if passed_test_count > 1 else 'test')
  print_text(passed_test_information)

  # Prints the list of skipped tests, if any.
  if skipped_tests:
    print_text(COLOR_SKIP + 'SKIPPED TESTS: ' + COLOR_NORMAL + '\n')
    for test in skipped_tests:
      print_text(test + '\n')
    print_text('\n')

  # Prints the list of failed tests, if any.
  if failed_tests:
    print_text(COLOR_ERROR + 'FAILED: ' + COLOR_NORMAL + '\n')
    for test_info in failed_tests:
      print_text(('%s\n%s\n' % (test_info[0], test_info[1])))
    print_text(COLOR_ERROR + '----------' + COLOR_NORMAL + '\n')
    for failed_test in sorted([test_info[0] for test_info in failed_tests]):
      print_text(('%s\n' % (failed_test)))


def parse_test_name(test_name):
  """Parses the testname provided by the user.
  It supports two types of test_name:
  1) Like 001-HelloWorld. In this case, it will just verify if the test actually
  exists and if it does, it returns the testname.
  2) Like test-art-host-run-test-debug-prebuild-interpreter-no-relocate-ntrace-cms-checkjni-picimage-npictest-ndebuggable-001-HelloWorld32
  In this case, it will parse all the variants and check if they are placed
  correctly. If yes, it will set the various VARIANT_TYPES to use the
  variants required to run the test. Again, it returns the test_name
  without the variant information like 001-HelloWorld.
  """
  test_set = set()
  for test in RUN_TEST_SET:
    if test.startswith(test_name):
      test_set.add(test)
  if test_set:
    return test_set

  regex = '^test-art-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['target']) + ')-'
  regex += 'run-test-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['run']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['prebuild']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['compiler']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['relocate']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['trace']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['gc']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['jni']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['image']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['pictest']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['debuggable']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['jvmti']) + ')-'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['cdex_level']) + ')-'
  regex += '(' + '|'.join(RUN_TEST_SET) + ')'
  regex += '(' + '|'.join(VARIANT_TYPE_DICT['address_sizes']) + ')$'
  match = re.match(regex, test_name)
  if match:
    _user_input_variants['target'].add(match.group(1))
    _user_input_variants['run'].add(match.group(2))
    _user_input_variants['prebuild'].add(match.group(3))
    _user_input_variants['compiler'].add(match.group(4))
    _user_input_variants['relocate'].add(match.group(5))
    _user_input_variants['trace'].add(match.group(6))
    _user_input_variants['gc'].add(match.group(7))
    _user_input_variants['jni'].add(match.group(8))
    _user_input_variants['image'].add(match.group(9))
    _user_input_variants['pictest'].add(match.group(10))
    _user_input_variants['debuggable'].add(match.group(11))
    _user_input_variants['jvmti'].add(match.group(12))
    _user_input_variants['cdex_level'].add(match.group(13))
    _user_input_variants['address_sizes'].add(match.group(15))
    return {match.group(14)}
  raise ValueError(test_name + " is not a valid test")


def setup_env_for_build_target(build_target, parser, options):
  """Setup environment for the build target

  The method setup environment for the master-art-host targets.
  """
  os.environ.update(build_target['env'])
  os.environ['SOONG_ALLOW_MISSING_DEPENDENCIES'] = 'true'
  print_text('%s\n' % (str(os.environ)))

  target_options = vars(parser.parse_args(build_target['flags']))
  target_options['host'] = True
  target_options['verbose'] = True
  target_options['build'] = True
  target_options['n_thread'] = options['n_thread']
  target_options['dry_run'] = options['dry_run']

  return target_options

def get_default_threads(target):
  if target is 'target':
    adb_command = 'adb shell cat /sys/devices/system/cpu/present'
    cpu_info_proc = subprocess.Popen(adb_command.split(), stdout=subprocess.PIPE)
    cpu_info = cpu_info_proc.stdout.read()
    if type(cpu_info) is bytes:
      cpu_info = cpu_info.decode('utf-8')
    cpu_info_regex = '\d*-(\d*)'
    match = re.match(cpu_info_regex, cpu_info)
    if match:
      return int(match.group(1))
    else:
      raise ValueError('Unable to predict the concurrency for the target. '
                       'Is device connected?')
  else:
    return multiprocessing.cpu_count()

def parse_option():
  global verbose
  global dry_run
  global ignore_skips
  global n_thread
  global build
  global gdb
  global gdb_arg
  global timeout
  global dex2oat_jobs
  global run_all_configs

  parser = argparse.ArgumentParser(description="Runs all or a subset of the ART test suite.")
  parser.add_argument('-t', '--test', action='append', dest='tests', help='name(s) of the test(s)')
  parser.add_argument('-j', type=int, dest='n_thread')
  parser.add_argument('--timeout', default=timeout, type=int, dest='timeout')
  for variant in TOTAL_VARIANTS_SET:
    flag = '--' + variant
    parser.add_argument(flag, action='store_true', dest=variant)
  parser.add_argument('--verbose', '-v', action='store_true', dest='verbose')
  parser.add_argument('--dry-run', action='store_true', dest='dry_run')
  parser.add_argument("--skip", action="append", dest="skips", default=[],
                      help="Skip the given test in all circumstances.")
  parser.add_argument("--no-skips", dest="ignore_skips", action="store_true", default=False,
                      help="Don't skip any run-test configurations listed in knownfailures.json.")
  parser.add_argument('--no-build-dependencies',
                      action='store_false', dest='build',
                      help="Don't build dependencies under any circumstances. This is the " +
                           "behavior if ART_TEST_RUN_TEST_ALWAYS_BUILD is not set to 'true'.")
  parser.add_argument('-b', '--build-dependencies',
                      action='store_true', dest='build',
                      help="Build dependencies under all circumstances. By default we will " +
                           "not build dependencies unless ART_TEST_RUN_TEST_BUILD=true.")
  parser.add_argument('--build-target', dest='build_target', help='master-art-host targets')
  parser.set_defaults(build = env.ART_TEST_RUN_TEST_BUILD)
  parser.add_argument('--gdb', action='store_true', dest='gdb')
  parser.add_argument('--gdb-arg', dest='gdb_arg')
  parser.add_argument('--dex2oat-jobs', type=int, dest='dex2oat_jobs',
                      help='Number of dex2oat jobs')
  parser.add_argument('-a', '--all', action='store_true', dest='run_all',
                      help="Run all the possible configurations for the input test set")

  options = vars(parser.parse_args())
  if options['build_target']:
    options = setup_env_for_build_target(target_config[options['build_target']],
                                         parser, options)

  tests = None
  env.EXTRA_DISABLED_TESTS.update(set(options['skips']))
  if options['tests']:
    tests = set()
    for test_name in options['tests']:
      tests |= parse_test_name(test_name)

  for variant_type in VARIANT_TYPE_DICT:
    for variant in VARIANT_TYPE_DICT[variant_type]:
      if options.get(variant):
        _user_input_variants[variant_type].add(variant)

  if options['verbose']:
    verbose = True
  if options['n_thread']:
    n_thread = max(1, options['n_thread'])
  ignore_skips = options['ignore_skips']
  if options['dry_run']:
    dry_run = True
    verbose = True
  build = options['build']
  if options['gdb']:
    n_thread = 1
    gdb = True
    if options['gdb_arg']:
      gdb_arg = options['gdb_arg']
  timeout = options['timeout']
  if options['dex2oat_jobs']:
    dex2oat_jobs = options['dex2oat_jobs']
  if options['run_all']:
    run_all_configs = True

  return tests

def main():
  gather_test_info()
  user_requested_tests = parse_option()
  setup_test_env()
  if build:
    build_targets = ''
    if 'host' in _user_input_variants['target']:
      build_targets += 'test-art-host-run-test-dependencies'
    if 'target' in _user_input_variants['target']:
      build_targets += 'test-art-target-run-test-dependencies'
    if 'jvm' in _user_input_variants['target']:
      build_targets += 'test-art-host-run-test-dependencies'
    build_command = 'make'
    build_command += ' DX='
    build_command += ' -j'
    build_command += ' -C ' + env.ANDROID_BUILD_TOP
    build_command += ' ' + build_targets
    # Add 'dist' to avoid Jack issues b/36169180.
    build_command += ' dist'
    if subprocess.call(build_command.split()):
      sys.exit(1)
  if user_requested_tests:
    test_runner_thread = threading.Thread(target=run_tests, args=(user_requested_tests,))
  else:
    test_runner_thread = threading.Thread(target=run_tests, args=(RUN_TEST_SET,))
  test_runner_thread.daemon = True
  try:
    test_runner_thread.start()
    # This loops waits for all the threads to finish, unless
    # stop_testrunner is set to True. When ART_TEST_KEEP_GOING
    # is set to false, stop_testrunner is set to True as soon as
    # a test fails to signal the parent thread  to stop
    # the execution of the testrunner.
    while threading.active_count() > 1 and not stop_testrunner:
      time.sleep(0.1)
    print_analysis()
  except Exception as e:
    print_analysis()
    print_text(str(e))
    sys.exit(1)
  if failed_tests:
    sys.exit(1)
  sys.exit(0)

if __name__ == '__main__':
  main()
