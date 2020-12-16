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

/**
 * Functional tests for SIMD vectorization.
 */
public class Main {

  static boolean[] a;

  //
  // Arithmetic operations.
  //

  /// CHECK-START: void Main.and(boolean) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.and(boolean) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAnd   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void and(boolean x) {
    for (int i = 0; i < 128; i++)
      a[i] &= x;  // NOTE: bitwise and, not the common &&
  }

  /// CHECK-START: void Main.or(boolean) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.or(boolean) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecOr    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void or(boolean x) {
    for (int i = 0; i < 128; i++)
      a[i] |= x;  // NOTE: bitwise or, not the common ||
  }

  /// CHECK-START: void Main.xor(boolean) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.xor(boolean) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecXor   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void xor(boolean x) {
    for (int i = 0; i < 128; i++)
      a[i] ^= x;  // NOTE: bitwise xor
  }

  /// CHECK-START: void Main.not() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.not() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecNot   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void not() {
    for (int i = 0; i < 128; i++)
      a[i] = !a[i];
  }

  //
  // Test Driver.
  //

  public static void main(String[] args) {
    // Set up.
    a = new boolean[128];
    for (int i = 0; i < 128; i++) {
      a[i] = (i & 1) == 0;
    }
    // Arithmetic operations.
    and(true);
    for (int i = 0; i < 128; i++) {
      expectEquals((i & 1) == 0, a[i], "and-true");
    }
    xor(true);
    for (int i = 0; i < 128; i++) {
      expectEquals((i & 1) != 0, a[i], "xor-true");
    }
    xor(false);
    for (int i = 0; i < 128; i++) {
      expectEquals((i & 1) != 0, a[i], "xor-false");
    }
    not();
    for (int i = 0; i < 128; i++) {
      expectEquals((i & 1) == 0, a[i], "not");
    }
    or(true);
    for (int i = 0; i < 128; i++) {
      expectEquals(true, a[i], "or-true");
    }
    and(false);
    for (int i = 0; i < 128; i++) {
      expectEquals(false, a[i], "and-false");
    }
    or(false);
    for (int i = 0; i < 128; i++) {
      expectEquals(false, a[i], "or-false");
    }
    // Done.
    System.out.println("passed");
  }

  private static void expectEquals(boolean expected, boolean result, String action) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result + " for " + action);
    }
  }
}
