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

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static int Inline(int x, int y) {
    int result;
    if (x <= y) {
      result = x * y;
    } else {
      result = 0;
    }
    return result;
  }

  /// CHECK-START: int Main.NestedLoop(int, int) inliner (before)
  /// CHECK-NOT:     Mul

  /// CHECK-START: int Main.NestedLoop(int, int) inliner (after)
  /// CHECK:         Mul
  /// CHECK-NOT:     Mul

  public static int NestedLoop(int max_x, int max_y) {
    int total = 0;
    for (int x = 0; x < max_x; ++x) {
      for (int y = 0; y < max_y; ++y) {
        total += Inline(x, y);
      }
    }
    return total;
  }

  public static void main(String[] args) {
    assertIntEquals(0, NestedLoop(1, 1));
    assertIntEquals(3, NestedLoop(2, 3));
  }
}
