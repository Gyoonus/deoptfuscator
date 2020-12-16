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
  public static int[] bar(int[] a) {
    a[0] = 0;
    a[1] = 0;
    a[2] = 0;
    // Up to this point, we record that the lower bound (inclusive) is 3.
    // The next instruction will record that the lower bound is 5.
    // The deoptimization code used to assume the lower bound has
    // to be the one it will add for the deoptimization check (here, it
    // would be 3).
    return new int[a.length - 5];
  }

  public static int[] foo(int[] a) {
    a[0] = 0;
    a[1] = 0;
    a[2] = 0;
    // Up to this point, we record that the lower bound (inclusive) is 3.
    // The next instruction will record that the lower bound is 1.
    // The deoptimization code used to assume the lower bound has
    // to be the one it will add for the deoptimization check (here, it
    // would be 3).
    return new int[a.length - 1];
  }

  public static void main(String[] args) {
    int[] a = new int[5];
    int[] result = bar(a);
    if (result.length != 0) {
      throw new Error("Expected 0, got " + result.length);
    }

    result = foo(a);
    if (result.length != 4) {
      throw new Error("Expected 5, got " + result.length);
    }
  }
}
