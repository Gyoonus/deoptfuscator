#!/usr/bin/env python2
#
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

import argparse
import os

from common.archs                     import archs_list
from common.logger                    import Logger
from file_format.c1visualizer.parser  import ParseC1visualizerStream
from file_format.checker.parser       import ParseCheckerStream
from match.file                       import MatchFiles

def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument("tested_file",
                      help="text file the checks should be verified against")
  parser.add_argument("source_path", nargs="?",
                      help="path to file/folder with checking annotations")
  parser.add_argument("--check-prefix", dest="check_prefix", default="CHECK", metavar="PREFIX",
                      help="prefix of checks in the test files (default: CHECK)")
  parser.add_argument("--list-passes", dest="list_passes", action="store_true",
                      help="print a list of all passes found in the tested file")
  parser.add_argument("--dump-pass", dest="dump_pass", metavar="PASS",
                      help="print a compiler pass dump")
  parser.add_argument("--arch", dest="arch", choices=archs_list,
                      help="Run tests for the specified target architecture.")
  parser.add_argument("--debuggable", action="store_true",
                      help="Run tests for debuggable code.")
  parser.add_argument("-q", "--quiet", action="store_true",
                      help="print only errors")
  return parser.parse_args()


def ListPasses(outputFilename):
  c1File = ParseC1visualizerStream(os.path.basename(outputFilename), open(outputFilename, "r"))
  for compiler_pass in c1File.passes:
    Logger.log(compiler_pass.name)


def DumpPass(outputFilename, passName):
  c1File = ParseC1visualizerStream(os.path.basename(outputFilename), open(outputFilename, "r"))
  compiler_pass = c1File.findPass(passName)
  if compiler_pass:
    maxLineNo = compiler_pass.startLineNo + len(compiler_pass.body)
    lenLineNo = len(str(maxLineNo)) + 2
    curLineNo = compiler_pass.startLineNo
    for line in compiler_pass.body:
      Logger.log((str(curLineNo) + ":").ljust(lenLineNo) + line)
      curLineNo += 1
  else:
    Logger.fail("Pass \"" + passName + "\" not found in the output")


def FindCheckerFiles(path):
  """ Returns a list of files to scan for check annotations in the given path.
      Path to a file is returned as a single-element list, directories are
      recursively traversed and all '.java' and '.smali' files returned.
  """
  if not path:
    Logger.fail("No source path provided")
  elif os.path.isfile(path):
    return [ path ]
  elif os.path.isdir(path):
    foundFiles = []
    for root, dirs, files in os.walk(path):
      for file in files:
        extension = os.path.splitext(file)[1]
        if extension in [".java", ".smali"]:
          foundFiles.append(os.path.join(root, file))
    return foundFiles
  else:
    Logger.fail("Source path \"" + path + "\" not found")


def RunTests(checkPrefix, checkPath, outputFilename, targetArch, debuggableMode):
  c1File = ParseC1visualizerStream(os.path.basename(outputFilename), open(outputFilename, "r"))
  for checkFilename in FindCheckerFiles(checkPath):
    checkerFile = ParseCheckerStream(os.path.basename(checkFilename),
                                     checkPrefix,
                                     open(checkFilename, "r"),
                                     targetArch)
    MatchFiles(checkerFile, c1File, targetArch, debuggableMode)


if __name__ == "__main__":
  args = ParseArguments()

  if args.quiet:
    Logger.Verbosity = Logger.Level.Error

  if args.list_passes:
    ListPasses(args.tested_file)
  elif args.dump_pass:
    DumpPass(args.tested_file, args.dump_pass)
  else:
    RunTests(args.check_prefix, args.source_path, args.tested_file, args.arch, args.debuggable)
