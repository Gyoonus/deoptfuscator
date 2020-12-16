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

  /// CHECK-START: int Main.sad1(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad1(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad1(char x, char y) {
    return x >= y ? x - y : y - x;
  }

  /// CHECK-START: int Main.sad2(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad2(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad2(char x, char y) {
    int diff = x - y;
    if (diff < 0) diff = -diff;
    return diff;
  }

  /// CHECK-START: int Main.sad3(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3(char x, char y) {
    int diff = x - y;
    return diff >= 0 ? diff : -diff;
  }

  /// CHECK-START: int Main.sad3Alt(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:i\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: int Main.sad3Alt(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static int sad3Alt(char x, char y) {
    int diff = x - y;
    return 0 <= diff ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL1(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL1(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL1(char x, char y) {
    long xl = x;
    long yl = y;
    return xl >= yl ? xl - yl : yl - xl;
  }

  /// CHECK-START: long Main.sadL2(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL2(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL2(char x, char y) {
    long diff = x - y;
    if (diff < 0L) diff = -diff;
    return diff;
  }

  /// CHECK-START: long Main.sadL3(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3(char x, char y) {
    long diff = x - y;
    return diff >= 0L ? diff : -diff;
  }

  /// CHECK-START: long Main.sadL3Alt(char, char) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Select:j\d+>> Select
  /// CHECK-DAG:                 Return [<<Select>>]
  //
  /// CHECK-START: long Main.sadL3Alt(char, char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect intrinsic:MathAbsLong
  /// CHECK-DAG:                 Return [<<Intrin>>]
  static long sadL3Alt(char x, char y) {
    long diff = x - y;
    return 0L <= diff ? diff : -diff;
  }

  public static void main(String[] args) {
    // Use cross-values to test all cases.
    char[] interesting = {
      (char) 0x0000, (char) 0x0001, (char) 0x007f,
      (char) 0x0080, (char) 0x0081, (char) 0x00ff,
      (char) 0x0100, (char) 0x0101, (char) 0x017f,
      (char) 0x0180, (char) 0x0181, (char) 0x01ff,
      (char) 0x7f00, (char) 0x7f01, (char) 0x7f7f,
      (char) 0x7f80, (char) 0x7f81, (char) 0x7fff,
      (char) 0x8000, (char) 0x8001, (char) 0x807f,
      (char) 0x8080, (char) 0x8081, (char) 0x80ff,
      (char) 0x8100, (char) 0x8101, (char) 0x817f,
      (char) 0x8180, (char) 0x8181, (char) 0x81ff,
      (char) 0xff00, (char) 0xff01, (char) 0xff7f,
      (char) 0xff80, (char) 0xff81, (char) 0xffff
    };
    for (int i = 0; i < interesting.length; i++) {
      for (int j = 0; j < interesting.length; j++) {
        char x = interesting[i];
        char y = interesting[j];
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
