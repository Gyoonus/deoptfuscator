/*
 * Copyright (C) 2018 The Android Open Source Project
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

  public static void main(String args[]) {
    doFloatingPointTest("1", "1.0");
    doFloatingPointTest("4", "2.0");
    checkValue(String.valueOf(doIntegerTest1(4)), "0");
    checkValue(String.valueOf(doIntegerTest2(4)), "4");

    // Another variant of the floating point test, but less brittle.
    staticField = 1;
    checkValue(String.valueOf($noinline$test()), "1.0");
    staticField = 4;
    checkValue(String.valueOf($noinline$test()), "2.0");
  }

  // This code is a reduced version of the original reproducer. The arm
  // code generator used to generate wrong code for it. Note that this
  // test is very brittle and a simple change in it could cause the compiler
  // to not trip.
  public static void doFloatingPointTest(String s, String expected) {
    float a = (float)Integer.valueOf(s);
    a = a < 2.0f ? a : 2.0f;
    checkValue("" + a, expected);
  }

  // The compiler used to trip on the two following methods. The test there
  // is very brittle and requires not running constant folding after
  // load/store elimination.
  public static int doIntegerTest1(int param) {
    Main main = new Main();
    main.field = 0;
    return (main.field == 0) ? 0 : param;
  }

  public static int doIntegerTest2(int param) {
    Main main = new Main();
    main.field = 0;
    return (main.field != 0) ? 0 : param;
  }

  public static void checkValue(String actual, String expected) {
    if (!expected.equals(actual)) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  static void $noinline$nothing() {}
  static int $noinline$getField() { return staticField; }

  static float $noinline$test() {
    // The 2.0f shall be materialized for GreaterThanOrEqual at the beginning of the method;
    // since the following call clobbers caller-saves, it is allocated to s16.
    // r0(field) = InvokeStaticOrDirect[]
    int one = $noinline$getField();
    // s0(a_1) = TypeConversion[r0(one)]
    float a = (float)one;
    // s16(a_2) = Select[s0(a_1), C(2.0f), GreaterThanOrEqual[s0(a_1), s16(2.0f)]]
    a = a < 2.0f ? a : 2.0f;
    // The following call is added to clobber caller-saves, forcing the output of the Select
    // to be allocated to s16.
    $noinline$nothing();
    return a;
  }

  int field;
  static int staticField;
}
