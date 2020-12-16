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

from common.archs               import archs_list
from common.testing             import ToUnicode
from file_format.checker.parser import ParseCheckerStream
from file_format.checker.struct import CheckerFile, TestCase, TestAssertion, TestExpression

import io
import unittest

CheckerException = SystemExit

class CheckerParser_PrefixTest(unittest.TestCase):

  def tryParse(self, string):
    checkerText = u"/// CHECK-START: pass\n" + ToUnicode(string)
    return ParseCheckerStream("<test-file>", "CHECK", io.StringIO(checkerText))

  def assertParses(self, string):
    checkFile = self.tryParse(string)
    self.assertEqual(len(checkFile.testCases), 1)
    self.assertNotEqual(len(checkFile.testCases[0].assertions), 0)

  def assertIgnored(self, string):
    checkFile = self.tryParse(string)
    self.assertEqual(len(checkFile.testCases), 1)
    self.assertEqual(len(checkFile.testCases[0].assertions), 0)

  def assertInvalid(self, string):
    with self.assertRaises(CheckerException):
      self.tryParse(string)

  def test_ValidFormat(self):
    self.assertParses("///CHECK:foo")
    self.assertParses("##CHECK:bar")

  def test_InvalidFormat(self):
    self.assertIgnored("CHECK")
    self.assertIgnored(":CHECK")
    self.assertIgnored("CHECK:")
    self.assertIgnored("//CHECK")
    self.assertIgnored("#CHECK")
    self.assertInvalid("///CHECK")
    self.assertInvalid("##CHECK")

  def test_InvalidPrefix(self):
    self.assertInvalid("///ACHECK:foo")
    self.assertInvalid("##ACHECK:foo")

  def test_NotFirstOnTheLine(self):
    self.assertIgnored("A/// CHECK: foo")
    self.assertIgnored("A # CHECK: foo")
    self.assertInvalid("/// /// CHECK: foo")
    self.assertInvalid("## ## CHECK: foo")

  def test_WhitespaceAgnostic(self):
    self.assertParses("  ///CHECK: foo")
    self.assertParses("///  CHECK: foo")
    self.assertParses("    ///CHECK: foo")
    self.assertParses("///    CHECK: foo")

class CheckerParser_TestExpressionTest(unittest.TestCase):

  def parseAssertion(self, string, variant=""):
    checkerText = (u"/// CHECK-START: pass\n" +
                   u"/// CHECK" + ToUnicode(variant) + u": " + ToUnicode(string))
    checkerFile = ParseCheckerStream("<test-file>", "CHECK", io.StringIO(checkerText))
    self.assertEqual(len(checkerFile.testCases), 1)
    testCase = checkerFile.testCases[0]
    self.assertEqual(len(testCase.assertions), 1)
    return testCase.assertions[0]

  def parseExpression(self, string):
    line = self.parseAssertion(string)
    self.assertEqual(1, len(line.expressions))
    return line.expressions[0]

  def assertEqualsRegex(self, string, expected):
    self.assertEqual(expected, self.parseAssertion(string).toRegex())

  def assertEqualsText(self, string, text):
    self.assertEqual(self.parseExpression(string), TestExpression.createPatternFromPlainText(text))

  def assertEqualsPattern(self, string, pattern):
    self.assertEqual(self.parseExpression(string), TestExpression.createPattern(pattern))

  def assertEqualsVarRef(self, string, name):
    self.assertEqual(self.parseExpression(string), TestExpression.createVariableReference(name))

  def assertEqualsVarDef(self, string, name, pattern):
    self.assertEqual(self.parseExpression(string),
                     TestExpression.createVariableDefinition(name, pattern))

  def assertVariantNotEqual(self, string, variant):
    self.assertNotEqual(variant, self.parseExpression(string).variant)

  # Test that individual parts of the line are recognized

  def test_TextOnly(self):
    self.assertEqualsText("foo", "foo")
    self.assertEqualsText("  foo  ", "foo")
    self.assertEqualsRegex("f$o^o", "(f\$o\^o)")

  def test_PatternOnly(self):
    self.assertEqualsPattern("{{a?b.c}}", "a?b.c")

  def test_VarRefOnly(self):
    self.assertEqualsVarRef("<<ABC>>", "ABC")

  def test_VarDefOnly(self):
    self.assertEqualsVarDef("<<ABC:a?b.c>>", "ABC", "a?b.c")

  def test_TextWithWhitespace(self):
    self.assertEqualsRegex("foo bar", "(foo), (bar)")
    self.assertEqualsRegex("foo   bar", "(foo), (bar)")

  def test_TextWithRegex(self):
    self.assertEqualsRegex("foo{{abc}}bar", "(foo)(abc)(bar)")

  def test_TextWithVar(self):
    self.assertEqualsRegex("foo<<ABC:abc>>bar", "(foo)(abc)(bar)")

  def test_PlainWithRegexAndWhitespaces(self):
    self.assertEqualsRegex("foo {{abc}}bar", "(foo), (abc)(bar)")
    self.assertEqualsRegex("foo{{abc}} bar", "(foo)(abc), (bar)")
    self.assertEqualsRegex("foo {{abc}} bar", "(foo), (abc), (bar)")

  def test_PlainWithVarAndWhitespaces(self):
    self.assertEqualsRegex("foo <<ABC:abc>>bar", "(foo), (abc)(bar)")
    self.assertEqualsRegex("foo<<ABC:abc>> bar", "(foo)(abc), (bar)")
    self.assertEqualsRegex("foo <<ABC:abc>> bar", "(foo), (abc), (bar)")

  def test_AllKinds(self):
    self.assertEqualsRegex("foo <<ABC:abc>>{{def}}bar", "(foo), (abc)(def)(bar)")
    self.assertEqualsRegex("foo<<ABC:abc>> {{def}}bar", "(foo)(abc), (def)(bar)")
    self.assertEqualsRegex("foo <<ABC:abc>> {{def}} bar", "(foo), (abc), (def), (bar)")

  # # Test that variables and patterns are parsed correctly

  def test_ValidPattern(self):
    self.assertEqualsPattern("{{abc}}", "abc")
    self.assertEqualsPattern("{{a[b]c}}", "a[b]c")
    self.assertEqualsPattern("{{(a{bc})}}", "(a{bc})")

  def test_ValidRef(self):
    self.assertEqualsVarRef("<<ABC>>", "ABC")
    self.assertEqualsVarRef("<<A1BC2>>", "A1BC2")

  def test_ValidDef(self):
    self.assertEqualsVarDef("<<ABC:abc>>", "ABC", "abc")
    self.assertEqualsVarDef("<<ABC:ab:c>>", "ABC", "ab:c")
    self.assertEqualsVarDef("<<ABC:a[b]c>>", "ABC", "a[b]c")
    self.assertEqualsVarDef("<<ABC:(a[bc])>>", "ABC", "(a[bc])")

  def test_Empty(self):
    self.assertEqualsText("{{}}", "{{}}")
    self.assertVariantNotEqual("<<>>", TestExpression.Variant.VarRef)
    self.assertVariantNotEqual("<<:>>", TestExpression.Variant.VarDef)

  def test_InvalidVarName(self):
    self.assertVariantNotEqual("<<0ABC>>", TestExpression.Variant.VarRef)
    self.assertVariantNotEqual("<<AB=C>>", TestExpression.Variant.VarRef)
    self.assertVariantNotEqual("<<ABC=>>", TestExpression.Variant.VarRef)
    self.assertVariantNotEqual("<<0ABC:abc>>", TestExpression.Variant.VarDef)
    self.assertVariantNotEqual("<<AB=C:abc>>", TestExpression.Variant.VarDef)
    self.assertVariantNotEqual("<<ABC=:abc>>", TestExpression.Variant.VarDef)

  def test_BodyMatchNotGreedy(self):
    self.assertEqualsRegex("{{abc}}{{def}}", "(abc)(def)")
    self.assertEqualsRegex("<<ABC:abc>><<DEF:def>>", "(abc)(def)")

  def test_NoVarDefsInNotChecks(self):
    with self.assertRaises(CheckerException):
      self.parseAssertion("<<ABC:abc>>", "-NOT")


class CheckerParser_FileLayoutTest(unittest.TestCase):

  # Creates an instance of CheckerFile from provided info.
  # Data format: [ ( <case-name>, [ ( <text>, <assert-variant> ), ... ] ), ... ]
  def createFile(self, caseList):
    testFile = CheckerFile("<test_file>")
    for caseEntry in caseList:
      caseName = caseEntry[0]
      testCase = TestCase(testFile, caseName, 0)
      assertionList = caseEntry[1]
      for assertionEntry in assertionList:
        content = assertionEntry[0]
        variant = assertionEntry[1]
        assertion = TestAssertion(testCase, variant, content, 0)
        assertion.addExpression(TestExpression.createPatternFromPlainText(content))
    return testFile

  def assertParsesTo(self, checkerText, expectedData):
    expectedFile = self.createFile(expectedData)
    actualFile = self.parse(checkerText)
    return self.assertEqual(expectedFile, actualFile)

  def parse(self, checkerText):
    return ParseCheckerStream("<test_file>", "CHECK", io.StringIO(ToUnicode(checkerText)))

  def test_EmptyFile(self):
    self.assertParsesTo("", [])

  def test_SingleGroup(self):
    self.assertParsesTo(
      """
        /// CHECK-START: Example Group
        /// CHECK:  foo
        /// CHECK:    bar
      """,
      [ ( "Example Group", [ ("foo", TestAssertion.Variant.InOrder),
                             ("bar", TestAssertion.Variant.InOrder) ] ) ])

  def test_MultipleGroups(self):
    self.assertParsesTo(
      """
        /// CHECK-START: Example Group1
        /// CHECK: foo
        /// CHECK: bar
        /// CHECK-START: Example Group2
        /// CHECK: abc
        /// CHECK: def
      """,
      [ ( "Example Group1", [ ("foo", TestAssertion.Variant.InOrder),
                              ("bar", TestAssertion.Variant.InOrder) ] ),
        ( "Example Group2", [ ("abc", TestAssertion.Variant.InOrder),
                              ("def", TestAssertion.Variant.InOrder) ] ) ])

  def test_AssertionVariants(self):
    self.assertParsesTo(
      """
        /// CHECK-START: Example Group
        /// CHECK:      foo1
        /// CHECK:      foo2
        /// CHECK-NEXT: foo3
        /// CHECK-NEXT: foo4
        /// CHECK-NOT:  bar
        /// CHECK-DAG:  abc
        /// CHECK-DAG:  def
      """,
      [ ( "Example Group", [ ("foo1", TestAssertion.Variant.InOrder),
                             ("foo2", TestAssertion.Variant.InOrder),
                             ("foo3", TestAssertion.Variant.NextLine),
                             ("foo4", TestAssertion.Variant.NextLine),
                             ("bar", TestAssertion.Variant.Not),
                             ("abc", TestAssertion.Variant.DAG),
                             ("def", TestAssertion.Variant.DAG) ] ) ])

  def test_MisplacedNext(self):
    with self.assertRaises(CheckerException):
      self.parse(
        """
          /// CHECK-START: Example Group
          /// CHECK-DAG:  foo
          /// CHECK-NEXT: bar
        """)
    with self.assertRaises(CheckerException):
      self.parse(
        """
          /// CHECK-START: Example Group
          /// CHECK-NOT:  foo
          /// CHECK-NEXT: bar
        """)
    with self.assertRaises(CheckerException):
      self.parse(
        """
          /// CHECK-START: Example Group
          /// CHECK-EVAL: foo
          /// CHECK-NEXT: bar
        """)
    with self.assertRaises(CheckerException):
      self.parse(
        """
          /// CHECK-START: Example Group
          /// CHECK-NEXT: bar
        """)

class CheckerParser_SuffixTests(unittest.TestCase):

  noarch_block = """
                  /// CHECK-START: Group
                  /// CHECK:       foo
                  /// CHECK-NEXT:  bar
                  /// CHECK-NOT:   baz
                  /// CHECK-DAG:   yoyo
                """

  arch_block = """
                  /// CHECK-START-{test_arch}: Group
                  /// CHECK:       foo
                  /// CHECK-NEXT:  bar
                  /// CHECK-NOT:   baz
                  /// CHECK-DAG:   yoyo
                """

  def parse(self, checkerText):
    return ParseCheckerStream("<test_file>", "CHECK", io.StringIO(ToUnicode(checkerText)))

  def test_NonArchTests(self):
    for arch in [None] + archs_list:
      checkerFile = self.parse(self.noarch_block)
      self.assertEqual(len(checkerFile.testCases), 1)
      self.assertEqual(len(checkerFile.testCases[0].assertions), 4)

  def test_IgnoreNonTargetArch(self):
    for targetArch in archs_list:
      for testArch in [a for a in archs_list if a != targetArch]:
        checkerText = self.arch_block.format(test_arch = testArch)
        checkerFile = self.parse(checkerText)
        self.assertEqual(len(checkerFile.testCases), 1)
        self.assertEqual(len(checkerFile.testCasesForArch(testArch)), 1)
        self.assertEqual(len(checkerFile.testCasesForArch(targetArch)), 0)

  def test_Arch(self):
    for arch in archs_list:
      checkerText = self.arch_block.format(test_arch = arch)
      checkerFile = self.parse(checkerText)
      self.assertEqual(len(checkerFile.testCases), 1)
      self.assertEqual(len(checkerFile.testCasesForArch(arch)), 1)
      self.assertEqual(len(checkerFile.testCases[0].assertions), 4)

  def test_NoDebugAndArch(self):
    testCase = self.parse("""
        /// CHECK-START: Group
        /// CHECK: foo
        """).testCases[0]
    self.assertFalse(testCase.forDebuggable)
    self.assertEqual(testCase.testArch, None)

  def test_SetDebugNoArch(self):
    testCase = self.parse("""
        /// CHECK-START-DEBUGGABLE: Group
        /// CHECK: foo
        """).testCases[0]
    self.assertTrue(testCase.forDebuggable)
    self.assertEqual(testCase.testArch, None)

  def test_NoDebugSetArch(self):
    testCase = self.parse("""
        /// CHECK-START-ARM: Group
        /// CHECK: foo
        """).testCases[0]
    self.assertFalse(testCase.forDebuggable)
    self.assertEqual(testCase.testArch, "ARM")

  def test_SetDebugAndArch(self):
    testCase = self.parse("""
        /// CHECK-START-ARM-DEBUGGABLE: Group
        /// CHECK: foo
        """).testCases[0]
    self.assertTrue(testCase.forDebuggable)
    self.assertEqual(testCase.testArch, "ARM")

class CheckerParser_EvalTests(unittest.TestCase):
  def parseTestCase(self, string):
    checkerText = u"/// CHECK-START: pass\n" + ToUnicode(string)
    checkerFile = ParseCheckerStream("<test-file>", "CHECK", io.StringIO(checkerText))
    self.assertEqual(len(checkerFile.testCases), 1)
    return checkerFile.testCases[0]

  def parseExpressions(self, string):
    testCase = self.parseTestCase("/// CHECK-EVAL: " + string)
    self.assertEqual(len(testCase.assertions), 1)
    assertion = testCase.assertions[0]
    self.assertEqual(assertion.variant, TestAssertion.Variant.Eval)
    self.assertEqual(assertion.originalText, string)
    return assertion.expressions

  def assertParsesToPlainText(self, text):
    testCase = self.parseTestCase("/// CHECK-EVAL: " + text)
    self.assertEqual(len(testCase.assertions), 1)
    assertion = testCase.assertions[0]
    self.assertEqual(assertion.variant, TestAssertion.Variant.Eval)
    self.assertEqual(assertion.originalText, text)
    self.assertEqual(len(assertion.expressions), 1)
    expression = assertion.expressions[0]
    self.assertEqual(expression.variant, TestExpression.Variant.PlainText)
    self.assertEqual(expression.text, text)

  def test_PlainText(self):
    self.assertParsesToPlainText("XYZ")
    self.assertParsesToPlainText("True")
    self.assertParsesToPlainText("{{abc}}")
    self.assertParsesToPlainText("<<ABC:abc>>")
    self.assertParsesToPlainText("<<ABC=>>")

  def test_VariableReference(self):
    self.assertEqual(self.parseExpressions("<<ABC>>"),
                     [ TestExpression.createVariableReference("ABC") ])
    self.assertEqual(self.parseExpressions("123<<ABC>>"),
                     [ TestExpression.createPlainText("123"),
                       TestExpression.createVariableReference("ABC") ])
    self.assertEqual(self.parseExpressions("123  <<ABC>>"),
                     [ TestExpression.createPlainText("123  "),
                       TestExpression.createVariableReference("ABC") ])
    self.assertEqual(self.parseExpressions("<<ABC>>XYZ"),
                     [ TestExpression.createVariableReference("ABC"),
                       TestExpression.createPlainText("XYZ") ])
    self.assertEqual(self.parseExpressions("<<ABC>>   XYZ"),
                     [ TestExpression.createVariableReference("ABC"),
                       TestExpression.createPlainText("   XYZ") ])
    self.assertEqual(self.parseExpressions("123<<ABC>>XYZ"),
                     [ TestExpression.createPlainText("123"),
                       TestExpression.createVariableReference("ABC"),
                       TestExpression.createPlainText("XYZ") ])
    self.assertEqual(self.parseExpressions("123 <<ABC>>  XYZ"),
                     [ TestExpression.createPlainText("123 "),
                       TestExpression.createVariableReference("ABC"),
                       TestExpression.createPlainText("  XYZ") ])
