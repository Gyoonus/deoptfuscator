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

import java.util.Arrays;

//
// Test on array parameters with or without potential aliasing.
//
public class Main {

  //
  // Cross-over on parameters with potential aliasing on parameters.
  // The arrays a and b may point to the same memory, which (without
  // further runtime tests) prevents hoisting the seemingly invariant
  // array reference.
  //

  /// CHECK-START: void Main.CrossOverLoop1(int[], int[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.CrossOverLoop1(int[], int[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void CrossOverLoop1(int a[], int b[]) {
    b[20] = 99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  /// CHECK-START: void Main.CrossOverLoop2(float[], float[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.CrossOverLoop2(float[], float[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void CrossOverLoop2(float a[], float b[]) {
    b[20] = 99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  /// CHECK-START: void Main.CrossOverLoop3(long[], long[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.CrossOverLoop3(long[], long[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void CrossOverLoop3(long a[], long b[]) {
    b[20] = 99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  /// CHECK-START: void Main.CrossOverLoop4(double[], double[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.CrossOverLoop4(double[], double[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void CrossOverLoop4(double a[], double b[]) {
    b[20] = 99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  //
  // False cross-over on parameters. Parameters have same width (which used to
  // cause a false type aliasing in an older version of the compiler), but since
  // the types are different cannot be aliased. Thus, the invariant array
  // reference can be hoisted.
  //

  /// CHECK-START: void Main.FalseCrossOverLoop1(int[], float[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.FalseCrossOverLoop1(int[], float[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void FalseCrossOverLoop1(int a[], float b[]) {
    b[20] = -99;
    for (int i = 0; i < a.length; i++) {
      a[i] = (int) b[20] - 7;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop2(float[], int[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.FalseCrossOverLoop2(float[], int[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void FalseCrossOverLoop2(float a[], int b[]) {
    b[20] = -99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop3(long[], double[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.FalseCrossOverLoop3(long[], double[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void FalseCrossOverLoop3(long a[], double b[]) {
    b[20] = -99;
    for (int i = 0; i < a.length; i++) {
      a[i] = (long) b[20] - 7;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop4(double[], long[]) licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  //
  /// CHECK-START: void Main.FalseCrossOverLoop4(double[], long[]) licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}
  private static void FalseCrossOverLoop4(double a[], long b[]) {
    b[20] = -99;
    for (int i = 0; i < a.length; i++) {
      a[i] = b[20] - 7;
    }
  }

  //
  // Main driver and testers.
  //

  public static void main(String[] args) {
    int[] aI = new int[100];
    float[] aF = new float[100];
    long[] aJ = new long[100];
    double[] aD = new double[100];

    // Type I.
    CrossOverLoop1(aI, aI);
    for (int i = 0; i < aI.length; i++) {
      expectEquals(i <= 20 ? 92 : 85, aI[i]);
    }
    // Type F.
    CrossOverLoop2(aF, aF);
    for (int i = 0; i < aF.length; i++) {
      expectEquals(i <= 20 ? 92 : 85, aF[i]);
    }
    // Type J.
    CrossOverLoop3(aJ, aJ);
    for (int i = 0; i < aJ.length; i++) {
      expectEquals(i <= 20 ? 92 : 85, aJ[i]);
    }
    // Type D.
    CrossOverLoop4(aD, aD);
    for (int i = 0; i < aD.length; i++) {
      expectEquals(i <= 20 ? 92 : 85, aD[i]);
    }

    // Type I vs F.
    FalseCrossOverLoop1(aI, aF);
    for (int i = 0; i < aI.length; i++) {
      expectEquals(-106, aI[i]);
    }
    // Type F vs I.
    FalseCrossOverLoop2(aF, aI);
    for (int i = 0; i < aF.length; i++) {
      expectEquals(-106, aF[i]);
    }
    // Type J vs D.
    FalseCrossOverLoop3(aJ, aD);
    for (int i = 0; i < aJ.length; i++) {
      expectEquals(-106, aJ[i]);
    }
    // Type D vs J.
    FalseCrossOverLoop4(aD, aJ);
    for (int i = 0; i < aD.length; i++) {
      expectEquals(-106, aD[i]);
    }

    // Real-world example where incorrect type assignment could introduce a bug.
    // The library sorting algorithm is heavy on array reads and writes, and
    // assigning the wrong J/D type to one of these would introduce errors.
    for (int i = 0; i < aD.length; i++) {
      aD[i] = aD.length - i - 1;
    }
    Arrays.sort(aD);
    for (int i = 0; i < aD.length; i++) {
      expectEquals((double) i, aD[i]);
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
