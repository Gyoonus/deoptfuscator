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
Common functions useful for writing test generators in python
"""

import itertools
import os
import string
from pathlib import Path

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

# An iterator which yields strings made from lowercase letters. First yields
# all 1 length strings, then all 2 and so on. It does this alphabetically.
NAME_GEN = itertools.chain.from_iterable(
    map(lambda n: itertools.product(string.ascii_lowercase, repeat=n),
        itertools.count(1)))

def gensym():
  """
  Returns a new, globally unique, identifier name that is a valid Java symbol
  on each call.
  """
  return ''.join(next(NAME_GEN))

def filter_blanks(s):
  """
  Takes a string returns the same string sans empty lines
  """
  return "\n".join(a for a in s.split("\n") if a.strip() != "")

def get_copyright(filetype = "java"):
  """
  Returns the standard copyright header for the given filetype
  """
  if filetype == "smali":
    return "\n".join(map(lambda a: "# " + a, get_copyright("java").split("\n")))
  else:
    fname = filetype + ".txt"
    with (Path(BUILD_TOP)/"development"/"docs"/"copyright-templates"/fname).open() as template:
      return "".join(template.readlines())

def subtree_sizes(n):
  """
  A generator that yields a tuple containing a possible arrangement of subtree
  nodes for a tree with a total of 'n' leaf nodes.
  """
  if n == 0:
    return
  elif n == 1:
    yield (0,)
  elif n == 2:
    yield (1, 1)
  else:
    for prevt in subtree_sizes(n - 1):
      prev = list(prevt)
      yield tuple([1] + prev)
      for i in range(len(prev)):
        prev[i] += 1
        yield tuple(prev)
        prev[i] -= 1

