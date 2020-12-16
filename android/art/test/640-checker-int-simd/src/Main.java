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

  static int[] a;

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
      a[i] = -a[i];
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
      a[i] = ~a[i];
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
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.sar2() loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecShr   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
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

  // Expose constants to optimizing compiler, but not to front-end.
  public static int $opt$inline$IntConstant32()       { return 32; }
  public static int $opt$inline$IntConstant33()       { return 33; }
  public static int $opt$inline$IntConstantMinus254() { return -254; }

  /// CHECK-START: void Main.shr32() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 32                        loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:i\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shr32() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Get:i\d+>> ArrayGet                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              ArraySet [{{l\d+}},{{i\d+}},<<Get>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.shr32() loop_optimization (after)
  /// CHECK-DAG: <<Get:d\d+>> VecLoad                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              VecStore [{{l\d+}},{{i\d+}},<<Get>>] loop:<<Loop>>      outer_loop:none
  static void shr32() {
    // TODO: remove a[i] = a[i] altogether?
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstant32();  // 0, since & 31
  }

  /// CHECK-START: void Main.shr33() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 33                        loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:i\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shr33() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 1                         loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:i\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.shr33() loop_optimization (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 1                         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:d\d+>> VecUShr [<<Get>>,<<Dist>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  static void shr33() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstant33();  // 1, since & 31
  }

  /// CHECK-START: void Main.shrMinus254() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant -254                      loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:i\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shrMinus254() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 2                         loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:i\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.shrMinus254() loop_optimization (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 2                         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:d\d+>> VecUShr [<<Get>>,<<Dist>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  static void shrMinus254() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstantMinus254();  // 2, since & 31
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
    a = new int[128];
    for (int i = 0; i < 128; i++) {
      a[i] = i;
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
      expectEquals(-i, a[i], "neg");
    }
    // Loop bounds.
    bounds();
    expectEquals(0, a[0], "bounds0");
    for (int i = 1; i < 127; i++) {
      expectEquals(11 - i, a[i], "bounds");
    }
    expectEquals(-127, a[127], "bounds127");
    // Shifts.
    for (int i = 0; i < 128; i++) {
      a[i] = 0xffffffff;
    }
    shl4();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xfffffff0, a[i], "shl4");
    }
    sar2();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xfffffffc, a[i], "sar2");
    }
    shr2();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x3fffffff, a[i], "shr2");
    }
    shr32();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x3fffffff, a[i], "shr32");
    }
    shr33();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x1fffffff, a[i], "shr33");
    }
    shrMinus254();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x07ffffff, a[i], "shrMinus254");
    }
    // Bit-wise not operator.
    not();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xf8000000, a[i], "not");
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
