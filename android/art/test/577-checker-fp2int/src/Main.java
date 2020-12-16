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

  /// CHECK-START: int Main.f2int(float) instruction_simplifier (before)
  /// CHECK-DAG: <<Result:i\d+>> InvokeStaticOrDirect intrinsic:FloatFloatToIntBits
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: int Main.f2int(float) instruction_simplifier (after)
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG: <<Raw:i\d+>> InvokeStaticOrDirect [<<Arg:f\d+>>{{(,[ij]\d+)?}}] intrinsic:FloatFloatToRawIntBits
  /// CHECK-DAG: <<Cond:z\d+>> NotEqual [<<Arg>>,<<Arg>>]
  /// CHECK-DAG: <<Result:i\d+>> Select [<<Raw>>,{{i\d+}},<<Cond>>]
  /// CHECK-DAG: Return [<<Result>>]
  private static int f2int(float f) {
    return Float.floatToIntBits(f);
  }

  /// CHECK-START: long Main.d2long(double) instruction_simplifier (before)
  /// CHECK-DAG: <<Result:j\d+>> InvokeStaticOrDirect intrinsic:DoubleDoubleToLongBits
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: long Main.d2long(double) instruction_simplifier (after)
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG: <<Raw:j\d+>> InvokeStaticOrDirect [<<Arg:d\d+>>{{(,[ij]\d+)?}}] intrinsic:DoubleDoubleToRawLongBits
  /// CHECK-DAG: <<Cond:z\d+>> NotEqual [<<Arg>>,<<Arg>>]
  /// CHECK-DAG: <<Result:j\d+>> Select [<<Raw>>,{{j\d+}},<<Cond>>]
  /// CHECK-DAG: Return [<<Result>>]
  private static long d2long(double d) {
    return Double.doubleToLongBits(d);
  }

  public static void main(String args[]) {
    // A few distinct numbers.
    expectEquals32(0xff800000, f2int(Float.NEGATIVE_INFINITY));
    expectEquals32(0xbf800000, f2int(-1.0f));
    expectEquals32(0x80000000, f2int(-0.0f));
    expectEquals32(0x00000000, f2int(+0.0f));
    expectEquals32(0x3f800000, f2int(+1.0f));
    expectEquals32(0x7f800000, f2int(Float.POSITIVE_INFINITY));

    // A few others.
    for (int i = 0; i <= 100; i++) {
      expectEquals32(i, f2int(Float.intBitsToFloat(i)));
    }

    // A few NaN numbers.
    float[] fvals = {
      Float.intBitsToFloat(0x7f800001),
      Float.intBitsToFloat(0x7fa00000),
      Float.intBitsToFloat(0x7fc00000),
      Float.intBitsToFloat(0x7fffffff),
      Float.intBitsToFloat(0xff800001),
      Float.intBitsToFloat(0xffa00000),
      Float.intBitsToFloat(0xffc00000),
      Float.intBitsToFloat(0xffffffff)
    };
    for (int i = 0; i < fvals.length; i++) {
      expectEquals32(0x7fc00000, f2int(fvals[i]));
    }

    // A few distinct numbers.
    expectEquals64(0xfff0000000000000L, d2long(Double.NEGATIVE_INFINITY));
    expectEquals64(0xbff0000000000000L, d2long(-1.0d));
    expectEquals64(0x8000000000000000L, d2long(-0.0d));
    expectEquals64(0x0000000000000000L, d2long(+0.0d));
    expectEquals64(0x3ff0000000000000L, d2long(+1.0d));
    expectEquals64(0x7ff0000000000000L, d2long(Double.POSITIVE_INFINITY));

    // A few others.
    for (long l = 0; l <= 100; l++) {
      expectEquals64(l, d2long(Double.longBitsToDouble(l)));
    }

    // A few NaN numbers.
    double[] dvals = {
      Double.longBitsToDouble(0x7ff0000000000001L),
      Double.longBitsToDouble(0x7ff4000000000000L),
      Double.longBitsToDouble(0x7ff8000000000000L),
      Double.longBitsToDouble(0x7fffffffffffffffL),
      Double.longBitsToDouble(0xfff0000000000001L),
      Double.longBitsToDouble(0xfff4000000000000L),
      Double.longBitsToDouble(0xfff8000000000000L),
      Double.longBitsToDouble(0xffffffffffffffffL)
    };
    for (int i = 0; i < dvals.length; i++) {
      expectEquals64(0x7ff8000000000000L, d2long(dvals[i]));
    }

    System.out.println("passed");
  }

  private static void expectEquals32(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: "
          + Integer.toHexString(expected)
          + ", found: "
          + Integer.toHexString(result));
    }
  }

  private static void expectEquals64(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: "
          + Long.toHexString(expected)
          + ", found: "
          + Long.toHexString(result));
    }
  }
}
