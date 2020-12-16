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
// Test on loop unrolling. Removes loop control overhead (including suspend
// checks) and exposes more opportunities for constant folding.
//
public class Main {

  static int sA = 0;

  /// CHECK-START: void Main.unroll() loop_optimization (before)
  /// CHECK-DAG: Phi            loop:<<Loop:B\d+>>
  /// CHECK-DAG: StaticFieldSet loop:<<Loop>>
  //
  /// CHECK-START: void Main.unroll() loop_optimization (after)
  /// CHECK-DAG: StaticFieldSet loop:none
  //
  /// CHECK-START: void Main.unroll() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>> IntConstant    68                  loop:none
  /// CHECK-DAG:              StaticFieldSet [{{l\d+}},<<Int>>]  loop:none
  //
  /// CHECK-START: void Main.unroll() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static void unroll() {
    for (int i = 4; i < 5; i++) {
      sA = 17 * i;
    }
  }

  /// CHECK-START: int Main.unrollLV() loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi              loop:<<Loop:B\d+>>
  /// CHECK-DAG:              StaticFieldSet   loop:<<Loop>>
  /// CHECK-DAG:              Return [<<Phi>>] loop:none
  //
  /// CHECK-START: int Main.unrollLV() loop_optimization (after)
  /// CHECK-DAG: StaticFieldSet loop:none
  //
  /// CHECK-START: int Main.unrollLV() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int1:i\d+>> IntConstant    187                 loop:none
  /// CHECK-DAG: <<Int2:i\d+>> IntConstant    12                  loop:none
  /// CHECK-DAG:               StaticFieldSet [{{l\d+}},<<Int1>>] loop:none
  /// CHECK-DAG:               Return [<<Int2>>]                  loop:none
  //
  /// CHECK-START: int Main.unrollLV() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static int unrollLV() {
    int i;
    for (i = 11; i < 12; i++) {
      sA = 17 * i;
    }
    return i;
  }

  /// CHECK-START: void Main.unrollNest() loop_optimization (before)
  /// CHECK-DAG:               SuspendCheck    loop:none
  /// CHECK-DAG: <<Phi1:i\d+>> Phi             loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG:               SuspendCheck    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi             loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG:               SuspendCheck    loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Phi3:i\d+>> Phi             loop:<<Loop3:B\d+>> outer_loop:<<Loop2>>
  /// CHECK-DAG:               SuspendCheck    loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:               StaticFieldSet  loop:<<Loop3>>      outer_loop:<<Loop2>>
  //
  /// CHECK-START: void Main.unrollNest() loop_optimization (after)
  /// CHECK-DAG: StaticFieldSet loop:none
  /// CHECK-DAG: SuspendCheck   loop:none
  /// CHECK-NOT: SuspendCheck
  //
  /// CHECK-START: void Main.unrollNest() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>> IntConstant    6                   loop:none
  /// CHECK-DAG:              StaticFieldSet [{{l\d+}},<<Int>>]  loop:none
  //
  /// CHECK-START: void Main.unrollNest() loop_optimization (after)
  /// CHECK-NOT: Phi
  public static void unrollNest() {
    // Unrolling each loop in turn ultimately removes the complete nest!
    for (int i = 4; i < 5; i++) {
      for (int j = 5; j < 6; j++) {
        for (int k = 6; k < 7; k++) {
          sA = k;
        }
      }
    }
  }

  //
  // Verifier.
  //

  public static void main(String[] args) {
    unroll();
    expectEquals(68, sA);
    expectEquals(12, unrollLV());
    expectEquals(187, sA);
    unrollNest();
    expectEquals(6, sA);
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
