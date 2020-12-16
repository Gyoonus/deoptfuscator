#!/usr/bin/env python
#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Analyzes the dump of initialization failures and creates a Graphviz dot file
   representing dependencies."""

import codecs
import os
import re
import string
import sys


_CLASS_RE = re.compile(r'^L(.*);$')
_ERROR_LINE_RE = re.compile(r'^dalvik.system.TransactionAbortError: (.*)')
_STACK_LINE_RE = re.compile(r'^\s*at\s[^\s]*\s([^\s]*)')

def Confused(filename, line_number, line):
  sys.stderr.write('%s:%d: confused by:\n%s\n' % (filename, line_number, line))
  raise Exception("giving up!")
  sys.exit(1)


def ProcessFile(filename):
  lines = codecs.open(filename, 'r', 'utf8', 'replace').read().split('\n')
  it = iter(lines)

  class_fail_class = {}
  class_fail_method = {}
  class_fail_load_library = {}
  class_fail_get_property = {}
  root_failures = set()
  root_errors = {}

  while True:
    try:
      # We start with a class descriptor.
      raw_line = it.next()
      m = _CLASS_RE.search(raw_line)
      # print(raw_line)
      if m is None:
        continue
      # Found a class.
      failed_clazz = m.group(1).replace('/','.')
      # print('Is a class %s' % failed_clazz)
      # The error line should be next.
      raw_line = it.next()
      m = _ERROR_LINE_RE.search(raw_line)
      # print(raw_line)
      if m is None:
        Confused(filename, -1, raw_line)
        continue
      # Found an error line.
      error = m.group(1)
      # print('Is an error %s' % error)
      # Get the top of the stack
      raw_line = it.next()
      m = _STACK_LINE_RE.search(raw_line)
      if m is None:
        continue
      # Found a stack line. Get the method.
      method = m.group(1)
      # print('Is a stack element %s' % method)
      (left_of_paren,paren,right_of_paren) = method.partition('(')
      (root_err_class,dot,root_method_name) = left_of_paren.rpartition('.')
      # print('Error class %s' % err_class)
      # print('Error method %s' % method_name)
      # Record the root error.
      root_failures.add(root_err_class)
      # Parse all the trace elements to find the "immediate" cause.
      immediate_class = root_err_class
      immediate_method = root_method_name
      root_errors[root_err_class] = error
      was_load_library = False
      was_get_property = False
      # Now go "up" the stack.
      while True:
        raw_line = it.next()
        m = _STACK_LINE_RE.search(raw_line)
        if m is None:
          break  # Nothing more to see here.
        method = m.group(1)
        (left_of_paren,paren,right_of_paren) = method.partition('(')
        (err_class,dot,err_method_name) = left_of_paren.rpartition('.')
        if err_method_name == "<clinit>":
          # A class initializer is on the stack...
          class_fail_class[err_class] = immediate_class
          class_fail_method[err_class] = immediate_method
          class_fail_load_library[err_class] = was_load_library
          immediate_class = err_class
          immediate_method = err_method_name
          class_fail_get_property[err_class] = was_get_property
          was_get_property = False
        was_load_library = err_method_name == "loadLibrary"
        was_get_property = was_get_property or err_method_name == "getProperty"
      failed_clazz_norm = re.sub(r"^L", "", failed_clazz)
      failed_clazz_norm = re.sub(r";$", "", failed_clazz_norm)
      failed_clazz_norm = re.sub(r"/", "", failed_clazz_norm)
      if immediate_class != failed_clazz_norm:
        class_fail_class[failed_clazz_norm] = immediate_class
        class_fail_method[failed_clazz_norm] = immediate_method
    except StopIteration:
      # print('Done')
      break  # Done

  # Assign IDs.
  fail_sources = set(class_fail_class.values());
  all_classes = fail_sources | set(class_fail_class.keys())
  i = 0
  class_index = {}
  for clazz in all_classes:
    class_index[clazz] = i
    i = i + 1

  # Now create the nodes.
  for (r_class, r_id) in class_index.items():
    error_string = ''
    if r_class in root_failures:
      error_string = ',style=filled,fillcolor=Red,tooltip="' + root_errors[r_class] + '",URL="' + root_errors[r_class] + '"'
    elif r_class in class_fail_load_library and class_fail_load_library[r_class] == True:
      error_string = error_string + ',style=filled,fillcolor=Bisque'
    elif r_class in class_fail_get_property and class_fail_get_property[r_class] == True:
      error_string = error_string + ',style=filled,fillcolor=Darkseagreen'
    print('  n%d [shape=box,label="%s"%s];' % (r_id, r_class, error_string))

  # Some space.
  print('')

  # Connections.
  for (failed_class,error_class) in class_fail_class.items():
    print('  n%d -> n%d;' % (class_index[failed_class], class_index[error_class]))


def main():
  print('digraph {')
  print('  overlap=false;')
  print('  splines=true;')
  ProcessFile(sys.argv[1])
  print('}')
  sys.exit(0)


if __name__ == '__main__':
  main()
