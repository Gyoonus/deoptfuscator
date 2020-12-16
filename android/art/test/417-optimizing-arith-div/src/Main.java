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
    float maxDelta = 0.00001F;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: " + b
          + ", with delta: " + maxDelta + " " + (a - b));
    }
  }

  public static void expectApproxEquals(double a, double b) {
    double maxDelta = 0.00001D;
    boolean aproxEquals = (a > b) ? ((a - b) < maxDelta) : ((b - a) < maxDelta);
    if (!aproxEquals) {
      throw new Error("Expected: " + a + ", found: "
          + b + ", with delta: " + maxDelta + " " + (a - b));
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

  public static void expectDivisionByZero(int value) {
    try {
      $opt$Div(value, 0);
      throw new Error("Expected RuntimeException when dividing by 0");
    } catch (java.lang.RuntimeException e) {
    }
    try {
      $opt$DivZero(value);
      throw new Error("Expected RuntimeException when dividing by 0");
    } catch (java.lang.RuntimeException e) {
    }
  }

  public static void expectDivisionByZero(long value) {
    try {
      $opt$Div(value, 0L);
      throw new Error("Expected RuntimeException when dividing by 0");
    } catch (java.lang.RuntimeException e) {
    }
    try {
      $opt$DivZero(value);
      throw new Error("Expected RuntimeException when dividing by 0");
    } catch (java.lang.RuntimeException e) {
    }
  }

  public static void main(String[] args) {
    div();
  }

  public static void div() {
    divInt();
    divLong();
    divFloat();
    divDouble();
  }

  private static void divInt() {
    expectEquals(2, $opt$DivConst(6));
    expectEquals(2, $opt$Div(6, 3));
    expectEquals(6, $opt$Div(6, 1));
    expectEquals(-2, $opt$Div(6, -3));
    expectEquals(1, $opt$Div(4, 3));
    expectEquals(-1, $opt$Div(4, -3));
    expectEquals(5, $opt$Div(23, 4));
    expectEquals(-5, $opt$Div(-23, 4));

    expectEquals(-Integer.MAX_VALUE, $opt$Div(Integer.MAX_VALUE, -1));
    expectEquals(Integer.MIN_VALUE, $opt$Div(Integer.MIN_VALUE, -1)); // overflow
    expectEquals(-1073741824, $opt$Div(Integer.MIN_VALUE, 2));

    expectEquals(0, $opt$Div(0, Integer.MAX_VALUE));
    expectEquals(0, $opt$Div(0, Integer.MIN_VALUE));

    expectDivisionByZero(0);
    expectDivisionByZero(1);
    expectDivisionByZero(Integer.MAX_VALUE);
    expectDivisionByZero(Integer.MIN_VALUE);
  }

  private static void divLong() {
    expectEquals(2L, $opt$DivConst(6L));
    expectEquals(2L, $opt$Div(6L, 3L));
    expectEquals(6L, $opt$Div(6L, 1L));
    expectEquals(-2L, $opt$Div(6L, -3L));
    expectEquals(1L, $opt$Div(4L, 3L));
    expectEquals(-1L, $opt$Div(4L, -3L));
    expectEquals(5L, $opt$Div(23L, 4L));
    expectEquals(-5L, $opt$Div(-23L, 4L));

    expectEquals(-Integer.MAX_VALUE, $opt$Div(Integer.MAX_VALUE, -1L));
    expectEquals(2147483648L, $opt$Div(Integer.MIN_VALUE, -1L));
    expectEquals(-1073741824L, $opt$Div(Integer.MIN_VALUE, 2L));

    expectEquals(-Long.MAX_VALUE, $opt$Div(Long.MAX_VALUE, -1L));
    expectEquals(Long.MIN_VALUE, $opt$Div(Long.MIN_VALUE, -1L)); // overflow

    expectEquals(11111111111111L, $opt$Div(33333333333333L, 3L));
    expectEquals(3L, $opt$Div(33333333333333L, 11111111111111L));

    expectEquals(0L, $opt$Div(0L, Long.MAX_VALUE));
    expectEquals(0L, $opt$Div(0L, Long.MIN_VALUE));

    expectDivisionByZero(0L);
    expectDivisionByZero(1L);
    expectDivisionByZero(Long.MAX_VALUE);
    expectDivisionByZero(Long.MIN_VALUE);
  }

  private static void divFloat() {
    expectApproxEquals(1.6666666F, $opt$Div(5F, 3F));
    expectApproxEquals(0F, $opt$Div(0F, 3F));
    expectApproxEquals(-0.3333333F, $opt$Div(1F, -3F));
    expectApproxEquals(4F, $opt$Div(-12F, -3F));
    expectApproxEquals(0.5, $opt$Div(0.1F, 0.2F));
    expectApproxEquals(-2.5F, $opt$Div(-0.5F, 0.2F));

    expectEquals(0F, $opt$Div(0F, Float.POSITIVE_INFINITY));
    expectEquals(0F, $opt$Div(11F, Float.POSITIVE_INFINITY));
    expectEquals(0F, $opt$Div(0F, Float.NEGATIVE_INFINITY));
    expectEquals(0F, $opt$Div(11F, Float.NEGATIVE_INFINITY));

    expectNaN($opt$Div(0F, 0F));
    expectNaN($opt$Div(Float.NaN, 11F));
    expectNaN($opt$Div(-11F, Float.NaN));
    expectNaN($opt$Div(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Div(Float.NaN, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.NaN));

    expectEquals(Float.POSITIVE_INFINITY, $opt$Div(3F, 0F));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Div(-3F, 0F));
    expectEquals(Float.POSITIVE_INFINITY, $opt$Div(Float.MAX_VALUE, Float.MIN_VALUE));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Div(-Float.MAX_VALUE, Float.MIN_VALUE));
  }

  private static void divDouble() {
    expectApproxEquals(1.6666666D, $opt$Div(5D, 3D));
    expectApproxEquals(0D, $opt$Div(0D, 3D));
    expectApproxEquals(-0.3333333D, $opt$Div(1D, -3D));
    expectApproxEquals(4D, $opt$Div(-12D, -3D));
    expectApproxEquals(0.5, $opt$Div(0.1D, 0.2D));
    expectApproxEquals(-2.5D, $opt$Div(-0.5D, 0.2D));

    expectEquals(0D, $opt$Div(0D, Float.POSITIVE_INFINITY));
    expectEquals(0D, $opt$Div(11D, Float.POSITIVE_INFINITY));
    expectEquals(0D, $opt$Div(0D, Float.NEGATIVE_INFINITY));
    expectEquals(0D, $opt$Div(11D, Float.NEGATIVE_INFINITY));

    expectNaN($opt$Div(0D, 0D));
    expectNaN($opt$Div(Float.NaN, 11D));
    expectNaN($opt$Div(-11D, Float.NaN));
    expectNaN($opt$Div(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Div(Float.NaN, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Div(Float.POSITIVE_INFINITY, Float.NaN));

    expectEquals(Float.POSITIVE_INFINITY, $opt$Div(3D, 0D));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Div(-3D, 0D));
    expectEquals(Float.POSITIVE_INFINITY, $opt$Div(Float.MAX_VALUE, Float.MIN_VALUE));
    expectEquals(Float.NEGATIVE_INFINITY, $opt$Div(-Float.MAX_VALUE, Float.MIN_VALUE));
  }

  static int $opt$Div(int a, int b) {
    return a / b;
  }

  static int $opt$DivZero(int a) {
    return a / 0;
  }

  // Division by literals != 0 should not generate checks.
  static int $opt$DivConst(int a) {
    return a / 3;
  }

  static long $opt$DivConst(long a) {
    return a / 3L;
  }

  static long $opt$Div(long a, long b) {
    return a / b;
  }

  static long $opt$DivZero(long a) {
    return a / 0L;
  }

  static float $opt$Div(float a, float b) {
    return a / b;
  }

  static double $opt$Div(double a, double b) {
    return a / b;
  }
}
