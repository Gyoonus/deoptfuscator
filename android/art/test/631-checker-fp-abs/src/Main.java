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

/**
 * A few tests of Math.abs for floating-point data.
 *
 * Note, as a "quality of implementation", rather than pure "spec compliance",
 * we require that Math.abs() clears the sign bit (but changes nothing else)
 * for all numbers, including NaN (signaling NaN may become quiet though).
 */
public class Main {

  private static final int SPQUIET = 1 << 22;
  private static final long DPQUIET = 1L << 51;

  public static boolean doThrow = false;

  /// CHECK-START: float Main.$opt$noinline$absSP(float) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:f\d+>> InvokeStaticOrDirect intrinsic:MathAbsFloat
  /// CHECK-DAG:                 Return [<<Result>>]
  private static float $opt$noinline$absSP(float f) {
    if (doThrow) {
      throw new Error("Something to prevent inlining");
    }
    return Math.abs(f);
  }

  /// CHECK-START: double Main.$opt$noinline$absDP(double) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:d\d+>> InvokeStaticOrDirect intrinsic:MathAbsDouble
  /// CHECK-DAG:                 Return [<<Result>>]
  private static double $opt$noinline$absDP(double d) {
    if (doThrow) {
      throw new Error("Something to prevent inlining");
    }
    return Math.abs(d);
  }

  public static void main(String args[]) {
    // A few obvious numbers.
    for (float f = -100.0f; f < 0.0f; f += 0.5f) {
      expectEqualsSP(-f, $opt$noinline$absSP(f));
    }
    for (float f = 0.0f; f <= 100.0f; f += 0.5f) {
      expectEqualsSP(f, $opt$noinline$absSP(f));
    }
    for (float f = -1.5f; f <= -1.499f; f = Math.nextAfter(f, Float.POSITIVE_INFINITY)) {
      expectEqualsSP(-f, $opt$noinline$absSP(f));
    }
    for (float f = 1.499f; f <= 1.5f; f = Math.nextAfter(f, Float.POSITIVE_INFINITY)) {
      expectEqualsSP(f, $opt$noinline$absSP(f));
    }

    // Zero
    expectEquals32(0, Float.floatToRawIntBits($opt$noinline$absSP(+0.0f)));
    expectEquals32(0, Float.floatToRawIntBits($opt$noinline$absSP(-0.0f)));

    // Inf.
    expectEqualsSP(Float.POSITIVE_INFINITY, $opt$noinline$absSP(Float.NEGATIVE_INFINITY));
    expectEqualsSP(Float.POSITIVE_INFINITY, $opt$noinline$absSP(Float.POSITIVE_INFINITY));

    // A few NaN numbers.
    int[] spnans = {
      0x7f800001,
      0x7fa00000,
      0x7fc00000,
      0x7fffffff,
      0xff800001,
      0xffa00000,
      0xffc00000,
      0xffffffff
    };
    for (int i = 0; i < spnans.length; i++) {
      float f = Float.intBitsToFloat(spnans[i]);
      expectEqualsNaN32(
          spnans[i] & Integer.MAX_VALUE,
          Float.floatToRawIntBits($opt$noinline$absSP(f)));
    }

    // A few obvious numbers.
    for (double d = -100.0; d < 0.0; d += 0.5) {
      expectEqualsDP(-d, $opt$noinline$absDP(d));
    }
    for (double d = 0.0; d <= 100.0; d += 0.5) {
      expectEqualsDP(d, $opt$noinline$absDP(d));
    }
    for (double d = -1.5d; d <= -1.49999999999d; d = Math.nextAfter(d, Double.POSITIVE_INFINITY)) {
      expectEqualsDP(-d, $opt$noinline$absDP(d));
    }
    for (double d = 1.49999999999d; d <= 1.5; d = Math.nextAfter(d, Double.POSITIVE_INFINITY)) {
      expectEqualsDP(d, $opt$noinline$absDP(d));
    }

    // Zero
    expectEquals64(0L, Double.doubleToRawLongBits($opt$noinline$absDP(+0.0f)));
    expectEquals64(0L, Double.doubleToRawLongBits($opt$noinline$absDP(-0.0f)));

    // Inf.
    expectEqualsDP(Double.POSITIVE_INFINITY, $opt$noinline$absDP(Double.NEGATIVE_INFINITY));
    expectEqualsDP(Double.POSITIVE_INFINITY, $opt$noinline$absDP(Double.POSITIVE_INFINITY));

    // A few NaN numbers.
    long[] dpnans = {
      0x7ff0000000000001L,
      0x7ff4000000000000L,
      0x7ff8000000000000L,
      0x7fffffffffffffffL,
      0xfff0000000000001L,
      0xfff4000000000000L,
      0xfff8000000000000L,
      0xffffffffffffffffL
    };
    for (int i = 0; i < dpnans.length; i++) {
      double d = Double.longBitsToDouble(dpnans[i]);
      expectEqualsNaN64(
          dpnans[i] & Long.MAX_VALUE,
          Double.doubleToRawLongBits($opt$noinline$absDP(d)));
    }

    System.out.println("passed");
  }

  private static void expectEquals32(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: 0x" + Integer.toHexString(expected)
          + ", found: 0x" + Integer.toHexString(result));
    }
  }

  // We allow that an expected NaN result has become quiet.
  private static void expectEqualsNaN32(int expected, int result) {
    if (expected != result && (expected | SPQUIET) != result) {
      throw new Error("Expected: 0x" + Integer.toHexString(expected)
          + ", found: 0x" + Integer.toHexString(result));
    }
  }

  private static void expectEquals64(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: 0x" + Long.toHexString(expected)
          + ", found: 0x" + Long.toHexString(result));
    }
  }

  // We allow that an expected NaN result has become quiet.
  private static void expectEqualsNaN64(long expected, long result) {
    if (expected != result && (expected | DPQUIET) != result) {
      throw new Error("Expected: 0x" + Long.toHexString(expected)
          + ", found: 0x" + Long.toHexString(result));
    }
  }

  private static void expectEqualsSP(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsDP(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
