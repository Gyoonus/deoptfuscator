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
  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    long l = $opt$ReturnLong();
    expectEquals(42, l);
    System.out.println("Long: " + l);

    l = $opt$TakeOneLong1(42);
    expectEquals(42, l);

    l = $opt$TakeOneLong2(0, 42);
    expectEquals(42, l);

    l = $opt$TakeOneLong3(0, 1, 42);
    expectEquals(42, l);

    l = $opt$TakeOneLong4(0, 1, 2, 42);
    expectEquals(42, l);

    l = $opt$AddTwoLongs(42, 41);
    expectEquals(83, l);

    l = $opt$SubTwoLongs(42, 41);
    expectEquals(1, l);

    l = $opt$MakeCallsWithLongs1();
    expectEquals(57, l);

    l = $opt$MakeCallsWithLongs2();
    expectEquals(900000000006L, l);

    l = $opt$SubTwoLongs(-600000000006L, -200000000002L);
    expectEquals(-400000000004L, l);

    l = $opt$AddTwoLongs(-600000000006L, -200000000002L);
    expectEquals(-800000000008L, l);
  }

  static long $opt$MakeCallsWithLongs1() {
    long l = $opt$SubTwoLongs(-600000000006L, -200000000002L);
    expectEquals(-400000000004L, l);

    l = $opt$AddTwoLongs(-600000000006L, -200000000002L);
    expectEquals(-800000000008L, l);

    return $opt$ReturnLong() + $opt$TakeOneLong1(1) + $opt$TakeOneLong2(0, 2)
        + $opt$TakeOneLong3(0, 0, 3) + $opt$TakeOneLong4(0, 0, 0, 4)
        // Test invoke-range.
        + $opt$TakeOneLong5(0, 0, 0, 0, 5);
  }

  static long $opt$MakeCallsWithLongs2() {
    return $opt$AddThreeLongs(400000000003L, 200000000002L, 300000000001L);
  }

  static long $opt$ReturnLong() {
    return 42;
  }

  static long $opt$TakeOneLong1(long l) {
    return l;
  }

  static long $opt$TakeOneLong2(int a, long l) {
    return l;
  }

  static long $opt$TakeOneLong3(int a, int b, long l) {
    return l;
  }

  static long $opt$TakeOneLong4(int a, int b, int c, long l) {
    return l;
  }

  static long $opt$TakeOneLong5(int a, int b, int c,int d,  long l) {
    return l;
  }

  static long $opt$AddTwoLongs(long a, long b) {
    return a + b;
  }

  static long $opt$AddThreeLongs(long a, long b, long c) {
    return a + b + c;
  }

  static long $opt$SubTwoLongs(long a, long b) {
    return a - b;
  }
}
