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
// Test on loop optimizations, in particular around polynomial induction.
//
public class Main {

  /// CHECK-START: int Main.poly1() loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Add loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  //
  /// CHECK-START: int Main.poly1() loop_optimization (after)
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0         loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 55        loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Int>>,<<Zer>>] loop:none
  /// CHECK-DAG:              Return [<<Add>>]      loop:none
  //
  /// CHECK-START: int Main.poly1() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 55 loop:none
  /// CHECK-DAG:               Return [<<Int>>]  loop:none
  //
  /// CHECK-START: int Main.poly1() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int poly1() {
    int a = 0;
    for (int i = 0; i <= 10; i++) {
      a += i;
    }
    return a;
  }

  // Multiplication in linear induction has been optimized earlier,
  // but that does not stop the induction variable recognition
  // and loop optimizer.
  //
  /// CHECK-START: int Main.poly2(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Shl loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  //
  /// CHECK-START: int Main.poly2(int) loop_optimization (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue        loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 185       loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Int>>,<<Par>>] loop:none
  /// CHECK-DAG:              Return [<<Add>>]      loop:none
  //
  /// CHECK-START: int Main.poly2(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int poly2(int a) {
    for (int i = 0; i < 10; i++) {
      int k = 3 * i + 5;
      a += k;
    }
    return a;
  }

  /// CHECK-START: int Main.poly3() loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Add loop:<<Loop>>
  /// CHECK-DAG: Add loop:<<Loop>>
  //
  /// CHECK-START: int Main.poly3() loop_optimization (after)
  /// CHECK-DAG: <<Ini:i\d+>> IntConstant 12345       loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant -2146736968 loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Int>>,<<Ini>>]   loop:none
  /// CHECK-DAG:              Return [<<Add>>]        loop:none
  //
  /// CHECK-START: int Main.poly3() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant -2146724623 loop:none
  /// CHECK-DAG:               Return [<<Int>>]        loop:none
  //
  /// CHECK-START: int Main.poly3() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int poly3() {
    int a = 12345;
    for (int i = 0; i <= 10; i++) {
      a += (2147483646 * i + 67890);
    }
    return a;
  }

  /// CHECK-START: int Main.polyBCE1() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.polyBCE1() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  public static int polyBCE1() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22 };
    int a = 0;
    int r = 0;
    for (int i = 0; i < 8; i++) {
      r += x[a];
      a += i;
    }
    return r;
  }

  /// CHECK-START: int Main.polyBCE2() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.polyBCE2() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  public static int polyBCE2() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26, 27 };
    int a = 1;
    int r = 0;
    for (int i = 0; i < 6; i++) {
      int k = 2 * i + 1;
      r -= x[a];
      a += k;
    }
    return r;
  }

  /// CHECK-START: int Main.polyBCE3() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.polyBCE3() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  public static int polyBCE3() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                31, 32, 33, 34, 35, 36, 37, 38 };
    int r = 0;
    int a1 = 1;
    int a2 = 2;
    for (int i = 0; i < 5; i++) {
      int t = a1 + a2;  // two polynomials combined into new polynomial
      r -= x[t];
      a1 += (3 * i + 1);
      a2 += (2 * i);
    }
    return r;
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    expectEquals(55, poly1());
    expectEquals(185, poly2(0));
    expectEquals(192, poly2(7));
    expectEquals(-2146724623, poly3());
    expectEquals(64, polyBCE1());
    expectEquals(-68, polyBCE2());
    expectEquals(-80, polyBCE3());

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
