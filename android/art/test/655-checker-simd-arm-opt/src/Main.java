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
 * Checker test for arm and arm64 simd optimizations.
 */
public class Main {

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START-ARM64: void Main.encodableConstants(byte[], short[], char[], int[], long[], float[], double[]) disassembly (after)
  /// CHECK-DAG: <<C1:i\d+>>   IntConstant 1
  /// CHECK-DAG: <<C2:i\d+>>   IntConstant 2
  /// CHECK-DAG: <<C3:i\d+>>   IntConstant 3
  /// CHECK-DAG: <<C4:i\d+>>   IntConstant 4
  /// CHECK-DAG: <<L5:j\d+>>   LongConstant 5
  /// CHECK-DAG: <<F2:f\d+>>   FloatConstant 2
  /// CHECK-DAG: <<D20:d\d+>>  DoubleConstant 20
  //
  /// CHECK-DAG:               VecReplicateScalar [<<C1>>]
  /// CHECK-DAG:               movi v{{[0-9]+}}.16b, #0x1
  /// CHECK-DAG:               VecReplicateScalar [<<C2>>]
  /// CHECK-DAG:               movi v{{[0-9]+}}.8h, #0x2, lsl #0
  /// CHECK-DAG:               VecReplicateScalar [<<C3>>]
  /// CHECK-DAG:               movi v{{[0-9]+}}.8h, #0x3, lsl #0
  /// CHECK-DAG:               VecReplicateScalar [<<C4>>]
  /// CHECK-DAG:               movi v{{[0-9]+}}.4s, #0x4, lsl #0
  /// CHECK-DAG:               VecReplicateScalar [<<L5>>]
  /// CHECK-DAG:               dup v{{[0-9]+}}.2d, x{{[0-9]+}}
  /// CHECK-DAG:               VecReplicateScalar [<<F2>>]
  /// CHECK-DAG:               fmov v{{[0-9]+}}.4s, #0x0
  /// CHECK-DAG:               VecReplicateScalar [<<D20>>]
  /// CHECK-DAG:               fmov v{{[0-9]+}}.2d, #0x34
  private static void encodableConstants(byte[] b, short[] s, char[] c, int[] a, long[] l, float[] f, double[] d) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
      b[i] += 1;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      s[i] += 2;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      c[i] += 3;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      a[i] += 4;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      l[i] += 5;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      f[i] += 2.0f;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      d[i] += 20.0;
    }
  }

  private static int sumArray(byte[] b, short[] s, char[] c, int[] a, long[] l, float[] f, double[] d) {
    int sum = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
      sum += b[i] + s[i] + c[i] + a[i] + l[i] + f[i] + d[i];
    }
    return sum;
  }

  public static final int ARRAY_SIZE = 100;

  public static void main(String[] args) {
    byte[] b = new byte[ARRAY_SIZE];
    short[] s = new short[ARRAY_SIZE];
    char[] c = new char[ARRAY_SIZE];
    int[] a = new int[ARRAY_SIZE];
    long[] l = new long[ARRAY_SIZE];
    float[] f = new float[ARRAY_SIZE];
    double[] d = new double[ARRAY_SIZE];

    encodableConstants(b, s, c, a, l, f, d);
    expectEquals(3700, sumArray(b, s, c, a, l, f, d));

    System.out.println("passed");
  }
}
