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

  /// CHECK-START: long Main.sad1(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sad1(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sad1(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT: InvokeStaticOrDirect intrinsic:MathAbsLong
  //
  // NOTE: for direct 64-bit operands, this is not an ABS.
  static long sad1(long x, long y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: long Main.sad2(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sad2(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sad2(long x, long y) {
    long diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: long Main.sad3(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sad3(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sad3(long x, long y) {
    long diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: long Main.sad3Alt(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sad3Alt(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sad3Alt(long x, long y) {
    long diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  public static void main(String[] args) {
    // Use cross-values for the interesting values.
    long[] interesting = {
      0x0000000000000000L, 0x0000000000000001L, 0x000000007fffffffL,
      0x0000000080000000L, 0x0000000080000001L, 0x00000000ffffffffL,
      0x0000000100000000L, 0x0000000100000001L, 0x000000017fffffffL,
      0x0000000180000000L, 0x0000000180000001L, 0x00000001ffffffffL,
      0x7fffffff00000000L, 0x7fffffff00000001L, 0x7fffffff7fffffffL,
      0x7fffffff80000000L, 0x7fffffff80000001L, 0x7fffffffffffffffL,
      0x8000000000000000L, 0x8000000000000001L, 0x800000007fffffffL,
      0x8000000080000000L, 0x8000000080000001L, 0x80000000ffffffffL,
      0x8000000100000000L, 0x8000000100000001L, 0x800000017fffffffL,
      0x8000000180000000L, 0x8000000180000001L, 0x80000001ffffffffL,
      0xffffffff00000000L, 0xffffffff00000001L, 0xffffffff7fffffffL,
      0xffffffff80000000L, 0xffffffff80000001L, 0xffffffffffffffffL
    };
    for (int i = 0; i < interesting.length; i++) {
      for (int j = 0; j < interesting.length; j++) {
        long x = interesting[i];
        long y = interesting[j];
        long e1 = x >= y ? x - y : y - x;  // still select
        expectEquals(e1, sad1(x, y));
        long e2 = Math.abs(x - y);  // pure abs
        expectEquals(e2, sad2(x, y));
        expectEquals(e2, sad3(x, y));
        expectEquals(e2, sad3Alt(x, y));
      }
    }
    System.out.println("passed");
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
