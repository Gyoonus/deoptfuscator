/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * Tests on bounds check elimination in loops that use intrinsics.
 * All bounds checks below should be statically eliminated.
 */
public class Main {

  /// CHECK-START: int Main.oneArray(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  //
  /// CHECK-START: int Main.oneArray(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int oneArray(int[] a) {
    int x = 0;
    for (int i = 0; i < a.length; i++) {
      x += a[i];
    }
    return x;
  }

  /// CHECK-START: int Main.oneArrayAbs(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  //
  /// CHECK-START: int Main.oneArrayAbs(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int oneArrayAbs(int[] a, int[] b) {
    int x = 0;
    for (int i = Math.abs(b.length); i < a.length; i++) {
      x += a[i];
    }
    return x;
  }


  /// CHECK-START: int Main.twoArrays(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.twoArrays(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int twoArrays(int[] a, int[] b) {
    int x = 0;
    for (int i = 0; i < Math.min(a.length, b.length); i++) {
      x += a[i] + b[i];
    }
    return x;
  }

  /// CHECK-START: int Main.threeArrays(int[], int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.threeArrays(int[], int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int threeArrays(int[] a, int[] b, int[] c) {
    int x = 0;
    for (int i = 0; i < Math.min(Math.min(a.length, b.length), c.length); i++) {
      x += a[i] + b[i] + c[i];
    }
    return x;
  }

  /// CHECK-START: int Main.fourArrays(int[], int[], int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.fourArrays(int[], int[], int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int fourArrays(int[] a, int[] b, int[] c, int[] d) {
    int x = 0;
    for (int i = 0; i < Math.min(Math.min(a.length, b.length), Math.min(c.length, d.length)); i++) {
      x += a[i] + b[i] + c[i] + d[i];
    }
    return x;
  }

  /// CHECK-START: int Main.oneArrayWithCleanup(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop2:B\d+>> outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START: int Main.oneArrayWithCleanup(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int oneArrayWithCleanup(int[] a) {
    int x = 0;
    int n = Math.min(4, a.length);
    for (int i = 0; i < n; i++) {
      x += a[i];
    }
    for (int i = n; i < a.length; i++) {
      x += a[i] * 10;
    }
    return x;
  }

  /// CHECK-START: int Main.twoArraysWithCleanup(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop2:B\d+>> outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START: int Main.twoArraysWithCleanup(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int twoArraysWithCleanup(int[] a, int[] b) {
    int x = 0;
    int n = Math.min(a.length, b.length);
    for (int i = n - 1; i >= 0; i--) {
      x += a[i] + b[i];
    }
    for (int i = n; i < a.length; i++) {
      x += a[i];
    }
    return x;
  }

  /// CHECK-START: int Main.threeArraysWithCleanup(int[], int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop2:B\d+>> outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START: int Main.threeArraysWithCleanup(int[], int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int threeArraysWithCleanup(int[] a, int[] b, int[] c) {
    int x = 0;
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = n - 1; i >= 0; i--) {
      x += a[i] + b[i] + c[i];
    }
    for (int i = n; i < a.length; i++) {
      x += a[i];
    }
    return x;
  }

  /// CHECK-START: int Main.altLoopLogic(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.altLoopLogic(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int altLoopLogic(int[] a, int[] b) {
    int x = 0;
    int n = Math.min(a.length, b.length);
    for (int i = n; i-- > 0;) {
      x += a[i] + b[i];
    }
    return x;
  }

  /// CHECK-START: int Main.hiddenMin(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.hiddenMin(int[], int[]) BCE (after)
  //
  // TODO: make this so
  static int hiddenMin(int[] a, int[] b) {
    int x = 0;
    for (int i = 0; i < a.length && i < b.length; i++) {
      x += a[i] + b[i];
    }
    return x;
  }

  /// CHECK-START: int Main.hiddenMinWithCleanup(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop2:B\d+>> outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START: int Main.hiddenMinWithCleanup(int[], int[]) BCE (after)
  //
  // TODO: make this so
  static int hiddenMinWithCleanup(int[] a, int[] b) {
    int x = 0;
    int i = 0;
    for (; i < a.length && i < b.length; i++) {
      x += a[i] + b[i];
    }
    for (; i < a.length; i++) {
      x += a[i];
    }
    return x;
  }

  public static void main(String[] args) {
    int[] a = { 1, 2, 3, 4, 5 };
    int[] b = { 6, 7, 8, 9, 4, 2 };
    int[] c = { 1, 2, 3 };
    int[] d = { 8, 5, 3, 2 };
    int[] e = { };

    expectEquals(15, oneArray(a));
    expectEquals(36, oneArray(b));
    expectEquals(6,  oneArray(c));
    expectEquals(18, oneArray(d));

    expectEquals(15, oneArrayAbs(a, e));
    expectEquals(5,  oneArrayAbs(a, d));

    expectEquals(30, twoArrays(a, a));
    expectEquals(49, twoArrays(a, b));
    expectEquals(12, twoArrays(a, c));
    expectEquals(28, twoArrays(a, d));

    expectEquals(45, threeArrays(a, a, a));
    expectEquals(33, threeArrays(a, b, c));
    expectEquals(58, threeArrays(a, b, d));
    expectEquals(28, threeArrays(a, c, d));

    expectEquals(60, fourArrays(a, a, a, a));
    expectEquals(49, fourArrays(a, b, c, d));

    expectEquals(60, oneArrayWithCleanup(a));
    expectEquals(90, oneArrayWithCleanup(b));
    expectEquals(6,  oneArrayWithCleanup(c));
    expectEquals(18, oneArrayWithCleanup(d));

    expectEquals(30, twoArraysWithCleanup(a, a));
    expectEquals(49, twoArraysWithCleanup(a, b));
    expectEquals(21, twoArraysWithCleanup(a, c));
    expectEquals(33, twoArraysWithCleanup(a, d));

    expectEquals(45, threeArraysWithCleanup(a, a, a));
    expectEquals(42, threeArraysWithCleanup(a, b, c));
    expectEquals(63, threeArraysWithCleanup(a, b, d));
    expectEquals(37, threeArraysWithCleanup(a, c, d));

    expectEquals(30, altLoopLogic(a, a));
    expectEquals(49, altLoopLogic(a, b));
    expectEquals(12, altLoopLogic(a, c));
    expectEquals(28, altLoopLogic(a, d));

    expectEquals(30, hiddenMin(a, a));
    expectEquals(49, hiddenMin(a, b));
    expectEquals(12, hiddenMin(a, c));
    expectEquals(28, hiddenMin(a, d));

    expectEquals(30, hiddenMinWithCleanup(a, a));
    expectEquals(49, hiddenMinWithCleanup(a, b));
    expectEquals(21, hiddenMinWithCleanup(a, c));
    expectEquals(33, hiddenMinWithCleanup(a, d));

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
