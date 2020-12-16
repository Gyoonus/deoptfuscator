/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * Tests of varying trip counts. Focused on testing
 * core and cleanup loop after vectorization.
 */
public class Main {

  static int[] sA;

  static void init() {
    for (int i = 0; i < sA.length; i++)
      sA[i] = 100;
  }

  static void doitTo(int n) {
    for (int i = 0; i < n; i++)
      sA[i] += 1;
  }

  static void doitFrom(int n) {
    for (int i = n; i < sA.length; i++)
      sA[i] += 1;
  }

  static void verify(int n) {
    for (int i = 0; i < n; i++)
      if (sA[i] != 101)
        throw new Error("failed inside loop");
    for (int i = n; i < sA.length; i++)
      if (sA[i] != 100)
        throw new Error("failed outside loop");
  }

  static void verify() {
    for (int i = 0; i < sA.length; i++)
      if (sA[i] != 101)
        throw new Error("failed inside loop");
  }

  static void driver() {
    for (int n = 0; n <= sA.length; n++) {
      init();
      doitTo(n);
      verify(n);
      doitFrom(n);
      verify();
    }
  }

  public static void main(String[] args) {
    sA = new int[17];
    driver();
    sA = new int[32];
    driver();
    System.out.println("passed");
  }
}

