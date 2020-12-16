#! /usr/bin/python3
#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Generate Java test files for test 648-many-direct-methods.
"""

import os
import sys
from pathlib import Path

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

# Allow us to import utils and mixins.
sys.path.append(str(Path(BUILD_TOP)/"art"/"test"/"utils"/"python"))

from testgen.utils import get_copyright, subtree_sizes, gensym, filter_blanks
import testgen.mixins as mixins

class MainClass(mixins.DumpMixin, mixins.Named, mixins.JavaFileMixin):
  """
  A Main.java file containing the Main class and the main function. It will run
  all the test functions we have.
  """

  MAIN_CLASS_TEMPLATE = """{copyright}
public class Main {{
{main_func}
{test_groups}

}}"""

  MAIN_FUNCTION_TEMPLATE = """
  public static void main(String[] args) {
    System.out.println("passed");
  }"""

  def __init__(self):
    """
    Initialize this MainClass. We start out with no tests.
    """
    self.tests = set()

  def add_test_method(self, num):
    """
    Add test method number 'num'
    """
    self.tests.add(TestMethod(num))

  def get_name(self):
    """
    Get the name of this class
    """
    return "Main"

  def __str__(self):
    """
    Print the MainClass Java code.
    """
    all_tests = sorted(self.tests)
    test_groups = ""
    for t in all_tests:
      test_groups += str(t)
    main_func = self.MAIN_FUNCTION_TEMPLATE

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright("java"),
                                           main_func = main_func,
                                           test_groups = test_groups)

class TestMethod(mixins.Named, mixins.NameComparableMixin):
  """
  A function that represents a test method. Should only be
  constructed by MainClass.add_test_method.
  """

  TEST_FUNCTION_TEMPLATE = """
  public static void {fname}() {{}}"""

  def __init__(self, farg):
    """
    Initialize a test method for the given argument.
    """
    self.farg = farg

  def get_name(self):
    """
    Get the name of this test method.
    """
    return "method{:05d}".format(self.farg)

  def __str__(self):
    """
    Print the Java code of this test method.
    """
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name())

# Number of generated test methods. This number has been chosen to
# make sure the number of direct methods in class Main is greater or
# equal to 2^16, and thus requires an *unsigned* 16-bit (short)
# integer to be represented (b/33650497).
NUM_TEST_METHODS = 32768

def create_test_file():
  """
  Creates the object representing the test file. It just needs to be dumped.
  """
  mc = MainClass()
  for i in range(1, NUM_TEST_METHODS + 1):
    mc.add_test_method(i)
  return mc

def main(argv):
  java_dir = Path(argv[1])
  if not java_dir.exists() or not java_dir.is_dir():
    print("{} is not a valid Java dir".format(java_dir), file=sys.stderr)
    sys.exit(1)
  mainclass = create_test_file()
  mainclass.dump(java_dir)

if __name__ == '__main__':
  main(sys.argv)
