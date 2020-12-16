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

  static long[] a;

  //
  // Arithmetic operations.
  //

  /// CHECK-START: void Main.add(long) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.add(long) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAdd   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void add(long x) {
    for (int i = 0; i < 128; i++)
      a[i] += x;
  }

  /// CHECK-START: void Main.sub(long) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.sub(long) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecSub   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void sub(long x) {
    for (int i = 0; i < 128; i++)
      a[i] -= x;
  }

  /// CHECK-START: void Main.mul(long) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  //  Not directly supported for longs.
  //
  /// CHECK-START-ARM64: void Main.mul(long) loop_optimization (after)
  /// CHECK-NOT: VecMul
  //
  /// CHECK-START-MIPS64: void Main.mul(long) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecMul   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  static void mul(long x) {
    for (int i = 0; i < 128; i++)
      a[i] *= x;
  }

  /// CHECK-START: void Main.div(long) loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.div(long) loop_optimization (after)
  /// CHECK-NOT: VecDiv
  //
  //  Not supported on any architecture.
  //
  static void div(long x) {
    for (int i = 0; i < 128; i++)
      a[i] /= x;
  }

  /// CHECK-START: void Main.neg() loop_optimization (before)
  /// CHECK-DAG: ArrayGet loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: ArraySet loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.neg() loop_optimization (after)
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
  /// CHECK-START-{ARM64,MIPS64}: void Main.not() loop_optimization (after)
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
  /// CHECK-START-{ARM64,MIPS64}: void Main.shl4() loop_optimization (after)
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
  /// CHECK-START-{ARM64,MIPS64}: void Main.sar2() loop_optimization (after)
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
  /// CHECK-START-{ARM64,MIPS64}: void Main.shr2() loop_optimization (after)
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
  public static int $opt$inline$IntConstant64()       { return 64; }
  public static int $opt$inline$IntConstant65()       { return 65; }
  public static int $opt$inline$IntConstantMinus254() { return -254; }

  /// CHECK-START: void Main.shr64() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 64                        loop:none
  /// CHECK-DAG: <<Get:j\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:j\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shr64() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Get:j\d+>> ArrayGet                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              ArraySet [{{l\d+}},{{i\d+}},<<Get>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.shr64() loop_optimization (after)
  /// CHECK-DAG: <<Get:d\d+>> VecLoad                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              VecStore [{{l\d+}},{{i\d+}},<<Get>>] loop:<<Loop>>      outer_loop:none
  static void shr64() {
    // TODO: remove a[i] = a[i] altogether?
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstant64();  // 0, since & 63
  }

  /// CHECK-START: void Main.shr65() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 65                        loop:none
  /// CHECK-DAG: <<Get:j\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:j\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shr65() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 1                         loop:none
  /// CHECK-DAG: <<Get:j\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:j\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.shr65() loop_optimization (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 1                         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:d\d+>> VecUShr [<<Get>>,<<Dist>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  static void shr65() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstant65();  // 1, since & 63
  }

  /// CHECK-START: void Main.shrMinus254() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant -254                      loop:none
  /// CHECK-DAG: <<Get:j\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:j\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.shrMinus254() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 2                         loop:none
  /// CHECK-DAG: <<Get:j\d+>>  ArrayGet                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:j\d+>> UShr [<<Get>>,<<Dist>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.shrMinus254() loop_optimization (after)
  /// CHECK-DAG: <<Dist:i\d+>> IntConstant 2                         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<UShr:d\d+>> VecUShr [<<Get>>,<<Dist>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<UShr>>] loop:<<Loop>>      outer_loop:none
  static void shrMinus254() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= $opt$inline$IntConstantMinus254();  // 2, since & 63
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
    a = new long[128];
    for (int i = 0; i < 128; i++) {
      a[i] = i;
    }
    // Arithmetic operations.
    add(2L);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + 2, a[i], "add");
    }
    sub(2L);
    for (int i = 0; i < 128; i++) {
      expectEquals(i, a[i], "sub");
    }
    mul(2L);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + i, a[i], "mul");
    }
    div(2L);
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
      a[i] = 0xffffffffffffffffL;
    }
    shl4();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xfffffffffffffff0L, a[i], "shl4");
    }
    sar2();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xfffffffffffffffcL, a[i], "sar2");
    }
    shr2();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x3fffffffffffffffL, a[i], "shr2");
    }
    shr64();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x3fffffffffffffffL, a[i], "shr64");
    }
    shr65();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x1fffffffffffffffL, a[i], "shr65");
    }
    shrMinus254();
    for (int i = 0; i < 128; i++) {
      expectEquals(0x07ffffffffffffffL, a[i], "shrMinus254");
    }
    // Bit-wise not operator.
    not();
    for (int i = 0; i < 128; i++) {
      expectEquals(0xf800000000000000L, a[i], "not");
    }
    // Done.
    System.out.println("passed");
  }

  private static void expectEquals(long expected, long result, String action) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result + " for " + action);
    }
  }
}
