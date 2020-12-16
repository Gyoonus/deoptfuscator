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
 * Tests on loop optimizations related to induction.
 */
public class Main {

  static int[] a = new int[10];

  static int[] novec = new int[20];  // to prevent vectorization

  /// CHECK-START: void Main.deadSingleLoop() loop_optimization (before)
  /// CHECK-DAG: Phi loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: void Main.deadSingleLoop() loop_optimization (after)
  /// CHECK-NOT: Phi
  static void deadSingleLoop() {
    for (int i = 0; i < 4; i++) {
    }
  }

  /// CHECK-START: void Main.deadSingleLoop() loop_optimization (before)
  /// CHECK-DAG: Phi loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: void Main.deadSingleLoop() loop_optimization (after)
  /// CHECK-NOT: Phi
  static void deadSingleLoopN(int n) {
    for (int i = 0; i < n; i++) {
    }
  }

  /// CHECK-START: void Main.potentialInfiniteLoop(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: void Main.potentialInfiniteLoop(int) loop_optimization (after)
  /// CHECK-DAG: Phi loop:{{B\d+}} outer_loop:none
  static void potentialInfiniteLoop(int n) {
    for (int i = 0; i <= n; i++) {  // loops forever when n = MAX_INT
    }
  }

  /// CHECK-START: void Main.deadNestedLoops() loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi loop:{{B\d+}}      outer_loop:<<Loop>>
  //
  /// CHECK-START: void Main.deadNestedLoops() loop_optimization (after)
  /// CHECK-NOT: Phi
  static void deadNestedLoops() {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
      }
    }
  }

  /// CHECK-START: void Main.deadNestedAndFollowingLoops() loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: Phi loop:{{B\d+}}       outer_loop:<<Loop2>>
  /// CHECK-DAG: Phi loop:{{B\d+}}       outer_loop:<<Loop2>>
  /// CHECK-DAG: Phi loop:<<Loop3:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: Phi loop:{{B\d+}}       outer_loop:<<Loop3>>
  /// CHECK-DAG: Phi loop:{{B\d+}}       outer_loop:none
  //
  /// CHECK-START: void Main.deadNestedAndFollowingLoops() loop_optimization (after)
  /// CHECK-NOT: Phi
  static void deadNestedAndFollowingLoops() {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
        }
        for (int k = 0; k < 4; k++) {
        }
      }
      for (int j = 0; j < 4; j++) {
        for (int k = 0; k < 4; k++) {
        }
      }
    }
    for (int i = 0; i < 4; i++) {
    }
  }

  /// CHECK-START: void Main.deadConditional(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: void Main.deadConditional(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static void deadConditional(int n) {
    int k = 0;
    int m = 0;
    for (int i = 0; i < n; i++) {
      if (i == 3)
        k = i;
      else
        m = i;
    }
  }

  /// CHECK-START: void Main.deadConditionalCycle(int) loop_optimization (before)
  /// CHECK-DAG: Phi loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.deadConditionalCycle(int) loop_optimization (after)
  /// CHECK-NOT: Phi
  public static void deadConditionalCycle(int n) {
    int k = 0;
    int m = 0;
    for (int i = 0; i < n; i++) {
      if (i == 3)
        k--;
      else
        m++;
    }
  }


  /// CHECK-START: void Main.deadInduction() loop_optimization (before)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.deadInduction() loop_optimization (after)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  static void deadInduction() {
    int dead = 0;
    for (int i = 0; i < a.length; i++) {
      a[i] = novec[2 * i] + 1;
      dead += 5;
    }
  }

  /// CHECK-START: void Main.deadManyInduction() loop_optimization (before)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.deadManyInduction() loop_optimization (after)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  static void deadManyInduction() {
    int dead1 = 0, dead2 = 1, dead3 = 3;
    for (int i = 0; i < a.length; i++) {
      dead1 += 5;
      a[i] = novec[2 * i] + 2;
      dead2 += 10;
      dead3 += 100;
    }
  }

  /// CHECK-START: void Main.deadSequence() loop_optimization (before)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.deadSequence() loop_optimization (after)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  static void deadSequence() {
    int dead = 0;
    for (int i = 0; i < a.length; i++) {
      a[i] = novec[2 * i] + 3;
      // Increment value defined inside loop,
      // but sequence itself not used anywhere.
      dead += i;
    }
  }

  /// CHECK-START: void Main.deadCycleWithException(int) loop_optimization (before)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT: BoundsCheck
  //
  /// CHECK-START: void Main.deadCycleWithException(int) loop_optimization (after)
  /// CHECK-DAG: Phi      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: Phi      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: ArrayGet loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT: ArrayGet loop:<<Loop>>      outer_loop:none
  static void deadCycleWithException(int k) {
    int dead = 0;
    for (int i = 0; i < a.length; i++) {
      a[i] = novec[2 * i] + 4;
      // Increment value of dead cycle may throw exception. Dynamic
      // BCE takes care of the bounds check though, which enables
      // removing the ArrayGet after removing the dead cycle.
      dead += a[k];
    }
  }

  /// CHECK-START: int Main.closedFormInductionUp() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedFormInductionUp() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedFormInductionUp() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 12395 loop:none
  /// CHECK-DAG:               Return [<<Int>>]  loop:none
  static int closedFormInductionUp() {
    int closed = 12345;
    for (int i = 0; i < 10; i++) {
      closed += 5;
    }
    return closed;  // only needs last value
  }

  /// CHECK-START: int Main.closedFormInductionInAndDown(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: int Main.closedFormInductionInAndDown(int) loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedFormInductionInAndDown(int) instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue        loop:none
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant -50       loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Int>>,<<Par>>] loop:none
  /// CHECK-DAG:               Return [<<Add>>]      loop:none
  static int closedFormInductionInAndDown(int closed) {
    for (int i = 0; i < 10; i++) {
      closed -= 5;
    }
    return closed;  // only needs last value
  }

  /// CHECK-START: int Main.closedFormInductionTrivialIf() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Select            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedFormInductionTrivialIf() loop_optimization (after)
  /// CHECK-NOT:               Phi
  /// CHECK-NOT:               Select
  //
  /// CHECK-START: int Main.closedFormInductionTrivialIf() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 81    loop:none
  /// CHECK-DAG:               Return [<<Int>>]  loop:none
  static int closedFormInductionTrivialIf() {
    int closed = 11;
    for (int i = 0; i < 10; i++) {
      // Trivial if becomes trivial select at HIR level.
      // Make sure this is still recognized as induction.
      if (i < 5) {
        closed += 7;
      } else {
        closed += 7;
      }
    }
    return closed;  // only needs last value
  }

  /// CHECK-START: int Main.closedFormNested() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>> Phi               loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Phi4:i\d+>> Phi               loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedFormNested() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedFormNested() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 100  loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  static int closedFormNested() {
    int closed = 0;
    for (int i = 0; i < 10; i++) {
      for (int j = 0; j < 10; j++) {
        closed++;
      }
    }
    return closed;  // only needs last-value
  }

  /// CHECK-START: int Main.closedFormNestedAlt() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>> Phi               loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Phi4:i\d+>> Phi               loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedFormNestedAlt() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedFormNestedAlt() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 15082 loop:none
  /// CHECK-DAG:               Return [<<Int>>]  loop:none
  static int closedFormNestedAlt() {
    int closed = 12345;
    for (int i = 0; i < 17; i++) {
      for (int j = 0; j < 23; j++) {
        closed += 7;
      }
    }
    return closed;  // only needs last-value
  }

  // TODO: taken test around closed form?
  static int closedFormInductionUpN(int n) {
    int closed = 12345;
    for (int i = 0; i < n; i++) {
      closed += 5;
    }
    return closed;  // only needs last value
  }

  // TODO: taken test around closed form?
  static int closedFormInductionInAndDownN(int closed, int n) {
    for (int i = 0; i < n; i++) {
      closed -= 5;
    }
    return closed;  // only needs last value
  }

  // TODO: move closed form even further out?
  static int closedFormNestedN(int n) {
    int closed = 0;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < 10; j++) {
        closed++;
      }
    }
    return closed;  // only needs last-value
  }

  // TODO: move closed form even further out?
  static int closedFormNestedNAlt(int n) {
    int closed = 12345;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < 23; j++) {
        closed += 7;
      }
    }
    return closed;  // only needs last-value
  }

  // TODO: move closed form even further out?
  static int closedFormNestedMN(int m, int n) {
    int closed = 0;
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < n; j++) {
        closed++;
      }
    }
    return closed;  // only needs last-value
  }

  // TODO: move closed form even further out?
  static int closedFormNestedMNAlt(int m, int n) {
    int closed = 12345;
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < n; j++) {
        closed += 7;
      }
    }
    return closed;  // only needs last-value
  }

  /// CHECK-START: int Main.mainIndexReturned() loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi              loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG:              Return [<<Phi>>] loop:none
  //
  /// CHECK-START: int Main.mainIndexReturned() loop_optimization (after)
  /// CHECK-NOT:              Phi
  //
  /// CHECK-START: int Main.mainIndexReturned() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 10   loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  static int mainIndexReturned() {
    int i;
    for (i = 0; i < 10; i++);
    return i;
  }

  /// CHECK-START: int Main.periodicReturned9() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: int Main.periodicReturned9() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.periodicReturned9() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 1    loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  static int periodicReturned9() {
    int k = 0;
    for (int i = 0; i < 9; i++) {
      k = 1 - k;
    }
    return k;
  }

  /// CHECK-START: int Main.periodicReturned10() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: int Main.periodicReturned10() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.periodicReturned10() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 0    loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  static int periodicReturned10() {
    int k = 0;
    for (int i = 0; i < 10; i++) {
      k = 1 - k;
    }
    return k;
  }

  /// CHECK-START: int Main.getSum21() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi3>>] loop:none
  //
  /// CHECK-START: int Main.getSum21() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.getSum21() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 21   loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static int getSum21() {
    int k = 0;
    int sum = 0;
    for (int i = 0; i < 6; i++) {
      k++;
      sum += k;
    }
    return sum;
  }

  // Ensure double induction does not "overshoot" the subscript range.
  private static int getIncr2(int[] arr) {
    for (int i = 0; i < 12; ) {
      arr[i++] = 30;
      arr[i++] = 29;
    }
    int sum = 0;
    for (int i = 0; i < 12; i++) {
      sum += arr[i];
    }
    return sum;
  }

  // TODO: handle as closed/empty eventually?
  static int mainIndexReturnedN(int n) {
    int i;
    for (i = 0; i < n; i++);
    return i;
  }

  // TODO: handle as closed/empty eventually?
  static int mainIndexShort1(short s) {
    int i = 0;
    for (i = 0; i < s; i++) { }
    return i;
  }

  // TODO: handle as closed/empty eventually?
  static int mainIndexShort2(short s) {
    int i = 0;
    for (i = 0; s > i; i++) { }
    return i;
  }

  /// CHECK-START: int Main.periodicReturnedN(int) loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: int Main.periodicReturnedN(int) loop_optimization (after)
  /// CHECK-NOT:               Phi
  static int periodicReturnedN(int n) {
    int k = 0;
    for (int i = 0; i < n; i++) {
      k = 1 - k;
    }
    return k;
  }

  // If ever replaced by closed form, last value should be correct!
  private static int getSumN(int n) {
    int k = 0;
    int sum = 0;
    for (int i = 0; i < n; i++) {
      k++;
      sum += k;
    }
    return sum;
  }

  // If ever replaced by closed form, last value should be correct!
  private static int closedTwice() {
    int closed = 0;
    for (int i = 0; i < 10; i++) {
      closed++;
    }
    // Closed form of first loop defines trip count of second loop.
    int other_closed = 0;
    for (int i = 0; i < closed; i++) {
      other_closed++;
    }
    return other_closed;
  }

  /// CHECK-START: int Main.closedFeed() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>> Phi               loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi4:i\d+>> Phi               loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi3>>] loop:none
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START: int Main.closedFeed() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedFeed() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 20   loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static int closedFeed() {
    int closed = 0;
    for (int i = 0; i < 10; i++) {
      closed++;
    }
    // Closed form of first loop feeds into initial value of second loop,
    // used when generating closed form for the latter.
    for (int i = 0; i < 10; i++) {
      closed++;
    }
    return closed;
  }

  /// CHECK-START: int Main.closedLargeUp() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedLargeUp() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedLargeUp() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant -10  loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static int closedLargeUp() {
    int closed = 0;
    for (int i = 0; i < 10; i++) {
      closed += 0x7fffffff;
    }
    return closed;
  }

  /// CHECK-START: int Main.closedLargeDown() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi1>>] loop:none
  //
  /// CHECK-START: int Main.closedLargeDown() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.closedLargeDown() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 10   loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static int closedLargeDown() {
    int closed = 0;
    for (int i = 0; i < 10; i++) {
      closed -= 0x7fffffff;
    }
    return closed;
  }

  /// CHECK-START: int Main.waterFall() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>> Phi               loop:<<Loop3:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi4:i\d+>> Phi               loop:<<Loop4:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi5:i\d+>> Phi               loop:<<Loop5:B\d+>> outer_loop:none
  /// CHECK-DAG:               Return [<<Phi5>>] loop:none
  //
  /// CHECK-START: int Main.waterFall() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: int Main.waterFall() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 50   loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static int waterFall() {
    int i = 0;
    for (; i < 10; i++);
    for (; i < 20; i++);
    for (; i < 30; i++);
    for (; i < 40; i++);
    for (; i < 50; i++);
    return i;  // this should become just 50
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom1() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom1() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom1() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 0    loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static boolean periodicBoolIdiom1() {
    boolean x = true;
    for (int i = 0; i < 7; i++) {
      x = !x;
    }
    return x;
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom2() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom2() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom2() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 0    loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static boolean periodicBoolIdiom2() {
    boolean x = true;
    for (int i = 0; i < 7; i++) {
      x = (x != true);
    }
    return x;
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom3() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom3() loop_optimization (after)
  /// CHECK-NOT:               Phi
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom3() instruction_simplifier$after_bce (after)
  /// CHECK-DAG: <<Int:i\d+>>  IntConstant 0    loop:none
  /// CHECK-DAG:               Return [<<Int>>] loop:none
  private static boolean periodicBoolIdiom3() {
    boolean x = true;
    for (int i = 0; i < 7; i++) {
      x = (x == false);
    }
    return x;
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom1N(boolean, int) loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom1N(boolean, int) loop_optimization (after)
  /// CHECK-NOT:               Phi
  private static boolean periodicBoolIdiom1N(boolean x, int n) {
    for (int i = 0; i < n; i++) {
      x = !x;
    }
    return x;
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom2N(boolean, int) loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom2N(boolean, int) loop_optimization (after)
  /// CHECK-NOT:               Phi
  private static boolean periodicBoolIdiom2N(boolean x, int n) {
    for (int i = 0; i < n; i++) {
      x = (x != true);
    }
    return x;
  }

  /// CHECK-START: boolean Main.periodicBoolIdiom3N(boolean, int) loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: boolean Main.periodicBoolIdiom3N(boolean, int) loop_optimization (after)
  /// CHECK-NOT:               Phi
  private static boolean periodicBoolIdiom3N(boolean x, int n) {
    for (int i = 0; i < n; i++) {
      x = (x == false);
    }
    return x;
  }

  /// CHECK-START: float Main.periodicFloat10() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi4:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: float Main.periodicFloat10() loop_optimization (after)
  /// CHECK-NOT: Phi
  //
  /// CHECK-START: float Main.periodicFloat10() loop_optimization (after)
  /// CHECK-DAG: <<Float:f\d+>>  FloatConstant 2    loop:none
  /// CHECK-DAG:                 Return [<<Float>>] loop:none
  private static float periodicFloat10() {
    float r = 4.5f;
    float s = 2.0f;
    float t = -1.0f;
    for (int i = 0; i < 10; i++) {
      float tmp = t; t = r; r = s; s = tmp;
    }
    return r;
  }

  /// CHECK-START: float Main.periodicFloat11() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi4:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: float Main.periodicFloat11() loop_optimization (after)
  /// CHECK-NOT: Phi
  //
  /// CHECK-START: float Main.periodicFloat11() loop_optimization (after)
  /// CHECK-DAG: <<Float:f\d+>>  FloatConstant -1   loop:none
  /// CHECK-DAG:                 Return [<<Float>>] loop:none
  private static float periodicFloat11() {
    float r = 4.5f;
    float s = 2.0f;
    float t = -1.0f;
    for (int i = 0; i < 11; i++) {
      float tmp = t; t = r; r = s; s = tmp;
    }
    return r;
  }

  /// CHECK-START: float Main.periodicFloat12() loop_optimization (before)
  /// CHECK-DAG: <<Phi1:i\d+>> Phi               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi4:f\d+>> Phi               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Return [<<Phi2>>] loop:none
  //
  /// CHECK-START: float Main.periodicFloat12() loop_optimization (after)
  /// CHECK-NOT: Phi
  //
  /// CHECK-START: float Main.periodicFloat12() loop_optimization (after)
  /// CHECK-DAG: <<Float:f\d+>>  FloatConstant 4.5  loop:none
  /// CHECK-DAG:                 Return [<<Float>>] loop:none
  private static float periodicFloat12() {
    float r = 4.5f;
    float s = 2.0f;
    float t = -1.0f;
    for (int i = 0; i < 12; i++) {
      float tmp = t; t = r; r = s; s = tmp;
    }
    return r;
  }

  private static int exceptionExitBeforeAdd() {
    int k = 0;
    try {
      for (int i = 0; i < 10; i++) {
        a[i] = 0;
        k += 10;  // increment last
      }
    } catch(Exception e) {
      // Flag error by returning current
      // value of k negated.
      return -k-1;
    }
    return k;
  }

  private static int exceptionExitAfterAdd() {
    int k = 0;
    try {
      for (int i = 0; i < 10; i++) {
        k += 10;  // increment first
        a[i] = 0;
      }
    } catch(Exception e) {
      // Flag error by returning current
      // value of k negated.
      return -k-1;
    }
    return k;
  }

  public static void main(String[] args) {
    deadSingleLoop();
    deadSingleLoopN(4);
    potentialInfiniteLoop(4);
    deadNestedLoops();
    deadNestedAndFollowingLoops();
    deadConditional(4);
    deadConditionalCycle(4);

    deadInduction();
    for (int i = 0; i < a.length; i++) {
      expectEquals(1, a[i]);
    }
    deadManyInduction();
    for (int i = 0; i < a.length; i++) {
      expectEquals(2, a[i]);
    }
    deadSequence();
    for (int i = 0; i < a.length; i++) {
      expectEquals(3, a[i]);
    }
    try {
      deadCycleWithException(-1);
      throw new Error("Expected: IOOB exception");
    } catch (IndexOutOfBoundsException e) {
    }
    for (int i = 0; i < a.length; i++) {
      expectEquals(i == 0 ? 4 : 3, a[i]);
    }
    deadCycleWithException(0);
    for (int i = 0; i < a.length; i++) {
      expectEquals(4, a[i]);
    }

    expectEquals(12395, closedFormInductionUp());
    expectEquals(12295, closedFormInductionInAndDown(12345));
    expectEquals(81, closedFormInductionTrivialIf());
    expectEquals(10 * 10, closedFormNested());
    expectEquals(12345 + 17 * 23 * 7, closedFormNestedAlt());
    for (int n = -4; n < 10; n++) {
      int tc = (n <= 0) ? 0 : n;
      expectEquals(12345 + tc * 5, closedFormInductionUpN(n));
      expectEquals(12345 - tc * 5, closedFormInductionInAndDownN(12345, n));
      expectEquals(tc * 10, closedFormNestedN(n));
      expectEquals(12345 + tc * 23 * 7, closedFormNestedNAlt(n));
      expectEquals(tc * (tc + 1), closedFormNestedMN(n, n + 1));
      expectEquals(12345 + tc * (tc + 1) * 7, closedFormNestedMNAlt(n, n + 1));
    }

    expectEquals(10, mainIndexReturned());
    expectEquals(1, periodicReturned9());
    expectEquals(0, periodicReturned10());
    expectEquals(21, getSum21());
    expectEquals(354, getIncr2(new int[12]));
    for (int n = -4; n < 4; n++) {
      int tc = (n <= 0) ? 0 : n;
      expectEquals(tc, mainIndexReturnedN(n));
      expectEquals(tc, mainIndexShort1((short) n));
      expectEquals(tc, mainIndexShort2((short) n));
      expectEquals(tc & 1, periodicReturnedN(n));
      expectEquals((tc * (tc + 1)) / 2, getSumN(n));
    }

    expectEquals(10, closedTwice());
    expectEquals(20, closedFeed());
    expectEquals(-10, closedLargeUp());
    expectEquals(10, closedLargeDown());
    expectEquals(50, waterFall());

    expectEquals(false, periodicBoolIdiom1());
    expectEquals(false, periodicBoolIdiom2());
    expectEquals(false, periodicBoolIdiom3());
    for (int n = -4; n < 10; n++) {
      int tc = (n <= 0) ? 0 : n;
      boolean even = (tc & 1) == 0;
      expectEquals(even, periodicBoolIdiom1N(true, n));
      expectEquals(!even, periodicBoolIdiom1N(false, n));
      expectEquals(even, periodicBoolIdiom2N(true, n));
      expectEquals(!even, periodicBoolIdiom2N(false, n));
      expectEquals(even, periodicBoolIdiom3N(true, n));
      expectEquals(!even, periodicBoolIdiom3N(false, n));
    }

    expectEquals( 2.0f, periodicFloat10());
    expectEquals(-1.0f, periodicFloat11());
    expectEquals( 4.5f, periodicFloat12());

    expectEquals(100, exceptionExitBeforeAdd());
    expectEquals(100, exceptionExitAfterAdd());
    a = null;
    expectEquals(-1, exceptionExitBeforeAdd());
    expectEquals(-11, exceptionExitAfterAdd());
    a = new int[4];
    expectEquals(-41, exceptionExitBeforeAdd());
    expectEquals(-51, exceptionExitAfterAdd());

    System.out.println("passed");
  }

  private static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
