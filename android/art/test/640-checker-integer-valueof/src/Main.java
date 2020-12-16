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

public class Main {

  /// CHECK-START: java.lang.Integer Main.foo(int) disassembly (after)
  /// CHECK: <<Integer:l\d+>>     InvokeStaticOrDirect method_name:java.lang.Integer.valueOf intrinsic:IntegerValueOf
  /// CHECK:                      pAllocObjectInitialized
  /// CHECK:                      Return [<<Integer>>]
  public static Integer foo(int a) {
    return Integer.valueOf(a);
  }

  /// CHECK-START: java.lang.Integer Main.foo2() disassembly (after)
  /// CHECK: <<Integer:l\d+>>     InvokeStaticOrDirect method_name:java.lang.Integer.valueOf intrinsic:IntegerValueOf
  /// CHECK-NOT:                  pAllocObjectInitialized
  /// CHECK:                      Return [<<Integer>>]
  public static Integer foo2() {
    return Integer.valueOf(-42);
  }

  /// CHECK-START: java.lang.Integer Main.foo3() disassembly (after)
  /// CHECK: <<Integer:l\d+>>     InvokeStaticOrDirect method_name:java.lang.Integer.valueOf intrinsic:IntegerValueOf
  /// CHECK-NOT:                  pAllocObjectInitialized
  /// CHECK:                      Return [<<Integer>>]
  public static Integer foo3() {
    return Integer.valueOf(42);
  }

  /// CHECK-START: java.lang.Integer Main.foo4() disassembly (after)
  /// CHECK: <<Integer:l\d+>>     InvokeStaticOrDirect method_name:java.lang.Integer.valueOf intrinsic:IntegerValueOf
  /// CHECK:                      pAllocObjectInitialized
  /// CHECK:                      Return [<<Integer>>]
  public static Integer foo4() {
    return Integer.valueOf(55555);
  }

  public static void main(String[] args) {
    assertEqual("42", foo(intField));
    assertEqual(foo(intField), foo(intField2));
    assertEqual("-42", foo2());
    assertEqual("42", foo3());
    assertEqual("55555", foo4());
    assertEqual("55555", foo(intField3));
    assertEqual("-129", foo(intFieldMinus129));
    assertEqual("-128", foo(intFieldMinus128));
    assertEqual(foo(intFieldMinus128), foo(intFieldMinus128));
    assertEqual("-127", foo(intFieldMinus127));
    assertEqual(foo(intFieldMinus127), foo(intFieldMinus127));
    assertEqual("126", foo(intField126));
    assertEqual(foo(intField126), foo(intField126));
    assertEqual("127", foo(intField127));
    assertEqual(foo(intField127), foo(intField127));
    assertEqual("128", foo(intField128));
  }

  static void assertEqual(String a, Integer b) {
    if (!a.equals(b.toString())) {
      throw new Error("Expected " + a + ", got " + b);
    }
  }

  static void assertEqual(Integer a, Integer b) {
    if (a != b) {
      throw new Error("Expected " + a + ", got " + b);
    }
  }

  static int intField = 42;
  static int intField2 = 42;
  static int intField3 = 55555;

  // Edge cases.
  static int intFieldMinus129 = -129;
  static int intFieldMinus128 = -128;
  static int intFieldMinus127 = -127;
  static int intField126 = 126;
  static int intField127 = 127;
  static int intField128 = 128;
}
