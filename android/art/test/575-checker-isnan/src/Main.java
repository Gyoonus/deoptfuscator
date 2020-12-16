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

  /// CHECK-START: boolean Main.isNaN32(float) instruction_simplifier (before)
  /// CHECK-DAG: <<Result:z\d+>> InvokeStaticOrDirect
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: boolean Main.isNaN32(float) instruction_simplifier (after)
  /// CHECK-DAG: <<Result:z\d+>> NotEqual
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: boolean Main.isNaN32(float) instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect
  private static boolean isNaN32(float x) {
    return Float.isNaN(x);
  }

  /// CHECK-START: boolean Main.isNaN64(double) instruction_simplifier (before)
  /// CHECK-DAG: <<Result:z\d+>> InvokeStaticOrDirect
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: boolean Main.isNaN64(double) instruction_simplifier (after)
  /// CHECK-DAG: <<Result:z\d+>> NotEqual
  /// CHECK-DAG: Return [<<Result>>]
  //
  /// CHECK-START: boolean Main.isNaN64(double) instruction_simplifier (after)
  /// CHECK-NOT: InvokeStaticOrDirect
  private static boolean isNaN64(double x) {
    return Double.isNaN(x);
  }

  public static void main(String args[]) {
    // A few distinct numbers.
    expectFalse(isNaN32(Float.NEGATIVE_INFINITY));
    expectFalse(isNaN32(-1.0f));
    expectFalse(isNaN32(-0.0f));
    expectFalse(isNaN32(0.0f));
    expectFalse(isNaN32(1.0f));
    expectFalse(isNaN32(Float.POSITIVE_INFINITY));

    // A few distinct subnormal numbers.
    expectFalse(isNaN32(Float.intBitsToFloat(0x00400000)));
    expectFalse(isNaN32(Float.intBitsToFloat(0x80400000)));
    expectFalse(isNaN32(Float.intBitsToFloat(0x00000001)));
    expectFalse(isNaN32(Float.intBitsToFloat(0x80000001)));

    // A few NaN numbers.
    expectTrue(isNaN32(Float.NaN));
    expectTrue(isNaN32(0.0f / 0.0f));
    expectTrue(isNaN32((float)Math.sqrt(-1.0f)));
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
      expectTrue(isNaN32(fvals[i]));
    }

    // A few distinct numbers.
    expectFalse(isNaN64(Double.NEGATIVE_INFINITY));
    expectFalse(isNaN32(-1.0f));
    expectFalse(isNaN64(-0.0d));
    expectFalse(isNaN64(0.0d));
    expectFalse(isNaN64(1.0d));
    expectFalse(isNaN64(Double.POSITIVE_INFINITY));

    // A few distinct subnormal numbers.
    expectFalse(isNaN64(Double.longBitsToDouble(0x0008000000000000l)));
    expectFalse(isNaN64(Double.longBitsToDouble(0x8008000000000000l)));
    expectFalse(isNaN64(Double.longBitsToDouble(0x0000000000000001l)));
    expectFalse(isNaN64(Double.longBitsToDouble(0x8000000000000001l)));

    // A few NaN numbers.
    expectTrue(isNaN64(Double.NaN));
    expectTrue(isNaN64(0.0d / 0.0d));
    expectTrue(isNaN64(Math.sqrt(-1.0d)));
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
      expectTrue(isNaN64(dvals[i]));
    }

    System.out.println("passed");
  }

  private static void expectTrue(boolean value) {
    if (!value) {
      throw new Error("Expected True");
    }
  }

  private static void expectFalse(boolean value) {
    if (value) {
      throw new Error("Expected False");
    }
  }
}
