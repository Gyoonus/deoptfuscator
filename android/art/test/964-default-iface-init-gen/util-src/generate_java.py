#!/usr/bin/python3
#
# Copyright (C) 2015 The Android Open Source Project
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
Generate java test files for test 964.
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

from functools import total_ordering
import itertools
import string

# The max depth the tree can have.
MAX_IFACE_DEPTH = 3

class MainClass(mixins.DumpMixin, mixins.Named, mixins.JavaFileMixin):
  """
  A Main.java file containing the Main class and the main function. It will run
  all the test functions we have.
  """

  MAIN_CLASS_TEMPLATE = """{copyright}
class Main {{
{test_groups}
{main_func}
}}
"""

  MAIN_FUNCTION_TEMPLATE = """
  public static void main(String[] args) {{
    {test_group_invoke}
  }}
"""

  TEST_GROUP_INVOKE_TEMPLATE = """
    {test_name}();
"""

  def __init__(self):
    """
    Initialize this MainClass. We start out with no tests.
    """
    self.tests = set()

  def add_test(self, ty):
    """
    Add a test for the concrete type 'ty'
    """
    self.tests.add(Func(ty))

  def get_expected(self):
    """
    Get the expected output of this test.
    """
    all_tests = sorted(self.tests)
    return filter_blanks("\n".join(a.get_expected() for a in all_tests))

  def get_name(self):
    """
    Gets the name of this class
    """
    return "Main"

  def __str__(self):
    """
    Print the java code for this test.
    """
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_groups = ""
    for t in all_tests:
      test_groups += str(t)
    for t in all_tests:
      test_invoke += self.TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = self.MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright('java'),
                                           test_groups = test_groups,
                                           main_func = main_func)

class Func(mixins.Named, mixins.NameComparableMixin):
  """
  A function that tests the functionality of a concrete type. Should only be
  constructed by MainClass.add_test.
  """

  TEST_FUNCTION_TEMPLATE = """
  public static void {fname}() {{
    try {{
      System.out.println("About to initialize {tree}");
      {farg} v = new {farg}();
      System.out.println("Initialized {tree}");
      v.touchAll();
      System.out.println("All of {tree} hierarchy initialized");
      return;
    }} catch (Error e) {{
      e.printStackTrace(System.out);
      return;
    }}
  }}
"""

  OUTPUT_FORMAT = """
About to initialize {tree}
{initialize_output}
Initialized {tree}
{touch_output}
All of {tree} hierarchy initialized
""".strip()

  def __init__(self, farg):
    """
    Initialize a test function for the given argument
    """
    self.farg = farg

  def __str__(self):
    """
    Print the java code for this test function.
    """
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(),
                                              farg=self.farg.get_name(),
                                              tree = self.farg.get_tree())

  def get_name(self):
    """
    Gets the name of this test function
    """
    return "TEST_FUNC_{}".format(self.farg.get_name())

  def get_expected(self):
    """
    Get the expected output of this function.
    """
    return self.OUTPUT_FORMAT.format(
        tree = self.farg.get_tree(),
        initialize_output = self.farg.get_initialize_output().strip(),
        touch_output = self.farg.get_touch_output().strip())

class TestClass(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.JavaFileMixin):
  """
  A class that will be instantiated to test interface initialization order.
  """

  TEST_CLASS_TEMPLATE = """{copyright}
public class {class_name} implements {ifaces} {{
  public void marker() {{
    return;
  }}

  public void touchAll() {{
{touch_calls}
  }}
}}
"""

  TOUCH_CALL_TEMPLATE = """
    System.out.println("{class_name} touching {iface_name}");
    {iface_name}.field.touch();
"""

  TOUCH_OUTPUT_TEMPLATE = """
{class_name} touching {iface_name}
{touch_output}
""".strip()

  def __init__(self, ifaces):
    """
    Initialize this test class which implements the given interfaces
    """
    self.ifaces = ifaces
    self.class_name = "CLASS_"+gensym()

  def get_name(self):
    """
    Gets the name of this interface
    """
    return self.class_name

  def get_tree(self):
    """
    Print out a representation of the type tree of this class
    """
    return "[{fname} {iftree}]".format(fname = self.get_name(), iftree = print_tree(self.ifaces))

  def get_initialize_output(self):
    return "\n".join(map(lambda i: i.get_initialize_output().strip(), dump_tree(self.ifaces)))

  def get_touch_output(self):
    return "\n".join(map(lambda a: self.TOUCH_OUTPUT_TEMPLATE.format(
                                      class_name = self.class_name,
                                      iface_name = a.get_name(),
                                      touch_output = a.get_touch_output()).strip(),
                         self.get_all_interfaces()))

  def get_all_interfaces(self):
    """
    Returns a set of all interfaces this class transitively implements
    """
    return sorted(set(dump_tree(self.ifaces)))

  def __str__(self):
    """
    Print the java code for this class.
    """
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    touches  = '\n'.join(map(lambda a: self.TOUCH_CALL_TEMPLATE.format(class_name = self.class_name,
                                                                       iface_name = a.get_name()),
                             self.get_all_interfaces()))
    return self.TEST_CLASS_TEMPLATE.format(copyright = get_copyright('java'),
                                           ifaces = j_ifaces,
                                           class_name = self.class_name,
                                           touch_calls = touches)

class TestInterface(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.JavaFileMixin):
  """
  An interface that will be used to test default method resolution order.
  """

  TEST_INTERFACE_TEMPLATE = """{copyright}
public interface {class_name} {extends} {ifaces} {{
  public static final Displayer field = new Displayer("{tree}");
  public void marker();
{funcs}
}}
"""

  DEFAULT_FUNC_TEMPLATE = """
  public default void {class_name}_DEFAULT_FUNC() {{ return; }}
"""

  OUTPUT_TEMPLATE = "initialization of {tree}"

  def __init__(self, ifaces, default):
    """
    Initialize interface with the given super-interfaces
    """
    self.ifaces = ifaces
    self.default = default
    end = "_DEFAULT" if default else ""
    self.class_name = "INTERFACE_"+gensym()+end
    self.cloned = False
    self.initialized = False

  def clone(self):
    """
    Clones this interface, returning a new one with the same structure but
    different name.
    """
    return TestInterface(tuple(map(lambda a: a.clone(), self.ifaces)), self.default)

  def get_name(self):
    """
    Gets the name of this interface
    """
    return self.class_name

  def __iter__(self):
    """
    Performs depth-first traversal of the interface tree this interface is the
    root of. Does not filter out repeats.
    """
    for i in self.ifaces:
      yield i
      yield from i

  def get_tree(self):
    """
    Print out a representation of the type tree of this class
    """
    return "[{class_name} {iftree}]".format(class_name = self.get_name(),
                                            iftree = print_tree(self.ifaces))

  def get_initialize_output(self):
    """
    Returns the expected output upon the class that implements this interface being initialized.
    """
    if self.default and not self.initialized:
      self.initialized = True
      return self.OUTPUT_TEMPLATE.format(tree = self.get_tree())
    else:
      return ""

  def get_touch_output(self):
    """
    Returns the expected output upon this interface being touched.
    """
    if not self.default and not self.initialized:
      self.initialized = True
      return self.OUTPUT_TEMPLATE.format(tree = self.get_tree())
    else:
      return ""

  def __str__(self):
    """
    Print the java code for this interface.
    """
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    if self.default:
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(class_name = self.class_name)
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = get_copyright('java'),
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               tree = self.get_tree(),
                                               class_name = self.class_name)

def dump_tree(ifaces):
  """
  Yields all the interfaces transitively implemented by the set in
  reverse-depth-first order
  """
  for i in ifaces:
    yield from dump_tree(i.ifaces)
    yield i

def print_tree(ifaces):
  """
  Prints the tree for the given ifaces.
  """
  return " ".join(i.get_tree() for i in  ifaces)

def clone_all(l):
  return tuple(a.clone() for a in l)

# Cached output of subtree_sizes for speed of access.
SUBTREES = [set(tuple(l) for l in subtree_sizes(i))
            for i in range(MAX_IFACE_DEPTH + 1)]

def create_test_classes():
  """
  Yield all the test classes with the different interface trees
  """
  for num in range(1, MAX_IFACE_DEPTH + 1):
    for split in SUBTREES[num]:
      ifaces = []
      for sub in split:
        ifaces.append(list(create_interface_trees(sub)))
      for supers in itertools.product(*ifaces):
        yield TestClass(clone_all(supers))
        for i in range(len(set(dump_tree(supers)) - set(supers))):
          ns = clone_all(supers)
          selected = sorted(set(dump_tree(ns)) - set(ns))[i]
          yield TestClass(tuple([selected] + list(ns)))

def create_interface_trees(num):
  """
  Yield all the interface trees up to 'num' depth.
  """
  if num == 0:
    yield TestInterface(tuple(), False)
    yield TestInterface(tuple(), True)
    return
  for split in SUBTREES[num]:
    ifaces = []
    for sub in split:
      ifaces.append(list(create_interface_trees(sub)))
    for supers in itertools.product(*ifaces):
      yield TestInterface(clone_all(supers), False)
      yield TestInterface(clone_all(supers), True)
      # TODO Should add on some from higher up the tree.

def create_all_test_files():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for clazz in create_test_classes():
    classes.add(clazz)
    for i in dump_tree(clazz.ifaces):
      classes.add(i)
    mc.add_test(clazz)
  return mc, classes

def main(argv):
  java_dir = Path(argv[1])
  if not java_dir.exists() or not java_dir.is_dir():
    print("{} is not a valid java dir".format(java_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[2])
  mainclass, all_files = create_all_test_files()
  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  for f in all_files:
    f.dump(java_dir)

if __name__ == '__main__':
  main(sys.argv)
