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
Generate java test files for test 966.
"""

import generate_smali as base
import os
import sys
from pathlib import Path

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

# Allow us to import mixins.
sys.path.append(str(Path(BUILD_TOP)/"art"/"test"/"utils"/"python"))

import testgen.mixins as mixins
import functools
import operator
import subprocess

class JavaConverter(mixins.DumpMixin, mixins.Named, mixins.JavaFileMixin):
  """
  A class that can convert a SmaliFile to a JavaFile.
  """
  def __init__(self, inner):
    self.inner = inner

  def get_name(self):
    """Gets the name of this file."""
    return self.inner.get_name()

  def __str__(self):
    out = ""
    for line in str(self.inner).splitlines(keepends = True):
      if line.startswith("#"):
        out += line[1:]
    return out

class Compiler:
  def __init__(self, sources, javac, temp_dir, classes_dir):
    self.javac = javac
    self.temp_dir = temp_dir
    self.classes_dir = classes_dir
    self.sources = sources

  def compile_files(self, args, files):
    """
    Compile the files given with the arguments given.
    """
    args = args.split()
    files = list(map(str, files))
    cmd = ['sh', '-a', '-e', '--', str(self.javac)] + args + files
    print("Running compile command: {}".format(cmd))
    subprocess.check_call(cmd)
    print("Compiled {} files".format(len(files)))

  def execute(self):
    """
    Compiles this test, doing partial compilation as necessary.
    """
    # Compile Main and all classes first. Force all interfaces to be default so that there will be
    # no compiler problems (works since classes only implement 1 interface).
    for f in self.sources:
      if isinstance(f, base.TestInterface):
        JavaConverter(f.get_specific_version(base.InterfaceType.default)).dump(self.temp_dir)
      else:
        JavaConverter(f).dump(self.temp_dir)
    self.compile_files("-d {}".format(self.classes_dir), self.temp_dir.glob("*.java"))

    # Now we compile the interfaces
    ifaces = set(i for i in self.sources if isinstance(i, base.TestInterface))
    while len(ifaces) != 0:
      # Find those ifaces where there are no (uncompiled) interfaces that are subtypes.
      tops = set(filter(lambda a: not any(map(lambda i: a in i.get_super_types(), ifaces)), ifaces))
      files = []
      # Dump these ones, they are getting compiled.
      for f in tops:
        out = JavaConverter(f)
        out.dump(self.temp_dir)
        files.append(self.temp_dir / out.get_file_name())
      # Force all superinterfaces of these to be empty so there will be no conflicts
      overrides = functools.reduce(operator.or_, map(lambda i: i.get_super_types(), tops), set())
      for overridden in overrides:
        out = JavaConverter(overridden.get_specific_version(base.InterfaceType.empty))
        out.dump(self.temp_dir)
        files.append(self.temp_dir / out.get_file_name())
      self.compile_files("-d {outdir} -cp {outdir}".format(outdir = self.classes_dir), files)
      # Remove these from the set of interfaces to be compiled.
      ifaces -= tops
    print("Finished compiling all files.")
    return

def main(argv):
  javac_exec = Path(argv[1])
  if not javac_exec.exists() or not javac_exec.is_file():
    print("{} is not a shell script".format(javac_exec), file=sys.stderr)
    sys.exit(1)
  temp_dir = Path(argv[2])
  if not temp_dir.exists() or not temp_dir.is_dir():
    print("{} is not a valid source dir".format(temp_dir), file=sys.stderr)
    sys.exit(1)
  classes_dir = Path(argv[3])
  if not classes_dir.exists() or not classes_dir.is_dir():
    print("{} is not a valid classes directory".format(classes_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[4])
  mainclass, all_files = base.create_all_test_files()

  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  print("Wrote expected output")

  Compiler(all_files, javac_exec, temp_dir, classes_dir).execute()

if __name__ == '__main__':
  main(sys.argv)
