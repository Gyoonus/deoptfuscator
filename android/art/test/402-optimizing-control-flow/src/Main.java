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

  public static void expectEquals(int expected, int value) {
    if (expected != value) {
      throw new Error("Expected: " + expected + ", found: " + value);
    }
  }

  public static void main(String[] args) {
    int result = $opt$testIfEq1(42);
    expectEquals(42, result);

    result = $opt$testIfEq2(42);
    expectEquals(7, result);

    result = $opt$testWhileLoop(42);
    expectEquals(45, result);

    result = $opt$testDoWhileLoop(42);
    expectEquals(45, result);

    result = $opt$testForLoop(42);
    expectEquals(44, result);

    result = $opt$testIfWithLocal(5);
    expectEquals(7, result);
  }

  static int $opt$testIfEq1(int a) {
    if (a + 1 == 43) {
      return 42;
    } else {
      return 7;
    }
  }

  static int $opt$testIfEq2(int a) {
    if (a + 1 == 41) {
      return 42;
    } else {
      return 7;
    }
  }

  static int $opt$testWhileLoop(int a) {
    while (a++ != 44) {}
    return a;
  }

  static int $opt$testDoWhileLoop(int a) {
    do {
    } while (a++ != 44);
    return a;
  }

  static int $opt$testForLoop(int a) {
    for (; a != 44; a++) {}
    return a;
  }

  static int $opt$testIfWithLocal(int a) {
    if (a == 5) {
      int f = 2;
      a += f;
    }
    // The SSA builder should not create a phi for f.

    return a;
  }
}
