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

  /// CHECK-START: int Main.round32(float) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:i\d+>> InvokeStaticOrDirect intrinsic:MathRoundFloat
  /// CHECK-DAG:                 Return [<<Result>>]
  private static int round32(float f) {
    return Math.round(f);
  }

  /// CHECK-START: long Main.round64(double) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:j\d+>> InvokeStaticOrDirect intrinsic:MathRoundDouble
  /// CHECK-DAG:                 Return [<<Result>>]
  private static long round64(double d) {
    return Math.round(d);
  }

  public static void main(String args[]) {
    // A few obvious numbers.
    expectEquals32(-2147483648, round32(Float.NEGATIVE_INFINITY));
    expectEquals32(-2, round32(-1.51f));
    expectEquals32(-1, round32(-1.2f));
    expectEquals32(-1, round32(-1.0f));
    expectEquals32(-1, round32(-0.5000001f));
    expectEquals32(0, round32(-0.5f));
    expectEquals32(0, round32(-0.2f));
    expectEquals32(0, round32(-0.0f));
    expectEquals32(0, round32(+0.0f));
    expectEquals32(0, round32(+0.2f));
    expectEquals32(1, round32(+0.5f));
    expectEquals32(1, round32(+1.0f));
    expectEquals32(1, round32(+1.2f));
    expectEquals32(2, round32(+1.5f));
    expectEquals32(2147483647, round32(Float.POSITIVE_INFINITY));

    // Near minint.
    expectEquals32(-2147483648, round32(Math.nextAfter(-2147483648.0f, Float.NEGATIVE_INFINITY)));
    expectEquals32(-2147483648, round32(-2147483648.0f));
    expectEquals32(-2147483520, round32(Math.nextAfter(-2147483648.0f, Float.POSITIVE_INFINITY)));

    // Near maxint.
    expectEquals32(2147483520, round32(Math.nextAfter(2147483648.0f, Float.NEGATIVE_INFINITY)));
    expectEquals32(2147483647, round32(2147483648.0f));
    expectEquals32(2147483647, round32(Math.nextAfter(2147483648.0f, Float.POSITIVE_INFINITY)));

    // Some others.
    for (int i = -100; i <= 100; ++i) {
      expectEquals32(i - 1, round32((float) i - 0.51f));
      expectEquals32(i, round32((float) i - 0.5f));
      expectEquals32(i, round32((float) i));
      expectEquals32(i + 1, round32((float) i + 0.5f));
      expectEquals32(i + 1, round32((float) i + 0.51f));
    }
    for (float f = -1.5f; f <= -1.499f; f = Math.nextAfter(f, Float.POSITIVE_INFINITY)) {
      expectEquals32(-1, round32(f));
    }

    // Some harder.
    float[] fvals = {
      -16777215.5f,
      -16777215.0f,
      -0.49999998f,
      -0.4999999701976776123046875f,
      0.4999999701976776123046875f,
      0.49999998f,
      16777215.0f,
      16777215.5f
    };
    int[] ivals = {
      -16777216,
      -16777215,
      0,
      0,
      0,
      0,
      16777215,
      16777216
    };
    for (int i = 0; i < fvals.length; i++) {
      expectEquals32(ivals[i], round32(fvals[i]));
    }

    // A few NaN numbers.
    float[] fnans = {
      Float.intBitsToFloat(0x7f800001),
      Float.intBitsToFloat(0x7fa00000),
      Float.intBitsToFloat(0x7fc00000),
      Float.intBitsToFloat(0x7fffffff),
      Float.intBitsToFloat(0xff800001),
      Float.intBitsToFloat(0xffa00000),
      Float.intBitsToFloat(0xffc00000),
      Float.intBitsToFloat(0xffffffff)
    };
    for (int i = 0; i < fnans.length; i++) {
      expectEquals32(0, round32(fnans[i]));
    }

    // A few obvious numbers.
    expectEquals64(-9223372036854775808L, round64(Double.NEGATIVE_INFINITY));
    expectEquals64(-2L, round64(-1.51d));
    expectEquals64(-1L, round64(-1.2d));
    expectEquals64(-1L, round64(-1.0d));
    expectEquals64(-1L, round64(-0.5000001f));
    expectEquals64(0L, round64(-0.5d));
    expectEquals64(0L, round64(-0.2d));
    expectEquals64(0L, round64(-0.0d));
    expectEquals64(0L, round64(+0.0d));
    expectEquals64(0L, round64(+0.2d));
    expectEquals64(1L, round64(+0.5d));
    expectEquals64(1L, round64(+1.0d));
    expectEquals64(1L, round64(+1.2d));
    expectEquals64(2L, round64(+1.5d));
    expectEquals64(9223372036854775807L, round64(Double.POSITIVE_INFINITY));

    // Near minlong.
    expectEquals64(-9223372036854775808L,
        round64(Math.nextAfter(-9223372036854775808.0, Double.NEGATIVE_INFINITY)));
    expectEquals64(-9223372036854775808L, round64(-9223372036854775808.0));
    expectEquals64(-9223372036854774784L,
        round64(Math.nextAfter(-9223372036854775809.0, Double.POSITIVE_INFINITY)));

    // Near maxlong.
    expectEquals64(9223372036854774784L,
        round64(Math.nextAfter(9223372036854775808.0, Double.NEGATIVE_INFINITY)));
    expectEquals64(9223372036854775807L, round64(9223372036854775808.0));
    expectEquals64(9223372036854775807L,
        round64(Math.nextAfter(9223372036854775808.0, Double.POSITIVE_INFINITY)));

    // Some others.
    for (long l = -100; l <= 100; ++l) {
      expectEquals64(l - 1, round64((double) l - 0.51d));
      expectEquals64(l, round64((double) l - 0.5d));
      expectEquals64(l, round64((double) l));
      expectEquals64(l + 1, round64((double) l + 0.5d));
      expectEquals64(l + 1, round64((double) l + 0.51d));
    }
    for (double d = -1.5d; d <= -1.49999999999d; d = Math.nextAfter(d, Double.POSITIVE_INFINITY)) {
      expectEquals64(-1L, round64(d));
    }

    // Some harder.
    double[] dvals = {
      -9007199254740991.5d,
      -9007199254740991.0d,
      -0.49999999999999997d,
      -0.49999999999999994d,
      0.49999999999999994d,
      0.49999999999999997d,
      9007199254740991.0d,
      9007199254740991.5d
    };
    long[] lvals = {
      -9007199254740992L,
      -9007199254740991L,
      0L,
      0L,
      0L,
      0L,
      9007199254740991L,
      9007199254740992L
    };
    for (int i = 0; i < dvals.length; i++) {
      expectEquals64(lvals[i], round64(dvals[i]));
    }

    // A few NaN numbers.
    double[] dnans = {
      Double.longBitsToDouble(0x7ff0000000000001L),
      Double.longBitsToDouble(0x7ff4000000000000L),
      Double.longBitsToDouble(0x7ff8000000000000L),
      Double.longBitsToDouble(0x7fffffffffffffffL),
      Double.longBitsToDouble(0xfff0000000000001L),
      Double.longBitsToDouble(0xfff4000000000000L),
      Double.longBitsToDouble(0xfff8000000000000L),
      Double.longBitsToDouble(0xffffffffffffffffL)
    };
    for (int i = 0; i < dnans.length; i++) {
      expectEquals64(0L, round64(dnans[i]));
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
