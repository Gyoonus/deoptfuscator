#!/usr/bin/python3
#
# Copyright (C) 2018 The Android Open Source Project
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
Generate java test files for 712-varhandle-invocations
"""

from enum import Enum
from pathlib import Path
from random import Random
from string import Template

import io
import re
import sys

class JavaType(object):
    def __init__(self, name, examples, supports_bitwise=False, supports_numeric=False):
        self.name=name
        self.examples=examples
        self.supports_bitwise=supports_bitwise
        self.supports_numeric=supports_numeric

    def is_value_type(self):
        return False

    def __repr__(self):
        return self.name

    def __str__(self):
        return self.name

class ValueType(JavaType):
    def __init__(self, name, boxed_type, examples, ordinal=-1, width=-1, supports_bitwise=True, supports_numeric=True):
        JavaType.__init__(self, name, examples, supports_bitwise, supports_numeric)
        self.ordinal=ordinal
        self.width=width
        self.boxed_type=boxed_type

    def boxing_method(self):
        return self.boxed_type + ".valueOf"

    def unboxing_method(self):
        return self.name + "Value"

    def is_value_type(self):
        return True

    def __eq__(self, other):
        return self.ordinal == other.ordinal

    def __hash__(self):
        return self.ordinal

    def __le__(self, other):
        return self.ordinal < other.ordinal

    def __repr__(self):
        return self.name

    def __str__(self):
        return self.name

BOOLEAN_TYPE = ValueType("boolean", "Boolean", [ "true", "false" ], ordinal = 0, width = 1, supports_numeric=False)
BYTE_TYPE=ValueType("byte", "Byte", [ "(byte) -128", "(byte) -61", "(byte) 7", "(byte) 127", "(byte) 33" ], ordinal=1, width=1)
SHORT_TYPE=ValueType("short", "Short", [ "(short) -32768", "(short) -384", "(short) 32767", "(short) 0xaa55" ], ordinal=2, width=2)
CHAR_TYPE=ValueType("char", "Character", [ r"'A'", r"'#'", r"'$'", r"'Z'", r"'t'", r"'c'" ], ordinal=3, width=2)
INT_TYPE=ValueType("int", "Integer", [ "-0x01234567", "0x7f6e5d4c", "0x12345678", "0x10215220", "42" ], ordinal=4, width=4)
LONG_TYPE=ValueType("long", "Long", [ "-0x0123456789abcdefl", "0x789abcdef0123456l", "0xfedcba9876543210l" ], ordinal=5, width=8)
FLOAT_TYPE=ValueType("float", "Float", [ "-7.77e23f", "1.234e-17f", "3.40e36f", "-8.888e3f", "4.442e11f" ], ordinal=6, width=4, supports_bitwise=False)
DOUBLE_TYPE=ValueType("double", "Double", [ "-1.0e-200", "1.11e200", "3.141", "1.1111", "6.022e23", "6.626e-34" ], ordinal=7, width=4, supports_bitwise=False)

VALUE_TYPES = { BOOLEAN_TYPE, BYTE_TYPE, SHORT_TYPE, CHAR_TYPE, INT_TYPE, LONG_TYPE, FLOAT_TYPE, DOUBLE_TYPE }

WIDENING_CONVERSIONS = {
    BOOLEAN_TYPE : set(),
    BYTE_TYPE : { SHORT_TYPE, INT_TYPE, LONG_TYPE, FLOAT_TYPE, DOUBLE_TYPE },
    SHORT_TYPE : { INT_TYPE, LONG_TYPE, FLOAT_TYPE, DOUBLE_TYPE },
    CHAR_TYPE : { INT_TYPE, LONG_TYPE, FLOAT_TYPE, DOUBLE_TYPE },
    INT_TYPE : { LONG_TYPE, FLOAT_TYPE, DOUBLE_TYPE },
    LONG_TYPE : { FLOAT_TYPE, DOUBLE_TYPE },
    FLOAT_TYPE : { DOUBLE_TYPE },
    DOUBLE_TYPE : set()
}

def types_that_widen_to(var_type):
    types_that_widen = { var_type }
    for src_type in WIDENING_CONVERSIONS:
        if var_type in WIDENING_CONVERSIONS[src_type]:
            types_that_widen.add(src_type)
    return types_that_widen

class VarHandleKind(object):
    ALL_SUPPORTED_TYPES = VALUE_TYPES
    VIEW_SUPPORTED_TYPES = list(filter(lambda x : x.width >= 2, ALL_SUPPORTED_TYPES))

    def __init__(self, name, imports=[], declarations=[], lookup='', coordinates=[], get_value='', may_throw_read_only=False):
        self.name = name
        self.imports = imports
        self.declarations = declarations
        self.lookup = lookup
        self.coordinates = coordinates
        self.get_value_ = get_value
        self.may_throw_read_only = may_throw_read_only

    def get_name(self):
        return self.name

    def get_coordinates(self):
        return self.coordinates

    def get_field_declarations(self, dictionary):
        return list(map(lambda d: Template(d).safe_substitute(dictionary), self.declarations))

    def get_imports(self):
        return self.imports

    def get_lookup(self, dictionary):
        return Template(self.lookup).safe_substitute(dictionary)

    def get_supported_types(self):
        return VarHandleKind.VIEW_SUPPORTED_TYPES if self.is_view() else VarHandleKind.ALL_SUPPORTED_TYPES

    def is_view(self):
        return "View" in self.name

    def get_value(self, dictionary):
        return Template(self.get_value_).safe_substitute(dictionary)

FIELD_VAR_HANDLE = VarHandleKind("Field",
                                 [
                                     'java.lang.invoke.MethodHandles',
                                     'java.lang.invoke.VarHandle'
                                 ],
                                 [
                                     "${var_type} field = ${initial_value}"
                                 ],
                                 'MethodHandles.lookup().findVarHandle(${test_class}.class, "field", ${var_type}.class)',
                                 [
                                     'this'
                                 ],
                                 'field',
                                 may_throw_read_only = False)

FINAL_FIELD_VAR_HANDLE = VarHandleKind("FinalField",
                                       [
                                           'java.lang.invoke.MethodHandles',
                                           'java.lang.invoke.VarHandle'
                                       ],
                                       [
                                           "${var_type} field = ${initial_value}"
                                       ],
                                       'MethodHandles.lookup().findVarHandle(${test_class}.class, "field", ${var_type}.class)',
                                       [
                                           'this'
                                       ],
                                       'field',
                                       may_throw_read_only = False)

STATIC_FIELD_VAR_HANDLE = VarHandleKind("StaticField",
                                        [
                                            'java.lang.invoke.MethodHandles',
                                            'java.lang.invoke.VarHandle'
                                        ],
                                        [
                                            "static ${var_type} field = ${initial_value}"
                                        ],
                                        'MethodHandles.lookup().findStaticVarHandle(${test_class}.class, "field", ${var_type}.class)',
                                        [],
                                        'field',
                                        may_throw_read_only = False)

STATIC_FINAL_FIELD_VAR_HANDLE = VarHandleKind("StaticFinalField",
                                              [
                                                  'java.lang.invoke.MethodHandles',
                                                  'java.lang.invoke.VarHandle'
                                              ],
                                              [
                                                  "static ${var_type} field = ${initial_value}"
                                              ],
                                              'MethodHandles.lookup().findStaticVarHandle(${test_class}.class, "field", ${var_type}.class)',
                                              [],
                                              'field',
                                              may_throw_read_only = False)

ARRAY_ELEMENT_VAR_HANDLE = VarHandleKind("ArrayElement",
                                         [
                                             'java.lang.invoke.MethodHandles',
                                             'java.lang.invoke.VarHandle'
                                         ],
                                         [
                                             "${var_type}[] array = new ${var_type}[11]",
                                             "int index = 3",
                                             "{ array[index] = ${initial_value}; }"
                                         ],
                                         'MethodHandles.arrayElementVarHandle(${var_type}[].class)',
                                         [ 'array', 'index'],
                                         'array[index]',
                                         may_throw_read_only = False)

BYTE_ARRAY_LE_VIEW_VAR_HANDLE = VarHandleKind("ByteArrayViewLE",
                                              [
                                                  'java.lang.invoke.MethodHandles',
                                                  'java.lang.invoke.VarHandle',
                                                  'java.nio.ByteOrder'
                                              ],
                                              [
                                                  "byte[] array = new byte[27]",
                                                  "int index = 8",
                                                  "{"
                                                  "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(array, index);"
                                                  "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(array, index, ${initial_value}, ByteOrder.LITTLE_ENDIAN);"
                                                  "}"
                                              ],
                                              'MethodHandles.byteArrayViewVarHandle(${var_type}[].class, ByteOrder.LITTLE_ENDIAN)',
                                              [
                                                  'array',
                                                  'index'
                                              ],
                                              'VarHandleUnitTestHelpers.getBytesAs_${var_type}(array, index, ByteOrder.LITTLE_ENDIAN)',
                                              may_throw_read_only = False)

BYTE_ARRAY_BE_VIEW_VAR_HANDLE = VarHandleKind("ByteArrayViewBE",
                                              [
                                                  'java.lang.invoke.MethodHandles',
                                                  'java.lang.invoke.VarHandle',
                                                  'java.nio.ByteOrder'
                                              ],
                                              [
                                                  "byte[] array = new byte[27]",
                                                  "int index = 8",
                                                  "{"
                                                  "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(array, index);"
                                                  "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(array, index, ${initial_value}, ByteOrder.BIG_ENDIAN);"
                                                  "}"
                                              ],
                                              'MethodHandles.byteArrayViewVarHandle(${var_type}[].class, ByteOrder.BIG_ENDIAN)',
                                              [
                                                  'array',
                                                  'index'
                                              ],
                                              'VarHandleUnitTestHelpers.getBytesAs_${var_type}(array, index, ByteOrder.BIG_ENDIAN)',
                                              may_throw_read_only = False)

DIRECT_BYTE_BUFFER_LE_VIEW_VAR_HANDLE = VarHandleKind("DirectByteBufferViewLE",
                                                      [
                                                          'java.lang.invoke.MethodHandles',
                                                          'java.lang.invoke.VarHandle',
                                                          'java.nio.ByteBuffer',
                                                          'java.nio.ByteOrder'
                                                      ],
                                                      [
                                                          "ByteBuffer bb = ByteBuffer.allocateDirect(31)",
                                                          "int index = 8",
                                                          "{"
                                                          "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, index);"
                                                          "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(bb, index, ${initial_value}, ByteOrder.LITTLE_ENDIAN);"
                                                          "}"
                                                      ],
                                                      'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.LITTLE_ENDIAN)',
                                                      [
                                                          'bb',
                                                          'index'
                                                      ],
                                                      'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.LITTLE_ENDIAN)',
                                                      may_throw_read_only = False)

DIRECT_BYTE_BUFFER_BE_VIEW_VAR_HANDLE = VarHandleKind("DirectByteBufferViewBE",
                                                      [
                                                          'java.lang.invoke.MethodHandles',
                                                          'java.lang.invoke.VarHandle',
                                                          'java.nio.ByteBuffer',
                                                          'java.nio.ByteOrder'
                                                      ],
                                                      [
                                                          "ByteBuffer bb = ByteBuffer.allocateDirect(31)",
                                                          "int index = 8",
                                                          "{"
                                                          "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, index);"
                                                          "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(bb, index, ${initial_value}, ByteOrder.BIG_ENDIAN);"
                                                          "}"
                                                      ],
                                                      'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.BIG_ENDIAN)',
                                                      [
                                                          'bb',
                                                          'index'
                                                      ],
                                                      'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.BIG_ENDIAN)',
                                                      may_throw_read_only = False)

HEAP_BYTE_BUFFER_LE_VIEW_VAR_HANDLE = VarHandleKind("HeapByteBufferViewLE",
                                                    [
                                                        'java.lang.invoke.MethodHandles',
                                                        'java.lang.invoke.VarHandle',
                                                        'java.nio.ByteBuffer',
                                                        'java.nio.ByteOrder'
                                                    ],
                                                    [
                                                        "byte[] array = new byte[36]",
                                                        "int offset = 8",
                                                        "ByteBuffer bb = ByteBuffer.wrap(array, offset, array.length - offset)",
                                                        "int index = 8",
                                                        "{"
                                                        "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, index);"
                                                        "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(bb, index, ${initial_value}, ByteOrder.LITTLE_ENDIAN);"
                                                        "}"
                                                    ],
                                                    'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.LITTLE_ENDIAN)',
                                                    [
                                                        'bb',
                                                        'index'
                                                    ],
                                                    'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.LITTLE_ENDIAN)',
                                                    may_throw_read_only = False)

HEAP_BYTE_BUFFER_BE_VIEW_VAR_HANDLE = VarHandleKind("HeapByteBufferViewBE",
                                                    [
                                                        'java.lang.invoke.MethodHandles',
                                                        'java.lang.invoke.VarHandle',
                                                        'java.nio.ByteBuffer',
                                                        'java.nio.ByteOrder'
                                                    ],
                                                    [
                                                        "byte[] array = new byte[47]",
                                                        "int offset = 8",
                                                        "ByteBuffer bb = ByteBuffer.wrap(array, offset, array.length - offset)",
                                                        "int index = 8",
                                                        "{"
                                                        "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, index);"
                                                        "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(bb, index, ${initial_value}, ByteOrder.BIG_ENDIAN);"
                                                        "}"
                                                    ],
                                                    'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.BIG_ENDIAN)',
                                                    [
                                                        'bb',
                                                        'index'
                                                    ],
                                                    'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.BIG_ENDIAN)',
                                                    may_throw_read_only = False)

HEAP_BYTE_BUFFER_RO_LE_VIEW_VAR_HANDLE = VarHandleKind("HeapByteBufferReadOnlyViewLE",
                                                       [
                                                           'java.lang.invoke.MethodHandles',
                                                           'java.lang.invoke.VarHandle',
                                                           'java.nio.ByteBuffer',
                                                           'java.nio.ByteOrder',
                                                           'java.nio.ReadOnlyBufferException'
                                                       ],
                                                       [
                                                           "byte[] array = new byte[43]",
                                                           "int index = 8",
                                                           "ByteBuffer bb",
                                                           "{"
                                                           "  bb = ByteBuffer.wrap(array).asReadOnlyBuffer();"
                                                           "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, index);"
                                                           "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(array, index, ${initial_value}, ByteOrder.LITTLE_ENDIAN);"
                                                           "  bb = bb.asReadOnlyBuffer();"

                                                           "}"
                                                       ],
                                                       'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.LITTLE_ENDIAN)',
                                                       [
                                                           'bb',
                                                           'index'
                                                       ],
                                                       'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.LITTLE_ENDIAN)',
                                                       may_throw_read_only = True)

HEAP_BYTE_BUFFER_RO_BE_VIEW_VAR_HANDLE = VarHandleKind("HeapByteBufferReadOnlyViewBE",
                                                       [
                                                           'java.lang.invoke.MethodHandles',
                                                           'java.lang.invoke.VarHandle',
                                                           'java.nio.ByteBuffer',
                                                           'java.nio.ByteOrder',
                                                           'java.nio.ReadOnlyBufferException'
                                                       ],
                                                       [
                                                           "byte[] array = new byte[29]",
                                                           "int index",
                                                           "ByteBuffer bb",
                                                           "{"
                                                           "  bb = ByteBuffer.wrap(array);"
                                                           "  index = VarHandleUnitTestHelpers.alignedOffset_${var_type}(bb, 8);"
                                                           "  VarHandleUnitTestHelpers.setBytesAs_${var_type}(array, index, ${initial_value}, ByteOrder.BIG_ENDIAN);"
                                                           "  bb = bb.asReadOnlyBuffer();"
                                                           "}"
                                                       ],
                                                       'MethodHandles.byteBufferViewVarHandle(${var_type}[].class, ByteOrder.BIG_ENDIAN)',
                                                       [
                                                           'bb',
                                                           'index'
                                                       ],
                                                       'VarHandleUnitTestHelpers.getBytesAs_${var_type}(bb, index, ByteOrder.BIG_ENDIAN)',
                                                       may_throw_read_only = True)

ALL_FIELD_VAR_HANDLE_KINDS = [
    FIELD_VAR_HANDLE,
    FINAL_FIELD_VAR_HANDLE,
    STATIC_FIELD_VAR_HANDLE,
    STATIC_FINAL_FIELD_VAR_HANDLE
]

ALL_BYTE_VIEW_VAR_HANDLE_KINDS = [
    BYTE_ARRAY_LE_VIEW_VAR_HANDLE,
    BYTE_ARRAY_BE_VIEW_VAR_HANDLE,
    DIRECT_BYTE_BUFFER_LE_VIEW_VAR_HANDLE,
    DIRECT_BYTE_BUFFER_BE_VIEW_VAR_HANDLE,
    HEAP_BYTE_BUFFER_LE_VIEW_VAR_HANDLE,
    HEAP_BYTE_BUFFER_BE_VIEW_VAR_HANDLE,
    HEAP_BYTE_BUFFER_RO_LE_VIEW_VAR_HANDLE,
    HEAP_BYTE_BUFFER_RO_BE_VIEW_VAR_HANDLE
]

ALL_VAR_HANDLE_KINDS = ALL_FIELD_VAR_HANDLE_KINDS + [ ARRAY_ELEMENT_VAR_HANDLE ] + ALL_BYTE_VIEW_VAR_HANDLE_KINDS

class AccessModeForm(Enum):
    GET = 0
    SET = 1
    STRONG_COMPARE_AND_SET = 2
    WEAK_COMPARE_AND_SET = 3
    COMPARE_AND_EXCHANGE = 4
    GET_AND_SET = 5
    GET_AND_UPDATE_BITWISE = 6
    GET_AND_UPDATE_NUMERIC = 7

class VarHandleAccessor:
    def __init__(self, method_name):
        self.method_name = method_name
        self.access_mode = self.get_access_mode(method_name)
        self.access_mode_form = self.get_access_mode_form(method_name)

    def get_return_type(self, var_type):
        if self.access_mode_form == AccessModeForm.SET:
            return None
        elif (self.access_mode_form == AccessModeForm.STRONG_COMPARE_AND_SET or
              self.access_mode_form == AccessModeForm.WEAK_COMPARE_AND_SET):
            return BOOLEAN_TYPE
        else:
            return var_type

    def get_number_of_var_type_arguments(self):
        if self.access_mode_form == AccessModeForm.GET:
            return 0
        elif (self.access_mode_form == AccessModeForm.SET or
              self.access_mode_form == AccessModeForm.GET_AND_SET or
              self.access_mode_form == AccessModeForm.GET_AND_UPDATE_BITWISE or
              self.access_mode_form == AccessModeForm.GET_AND_UPDATE_NUMERIC):
            return 1
        elif (self.access_mode_form == AccessModeForm.STRONG_COMPARE_AND_SET or
              self.access_mode_form == AccessModeForm.WEAK_COMPARE_AND_SET or
              self.access_mode_form == AccessModeForm.COMPARE_AND_EXCHANGE):
            return 2
        else:
            raise ValueError(self.access_mode_form)

    def is_read_only(self):
        return self.access_mode_form == AccessModeForm.GET

    def get_java_bitwise_operator(self):
        if "BitwiseAnd" in self.method_name:
            return "&"
        elif "BitwiseOr" in self.method_name:
            return "|"
        elif "BitwiseXor" in self.method_name:
            return "^"
        raise ValueError(self.method_name)

    def get_java_numeric_operator(self):
        if "Add" in self.method_name:
            return "+"
        raise ValueError(self.method_name)

    @staticmethod
    def get_access_mode(accessor_method):
        """Converts an access method name to AccessMode value. For example, getAndSet becomes GET_AND_SET"""
        return re.sub('([A-Z])', r'_\1', accessor_method).upper()

    @staticmethod
    def get_access_mode_form(accessor_method):
        prefix_mode_list = [
            ('getAndAdd', AccessModeForm.GET_AND_UPDATE_NUMERIC),
            ('getAndBitwise', AccessModeForm.GET_AND_UPDATE_BITWISE),
            ('getAndSet', AccessModeForm.GET_AND_SET),
            ('get', AccessModeForm.GET),
            ('set', AccessModeForm.SET),
            ('compareAndSet', AccessModeForm.STRONG_COMPARE_AND_SET),
            ('weakCompareAndSet', AccessModeForm.WEAK_COMPARE_AND_SET),
            ('compareAndExchange', AccessModeForm.COMPARE_AND_EXCHANGE)]
        for prefix, mode in prefix_mode_list:
            if accessor_method.startswith(prefix):
                return mode
        raise ValueError(accessor_method)

VAR_HANDLE_ACCESSORS = [
    VarHandleAccessor('get'),
    VarHandleAccessor('set'),
    VarHandleAccessor('getVolatile'),
    VarHandleAccessor('setVolatile'),
    VarHandleAccessor('getAcquire'),
    VarHandleAccessor('setRelease'),
    VarHandleAccessor('getOpaque'),
    VarHandleAccessor('setOpaque'),
    VarHandleAccessor('compareAndSet'),
    VarHandleAccessor('compareAndExchange'),
    VarHandleAccessor('compareAndExchangeAcquire'),
    VarHandleAccessor('compareAndExchangeRelease'),
    VarHandleAccessor('weakCompareAndSetPlain'),
    VarHandleAccessor('weakCompareAndSet'),
    VarHandleAccessor('weakCompareAndSetAcquire'),
    VarHandleAccessor('weakCompareAndSetRelease'),
    VarHandleAccessor('getAndSet'),
    VarHandleAccessor('getAndSetAcquire'),
    VarHandleAccessor('getAndSetRelease'),
    VarHandleAccessor('getAndAdd'),
    VarHandleAccessor('getAndAddAcquire'),
    VarHandleAccessor('getAndAddRelease'),
    VarHandleAccessor('getAndBitwiseOr'),
    VarHandleAccessor('getAndBitwiseOrRelease'),
    VarHandleAccessor('getAndBitwiseOrAcquire'),
    VarHandleAccessor('getAndBitwiseAnd'),
    VarHandleAccessor('getAndBitwiseAndRelease'),
    VarHandleAccessor('getAndBitwiseAndAcquire'),
    VarHandleAccessor('getAndBitwiseXor'),
    VarHandleAccessor('getAndBitwiseXorRelease'),
    VarHandleAccessor('getAndBitwiseXorAcquire')
]

# Pseudo-RNG used for arbitrary decisions
RANDOM = Random(0)

BANNER = '// This file is generated by util-src/generate_java.py do not directly modify!'

# List of generated test classes
GENERATED_TEST_CLASSES = []

def java_file_for_class(class_name):
    return class_name + ".java"

def capitalize_first(word):
    return word[0].upper() + word[1:]

def indent_code(code):
    """Applies rudimentary indentation to code"""
    return code

def build_template_dictionary(test_class, var_handle_kind, accessor, var_type):
    initial_value = RANDOM.choice(var_type.examples)
    updated_value = RANDOM.choice(list(filter(lambda v : v != initial_value, var_type.examples)))
    coordinates = ", ".join(var_handle_kind.get_coordinates())
    if accessor.get_number_of_var_type_arguments() != 0 and coordinates != "":
        coordinates += ", "
    dictionary = {
        'accessor_method' : accessor.method_name,
        'access_mode' : accessor.access_mode,
        'banner' : BANNER,
        'coordinates' : coordinates,
        'initial_value' : initial_value,
        'test_class' : test_class,
        'updated_value' : updated_value,
        'var_type' : var_type,
    }
    dictionary['imports'] = ";\n".join(list(map(lambda x: "import " + x, var_handle_kind.get_imports())))
    dictionary['lookup'] = var_handle_kind.get_lookup(dictionary)
    dictionary['field_declarations'] = ";\n".join(var_handle_kind.get_field_declarations(dictionary))
    dictionary['read_value'] = var_handle_kind.get_value(dictionary)
    return dictionary

def emit_accessor_test(var_handle_kind, accessor, var_type, output_path):
    test_class = var_handle_kind.get_name() + capitalize_first(accessor.method_name) + capitalize_first(var_type.name)
    GENERATED_TEST_CLASSES.append(test_class)
    src_file_path = output_path / java_file_for_class(test_class)
    expansions = build_template_dictionary(test_class, var_handle_kind, accessor, var_type)
    # Compute test operation
    if accessor.access_mode_form == AccessModeForm.GET:
        test_template = Template("""
        ${var_type} value = (${var_type}) vh.${accessor_method}(${coordinates});
        assertEquals(${initial_value}, value);""")
    elif accessor.access_mode_form == AccessModeForm.SET:
        test_template = Template("""
        vh.${accessor_method}(${coordinates}${updated_value});
        assertEquals(${updated_value}, ${read_value});""")
    elif accessor.access_mode_form == AccessModeForm.STRONG_COMPARE_AND_SET:
        test_template = Template("""
        assertEquals(${initial_value}, ${read_value});
        // Test an update that should succeed.
        boolean applied = (boolean) vh.${accessor_method}(${coordinates}${initial_value}, ${updated_value});
        assertEquals(${updated_value}, ${read_value});
        assertTrue(applied);
        // Test an update that should fail.
        applied = (boolean) vh.${accessor_method}(${coordinates}${initial_value}, ${initial_value});
        assertFalse(applied);
        assertEquals(${updated_value}, ${read_value});""")
    elif accessor.access_mode_form == AccessModeForm.WEAK_COMPARE_AND_SET:
        test_template = Template("""
        assertEquals(${initial_value}, ${read_value});
        // Test an update that should succeed.
        int attempts = 10000;
        boolean applied;
        do {
            applied = (boolean) vh.${accessor_method}(${coordinates}${initial_value}, ${updated_value});
        } while (applied == false && attempts-- > 0);
        assertEquals(${updated_value}, ${read_value});
        assertTrue(attempts > 0);
        // Test an update that should fail.
        applied = (boolean) vh.${accessor_method}(${coordinates}${initial_value}, ${initial_value});
        assertFalse(applied);
        assertEquals(${updated_value}, ${read_value});""")
    elif accessor.access_mode_form == AccessModeForm.COMPARE_AND_EXCHANGE:
        test_template = Template("""
        // This update should succeed.
        ${var_type} witness_value = (${var_type}) vh.${accessor_method}(${coordinates}${initial_value}, ${updated_value});
        assertEquals(${initial_value}, witness_value);
        assertEquals(${updated_value}, ${read_value});
        // This update should fail.
        witness_value = (${var_type}) vh.${accessor_method}(${coordinates}${initial_value}, ${initial_value});
        assertEquals(${updated_value}, witness_value);
        assertEquals(${updated_value}, ${read_value});""")
    elif accessor.access_mode_form == AccessModeForm.GET_AND_SET:
        test_template = Template("""
        ${var_type} old_value = (${var_type}) vh.${accessor_method}(${coordinates}${updated_value});
        assertEquals(${initial_value}, old_value);
        assertEquals(${updated_value}, ${read_value});""")
    elif accessor.access_mode_form == AccessModeForm.GET_AND_UPDATE_BITWISE:
        if var_type.supports_bitwise == True:
            expansions['binop'] = accessor.get_java_bitwise_operator()
            test_template = Template("""
            ${var_type} old_value = (${var_type}) vh.${accessor_method}(${coordinates}${updated_value});
            assertEquals(${initial_value}, old_value);
            assertEquals(${initial_value} ${binop} ${updated_value}, ${read_value});""")
        else:
            test_template = Template("""
            vh.${accessor_method}(${coordinates}${initial_value}, ${updated_value});
            failUnreachable();""")
    elif accessor.access_mode_form == AccessModeForm.GET_AND_UPDATE_NUMERIC:
        if var_type.supports_numeric == True:
            expansions['binop'] = accessor.get_java_numeric_operator()
            test_template = Template("""
            ${var_type} old_value = (${var_type}) vh.${accessor_method}(${coordinates}${updated_value});
            assertEquals(${initial_value}, old_value);
            ${var_type} expected_value = (${var_type}) (${initial_value} ${binop} ${updated_value});
            assertEquals(expected_value, ${read_value});""")
        else:
            test_template = Template("""
            vh.${accessor_method}(${coordinates}${initial_value}, ${updated_value});
            failUnreachable();""")
    else:
        raise ValueError(accessor.access_mode_form)

    if var_handle_kind.may_throw_read_only and not accessor.is_read_only():
        # ByteBufferViews can be read-only and dynamically raise ReadOnlyBufferException.
        expansions['try_statement'] = "try {"
        expansions['catch_statement'] = "failUnreachable();\n} catch (ReadOnlyBufferException ex) {}"
    else:
        expansions['try_statement'] = ""
        expansions['catch_statement'] = ""

    expansions['test_body'] = test_template.safe_substitute(expansions)

    s = Template("""${banner}

${imports};

class ${test_class} extends VarHandleUnitTest {
    ${field_declarations};
    static final VarHandle vh;
    static {
        try {
            vh = ${lookup};
        } catch (Exception e) {
            throw new RuntimeException("Unexpected initialization exception", e);
        }
    }

    @Override
    public void doTest() throws Exception {
        if (!vh.isAccessModeSupported(VarHandle.AccessMode.${access_mode})) {
            try {
                ${test_body}
                failUnreachable();
            } catch (UnsupportedOperationException ex) {}
        } else {
            ${try_statement}
            ${test_body}
            ${catch_statement}
        }
    }

    public static void main(String[] args) {
         new ${test_class}().run();
    }
}
""").safe_substitute(expansions)
    with src_file_path.open("w") as src_file:
        print(s, file=src_file)

def emit_value_type_accessor_tests(output_path):
    for var_handle_kind in ALL_VAR_HANDLE_KINDS:
        for accessor in VAR_HANDLE_ACCESSORS:
            for var_type in var_handle_kind.get_supported_types():
                emit_accessor_test(var_handle_kind, accessor, var_type, output_path)

def emit_reference_accessor_tests(output_path):
    ref_type = JavaType("Widget", [ "Widget.ONE", "Widget.TWO", "null" ])
    for var_handle_kind in ALL_VAR_HANDLE_KINDS:
        if var_handle_kind.is_view():
            # Views as reference type arrays are not supported. They
            # fail instantiation. This is tested in 710-varhandle-creation.
            continue
        for accessor in VAR_HANDLE_ACCESSORS:
            emit_accessor_test(var_handle_kind, accessor, ref_type, output_path)

def emit_boxing_value_type_accessor_test(accessor, var_type, output_path):
    test_class = "Boxing" + capitalize_first(accessor.method_name) + capitalize_first(var_type.name)
    GENERATED_TEST_CLASSES.append(test_class)
    src_file_path = output_path / java_file_for_class(test_class)
    var_handle_kind = FIELD_VAR_HANDLE
    expansions = build_template_dictionary(test_class, var_handle_kind, accessor, var_type)
    template = Template("""
${banner}

${imports};
import java.lang.invoke.WrongMethodTypeException;

public class ${test_class} extends VarHandleUnitTest {
    ${field_declarations};
    private static final VarHandle vh;
    static {
        try {
            vh = ${lookup};
        } catch (Exception e) {
            throw new RuntimeException("Unexpected initialization exception", e);
        }
    }

    @Override
    public void doTest() throws Exception {
       ${body}
    }

    public static void main(String[] args) {
         new ${test_class}().run();
    }
}
""")
    with io.StringIO() as body_text:
        compatible_types = types_that_widen_to(var_type)
        for value_type in VALUE_TYPES:
            print("try {", file=body_text)
            return_type = accessor.get_return_type(var_type)
            if return_type:
                print("{0} result = ({0}) ".format(return_type), end="", file=body_text)
            print("vh.{0}(this".format(accessor.method_name), end="", file=body_text)
            num_args = accessor.get_number_of_var_type_arguments()
            for i in range(0, num_args):
                print(", {0}({1})".format(value_type.boxing_method(), value_type.examples[i]), end="", file=body_text)
            print(");", file=body_text)
            if value_type in compatible_types:
                print("   assertTrue(vh.isAccessModeSupported(VarHandle.AccessMode.{0}));".format(accessor.access_mode),
                      file=body_text)
            else:
                print("failUnreachable();", file=body_text)
                print("} catch (WrongMethodTypeException e) {", file=body_text)
            print("} catch (UnsupportedOperationException e) {", file=body_text)
            print("   assertFalse(vh.isAccessModeSupported(VarHandle.AccessMode.{0}));".format(accessor.access_mode),
                  file=body_text)
            print("}", file=body_text)
        expansions['body'] = body_text.getvalue();
        with src_file_path.open("w") as src_file:
            print(template.safe_substitute(expansions), file=src_file)

def emit_boxing_return_value_type_test(accessor, var_type, output_path):
    test_class = "BoxingReturn" + capitalize_first(accessor.method_name) + capitalize_first(var_type.name)
    GENERATED_TEST_CLASSES.append(test_class)
    src_file_path = output_path / java_file_for_class(test_class)
    var_handle_kind = FIELD_VAR_HANDLE
    expansions = build_template_dictionary(test_class, var_handle_kind, accessor, var_type)
    template = Template("""
${banner}

${imports};
import java.lang.invoke.WrongMethodTypeException;

public class ${test_class} extends VarHandleUnitTest {
    ${field_declarations};
    private static final VarHandle vh;
    static {
        try {
            vh = ${lookup};
        } catch (Exception e) {
            throw new RuntimeException("Unexpected initialization exception", e);
        }
    }

    @Override
    public void doTest() throws Exception {
       ${body}
    }

    public static void main(String[] args) {
         new ${test_class}().run();
    }
}
""")
    with io.StringIO() as body_text:
        return_type = accessor.get_return_type(var_type)
        compatible_types = { return_type }
        for value_type in VALUE_TYPES:
            print("try {", file=body_text)
            print("{0} result = ({0}) ".format(value_type.boxed_type), end="", file=body_text)
            print("vh.{0}(this".format(accessor.method_name), end="", file=body_text)
            num_args = accessor.get_number_of_var_type_arguments()
            for i in range(0, num_args):
                print(", {0})".format(var_type.examples[i]), end="", file=body_text)
            print(");", file=body_text)
            if value_type in compatible_types:
                print("   assertTrue(vh.isAccessModeSupported(VarHandle.AccessMode.{0}));".format(accessor.access_mode),
                      file=body_text)
            else:
                print("failUnreachable();", file=body_text)
                print("} catch (WrongMethodTypeException e) {", file=body_text)
            print("} catch (UnsupportedOperationException e) {", file=body_text)
            print("   assertFalse(vh.isAccessModeSupported(VarHandle.AccessMode.{0}));".format(accessor.access_mode),
                  file=body_text)
            print("}", file=body_text)
        expansions['body'] = body_text.getvalue();
        with src_file_path.open("w") as src_file:
            print(template.safe_substitute(expansions), file=src_file)

def emit_boxing_value_type_accessor_tests(output_path):
    for var_type in VALUE_TYPES:
        for accessor in VAR_HANDLE_ACCESSORS:
            if accessor.get_number_of_var_type_arguments() > 0:
                emit_boxing_value_type_accessor_test(accessor, var_type, output_path)
            else:
                emit_boxing_return_value_type_test(accessor, var_type, output_path)

def emit_main(output_path, manual_test_classes):
    main_file_path = output_path / "Main.java"
    all_test_classes = GENERATED_TEST_CLASSES + manual_test_classes
    with main_file_path.open("w") as main_file:
        print("// " + BANNER, file=main_file)
        print("""
public class Main {
    public static void main(String[] args) {
""", file=main_file)
        for cls in all_test_classes:
            print("         " + cls + ".main(args);", file=main_file)
        print("        VarHandleUnitTest.DEFAULT_COLLECTOR.printSummary();", file=main_file)
        print("        System.exit(VarHandleUnitTest.DEFAULT_COLLECTOR.failuresOccurred() ? 1 : 0);", file=main_file)
        print("    }\n}", file=main_file)

def main(argv):
    final_java_dir = Path(argv[1])
    if not final_java_dir.exists() or not final_java_dir.is_dir():
        print("{} is not a valid java dir".format(final_java_dir), file=sys.stderr)
        sys.exit(1)
    emit_value_type_accessor_tests(final_java_dir)
    emit_reference_accessor_tests(final_java_dir)
    emit_boxing_value_type_accessor_tests(final_java_dir)
    emit_main(final_java_dir, argv[2:])

if __name__ == '__main__':
    main(sys.argv)
