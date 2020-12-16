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

public class Main {

  public static void main(String[] args) {
    remFloat();
    remDouble();
  }

  private static void remFloat() {
    expectApproxEquals(1.98F, $opt$Rem(1.98F, 2F));
    expectApproxEquals(0F, $opt$Rem(2F, 0.5F));
    expectApproxEquals(0.09999F, $opt$Rem(1.0F, 0.1F));
    expectApproxEquals(1.9F, $opt$Rem(6.5F, 2.3F));
    expectApproxEquals(0.48F, $opt$Rem(1.98F, 1.5F));
    expectApproxEquals(0.9999F, $opt$Rem(0.9999F, 1.222F));
    expectApproxEquals(0.9999F, $opt$Rem(0.9999F, 1.0001F));
    expectApproxEquals(-1.98F, $opt$Rem(-1.98F, 2F));
    expectApproxEquals(-0F, $opt$Rem(-2F, 0.5F));
    expectApproxEquals(-0.09999F, $opt$Rem(-1.0F, 0.1F));
    expectApproxEquals(-1.9F, $opt$Rem(-6.5F, 2.3F));
    expectApproxEquals(-0.48F, $opt$Rem(-1.98F, 1.5F));
    expectApproxEquals(-0.9999F, $opt$Rem(-0.9999F, 1.222F));
    expectApproxEquals(-0.9999F, $opt$Rem(-0.9999F, 1.0001F));
    expectApproxEquals(1.98F, $opt$Rem(1.98F, -2F));
    expectApproxEquals(0F, $opt$Rem(2F, -0.5F));
    expectApproxEquals(0.09999F, $opt$Rem(1.0F, -0.1F));
    expectApproxEquals(1.9F, $opt$Rem(6.5F, -2.3F));
    expectApproxEquals(0.48F, $opt$Rem(1.98F, -1.5F));
    expectApproxEquals(0.9999F, $opt$Rem(0.9999F, -1.222F));
    expectApproxEquals(0.9999F, $opt$Rem(0.9999F, -1.0001F));
    expectApproxEquals(-1.98F, $opt$Rem(-1.98F, -2F));
    expectApproxEquals(-0F, $opt$Rem(-2F, -0.5F));
    expectApproxEquals(-0.09999F, $opt$Rem(-1.0F, -0.1F));
    expectApproxEquals(-1.9F, $opt$Rem(-6.5F, -2.3F));
    expectApproxEquals(-0.48F, $opt$Rem(-1.98F, -1.5F));
    expectApproxEquals(-0.9999F, $opt$Rem(-0.9999F, -1.222F));
    expectApproxEquals(-0.9999F, $opt$Rem(-0.9999F, -1.0001F));

    expectApproxEquals(1.68267e-18F, $opt$Rem(61615.2F, -2.48699e-17F));
    expectApproxEquals(-8.63819e-09F, $opt$Rem(-1.73479e+14F, 3.11154e-08F));
    expectApproxEquals(1.10911e-12F, $opt$Rem(338122F, 4.57572e-12F));

    expectApproxEquals(2F, $opt$RemConst(6F));
    expectApproxEquals(2F, $opt$Rem(5.1F, 3.1F));
    expectApproxEquals(2.1F, $opt$Rem(5.1F, 3F));
    expectApproxEquals(-2F, $opt$Rem(-5.1F, 3.1F));
    expectApproxEquals(-2.1F, $opt$Rem(-5.1F, -3F));
    expectApproxEquals(2F, $opt$Rem(6F, 4F));
    expectApproxEquals(2F, $opt$Rem(6F, -4F));
    expectApproxEquals(0F, $opt$Rem(6F, 3F));
    expectApproxEquals(0F, $opt$Rem(6F, -3F));
    expectApproxEquals(0F, $opt$Rem(6F, 1F));
    expectApproxEquals(0F, $opt$Rem(6F, -1F));
    expectApproxEquals(-1F, $opt$Rem(-7F, 3F));
    expectApproxEquals(-1F, $opt$Rem(-7F, -3F));
    expectApproxEquals(0F, $opt$Rem(6F, 6F));
    expectApproxEquals(0F, $opt$Rem(-6F, -6F));
    expectApproxEquals(7F, $opt$Rem(7F, 9F));
    expectApproxEquals(7F, $opt$Rem(7F, -9F));
    expectApproxEquals(-7F, $opt$Rem(-7F, 9F));
    expectApproxEquals(-7F, $opt$Rem(-7F, -9F));
    expectApproxEquals(0F, $opt$Rem(Float.MAX_VALUE, 1F));
    expectApproxEquals(0F, $opt$Rem(Float.MAX_VALUE, -1F));
    expectApproxEquals(0F, $opt$Rem(Float.MIN_VALUE, 1F));
    expectApproxEquals(0F, $opt$Rem(Float.MIN_VALUE, -1F));
    expectApproxEquals(0F, $opt$Rem(0F, 7F));
    expectApproxEquals(0F, $opt$Rem(0F, Float.MAX_VALUE));
    expectApproxEquals(0F, $opt$Rem(0F, Float.MIN_VALUE));
    expectApproxEquals(0F, $opt$Rem(0F, Float.POSITIVE_INFINITY));
    expectApproxEquals(0F, $opt$Rem(0F, Float.NEGATIVE_INFINITY));
    expectApproxEquals(4F, $opt$Rem(4F, Float.POSITIVE_INFINITY));
    expectApproxEquals(4F, $opt$Rem(4F, Float.NEGATIVE_INFINITY));
    expectApproxEquals(-4F, $opt$Rem(-4F, Float.POSITIVE_INFINITY));
    expectApproxEquals(-4F, $opt$Rem(-4F, Float.NEGATIVE_INFINITY));
    expectApproxEquals(0F, $opt$Rem(Float.MIN_NORMAL, Float.MIN_VALUE));
    expectApproxEquals(0F, $opt$Rem(Float.MIN_NORMAL, Float.MIN_NORMAL));
    expectApproxEquals(0F, $opt$Rem(Float.MIN_VALUE, Float.MIN_VALUE));
    expectApproxEquals(0F, $opt$Rem(Float.MAX_VALUE, Float.MIN_VALUE));
    expectApproxEquals(0F, $opt$Rem(Float.MAX_VALUE, Float.MAX_VALUE));
    expectApproxEquals(0F, $opt$Rem(Float.MAX_VALUE, Float.MIN_NORMAL));
    expectApproxEquals(Float.MIN_NORMAL, $opt$Rem(Float.MIN_NORMAL, Float.MAX_VALUE));
    expectApproxEquals(Float.MIN_NORMAL, $opt$Rem(Float.MIN_NORMAL, Float.NEGATIVE_INFINITY));
    expectApproxEquals(Float.MIN_NORMAL, $opt$Rem(Float.MIN_NORMAL, Float.POSITIVE_INFINITY));
    expectApproxEquals(Float.MIN_VALUE, $opt$Rem(Float.MIN_VALUE, Float.MAX_VALUE));
    expectApproxEquals(Float.MIN_VALUE, $opt$Rem(Float.MIN_VALUE, Float.MIN_NORMAL));
    expectApproxEquals(Float.MIN_VALUE, $opt$Rem(Float.MIN_VALUE, Float.NEGATIVE_INFINITY));
    expectApproxEquals(Float.MIN_VALUE, $opt$Rem(Float.MIN_VALUE, Float.POSITIVE_INFINITY));
    expectApproxEquals(Float.MAX_VALUE, $opt$Rem(Float.MAX_VALUE, Float.NEGATIVE_INFINITY));
    expectApproxEquals(Float.MAX_VALUE, $opt$Rem(Float.MAX_VALUE, Float.POSITIVE_INFINITY));

    expectNaN($opt$Rem(Float.NaN, 3F));
    expectNaN($opt$Rem(3F, Float.NaN));
    expectNaN($opt$Rem(3F, 0F));
    expectNaN($opt$Rem(1F, 0F));
    expectNaN($opt$Rem(-1F, 0F));
    expectNaN($opt$Rem(Float.NEGATIVE_INFINITY, Float.MIN_VALUE));
    expectNaN($opt$Rem(Float.NEGATIVE_INFINITY, Float.MAX_VALUE));
    expectNaN($opt$Rem(Float.NEGATIVE_INFINITY, Float.MIN_NORMAL));
    expectNaN($opt$Rem(Float.NEGATIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Rem(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY));
    expectNaN($opt$Rem(Float.POSITIVE_INFINITY, Float.MIN_VALUE));
    expectNaN($opt$Rem(Float.POSITIVE_INFINITY, Float.MAX_VALUE));
    expectNaN($opt$Rem(Float.POSITIVE_INFINITY, Float.MIN_NORMAL));
    expectNaN($opt$Rem(Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY));
    expectNaN($opt$Rem(Float.POSITIVE_INFINITY, Float.POSITIVE_INFINITY));
  }

  private static void remDouble() {
    expectApproxEquals(1.98D, $opt$Rem(1.98D, 2D));
    expectApproxEquals(0D, $opt$Rem(2D, 0.5D));
    expectApproxEquals(0.09999D, $opt$Rem(1.0D, 0.1D));
    expectApproxEquals(1.9D, $opt$Rem(6.5D, 2.3D));
    expectApproxEquals(0.48D, $opt$Rem(1.98D, 1.5D));
    expectApproxEquals(0.9999D, $opt$Rem(0.9999D, 1.222D));
    expectApproxEquals(0.9999D, $opt$Rem(0.9999D, 1.0001D));
    expectApproxEquals(-1.98D, $opt$Rem(-1.98D, 2D));
    expectApproxEquals(-0D, $opt$Rem(-2D, 0.5D));
    expectApproxEquals(-0.09999D, $opt$Rem(-1.0D, 0.1D));
    expectApproxEquals(-1.9D, $opt$Rem(-6.5D, 2.3D));
    expectApproxEquals(-0.48D, $opt$Rem(-1.98D, 1.5D));
    expectApproxEquals(-0.9999D, $opt$Rem(-0.9999D, 1.222D));
    expectApproxEquals(-0.9999D, $opt$Rem(-0.9999D, 1.0001D));
    expectApproxEquals(1.98D, $opt$Rem(1.98D, -2D));
    expectApproxEquals(0D, $opt$Rem(2D, -0.5D));
    expectApproxEquals(0.09999D, $opt$Rem(1.0D, -0.1D));
    expectApproxEquals(1.9D, $opt$Rem(6.5D, -2.3D));
    expectApproxEquals(0.48D, $opt$Rem(1.98D, -1.5D));
    expectApproxEquals(0.9999D, $opt$Rem(0.9999D, -1.222D));
    expectApproxEquals(0.9999D, $opt$Rem(0.9999D, -1.0001D));
    expectApproxEquals(-1.98D, $opt$Rem(-1.98D, -2D));
    expectApproxEquals(-0D, $opt$Rem(-2D, -0.5D));
    expectApproxEquals(-0.09999D, $opt$Rem(-1.0D, -0.1D));
    expectApproxEquals(-1.9D, $opt$Rem(-6.5D, -2.3D));
    expectApproxEquals(-0.48D, $opt$Rem(-1.98D, -1.5D));
    expectApproxEquals(-0.9999D, $opt$Rem(-0.9999D, -1.222D));
    expectApproxEquals(-0.9999D, $opt$Rem(-0.9999D, -1.0001D));

    expectApproxEquals(2D, $opt$RemConst(6D));
    expectApproxEquals(2D, $opt$Rem(5.1D, 3.1D));
    expectApproxEquals(2.1D, $opt$Rem(5.1D, 3D));
    expectApproxEquals(-2D, $opt$Rem(-5.1D, 3.1D));
    expectApproxEquals(-2.1D, $opt$Rem(-5.1D, -3D));
    expectApproxEquals(2D, $opt$Rem(6D, 4D));
    expectApproxEquals(2D, $opt$Rem(6D, -4D));
    expectApproxEquals(0D, $opt$Rem(6D, 3D));
    expectApproxEquals(0D, $opt$Rem(6D, -3D));
    expectApproxEquals(0D, $opt$Rem(6D, 1D));
    expectApproxEquals(0D, $opt$Rem(6D, -1D));
    expectApproxEquals(-1D, $opt$Rem(-7D, 3D));
    expectApproxEquals(-1D, $opt$Rem(-7D, -3D));
    expectApproxEquals(0D, $opt$Rem(6D, 6D));
    expectApproxEquals(0D, $opt$Rem(-6D, -6D));
    expectApproxEquals(7D, $opt$Rem(7D, 9D));
    expectApproxEquals(7D, $opt$Rem(7D, -9D));
    expectApproxEquals(-7D, $opt$Rem(-7D, 9D));
    expectApproxEquals(-7D, $opt$Rem(-7D, -9D));
    expectApproxEquals(0D, $opt$Rem(Double.MAX_VALUE, 1D));
    expectApproxEquals(0D, $opt$Rem(Double.MAX_VALUE, -1D));
    expectApproxEquals(0D, $opt$Rem(Double.MIN_VALUE, 1D));
    expectApproxEquals(0D, $opt$Rem(Double.MIN_VALUE, -1D));
    expectApproxEquals(0D, $opt$Rem(0D, 7D));
    expectApproxEquals(0D, $opt$Rem(0D, Double.MAX_VALUE));
    expectApproxEquals(0D, $opt$Rem(0D, Double.MIN_VALUE));
    expectApproxEquals(0D, $opt$Rem(0D, Double.POSITIVE_INFINITY));
    expectApproxEquals(0D, $opt$Rem(0D, Double.NEGATIVE_INFINITY));
    expectApproxEquals(4D, $opt$Rem(4D, Double.POSITIVE_INFINITY));
    expectApproxEquals(4D, $opt$Rem(4D, Double.NEGATIVE_INFINITY));
    expectApproxEquals(-4D, $opt$Rem(-4D, Double.POSITIVE_INFINITY));
    expectApproxEquals(-4D, $opt$Rem(-4D, Double.NEGATIVE_INFINITY));
    expectApproxEquals(0D, $opt$Rem(Double.MIN_NORMAL, Double.MIN_VALUE));
    expectApproxEquals(0D, $opt$Rem(Double.MIN_NORMAL, Double.MIN_NORMAL));
    expectApproxEquals(0D, $opt$Rem(Double.MIN_VALUE, Double.MIN_VALUE));
    expectApproxEquals(0D, $opt$Rem(Double.MAX_VALUE, Double.MIN_VALUE));
    expectApproxEquals(0D, $opt$Rem(Double.MAX_VALUE, Double.MAX_VALUE));
    expectApproxEquals(0D, $opt$Rem(Double.MAX_VALUE, Double.MIN_NORMAL));
    expectApproxEquals(Double.MIN_NORMAL, $opt$Rem(Double.MIN_NORMAL, Double.MAX_VALUE));
    expectApproxEquals(Double.MIN_NORMAL, $opt$Rem(Double.MIN_NORMAL, Double.NEGATIVE_INFINITY));
    expectApproxEquals(Double.MIN_NORMAL, $opt$Rem(Double.MIN_NORMAL, Double.POSITIVE_INFINITY));
    expectApproxEquals(Double.MIN_VALUE, $opt$Rem(Double.MIN_VALUE, Double.MAX_VALUE));
    expectApproxEquals(Double.MIN_VALUE, $opt$Rem(Double.MIN_VALUE, Double.MIN_NORMAL));
    expectApproxEquals(Double.MIN_VALUE, $opt$Rem(Double.MIN_VALUE, Double.NEGATIVE_INFINITY));
    expectApproxEquals(Double.MIN_VALUE, $opt$Rem(Double.MIN_VALUE, Double.POSITIVE_INFINITY));
    expectApproxEquals(Double.MAX_VALUE, $opt$Rem(Double.MAX_VALUE, Double.NEGATIVE_INFINITY));
    expectApproxEquals(Double.MAX_VALUE, $opt$Rem(Double.MAX_VALUE, Double.POSITIVE_INFINITY));

    expectNaN($opt$Rem(Double.NaN, 3D));
    expectNaN($opt$Rem(3D, Double.NaN));
    expectNaN($opt$Rem(3D, 0D));
    expectNaN($opt$Rem(1D, 0D));
    expectNaN($opt$Rem(-1D, 0D));
    expectNaN($opt$Rem(Double.NEGATIVE_INFINITY, Double.MIN_VALUE));
    expectNaN($opt$Rem(Double.NEGATIVE_INFINITY, Double.MAX_VALUE));
    expectNaN($opt$Rem(Double.NEGATIVE_INFINITY, Double.MIN_NORMAL));
    expectNaN($opt$Rem(Double.NEGATIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectNaN($opt$Rem(Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY));
    expectNaN($opt$Rem(Double.POSITIVE_INFINITY, Double.MIN_VALUE));
    expectNaN($opt$Rem(Double.POSITIVE_INFINITY, Double.MAX_VALUE));
    expectNaN($opt$Rem(Double.POSITIVE_INFINITY, Double.MIN_NORMAL));
    expectNaN($opt$Rem(Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY));
    expectNaN($opt$Rem(Double.POSITIVE_INFINITY, Double.POSITIVE_INFINITY));
  }

  static float $opt$Rem(float a, float b) {
    return a % b;
  }

 static float $opt$RemConst(float a) {
    return a % 4F;
  }

  static double $opt$Rem(double a, double b) {
    return a % b;
  }

  static double $opt$RemConst(double a) {
    return a % 4D;
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

}
