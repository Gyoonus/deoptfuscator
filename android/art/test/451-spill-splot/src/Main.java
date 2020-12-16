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

public class Main {
  public static void main(String[] args) {
    // Create a few local variables to make sure some get spilled, and we get
    // a conflict of swapping a single entry stack slot (float) with a double entry
    // stack slot (double).
    double a = 0.0;
    double b = 1.0;
    double c = 2.0;
    double d = 3.0;
    double e = 4.0;
    double f = 5.0;
    double g = 6.0;
    double h = 7.0;
    double i = 8.0;
    double j = 9.0;

    float aa = 0;
    float bb = 1;
    float cc = 2;
    float dd = 3;
    float ee = 4;
    float ff = 5;
    float gg = 6;
    float hh = 7;
    float ii = 8;
    float jj = 9;
    float kk = 10;
    float ll = 10;
    float mm = 10;
    float nn = 10;

    for (int count = 0; count < 2; count++) {
      System.out.println(aa + bb + cc + dd + ee + ff + gg + hh + ii + jj + kk + ll + mm + nn);
      System.out.println(a + b + c + d + e + f + g + h + i + j);
      a = $noinline$computeDouble();
      b = $noinline$computeDouble();
      c = $noinline$computeDouble();
      d = $noinline$computeDouble();
      e = $noinline$computeDouble();
      f = $noinline$computeDouble();
      g = $noinline$computeDouble();
      h = $noinline$computeDouble();
      i = $noinline$computeDouble();
      j = $noinline$computeDouble();
      System.out.println(a + b + c + d + e + f + g + h + i + j);
      aa = $noinline$computeFloat();
      bb = $noinline$computeFloat();
      cc = $noinline$computeFloat();
      dd = $noinline$computeFloat();
      ee = $noinline$computeFloat();
      ff = $noinline$computeFloat();
      gg = $noinline$computeFloat();
      hh = $noinline$computeFloat();
      ii = $noinline$computeFloat();
      jj = $noinline$computeFloat();
      kk = $noinline$computeFloat();
      ll = $noinline$computeFloat();
      mm = $noinline$computeFloat();
      nn = $noinline$computeFloat();
    }
  }

  static boolean doThrow = false;

  public static double $noinline$computeDouble() {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return 2.0;
  }

  public static float $noinline$computeFloat() {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }
    return 4.0f;
  }
}
