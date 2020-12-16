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

from common.logger                 import Logger
from file_format.c1visualizer.test import C1visualizerParser_Test
from file_format.checker.test      import CheckerParser_PrefixTest, \
                                          CheckerParser_TestExpressionTest, \
                                          CheckerParser_FileLayoutTest, \
                                          CheckerParser_SuffixTests, \
                                          CheckerParser_EvalTests
from match.test                    import MatchLines_Test, \
                                          MatchFiles_Test

import unittest

if __name__ == '__main__':
  Logger.Verbosity = Logger.Level.NoOutput
  unittest.main(verbosity=2)
