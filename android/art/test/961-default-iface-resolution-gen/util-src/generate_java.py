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
Generate Java test files for test 961.
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

# The max depth the type tree can have. Includes the class object in the tree.
# Increasing this increases the number of generated files significantly. This
# value was chosen as it is fairly quick to run and very comprehensive, checking
# every possible interface tree up to 5 layers deep.
MAX_IFACE_DEPTH = 5

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

  def get_expected(self):
    """
    Get the expected output of this test.
    """
    all_tests = sorted(self.tests)
    return filter_blanks("\n".join(a.get_expected() for a in all_tests))

  def add_test(self, ty):
    """
    Add a test for the concrete type 'ty'
    """
    self.tests.add(Func(ty))

  def get_name(self):
    """
    Get the name of this class
    """
    return "Main"

  def __str__(self):
    """
    Print the MainClass java code.
    """
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_groups = ""
    for t in all_tests:
      test_groups += str(t)
    for t in all_tests:
      test_invoke += self.TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = self.MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright("java"),
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
      {farg} v = new {farg}();
        System.out.printf("%s calls default method on %s\\n",
                          v.CalledClassName(),
                          v.CalledInterfaceName());
        return;
    }} catch (Error e) {{
      e.printStackTrace(System.out);
      return;
    }}
  }}
"""

  def __init__(self, farg):
    """
    Initialize a test function for the given argument
    """
    self.farg = farg

  def get_expected(self):
    """
    Get the expected output calling this function.
    """
    return "{tree} calls default method on {iface_tree}".format(
        tree = self.farg.get_tree(), iface_tree = self.farg.get_called().get_tree())

  def get_name(self):
    """
    Get the name of this function
    """
    return "TEST_FUNC_{}".format(self.farg.get_name())

  def __str__(self):
    """
    Print the java code of this function.
    """
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(), farg=self.farg.get_name())

class TestClass(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.JavaFileMixin):
  """
  A class that will be instantiated to test default method resolution order.
  """

  TEST_CLASS_TEMPLATE = """{copyright}
public class {class_name} implements {iface_name} {{
  public String CalledClassName() {{
    return "{tree}";
  }}
}}
"""

  def __init__(self, iface):
    """
    Initialize this test class which implements the given interface
    """
    self.iface = iface
    self.class_name = "CLASS_"+gensym()

  def get_name(self):
    """
    Get the name of this class
    """
    return self.class_name

  def get_tree(self):
    """
    Print out a representation of the type tree of this class
    """
    return "[{class_name} {iface_tree}]".format(class_name = self.class_name,
                                                iface_tree = self.iface.get_tree())

  def __iter__(self):
    """
    Step through all interfaces implemented transitively by this class
    """
    yield self.iface
    yield from self.iface

  def get_called(self):
    """
    Get the interface whose default method would be called when calling the
    CalledInterfaceName function.
    """
    all_ifaces = set(iface for iface in self if iface.default)
    for i in all_ifaces:
      if all(map(lambda j: i not in j.get_super_types(), all_ifaces)):
        return i
    raise Exception("UNREACHABLE! Unable to find default method!")

  def __str__(self):
    """
    Print the java code of this class.
    """
    return self.TEST_CLASS_TEMPLATE.format(copyright = get_copyright('java'),
                                           iface_name = self.iface.get_name(),
                                           tree = self.get_tree(),
                                           class_name = self.class_name)

class TestInterface(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.JavaFileMixin):
  """
  An interface that will be used to test default method resolution order.
  """

  TEST_INTERFACE_TEMPLATE = """{copyright}
public interface {class_name} {extends} {ifaces} {{
  public String CalledClassName();

{funcs}
}}
"""

  DEFAULT_FUNC_TEMPLATE = """
  public default String CalledInterfaceName() {{
    return "{tree}";
  }}
"""

  def __init__(self, ifaces, default):
    """
    Initialize interface with the given super-interfaces
    """
    self.ifaces = sorted(ifaces)
    self.default = default
    end = "_DEFAULT" if default else ""
    self.class_name = "INTERFACE_"+gensym()+end

  def get_super_types(self):
    """
    Returns a set of all the supertypes of this interface
    """
    return set(i2 for i2 in self)

  def get_name(self):
    """
    Get the name of this class
    """
    return self.class_name

  def get_tree(self):
    """
    Print out a representation of the type tree of this class
    """
    return "[{class_name} {iftree}]".format(class_name = self.get_name(),
                                            iftree = print_tree(self.ifaces))

  def __iter__(self):
    """
    Performs depth-first traversal of the interface tree this interface is the
    root of. Does not filter out repeats.
    """
    for i in self.ifaces:
      yield i
      yield from i

  def __str__(self):
    """
    Print the java code of this interface.
    """
    j_ifaces = " "
    for i in self.ifaces:
      j_ifaces += " {},".format(i.get_name())
    j_ifaces = j_ifaces[0:-1]
    if self.default:
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(ifaces = j_ifaces,
                                                tree = self.get_tree(),
                                                class_name = self.class_name)
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = get_copyright('java'),
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               tree = self.get_tree(),
                                               class_name = self.class_name)

def print_tree(ifaces):
  """
  Prints a list of iface trees
  """
  return " ".join(i.get_tree() for i in  ifaces)

# The deduplicated output of subtree_sizes for each size up to
# MAX_LEAF_IFACE_PER_OBJECT.
SUBTREES = [set(tuple(sorted(l)) for l in subtree_sizes(i))
            for i in range(MAX_IFACE_DEPTH + 1)]

def create_interface_trees():
  """
  Return all legal interface trees
  """
  def dump_supers(s):
    """
    Does depth first traversal of all the interfaces in the list.
    """
    for i in s:
      yield i
      yield from i

  def create_interface_trees_inner(num, allow_default):
    for split in SUBTREES[num]:
      ifaces = []
      for sub in split:
        if sub == 1:
          ifaces.append([TestInterface([], allow_default)])
          if allow_default:
            ifaces[-1].append(TestInterface([], False))
        else:
          ifaces.append(list(create_interface_trees_inner(sub, allow_default)))
      for supers in itertools.product(*ifaces):
        all_supers = sorted(set(dump_supers(supers)) - set(supers))
        for i in range(len(all_supers) + 1):
          for combo in itertools.combinations(all_supers, i):
            yield TestInterface(list(combo) + list(supers), allow_default)
      if allow_default:
        for i in range(len(split)):
          ifaces = []
          for sub, cs in zip(split, itertools.count()):
            if sub == 1:
              ifaces.append([TestInterface([], i == cs)])
            else:
              ifaces.append(list(create_interface_trees_inner(sub, i == cs)))
          for supers in itertools.product(*ifaces):
            all_supers = sorted(set(dump_supers(supers)) - set(supers))
            for i in range(len(all_supers) + 1):
              for combo in itertools.combinations(all_supers, i):
                yield TestInterface(list(combo) + list(supers), False)

  for num in range(1, MAX_IFACE_DEPTH):
    yield from create_interface_trees_inner(num, True)

def create_all_test_files():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for tree in create_interface_trees():
    classes.add(tree)
    for i in tree:
      classes.add(i)
    test_class = TestClass(tree)
    mc.add_test(test_class)
    classes.add(test_class)
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
