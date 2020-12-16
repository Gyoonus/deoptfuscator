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

  /// CHECK-START: int Main.sad1(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:MathAbsInt
  //
  // NOTE: for direct 32-bit operands, this is not an ABS.
  static int sad1(int x, int y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: int Main.sad2(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad2(int x, int y) {
    int diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: int Main.sad3(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3(int x, int y) {
    int diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: int Main.sad3Alt(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3Alt(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3Alt(int x, int y) {
    int diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL1(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL1(int x, int y) {
    long xl = x;
    long yl = y;
    return xl >= yl ? xl - yl : yl - xl;
  }

  /// CHECK-START: long Main.sadL2(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL2(int x, int y) {
    long diff = x - y;
    if (diff < 0L) diff = -diff;
    return diff;
  }

  /// CHECK-START: long Main.sadL3(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3(int x, int y) {
    long diff = x - y;
    return diff >= 0L ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL3Alt(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3Alt(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3Alt(int x, int y) {
    long diff = x - y;
    return 0L <= diff ? diff : -diff;
  }

  public static void main(String[] args) {
    // Use cross-values for the interesting values.
    int[] interesting = {
      0x00000000, 0x00000001, 0x00007fff, 0x00008000, 0x00008001, 0x0000ffff,
      0x00010000, 0x00010001, 0x00017fff, 0x00018000, 0x00018001, 0x0001ffff,
      0x7fff0000, 0x7fff0001, 0x7fff7fff, 0x7fff8000, 0x7fff8001, 0x7fffffff,
      0x80000000, 0x80000001, 0x80007fff, 0x80008000, 0x80008001, 0x8000ffff,
      0x80010000, 0x80010001, 0x80017fff, 0x80018000, 0x80018001, 0x8001ffff,
      0xffff0000, 0xffff0001, 0xffff7fff, 0xffff8000, 0xffff8001, 0xffffffff
    };
    for (int i = 0; i < interesting.length; i++) {
      for (int j = 0; j < interesting.length; j++) {
        int x = interesting[i];
        int y = interesting[j];
        int e1 = x >= y ? x - y : y - x;  // still select
        expectEquals(e1, sad1(x, y));
        int e2 = Math.abs(x - y);  // pure abs
        expectEquals(e2, sad2(x, y));
        expectEquals(e2, sad3(x, y));
        expectEquals(e2, sad3Alt(x, y));
        long eL1 = Math.abs(((long)x) - ((long)y));  // now, different, but abs
        expectEquals(eL1, sadL1(x, y));
        long eL2 = Math.abs((long)(x - y));  // also, different, but abs
        expectEquals(eL2, sadL2(x, y));
        expectEquals(eL2, sadL3(x, y));
        expectEquals(eL2, sadL3Alt(x, y));
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
