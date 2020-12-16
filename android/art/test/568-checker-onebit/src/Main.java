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

public class Main {

  /// CHECK-START: int Main.hi32(int) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerHighestOneBit
  /// CHECK-DAG:                 Return [<<Result>>]
  private static int hi32(int x) {
    return Integer.highestOneBit(x);
  }

  /// CHECK-START: int Main.lo32(int) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerLowestOneBit
  /// CHECK-DAG:                 Return [<<Result>>]
  private static int lo32(int x) {
    return Integer.lowestOneBit(x);
  }

  /// CHECK-START: long Main.hi64(long) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:j\d+>> InvokeStaticOrDirect intrinsic:LongHighestOneBit
  /// CHECK-DAG:                 Return [<<Result>>]
  private static long hi64(long x) {
    return Long.highestOneBit(x);
  }

  /// CHECK-START: long Main.lo64(long) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:j\d+>> InvokeStaticOrDirect intrinsic:LongLowestOneBit
  /// CHECK-DAG:                 Return [<<Result>>]
  private static long lo64(long x) {
    return Long.lowestOneBit(x);
  }

  public static void main(String args[]) {
    // Hidden zeros.
    int[] xi = new int[32];
    long[] xj = new long[64];

    expectEquals32(0x00000000, hi32(0x00000000));
    expectEquals32(0x00000000, lo32(0x00000000));
    expectEquals32(0x00010000, hi32(0x00010000));
    expectEquals32(0x00010000, lo32(0x00010000));
    expectEquals32(0x00800000, hi32(0x00FF0000));
    expectEquals32(0x00010000, lo32(0x00FF0000));
    expectEquals32(0x80000000, hi32(0xFFFFFFFF));
    expectEquals32(0x00000001, lo32(0xFFFFFFFF));

    for (int i = 0; i < 32; i++) {
      expectEquals32(0, hi32(xi[i]));
      expectEquals32(0, lo32(xi[i]));
      expectEquals32(1 << i, hi32(1 << i));
      expectEquals32(1 << i, lo32(1 << i));
      int expected = i < 29 ? 0x8 << i : 0x80000000;
      expectEquals32(expected, hi32(0xF << i));
      expectEquals32(0x1 << i, lo32(0xF << i));
    }

    expectEquals64(0x0000000000000000L, hi64(0x0000000000000000L));
    expectEquals64(0x0000000000000000L, lo64(0x0000000000000000L));
    expectEquals64(0x0000000100000000L, hi64(0x0000000100000000L));
    expectEquals64(0x0000000100000000L, lo64(0x0000000100000000L));
    expectEquals64(0x0000008000000000L, hi64(0x000000FF00000000L));
    expectEquals64(0x0000000100000000L, lo64(0x000000FF00000000L));
    expectEquals64(0x8000000000000000L, hi64(0xFFFFFFFFFFFFFFFFL));
    expectEquals64(0x0000000000000001L, lo64(0xFFFFFFFFFFFFFFFFL));

    for (int i = 0; i < 64; i++) {
      expectEquals64(0L, hi64(xj[i]));
      expectEquals64(0L, lo64(xj[i]));
      expectEquals64(1L << i, hi64(1L << i));
      expectEquals64(1L << i, lo64(1L << i));
      long expected = i < 61 ? 0x8L << i : 0x8000000000000000L;
      expectEquals64(expected, hi64(0xFL << i));
      expectEquals64(0x1L << i, lo64(0xFL << i));
    }

    System.out.println("passed");
  }

  private static void expectEquals32(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
  private static void expectEquals64(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
