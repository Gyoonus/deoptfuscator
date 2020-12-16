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

from common.testing                  import ToUnicode
from file_format.c1visualizer.parser import ParseC1visualizerStream
from file_format.c1visualizer.struct import C1visualizerFile, C1visualizerPass

import io
import unittest

class C1visualizerParser_Test(unittest.TestCase):

  def createFile(self, passList):
    """ Creates an instance of CheckerFile from provided info.

    Data format: [ ( <case-name>, [ ( <text>, <assert-variant> ), ... ] ), ... ]
    """
    c1File = C1visualizerFile("<c1_file>")
    for passEntry in passList:
      passName = passEntry[0]
      passBody = passEntry[1]
      c1Pass = C1visualizerPass(c1File, passName, passBody, 0)
    return c1File

  def assertParsesTo(self, c1Text, expectedData):
    expectedFile = self.createFile(expectedData)
    actualFile = ParseC1visualizerStream("<c1_file>", io.StringIO(ToUnicode(c1Text)))
    return self.assertEqual(expectedFile, actualFile)

  def test_EmptyFile(self):
    self.assertParsesTo("", [])

  def test_SingleGroup(self):
    self.assertParsesTo(
      """
        begin_compilation
          method "MyMethod"
        end_compilation
        begin_cfg
          name "pass1"
          foo
          bar
        end_cfg
      """,
      [ ( "MyMethod pass1", [ "foo", "bar" ] ) ])

  def test_MultipleGroups(self):
    self.assertParsesTo(
      """
        begin_compilation
          name "xyz1"
          method "MyMethod1"
          date 1234
        end_compilation
        begin_cfg
          name "pass1"
          foo
          bar
        end_cfg
        begin_cfg
          name "pass2"
          abc
          def
        end_cfg
      """,
      [ ( "MyMethod1 pass1", [ "foo", "bar" ] ),
        ( "MyMethod1 pass2", [ "abc", "def" ] ) ])
    self.assertParsesTo(
      """
        begin_compilation
          name "xyz1"
          method "MyMethod1"
          date 1234
        end_compilation
        begin_cfg
          name "pass1"
          foo
          bar
        end_cfg
        begin_compilation
          name "xyz2"
          method "MyMethod2"
          date 5678
        end_compilation
        begin_cfg
          name "pass2"
          abc
          def
        end_cfg
      """,
      [ ( "MyMethod1 pass1", [ "foo", "bar" ] ),
        ( "MyMethod2 pass2", [ "abc", "def" ] ) ])
