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

//
// Test on loop optimizations, in particular dynamic BCE. In all cases,
// bounds check on a[] is resolved statically. Bounds checks on b[]
// exercise various different scenarios. In all cases, loop-based
// dynamic BCE is better than the dominator-based BCE, since it
// generates the test outside the loop.
//
public class Main {

  /// CHECK-START: void Main.oneConstantIndex(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.oneConstantIndex(int[], int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.oneConstantIndex(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void oneConstantIndex(int[] a, int[] b) {
    // Dynamic bce on b requires two deopts: one null and one bound.
    for (int i = 0; i < a.length; i++) {
      a[i] = b[1];
    }
  }

  /// CHECK-START: void Main.multipleConstantIndices(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.multipleConstantIndices(int[], int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleConstantIndices(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void multipleConstantIndices(int[] a, int[] b) {
    // Dynamic bce on b requires two deopts: one null and one bound.
    for (int i = 0; i < a.length; i++) {
      a[i] = b[0] + b[1] + b[2];
    }
  }

  /// CHECK-START: void Main.oneInvariantIndex(int[], int[], int) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.oneInvariantIndex(int[], int[], int) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.oneInvariantIndex(int[], int[], int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void oneInvariantIndex(int[] a, int[] b, int c) {
    // Dynamic bce on b requires two deopts: one null and one bound.
    for (int i = 0; i < a.length; i++) {
      a[i] = b[c];
    }
  }

  /// CHECK-START: void Main.multipleInvariantIndices(int[], int[], int) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.multipleInvariantIndices(int[], int[], int) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleInvariantIndices(int[], int[], int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void multipleInvariantIndices(int[] a, int[] b, int c) {
    // Dynamic bce on b requires three deopts: one null and two bounds.
    for (int i = 0; i < a.length; i++) {
      a[i] = b[c-1] + b[c] + b[c+1];
    }
  }

  /// CHECK-START: void Main.oneUnitStride(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.oneUnitStride(int[], int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.oneUnitStride(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void oneUnitStride(int[] a, int[] b) {
    // Dynamic bce on b requires three deopts: one null and two bounds.
    for (int i = 0; i < a.length; i++) {
      a[i] = b[i];
    }
  }

  /// CHECK-START: void Main.multipleUnitStrides(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.multipleUnitStrides(int[], int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleUnitStrides(int[], int[]) instruction_simplifier$after_bce (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleUnitStrides(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void multipleUnitStrides(int[] a, int[] b) {
    // Dynamic bce on b requires four deopts: one null and three bounds.
    // One redundant deopt is removed by simplifier.
    // TODO: range information could remove another
    for (int i = 1; i < a.length - 1; i++) {
      a[i] = b[i-1] + b[i] + b[i+1];
    }
  }

  /// CHECK-START: void Main.multipleUnitStridesConditional(int[], int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.multipleUnitStridesConditional(int[], int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleUnitStridesConditional(int[], int[]) instruction_simplifier$after_bce (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.multipleUnitStridesConditional(int[], int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void multipleUnitStridesConditional(int[] a, int[] b) {
    // Dynamic bce on b requires four deopts: one null and three bounds.
    // The two conditional references may be included, since they are in range.
    // One redundant deopt is removed by simplifier.
    for (int i = 2; i < a.length - 2; i++) {
      int t = b[i-2] + b[i] + b[i+2] + (((i & 1) == 0) ? b[i+1] : b[i-1]);
      a[i] = t;
    }
  }

  /// CHECK-START: void Main.shifter(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.shifter(int[]) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.shifter(int[]) instruction_simplifier$after_bce (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.shifter(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void shifter(int[] x) {
    // Real-life example: should have four deopts: one null and three bounds.
    // Two redundant deopts are removed by simplifier.
    for (int i = 16; i < 80; i++) {
      int t = x[i - 3] ^ x[i - 8] ^ x[i - 14] ^ x[i - 16];
      x[i] = t << 1 | t >>> 31;
    }
  }

  /// CHECK-START: void Main.stencil(int[], int, int) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: void Main.stencil(int[], int, int) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.stencil(int[], int, int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void stencil(int[] array, int start, int end) {
    // Real-life example: should have four deopts: one null and three bounds.
    for (int i = end; i >= start; i--) {
      array[i] = (array[i-2] + array[i-1] + array[i] + array[i+1] + array[i+2]) / 5;
    }
  }

  /// CHECK-START: void Main.shortBound1(int[], short) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: void Main.shortBound1(int[], short) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.shortBound1(int[], short) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void shortBound1(int[] array, short s) {
    // Lower precision bound will appear in deopt arithmetic
    // and follows normal implicit widening conversion.
    for (int i = 0; i < s; i++) {
      array[i] = 222;
    }
  }

  /// CHECK-START: void Main.shortBound2(int[], short) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: void Main.shortBound2(int[], short) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-DAG: Deoptimize loop:none
  /// CHECK-NOT: Deoptimize
  //
  /// CHECK-START: void Main.shortBound2(int[], short) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void shortBound2(int[] array, short s) {
    // Lower precision bound will appear in deopt arithmetic
    // and follows normal implicit widening conversion.
    for (int i = 0; s > i; i++) {
      array[i] = 444;
    }
  }

  /// CHECK-START: void Main.narrowingFromLong(int[], int) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: void Main.narrowingFromLong(int[], int) BCE (after)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  public static void narrowingFromLong(int[] array, int n) {
    // Parallel induction in long precision that is narrowed provides type
    // conversion challenges for BCE in deopt arithmetic when combined
    // with the int loop induction. Therefore, currently skipped.
    long l = 0;
    for (int i = 0; i < n; i++, l++) {
      array[(int)l] = 888;
    }
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    int[] a = new int[10];
    int b[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int b1[] = { 100 };

    oneConstantIndex(a, b);
    for (int i = 0; i < a.length; i++) {
      expectEquals(2, a[i]);
    }
    try {
      oneConstantIndex(a, b1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    multipleConstantIndices(a, b);
    for (int i = 0; i < a.length; i++) {
      expectEquals(6, a[i]);
    }
    try {
      multipleConstantIndices(a, b1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    oneInvariantIndex(a, b, 1);
    for (int i = 0; i < a.length; i++) {
      expectEquals(2, a[i]);
    }
    try {
      oneInvariantIndex(a, b1, 1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    multipleInvariantIndices(a, b, 1);
    for (int i = 0; i < a.length; i++) {
      expectEquals(6, a[i]);
    }
    try {
      multipleInvariantIndices(a, b1, 1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    oneUnitStride(a, b);
    for (int i = 0; i < a.length; i++) {
      expectEquals(i + 1, a[i]);
    }
    try {
      oneUnitStride(a, b1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
      expectEquals(100, a[0]);
    }

    multipleUnitStrides(a, b);
    for (int i = 1; i < a.length - 1; i++) {
      expectEquals(3 * i + 3, a[i]);
    }
    try {
      multipleUnitStrides(a, b1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    multipleUnitStridesConditional(a, b);
    for (int i = 2; i < a.length - 2; i++) {
      int e = 3 * i + 3 + (((i & 1) == 0) ? i + 2 : i);
      expectEquals(e, a[i]);
    }
    try {
      multipleUnitStridesConditional(a, b1);
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }

    shortBound1(a, (short)a.length);
    for (int i = 0; i < a.length; i++) {
      expectEquals(222, a[i]);
    }
    shortBound2(a, (short)a.length);
    for (int i = 0; i < a.length; i++) {
      expectEquals(444, a[i]);
    }

    try {
      shortBound1(a, (short)(a.length + 1));
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }
    for (int i = 0; i < a.length; i++) {
      expectEquals(222, a[i]);
    }

    try {
      shortBound2(a, (short)(a.length + 1));
      throw new Error("Should throw AIOOBE");
    } catch (ArrayIndexOutOfBoundsException e) {
    }
    for (int i = 0; i < a.length; i++) {
      expectEquals(444, a[i]);
    }

    narrowingFromLong(a, a.length);
    for (int i = 0; i < a.length; i++) {
      expectEquals(888, a[i]);
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
