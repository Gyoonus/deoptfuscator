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

// Test for the optimizing compiler's parallel swap support in
// the presence of register pairs (in this case, doubles on ARM).
public class Main {
  public static void main(String[] args) {
    new Main().foo();
  }

  public void foo() {
    // Do multiple calls to force swapping of registers. Note that
    // this depends on the calling convention, as a stack-only convention
    // may not need the swapping.
    callWithDoubles(a, b, c, d, e, f, g);
    callWithDoubles(b, c, d, e, f, g, a);
    callWithDoubles(c, d, e, f, g, a, b);
    callWithDoubles(d, e, f, g, a, b, c);
  }

  public static void callWithDoubles(
      double a, double b, double c, double d, double e, double f, double g) {
    System.out.println(a - b - c - d - e - f - g);
  }

  double a = 1.0;
  double b = 2.0;
  double c = 3.0;
  double d = 4.0;
  double e = 5.0;
  double f = 6.0;
  double g = 7.0;
}
