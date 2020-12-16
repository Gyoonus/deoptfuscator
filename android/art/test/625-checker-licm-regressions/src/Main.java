/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * Regression tests for LICM.
 */
public class Main {

  static int sA;

  //
  // We cannot hoist the null check (can throw) above the field
  // assignment (has write side effects) because that would result
  // in throwing an exception before the assignment is done.
  //
  /// CHECK-START: void Main.foo(int[]) licm (before)
  /// CHECK-DAG: LoadClass      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: StaticFieldSet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: NullCheck      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayLength    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.foo(int[]) licm (after)
  /// CHECK-DAG: LoadClass      loop:none
  /// CHECK-DAG: StaticFieldSet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: NullCheck      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayLength    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.foo(int[]) licm (after)
  /// CHECK-NOT: LoadClass      loop:{{B\d+}} outer_loop:none
  static void foo(int[] arr) {
    int j = 0;
    do {
      sA = 1;
    } while (j < arr.length);
  }

  //
  // Similar situation as in foo(), but now a proper induction value
  // is assigned to the field inside the do-while loop.
  //
  /// CHECK-START: void Main.bar(int[]) licm (before)
  /// CHECK-DAG: LoadClass      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: StaticFieldSet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: NullCheck      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayLength    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.bar(int[]) licm (after)
  /// CHECK-DAG: LoadClass      loop:none
  /// CHECK-DAG: StaticFieldSet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: NullCheck      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayLength    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.bar(int[]) licm (after)
  /// CHECK-NOT: LoadClass      loop:{{B\d+}} outer_loop:none
  static void bar(int[] arr) {
    int j = 0;
    do {
      j++;
      sA = j;
    } while (j < arr.length);
  }

  //
  // Similar situation as in bar(), but now an explicit catch
  // statement may need the latest value of local j.
  //
  /// CHECK-START: int Main.catcher(int[]) licm (before)
  /// CHECK-DAG: NullCheck   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayLength loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.catcher(int[]) licm (after)
  /// CHECK-DAG: NullCheck   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArrayLength loop:<<Loop>>      outer_loop:none
  static int catcher(int[] arr) {
    int j = 0;
    try {
      do {
        j++;
      } while (j < arr.length);
    } catch (NullPointerException e) {
      return -j;  // flag exception with negative value
    }
    return j;
  }

  public static void main(String[] args) {
    sA = 0;
    try {
      foo(null);
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
    }
    expectEquals(1, sA);

    sA = 0;
    try {
      bar(null);
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
    }
    expectEquals(1, sA);

    for (int i = 0; i < 5; i++) {
      sA = 0;
      bar(new int[i]);
      expectEquals(i == 0 ? 1 : i, sA);
    }

    expectEquals(-1, catcher(null));
    for (int i = 0; i < 5; i++) {
      expectEquals(i == 0 ? 1 : i, catcher(new int[i]));
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
