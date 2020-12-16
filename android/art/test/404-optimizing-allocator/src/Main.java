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

// Note that $opt$reg$ is a marker for the optimizing compiler to test
// it does use its register allocator.

public class Main {
  public static void main(String[] args) {

    expectEquals(4, $opt$reg$TestLostCopy());
    expectEquals(-10, $opt$reg$TestTwoLive());
    expectEquals(-20, $opt$reg$TestThreeLive());
    expectEquals(5, $opt$reg$TestFourLive());
    expectEquals(10, $opt$reg$TestMultipleLive());
    expectEquals(1, $opt$reg$TestWithBreakAndContinue());
    expectEquals(-15, $opt$reg$testSpillInIf(5, 6, 7));
    expectEquals(-567, $opt$reg$TestAgressiveLive1(1, 2, 3, 4, 5, 6, 7));
    expectEquals(-77, $opt$reg$TestAgressiveLive2(1, 2, 3, 4, 5, 6, 7));
  }

  public static int $opt$reg$TestLostCopy() {
    int a = 0;
    int b = 0;
    do {
      b = a;
      a++;
    } while (a != 5);
    return b;
  }

  public static int $opt$reg$TestTwoLive() {
    int a = 0;
    int b = 0;
    do {
      a++;
      b += 3;
    } while (a != 5);
    return a - b;
  }

  public static int $opt$reg$TestThreeLive() {
    int a = 0;
    int b = 0;
    int c = 0;
    do {
      a++;
      b += 3;
      c += 2;
    } while (a != 5);
    return a - b - c;
  }

  public static int $opt$reg$TestFourLive() {
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    do {
      a++;
      b += 3;
      c += 2;
      d++;
    } while (a != 5);
    return d;
  }

  public static int $opt$reg$TestMultipleLive() {
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;
    int f = 0;
    int g = 0;
    do {
      a++;
      b++;
      c++;
      d++;
      e += 3;
      f += 2;
      g += 2;
    } while (a != 5);
    return f;
  }

  public static int $opt$reg$TestWithBreakAndContinue() {
    int a = 0;
    int b = 0;
    do {
      a++;
      if (a == 2) {
        continue;
      }
      b++;
      if (a == 5) {
        break;
      }
    } while (true);
    return a - b;
  }

  public static int $opt$reg$testSpillInIf(int a, int b, int c) {
    int d = 0;
    int e = 0;
    if (a == 5) {
      b++;
      c++;
      d += 2;
      e += 3;
    }

    return a - b - c - d - e;
  }

  public static int $opt$reg$TestAgressiveLive1(int a, int b, int c, int d, int e, int f, int g) {
    int h = a - b;
    int i = c - d;
    int j = e - f;
    int k = 42 + g - a;
    do {
      b++;
      while (k != 1) {
        --k;
        ++i;
        if (i == 9) {
          ++i;
        }
        j += 5;
      }
      k = 9;
      h++;
    } while (h != 5);
    return a - b - c - d - e - f - g - h - i - j - k;
  }

  public static int $opt$reg$TestAgressiveLive2(int a, int b, int c, int d, int e, int f, int g) {
    int h = a - b;
    int i = c - d;
    int j = e - f;
    int k = 42 + g - a;
    do {
      h++;
    } while (h != 5);
    return a - b - c - d - e - f - g - h - i - j - k;
  }

  public static void expectEquals(int expected, int value) {
    if (expected != value) {
      throw new Error("Expected: " + expected + ", got: " + value);
    }
  }
}
