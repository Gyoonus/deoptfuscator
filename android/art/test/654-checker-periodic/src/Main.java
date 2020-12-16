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
 * Tests for last value of a few periodic sequences
 * (found by fuzz testing).
 */
public class Main {

  /// CHECK-START: int Main.doitUpInt(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.doitUpInt(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  static int doitUpInt(int n) {
    // Complete loop is replaced by last-value.
    int lI = 1;
    for (int i1 = 0; i1  < n; i1++) {
      lI = (1486662021 - lI);
    }
    return lI;
  }

  /// CHECK-START: int Main.doitDownInt(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.doitDownInt(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  static int doitDownInt(int n) {
    // Complete loop is replaced by last-value.
    int lI = 1;
    for (int i1 = n - 1; i1 >= 0; i1--) {
      lI = (1486662021 - lI);
    }
    return lI;
  }

  /// CHECK-START: float Main.doitUpFloat(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: float Main.doitUpFloat(int) loop_optimization (after)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  static float doitUpFloat(int n) {
    // FP arithmetic is not sufficiently precise.
    // The loop remains.
    float lF = 1.0f;
    for (int i1 = 0; i1  < n; i1++) {
      lF = (1486662021.0f - lF);
    }
    return lF;
  }

  /// CHECK-START: float Main.doitDownFloat(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: float Main.doitDownFloat(int) loop_optimization (after)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  static float doitDownFloat(int n) {
    // FP arithmetic is not sufficiently precise.
    // The loop remains.
    float lF = 1.0f;
    for (int i1 = n - 1; i1 >= 0; i1--) {
      lF = (1486662021.0f - lF);
    }
    return lF;
  }

  /// CHECK-START: float Main.doitUpFloatAlt(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: float Main.doitUpFloatAlt(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  static float doitUpFloatAlt(int n) {
    // Complete loop is replaced by last-value
    // since the values are now precise.
    float lF = 1.0f;
    float l2 = 1486662020.0f;
    for (int i1 = 0; i1  < n; i1++) {
      float old = lF;
      lF = l2;
      l2 = old;
    }
    return lF;
  }

  /// CHECK-START: float Main.doitDownFloatAlt(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              Phi  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: float Main.doitDownFloatAlt(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  static float doitDownFloatAlt(int n) {
    // Complete loop is replaced by last-value
    // since the values are now precise.
    float lF = 1.0f;
    float l2 = 1486662020.0f;
    for (int i1 = n - 1; i1 >= 0; i1--) {
      float old = lF;
      lF = l2;
      l2 = old;
    }
    return lF;
  }

  // Main driver.
  public static void main(String[] args) {
    for (int i = 0; i < 10; i++) {
      int ei = (i & 1) == 0 ? 1 : 1486662020;
      int ci = doitUpInt(i);
      expectEquals(ei, ci);
    }
    for (int i = 0; i < 10; i++) {
      int ei = (i & 1) == 0 ? 1 : 1486662020;
      int ci = doitDownInt(i);
      expectEquals(ei, ci);
    }
    for (int i = 0; i < 10; i++) {
      float ef = i == 0 ? 1.0f : ((i & 1) == 0 ? 0.0f : 1486662021.0f);
      float cf = doitUpFloat(i);
      expectEquals(ef, cf);
    }
    for (int i = 0; i < 10; i++) {
      float ef = i == 0 ? 1.0f : ((i & 1) == 0 ? 0.0f : 1486662021.0f);
      float cf = doitDownFloat(i);
      expectEquals(ef, cf);
    }
    for (int i = 0; i < 10; i++) {
      float ef = (i & 1) == 0 ? 1.0f : 1486662020.0f;
      float cf = doitUpFloatAlt(i);
      expectEquals(ef, cf);
    }
    for (int i = 0; i < 10; i++) {
      float ef = (i & 1) == 0 ? 1.0f : 1486662020.0f;
      float cf = doitDownFloatAlt(i);
      expectEquals(ef, cf);
    }
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}


