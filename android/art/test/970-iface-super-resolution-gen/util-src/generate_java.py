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

class JavaConverter(mixins.DumpMixin, mixins.Named, mixins.JavaFileMixin):
  """
  A class that can convert a SmaliFile to a JavaFile.
  """
  def __init__(self, inner):
    self.inner = inner

  def get_name(self):
    return self.inner.get_name()

  def __str__(self):
    out = ""
    for line in str(self.inner).splitlines(keepends = True):
      if line.startswith("#"):
        out += line[1:]
    return out

def main(argv):
  final_java_dir = Path(argv[1])
  if not final_java_dir.exists() or not final_java_dir.is_dir():
    print("{} is not a valid java dir".format(final_java_dir), file=sys.stderr)
    sys.exit(1)
  initial_java_dir = Path(argv[2])
  if not initial_java_dir.exists() or not initial_java_dir.is_dir():
    print("{} is not a valid java dir".format(initial_java_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[3])
  mainclass, all_files = base.create_all_test_files()
  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  for f in all_files:
    if f.initial_build_different():
      JavaConverter(f).dump(final_java_dir)
      JavaConverter(f.get_initial_build_version()).dump(initial_java_dir)
    else:
      JavaConverter(f).dump(initial_java_dir)
      if isinstance(f, base.TestInterface):
        JavaConverter(f).dump(final_java_dir)


if __name__ == '__main__':
  main(sys.argv)
