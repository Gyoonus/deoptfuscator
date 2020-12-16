/*
 * Copyright (C) 2015 The Android Open Source Project
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
// Test on loop optimizations, in particular around common induction.
//
public class Main {

  static int sResult;

  //
  // Various sequence variables used in bound checks.
  //

  /// CHECK-START: int Main.linear(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linear(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linear(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearDown(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearDown(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearObscure(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearObscure(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearObscure(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      int k = i + 5;
      result += x[k - 5];
    }
    return result;
  }

  /// CHECK-START: int Main.linearVeryObscure(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearVeryObscure(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearVeryObscure(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      int k = (-i) + (i << 5) + i - (32 * i) + 5 + (int) i;
      result += x[k - 5];
    }
    return result;
  }

  /// CHECK-START: int Main.hiddenStride(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.hiddenStride(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  static int hiddenStride(int[] a) {
    int result = 0;
    for (int i = 1; i <= 1; i++) {
      // Obscured unit stride.
      for (int j = 0; j < a.length; j += i) {
        result += a[j];
      }
    }
    return result;
  }

  /// CHECK-START: int Main.linearWhile(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWhile(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearWhile(int[] x) {
    int i = 0;
    int result = 0;
    while (i < x.length) {
      result += x[i++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearThreeWayPhi(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearThreeWayPhi(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearThreeWayPhi(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; ) {
      if (x[i] == 5) {
        i++;
        continue;
      }
      result += x[i++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearFourWayPhi(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearFourWayPhi(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearFourWayPhi(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; ) {
      if (x[i] == 5) {
        i++;
        continue;
      } else if (x[i] == 6) {
        i++;
        result += 7;
        continue;
      }
      result += x[i++];
    }
    return result;
  }

  /// CHECK-START: int Main.wrapAroundThenLinear(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.wrapAroundThenLinear(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int wrapAroundThenLinear(int[] x) {
    // Loop with wrap around (length - 1, 0, 1, 2, ..).
    int w = x.length - 1;
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      result += x[w];
      w = i;
    }
    return result;
  }

  /// CHECK-START: int Main.wrapAroundThenLinearThreeWayPhi(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.wrapAroundThenLinearThreeWayPhi(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int wrapAroundThenLinearThreeWayPhi(int[] x) {
    // Loop with wrap around (length - 1, 0, 1, 2, ..).
    int w = x.length - 1;
    int result = 0;
    for (int i = 0; i < x.length; ) {
       if (x[w] == 1) {
         w = i++;
         continue;
       }
       result += x[w];
       w = i++;
    }
    return result;
  }

  /// CHECK-START: int[] Main.linearWithParameter(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.linearWithParameter(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int[] linearWithParameter(int n) {
    int[] x = new int[n];
    for (int i = 0; i < n; i++) {
      x[i] = i;
    }
    return x;
  }

  /// CHECK-START: int[] Main.linearCopy(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.linearCopy(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int[] linearCopy(int x[]) {
    int n = x.length;
    int y[] = new int[n];
    for (int i = 0; i < n; i++) {
      y[i] = x[i];
    }
    return y;
  }

  /// CHECK-START: int Main.linearByTwo(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearByTwo(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearByTwo(int x[]) {
    int n = x.length / 2;
    int result = 0;
    for (int i = 0; i < n; i++) {
      int ii = i << 1;
      result += x[ii];
      result += x[ii + 1];
    }
    return result;
  }

  /// CHECK-START: int Main.linearByTwoSkip1(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearByTwoSkip1(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearByTwoSkip1(int x[]) {
    int result = 0;
    for (int i = 0; i < x.length / 2; i++) {
      result += x[2 * i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearByTwoSkip2(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearByTwoSkip2(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearByTwoSkip2(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int linearByTwoSkip2(int x[]) {
    int result = 0;
    // This case is not optimized.
    for (int i = 0; i < x.length; i+=2) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearWithCompoundStride() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithCompoundStride() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearWithCompoundStride() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    int result = 0;
    for (int i = 0; i <= 12; ) {
      i++;
      result += x[i];
      i++;
    }
    return result;
  }

  /// CHECK-START: int Main.linearWithLargePositiveStride() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithLargePositiveStride() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearWithLargePositiveStride() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int result = 0;
    int k = 0;
    // Range analysis has no problem with a trip-count defined by a
    // reasonably large positive stride far away from upper bound.
    for (int i = 1; i <= 10 * 10000000 + 1; i += 10000000) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearWithVeryLargePositiveStride() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithVeryLargePositiveStride() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithVeryLargePositiveStride() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int linearWithVeryLargePositiveStride() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int result = 0;
    int k = 0;
    // Range analysis conservatively bails due to potential of wrap-around
    // arithmetic while computing the trip-count for this very large stride.
    for (int i = 1; i < Integer.MAX_VALUE; i += 195225786) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearWithLargeNegativeStride() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithLargeNegativeStride() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearWithLargeNegativeStride() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int result = 0;
    int k = 0;
    // Range analysis has no problem with a trip-count defined by a
    // reasonably large negative stride far away from lower bound.
    for (int i = -1; i >= -10 * 10000000 - 1; i -= 10000000) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearWithVeryLargeNegativeStride() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithVeryLargeNegativeStride() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearWithVeryLargeNegativeStride() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int linearWithVeryLargeNegativeStride() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    int result = 0;
    int k = 0;
    // Range analysis conservatively bails due to potential of wrap-around
    // arithmetic while computing the trip-count for this very large stride.
    for (int i = -2; i > Integer.MIN_VALUE; i -= 195225786) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.linearForNEUp() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearForNEUp() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearForNEUp() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = 0; i != 10; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearForNEDown() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearForNEDown() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearForNEDown() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = 9; i != -1; i--) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearForNEArrayLengthUp(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearForNEArrayLengthUp(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearForNEArrayLengthUp(int[] x) {
    int result = 0;
    for (int i = 0; i != x.length; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearForNEArrayLengthDown(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearForNEArrayLengthDown(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearForNEArrayLengthDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i != -1; i--) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearDoWhileUp() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearDoWhileUp() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearDoWhileUp() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    int i = 0;
    do {
      result += x[i++];
    } while (i < 10);
    return result;
  }

  /// CHECK-START: int Main.linearDoWhileDown() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearDoWhileDown() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearDoWhileDown() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    int i = 9;
    do {
      result += x[i--];
    } while (0 <= i);
    return result;
  }

  /// CHECK-START: int Main.linearLong() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearLong() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearLong() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Induction on constant interval is done in higher precision than necessary,
    // but truncated at the use as subscript.
    for (long i = 0; i < 10; i++) {
      result += x[(int)i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearLongAlt(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearLongAlt(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearLongAlt(int[] x) {
    int result = 0;
    // Induction on array length is done in higher precision than necessary,
    // but truncated at the use as subscript.
    for (long i = 0; i < x.length; i++) {
      result += x[(int)i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearShort() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearShort() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearShort() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Induction is done in short precision, but fits.
    for (short i = 0; i < 10; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearChar() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearChar() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearChar() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Induction is done in char precision, but fits.
    for (char i = 0; i < 10; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearByte() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.linearByte() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int linearByte() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Induction is done in byte precision, but fits.
    for (byte i = 0; i < 10; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.invariantFromPreLoop(int[], int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.invariantFromPreLoop(int[], int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int invariantFromPreLoop(int[] x, int y) {
    int result = 0;
    // Strange pre-loop that sets upper bound.
    int hi;
    while (true) {
      y = y % 3;
      hi = x.length;
      if (y != 123) break;
    }
    for (int i = 0; i < hi; i++) {
       result += x[i];
    }
    return result;
  }

  /// CHECK-START: void Main.linearTriangularOnTwoArrayLengths(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.linearTriangularOnTwoArrayLengths(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void linearTriangularOnTwoArrayLengths(int n) {
    int[] a = new int[n];
    for (int i = 0; i < a.length; i++) {
      int[] b = new int[i];
      for (int j = 0; j < b.length; j++) {
        // Need to know j < b.length < a.length for static bce.
        a[j] += 1;
        // Need to know just j < b.length for static bce.
        b[j] += 1;
      }
      verifyTriangular(a, b, i, n);
    }
  }

  /// CHECK-START: void Main.linearTriangularOnOneArrayLength(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.linearTriangularOnOneArrayLength(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void linearTriangularOnOneArrayLength(int n) {
    int[] a = new int[n];
    for (int i = 0; i < a.length; i++) {
      int[] b = new int[i];
      for (int j = 0; j < i; j++) {
        // Need to know j < i < a.length for static bce.
        a[j] += 1;
        // Need to know just j < i for static bce.
        b[j] += 1;
      }
      verifyTriangular(a, b, i, n);
    }
  }

  /// CHECK-START: void Main.linearTriangularOnParameter(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.linearTriangularOnParameter(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void linearTriangularOnParameter(int n) {
    int[] a = new int[n];
    for (int i = 0; i < n; i++) {
      int[] b = new int[i];
      for (int j = 0; j < i; j++) {
        // Need to know j < i < n for static bce.
        a[j] += 1;
        // Need to know just j < i for static bce.
        b[j] += 1;
      }
      verifyTriangular(a, b, i, n);
    }
  }

  /// CHECK-START: void Main.linearTriangularStrictLower(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.linearTriangularStrictLower(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void linearTriangularStrictLower(int n) {
    int[] a = new int[n];
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < i; j++) {
        a[j] += 1;
      }
      for (int j = i - 1; j >= 0; j--) {
        a[j] += 1;
      }
      for (int j = i; j < n; j++) {
        a[j] += 1;
      }
      for (int j = n - 1; j >= i; j--) {
        a[j] += 1;
      }
    }
    verifyTriangular(a);
  }

  /// CHECK-START: void Main.linearTriangularStrictUpper(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.linearTriangularStrictUpper(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void linearTriangularStrictUpper(int n) {
    int[] a = new int[n];
    for (int i = 0; i < n; i++) {
      for (int j = 0; j <= i; j++) {
        a[j] += 1;
      }
      for (int j = i; j >= 0; j--) {
        a[j] += 1;
      }
      for (int j = i + 1; j < n; j++) {
        a[j] += 1;
      }
      for (int j = n - 1; j >= i + 1; j--) {
        a[j] += 1;
      }
    }
    verifyTriangular(a);
  }

  // Verifier for triangular loops.
  private static void verifyTriangular(int[] a, int[] b, int m, int n) {
    expectEquals(n, a.length);
    for (int i = 0, k = m; i < n; i++) {
      expectEquals(a[i], k);
      if (k > 0) k--;
    }
    expectEquals(m, b.length);
    for (int i = 0; i < m; i++) {
      expectEquals(b[i], 1);
    }
  }

  // Verifier for triangular loops.
  private static void verifyTriangular(int[] a) {
    int n = a.length;
    for (int i = 0; i < n; i++) {
      expectEquals(a[i], n + n);
    }
  }

  /// CHECK-START: int[] Main.linearTriangularOOB() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.linearTriangularOOB() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.linearTriangularOOB() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int[] linearTriangularOOB() {
    int[] a = new int[200];
    try {
      for (int i = 0; i < 200; i++) {
        // Lower bound must be recognized as lower precision induction with arithmetic
        // wrap-around to -128 when i exceeds 127.
        for (int j = (byte) i; j < 200; j++) {
          a[j] += 1;
        }
      }
    } catch (ArrayIndexOutOfBoundsException e) {
      return a;
    }
    return null;  // failure if this is reached
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    int[] empty = { };
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    // Linear and wrap-around.
    expectEquals(0, linear(empty));
    expectEquals(55, linear(x));
    expectEquals(0, linearDown(empty));
    expectEquals(55, linearDown(x));
    expectEquals(0, linearObscure(empty));
    expectEquals(55, linearObscure(x));
    expectEquals(0, linearVeryObscure(empty));
    expectEquals(55, linearVeryObscure(x));
    expectEquals(0, hiddenStride(empty));
    expectEquals(55, hiddenStride(x));
    expectEquals(0, linearWhile(empty));
    expectEquals(55, linearWhile(x));
    expectEquals(0, linearThreeWayPhi(empty));
    expectEquals(50, linearThreeWayPhi(x));
    expectEquals(0, linearFourWayPhi(empty));
    expectEquals(51, linearFourWayPhi(x));
    expectEquals(0, wrapAroundThenLinear(empty));
    expectEquals(55, wrapAroundThenLinear(x));
    expectEquals(0, wrapAroundThenLinearThreeWayPhi(empty));
    expectEquals(54, wrapAroundThenLinearThreeWayPhi(x));

    // Linear with parameter.
    sResult = 0;
    try {
      linearWithParameter(-1);
    } catch (NegativeArraySizeException e) {
      sResult = 1;
    }
    expectEquals(1, sResult);
    for (int n = 0; n < 32; n++) {
      int[] r = linearWithParameter(n);
      expectEquals(n, r.length);
      for (int i = 0; i < n; i++) {
        expectEquals(i, r[i]);
      }
    }

    // Linear copy.
    expectEquals(0, linearCopy(empty).length);
    {
      int[] r = linearCopy(x);
      expectEquals(x.length, r.length);
      for (int i = 0; i < x.length; i++) {
        expectEquals(x[i], r[i]);
      }
    }

    // Linear with non-unit strides.
    expectEquals(55, linearByTwo(x));
    expectEquals(25, linearByTwoSkip1(x));
    expectEquals(25, linearByTwoSkip2(x));
    expectEquals(56, linearWithCompoundStride());
    expectEquals(66, linearWithLargePositiveStride());
    expectEquals(66, linearWithVeryLargePositiveStride());
    expectEquals(66, linearWithLargeNegativeStride());
    expectEquals(66, linearWithVeryLargeNegativeStride());

    // Special forms.
    expectEquals(55, linearForNEUp());
    expectEquals(55, linearForNEDown());
    expectEquals(55, linearForNEArrayLengthUp(x));
    expectEquals(55, linearForNEArrayLengthDown(x));
    expectEquals(55, linearDoWhileUp());
    expectEquals(55, linearDoWhileDown());
    expectEquals(55, linearLong());
    expectEquals(55, linearLongAlt(x));
    expectEquals(55, linearShort());
    expectEquals(55, linearChar());
    expectEquals(55, linearByte());
    expectEquals(55, invariantFromPreLoop(x, 1));
    linearTriangularOnTwoArrayLengths(10);
    linearTriangularOnOneArrayLength(10);
    linearTriangularOnParameter(10);
    linearTriangularStrictLower(10);
    linearTriangularStrictUpper(10);
    {
      int[] t = linearTriangularOOB();
      for (int i = 0; i < 200; i++) {
        expectEquals(i <= 127 ? i + 1 : 128, t[i]);
      }
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
