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

from common.archs               import archs_list
from common.logger              import Logger
from file_format.common         import SplitStream
from file_format.checker.struct import CheckerFile, TestCase, TestAssertion, TestExpression

import re

def __isCheckerLine(line):
  return line.startswith("///") or line.startswith("##")

def __extractLine(prefix, line, arch = None, debuggable = False):
  """ Attempts to parse a check line. The regex searches for a comment symbol
      followed by the CHECK keyword, given attribute and a colon at the very
      beginning of the line. Whitespaces are ignored.
  """
  rIgnoreWhitespace = r"\s*"
  rCommentSymbols = [r"///", r"##"]
  arch_specifier = r"-%s" % arch if arch is not None else r""
  dbg_specifier = r"-DEBUGGABLE" if debuggable else r""
  regexPrefix = rIgnoreWhitespace + \
                r"(" + r"|".join(rCommentSymbols) + r")" + \
                rIgnoreWhitespace + \
                prefix + arch_specifier + dbg_specifier + r":"

  # The 'match' function succeeds only if the pattern is matched at the
  # beginning of the line.
  match = re.match(regexPrefix, line)
  if match is not None:
    return line[match.end():].strip()
  else:
    return None

def __preprocessLineForStart(prefix, line, targetArch):
  """ This function modifies a CHECK-START-{x,y,z} into a matching
      CHECK-START-y line for matching targetArch y. If no matching
      architecture is found, CHECK-START-x is returned arbitrarily
      to ensure all following check lines are put into a test that
      is skipped. Any other line is left unmodified.
  """
  if targetArch is not None:
    if prefix in line:
      # Find { } on the line and assume that defines the set.
      s = line.find('{')
      e = line.find('}')
      if 0 < s and s < e:
        archs = line[s+1:e].split(',')
        # First verify that every archs is valid. Return the
        # full line on failure to prompt error back to user.
        for arch in archs:
          if not arch in archs_list:
            return line
        # Now accept matching arch or arbitrarily return first.
        if targetArch in archs:
          return line[:s] + targetArch + line[e + 1:]
        else:
          return line[:s] + archs[0] + line[e + 1:]
  return line

def __processLine(line, lineNo, prefix, fileName, targetArch):
  """ This function is invoked on each line of the check file and returns a triplet
      which instructs the parser how the line should be handled. If the line is
      to be included in the current check group, it is returned in the first
      value. If the line starts a new check group, the name of the group is
      returned in the second value. The third value indicates whether the line
      contained an architecture-specific suffix.
  """
  if not __isCheckerLine(line):
    return None, None, None

  # Lines beginning with 'CHECK-START' start a new test case.
  # We currently only consider the architecture suffix(es) in "CHECK-START" lines.
  for debuggable in [True, False]:
    sline = __preprocessLineForStart(prefix + "-START", line, targetArch)
    for arch in [None] + archs_list:
      startLine = __extractLine(prefix + "-START", sline, arch, debuggable)
      if startLine is not None:
        return None, startLine, (arch, debuggable)

  # Lines starting only with 'CHECK' are matched in order.
  plainLine = __extractLine(prefix, line)
  if plainLine is not None:
    return (plainLine, TestAssertion.Variant.InOrder, lineNo), None, None

  # 'CHECK-NEXT' lines are in-order but must match the very next line.
  nextLine = __extractLine(prefix + "-NEXT", line)
  if nextLine is not None:
    return (nextLine, TestAssertion.Variant.NextLine, lineNo), None, None

  # 'CHECK-DAG' lines are no-order assertions.
  dagLine = __extractLine(prefix + "-DAG", line)
  if dagLine is not None:
    return (dagLine, TestAssertion.Variant.DAG, lineNo), None, None

  # 'CHECK-NOT' lines are no-order negative assertions.
  notLine = __extractLine(prefix + "-NOT", line)
  if notLine is not None:
    return (notLine, TestAssertion.Variant.Not, lineNo), None, None

  # 'CHECK-EVAL' lines evaluate a Python expression.
  evalLine = __extractLine(prefix + "-EVAL", line)
  if evalLine is not None:
    return (evalLine, TestAssertion.Variant.Eval, lineNo), None, None

  Logger.fail("Checker assertion could not be parsed: '" + line + "'", fileName, lineNo)

def __isMatchAtStart(match):
  """ Tests if the given Match occurred at the beginning of the line. """
  return (match is not None) and (match.start() == 0)

def __firstMatch(matches, string):
  """ Takes in a list of Match objects and returns the minimal start point among
      them. If there aren't any successful matches it returns the length of
      the searched string.
  """
  starts = map(lambda m: len(string) if m is None else m.start(), matches)
  return min(starts)

def ParseCheckerAssertion(parent, line, variant, lineNo):
  """ This method parses the content of a check line stripped of the initial
      comment symbol and the CHECK-* keyword.
  """
  assertion = TestAssertion(parent, variant, line, lineNo)
  isEvalLine = (variant == TestAssertion.Variant.Eval)

  # Loop as long as there is something to parse.
  while line:
    # Search for the nearest occurrence of the special markers.
    if isEvalLine:
      # The following constructs are not supported in CHECK-EVAL lines
      matchWhitespace = None
      matchPattern = None
      matchVariableDefinition = None
    else:
      matchWhitespace = re.search(r"\s+", line)
      matchPattern = re.search(TestExpression.Regex.regexPattern, line)
      matchVariableDefinition = re.search(TestExpression.Regex.regexVariableDefinition, line)
    matchVariableReference = re.search(TestExpression.Regex.regexVariableReference, line)

    # If one of the above was identified at the current position, extract them
    # from the line, parse them and add to the list of line parts.
    if __isMatchAtStart(matchWhitespace):
      # A whitespace in the check line creates a new separator of line parts.
      # This allows for ignored output between the previous and next parts.
      line = line[matchWhitespace.end():]
      assertion.addExpression(TestExpression.createSeparator())
    elif __isMatchAtStart(matchPattern):
      pattern = line[0:matchPattern.end()]
      pattern = pattern[2:-2]
      line = line[matchPattern.end():]
      assertion.addExpression(TestExpression.createPattern(pattern))
    elif __isMatchAtStart(matchVariableReference):
      var = line[0:matchVariableReference.end()]
      line = line[matchVariableReference.end():]
      name = var[2:-2]
      assertion.addExpression(TestExpression.createVariableReference(name))
    elif __isMatchAtStart(matchVariableDefinition):
      var = line[0:matchVariableDefinition.end()]
      line = line[matchVariableDefinition.end():]
      colonPos = var.find(":")
      name = var[2:colonPos]
      body = var[colonPos+1:-2]
      assertion.addExpression(TestExpression.createVariableDefinition(name, body))
    else:
      # If we're not currently looking at a special marker, this is a plain
      # text match all the way until the first special marker (or the end
      # of the line).
      firstMatch = __firstMatch([ matchWhitespace,
                                  matchPattern,
                                  matchVariableReference,
                                  matchVariableDefinition ],
                                line)
      text = line[0:firstMatch]
      line = line[firstMatch:]
      if isEvalLine:
        assertion.addExpression(TestExpression.createPlainText(text))
      else:
        assertion.addExpression(TestExpression.createPatternFromPlainText(text))
  return assertion

def ParseCheckerStream(fileName, prefix, stream, targetArch = None):
  checkerFile = CheckerFile(fileName)
  fnProcessLine = lambda line, lineNo: __processLine(line, lineNo, prefix, fileName, targetArch)
  fnLineOutsideChunk = lambda line, lineNo: \
      Logger.fail("Checker line not inside a group", fileName, lineNo)
  for caseName, caseLines, startLineNo, testData in \
      SplitStream(stream, fnProcessLine, fnLineOutsideChunk):
    testArch = testData[0]
    forDebuggable = testData[1]
    testCase = TestCase(checkerFile, caseName, startLineNo, testArch, forDebuggable)
    for caseLine in caseLines:
      ParseCheckerAssertion(testCase, caseLine[0], caseLine[1], caseLine[2])
  return checkerFile
