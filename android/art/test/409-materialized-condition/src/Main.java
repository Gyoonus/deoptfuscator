/*
 * Copyright (C) 2014 The Android Open Source Project
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

  public static void doNothing(boolean b) {
    System.out.println("In do nothing.");
  }

  public static void inIf() {
    System.out.println("In if.");
  }

  public static int bar() {
    return 42;
  }

  public static int foo1() {
    int b = bar();
    doNothing(b == 42);
    // This second `b == 42` will be GVN'ed away.
    if (b == 42) {
      inIf();
      return b;
    }
    return 0;
  }

  public static int foo2() {
    int b = bar();
    doNothing(b == 41);
    // This second `b == 41` will be GVN'ed away.
    if (b == 41) {
      inIf();
      return 0;
    }
    return b;
  }

  public static boolean $noinline$intEq0(int x) {
    return x == 0;
  }

  public static boolean $noinline$intNe0(int x) {
    return x != 0;
  }

  public static boolean $noinline$longEq0(long x) {
    return x == 0;
  }

  public static boolean $noinline$longNe0(long x) {
    return x != 0;
  }

  public static boolean $noinline$longEqCst(long x) {
    return x == 0x0123456789ABCDEFL;
  }

  public static boolean $noinline$longNeCst(long x) {
    return x != 0x0123456789ABCDEFL;
  }

  public static void assertEqual(boolean expected, boolean actual) {
    if (expected != actual) {
      throw new Error("Assertion failed: " + expected + " != " + actual);
    }
  }

  // The purpose of this method is to test code generation for a materialized
  // HCondition that is not equality or inequality, and that has one boolean
  // input. That can't be done directly, so we have to rely on the instruction
  // simplifier to transform the control-flow graph appropriately.
  public static boolean $noinline$booleanCondition(boolean in) {
    int value = in ? 1 : 0;

    // Calling a non-inlineable method that uses `value` as well prevents a
    // transformation of the return value into `false`.
    $noinline$intNe0(value);
    return value > 127;
  }

  public static void main(String[] args) {
    System.out.println("foo1");
    int res = foo1();
    if (res != 42) {
      throw new Error("Unexpected return value for foo1: " + res + ", expected 42.");
    }

    System.out.println("foo2");
    res = foo2();
    if (res != 42) {
      throw new Error("Unexpected return value for foo2: " + res + ", expected 42.");
    }

    assertEqual($noinline$booleanCondition(false), false);
    assertEqual($noinline$booleanCondition(true), false);

    int[] int_inputs = {0, 1, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, 42, -9000};
    long[] long_inputs = {
        0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE, 0x100000000L,
        0x100000001L, -9000L, 0x0123456789ABCDEFL};

    boolean[] int_eq_0_expected = {true, false, false, false, false, false, false};

    for (int i = 0; i < int_inputs.length; i++) {
      assertEqual(int_eq_0_expected[i], $noinline$intEq0(int_inputs[i]));
    }

    boolean[] int_ne_0_expected = {false, true, true, true, true, true, true};

    for (int i = 0; i < int_inputs.length; i++) {
      assertEqual(int_ne_0_expected[i], $noinline$intNe0(int_inputs[i]));
    }

    boolean[] long_eq_0_expected = {true, false, false, false, false, false, false, false, false};

    for (int i = 0; i < long_inputs.length; i++) {
      assertEqual(long_eq_0_expected[i], $noinline$longEq0(long_inputs[i]));
    }

    boolean[] long_ne_0_expected = {false, true, true, true, true, true, true, true, true};

    for (int i = 0; i < long_inputs.length; i++) {
      assertEqual(long_ne_0_expected[i], $noinline$longNe0(long_inputs[i]));
    }

    boolean[] long_eq_cst_expected = {false, false, false, false, false, false, false, false, true};

    for (int i = 0; i < long_inputs.length; i++) {
      assertEqual(long_eq_cst_expected[i], $noinline$longEqCst(long_inputs[i]));
    }

    boolean[] long_ne_cst_expected = {true, true, true, true, true, true, true, true, false};

    for (int i = 0; i < long_inputs.length; i++) {
      assertEqual(long_ne_cst_expected[i], $noinline$longNeCst(long_inputs[i]));
    }
  }
}
