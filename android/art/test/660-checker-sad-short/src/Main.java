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

  /// CHECK-START: int Main.sad1(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad1(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad1(short x, short y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: int Main.sad2(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad2(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad2(short x, short y) {
    int diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: int Main.sad3(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3(short x, short y) {
    int diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: int Main.sad3Alt(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3Alt(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3Alt(short x, short y) {
    int diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL1(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL1(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL1(short x, short y) {
    long xl = x;
    long yl = y;
    return xl >= yl ? xl - yl : yl - xl;
  }

  /// CHECK-START: long Main.sadL2(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL2(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL2(short x, short y) {
    long diff = x - y;
    if (diff < 0L) diff = -diff;
    return diff;
  }

  /// CHECK-START: long Main.sadL3(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3(short x, short y) {
    long diff = x - y;
    return diff >= 0L ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL3Alt(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3Alt(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3Alt(short x, short y) {
    long diff = x - y;
    return 0L <= diff ? diff : -diff;
  }

  public static void main(String[] args) {
    // Use cross-values to test all cases.
    short[] interesting = {
      (short) 0x0000, (short) 0x0001, (short) 0x007f,
      (short) 0x0080, (short) 0x0081, (short) 0x00ff,
      (short) 0x0100, (short) 0x0101, (short) 0x017f,
      (short) 0x0180, (short) 0x0181, (short) 0x01ff,
      (short) 0x7f00, (short) 0x7f01, (short) 0x7f7f,
      (short) 0x7f80, (short) 0x7f81, (short) 0x7fff,
      (short) 0x8000, (short) 0x8001, (short) 0x807f,
      (short) 0x8080, (short) 0x8081, (short) 0x80ff,
      (short) 0x8100, (short) 0x8101, (short) 0x817f,
      (short) 0x8180, (short) 0x8181, (short) 0x81ff,
      (short) 0xff00, (short) 0xff01, (short) 0xff7f,
      (short) 0xff80, (short) 0xff81, (short) 0xffff
    };
    for (int i = 0; i < interesting.length; i++) {
      for (int j = 0; j < interesting.length; j++) {
        short x = interesting[i];
        short y = interesting[j];
        int e = Math.abs(x - y);
        expectEquals(e, sad1(x, y));
        expectEquals(e, sad2(x, y));
        expectEquals(e, sad3(x, y));
        expectEquals(e, sad3Alt(x, y));
        expectEquals(e, sadL1(x, y));
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
