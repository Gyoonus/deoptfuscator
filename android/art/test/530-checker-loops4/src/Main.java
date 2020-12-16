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
// Test on loop optimizations, in particular around geometric induction.
//
public class Main {

  /// CHECK-START: int Main.geo1(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Mul loop:<<Loop>>
  //
  /// CHECK-START: int Main.geo1(int) loop_optimization (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue         loop:none
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0          loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 1410065408 loop:none
  /// CHECK-DAG: <<Mul:i\d+>> Mul [<<Par>>,<<Int>>]  loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Mul>>,<<Zer>>]  loop:none
  /// CHECK-DAG:              Return [<<Add>>]       loop:none
  //
  /// CHECK-START: int Main.geo1(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geo1(int a) {
    for (int i = 0; i < 10; i++) {
      a *= 10;
    }
    return a;
  }

  /// CHECK-START: int Main.geo2(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Shl loop:<<Loop>>
  //
  /// CHECK-START: int Main.geo2(int) loop_optimization (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue        loop:none
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0         loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 1024      loop:none
  /// CHECK-DAG: <<Mul:i\d+>> Mul [<<Par>>,<<Int>>] loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Mul>>,<<Zer>>] loop:none
  /// CHECK-DAG:              Return [<<Add>>]      loop:none
  //
  /// CHECK-START: int Main.geo2(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geo2(int a) {
    for (int i = 0; i < 10; i++) {
      a <<= 1;
    }
    return a;
  }

  /// CHECK-START: int Main.geo3(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Div loop:<<Loop>>
  //
  /// CHECK-START: int Main.geo3(int) loop_optimization (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue        loop:none
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0         loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 59049     loop:none
  /// CHECK-DAG: <<Div:i\d+>> Div [<<Par>>,<<Int>>] loop:none
  /// CHECK-DAG: <<Add:i\d+>> Add [<<Div>>,<<Zer>>] loop:none
  /// CHECK-DAG:              Return [<<Add>>]      loop:none
  //
  /// CHECK-START: int Main.geo3(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geo3(int a) {
    for (int i = 0; i < 10; i++) {
      a /= 3;
    }
    return a;
  }

  /// CHECK-START: int Main.geo4(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Rem loop:<<Loop>>
  //
  /// CHECK-START: int Main.geo4(int) loop_optimization (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue        loop:none
  /// CHECK-DAG: <<Int:i\d+>> IntConstant 7         loop:none
  /// CHECK-DAG: <<Rem:i\d+>> Rem [<<Par>>,<<Int>>] loop:none
  /// CHECK-DAG:              Return [<<Rem>>]      loop:none
  //
  /// CHECK-START: int Main.geo4(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geo4(int a) {
    for (int i = 0; i < 10; i++) {
      a %= 7;  // a wrap-around induction
    }
    return a;
  }

  /// CHECK-START: int Main.geo5() loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Shr loop:<<Loop>>
  //
  /// CHECK-START: int Main.geo5() loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0          loop:none
  /// CHECK-DAG: <<Int1:i\d+>> IntConstant 2147483647 loop:none
  /// CHECK-DAG: <<Int2:i\d+>> IntConstant 1024       loop:none
  /// CHECK-DAG: <<Div:i\d+>>  Div [<<Int1>>,<<Int2>>] loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Div>>,<<Zero>>]  loop:none
  /// CHECK-DAG:               Return [<<Add>>]        loop:none
  //
  /// CHECK-START: int Main.geo5() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geo5() {
    int a = 0x7fffffff;
    for (int i = 0; i < 10; i++) {
      a >>= 1;
    }
    return a;
  }

  // TODO: someday?
  //
  /// CHECK-START: int Main.geo1BCE() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo1BCE() BCE (after)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo1BCE() BCE (after)
  /// CHECK-NOT: BoundsCheck loop:none
  /// CHECK-NOT: Deoptimize
  public static int geo1BCE() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26 };
    int a = 1;
    int r = 0;
    for (int i = 0; i < 3; i++) {
      r += x[a];
      a *= 5;
    }
    return r;
  }

  // TODO: someday?
  //
  /// CHECK-START: int Main.geo2BCE() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo2BCE() BCE (after)
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo2BCE() BCE (after)
  /// CHECK-NOT: BoundsCheck loop:none
  /// CHECK-NOT: Deoptimize
  public static int geo2BCE() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26 };
    int a = 1;
    int r = 0;
    for (int i = 0; i < 5; i++) {
      r += x[a];
      a <<= 1;
    }
    return r;
  }

  /// CHECK-START: int Main.geo3BCE() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo3BCE() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  public static int geo3BCE() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26 };
    int a = 25;
    int r = 0;
    for (int i = 0; i < 100; i++) {  // a converges to 0
      r += x[a];
      a /= 5;
    }
    return r;
  }

  /// CHECK-START: int Main.geo4BCE() BCE (before)
  /// CHECK-DAG: BoundsCheck loop:none
  /// CHECK-DAG: BoundsCheck loop:{{B\d+}}
  //
  /// CHECK-START: int Main.geo4BCE() BCE (after)
  /// CHECK-NOT: BoundsCheck
  /// CHECK-NOT: Deoptimize
  public static int geo4BCE() {
    int[] x = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
                11, 12, 13, 14, 15, 16, 17, 19, 19, 20,
                21, 22, 23, 24, 25, 26 };
    int a = 25;
    int r = 0;
    for (int i = 0; i < 100; i++) {  // a converges to 0
      r += x[a];
      a %= 5;  // a wrap-around induction
    }
    return r;
  }

  /// CHECK-START: int Main.geoMulBlackHole(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Mul loop:<<Loop>>
  //
  /// CHECK-START: int Main.geoMulBlackHole(int) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:               Return [<<Zero>>]
  //
  /// CHECK-START: int Main.geoMulBlackHole(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  /// CHECK-NOT: Mul
  public static int geoMulBlackHole(int a) {
    for (int i = 0; i < 100; i++) {
      a *= 10;
    }
    return a;
  }

  /// CHECK-START: int Main.geoShlBlackHole(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Shl loop:<<Loop>>
  //
  /// CHECK-START: int Main.geoShlBlackHole(int) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:               Return [<<Zero>>]
  //
  /// CHECK-START: int Main.geoShlBlackHole(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  /// CHECK-NOT: Shl
  public static int geoShlBlackHole(int a) {
    for (int i = 0; i < 100; i++) {
      a <<= 1;
    }
    return a;
  }

  /// CHECK-START: int Main.geoDivBlackHole(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Div loop:<<Loop>>
  //
  /// CHECK-START: int Main.geoDivBlackHole(int) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:               Return [<<Zero>>]
  //
  /// CHECK-START: int Main.geoDivBlackHole(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  /// CHECK-NOT: Div
  public static int geoDivBlackHole(int a) {
    for (int i = 0; i < 100; i++) {
      a /= 10;
    }
    return a;
  }

  // Even though Rem is already optimized away by the time induction analysis
  // and the loop optimizer run, the loop is optimized with a trivial
  // wrap-around induction just as the wrap-around for REM would.
  //
  /// CHECK-START: int Main.geoRemBlackHole(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>>
  /// CHECK-DAG: Phi loop:<<Loop>>
  //
  /// CHECK-START: int Main.geoRemBlackHole(int) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:               Return [<<Zero>>]
  //
  /// CHECK-START: int Main.geoRemBlackHole(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int geoRemBlackHole(int a) {
    for (int i = 0; i < 100; i++) {
      a %= 1;
    }
    return a;
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    int m = 1410065408;
    for (int i = -100; i <= 100; i++) {
      expectEquals(m * i, geo1(i));
    }
    for (int i = 1; i <= 1000000000; i *= 10) {
      expectEquals( m * i, geo1( i));
      expectEquals(-m * i, geo1(-i));
    }

    for (int i = -100; i <= 100; i++) {
      expectEquals(i << 10, geo2(i));
    }
    for (int i = 0; i < 22; i++) {
      expectEquals(1 << (i + 10), geo2(1 << i));
    }
    expectEquals(0x80000400, geo2(0x00200001));
    expectEquals(0x00000000, geo2(0x00400000));
    expectEquals(0x00000400, geo2(0x00400001));

    int d = 59049;
    for (int i = -100; i <= 100; i++) {
      expectEquals(0, geo3(i));
    }
    for (int i = 1; i <= 100; i++) {
      expectEquals( i, geo3( i * d));
      expectEquals( i, geo3( i * d + 1));
      expectEquals(-i, geo3(-i * d));
      expectEquals(-i, geo3(-i * d - 1));
    }

    for (int i = -100; i <= 100; i++) {
      expectEquals(i % 7, geo4(i));
    }

    expectEquals(0x1fffff, geo5());

    expectEquals(34,  geo1BCE());
    expectEquals(36,  geo2BCE());
    expectEquals(131, geo3BCE());
    expectEquals(125, geo4BCE());

    // Nothing escapes!
    expectEquals(0, geoMulBlackHole(0));
    expectEquals(0, geoShlBlackHole(0));
    expectEquals(0, geoDivBlackHole(0));
    expectEquals(0, geoRemBlackHole(0));
    for (int i = -100; i <= 100; i++) {
      expectEquals(0, geoMulBlackHole(i));
      expectEquals(0, geoShlBlackHole(i));
      expectEquals(0, geoDivBlackHole(i));
      expectEquals(0, geoRemBlackHole(i));
    }
    for (int i = 0; i < 31; i++) {
      expectEquals(0, geoMulBlackHole(1 << i));
      expectEquals(0, geoShlBlackHole(1 << i));
      expectEquals(0, geoDivBlackHole(1 << i));
      expectEquals(0, geoRemBlackHole(1 << i));
    }
    expectEquals(0, geoMulBlackHole(0x7fffffff));
    expectEquals(0, geoShlBlackHole(0x7fffffff));
    expectEquals(0, geoDivBlackHole(0x7fffffff));
    expectEquals(0, geoRemBlackHole(0x7fffffff));
    expectEquals(0, geoMulBlackHole(0x80000000));
    expectEquals(0, geoShlBlackHole(0x80000000));
    expectEquals(0, geoDivBlackHole(0x80000000));
    expectEquals(0, geoRemBlackHole(0x80000000));

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
