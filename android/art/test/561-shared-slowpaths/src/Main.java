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

//
// Test on correctness in situations where slow paths may be shared
// (actual sharing may vary between different code generators).
//
//
public class Main {

  // A method with two loops that can be optimized with dynamic BCE,
  // resulting in a two times a deopt on null, a deopt on lower OOB,
  // and a deopt on upper OOB.
  private static void init(int[] x, int [] y, int l1, int h1, int l2, int h2) {
    for (int i = l1; i < h1; i++) {
      x[i] = i;
    }
    for (int i = l2; i < h2; i++) {
      y[i] = i;
    }
  }

  // Test that each of the six possible exceptions situations for init()
  // are correctly handled by the deopt instructions.
  public static void main(String[] args) {
    int[] x = new int[100];
    int[] y = new int[100];
    int z;

    // All is well.
    z = 0;
    reset(x, y);
    try {
      init(x, y, 0, 100, 0, 100);
    } catch (Exception e) {
      z = 1;
    }
    expectEquals(z, 0);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], i);
      expectEquals(y[i], i);
    }

    // Null deopt on x.
    z = 0;
    reset(x, y);
    try {
      init(null, y, 0, 100, 0, 100);
    } catch (NullPointerException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], 0);
      expectEquals(y[i], 0);
    }

    // Lower out-of-bounds on x.
    z = 0;
    reset(x, y);
    try {
      init(x, y, -1, 100, 0, 100);
    } catch (ArrayIndexOutOfBoundsException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], 0);
      expectEquals(y[i], 0);
    }

    // Upper out-of-bounds on x.
    z = 0;
    reset(x, y);
    try {
      init(x, y, 0, 101, 0, 100);
    } catch (ArrayIndexOutOfBoundsException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], i);
      expectEquals(y[i], 0);
    }

    // Null deopt on y.
    z = 0;
    reset(x, y);
    try {
      init(x, null, 0, 100, 0, 100);
    } catch (NullPointerException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], i);
      expectEquals(y[i], 0);
    }

    // Lower out-of-bounds on y.
    z = 0;
    reset(x, y);
    try {
      init(x, y, 0, 100, -1, 100);
    } catch (ArrayIndexOutOfBoundsException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], i);
      expectEquals(y[i], 0);
    }

    // Upper out-of-bounds on y.
    z = 0;
    reset(x, y);
    try {
      init(x, y, 0, 100, 0, 101);
    } catch (ArrayIndexOutOfBoundsException e) {
      z = 1;
    }
    expectEquals(z, 1);
    for (int i = 0; i < 100; i++) {
      expectEquals(x[i], i);
      expectEquals(y[i], i);
    }

    System.out.println("passed");
  }

  private static void reset(int[] x, int[] y) {
    for (int i = 0; i < x.length; i++) x[i] = 0;
    for (int i = 0; i < y.length; i++) y[i] = 0;
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
