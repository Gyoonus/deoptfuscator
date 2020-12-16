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

from collections                      import namedtuple
from common.immutables                import ImmutableDict
from common.logger                    import Logger
from file_format.c1visualizer.struct  import C1visualizerFile, C1visualizerPass
from file_format.checker.struct       import CheckerFile, TestCase, TestAssertion
from match.line                       import MatchLines, EvaluateLine

MatchScope = namedtuple("MatchScope", ["start", "end"])
MatchInfo = namedtuple("MatchInfo", ["scope", "variables"])

class MatchFailedException(Exception):
  def __init__(self, assertion, lineNo, variables):
    self.assertion = assertion
    self.lineNo = lineNo
    self.variables = variables

def splitIntoGroups(assertions):
  """ Breaks up a list of assertions, grouping instructions which should be
      tested in the same scope (consecutive DAG and NOT instructions).
   """
  splitAssertions = []
  lastVariant = None
  for assertion in assertions:
    if (assertion.variant == lastVariant and
        assertion.variant in [TestAssertion.Variant.DAG, TestAssertion.Variant.Not]):
      splitAssertions[-1].append(assertion)
    else:
      splitAssertions.append([assertion])
      lastVariant = assertion.variant
  return splitAssertions

def findMatchingLine(assertion, c1Pass, scope, variables, excludeLines=[]):
  """ Finds the first line in `c1Pass` which matches `assertion`.

  Scan only lines numbered between `scope.start` and `scope.end` and not on the
  `excludeLines` list.

  Returns the index of the `c1Pass` line matching the assertion and variables
  values after the match.

  Raises MatchFailedException if no such `c1Pass` line can be found.
  """
  for i in range(scope.start, scope.end):
    if i in excludeLines: continue
    newVariables = MatchLines(assertion, c1Pass.body[i], variables)
    if newVariables is not None:
      return MatchInfo(MatchScope(i, i), newVariables)
  raise MatchFailedException(assertion, scope.start, variables)

def matchDagGroup(assertions, c1Pass, scope, variables):
  """ Attempts to find matching `c1Pass` lines for a group of DAG assertions.

  Assertions are matched in the list order and variable values propagated. Only
  lines in `scope` are scanned and each line can only match one assertion.

  Returns the range of `c1Pass` lines covered by this group (min/max of matching
  line numbers) and the variable values after the match of the last assertion.

  Raises MatchFailedException when an assertion cannot be satisfied.
  """
  matchedLines = []
  for assertion in assertions:
    assert assertion.variant == TestAssertion.Variant.DAG
    match = findMatchingLine(assertion, c1Pass, scope, variables, matchedLines)
    variables = match.variables
    assert match.scope.start == match.scope.end
    assert match.scope.start not in matchedLines
    matchedLines.append(match.scope.start)
  return MatchInfo(MatchScope(min(matchedLines), max(matchedLines)), variables)

def testNotGroup(assertions, c1Pass, scope, variables):
  """ Verifies that none of the given NOT assertions matches a line inside
      the given `scope` of `c1Pass` lines.

  Raises MatchFailedException if an assertion matches a line in the scope.
  """
  for i in range(scope.start, scope.end):
    line = c1Pass.body[i]
    for assertion in assertions:
      assert assertion.variant == TestAssertion.Variant.Not
      if MatchLines(assertion, line, variables) is not None:
        raise MatchFailedException(assertion, i, variables)

def testEvalGroup(assertions, scope, variables):
  for assertion in assertions:
    if not EvaluateLine(assertion, variables):
      raise MatchFailedException(assertion, scope.start, variables)

def MatchTestCase(testCase, c1Pass):
  """ Runs a test case against a C1visualizer graph dump.

  Raises MatchFailedException when an assertion cannot be satisfied.
  """
  assert testCase.name == c1Pass.name

  matchFrom = 0
  variables = ImmutableDict()
  c1Length = len(c1Pass.body)

  # NOT assertions are verified retrospectively, once the scope is known.
  pendingNotAssertions = None

  # Prepare assertions by grouping those that are verified in the same scope.
  # We also add None as an EOF assertion that will set scope for NOTs.
  assertionGroups = splitIntoGroups(testCase.assertions)
  assertionGroups.append(None)

  for assertionGroup in assertionGroups:
    if assertionGroup is None:
      # EOF marker always matches the last+1 line of c1Pass.
      match = MatchInfo(MatchScope(c1Length, c1Length), None)
    elif assertionGroup[0].variant == TestAssertion.Variant.Not:
      # NOT assertions will be tested together with the next group.
      assert not pendingNotAssertions
      pendingNotAssertions = assertionGroup
      continue
    elif assertionGroup[0].variant == TestAssertion.Variant.InOrder:
      # Single in-order assertion. Find the first line that matches.
      assert len(assertionGroup) == 1
      scope = MatchScope(matchFrom, c1Length)
      match = findMatchingLine(assertionGroup[0], c1Pass, scope, variables)
    elif assertionGroup[0].variant == TestAssertion.Variant.NextLine:
      # Single next-line assertion. Test if the current line matches.
      assert len(assertionGroup) == 1
      scope = MatchScope(matchFrom, matchFrom + 1)
      match = findMatchingLine(assertionGroup[0], c1Pass, scope, variables)
    elif assertionGroup[0].variant == TestAssertion.Variant.DAG:
      # A group of DAG assertions. Match them all starting from the same point.
      scope = MatchScope(matchFrom, c1Length)
      match = matchDagGroup(assertionGroup, c1Pass, scope, variables)
    else:
      assert assertionGroup[0].variant == TestAssertion.Variant.Eval
      scope = MatchScope(matchFrom, c1Length)
      testEvalGroup(assertionGroup, scope, variables)
      continue

    if pendingNotAssertions:
      # Previous group were NOT assertions. Make sure they don't match any lines
      # in the [matchFrom, match.start) scope.
      scope = MatchScope(matchFrom, match.scope.start)
      testNotGroup(pendingNotAssertions, c1Pass, scope, variables)
      pendingNotAssertions = None

    # Update state.
    assert matchFrom <= match.scope.end
    matchFrom = match.scope.end + 1
    variables = match.variables

def MatchFiles(checkerFile, c1File, targetArch, debuggableMode):
  for testCase in checkerFile.testCases:
    if testCase.testArch not in [None, targetArch]:
      continue
    if testCase.forDebuggable != debuggableMode:
      continue

    # TODO: Currently does not handle multiple occurrences of the same group
    # name, e.g. when a pass is run multiple times. It will always try to
    # match a check group against the first output group of the same name.
    c1Pass = c1File.findPass(testCase.name)
    if c1Pass is None:
      Logger.fail("Test case not found in the CFG file",
                  testCase.fileName, testCase.startLineNo, testCase.name)

    Logger.startTest(testCase.name)
    try:
      MatchTestCase(testCase, c1Pass)
      Logger.testPassed()
    except MatchFailedException as e:
      lineNo = c1Pass.startLineNo + e.lineNo
      if e.assertion.variant == TestAssertion.Variant.Not:
        msg = "NOT assertion matched line {}"
      else:
        msg = "Assertion could not be matched starting from line {}"
      msg = msg.format(lineNo)
      Logger.testFailed(msg, e.assertion, e.variables)
