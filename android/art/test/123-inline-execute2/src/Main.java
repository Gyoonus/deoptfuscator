/*
 * Copyright (C) 2015 The Android Open Source Project
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

import java.util.Locale;

public class Main {
  public static void main(String args[]) {
    for (int i = 0; i <= 360; i += 45) {
      double d = i * (Math.PI / 180.0);
      System.out.println("Math.sin(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.sin(d)));

      System.out.println("Math.sinh(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.sinh(d)));
      System.out.println("Math.asin(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.asin(d)));
      System.out.println("Math.cos(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.cos(d)));
      System.out.println("Math.cosh(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.cosh(d)));
      System.out.println("Math.acos(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.acos(d)));
      if ((i + 90) % 180 != 0) {
        System.out.println("Math.tan(" + d + ") = "
            + String.format(Locale.US, "%.12f", Math.tan(d)));
      }
      System.out.println("Math.tanh(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.tanh(d)));
      System.out.println("Math.atan(" + d + ") = "
          + String.format(Locale.US, "%.12f", Math.atan(d)));
      System.out.println("Math.atan2(" + d + ", " + (d + 1.0) + ") = "
          + String.format(Locale.US, "%.12f", Math.atan2(d, d + 1.0)));
    }

    for (int j = -3; j <= 3; j++) {
      double e = (double) j;
      System.out.println("Math.cbrt(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.cbrt(e)));
      System.out.println("Math.log(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.log(e)));
      System.out.println("Math.log10(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.log10(e)));
      System.out.println("Math.log1p(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.log1p(e)));
      System.out.println("Math.exp(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.exp(e)));
      System.out.println("Math.expm1(" + e + ") = "
          + String.format(Locale.US, "%.12f", Math.expm1(e)));
      System.out.println("Math.pow(" + e + ", " + (e + 1.0) + ") = "
          + String.format(Locale.US, "%.12f", Math.pow(e, e + 1.0)));
      System.out.println("Math.hypot(" + e + ", " + (e + 1.0) + ") = "
          + String.format(Locale.US, "%.12f", Math.hypot(e, e + 1.0)));
    }

    System.out.println("Math.ceil(0.0001) = "
        + String.format(Locale.US, "%.12f", Math.ceil(0.0001)));
    System.out.println("Math.floor(0.0001) = "
        + String.format(Locale.US, "%.12f", Math.floor(0.0001)));
    System.out.println("Math.nextAfter(1.0, 2.0) = "
        + String.format(Locale.US, "%.12f", Math.nextAfter(1.0, 2.0)));
    System.out.println("Math.nextAfter(2.0, 1.0) = "
        + String.format(Locale.US, "%.12f", Math.nextAfter(2.0, 1.0)));
    System.out.println("Math.rint(0.5000001) = "
        + String.format(Locale.US, "%.12f", Math.rint(0.5000001)));

    for (int i = 0; i <= 360; i += 45) {
      double d = i * (StrictMath.PI / 180.0);
      System.out.println("StrictMath.sin(" + d + ") = " + StrictMath.sin(d));
      System.out.println("StrictMath.sinh(" + d + ") = " + StrictMath.sinh(d));
      System.out.println("StrictMath.asin(" + d + ") = " + StrictMath.asin(d));
      System.out.println("StrictMath.cos(" + d + ") = " + StrictMath.cos(d));
      System.out.println("StrictMath.cosh(" + d + ") = " + StrictMath.cosh(d));
      System.out.println("StrictMath.acos(" + d + ") = " + StrictMath.acos(d));
      System.out.println("StrictMath.tan(" + d + ") = " + StrictMath.tan(d));
      System.out.println("StrictMath.tanh(" + d + ") = " + StrictMath.tanh(d));
      System.out.println("StrictMath.atan(" + d + ") = " + StrictMath.atan(d));
      System.out.println("StrictMath.atan2(" + d + ", " + (d + 1.0) + ") = "
          + StrictMath.atan2(d, d + 1.0));
    }

    for (int j = -3; j <= 3; j++) {
      double e = (double) j;
      System.out.println("StrictMath.cbrt(" + e + ") = " + StrictMath.cbrt(e));
      System.out.println("StrictMath.log(" + e + ") = " + StrictMath.log(e));
      System.out.println("StrictMath.log10(" + e + ") = " + StrictMath.log10(e));
      System.out.println("StrictMath.log1p(" + e + ") = " + StrictMath.log1p(e));
      System.out.println("StrictMath.exp(" + e + ") = " + StrictMath.exp(e));
      System.out.println("StrictMath.expm1(" + e + ") = " + StrictMath.expm1(e));
      System.out.println("StrictMath.pow(" + e + ", " + (e + 1.0) + ") = "
          + StrictMath.pow(e, e + 1.0));
      System.out.println("StrictMath.hypot(" + e + ", " + (e + 1.0) + ") = "
          + StrictMath.hypot(e, e + 1.0));
    }

    System.out.println("StrictMath.ceil(0.0001) = " + StrictMath.ceil(0.0001));
    System.out.println("StrictMath.floor(0.0001) = " + StrictMath.floor(0.0001));
    System.out.println("StrictMath.nextAfter(1.0, 2.0) = " + StrictMath.nextAfter(1.0, 2.0));
    System.out.println("StrictMath.rint(0.5000001) = " + StrictMath.rint(0.5000001));
  }

}
