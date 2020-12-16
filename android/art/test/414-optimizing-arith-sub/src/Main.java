/*
 * Copyright (C) 2014 The Android Open Source Project
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

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.
public class Main {

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectApproxEquals(float a, float b) {
    float maxDelta = 0.0001F;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta + " " + (a - b));
    }
  }

  public static void expectApproxEquals(double a, double b) {
    double maxDelta = 0.00001D;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b + ", with delta: " + maxDelta + " " + (a - b));
    }
  }

  public static void expectNaN(float a) {
    if (a == a) {
      throw new Error("Expected NaN: " + a);
    }
  }

  public static void expectNaN(double a) {
    if (a == a) {
      throw new Error("Expected NaN: " + a);
    }
  }

  public static void main(String[] args) {
    subInt();
    subLong();
    subFloat();
    subDouble();
  }

  private static void subInt() {
    expectEquals(2, $opt$Sub(5, 3));
    expectEquals(0, $opt$Sub(0, 0));
    expectEquals(-3, $opt$Sub(0, 3));
    expectEquals(3, $opt$Sub(3, 0));
    expectEquals(4, $opt$Sub(1, -3));
    expectEquals(-9, $opt$Sub(-12, -3));
    expectEquals(134217724, $opt$Sub(134217729, 5)); // (2^27 + 1) - 5
  }

  private static void subLong() {
    expectEquals(2L, $opt$Sub(5L, 3L));
    expectEquals(0L, $opt$Sub(0L, 0L));
    expectEquals(-3L, $opt$Sub(0L, 3L));
    expectEquals(3L, $opt$Sub(3L, 0L));
    expectEquals(4L, $opt$Sub(1L, -3L));
    expectEquals(-9L, $opt$Sub(-12L, -3L));
    expectEquals(134217724L, $opt$Sub(134217729L, 5L)); // (2^27 + 1) - 5
    expectEquals(34359738362L, $opt$Sub(34359738369L, 7L)); // (2^35 + 1) - 7
  }

  private static void subFloat() {
    expectApproxEquals(2F, $opt$Sub(5F, 3F));
    expectApproxEquals(0F, $opt$Sub(0F, 0F));
    expectApproxEquals(-3F, $opt$Sub(0F, 3F));
    expectApproxEquals(3F, $opt$Sub(3F, 0F));
    expectApproxEquals(4F, $opt$Sub(1F, -3F));
    expectApproxEquals(-9F, $opt$Sub(-12F, -3F));
    expectApproxEquals(34359738362F, $opt$Sub(34359738369F, 7F)); // (2^35 + 1) - 7
    expectApproxEquals(-0.1F, $opt$Sub(0.1F, 0.2F));
    expectApproxEquals(0.2F, $opt$Sub(-0.5F, -0.7F));

    expectNaN($opt$Sub(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Sub(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Sub(Float.NaN, 11F));
    expectNaN($opt$Sub(Float.NaN, -11F));
    expectNaN($opt$Sub(Float.NaN, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Sub(Float.NaN, Float.POSITIVE_INFINITY));

    expectEquals(Float.NEGATIVE_INFINITY, $opt$Sub(-Float.MAX_VALUE, Float.MAX_VALUE));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Sub(2F, Float.POSITIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$Sub(Float.MAX_VALUE, -Float.MAX_VALUE));
    expectEquals(Float.POSITIVE_INFINITY, $opt$Sub(2F, Float.NEGATIVE_INFINITY));
    expectEquals(Float.POSITIVE_INFINITY, $opt$Sub(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Sub(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY));
  }

  private static void subDouble() {
    expectApproxEquals(2D, $opt$Sub(5D, 3D));
    expectApproxEquals(0D, $opt$Sub(0D, 0D));
    expectApproxEquals(-3D, $opt$Sub(0D, 3D));
    expectApproxEquals(3D, $opt$Sub(3D, 0D));
    expectApproxEquals(4D, $opt$Sub(1D, -3D));
    expectApproxEquals(-9D, $opt$Sub(-12D, -3D));
    expectApproxEquals(134217724D, $opt$Sub(134217729D, 5D)); // (2^27 + 1) - 5
    expectApproxEquals(34359738362D, $opt$Sub(34359738369D, 7D)); // (2^35 + 1) - 7
    expectApproxEquals(-0.1D, $opt$Sub(0.1D, 0.2D));
    expectApproxEquals(0.2D, $opt$Sub(-0.5D, -0.7D));

    expectNaN($opt$Sub(Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectNaN($opt$Sub(Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY));
    expectNaN($opt$Sub(Double.NaN, 11D));
    expectNaN($opt$Sub(Double.NaN, -11D));
    expectNaN($opt$Sub(Double.NaN, Double.NEGATIVE_INFINITY));
    expectNaN($opt$Sub(Double.NaN, Double.POSITIVE_INFINITY));

    expectEquals(Double.NEGATIVE_INFINITY, $opt$Sub(-Double.MAX_VALUE, Double.MAX_VALUE));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$Sub(2D, Double.POSITIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$Sub(Double.MAX_VALUE, -Double.MAX_VALUE));
    expectEquals(Double.POSITIVE_INFINITY, $opt$Sub(2D, Double.NEGATIVE_INFINITY));
    expectEquals(Double.POSITIVE_INFINITY, $opt$Sub(Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectEquals(Double.NEGATIVE_INFINITY, $opt$Sub(Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY));
  }

  static int $opt$Sub(int a, int b) {
    return a - b;
  }

  static long $opt$Sub(long a, long b) {
    return a - b;
  }

  static float $opt$Sub(float a, float b) {
    return a - b;
  }

  static double $opt$Sub(double a, double b) {
    return a - b;
  }

}
