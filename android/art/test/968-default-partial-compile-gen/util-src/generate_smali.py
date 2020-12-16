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
Generate Smali test files for test 967.
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

from enum import Enum
from functools import total_ordering
import itertools
import string

# The max depth the type tree can have.
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

{test_funcs}

{main_func}

# }}
"""

  MAIN_FUNCTION_TEMPLATE = """
#   public static void main(String[] args) {{
.method public static main([Ljava/lang/String;)V
    .locals 0

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
    Print the MainClass smali code.
    """
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_funcs = ""
    for t in all_tests:
      test_funcs += str(t)
    for t in all_tests:
      test_invoke += self.TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = self.MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright("smali"),
                                           test_funcs = test_funcs,
                                           main_func = main_func)

class Func(mixins.Named, mixins.NameComparableMixin):
  """
  A function that tests the functionality of a concrete type. Should only be
  constructed by MainClass.add_test.
  """

  TEST_FUNCTION_TEMPLATE = """
#   public static void {fname}() {{
#     {farg} v = null;
#     try {{
#       v = new {farg}();
#     }} catch (Throwable e) {{
#       System.out.println("Unexpected error occurred which creating {farg} instance");
#       e.printStackTrace(System.out);
#       return;
#     }}
#     try {{
#       System.out.printf("{tree} calls %s\\n", v.getName());
#       return;
#     }} catch (AbstractMethodError e) {{
#       System.out.println("{tree} threw AbstractMethodError");
#     }} catch (NoSuchMethodError e) {{
#       System.out.println("{tree} threw NoSuchMethodError");
#     }} catch (IncompatibleClassChangeError e) {{
#       System.out.println("{tree} threw IncompatibleClassChangeError");
#     }} catch (Throwable e) {{
#       e.printStackTrace(System.out);
#       return;
#     }}
#   }}
.method public static {fname}()V
    .locals 7
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;

    :new_{fname}_try_start
      new-instance v0, L{farg};
      invoke-direct {{v0}}, L{farg};-><init>()V
      goto :call_{fname}_try_start
    :new_{fname}_try_end
    .catch Ljava/lang/Throwable; {{:new_{fname}_try_start .. :new_{fname}_try_end}} :new_error_{fname}_start
    :new_error_{fname}_start
      move-exception v6
      const-string v5, "Unexpected error occurred which creating {farg} instance"
      invoke-virtual {{v4,v5}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      invoke-virtual {{v6,v4}}, Ljava/lang/Throwable;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
    :call_{fname}_try_start
      const/4 v1, 1
      new-array v2,v1, [Ljava/lang/Object;
      const/4 v1, 0
      invoke-virtual {{v0}}, L{farg};->getName()Ljava/lang/String;
      move-result-object v3
      aput-object v3,v2,v1

      const-string v5, "{tree} calls %s\\n"

      invoke-virtual {{v4,v5,v2}}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_{fname}_try_end
    .catch Ljava/lang/AbstractMethodError; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :AME_{fname}_start
    .catch Ljava/lang/NoSuchMethodError; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :NSME_{fname}_start
    .catch Ljava/lang/IncompatibleClassChangeError; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :ICCE_{fname}_start
    .catch Ljava/lang/Throwable; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :error_{fname}_start
    :AME_{fname}_start
      const-string v5, "{tree} threw AbstractMethodError"
      invoke-virtual {{v4,v5}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      return-void
    :NSME_{fname}_start
      const-string v5, "{tree} threw NoSuchMethodError"
      invoke-virtual {{v4,v5}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      return-void
    :ICCE_{fname}_start
      const-string v5, "{tree} threw IncompatibleClassChangeError"
      invoke-virtual {{v4,v5}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      return-void
    :error_{fname}_start
      move-exception v6
      invoke-virtual {{v6,v4}}, Ljava/lang/Throwable;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method
"""

  NSME_RESULT_TEMPLATE = "{tree} threw NoSuchMethodError"
  ICCE_RESULT_TEMPLATE = "{tree} threw IncompatibleClassChangeError"
  AME_RESULT_TEMPLATE = "{tree} threw AbstractMethodError"
  NORMAL_RESULT_TEMPLATE = "{tree} calls {result}"

  def __init__(self, farg):
    """
    Initialize a test function for the given argument
    """
    self.farg = farg

  def get_expected(self):
    """
    Get the expected output calling this function.
    """
    exp = self.farg.get_called()
    if exp.is_empty():
      return self.NSME_RESULT_TEMPLATE.format(tree = self.farg.get_tree())
    elif exp.is_abstract():
      return self.AME_RESULT_TEMPLATE.format(tree = self.farg.get_tree())
    elif exp.is_conflict():
      return self.ICCE_RESULT_TEMPLATE.format(tree = self.farg.get_tree())
    else:
      assert exp.is_default()
      return self.NORMAL_RESULT_TEMPLATE.format(tree = self.farg.get_tree(),
                                                result = exp.get_tree())

  def get_name(self):
    """
    Get the name of this function
    """
    return "TEST_FUNC_{}".format(self.farg.get_name())

  def __str__(self):
    """
    Print the smali code of this function.
    """
    return self.TEST_FUNCTION_TEMPLATE.format(tree = self.farg.get_tree(),
                                              fname = self.get_name(),
                                              farg = self.farg.get_name())

class TestClass(mixins.DumpMixin, mixins.Named, mixins.NameComparableMixin, mixins.SmaliFileMixin):
  """
  A class that will be instantiated to test default method resolution order.
  """

  TEST_CLASS_TEMPLATE = """{copyright}

.class public L{class_name};
.super Ljava/lang/Object;
.implements L{iface_name};

# public class {class_name} implements {iface_name} {{

.method public constructor <init>()V
  .registers 1
  invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
  return-void
.end method

{funcs}

# }}
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
    Returns the interface that will be called when the method on this class is invoked or
    CONFLICT_TYPE if there is no interface that will be called.
    """
    return self.iface.get_called()

  def __str__(self):
    """
    Print the smali code of this class.
    """
    return self.TEST_CLASS_TEMPLATE.format(copyright = get_copyright('smali'),
                                           iface_name = self.iface.get_name(),
                                           tree = self.get_tree(),
                                           class_name = self.class_name,
                                           funcs = "")

class InterfaceType(Enum):
  """
  An enumeration of all the different types of interfaces we can have.

  default: It has a default method
  abstract: It has a method declared but not defined
  empty: It does not have the method
  """
  default = 0
  abstract = 1
  empty = 2

  def get_suffix(self):
    if self == InterfaceType.default:
      return "_DEFAULT"
    elif self == InterfaceType.abstract:
      return "_ABSTRACT"
    elif self == InterfaceType.empty:
      return "_EMPTY"
    else:
      raise TypeError("Interface type had illegal value.")

class ConflictInterface:
  """
  A singleton representing a conflict of default methods.
  """

  def is_conflict(self):
    """
    Returns true if this is a conflict interface and calling the method on this interface will
    result in an IncompatibleClassChangeError.
    """
    return True

  def is_abstract(self):
    """
    Returns true if this is an abstract interface and calling the method on this interface will
    result in an AbstractMethodError.
    """
    return False

  def is_empty(self):
    """
    Returns true if this is an abstract interface and calling the method on this interface will
    result in a NoSuchMethodError.
    """
    return False

  def is_default(self):
    """
    Returns true if this is a default interface and calling the method on this interface will
    result in a method actually being called.
    """
    return False

CONFLICT_TYPE = ConflictInterface()

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

  DEFAULT_FUNC_TEMPLATE = """
#   public default String getName() {{
#     return "{tree}";
#   }}
.method public getName()Ljava/lang/String;
  .locals 1
  const-string v0, "{tree}"
  return-object v0
.end method
"""

  ABSTRACT_FUNC_TEMPLATE = """
#   public String getName();
.method public abstract getName()Ljava/lang/String;
.end method
"""

  EMPTY_FUNC_TEMPLATE = """"""

  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""

  def __init__(self, ifaces, iface_type, full_name = None):
    """
    Initialize interface with the given super-interfaces
    """
    self.ifaces = sorted(ifaces)
    self.iface_type = iface_type
    if full_name is None:
      end = self.iface_type.get_suffix()
      self.class_name = "INTERFACE_"+gensym()+end
    else:
      self.class_name = full_name

  def get_specific_version(self, v):
    """
    Returns a copy of this interface of the given type for use in partial compilation.
    """
    return TestInterface(self.ifaces, v, full_name = self.class_name)

  def get_super_types(self):
    """
    Returns a set of all the supertypes of this interface
    """
    return set(i2 for i2 in self)

  def is_conflict(self):
    """
    Returns true if this is a conflict interface and calling the method on this interface will
    result in an IncompatibleClassChangeError.
    """
    return False

  def is_abstract(self):
    """
    Returns true if this is an abstract interface and calling the method on this interface will
    result in an AbstractMethodError.
    """
    return self.iface_type == InterfaceType.abstract

  def is_empty(self):
    """
    Returns true if this is an abstract interface and calling the method on this interface will
    result in a NoSuchMethodError.
    """
    return self.iface_type == InterfaceType.empty

  def is_default(self):
    """
    Returns true if this is a default interface and calling the method on this interface will
    result in a method actually being called.
    """
    return self.iface_type == InterfaceType.default

  def get_called(self):
    """
    Returns the interface that will be called when the method on this class is invoked or
    CONFLICT_TYPE if there is no interface that will be called.
    """
    if not self.is_empty() or len(self.ifaces) == 0:
      return self
    else:
      best = self
      for super_iface in self.ifaces:
        super_best = super_iface.get_called()
        if super_best.is_conflict():
          return CONFLICT_TYPE
        elif best.is_default():
          if super_best.is_default():
            return CONFLICT_TYPE
        elif best.is_abstract():
          if super_best.is_default():
            best = super_best
        else:
          assert best.is_empty()
          best = super_best
      return best

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
    Print the smali code of this interface.
    """
    s_ifaces = " "
    j_ifaces = " "
    for i in self.ifaces:
      s_ifaces += self.IMPLEMENTS_TEMPLATE.format(iface_name = i.get_name())
      j_ifaces += " {},".format(i.get_name())
    j_ifaces = j_ifaces[0:-1]
    if self.is_default():
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(tree = self.get_tree())
    elif self.is_abstract():
      funcs = self.ABSTRACT_FUNC_TEMPLATE.format()
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = get_copyright('smali'),
                                               implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               tree = self.get_tree(),
                                               class_name = self.class_name)

def print_tree(ifaces):
  """
  Prints a list of iface trees
  """
  return " ".join(i.get_tree() for i in ifaces)

# The deduplicated output of subtree_sizes for each size up to
# MAX_LEAF_IFACE_PER_OBJECT.
SUBTREES = [set(tuple(sorted(l)) for l in subtree_sizes(i))
            for i in range(MAX_IFACE_DEPTH + 1)]

def create_test_classes():
  """
  Yield all the test classes with the different interface trees
  """
  for num in range(1, MAX_IFACE_DEPTH + 1):
    for iface in create_interface_trees(num):
      yield TestClass(iface)

def create_interface_trees(num):
  """
  Yield all the interface trees up to 'num' depth.
  """
  if num == 0:
    for iftype in InterfaceType:
      yield TestInterface(tuple(), iftype)
    return
  for split in SUBTREES[num]:
    ifaces = []
    for sub in split:
      ifaces.append(list(create_interface_trees(sub)))
    yield TestInterface(tuple(), InterfaceType.default)
    for supers in itertools.product(*ifaces):
      for iftype in InterfaceType:
        if iftype == InterfaceType.default:
          # We can just stop at defaults. We have other tests that a default can override an
          # abstract and this cuts down on the number of cases significantly, improving speed of
          # this test.
          continue
        yield TestInterface(supers, iftype)

def create_all_test_files():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for clazz in create_test_classes():
    classes.add(clazz)
    for i in clazz:
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
