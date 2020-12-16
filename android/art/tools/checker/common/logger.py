# Copyright (C) 2014 The Android Open Source Project
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

from __future__ import print_function
import collections
import sys

class Logger(object):

  class Level(object):
    NoOutput, Error, Info = range(3)

  class Color(object):
    Default, Blue, Gray, Purple, Red, Green = range(6)

    @staticmethod
    def terminalCode(color, out=sys.stdout):
      if not out.isatty():
        return ''
      elif color == Logger.Color.Blue:
        return '\033[94m'
      elif color == Logger.Color.Gray:
        return '\033[37m'
      elif color == Logger.Color.Purple:
        return '\033[95m'
      elif color == Logger.Color.Red:
        return '\033[91m'
      elif color == Logger.Color.Green:
        return '\033[32m'
      else:
        return '\033[0m'

  Verbosity = Level.Info

  @staticmethod
  def log(text, level=Level.Info, color=Color.Default, newLine=True, out=sys.stdout):
    if level <= Logger.Verbosity:
      text = Logger.Color.terminalCode(color, out) + text + \
             Logger.Color.terminalCode(Logger.Color.Default, out)
      if newLine:
        print(text, file=out)
      else:
        print(text, end="", file=out)
      out.flush()

  @staticmethod
  def fail(msg, file=None, line=-1, lineText=None, variables=None):
    Logger.log("error: ", Logger.Level.Error, color=Logger.Color.Red, newLine=False, out=sys.stderr)
    Logger.log(msg, Logger.Level.Error, out=sys.stderr)

    if lineText:
      loc = ""
      if file:
        loc += file + ":"
      if line > 0:
        loc += str(line) + ":"
      if loc:
        loc += " "
      Logger.log(loc, Logger.Level.Error, color=Logger.Color.Gray, newLine=False, out=sys.stderr)
      Logger.log(lineText, Logger.Level.Error, out=sys.stderr)

    if variables:
      longestName = 0
      for var in variables:
        longestName = max(longestName, len(var))

      for var in collections.OrderedDict(sorted(variables.items())):
        padding = ' ' * (longestName - len(var))
        Logger.log(var, Logger.Level.Error, color=Logger.Color.Green, newLine=False, out=sys.stderr)
        Logger.log(padding, Logger.Level.Error, newLine=False, out=sys.stderr)
        Logger.log(" = ", Logger.Level.Error, newLine=False, out=sys.stderr)
        Logger.log(variables[var], Logger.Level.Error, out=sys.stderr)

    sys.exit(1)

  @staticmethod
  def startTest(name):
    Logger.log("TEST ", color=Logger.Color.Purple, newLine=False)
    Logger.log(name + "... ", newLine=False)

  @staticmethod
  def testPassed():
    Logger.log("PASS", color=Logger.Color.Blue)

  @staticmethod
  def testFailed(msg, assertion, variables):
    Logger.log("FAIL", color=Logger.Color.Red)
    Logger.fail(msg, assertion.fileName, assertion.lineNo, assertion.originalText, variables)
