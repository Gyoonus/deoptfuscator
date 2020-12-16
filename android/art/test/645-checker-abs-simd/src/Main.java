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
 * Tests for ABS vectorization.
 */
public class Main {

  private static final int SPQUIET = 1 << 22;
  private static final long DPQUIET = 1L << 51;

  /// CHECK-START: void Main.doitByte(byte[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.doitByte(byte[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                   loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitByte(byte[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = (byte) Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitChar(char[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.doitChar(char[]) loop_optimization (after)
  /// CHECK-NOT: VecAbs
  private static void doitChar(char[] x) {
    // Basically a nop due to zero extension.
    for (int i = 0; i < x.length; i++) {
      x[i] = (char) Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitShort(short[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.doitShort(short[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                   loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitShort(short[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = (short) Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitCastedChar(char[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.doitCastedChar(char[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                   loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitCastedChar(char[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = (char) Math.abs((short) x[i]);
    }
  }

  /// CHECK-START: void Main.doitInt(int[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.doitInt(int[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                   loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                  loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsInt loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                  loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitInt(int[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitLong(long[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsLong loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.doitLong(long[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                    loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                     loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                   loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                   loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsLong loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                   loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitLong(long[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitFloat(float[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsFloat loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.doitFloat(float[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                     loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                      loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                    loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsFloat loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                    loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitFloat(float[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = Math.abs(x[i]);
    }
  }

  /// CHECK-START: void Main.doitDouble(double[]) loop_optimization (before)
  /// CHECK-DAG: Phi                                          loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayGet                                     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsDouble loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                     loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.doitDouble(double[]) loop_optimization (after)
  /// CHECK-DAG: VecLoad                                      loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAbs                                       loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: VecStore                                     loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: ArrayGet                                     loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeStaticOrDirect intrinsic:MathAbsDouble loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: ArraySet                                     loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  private static void doitDouble(double[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = Math.abs(x[i]);
    }
  }

  public static void main(String[] args) {
    // Bytes, chars, shorts.
    byte[] xb = new byte[256];
    for (int i = 0; i < 256; i++) {
      xb[i] = (byte) i;
    }
    doitByte(xb);
    for (int i = 0; i < 256; i++) {
      expectEquals32((byte) Math.abs((byte) i), xb[i]);
    }
    char[] xc = new char[1024 * 64];
    for (int i = 0; i < 1024 * 64; i++) {
      xc[i] = (char) i;
    }
    doitChar(xc);
    for (int i = 0; i < 1024 * 64; i++) {
      expectEquals32((char) Math.abs((char) i), xc[i]);
    }
    short[] xs = new short[1024 * 64];
    for (int i = 0; i < 1024 * 64; i++) {
      xs[i] = (short) i;
    }
    doitShort(xs);
    for (int i = 0; i < 1024 * 64; i++) {
      expectEquals32((short) Math.abs((short) i), xs[i]);
    }
    for (int i = 0; i < 1024 * 64; i++) {
      xc[i] = (char) i;
    }
    doitCastedChar(xc);
    for (int i = 0; i < 1024 * 64; i++) {
      expectEquals32((char) Math.abs((short) i), xc[i]);
    }
    // Set up minint32, maxint32 and some others.
    int[] xi = new int[8];
    xi[0] = 0x80000000;
    xi[1] = 0x7fffffff;
    xi[2] = 0x80000001;
    xi[3] = -13;
    xi[4] = -1;
    xi[5] = 0;
    xi[6] = 1;
    xi[7] = 999;
    doitInt(xi);
    expectEquals32(0x80000000, xi[0]);
    expectEquals32(0x7fffffff, xi[1]);
    expectEquals32(0x7fffffff, xi[2]);
    expectEquals32(13, xi[3]);
    expectEquals32(1, xi[4]);
    expectEquals32(0, xi[5]);
    expectEquals32(1, xi[6]);
    expectEquals32(999, xi[7]);

    // Set up minint64, maxint64 and some others.
    long[] xl = new long[8];
    xl[0] = 0x8000000000000000L;
    xl[1] = 0x7fffffffffffffffL;
    xl[2] = 0x8000000000000001L;
    xl[3] = -13;
    xl[4] = -1;
    xl[5] = 0;
    xl[6] = 1;
    xl[7] = 999;
    doitLong(xl);
    expectEquals64(0x8000000000000000L, xl[0]);
    expectEquals64(0x7fffffffffffffffL, xl[1]);
    expectEquals64(0x7fffffffffffffffL, xl[2]);
    expectEquals64(13, xl[3]);
    expectEquals64(1, xl[4]);
    expectEquals64(0, xl[5]);
    expectEquals64(1, xl[6]);
    expectEquals64(999, xl[7]);

    // Set up float NaN and some others.
    float[] xf = new float[16];
    xf[0] = Float.intBitsToFloat(0x7f800001);
    xf[1] = Float.intBitsToFloat(0x7fa00000);
    xf[2] = Float.intBitsToFloat(0x7fc00000);
    xf[3] = Float.intBitsToFloat(0x7fffffff);
    xf[4] = Float.intBitsToFloat(0xff800001);
    xf[5] = Float.intBitsToFloat(0xffa00000);
    xf[6] = Float.intBitsToFloat(0xffc00000);
    xf[7] = Float.intBitsToFloat(0xffffffff);
    xf[8] = Float.NEGATIVE_INFINITY;
    xf[9] = -99.2f;
    xf[10] = -1.0f;
    xf[11] = -0.0f;
    xf[12] = +0.0f;
    xf[13] = +1.0f;
    xf[14] = +99.2f;
    xf[15] = Float.POSITIVE_INFINITY;
    doitFloat(xf);
    expectEqualsNaN32(0x7f800001, Float.floatToRawIntBits(xf[0]));
    expectEqualsNaN32(0x7fa00000, Float.floatToRawIntBits(xf[1]));
    expectEqualsNaN32(0x7fc00000, Float.floatToRawIntBits(xf[2]));
    expectEqualsNaN32(0x7fffffff, Float.floatToRawIntBits(xf[3]));
    expectEqualsNaN32(0x7f800001, Float.floatToRawIntBits(xf[4]));
    expectEqualsNaN32(0x7fa00000, Float.floatToRawIntBits(xf[5]));
    expectEqualsNaN32(0x7fc00000, Float.floatToRawIntBits(xf[6]));
    expectEqualsNaN32(0x7fffffff, Float.floatToRawIntBits(xf[7]));
    expectEquals32(
        Float.floatToRawIntBits(Float.POSITIVE_INFINITY),
        Float.floatToRawIntBits(xf[8]));
    expectEquals32(
        Float.floatToRawIntBits(99.2f),
        Float.floatToRawIntBits(xf[9]));
    expectEquals32(
        Float.floatToRawIntBits(1.0f),
        Float.floatToRawIntBits(xf[10]));
    expectEquals32(0, Float.floatToRawIntBits(xf[11]));
    expectEquals32(0, Float.floatToRawIntBits(xf[12]));
    expectEquals32(
        Float.floatToRawIntBits(1.0f),
        Float.floatToRawIntBits(xf[13]));
    expectEquals32(
        Float.floatToRawIntBits(99.2f),
        Float.floatToRawIntBits(xf[14]));
    expectEquals32(
        Float.floatToRawIntBits(Float.POSITIVE_INFINITY),
        Float.floatToRawIntBits(xf[15]));

    // Set up double NaN and some others.
    double[] xd = new double[16];
    xd[0] = Double.longBitsToDouble(0x7ff0000000000001L);
    xd[1] = Double.longBitsToDouble(0x7ff4000000000000L);
    xd[2] = Double.longBitsToDouble(0x7ff8000000000000L);
    xd[3] = Double.longBitsToDouble(0x7fffffffffffffffL);
    xd[4] = Double.longBitsToDouble(0xfff0000000000001L);
    xd[5] = Double.longBitsToDouble(0xfff4000000000000L);
    xd[6] = Double.longBitsToDouble(0xfff8000000000000L);
    xd[7] = Double.longBitsToDouble(0xffffffffffffffffL);
    xd[8] = Double.NEGATIVE_INFINITY;
    xd[9] = -99.2f;
    xd[10] = -1.0f;
    xd[11] = -0.0f;
    xd[12] = +0.0f;
    xd[13] = +1.0f;
    xd[14] = +99.2f;
    xd[15] = Double.POSITIVE_INFINITY;
    doitDouble(xd);
    expectEqualsNaN64(0x7ff0000000000001L, Double.doubleToRawLongBits(xd[0]));
    expectEqualsNaN64(0x7ff4000000000000L, Double.doubleToRawLongBits(xd[1]));
    expectEqualsNaN64(0x7ff8000000000000L, Double.doubleToRawLongBits(xd[2]));
    expectEqualsNaN64(0x7fffffffffffffffL, Double.doubleToRawLongBits(xd[3]));
    expectEqualsNaN64(0x7ff0000000000001L, Double.doubleToRawLongBits(xd[4]));
    expectEqualsNaN64(0x7ff4000000000000L, Double.doubleToRawLongBits(xd[5]));
    expectEqualsNaN64(0x7ff8000000000000L, Double.doubleToRawLongBits(xd[6]));
    expectEqualsNaN64(0x7fffffffffffffffL, Double.doubleToRawLongBits(xd[7]));
    expectEquals64(
        Double.doubleToRawLongBits(Double.POSITIVE_INFINITY),
        Double.doubleToRawLongBits(xd[8]));
    expectEquals64(
        Double.doubleToRawLongBits(99.2f),
        Double.doubleToRawLongBits(xd[9]));
    expectEquals64(
        Double.doubleToRawLongBits(1.0f),
        Double.doubleToRawLongBits(xd[10]));
    expectEquals64(0, Double.doubleToRawLongBits(xd[11]));
    expectEquals64(0, Double.doubleToRawLongBits(xd[12]));
    expectEquals64(
        Double.doubleToRawLongBits(1.0f),
        Double.doubleToRawLongBits(xd[13]));
    expectEquals64(
        Double.doubleToRawLongBits(99.2f),
        Double.doubleToRawLongBits(xd[14]));
    expectEquals64(
        Double.doubleToRawLongBits(Double.POSITIVE_INFINITY),
        Double.doubleToRawLongBits(xd[15]));

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

  // We allow that an expected NaN result has become quiet.
  private static void expectEqualsNaN32(int expected, int result) {
    if (expected != result && (expected | SPQUIET) != result) {
      throw new Error("Expected: 0x" + Integer.toHexString(expected)
          + ", found: 0x" + Integer.toHexString(result));
    }
  }

  // We allow that an expected NaN result has become quiet.
  private static void expectEqualsNaN64(long expected, long result) {
    if (expected != result && (expected | DPQUIET) != result) {
      throw new Error("Expected: 0x" + Long.toHexString(expected)
          + ", found: 0x" + Long.toHexString(result));
    }
  }
}
