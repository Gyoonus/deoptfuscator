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
 * Functional tests for SIMD vectorization.
 */
public class Main {

  static char[] a;

  //
  // Arithmetic operations.
  //

  /// CHECK-START: void Main.add(int) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.add(int) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAdd   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void add(int x) {
    for (int i = 0; i < 128; i++)
      a[i] += x;
  }

  /// CHECK-START: void Main.sub(int) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.sub(int) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecSub   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void sub(int x) {
    for (int i = 0; i < 128; i++)
      a[i] -= x;
  }

  /// CHECK-START: void Main.mul(int) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.mul(int) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecMul   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void mul(int x) {
    for (int i = 0; i < 128; i++)
      a[i] *= x;
  }

  /// CHECK-START: void Main.div(int) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.div(int) loop_optimization (after)
  /// CHECK-NOT: VecDiv
  //
  //  Not supported on any architecture.
  //
  static void div(int x) {
    for (int i = 0; i < 128; i++)
      a[i] /= x;
  }

  /// CHECK-START: void Main.neg() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.neg() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecNeg   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void neg() {
    for (int i = 0; i < 128; i++)
      a[i] = (char) -a[i];
  }

  /// CHECK-START: void Main.not() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.not() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecNot   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void not() {
    for (int i = 0; i < 128; i++)
      a[i] = (char) ~a[i];
  }

  /// CHECK-START: void Main.shl4() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.shl4() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecShl   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void shl4() {
    for (int i = 0; i < 128; i++)
      a[i] <<= 4;
  }

  /// CHECK-START: void Main.sar2() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  // TODO: would need signess flip.
  /// CHECK-START: void Main.sar2() loop_optimization (after)
  /// CHECK-NOT: VecShr
  static void sar2() {
    for (int i = 0; i < 128; i++)
      a[i] >>= 2;
  }

  /// CHECK-START: void Main.shr2() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.shr2() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecUShr  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void shr2() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 2;
  }

  //
  // Shift sanity.
  //

  static void sar31() {
    for (int i = 0; i < 128; i++)
      a[i] >>= 31;
  }

  static void shr31() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 31;
  }

  static void shr32() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 32;  // 0, since & 31
  }

  static void shr33() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 33;  // 1, since & 31
  }

  //
  // Loop bounds.
  //

  static void bounds() {
    for (int i = 1; i < 127; i++)
      a[i] += 11;
  }

  //
  // Test Driver.
  //

  public static void main(String[] args) {
    // Set up.
    a = new char[128];
    for (int i = 0; i < 128; i++) {
      a[i] = (char) i;
    }
    // Arithmetic operations.
    add(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + 2, a[i], "add");
    }
    sub(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i, a[i], "sub");
    }
    mul(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + i, a[i], "mul");
    }
    div(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i, a[i], "div");
    }
    neg();
    for (int i = 0; i < 128; i++) {
      expectEquals((char)-i, a[i], "neg");
    }
    // Loop bounds.
    bounds();
    expectEquals(0, a[0], "bounds0");
    for (int i = 1; i < 127; i++) {
      expectEquals((char)(11 - i), a[i], "bounds");
    }
    expectEquals((char)-127, a[127], "bounds127");
    // Shifts.
    for (int i = 0; i < 128; i++) {
      a[i] = (char) 0xffff;
    }
    shl4();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0xfff0, a[i], "shl4");
    }
    sar2();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x3ffc, a[i], "sar2");
    }
    shr2();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x0fff, a[i], "shr2");
      a[i] = (char) 0xffff;  // reset
    }
    sar31();
    for (int i = 0; i < 128; i++) {
      expectEquals(0, a[i], "sar31");
      a[i] = (char) 0xffff;  // reset
    }
    shr31();
    for (int i = 0; i < 128; i++) {
      expectEquals(0, a[i], "shr31");
      a[i] = (char) 0x1200;  // reset
    }
    shr32();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x1200, a[i], "shr32");
    }
    shr33();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x0900, a[i], "shr33");
      a[i] = (char) 0xf1f0;  // reset
    }
    not();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x0e0f, a[i], "not");
    }
    // Done.
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result, String action) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result + " for " + action);
    }
  }
}
