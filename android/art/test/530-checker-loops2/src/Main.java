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
// Test on loop optimizations, in particular around less common induction.
//
public class Main {

  static int sResult;

  //
  // Various sequence variables used in bound checks.
  //

  /// CHECK-START: void Main.bubble(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.bubble(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void bubble(int[] a) {
    for (int i = a.length; --i >= 0;) {
      for (int j = 0; j < i; j++) {
        if (a[j] > a[j+1]) {
          int tmp = a[j];
          a[j]  = a[j+1];
          a[j+1] = tmp;
        }
      }
    }
  }

  /// CHECK-START: int Main.periodicIdiom(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.periodicIdiom(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int periodicIdiom(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];
      k = 1 - k;
    }
    return result;
  }

  /// CHECK-START: int Main.periodicSequence2(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.periodicSequence2(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int periodicSequence2(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int l = 1;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];
      int t = l;
      l = k;
      k = t;
    }
    return result;
  }

  /// CHECK-START: int Main.periodicSequence4(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.periodicSequence4(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int periodicSequence4(int tc) {
    int[] x = { 1, 3, 5, 7 };
    // Loop with periodic sequence (0, 1, 2, 3).
    int k = 0;
    int l = 1;
    int m = 2;
    int n = 3;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k] + x[l] + x[m] + x[n];  // all used at once
      int t = n;
      n = k;
      k = l;
      l = m;
      m = t;
    }
    return result;
  }

  /// CHECK-START: int Main.periodicXorSequence(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.periodicXorSequence(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int periodicXorSequence(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];
      k ^= 1;
    }
    return result;
  }

  /// CHECK-START: int Main.justRightUp1() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightUp1() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightUp1() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MAX_VALUE - 10, k = 0; i < Integer.MAX_VALUE; i++) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.justRightUp2() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightUp2() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightUp2() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MAX_VALUE - 10; i < Integer.MAX_VALUE; i++) {
      result += x[i - Integer.MAX_VALUE + 10];
    }
    return result;
  }

  /// CHECK-START: int Main.justRightUp3() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightUp3() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightUp3() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MAX_VALUE - 10, k = 0; i <= Integer.MAX_VALUE - 1; i++) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.justOOBUp() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justOOBUp() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justOOBUp() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int justOOBUp() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Infinite loop!
    for (int i = Integer.MAX_VALUE - 9, k = 0; i <= Integer.MAX_VALUE; i++) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.justRightDown1() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightDown1() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightDown1() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MIN_VALUE + 10, k = 0; i > Integer.MIN_VALUE; i--) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.justRightDown2() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightDown2() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightDown2() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MIN_VALUE + 10; i > Integer.MIN_VALUE; i--) {
      result += x[Integer.MAX_VALUE + i];
    }
    return result;
  }

  /// CHECK-START: int Main.justRightDown3() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justRightDown3() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int justRightDown3() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    for (int i = Integer.MIN_VALUE + 10, k = 0; i >= Integer.MIN_VALUE + 1; i--) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.justOOBDown() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justOOBDown() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.justOOBDown() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static int justOOBDown() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int result = 0;
    // Infinite loop!
    for (int i = Integer.MIN_VALUE + 9, k = 0; i >= Integer.MIN_VALUE; i--) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: void Main.lowerOOB(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.lowerOOB(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.lowerOOB(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void lowerOOB(int[] x) {
    // OOB!
    for (int i = -1; i < x.length; i++) {
      sResult += x[i];
    }
  }

  /// CHECK-START: void Main.upperOOB(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.upperOOB(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.upperOOB(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void upperOOB(int[] x) {
    // OOB!
    for (int i = 0; i <= x.length; i++) {
      sResult += x[i];
    }
  }

  /// CHECK-START: void Main.doWhileUpOOB() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doWhileUpOOB() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doWhileUpOOB() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void doWhileUpOOB() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int i = 0;
    // OOB!
    do {
      sResult += x[i++];
    } while (i <= x.length);
  }

  /// CHECK-START: void Main.doWhileDownOOB() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doWhileDownOOB() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doWhileDownOOB() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void doWhileDownOOB() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int i = x.length - 1;
    // OOB!
    do {
      sResult += x[i--];
    } while (-1 <= i);
  }

  /// CHECK-START: void Main.justRightTriangular1() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.justRightTriangular1() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void justRightTriangular1() {
    int[] a = { 1 } ;
    for (int i = Integer.MIN_VALUE + 5; i <= Integer.MIN_VALUE + 10; i++) {
      for (int j = Integer.MIN_VALUE + 4; j < i - 5; j++) {
        sResult += a[j - (Integer.MIN_VALUE + 4)];
      }
    }
  }

  /// CHECK-START: void Main.justRightTriangular2() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.justRightTriangular2() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static void justRightTriangular2() {
    int[] a = { 1 } ;
    for (int i = Integer.MIN_VALUE + 5; i <= 10; i++) {
      for (int j = 4; j < i - 5; j++) {
        sResult += a[j - 4];
      }
    }
  }

  /// CHECK-START: void Main.justOOBTriangular() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.justOOBTriangular() BCE (after)
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.justOOBTriangular() BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static void justOOBTriangular() {
    int[] a = { 1 } ;
    for (int i = Integer.MIN_VALUE + 4; i <= 10; i++) {
      for (int j = 4; j < i - 5; j++) {
        sResult += a[j - 4];
      }
    }
  }

  /// CHECK-START: void Main.hiddenOOB1(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenOOB1(int) BCE (after)
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.hiddenOOB1(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static void hiddenOOB1(int lo) {
    int[] a = { 1 } ;
    for (int i = lo; i <= 10; i++) {
      // Dangerous loop where careless static range analysis would yield strict upper bound
      // on index j of 5. When, for instance, lo and thus i = -2147483648, the upper bound
      // becomes really positive due to arithmetic wrap-around, causing OOB.
      for (int j = 4; j < i - 5; j++) {
        sResult += a[j - 4];
      }
    }
  }

  /// CHECK-START: void Main.hiddenOOB2(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenOOB2(int) BCE (after)
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.hiddenOOB2(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static void hiddenOOB2(int hi) {
    int[] a = { 1 } ;
    for (int i = 0; i < hi; i++) {
      // Dangerous loop where careless static range analysis would yield strict lower bound
      // on index j of 5. When, for instance, hi and thus i = 2147483647, the upper bound
      // becomes really negative due to arithmetic wrap-around, causing OOB.
      for (int j = 6; j > i + 5; j--) {
        sResult += a[j - 6];
      }
    }
  }

  /// CHECK-START: void Main.hiddenOOB3(int) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenOOB3(int) BCE (after)
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.hiddenOOB3(int) BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static void hiddenOOB3(int hi) {
    int[] a = { 11 } ;
    for (int i = -1; i <= hi; i++) {
      // Dangerous loop where careless static range analysis would yield strict lower bound
      // on index j of 0. For large i, the initial value of j becomes really negative due
      // to arithmetic wrap-around, causing OOB.
      for (int j = i + 1; j < 1; j++) {
        sResult += a[j];
      }
    }
  }

  /// CHECK-START: void Main.hiddenInfiniteOOB() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenInfiniteOOB() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenInfiniteOOB() BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void hiddenInfiniteOOB() {
    int[] a = { 11 } ;
    for (int i = -1; i <= 0; i++) {
      // Dangerous loop where careless static range analysis would yield a safe upper bound
      // of -3. In reality, due to arithmetic wrap-around (when i = -1, j <= 2147483647;
      // whereas when i = 0, j <= -3), this is an infinite loop that goes OOB.
      for (int j = -3; j <= 2147483646 * i - 3; j++) {
        sResult += a[j + 3];
      }
    }
  }

  /// CHECK-START: void Main.hiddenFiniteOOB() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.hiddenFiniteOOB() BCE (after)
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.hiddenFiniteOOB() BCE (after)
  /// CHECK-NOT: BoundsCheck
  private static void hiddenFiniteOOB() {
    int[] a = { 111 } ;
    for (int i = -1; i <= 0; i++) {
      // Dangerous loop similar as above where the loop is now finite, but the
      // loop still goes out of bounds for i = -1 due to the large upper bound.
      for (int j = -4; j < 2147483646 * i - 3; j++) {
        sResult += a[j + 4];
      }
    }
  }

  /// CHECK-START: void Main.inductionOOB(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.inductionOOB(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.inductionOOB(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void inductionOOB(int[] a) {
    // Careless range analysis would remove the bounds check.
    // However, the narrower induction b wraps around arithmetically
    // before it reaches the end of arrays longer than 127.
    byte b = 0;
    for (int i = 0; i < a.length; i++) {
      a[b++] = i;
    }
  }

  /// CHECK-START: void Main.controlOOB(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.controlOOB(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.controlOOB(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void controlOOB(int[] a) {
    // As above, but now the loop control also wraps around.
    for (byte i = 0; i < a.length; i++) {
      a[i] = -i;
    }
  }

  /// CHECK-START: void Main.conversionOOB(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.conversionOOB(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.conversionOOB(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  private static void conversionOOB(int[] a) {
    // As above, but with wrap around caused by an explicit conversion.
    for (int i = 0; i < a.length; ) {
      a[i] = i;
      i = (byte) (i + 1);
    }
  }

  /// CHECK-START: int Main.doNotHoist(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int Main.doNotHoist(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static int doNotHoist(int[] a) {
     int n = a.length;
     int x = 0;
     // BCE applies, but hoisting would crash the loop.
     for (int i = -10000; i < 10000; i++) {
       for (int j = 0; j <= 1; j++) {
         if (0 <= i && i < n)
           x += a[i];
       }
    }
    return x;
  }


  /// CHECK-START: int[] Main.add() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.add() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int[] add() {
    int[] a = new int[10];
    for (int i = 0; i <= 3; i++) {
      for (int j = 0; j <= 6; j++) {
        a[i + j] += 1;
      }
    }
    return a;
  }

  /// CHECK-START: int[] Main.multiply1() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.multiply1() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  private static int[] multiply1() {
    int[] a = new int[10];
    try {
      for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 3; j++) {
          // Range [0,9]: safe.
          a[i * j] += 1;
        }
      }
    } catch (Exception e) {
      a[0] += 1000;
    }
    return a;
  }

  /// CHECK-START: int[] Main.multiply2() BCE (before)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.multiply2() BCE (after)
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: int[] Main.multiply2() BCE (after)
  /// CHECK-NOT: Deoptimize
  static int[] multiply2() {
    int[] a = new int[10];
    try {
      for (int i = -3; i <= 3; i++) {
        for (int j = -3; j <= 3; j++) {
          // Range [-9,9]: unsafe.
          a[i * j] += 1;
        }
      }
    } catch (Exception e) {
      a[0] += 1000;
    }
    return a;
  }

  /// CHECK-START: int Main.linearDynamicBCE1(int[], int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.linearDynamicBCE1(int[], int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:{{B\d+}}
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.linearDynamicBCE1(int[], int, int) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  private static int linearDynamicBCE1(int[] x, int lo, int hi) {
    int result = 0;
    for (int i = lo; i < hi; i++) {
      sResult += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.linearDynamicBCE2(int[], int, int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.linearDynamicBCE2(int[], int, int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:{{B\d+}}
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.linearDynamicBCE2(int[], int, int, int) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  private static int linearDynamicBCE2(int[] x, int lo, int hi, int offset) {
    int result = 0;
    for (int i = lo; i < hi; i++) {
      sResult += x[offset + i];
    }
    return result;
  }

  /// CHECK-START: int Main.wrapAroundDynamicBCE(int[]) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.wrapAroundDynamicBCE(int[]) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:{{B\d+}}
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.wrapAroundDynamicBCE(int[]) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  private static int wrapAroundDynamicBCE(int[] x) {
    int w = 9;
    int result = 0;
    for (int i = 0; i < 10; i++) {
      result += x[w];
      w = i;
    }
    return result;
  }

  /// CHECK-START: int Main.periodicDynamicBCE(int[]) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.periodicDynamicBCE(int[]) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:{{B\d+}}
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.periodicDynamicBCE(int[]) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  private static int periodicDynamicBCE(int[] x) {
    int k = 0;
    int result = 0;
    for (int i = 0; i < 10; i++) {
      result += x[k];
      k = 1 - k;
    }
    return result;
  }

  /// CHECK-START: int Main.dynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:{{B\d+}}
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.dynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  static int dynamicBCEPossiblyInfiniteLoop(int[] x, int lo, int hi) {
    // This loop could be infinite for hi = max int. Since i is also used
    // as subscript, however, dynamic bce can proceed.
    int result = 0;
    for (int i = lo; i <= hi; i++) {
      result += x[i];
    }
    return result;
  }

  /// CHECK-START: int Main.noDynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.noDynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.noDynamicBCEPossiblyInfiniteLoop(int[], int, int) BCE (after)
  /// CHECK-NOT: Deoptimize
  static int noDynamicBCEPossiblyInfiniteLoop(int[] x, int lo, int hi) {
    // As above, but now the index is not used as subscript,
    // and dynamic bce is not applied.
    int result = 0;
    for (int k = 0, i = lo; i <= hi; i++) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.noDynamicBCEMixedInductionTypes(int[], long, long) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.noDynamicBCEMixedInductionTypes(int[], long, long) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.noDynamicBCEMixedInductionTypes(int[], long, long) BCE (after)
  /// CHECK-NOT: Deoptimize
  static int noDynamicBCEMixedInductionTypes(int[] x, long lo, long hi) {
    int result = 0;
    // Mix of int and long induction.
    int k = 0;
    for (long i = lo; i < hi; i++) {
      result += x[k++];
    }
    return result;
  }

  /// CHECK-START: int Main.dynamicBCEConstantRange(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<InnerLoop:B\d+>>
  /// CHECK-DAG: ArrayGet    loop:<<InnerLoop>>
  /// CHECK-DAG: If          loop:<<InnerLoop>>
  /// CHECK-DAG: If          loop:<<OuterLoop:B\d+>>
  /// CHECK-EVAL: "<<InnerLoop>>" != "<<OuterLoop>>"
  //
  /// CHECK-START: int Main.dynamicBCEConstantRange(int[]) BCE (after)
  /// CHECK-DAG: ArrayGet   loop:<<InnerLoop:B\d+>>
  /// CHECK-DAG: Deoptimize loop:<<OuterLoop:B\d+>>
  /// CHECK-EVAL: "<<InnerLoop>>" != "<<OuterLoop>>"
  //
  /// CHECK-START: int Main.dynamicBCEConstantRange(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  //
  //  No additional top tests were introduced.
  /// CHECK-START: int Main.dynamicBCEConstantRange(int[]) BCE (after)
  /// CHECK-DAG: If
  /// CHECK-DAG: If
  /// CHECK-NOT: If
  static int dynamicBCEConstantRange(int[] x) {
    int result = 0;
    for (int i = 2; i <= 6; i++) {
      // Range analysis sees that innermost loop is finite and always taken.
      for (int j = i - 2; j <= i + 2; j++) {
        result += x[j];
      }
    }
    return result;
  }

  /// CHECK-START: int Main.dynamicBCEAndConstantIndices(int[], int[][], int, int) BCE (before)
  /// CHECK-DAG: {{l\d+}} ArrayGet loop:<<Loop:B\d+>>
  /// CHECK-DAG: {{l\d+}} ArrayGet loop:<<Loop>>
  /// CHECK-DAG: {{l\d+}} ArrayGet loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndices(int[], int[][], int, int) BCE (after)
  //  Order matters:
  /// CHECK:              Deoptimize loop:<<Loop:B\d+>>
  /// CHECK-NOT:          Goto       loop:<<Loop>>
  /// CHECK-DAG: {{l\d+}} ArrayGet   loop:<<Loop>>
  /// CHECK-DAG: {{l\d+}} ArrayGet   loop:<<Loop>>
  /// CHECK-DAG: {{l\d+}} ArrayGet   loop:<<Loop>>
  /// CHECK:              Goto       loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndices(int[], int[][], int, int) BCE (after)
  /// CHECK-DAG: Deoptimize loop:none
  static int dynamicBCEAndConstantIndices(int[] x, int[][] a, int lo, int hi) {
    // Deliberately test array length on a before the loop so that only bounds checks
    // on constant subscripts remain, making them a viable candidate for hoisting.
    if (a.length == 0) {
      return -1;
    }
    // Loop that allows BCE on x[i].
    int result = 0;
    for (int i = lo; i < hi; i++) {
      result += x[i];
      if ((i % 10) != 0) {
        // None of the subscripts inside a conditional are removed by dynamic bce,
        // making them a candidate for deoptimization based on constant indices.
        // Compiler should ensure the array loads are not subsequently hoisted
        // "above" the deoptimization "barrier" on the bounds.
        a[1][i] = 1;
        a[2][i] = 2;
        a[99][i] = 3;
      }
    }
    return result;
  }

  /// CHECK-START: int Main.dynamicBCEAndConstantIndicesAllPrimTypes(int[], boolean[], byte[], char[], short[], int[], long[], float[], double[], int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  //  For brevity, just test occurrence of at least one of each in the loop:
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndicesAllPrimTypes(int[], boolean[], byte[], char[], short[], int[], long[], float[], double[], int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-NOT: ArrayGet    loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndicesAllPrimTypes(int[], boolean[], byte[], char[], short[], int[], long[], float[], double[], int, int) BCE (after)
  /// CHECK-NOT: NullCheck   loop:{{B\d+}}
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndicesAllPrimTypes(int[], boolean[], byte[], char[], short[], int[], long[], float[], double[], int, int) BCE (after)
  /// CHECK-DAG: Deoptimize  loop:none
  static int dynamicBCEAndConstantIndicesAllPrimTypes(int[] q,
                                                      boolean[] r,
                                                      byte[] s,
                                                      char[] t,
                                                      short[] u,
                                                      int[] v,
                                                      long[] w,
                                                      float[] x,
                                                      double[] y, int lo, int hi) {
    int result = 0;
    for (int i = lo; i < hi; i++) {
      // All constant index array references can be hoisted out of the loop during BCE on q[i].
      result += q[i] + (r[0] ? 1 : 0) + (int) s[0] + (int) t[0] + (int) u[0] + (int) v[0] +
                                        (int) w[0] + (int) x[0] + (int) y[0];
    }
    return result;
  }

  /// CHECK-START: int Main.dynamicBCEAndConstantIndexRefType(int[], java.lang.Integer[], int, int) BCE (before)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  /// CHECK-DAG: ArrayGet    loop:<<Loop>>
  /// CHECK-DAG: NullCheck   loop:<<Loop>>
  /// CHECK-DAG: ArrayLength loop:<<Loop>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndexRefType(int[], java.lang.Integer[], int, int) BCE (after)
  /// CHECK-DAG: ArrayGet    loop:<<Loop:B\d+>>
  /// CHECK-DAG: Deoptimize  loop:none
  //
  /// CHECK-START: int Main.dynamicBCEAndConstantIndexRefType(int[], java.lang.Integer[], int, int) BCE (after)
  /// CHECK-NOT: ArrayLength loop:{{B\d+}}
  /// CHECK-NOT: BoundsCheck loop:{{B\d+}}
  static int dynamicBCEAndConstantIndexRefType(int[] q, Integer[] z, int lo, int hi) {
    int result = 0;
    for (int i = lo; i < hi; i++) {
      // Similar to above, but now implicit call to intValue() may prevent hoisting
      // z[0] itself during BCE on q[i]. Therefore, we just check BCE on q[i].
      result += q[i] + z[0];
    }
    return result;
  }

  /// CHECK-START: int Main.shortIndex(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.shortIndex(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>>
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>
  //
  /// CHECK-START: int Main.shortIndex(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  static int shortIndex(int[] a) {
    int r = 0;
    // Make sure short/int conversions compiles well (b/32193474).
    for (short i = 1; i < 10; i++) {
      int ki = i - 1;
      r += a[ki] + a[i];
    }
    return r;
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    // Set to run expensive tests for correctness too.
    boolean HEAVY = false;

    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    int[] a200 = new int[200];

    // Sorting.
    int[] sort = { 5, 4, 1, 9, 10, 2, 7, 6, 3, 8 };
    bubble(sort);
    for (int i = 0; i < 10; i++) {
      expectEquals(sort[i], x[i]);
    }

    // Periodic adds (1, 3), one at the time.
    expectEquals(0, periodicIdiom(-1));
    for (int tc = 0; tc < 32; tc++) {
      int expected = (tc >> 1) << 2;
      if ((tc & 1) != 0) {
        expected += 1;
      }
      expectEquals(expected, periodicIdiom(tc));
    }

    // Periodic adds (1, 3), one at the time.
    expectEquals(0, periodicSequence2(-1));
    for (int tc = 0; tc < 32; tc++) {
      int expected = (tc >> 1) << 2;
      if ((tc & 1) != 0) {
        expected += 1;
      }
      expectEquals(expected, periodicSequence2(tc));
    }

    // Periodic adds (1, 3, 5, 7), all at once.
    expectEquals(0, periodicSequence4(-1));
    for (int tc = 0; tc < 32; tc++) {
      expectEquals(tc * 16, periodicSequence4(tc));
    }

    // Periodic adds (1, 3), one at the time.
    expectEquals(0, periodicXorSequence(-1));
    for (int tc = 0; tc < 32; tc++) {
      int expected = (tc >> 1) << 2;
      if ((tc & 1) != 0) {
        expected += 1;
      }
      expectEquals(expected, periodicXorSequence(tc));
    }

    // Large bounds.
    expectEquals(55, justRightUp1());
    expectEquals(55, justRightUp2());
    expectEquals(55, justRightUp3());
    expectEquals(55, justRightDown1());
    expectEquals(55, justRightDown2());
    expectEquals(55, justRightDown3());

    // Large bounds OOB.
    sResult = 0;
    try {
      justOOBUp();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult = 1;
    }
    expectEquals(1, sResult);
    sResult = 0;
    try {
      justOOBDown();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult = 1;
    }
    expectEquals(1, sResult);

    // Lower bound goes OOB.
    sResult = 0;
    try {
      lowerOOB(x);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);

    // Upper bound goes OOB.
    sResult = 0;
    try {
      upperOOB(x);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1055, sResult);

    // Do while up goes OOB.
    sResult = 0;
    try {
      doWhileUpOOB();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1055, sResult);

    // Do while down goes OOB.
    sResult = 0;
    try {
      doWhileDownOOB();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1055, sResult);

    // Triangular.
    sResult = 0;
    justRightTriangular1();
    expectEquals(1, sResult);
    if (HEAVY) {
      sResult = 0;
      justRightTriangular2();
      expectEquals(1, sResult);
    }
    sResult = 0;
    try {
      justOOBTriangular();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1001, sResult);

    // Hidden OOB.
    sResult = 0;
    try {
      hiddenOOB1(10);  // no OOB
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1, sResult);
    sResult = 0;
    try {
      hiddenOOB1(-2147483648);  // OOB
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1001, sResult);
    sResult = 0;
    try {
      hiddenOOB2(1);  // no OOB
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1, sResult);
    sResult = 0;
    try {
      hiddenOOB3(-1);  // no OOB
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(11, sResult);

    // Expensive hidden OOB test.
    if (HEAVY) {
      sResult = 0;
      try {
        hiddenOOB2(2147483647);  // OOB
      } catch (ArrayIndexOutOfBoundsException e) {
        sResult += 1000;
      }
      expectEquals(1002, sResult);
      sResult = 0;
      try {
        hiddenOOB3(2147483647);  // OOB
      } catch (ArrayIndexOutOfBoundsException e) {
        sResult += 1000;
      }
      expectEquals(1011, sResult);
    }

    // More hidden OOB.
    sResult = 0;
    try {
      hiddenInfiniteOOB();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1011, sResult);
    sResult = 0;
    try {
      hiddenFiniteOOB();
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1111, sResult);
    sResult = 0;
    try {
      inductionOOB(a200);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);
    for (int i = 0; i < 200; i++) {
      expectEquals(i < 128 ? i : 0, a200[i]);
    }
    sResult = 0;
    try {
      controlOOB(a200);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);
    for (int i = 0; i < 200; i++) {
      expectEquals(i < 128 ? -i : 0, a200[i]);
    }
    sResult = 0;
    try {
      conversionOOB(a200);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);
    for (int i = 0; i < 200; i++) {
      expectEquals(i < 128 ? i : 0, a200[i]);
    }

    // No hoisting after BCE.
    expectEquals(110, doNotHoist(x));

    // Addition.
    {
      int[] e1 ={ 1, 2, 3, 4, 4, 4, 4, 3, 2, 1 };
      int[] a1 = add();
      for (int i = 0; i < 10; i++) {
        expectEquals(a1[i], e1[i]);
      }
    }

    // Multiplication.
    {
      int[] e1 = { 7, 1, 2, 2, 1, 0, 2, 0, 0, 1 };
      int[] a1 = multiply1();
      for (int i = 0; i < 10; i++) {
        expectEquals(a1[i], e1[i]);
      }
      int[] e2 = { 1001, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
      int[] a2 = multiply2();
      for (int i = 0; i < 10; i++) {
        expectEquals(a2[i], e2[i]);
      }
    }

    // Dynamic BCE.
    sResult = 0;
    try {
      linearDynamicBCE1(x, -1, x.length);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);
    sResult = 0;
    linearDynamicBCE1(x, 0, x.length);
    expectEquals(55, sResult);
    sResult = 0;
    try {
      linearDynamicBCE1(x, 0, x.length + 1);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1055, sResult);

    // Dynamic BCE with offset.
    sResult = 0;
    try {
      linearDynamicBCE2(x, 0, x.length, -1);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1000, sResult);
    sResult = 0;
    linearDynamicBCE2(x, 0, x.length, 0);
    expectEquals(55, sResult);
    sResult = 0;
    try {
      linearDynamicBCE2(x, 0, x.length, 1);
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult += 1000;
    }
    expectEquals(1054, sResult);

    // Dynamic BCE candidates.
    expectEquals(55, wrapAroundDynamicBCE(x));
    expectEquals(15, periodicDynamicBCE(x));
    expectEquals(55, dynamicBCEPossiblyInfiniteLoop(x, 0, 9));
    expectEquals(55, noDynamicBCEPossiblyInfiniteLoop(x, 0, 9));
    expectEquals(55, noDynamicBCEMixedInductionTypes(x, 0, 10));
    expectEquals(125, dynamicBCEConstantRange(x));

    // Dynamic BCE combined with constant indices.
    int[][] a;
    a = new int[0][0];
    expectEquals(-1, dynamicBCEAndConstantIndices(x, a, 0, 10));
    a = new int[100][10];
    expectEquals(55, dynamicBCEAndConstantIndices(x, a, 0, 10));
    for (int i = 0; i < 10; i++) {
      expectEquals((i % 10) != 0 ? 1 : 0, a[1][i]);
      expectEquals((i % 10) != 0 ? 2 : 0, a[2][i]);
      expectEquals((i % 10) != 0 ? 3 : 0, a[99][i]);
    }
    a = new int[3][10];
    sResult = 0;
    try {
      expectEquals(55, dynamicBCEAndConstantIndices(x, a, 0, 10));
    } catch (ArrayIndexOutOfBoundsException e) {
      sResult = 1;
    }
    expectEquals(1, sResult);
    expectEquals(a[1][1], 1);
    expectEquals(a[2][1], 2);

    // Dynamic BCE combined with constant indices of all types.
    boolean[] x1 = { true };
    byte[] x2 = { 2 };
    char[] x3 = { 3 };
    short[] x4 = { 4 };
    int[] x5 = { 5 };
    long[] x6 = { 6 };
    float[] x7 = { 7 };
    double[] x8 = { 8 };
    expectEquals(415,
        dynamicBCEAndConstantIndicesAllPrimTypes(x, x1, x2, x3, x4, x5, x6, x7, x8, 0, 10));
    Integer[] x9 = { 9 };
    expectEquals(145, dynamicBCEAndConstantIndexRefType(x, x9, 0, 10));

    expectEquals(99, shortIndex(x));

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
