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

class Main {
  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    Main m = new Main();

    m.$noinline$SetInstanceField();
    assertIntEquals(123456, m.i);

    $noinline$SetStaticField();
    assertIntEquals(456789, s);
  }

  private static boolean doThrow = false;

  private void $noinline$SetInstanceField() {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }

    // Set a value than does not fit in a 16-bit (signed) integer.
    i = 123456;
  }

  private static void $noinline$SetStaticField() {
    if (doThrow) {
      // Try defeating inlining.
      throw new Error();
    }

    // Set a value than does not fit in a 16-bit (signed) integer.
    s = 456789;
  }

  private int i = 0;
  private static int s = 0;
}
