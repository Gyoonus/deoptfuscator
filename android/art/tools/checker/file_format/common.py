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

def SplitStream(stream, fnProcessLine, fnLineOutsideChunk):
  """ Reads the given input stream and splits it into chunks based on
      information extracted from individual lines.

  Arguments:
   - fnProcessLine: Called on each line with the text and line number. Must
     return a triplet, composed of the name of the chunk started on this line,
     the data extracted, and the name of the architecture this test applies to
     (or None to indicate that all architectures should run this test).
   - fnLineOutsideChunk: Called on attempt to attach data prior to creating
     a chunk.
  """
  lineNo = 0
  allChunks = []
  currentChunk = None

  for line in stream:
    lineNo += 1
    line = line.strip()
    if not line:
      continue

    # Let the child class process the line and return information about it.
    # The _processLine method can modify the content of the line (or delete it
    # entirely) and specify whether it starts a new group.
    processedLine, newChunkName, testArch = fnProcessLine(line, lineNo)
    # Currently, only a full chunk can be specified as architecture-specific.
    assert testArch is None or newChunkName is not None
    if newChunkName is not None:
      currentChunk = (newChunkName, [], lineNo, testArch)
      allChunks.append(currentChunk)
    if processedLine is not None:
      if currentChunk is not None:
        currentChunk[1].append(processedLine)
      else:
        fnLineOutsideChunk(line, lineNo)
  return allChunks
