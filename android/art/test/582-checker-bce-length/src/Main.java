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

/**
 * Regression test on duplicate removal of same bounds check.
 */
public class Main {

  /// CHECK-START: void Main.doit1(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doit1(int[]) BCE (after)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doit1(int[]) BCE (after)
  /// CHECK-NOT: Deoptimize
  public static void doit1(int[] a) {
    a[a.length-3] = 1;
    a[a.length-2] = 2;
    a[a.length-1] = 3;
    // This introduces a problematic BoundsCheck(x,x) node
    // (1) certain OOB, so should be rejected
    // (2) exposed bug in removing same BC twice if (1) would not be done.
    a[a.length-0] = 4;
  }

  /// CHECK-START: void Main.doit2(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  /// CHECK-DAG: BoundsCheck
  //
  /// CHECK-START: void Main.doit2(int[]) BCE (after)
  /// CHECK-DAG: Deoptimize
  /// CHECK-DAG: Deoptimize
  //
  /// CHECK-START: void Main.doit2(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  public static void doit2(int[] a) {
    a[a.length-4] = -101;
    a[a.length-3] = -102;
    a[a.length-2] = -103;
    a[a.length-1] = -104;
  }

  public static void main(String[] args) {
    int[] a = new int[4];

    int fail = 0;
    try {
      doit1(a);
    } catch (ArrayIndexOutOfBoundsException e) {
      fail++;
    }
    expectEquals(1, fail);
    expectEquals(0, a[0]);
    expectEquals(1, a[1]);
    expectEquals(2, a[2]);
    expectEquals(3, a[3]);

    try {
      doit2(a);
    } catch (ArrayIndexOutOfBoundsException e) {
      fail++;
    }
    expectEquals(1, fail);
    expectEquals(-101, a[0]);
    expectEquals(-102, a[1]);
    expectEquals(-103, a[2]);
    expectEquals(-104, a[3]);

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
