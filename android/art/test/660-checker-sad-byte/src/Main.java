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
 * Tests for SAD (sum of absolute differences).
 */
public class Main {

  /// CHECK-START: int Main.sad1(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad1(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad1(byte x, byte y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: int Main.sad2(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad2(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad2(byte x, byte y) {
    int diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: int Main.sad3(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3(byte x, byte y) {
    int diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: int Main.sad3Alt(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3Alt(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3Alt(byte x, byte y) {
    int diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL1(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL1(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL1(byte x, byte y) {
    long xl = x;
    long yl = y;
    return xl >= yl ? xl - yl : yl - xl;
  }

  /// CHECK-START: long Main.sadL2(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL2(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL2(byte x, byte y) {
    long diff = x - y;
    if (diff < 0L) diff = -diff;
    return diff;
  }

  /// CHECK-START: long Main.sadL3(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3(byte x, byte y) {
    long diff = x - y;
    return diff >= 0L ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL3Alt(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3Alt(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3Alt(byte x, byte y) {
    long diff = x - y;
    return 0L <= diff ? diff : -diff;
  }

  public static void main(String[] args) {
    // Use cross-values to test all cases.
    int n = 256;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        byte x = (byte) i;
        byte y = (byte) j;
        int e = Math.abs(x - y);
        expectEquals(e, sad1(x, y));
        expectEquals(e, sad2(x, y));
        expectEquals(e, sad3(x, y));
        expectEquals(e, sad3Alt(x, y));
        expectEquals(e, sadL2(x, y));
        expectEquals(e, sadL3(x, y));
        expectEquals(e, sadL3Alt(x, y));
      }
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
}
