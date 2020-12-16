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

from common.logger                   import Logger
from file_format.common              import SplitStream
from file_format.c1visualizer.struct import C1visualizerFile, C1visualizerPass

import re

class C1ParserState:
  OutsideBlock, InsideCompilationBlock, StartingCfgBlock, InsideCfgBlock = range(4)

  def __init__(self):
    self.currentState = C1ParserState.OutsideBlock
    self.lastMethodName = None

def __parseC1Line(line, lineNo, state, fileName):
  """ This function is invoked on each line of the output file and returns
      a triplet which instructs the parser how the line should be handled. If the
      line is to be included in the current group, it is returned in the first
      value. If the line starts a new output group, the name of the group is
      returned in the second value. The third value is only here to make the
      function prototype compatible with `SplitStream` and is always set to
      `None` here.
  """
  if state.currentState == C1ParserState.StartingCfgBlock:
    # Previous line started a new 'cfg' block which means that this one must
    # contain the name of the pass (this is enforced by C1visualizer).
    if re.match("name\s+\"[^\"]+\"", line):
      # Extract the pass name, prepend it with the name of the method and
      # return as the beginning of a new group.
      state.currentState = C1ParserState.InsideCfgBlock
      return (None, state.lastMethodName + " " + line.split("\"")[1], None)
    else:
      Logger.fail("Expected output group name", fileName, lineNo)

  elif state.currentState == C1ParserState.InsideCfgBlock:
    if line == "end_cfg":
      state.currentState = C1ParserState.OutsideBlock
      return (None, None, None)
    else:
      return (line, None, None)

  elif state.currentState == C1ParserState.InsideCompilationBlock:
    # Search for the method's name. Format: method "<name>"
    if re.match("method\s+\"[^\"]*\"", line):
      methodName = line.split("\"")[1].strip()
      if not methodName:
        Logger.fail("Empty method name in output", fileName, lineNo)
      state.lastMethodName = methodName
    elif line == "end_compilation":
      state.currentState = C1ParserState.OutsideBlock
    return (None, None, None)

  else:
    assert state.currentState == C1ParserState.OutsideBlock
    if line == "begin_cfg":
      # The line starts a new group but we'll wait until the next line from
      # which we can extract the name of the pass.
      if state.lastMethodName is None:
        Logger.fail("Expected method header", fileName, lineNo)
      state.currentState = C1ParserState.StartingCfgBlock
      return (None, None, None)
    elif line == "begin_compilation":
      state.currentState = C1ParserState.InsideCompilationBlock
      return (None, None, None)
    else:
      Logger.fail("C1visualizer line not inside a group", fileName, lineNo)

def ParseC1visualizerStream(fileName, stream):
  c1File = C1visualizerFile(fileName)
  state = C1ParserState()
  fnProcessLine = lambda line, lineNo: __parseC1Line(line, lineNo, state, fileName)
  fnLineOutsideChunk = lambda line, lineNo: \
      Logger.fail("C1visualizer line not inside a group", fileName, lineNo)
  for passName, passLines, startLineNo, testArch in \
      SplitStream(stream, fnProcessLine, fnLineOutsideChunk):
    C1visualizerPass(c1File, passName, passLines, startLineNo + 1)
  return c1File
