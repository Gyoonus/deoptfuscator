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

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.

public class Main {
  public static void main(String[] args) {
      System.out.println($opt$registerConflictTest());
  }

  static double $opt$registerConflictTest() {
      double a = 0;
      double b = 0;
      double d0 = 0, d1 = 0, d2 = 0, d3 = 0, d4 = 0, d5 = 0, d6 = 0, d7 = 0;
      double d8 = 0, d9 = 0, d10 = 0, d11 = 0, d12 = 0, d13 = 0, d14 = 0, d15 = 0;
      double d16 = 0, d17 = 0, d18 = 0, d19 = 0, d20 = 0, d21 = 0, d22 = 0, d23 = 0;
      double d24 = 0, d25 = 0, d26 = 0, d27 = 0, d28 = 0, d29 = 0, d30 = 0, d31 = 0;
      while (a == b) {
          d0 = a;
          d1 = d0 + 1;
          d2 = d1 + 1;
          d3 = d2 + 1;
          d4 = d3 + 1;
          d5 = d4 + 1;
          d6 = d5 + 1;
          d7 = d6 + 1;
          d8 = d7 + 1;
          d9 = d8 + 1;
          d10 = d9 + 1;
          d11 = d10 + 1;
          d12 = d11 + 1;
          d13 = d12 + 1;
          d14 = d13 + 1;
          d15 = d14 + 1;
          d16 = d15 + 1;
          d17 = d16 + 1;
          d18 = d17 + 1;
          d19 = d18 + 1;
          d20 = d19 + 1;
          d21 = d20 + 1;
          d22 = d21 + 1;
          d23 = d22 + 1;
          d24 = d23 + 1;
          d25 = d24 + 1;
          d26 = d25 + 1;
          d27 = d26 + 1;
          d28 = d27 + 1;
          d29 = d28 + 1;
          d30 = d29 + 1;
          d31 = d30 + 1;
          a = 1;
          b = d31;
      }
      return d0 + d1 + d2 + d3 + d4 + d5 + d6 + d7
              + d8 + d9 + d10 + d11 + d12 + d13 + d14 + d15
              + d16 + d17 + d18 + d19 + d20 + d21 + d22 + d23
              + d24 + d25 + d26 + d27 + d28 + d29 + d30 + d31;
  }
}
