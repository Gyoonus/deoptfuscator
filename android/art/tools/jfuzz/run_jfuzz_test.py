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

import abc
import argparse
import filecmp
import os
import shlex
import shutil
import subprocess
import sys

from glob import glob
from subprocess import DEVNULL
from tempfile import mkdtemp

sys.path.append(os.path.dirname(os.path.dirname(
    os.path.realpath(__file__))))

from common.common import RetCode
from common.common import CommandListToCommandString
from common.common import FatalError
from common.common import GetJackClassPath
from common.common import GetEnvVariableOrError
from common.common import RunCommand
from common.common import RunCommandForOutput
from common.common import DeviceTestEnv

# Return codes supported by bisection bug search.
BISECTABLE_RET_CODES = (RetCode.SUCCESS, RetCode.ERROR, RetCode.TIMEOUT)


def GetExecutionModeRunner(dexer, debug_info, device, mode):
  """Returns a runner for the given execution mode.

  Args:
    dexer: string, defines dexer
    debug_info: boolean, if True include debugging info
    device: string, target device serial number (or None)
    mode: string, execution mode
  Returns:
    TestRunner with given execution mode
  Raises:
    FatalError: error for unknown execution mode
  """
  if mode == 'ri':
    return TestRunnerRIOnHost(debug_info)
  if mode == 'hint':
    return TestRunnerArtIntOnHost(dexer, debug_info)
  if mode == 'hopt':
    return TestRunnerArtOptOnHost(dexer, debug_info)
  if mode == 'tint':
    return TestRunnerArtIntOnTarget(dexer, debug_info, device)
  if mode == 'topt':
    return TestRunnerArtOptOnTarget(dexer, debug_info, device)
  raise FatalError('Unknown execution mode')


#
# Execution mode classes.
#


class TestRunner(object):
  """Abstraction for running a test in a particular execution mode."""
  __meta_class__ = abc.ABCMeta

  @abc.abstractproperty
  def description(self):
    """Returns a description string of the execution mode."""

  @abc.abstractproperty
  def id(self):
    """Returns a short string that uniquely identifies the execution mode."""

  @property
  def output_file(self):
    return self.id + '_out.txt'

  @abc.abstractmethod
  def GetBisectionSearchArgs(self):
    """Get arguments to pass to bisection search tool.

    Returns:
      list of strings - arguments for bisection search tool, or None if
      runner is not bisectable
    """

  @abc.abstractmethod
  def CompileAndRunTest(self):
    """Compile and run the generated test.

    Ensures that the current Test.java in the temporary directory is compiled
    and executed under the current execution mode. On success, transfers the
    generated output to the file self.output_file in the temporary directory.

    Most nonzero return codes are assumed non-divergent, since systems may
    exit in different ways. This is enforced by normalizing return codes.

    Returns:
      normalized return code
    """


class TestRunnerWithHostCompilation(TestRunner):
  """Abstract test runner that supports compilation on host."""

  def  __init__(self, dexer, debug_info):
    """Constructor for the runner with host compilation.

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
    """
    self._dexer = dexer
    self._debug_info = debug_info
    self._jack_args = ['-cp', GetJackClassPath(), '--output-dex', '.',
                       'Test.java']

  def CompileOnHost(self):
    if self._dexer == 'dx' or self._dexer == 'd8':
      dbg = '-g' if self._debug_info else '-g:none'
      if RunCommand(['javac', '--release=8', dbg, 'Test.java'],
                    out=None, err=None, timeout=30) == RetCode.SUCCESS:
        dx = 'dx' if self._dexer == 'dx' else 'd8-compat-dx'
        retc = RunCommand([dx, '--dex', '--output=classes.dex'] + glob('*.class'),
                          out=None, err='dxerr.txt', timeout=30)
      else:
        retc = RetCode.NOTCOMPILED
    elif self._dexer == 'jack':
      retc = RunCommand(['jack'] + self._jack_args,
                        out=None, err='jackerr.txt', timeout=30)
    else:
      raise FatalError('Unknown dexer: ' + self._dexer)
    return retc


class TestRunnerRIOnHost(TestRunner):
  """Concrete test runner of the reference implementation on host."""

  def  __init__(self, debug_info):
    """Constructor for the runner with host compilation.

    Args:
      debug_info: boolean, if True include debugging info
    """
    self._debug_info = debug_info

  @property
  def description(self):
    return 'RI on host'

  @property
  def id(self):
    return 'RI'

  def CompileAndRunTest(self):
    dbg = '-g' if self._debug_info else '-g:none'
    if RunCommand(['javac', '--release=8', dbg, 'Test.java'],
                  out=None, err=None, timeout=30) == RetCode.SUCCESS:
      retc = RunCommand(['java', 'Test'], self.output_file, err=None)
    else:
      retc = RetCode.NOTCOMPILED
    return retc

  def GetBisectionSearchArgs(self):
    return None


class TestRunnerArtOnHost(TestRunnerWithHostCompilation):
  """Abstract test runner of Art on host."""

  def  __init__(self, dexer, debug_info, extra_args=None):
    """Constructor for the Art on host tester.

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
      extra_args: list of strings, extra arguments for dalvikvm
    """
    super().__init__(dexer, debug_info)
    self._art_cmd = ['/bin/bash', 'art', '-cp', 'classes.dex']
    if extra_args is not None:
      self._art_cmd += extra_args
    self._art_cmd.append('Test')

  def CompileAndRunTest(self):
    if self.CompileOnHost() == RetCode.SUCCESS:
      retc = RunCommand(self._art_cmd, self.output_file, 'arterr.txt')
    else:
      retc = RetCode.NOTCOMPILED
    return retc


class TestRunnerArtIntOnHost(TestRunnerArtOnHost):
  """Concrete test runner of interpreter mode Art on host."""

  def  __init__(self, dexer, debug_info):
    """Constructor for the Art on host tester (interpreter).

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
   """
    super().__init__(dexer, debug_info, ['-Xint'])

  @property
  def description(self):
    return 'Art interpreter on host'

  @property
  def id(self):
    return 'HInt'

  def GetBisectionSearchArgs(self):
    return None


class TestRunnerArtOptOnHost(TestRunnerArtOnHost):
  """Concrete test runner of optimizing compiler mode Art on host."""

  def  __init__(self, dexer, debug_info):
    """Constructor for the Art on host tester (optimizing).

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
   """
    super().__init__(dexer, debug_info, None)

  @property
  def description(self):
    return 'Art optimizing on host'

  @property
  def id(self):
    return 'HOpt'

  def GetBisectionSearchArgs(self):
    cmd_str = CommandListToCommandString(
        self._art_cmd[0:2] + ['{ARGS}'] + self._art_cmd[2:])
    return ['--raw-cmd={0}'.format(cmd_str), '--timeout', str(30)]


class TestRunnerArtOnTarget(TestRunnerWithHostCompilation):
  """Abstract test runner of Art on target."""

  def  __init__(self, dexer, debug_info, device, extra_args=None):
    """Constructor for the Art on target tester.

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
      device: string, target device serial number (or None)
      extra_args: list of strings, extra arguments for dalvikvm
    """
    super().__init__(dexer, debug_info)
    self._test_env = DeviceTestEnv('jfuzz_', specific_device=device)
    self._dalvik_cmd = ['dalvikvm']
    if extra_args is not None:
      self._dalvik_cmd += extra_args
    self._device = device
    self._device_classpath = None

  def CompileAndRunTest(self):
    if self.CompileOnHost() == RetCode.SUCCESS:
      self._device_classpath = self._test_env.PushClasspath('classes.dex')
      cmd = self._dalvik_cmd + ['-cp', self._device_classpath, 'Test']
      (output, retc) = self._test_env.RunCommand(
          cmd, {'ANDROID_LOG_TAGS': '*:s'})
      with open(self.output_file, 'w') as run_out:
        run_out.write(output)
    else:
      retc = RetCode.NOTCOMPILED
    return retc

  def GetBisectionSearchArgs(self):
    cmd_str = CommandListToCommandString(
        self._dalvik_cmd + ['-cp',self._device_classpath, 'Test'])
    cmd = ['--raw-cmd={0}'.format(cmd_str), '--timeout', str(30)]
    if self._device:
      cmd += ['--device-serial', self._device]
    else:
      cmd.append('--device')
    return cmd


class TestRunnerArtIntOnTarget(TestRunnerArtOnTarget):
  """Concrete test runner of interpreter mode Art on target."""

  def  __init__(self, dexer, debug_info, device):
    """Constructor for the Art on target tester (interpreter).

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
      device: string, target device serial number (or None)
    """
    super().__init__(dexer, debug_info, device, ['-Xint'])

  @property
  def description(self):
    return 'Art interpreter on target'

  @property
  def id(self):
    return 'TInt'

  def GetBisectionSearchArgs(self):
    return None


class TestRunnerArtOptOnTarget(TestRunnerArtOnTarget):
  """Concrete test runner of optimizing compiler mode Art on target."""

  def  __init__(self, dexer, debug_info, device):
    """Constructor for the Art on target tester (optimizing).

    Args:
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
      device: string, target device serial number (or None)
    """
    super().__init__(dexer, debug_info, device, None)

  @property
  def description(self):
    return 'Art optimizing on target'

  @property
  def id(self):
    return 'TOpt'

  def GetBisectionSearchArgs(self):
    cmd_str = CommandListToCommandString(
        self._dalvik_cmd + ['-cp', self._device_classpath, 'Test'])
    cmd = ['--raw-cmd={0}'.format(cmd_str), '--timeout', str(30)]
    if self._device:
      cmd += ['--device-serial', self._device]
    else:
      cmd.append('--device')
    return cmd


#
# Tester class.
#


class JFuzzTester(object):
  """Tester that runs JFuzz many times and report divergences."""

  def  __init__(self, num_tests, device, mode1, mode2, jfuzz_args,
                report_script, true_divergence_only, dexer, debug_info):
    """Constructor for the tester.

    Args:
      num_tests: int, number of tests to run
      device: string, target device serial number (or None)
      mode1: string, execution mode for first runner
      mode2: string, execution mode for second runner
      jfuzz_args: list of strings, additional arguments for jfuzz
      report_script: string, path to script called for each divergence
      true_divergence_only: boolean, if True don't bisect timeout divergences
      dexer: string, defines dexer
      debug_info: boolean, if True include debugging info
    """
    self._num_tests = num_tests
    self._device = device
    self._runner1 = GetExecutionModeRunner(dexer, debug_info, device, mode1)
    self._runner2 = GetExecutionModeRunner(dexer, debug_info, device, mode2)
    self._jfuzz_args = jfuzz_args
    self._report_script = report_script
    self._true_divergence_only = true_divergence_only
    self._dexer = dexer
    self._debug_info = debug_info
    self._save_dir = None
    self._results_dir = None
    self._jfuzz_dir = None
    # Statistics.
    self._test = 0
    self._num_success = 0
    self._num_not_compiled = 0
    self._num_not_run = 0
    self._num_timed_out = 0
    self._num_divergences = 0

  def __enter__(self):
    """On entry, enters new temp directory after saving current directory.

    Raises:
      FatalError: error when temp directory cannot be constructed
    """
    self._save_dir = os.getcwd()
    self._results_dir = mkdtemp(dir='/tmp/')
    self._jfuzz_dir = mkdtemp(dir=self._results_dir)
    if self._results_dir is None or self._jfuzz_dir is None:
      raise FatalError('Cannot obtain temp directory')
    os.chdir(self._jfuzz_dir)
    return self

  def __exit__(self, etype, evalue, etraceback):
    """On exit, re-enters previously saved current directory and cleans up."""
    os.chdir(self._save_dir)
    shutil.rmtree(self._jfuzz_dir)
    if self._num_divergences == 0:
      shutil.rmtree(self._results_dir)

  def Run(self):
    """Runs JFuzz many times and report divergences."""
    print()
    print('**\n**** JFuzz Testing\n**')
    print()
    print('#Tests    :', self._num_tests)
    print('Device    :', self._device)
    print('Directory :', self._results_dir)
    print('Exec-mode1:', self._runner1.description)
    print('Exec-mode2:', self._runner2.description)
    print('Dexer     :', self._dexer)
    print('Debug-info:', self._debug_info)
    print()
    self.ShowStats()
    for self._test in range(1, self._num_tests + 1):
      self.RunJFuzzTest()
      self.ShowStats()
    if self._num_divergences == 0:
      print('\n\nsuccess (no divergences)\n')
    else:
      print('\n\nfailure (divergences)\n')

  def ShowStats(self):
    """Shows current statistics (on same line) while tester is running."""
    print('\rTests:', self._test,
          'Success:', self._num_success,
          'Not-compiled:', self._num_not_compiled,
          'Not-run:', self._num_not_run,
          'Timed-out:', self._num_timed_out,
          'Divergences:', self._num_divergences,
          end='')
    sys.stdout.flush()

  def RunJFuzzTest(self):
    """Runs a single JFuzz test, comparing two execution modes."""
    self.ConstructTest()
    retc1 = self._runner1.CompileAndRunTest()
    retc2 = self._runner2.CompileAndRunTest()
    self.CheckForDivergence(retc1, retc2)
    self.CleanupTest()

  def ConstructTest(self):
    """Use JFuzz to generate next Test.java test.

    Raises:
      FatalError: error when jfuzz fails
    """
    if (RunCommand(['jfuzz'] + self._jfuzz_args, out='Test.java', err=None)
          != RetCode.SUCCESS):
      raise FatalError('Unexpected error while running JFuzz')

  def CheckForDivergence(self, retc1, retc2):
    """Checks for divergences and updates statistics.

    Args:
      retc1: int, normalized return code of first runner
      retc2: int, normalized return code of second runner
    """
    if retc1 == retc2:
      # No divergence in return code.
      if retc1 == RetCode.SUCCESS:
        # Both compilations and runs were successful, inspect generated output.
        runner1_out = self._runner1.output_file
        runner2_out = self._runner2.output_file
        if not filecmp.cmp(runner1_out, runner2_out, shallow=False):
          # Divergence in output.
          self.ReportDivergence(retc1, retc2, is_output_divergence=True)
        else:
          # No divergence in output.
          self._num_success += 1
      elif retc1 == RetCode.TIMEOUT:
        self._num_timed_out += 1
      elif retc1 == RetCode.NOTCOMPILED:
        self._num_not_compiled += 1
      else:
        self._num_not_run += 1
    else:
      # Divergence in return code.
      if self._true_divergence_only:
        # When only true divergences are requested, any divergence in return
        # code where one is a time out is treated as a regular time out.
        if RetCode.TIMEOUT in (retc1, retc2):
          self._num_timed_out += 1
          return
        # When only true divergences are requested, a runtime crash in just
        # the RI is treated as if not run at all.
        if retc1 == RetCode.ERROR and retc2 == RetCode.SUCCESS:
          if self._runner1.GetBisectionSearchArgs() is None:
            self._num_not_run += 1
            return
      self.ReportDivergence(retc1, retc2, is_output_divergence=False)

  def GetCurrentDivergenceDir(self):
    return self._results_dir + '/divergence' + str(self._num_divergences)

  def ReportDivergence(self, retc1, retc2, is_output_divergence):
    """Reports and saves a divergence."""
    self._num_divergences += 1
    print('\n' + str(self._num_divergences), end='')
    if is_output_divergence:
      print(' divergence in output')
    else:
      print(' divergence in return code: ' + retc1.name + ' vs. ' +
            retc2.name)
    # Save.
    ddir = self.GetCurrentDivergenceDir()
    os.mkdir(ddir)
    for f in glob('*.txt') + ['Test.java']:
      shutil.copy(f, ddir)
    # Maybe run bisection bug search.
    if retc1 in BISECTABLE_RET_CODES and retc2 in BISECTABLE_RET_CODES:
      self.MaybeBisectDivergence(retc1, retc2, is_output_divergence)
    # Call reporting script.
    if self._report_script:
      self.RunReportScript(retc1, retc2, is_output_divergence)

  def RunReportScript(self, retc1, retc2, is_output_divergence):
    """Runs report script."""
    try:
      title = "Divergence between {0} and {1} (found with fuzz testing)".format(
          self._runner1.description, self._runner2.description)
      # Prepare divergence comment.
      jfuzz_cmd_and_version = subprocess.check_output(
          ['grep', '-o', 'jfuzz.*', 'Test.java'], universal_newlines=True)
      (jfuzz_cmd_str, jfuzz_ver) = jfuzz_cmd_and_version.split('(')
      # Strip right parenthesis and new line.
      jfuzz_ver = jfuzz_ver[:-2]
      jfuzz_args = ['\'-{0}\''.format(arg)
                    for arg in jfuzz_cmd_str.strip().split(' -')][1:]
      wrapped_args = ['--jfuzz_arg={0}'.format(opt) for opt in jfuzz_args]
      repro_cmd_str = (os.path.basename(__file__) +
                       ' --num_tests=1 --dexer=' + self._dexer +
                       (' --debug_info ' if self._debug_info else ' ') +
                       ' '.join(wrapped_args))
      comment = 'jfuzz {0}\nReproduce test:\n{1}\nReproduce divergence:\n{2}\n'.format(
          jfuzz_ver, jfuzz_cmd_str, repro_cmd_str)
      if is_output_divergence:
        (output, _, _) = RunCommandForOutput(
            ['diff', self._runner1.output_file, self._runner2.output_file],
            None, subprocess.PIPE, subprocess.STDOUT)
        comment += 'Diff:\n' + output
      else:
        comment += '{0} vs {1}\n'.format(retc1, retc2)
      # Prepare report script command.
      script_cmd = [self._report_script, title, comment]
      ddir = self.GetCurrentDivergenceDir()
      bisection_out_files = glob(ddir + '/*_bisection_out.txt')
      if bisection_out_files:
        script_cmd += ['--bisection_out', bisection_out_files[0]]
      subprocess.check_call(script_cmd, stdout=DEVNULL, stderr=DEVNULL)
    except subprocess.CalledProcessError as err:
      print('Failed to run report script.\n', err)

  def RunBisectionSearch(self, args, expected_retcode, expected_output,
                         runner_id):
    ddir = self.GetCurrentDivergenceDir()
    outfile_path = ddir + '/' + runner_id + '_bisection_out.txt'
    logfile_path = ddir + '/' + runner_id + '_bisection_log.txt'
    errfile_path = ddir + '/' + runner_id + '_bisection_err.txt'
    args = list(args) + ['--logfile', logfile_path, '--cleanup']
    args += ['--expected-retcode', expected_retcode.name]
    if expected_output:
      args += ['--expected-output', expected_output]
    bisection_search_path = os.path.join(
        GetEnvVariableOrError('ANDROID_BUILD_TOP'),
        'art/tools/bisection_search/bisection_search.py')
    if RunCommand([bisection_search_path] + args, out=outfile_path,
                  err=errfile_path, timeout=300) == RetCode.TIMEOUT:
      print('Bisection search TIMEOUT')

  def MaybeBisectDivergence(self, retc1, retc2, is_output_divergence):
    bisection_args1 = self._runner1.GetBisectionSearchArgs()
    bisection_args2 = self._runner2.GetBisectionSearchArgs()
    if is_output_divergence:
      maybe_output1 = self._runner1.output_file
      maybe_output2 = self._runner2.output_file
    else:
      maybe_output1 = maybe_output2 = None
    if bisection_args1 is not None:
      self.RunBisectionSearch(bisection_args1, retc2, maybe_output2,
                              self._runner1.id)
    if bisection_args2 is not None:
      self.RunBisectionSearch(bisection_args2, retc1, maybe_output1,
                              self._runner2.id)

  def CleanupTest(self):
    """Cleans up after a single test run."""
    for file_name in os.listdir(self._jfuzz_dir):
      file_path = os.path.join(self._jfuzz_dir, file_name)
      if os.path.isfile(file_path):
        os.unlink(file_path)
      elif os.path.isdir(file_path):
        shutil.rmtree(file_path)


def main():
  # Handle arguments.
  parser = argparse.ArgumentParser()
  parser.add_argument('--num_tests', default=10000, type=int,
                      help='number of tests to run')
  parser.add_argument('--device', help='target device serial number')
  parser.add_argument('--mode1', default='ri',
                      help='execution mode 1 (default: ri)')
  parser.add_argument('--mode2', default='hopt',
                      help='execution mode 2 (default: hopt)')
  parser.add_argument('--report_script',
                      help='script called for each divergence')
  parser.add_argument('--jfuzz_arg', default=[], dest='jfuzz_args',
                      action='append',
                      help='argument for jfuzz')
  parser.add_argument('--true_divergence', default=False, action='store_true',
                      help='do not bisect timeout divergences')
  parser.add_argument('--dexer', default='dx', type=str,
                      help='defines dexer as dx, d8, or jack (default: dx)')
  parser.add_argument('--debug_info', default=False, action='store_true',
                      help='include debugging info')
  args = parser.parse_args()
  if args.mode1 == args.mode2:
    raise FatalError('Identical execution modes given')
  # Run the JFuzz tester.
  with JFuzzTester(args.num_tests,
                   args.device,
                   args.mode1, args.mode2,
                   args.jfuzz_args,
                   args.report_script,
                   args.true_divergence,
                   args.dexer,
                   args.debug_info) as fuzzer:
    fuzzer.Run()

if __name__ == '__main__':
  main()
