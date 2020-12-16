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
Generate Smali test files for test 966.
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

class MainClass(mixins.DumpMixin, mixins.Named, mixins.SmaliFileMixin):
  """
  A Main.smali file containing the Main class and the main function. It will run
  all the test functions we have.
  """

  MAIN_CLASS_TEMPLATE = """{copyright}

.class public LMain;
.super Ljava/lang/Object;

# class Main {{

.method public constructor <init>()V
    .registers 1
    invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
    return-void
.end method

{test_groups}

{main_func}

# }}
"""

  MAIN_FUNCTION_TEMPLATE = """
#   public static void main(String[] args) {{
.method public static main([Ljava/lang/String;)V
    .locals 2

    {test_group_invoke}

    return-void
.end method
#   }}
"""

  TEST_GROUP_INVOKE_TEMPLATE = """
#     {test_name}();
    invoke-static {{}}, {test_name}()V
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

  def initial_build_different(self):
    return False

  def get_name(self):
    """
    Gets the name of this class
    """
    return "Main"

  def __str__(self):
    """
    Print the smali code for this test.
    """
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_groups = ""
    for t in all_tests:
      test_groups += str(t)
    for t in all_tests:
      test_invoke += self.TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = self.MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright('smali'),
                                           test_groups = test_groups,
                                           main_func = main_func)

class Func(mixins.Named, mixins.NameComparableMixin):
  """
  A function that tests the functionality of a concrete type. Should only be
  constructed by MainClass.add_test.
  """

  TEST_FUNCTION_TEMPLATE = """
#   public static void {fname}() {{
#     try {{
#       {farg} v = new {farg}();
#       System.out.println("Testing {tree}");
#       v.testAll();
#       System.out.println("Success: testing {tree}");
#       return;
#     }} catch (Exception e) {{
#       System.out.println("Failure: testing {tree}");
#       e.printStackTrace(System.out);
#       return;
#     }}
#   }}
.method public static {fname}()V
    .locals 7
    :call_{fname}_try_start
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;

      new-instance v6, L{farg};
      invoke-direct {{v6}}, L{farg};-><init>()V

      const-string v3, "Testing {tree}"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

      invoke-virtual {{v6}}, L{farg};->testAll()V

      const-string v3, "Success: testing {tree}"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

      return-void
    :call_{fname}_try_end
    .catch Ljava/lang/Exception; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :error_{fname}_start
    :error_{fname}_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "Failure: testing {tree}"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      invoke-virtual {{v3,v2}}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method
"""

  OUTPUT_FORMAT = """
Testing {tree}
{test_output}
Success: testing {tree}
""".strip()

  def __init__(self, farg):
    """
    Initialize a test function for the given argument
    """
    self.farg = farg

  def __str__(self):
    """
    Print the smali code for this test function.
    """
    return self.TEST_FUNCTION_TEMPLATE.format(fname = self.get_name(),
                                              farg  = self.farg.get_name(),
                                              tree  = self.farg.get_tree())

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
        test_output = self.farg.get_test_output().strip())

class TestClass(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.SmaliFileMixin):
  """
  A class that will be instantiated to test interface initialization order.
  """

  TEST_CLASS_TEMPLATE = """{copyright}

.class public L{class_name};
.super Ljava/lang/Object;
{implements_spec}

# public class {class_name} implements {ifaces} {{
#
#   public {class_name}() {{
#   }}
.method public constructor <init>()V
  .locals 2
  invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
  return-void
.end method

#   public String getCalledInterface() {{
#     throw new Error("Should not be called");
#   }}
.method public getCalledInterface()V
  .locals 2
  const-string v0, "Should not be called"
  new-instance v1, Ljava/lang/Error;
  invoke-direct {{v1, v0}}, Ljava/lang/Error;-><init>(Ljava/lang/String;)V
  throw v1
.end method

#   public void testAll() {{
#     boolean failed = false;
#     Error exceptions = new Error("Test failures");
.method public testAll()V
  .locals 5
  const/4 v0, 0
  const-string v1, "Test failures"
  new-instance v2, Ljava/lang/Error;
  invoke-direct {{v2, v1}}, Ljava/lang/Error;-><init>(Ljava/lang/String;)V

  {test_calls}

#     if (failed) {{
  if-eqz v0, :end
#       throw exceptions;
    throw v2
  :end
#     }}
  return-void
#   }}
.end method

{test_funcs}

# }}
"""

  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""

  TEST_CALL_TEMPLATE = """
#     try {{
#       test_{iface}_super();
#     }} catch (Throwable t) {{
#       exceptions.addSuppressed(t);
#       failed = true;
#     }}
  :try_{iface}_start
    invoke-virtual {{p0}}, L{class_name};->test_{iface}_super()V
    goto :error_{iface}_end
  :try_{iface}_end
  .catch Ljava/lang/Throwable; {{:try_{iface}_start .. :try_{iface}_end}} :error_{iface}_start
  :error_{iface}_start
    move-exception v3
    invoke-virtual {{v2, v3}}, Ljava/lang/Throwable;->addSuppressed(Ljava/lang/Throwable;)V
    const/4 v0, 1
  :error_{iface}_end
"""

  TEST_FUNC_TEMPLATE = """
#   public void test_{iface}_super() {{
#     try {{
#       System.out.println("{class_name} -> {iface}.super.getCalledInterface(): " +
#                          {iface}.super.getCalledInterface());
#     }} catch (NoSuchMethodError e) {{
#       System.out.println("{class_name} -> {iface}.super.getCalledInterface(): NoSuchMethodError");
#     }} catch (IncompatibleClassChangeError e) {{
#       System.out.println("{class_name} -> {iface}.super.getCalledInterface(): IncompatibleClassChangeError");
#     }} catch (Throwable t) {{
#       System.out.println("{class_name} -> {iface}.super.getCalledInterface(): Unknown error occurred");
#       throw t;
#     }}
#   }}
.method public test_{iface}_super()V
  .locals 3
  sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
  :try_start
    const-string v1, "{class_name} -> {iface}.super.getCalledInterface(): "
    invoke-super {{p0}}, L{iface};->getCalledInterface()Ljava/lang/String;
    move-result-object v2

    invoke-virtual {{v1, v2}}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1

    invoke-virtual {{v0, v1}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    return-void
  :try_end
  .catch Ljava/lang/NoSuchMethodError; {{:try_start .. :try_end}} :AME_catch
  .catch Ljava/lang/IncompatibleClassChangeError; {{:try_start .. :try_end}} :ICCE_catch
  .catch Ljava/lang/Throwable; {{:try_start .. :try_end}} :throwable_catch
  :AME_catch
    const-string v1, "{class_name} -> {iface}.super.getCalledInterface(): NoSuchMethodError"
    invoke-virtual {{v0, v1}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
  :ICCE_catch
    const-string v1, "{class_name} -> {iface}.super.getCalledInterface(): IncompatibleClassChangeError"
    invoke-virtual {{v0, v1}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
  :throwable_catch
    move-exception v2
    const-string v1, "{class_name} -> {iface}.super.getCalledInterface(): Unknown error occurred"
    invoke-virtual {{v0, v1}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    throw v2
.end method
""".strip()

  OUTPUT_TEMPLATE = "{class_name} -> {iface}.super.getCalledInterface(): {result}"

  def __init__(self, ifaces, name = None):
    """
    Initialize this test class which implements the given interfaces
    """
    self.ifaces = ifaces
    if name is None:
      self.class_name = "CLASS_"+gensym()
    else:
      self.class_name = name

  def get_initial_build_version(self):
    """
    Returns a version of this class that can be used for the initial build (meaning no compiler
    checks will be triggered).
    """
    return TestClass([i.get_initial_build_version() for i in self.ifaces], self.class_name)

  def initial_build_different(self):
    return False

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

  def get_test_output(self):
    return '\n'.join(map(lambda a: self.OUTPUT_TEMPLATE.format(class_name = self.get_name(),
                                                               iface = a.get_name(),
                                                               result = a.get_output()),
                         self.ifaces))

  def __str__(self):
    """
    Print the smali code for this class.
    """
    funcs = '\n'.join(map(lambda a: self.TEST_FUNC_TEMPLATE.format(iface = a.get_name(),
                                                                   class_name = self.get_name()),
                          self.ifaces))
    calls = '\n'.join(map(lambda a: self.TEST_CALL_TEMPLATE.format(iface = a.get_name(),
                                                                   class_name = self.get_name()),
                          self.ifaces))
    s_ifaces = '\n'.join(map(lambda a: self.IMPLEMENTS_TEMPLATE.format(iface_name = a.get_name()),
                             self.ifaces))
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    return self.TEST_CLASS_TEMPLATE.format(copyright = get_copyright('smali'),
                                           implements_spec = s_ifaces,
                                           ifaces = j_ifaces,
                                           class_name = self.class_name,
                                           test_funcs = funcs,
                                           test_calls = calls)

class IncompatibleClassChangeErrorResult(mixins.Named):
  def get_name(self):
    return "IncompatibleClassChangeError"

ICCE = IncompatibleClassChangeErrorResult()

class NoSuchMethodErrorResult(mixins.Named):
  def get_name(self):
    return "NoSuchMethodError"

NSME = NoSuchMethodErrorResult()

class TestInterface(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.SmaliFileMixin):
  """
  An interface that will be used to test default method resolution order.
  """

  TEST_INTERFACE_TEMPLATE = """{copyright}
.class public abstract interface L{class_name};
.super Ljava/lang/Object;
{implements_spec}

# public interface {class_name} {extends} {ifaces} {{

{funcs}

# }}
"""

  ABSTRACT_FUNC_TEMPLATE = """
"""

  DEFAULT_FUNC_TEMPLATE = """
#   public default String getCalledInterface() {{
#     return "{class_name}";
#   }}
.method public getCalledInterface()Ljava/lang/String;
  .locals 1
  const-string v0, "{class_name}"
  return-object v0
.end method
"""

  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""

  def __init__(self, ifaces, default, name = None):
    """
    Initialize interface with the given super-interfaces
    """
    self.ifaces = ifaces
    self.default = default
    if name is None:
      end = "_DEFAULT" if default else ""
      self.class_name = "INTERFACE_"+gensym()+end
    else:
      self.class_name = name

  def get_initial_build_version(self):
    """
    Returns a version of this class that can be used for the initial build (meaning no compiler
    checks will be triggered).
    """
    return TestInterface([i.get_initial_build_version() for i in self.ifaces],
                         True,
                         self.class_name)

  def initial_build_different(self):
    return not self.default

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

  def get_called(self):
    """
    Get the interface whose default method would be called when calling the
    CalledInterfaceName function.
    """
    all_ifaces = set(iface for iface in self if iface.default)
    for i in all_ifaces:
      if all(map(lambda j: i not in j.get_super_types(), all_ifaces)):
        return i
    return ICCE if any(map(lambda i: i.default, all_ifaces)) else NSME

  def get_super_types(self):
    """
    Returns a set of all the supertypes of this interface
    """
    return set(i2 for i2 in self)

  def get_output(self):
    if self.default:
      return self.get_name()
    else:
      return self.get_called().get_name()

  def get_tree(self):
    """
    Print out a representation of the type tree of this class
    """
    return "[{class_name} {iftree}]".format(class_name = self.get_name(),
                                            iftree = print_tree(self.ifaces))
  def __str__(self):
    """
    Print the smali code for this interface.
    """
    s_ifaces = '\n'.join(map(lambda a: self.IMPLEMENTS_TEMPLATE.format(iface_name = a.get_name()),
                             self.ifaces))
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    if self.default:
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(class_name = self.class_name)
    else:
      funcs = self.ABSTRACT_FUNC_TEMPLATE
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = get_copyright('smali'),
                                               implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
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

# Cached output of subtree_sizes for speed of access.
SUBTREES = [set(tuple(sorted(l)) for l in subtree_sizes(i)) for i in range(MAX_IFACE_DEPTH + 1)]

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
      yield TestClass(supers)

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
      yield TestInterface(supers, False)
      yield TestInterface(supers, True)
      for selected in (set(dump_tree(supers)) - set(supers)):
         yield TestInterface(tuple([selected] + list(supers)), True)
         yield TestInterface(tuple([selected] + list(supers)), False)
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
  smali_dir = Path(argv[1])
  if not smali_dir.exists() or not smali_dir.is_dir():
    print("{} is not a valid smali dir".format(smali_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[2])
  mainclass, all_files = create_all_test_files()
  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  for f in all_files:
    f.dump(smali_dir)

if __name__ == '__main__':
  main(sys.argv)
