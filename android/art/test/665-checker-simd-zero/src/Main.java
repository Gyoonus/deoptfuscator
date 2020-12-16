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
 * Tests for zero vectorization.
 */
public class Main {

  /// CHECK-START: void Main.zeroz(boolean[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zeroz(boolean[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zeroz(boolean[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = false;
    }
  }

  /// CHECK-START: void Main.zerob(byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zerob(byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zerob(byte[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zeroc(char[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zeroc(char[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zeroc(char[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zeros(short[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zeros(short[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zeros(short[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zeroi(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zeroi(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zeroi(int[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zerol(long[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:j\d+>> LongConstant 0                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zerol(long[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:j\d+>> LongConstant 0                       loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zerol(long[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zerof(float[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:f\d+>> FloatConstant 0                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zerof(float[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:f\d+>> FloatConstant 0                      loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zerof(float[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  /// CHECK-START: void Main.zerod(double[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:d\d+>> DoubleConstant 0                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Zero>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.zerod(double[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:d\d+>> DoubleConstant 0                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Zero>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Repl>>] loop:<<Loop>>      outer_loop:none
  private static void zerod(double[] x) {
    for (int i = 0; i < x.length; i++) {
      x[i] = 0;
    }
  }

  public static void main(String[] args) {
    int total = 1111;

    boolean[] xz = new boolean[total];
    byte[]    xb = new byte[total];
    char[]    xc = new char[total];
    short[]   xs = new short[total];
    int[]     xi = new int[total];
    long[]    xl = new long[total];
    float[]   xf = new float[total];
    double[]  xd = new double[total];

    for (int i = 0; i < total; i++) {
      xz[i] = true;
      xb[i] = 1;
      xc[i] = 1;
      xs[i] = 1;
      xi[i] = 1;
      xl[i] = 1;
      xf[i] = 1;
      xd[i] = 1;
    }

    for (int i = 0; i < total; i++) {
      expectEquals(true, xz[i]);
      expectEquals(1, xb[i]);
      expectEquals(1, xc[i]);
      expectEquals(1, xs[i]);
      expectEquals(1, xi[i]);
      expectEquals(1, xl[i]);
      expectEquals(1, xf[i]);
      expectEquals(1, xd[i]);
    }

    zeroz(xz);
    zerob(xb);
    zeroc(xc);
    zeros(xs);
    zeroi(xi);
    zerol(xl);
    zerof(xf);
    zerod(xd);

    for (int i = 0; i < total; i++) {
      expectEquals(false, xz[i]);
      expectEquals(0, xb[i]);
      expectEquals(0, xc[i]);
      expectEquals(0, xs[i]);
      expectEquals(0, xi[i]);
      expectEquals(0, xl[i]);
      expectEquals(0, xf[i]);
      expectEquals(0, xd[i]);
    }

    System.out.println("passed");
  }

  private static void expectEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
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

  private static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
