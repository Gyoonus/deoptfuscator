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

"""Tests for bisection-search module."""

import unittest

from unittest.mock import Mock

from bisection_search import BugSearch
from bisection_search import Dex2OatWrapperTestable
from bisection_search import FatalError
from bisection_search import MANDATORY_PASSES


class BisectionTestCase(unittest.TestCase):
  """BugSearch method test case.

  Integer constants were chosen arbitrarily. They should be large enough and
  random enough to ensure binary search does nontrivial work.

  Attributes:
    _METHODS: list of strings, methods compiled by testable
    _PASSES: list of strings, passes run by testable
    _FAILING_METHOD: string, name of method which fails in some tests
    _FAILING_PASS: string, name of pass which fails in some tests
    _MANDATORY_PASS: string, name of a mandatory pass
  """
  _METHODS_COUNT = 1293
  _PASSES_COUNT = 573
  _FAILING_METHOD_IDX = 237
  _FAILING_PASS_IDX = 444
  _METHODS = ['method_{0}'.format(i) for i in range(_METHODS_COUNT)]
  _PASSES = ['pass_{0}'.format(i) for i in range(_PASSES_COUNT)]
  _FAILING_METHOD = _METHODS[_FAILING_METHOD_IDX]
  _FAILING_PASS = _PASSES[_FAILING_PASS_IDX]
  _MANDATORY_PASS = MANDATORY_PASSES[0]

  def setUp(self):
    self.testable_mock = Mock(spec=Dex2OatWrapperTestable)
    self.testable_mock.GetAllMethods.return_value = self._METHODS
    self.testable_mock.GetAllPassesForMethod.return_value = self._PASSES

  def MethodFailsForAllPasses(self, compiled_methods, run_passes=None):
    return self._FAILING_METHOD not in compiled_methods

  def MethodFailsForAPass(self, compiled_methods, run_passes=None):
    return (self._FAILING_METHOD not in compiled_methods or
            (run_passes is not None and self._FAILING_PASS not in run_passes))

  def testNeverFails(self):
    self.testable_mock.Test.return_value = True
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (None, None))

  def testAlwaysFails(self):
    self.testable_mock.Test.return_value = False
    with self.assertRaises(FatalError):
      BugSearch(self.testable_mock)

  def testAMethodFailsForAllPasses(self):
    self.testable_mock.Test.side_effect = self.MethodFailsForAllPasses
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (self._FAILING_METHOD, None))

  def testAMethodFailsForAPass(self):
    self.testable_mock.Test.side_effect = self.MethodFailsForAPass
    res = BugSearch(self.testable_mock)
    self.assertEqual(res, (self._FAILING_METHOD, self._FAILING_PASS))

  def testMandatoryPassPresent(self):
    self.testable_mock.GetAllPassesForMethod.return_value += (
        [self._MANDATORY_PASS])
    self.testable_mock.Test.side_effect = self.MethodFailsForAPass
    BugSearch(self.testable_mock)
    for (ordered_args, keyword_args) in self.testable_mock.Test.call_args_list:
      passes = None
      if 'run_passes' in keyword_args:
        passes = keyword_args['run_passes']
      if len(ordered_args) > 1:  # run_passes passed as ordered argument
        passes = ordered_args[1]
      if passes is not None:
        self.assertIn(self._MANDATORY_PASS, passes)

if __name__ == '__main__':
  unittest.main()
