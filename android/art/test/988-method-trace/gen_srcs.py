#!/usr/bin/python3
# Copyright (C) 2017 The Android Open Source Project
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

#
# Generates the src/art/Test988Intrinsics.java file.
# Re-run this every time art/compiler/intrinics_list.h is modified.
#
# $> python3.4 gen_srcs.py > src/art/Test988Intrinsics.java
#

import argparse
import os
import re
import collections
import sys

from string import Template

# Relative path to art/runtime/intrinsics_list.h
INTRINSICS_LIST_H = os.path.dirname(os.path.realpath(__file__)) + "/../../runtime/intrinsics_list.h"

# Macro parameter index to V(). Negative means from the end.
IDX_STATIC_OR_VIRTUAL = 1
IDX_SIGNATURE = -1
IDX_METHOD_NAME = -2
IDX_CLASS_NAME = -3

# Exclude all hidden API.
KLASS_BLACK_LIST = ['sun.misc.Unsafe', 'libcore.io.Memory', 'java.lang.StringFactory',
                    'java.lang.invoke.MethodHandle', # invokes are tested by 956-method-handles
                    'java.lang.invoke.VarHandle' ]  # TODO(b/65872996): will tested separately
METHOD_BLACK_LIST = [('java.lang.ref.Reference', 'getReferent'),
                     ('java.lang.String', 'getCharsNoCheck'),
                     ('java.lang.System', 'arraycopy')]  # arraycopy has a manual test.

# When testing a virtual function, it needs to operate on an instance.
# These instances will be created with the following values,
# otherwise a default 'new T()' is used.
KLASS_INSTANCE_INITIALIZERS = {
  'java.lang.String' : '"some large string"',
  'java.lang.StringBuffer' : 'new java.lang.StringBuffer("some large string buffer")',
  'java.lang.StringBuilder' : 'new java.lang.StringBuilder("some large string builder")',
  'java.lang.ref.Reference' : 'new java.lang.ref.WeakReference(new Object())'
};

OUTPUT_TPL = Template("""
/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// AUTO-GENENERATED by gen_srcs.py: DO NOT EDIT HERE DIRECTLY.
//
// $$> python3.4 gen_srcs.py > src/art/Test988Intrinsics.java
//
// RUN ABOVE COMMAND TO REGENERATE THIS FILE.

package art;

class Test988Intrinsics {
  // Pre-initialize *all* instance variables used so that their constructors are not in the trace.
$static_fields

  static void initialize() {
    // Ensure all static variables are initialized.
    // In addition, pre-load classes here so that we don't see diverging class loading traces.
$initialize_classes
  }

  static void test() {
    // Call each intrinsic from art/runtime/intrinsics_list.h to make sure they are traced.
$test_body
  }
}
""")

JNI_TYPES = {
  'Z' : 'boolean',
  'B' : 'byte',
  'C' : 'char',
  'S' : 'short',
  'I' : 'int',
  'J' : 'long',
  'F' : 'float',
  'D' : 'double',
  'L' : 'object'
};

debug_printing_enabled = False

def debug_print(x):
  if debug_printing_enabled:
    print(x, file=sys.stderr)

# Parse JNI sig into a list, e.g. "II" -> ['I', 'I'], '[[IJ' -> ['[[I', 'J'], etc.
def sig_to_parameter_type_list(sig):
  sig = re.sub(r'[(](.*)[)].*', r'\1', sig)

  lst = []
  obj = ""
  is_obj = False
  is_array = False
  for x in sig:
    if is_obj:
      obj = obj + x
      if x == ";":
        is_obj = False
        lst.append(obj)
        obj = ""
    elif is_array:
      obj = obj + x
      if x != "[":
        is_array = False
        lst.append(obj)
        obj = ""
    else:
      if x == "[":
        obj = "["
        is_array = True
      elif x == "L":
        obj = "L"
        is_obj = True
      else:
        lst.append(x)

  return lst

# Convert a single JNI descriptor into a pretty java name, e.g. "[I" -> "int[]", etc.
def javafy_name(kls_name):
  if kls_name.startswith("L"):
    kls_name = kls_name.lstrip("L").rstrip(";")
    return kls_name.replace("/", ".")
  elif kls_name.startswith("["):
    array_count = kls_name.count("[")
    non_array = javafy_name(kls_name.lstrip("["))
    return non_array + ("[]" * array_count)

  return JNI_TYPES.get(kls_name, kls_name)

def extract_staticness(static_or_virtual):
  if static_or_virtual == "kStatic":
    return 'static'
  return 'virtual' # kVirtual, kDirect

class MethodInfo:
  def __init__(self, staticness, pretty_params, method, kls):
    # 'virtual' or 'static'
    self.staticness = staticness
    # list of e.g. ['int', 'double', 'java.lang.String'] etc
    self.parameters = pretty_params
    # e.g. 'toString'
    self.method_name = method
    # e.g. 'java.lang.String'
    self.klass = kls

  def __str__(self):
    return "MethodInfo " + str(self.__dict__)

  def dummy_parameters(self):
    dummy_values = {
     'boolean' : 'false',
     'byte' : '(byte)0',
     'char' : "'x'",
     'short' : '(short)0',
     'int' : '0',
     'long' : '0L',
     'float' : '0.0f',
     'double' : '0.0'
    }

    def object_dummy(name):
      if name == "java.lang.String":
        return '"hello"'
      else:
        return "(%s)null" %(name)
    return [ dummy_values.get(param, object_dummy(param)) for param in self.parameters ]

  def dummy_instance_value(self):
    return KLASS_INSTANCE_INITIALIZERS.get(self.klass, 'new %s()' %(self.klass))

  def is_blacklisted(self):
    for blk in KLASS_BLACK_LIST:
      if self.klass.startswith(blk):
        return True

    return (self.klass, self.method_name) in METHOD_BLACK_LIST

# parse the V(...) \ list of items into a MethodInfo
def parse_method_info(items):
  def get_item(idx):
    return items[idx].strip().strip("\"")

  staticness = get_item(IDX_STATIC_OR_VIRTUAL)
  sig = get_item(IDX_SIGNATURE)
  method = get_item(IDX_METHOD_NAME)
  kls = get_item(IDX_CLASS_NAME)

  debug_print ((sig, method, kls))

  staticness = extract_staticness(staticness)
  kls = javafy_name(kls)
  param_types = sig_to_parameter_type_list(sig)
  pretty_params = param_types
  pretty_params = [javafy_name(i) for i in param_types]

  return MethodInfo(staticness, pretty_params, method, kls)

# parse a line containing '  V(...)' into a MethodInfo
def parse_line(line):
  line = line.strip()
  if not line.startswith("V("):
    return None

  line = re.sub(r'V[(](.*)[)]', r'\1', line)
  debug_print(line)

  items = line.split(",")

  method_info = parse_method_info(items)
  return method_info

# Generate all the MethodInfo that we parse from intrinsics_list.h
def parse_all_method_infos():
  with open(INTRINSICS_LIST_H) as f:
    for line in f:
      s = parse_line(line)
      if s is not None:
        yield s

# Format a receiver name. For statics, it's the class name, for receivers, it's an instance variable
def format_receiver_name(method_info):
  receiver = method_info.klass
  if method_info.staticness == 'virtual':
    receiver = "instance_" + method_info.klass.replace(".", "_")
  return receiver

# Format a dummy call with dummy method parameters to the requested method.
def format_call_to(method_info):
  dummy_args = ", ".join(method_info.dummy_parameters())
  receiver = format_receiver_name(method_info)

  return ("%s.%s(%s);" %(receiver, method_info.method_name, dummy_args))

# Format a static variable with an instance that could be used as the receiver
# (or None for non-static methods).
def format_instance_variable(method_info):
  if method_info.staticness == 'static':
    return None
  return "static %s %s = %s;" %(method_info.klass, format_receiver_name(method_info), method_info.dummy_instance_value())

def format_initialize_klass(method_info):
  return "%s.class.toString();" %(method_info.klass)

def indent_list(lst, indent):
  return [' ' * indent + i for i in lst]

def main():
  global debug_printing_enabled
  parser = argparse.ArgumentParser(description='Generate art/test/988-method-trace/src/art/Test988Intrinsics.java')
  parser.add_argument('-d', '--debug', action='store_true', dest='debug', help='Print extra debugging information to stderr.')
  parser.add_argument('output_file', nargs='?', metavar='<output-file>', default=sys.stdout, type=argparse.FileType('w'), help='Destination file to write to (default: stdout).')
  args = parser.parse_args()

  debug_printing_enabled = args.debug

  #####

  call_str_list = []
  instance_variable_dict = collections.OrderedDict()
  initialize_klass_dict = collections.OrderedDict()
  for i in parse_all_method_infos():
    debug_print(i)
    if i.is_blacklisted():
      debug_print("Blacklisted: " + str(i))
      continue

    call_str = format_call_to(i)
    debug_print(call_str)

    call_str_list.append(call_str)

    instance_variable = format_instance_variable(i)
    if instance_variable is not None:
      debug_print(instance_variable)
      instance_variable_dict[i.klass] = instance_variable

    initialize_klass_dict[i.klass] = format_initialize_klass(i)

  static_fields = indent_list([ value for (key, value) in instance_variable_dict.items() ], 2)
  test_body = indent_list(call_str_list, 4)
  initialize_classes = indent_list([ value for (key, value) in initialize_klass_dict.items() ], 4)

  print(OUTPUT_TPL.substitute(static_fields="\n".join(static_fields),
                              test_body="\n".join(test_body),
                              initialize_classes="\n".join(initialize_classes)).
                   strip("\n"), \
        file=args.output_file)

if __name__ == '__main__':
  main()
