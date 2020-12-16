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

from common.logger import Logger
from common.mixins import EqualityMixin, PrintableMixin

import re

class CheckerFile(PrintableMixin):

  def __init__(self, fileName):
    self.fileName = fileName
    self.testCases = []

  def addTestCase(self, new_test_case):
    self.testCases.append(new_test_case)

  def testCasesForArch(self, targetArch):
    return [t for t in self.testCases if t.testArch == targetArch]

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.testCases == other.testCases


class TestCase(PrintableMixin):

  def __init__(self, parent, name, startLineNo, testArch = None, forDebuggable = False):
    assert isinstance(parent, CheckerFile)

    self.parent = parent
    self.name = name
    self.assertions = []
    self.startLineNo = startLineNo
    self.testArch = testArch
    self.forDebuggable = forDebuggable

    if not self.name:
      Logger.fail("Test case does not have a name", self.fileName, self.startLineNo)

    self.parent.addTestCase(self)

  @property
  def fileName(self):
    return self.parent.fileName

  def addAssertion(self, new_assertion):
    if new_assertion.variant == TestAssertion.Variant.NextLine:
      if not self.assertions or \
         (self.assertions[-1].variant != TestAssertion.Variant.InOrder and \
          self.assertions[-1].variant != TestAssertion.Variant.NextLine):
        Logger.fail("A next-line assertion can only be placed after an "
                    "in-order assertion or another next-line assertion.",
                    new_assertion.fileName, new_assertion.lineNo)
    self.assertions.append(new_assertion)

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.name == other.name \
       and self.assertions == other.assertions


class TestAssertion(PrintableMixin):

  class Variant(object):
    """Supported types of assertions."""
    InOrder, NextLine, DAG, Not, Eval = range(5)

  def __init__(self, parent, variant, originalText, lineNo):
    assert isinstance(parent, TestCase)

    self.parent = parent
    self.variant = variant
    self.expressions = []
    self.lineNo = lineNo
    self.originalText = originalText

    self.parent.addAssertion(self)

  @property
  def fileName(self):
    return self.parent.fileName

  def addExpression(self, new_expression):
    assert isinstance(new_expression, TestExpression)
    if self.variant == TestAssertion.Variant.Not:
      if new_expression.variant == TestExpression.Variant.VarDef:
        Logger.fail("CHECK-NOT lines cannot define variables", self.fileName, self.lineNo)
    self.expressions.append(new_expression)

  def toRegex(self):
    """ Returns a regex pattern for this entire assertion. Only used in tests. """
    regex = ""
    for expression in self.expressions:
      if expression.variant == TestExpression.Variant.Separator:
        regex = regex + ", "
      else:
        regex = regex + "(" + expression.text + ")"
    return regex

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.variant == other.variant \
       and self.expressions == other.expressions


class TestExpression(EqualityMixin, PrintableMixin):

  class Variant(object):
    """Supported language constructs."""
    PlainText, Pattern, VarRef, VarDef, Separator = range(5)

  class Regex(object):
    rName = r"([a-zA-Z][a-zA-Z0-9]*)"
    rRegex = r"(.+?)"
    rPatternStartSym = r"(\{\{)"
    rPatternEndSym = r"(\}\})"
    rVariableStartSym = r"(<<)"
    rVariableEndSym = r"(>>)"
    rVariableSeparator = r"(:)"
    rVariableDefinitionBody = rName + rVariableSeparator + rRegex

    regexPattern = rPatternStartSym + rRegex + rPatternEndSym
    regexVariableReference = rVariableStartSym + rName + rVariableEndSym
    regexVariableDefinition = rVariableStartSym + rVariableDefinitionBody + rVariableEndSym

  def __init__(self, variant, name, text):
    self.variant = variant
    self.name = name
    self.text = text

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.variant == other.variant \
       and self.name == other.name \
       and self.text == other.text

  @staticmethod
  def createSeparator():
    return TestExpression(TestExpression.Variant.Separator, None, None)

  @staticmethod
  def createPlainText(text):
    return TestExpression(TestExpression.Variant.PlainText, None, text)

  @staticmethod
  def createPatternFromPlainText(text):
    return TestExpression(TestExpression.Variant.Pattern, None, re.escape(text))

  @staticmethod
  def createPattern(pattern):
    return TestExpression(TestExpression.Variant.Pattern, None, pattern)

  @staticmethod
  def createVariableReference(name):
    assert re.match(TestExpression.Regex.rName, name)
    return TestExpression(TestExpression.Variant.VarRef, name, None)

  @staticmethod
  def createVariableDefinition(name, pattern):
    assert re.match(TestExpression.Regex.rName, name)
    return TestExpression(TestExpression.Variant.VarDef, name, pattern)
