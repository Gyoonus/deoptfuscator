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
// it compiles these methods.

public class Main {
  public static void main(String[] args) {

    expectEquals(4, $opt$TestLostCopy());
    expectEquals(-10, $opt$TestTwoLive());
    expectEquals(-20, $opt$TestThreeLive());
    expectEquals(5, $opt$TestFourLive());
    expectEquals(10, $opt$TestMultipleLive());
    expectEquals(1, $opt$TestWithBreakAndContinue());
    expectEquals(-15, $opt$testSpillInIf(5, 6, 7));
    expectEquals(-567, $opt$TestAgressiveLive1(1, 2, 3, 4, 5, 6, 7));
    expectEquals(-77, $opt$TestAgressiveLive2(1, 2, 3, 4, 5, 6, 7));

    expectEquals(-55834574850L, $opt$testSpillInIf(5, 6L << 32, 7L << 32));
    expectEquals(-73014444553L, $opt$TestAgressiveLive1(
        1L << 32, (1L << 32) + 1, 3L << 32, 4L << 32, 5L << 32, 6L << 32, (1L << 32) + 2));
    expectEquals(-124554051632L, $opt$TestAgressiveLive2(
        1L << 32, (1L << 32) + 1, 3L << 32, 4L << 32, 5L << 32, 6L << 32, 7L << 32));
  }

  public static long $opt$TestLostCopy() {
    long a = 0;
    long b = 0;
    do {
      b = a;
      a++;
    } while (a != 5);
    return b;
  }

  public static long $opt$TestTwoLive() {
    long a = 0;
    long b = 0;
    do {
      a++;
      b += 3;
    } while (a != 5);
    return a - b;
  }

  public static long $opt$TestThreeLive() {
    long a = 0;
    long b = 0;
    long c = 0;
    do {
      a++;
      b += 3;
      c += 2;
    } while (a != 5);
    return a - b - c;
  }

  public static long $opt$TestFourLive() {
    long a = 0;
    long b = 0;
    long c = 0;
    long d = 0;
    do {
      a++;
      b += 3;
      c += 2;
      d++;
    } while (a != 5);
    return d;
  }

  public static long $opt$TestMultipleLive() {
    long a = 0;
    long b = 0;
    long c = 0;
    long d = 0;
    long e = 0;
    long f = 0;
    long g = 0;
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

  public static long $opt$TestWithBreakAndContinue() {
    long a = 0;
    long b = 0;
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

  public static long $opt$testSpillInIf(long a, long b, long c) {
    long d = 0;
    long e = 0;
    if (a == 5) {
      b++;
      c++;
      d += 2;
      e += 3;
    }

    return a - b - c - d - e;
  }

  public static long $opt$TestAgressiveLive1(long a, long b, long c, long d, long e, long f, long g) {
    long h = a - b;
    long i = c - d;
    long j = e - f;
    long k = 42 + g - a;
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

  public static long $opt$TestAgressiveLive2(long a, long b, long c, long d, long e, long f, long g) {
    long h = a - b;
    long i = c - d;
    long j = e - f;
    long k = 42 + g - a;
    do {
      h++;
    } while (h != 5);
    return a - b - c - d - e - f - g - h - i - j - k;
  }

  public static void expectEquals(long expected, long value) {
    if (expected != value) {
      throw new Error("Expected: " + expected + ", got: " + value);
    }
  }
}
