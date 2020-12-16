#!/usr/bin/env python3.4
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Performs bisection bug search on methods and optimizations.

See README.md.

Example usage:
./bisection-search.py -cp classes.dex --expected-output output Test
"""

import abc
import argparse
import os
import re
import shlex
import sys

from subprocess import call
from tempfile import NamedTemporaryFile

sys.path.append(os.path.dirname(os.path.dirname(
        os.path.realpath(__file__))))

from common.common import DeviceTestEnv
from common.common import FatalError
from common.common import GetEnvVariableOrError
from common.common import HostTestEnv
from common.common import LogSeverity
from common.common import RetCode


# Passes that are never disabled during search process because disabling them
# would compromise correctness.
MANDATORY_PASSES = ['dex_cache_array_fixups_arm',
                    'dex_cache_array_fixups_mips',
                    'instruction_simplifier$before_codegen',
                    'pc_relative_fixups_x86',
                    'pc_relative_fixups_mips',
                    'x86_memory_operand_generation']

# Passes that show up as optimizations in compiler verbose output but aren't
# driven by run-passes mechanism. They are mandatory and will always run, we
# never pass them to --run-passes.
NON_PASSES = ['builder', 'prepare_for_register_allocation',
              'liveness', 'register']

# If present in raw cmd, this tag will be replaced with runtime arguments
# controlling the bisection search. Otherwise arguments will be placed on second
# position in the command.
RAW_CMD_RUNTIME_ARGS_TAG = '{ARGS}'

# Default core image path relative to ANDROID_HOST_OUT.
DEFAULT_IMAGE_RELATIVE_PATH = '/framework/core.art'

class Dex2OatWrapperTestable(object):
  """Class representing a testable compilation.

  Accepts filters on compiled methods and optimization passes.
  """

  def __init__(self, base_cmd, test_env, expected_retcode=None,
               output_checker=None, verbose=False):
    """Constructor.

    Args:
      base_cmd: list of strings, base command to run.
      test_env: ITestEnv.
      expected_retcode: RetCode, expected normalized return code.
      output_checker: IOutputCheck, output checker.
      verbose: bool, enable verbose output.
    """
    self._base_cmd = base_cmd
    self._test_env = test_env
    self._expected_retcode = expected_retcode
    self._output_checker = output_checker
    self._compiled_methods_path = self._test_env.CreateFile('compiled_methods')
    self._passes_to_run_path = self._test_env.CreateFile('run_passes')
    self._verbose = verbose
    if RAW_CMD_RUNTIME_ARGS_TAG in self._base_cmd:
      self._arguments_position = self._base_cmd.index(RAW_CMD_RUNTIME_ARGS_TAG)
      self._base_cmd.pop(self._arguments_position)
    else:
      self._arguments_position = 1

  def Test(self, compiled_methods, passes_to_run=None):
    """Tests compilation with compiled_methods and run_passes switches active.

    If compiled_methods is None then compiles all methods.
    If passes_to_run is None then runs default passes.

    Args:
      compiled_methods: list of strings representing methods to compile or None.
      passes_to_run: list of strings representing passes to run or None.

    Returns:
      True if test passes with given settings. False otherwise.
    """
    if self._verbose:
      print('Testing methods: {0} passes: {1}.'.format(
          compiled_methods, passes_to_run))
    cmd = self._PrepareCmd(compiled_methods=compiled_methods,
                           passes_to_run=passes_to_run)
    (output, ret_code) = self._test_env.RunCommand(
        cmd, LogSeverity.ERROR)
    res = True
    if self._expected_retcode:
      res = self._expected_retcode == ret_code
    if self._output_checker:
      res = res and self._output_checker.Check(output)
    if self._verbose:
      print('Test passed: {0}.'.format(res))
    return res

  def GetAllMethods(self):
    """Get methods compiled during the test.

    Returns:
      List of strings representing methods compiled during the test.

    Raises:
      FatalError: An error occurred when retrieving methods list.
    """
    cmd = self._PrepareCmd()
    (output, _) = self._test_env.RunCommand(cmd, LogSeverity.INFO)
    match_methods = re.findall(r'Building ([^\n]+)\n', output)
    if not match_methods:
      raise FatalError('Failed to retrieve methods list. '
                       'Not recognized output format.')
    return match_methods

  def GetAllPassesForMethod(self, compiled_method):
    """Get all optimization passes ran for a method during the test.

    Args:
      compiled_method: string representing method to compile.

    Returns:
      List of strings representing passes ran for compiled_method during test.

    Raises:
      FatalError: An error occurred when retrieving passes list.
    """
    cmd = self._PrepareCmd(compiled_methods=[compiled_method])
    (output, _) = self._test_env.RunCommand(cmd, LogSeverity.INFO)
    match_passes = re.findall(r'Starting pass: ([^\n]+)\n', output)
    if not match_passes:
      raise FatalError('Failed to retrieve passes list. '
                       'Not recognized output format.')
    return [p for p in match_passes if p not in NON_PASSES]

  def _PrepareCmd(self, compiled_methods=None, passes_to_run=None):
    """Prepare command to run."""
    cmd = self._base_cmd[0:self._arguments_position]
    # insert additional arguments before the first argument
    if compiled_methods is not None:
      self._test_env.WriteLines(self._compiled_methods_path, compiled_methods)
      cmd += ['-Xcompiler-option', '--compiled-methods={0}'.format(
          self._compiled_methods_path)]
    if passes_to_run is not None:
      self._test_env.WriteLines(self._passes_to_run_path, passes_to_run)
      cmd += ['-Xcompiler-option', '--run-passes={0}'.format(
          self._passes_to_run_path)]
    cmd += ['-Xcompiler-option', '--runtime-arg', '-Xcompiler-option',
            '-verbose:compiler', '-Xcompiler-option', '-j1']
    cmd += self._base_cmd[self._arguments_position:]
    return cmd


class IOutputCheck(object):
  """Abstract output checking class.

  Checks if output is correct.
  """
  __meta_class__ = abc.ABCMeta

  @abc.abstractmethod
  def Check(self, output):
    """Check if output is correct.

    Args:
      output: string, output to check.

    Returns:
      boolean, True if output is correct, False otherwise.
    """


class EqualsOutputCheck(IOutputCheck):
  """Concrete output checking class checking for equality to expected output."""

  def __init__(self, expected_output):
    """Constructor.

    Args:
      expected_output: string, expected output.
    """
    self._expected_output = expected_output

  def Check(self, output):
    """See base class."""
    return self._expected_output == output


class ExternalScriptOutputCheck(IOutputCheck):
  """Concrete output checking class calling an external script.

  The script should accept two arguments, path to expected output and path to
  program output. It should exit with 0 return code if outputs are equivalent
  and with different return code otherwise.
  """

  def __init__(self, script_path, expected_output_path, logfile):
    """Constructor.

    Args:
      script_path: string, path to checking script.
      expected_output_path: string, path to file with expected output.
      logfile: file handle, logfile.
    """
    self._script_path = script_path
    self._expected_output_path = expected_output_path
    self._logfile = logfile

  def Check(self, output):
    """See base class."""
    ret_code = None
    with NamedTemporaryFile(mode='w', delete=False) as temp_file:
      temp_file.write(output)
      temp_file.flush()
      ret_code = call(
          [self._script_path, self._expected_output_path, temp_file.name],
          stdout=self._logfile, stderr=self._logfile, universal_newlines=True)
    return ret_code == 0


def BinarySearch(start, end, test):
  """Binary search integers using test function to guide the process."""
  while start < end:
    mid = (start + end) // 2
    if test(mid):
      start = mid + 1
    else:
      end = mid
  return start


def FilterPasses(passes, cutoff_idx):
  """Filters passes list according to cutoff_idx but keeps mandatory passes."""
  return [opt_pass for idx, opt_pass in enumerate(passes)
          if opt_pass in MANDATORY_PASSES or idx < cutoff_idx]


def BugSearch(testable):
  """Find buggy (method, optimization pass) pair for a given testable.

  Args:
    testable: Dex2OatWrapperTestable.

  Returns:
    (string, string) tuple. First element is name of method which when compiled
    exposes test failure. Second element is name of optimization pass such that
    for aforementioned method running all passes up to and excluding the pass
    results in test passing but running all passes up to and including the pass
    results in test failing.

    (None, None) if test passes when compiling all methods.
    (string, None) if a method is found which exposes the failure, but the
      failure happens even when running just mandatory passes.

  Raises:
    FatalError: Testable fails with no methods compiled.
    AssertionError: Method failed for all passes when bisecting methods, but
    passed when bisecting passes. Possible sporadic failure.
  """
  all_methods = testable.GetAllMethods()
  faulty_method_idx = BinarySearch(
      0,
      len(all_methods) + 1,
      lambda mid: testable.Test(all_methods[0:mid]))
  if faulty_method_idx == len(all_methods) + 1:
    return (None, None)
  if faulty_method_idx == 0:
    raise FatalError('Testable fails with no methods compiled.')
  faulty_method = all_methods[faulty_method_idx - 1]
  all_passes = testable.GetAllPassesForMethod(faulty_method)
  faulty_pass_idx = BinarySearch(
      0,
      len(all_passes) + 1,
      lambda mid: testable.Test([faulty_method],
                                FilterPasses(all_passes, mid)))
  if faulty_pass_idx == 0:
    return (faulty_method, None)
  assert faulty_pass_idx != len(all_passes) + 1, ('Method must fail for some '
                                                  'passes.')
  faulty_pass = all_passes[faulty_pass_idx - 1]
  return (faulty_method, faulty_pass)


def PrepareParser():
  """Prepares argument parser."""
  parser = argparse.ArgumentParser(
      description='Tool for finding compiler bugs. Either --raw-cmd or both '
                  '-cp and --class are required.')
  command_opts = parser.add_argument_group('dalvikvm command options')
  command_opts.add_argument('-cp', '--classpath', type=str, help='classpath')
  command_opts.add_argument('--class', dest='classname', type=str,
                            help='name of main class')
  command_opts.add_argument('--lib', type=str, default='libart.so',
                            help='lib to use, default: libart.so')
  command_opts.add_argument('--dalvikvm-option', dest='dalvikvm_opts',
                            metavar='OPT', nargs='*', default=[],
                            help='additional dalvikvm option')
  command_opts.add_argument('--arg', dest='test_args', nargs='*', default=[],
                            metavar='ARG', help='argument passed to test')
  command_opts.add_argument('--image', type=str, help='path to image')
  command_opts.add_argument('--raw-cmd', type=str,
                            help='bisect with this command, ignore other '
                                 'command options')
  bisection_opts = parser.add_argument_group('bisection options')
  bisection_opts.add_argument('--64', dest='x64', action='store_true',
                              default=False, help='x64 mode')
  bisection_opts.add_argument(
      '--device', action='store_true', default=False, help='run on device')
  bisection_opts.add_argument(
      '--device-serial', help='device serial number, implies --device')
  bisection_opts.add_argument('--expected-output', type=str,
                              help='file containing expected output')
  bisection_opts.add_argument(
      '--expected-retcode', type=str, help='expected normalized return code',
      choices=[RetCode.SUCCESS.name, RetCode.TIMEOUT.name, RetCode.ERROR.name])
  bisection_opts.add_argument(
      '--check-script', type=str,
      help='script comparing output and expected output')
  bisection_opts.add_argument(
      '--logfile', type=str, help='custom logfile location')
  bisection_opts.add_argument('--cleanup', action='store_true',
                              default=False, help='clean up after bisecting')
  bisection_opts.add_argument('--timeout', type=int, default=60,
                              help='if timeout seconds pass assume test failed')
  bisection_opts.add_argument('--verbose', action='store_true',
                              default=False, help='enable verbose output')
  return parser


def PrepareBaseCommand(args, classpath):
  """Prepares base command used to run test."""
  if args.raw_cmd:
    return shlex.split(args.raw_cmd)
  else:
    base_cmd = ['dalvikvm64'] if args.x64 else ['dalvikvm32']
    if not args.device:
      base_cmd += ['-XXlib:{0}'.format(args.lib)]
      if not args.image:
        image_path = (GetEnvVariableOrError('ANDROID_HOST_OUT') +
                      DEFAULT_IMAGE_RELATIVE_PATH)
      else:
        image_path = args.image
      base_cmd += ['-Ximage:{0}'.format(image_path)]
    if args.dalvikvm_opts:
      base_cmd += args.dalvikvm_opts
    base_cmd += ['-cp', classpath, args.classname] + args.test_args
  return base_cmd


def main():
  # Parse arguments
  parser = PrepareParser()
  args = parser.parse_args()
  if not args.raw_cmd and (not args.classpath or not args.classname):
    parser.error('Either --raw-cmd or both -cp and --class are required')
  if args.device_serial:
    args.device = True
  if args.expected_retcode:
    args.expected_retcode = RetCode[args.expected_retcode]
  if not args.expected_retcode and not args.check_script:
    args.expected_retcode = RetCode.SUCCESS

  # Prepare environment
  classpath = args.classpath
  if args.device:
    test_env = DeviceTestEnv(
        'bisection_search_', args.cleanup, args.logfile, args.timeout,
        args.device_serial)
    if classpath:
      classpath = test_env.PushClasspath(classpath)
  else:
    test_env = HostTestEnv(
        'bisection_search_', args.cleanup, args.logfile, args.timeout, args.x64)
  base_cmd = PrepareBaseCommand(args, classpath)
  output_checker = None
  if args.expected_output:
    if args.check_script:
      output_checker = ExternalScriptOutputCheck(
          args.check_script, args.expected_output, test_env.logfile)
    else:
      with open(args.expected_output, 'r') as expected_output_file:
        output_checker = EqualsOutputCheck(expected_output_file.read())

  # Perform the search
  try:
    testable = Dex2OatWrapperTestable(base_cmd, test_env, args.expected_retcode,
                                      output_checker, args.verbose)
    if testable.Test(compiled_methods=[]):
      (method, opt_pass) = BugSearch(testable)
    else:
      print('Testable fails with no methods compiled.')
      sys.exit(1)
  except Exception as e:
    print('Error occurred.\nLogfile: {0}'.format(test_env.logfile.name))
    test_env.logfile.write('Exception: {0}\n'.format(e))
    raise

  # Report results
  if method is None:
    print('Couldn\'t find any bugs.')
  elif opt_pass is None:
    print('Faulty method: {0}. Fails with just mandatory passes.'.format(
        method))
  else:
    print('Faulty method and pass: {0}, {1}.'.format(method, opt_pass))
  print('Logfile: {0}'.format(test_env.logfile.name))
  sys.exit(0)


if __name__ == '__main__':
  main()
