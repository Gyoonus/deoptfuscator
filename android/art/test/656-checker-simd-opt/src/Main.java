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
import java.lang.reflect.Array;
import java.lang.reflect.Method;

/**
 * Tests for SIMD related optimizations.
 */
public class Main {

  /// CHECK-START: void Main.unroll(float[], float[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons:f\d+>> FloatConstant 2.5                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:f\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:f\d+>>  Mul [<<Get>>,<<Cons>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Mul>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.unroll(float[], float[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:f\d+>> FloatConstant 2.5                    loop:none
  /// CHECK-DAG: <<Incr:i\d+>> IntConstant 4                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<Cons>>]        loop:none
  /// CHECK-NOT:               VecReplicateScalar
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul1:d\d+>> VecMul [<<Get1>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Mul1>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Phi>>,<<Incr>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Add>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul2:d\d+>> VecMul [<<Get2>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Add>>,<<Mul2>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               Add [<<Add>>,<<Incr>>]               loop:<<Loop>>      outer_loop:none
  private static void unroll(float[] x, float[] y) {
    for (int i = 0; i < 100; i++) {
      x[i] = y[i] * 2.5f;
    }
  }

  /// CHECK-START-ARM64: void Main.stencil(int[], int[], int) loop_optimization (after)
  /// CHECK-DAG: <<CP1:i\d+>>   IntConstant 1                         loop:none
  /// CHECK-DAG: <<CP2:i\d+>>   IntConstant 2                         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>>  Add [<<Phi>>,<<CP1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:d\d+>>  VecLoad [{{l\d+}},<<Phi>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>>  VecLoad [{{l\d+}},<<Add1>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Get1>>,<<Get2>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add3:i\d+>>  Add [<<Phi>>,<<CP2>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get3:d\d+>>  VecLoad [{{l\d+}},<<Add3>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add4:d\d+>>  VecAdd [<<Add2>>,<<Get3>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [{{l\d+}},<<Add1>>,<<Add4>>] loop:<<Loop>>      outer_loop:none
  private static void stencil(int[] a, int[] b, int n) {
    for (int i = 1; i < n - 1; i++) {
      a[i] = b[i - 1] + b[i] + b[i + 1];
    }
  }

  private static void stencilAddInt(int[] a, int[] b, int n) {
    try {
      Class<?> c = Class.forName("Smali");
      Method m = c.getMethod("stencilAddInt",
          Array.newInstance(int.class, 1).getClass(),
          Array.newInstance(int.class, 1).getClass(),
          int.class);
      m.invoke(null, a, b, n);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  private static void stencilSubInt(int[] a, int[] b, int n) {
    try {
      Class<?> c = Class.forName("Smali");
      Method m = c.getMethod("stencilSubInt",
          Array.newInstance(int.class, 1).getClass(),
          Array.newInstance(int.class, 1).getClass(),
          int.class);
      m.invoke(null, a, b, n);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  /// CHECK-START: long Main.longInductionReduction(long[]) loop_optimization (before)
  /// CHECK-DAG: <<L0:j\d+>>    LongConstant 0             loop:none
  /// CHECK-DAG: <<L1:j\d+>>    LongConstant 1             loop:none
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0              loop:none
  /// CHECK-DAG: <<Get:j\d+>>   ArrayGet [{{l\d+}},<<I0>>] loop:none
  /// CHECK-DAG: <<Phi1:j\d+>>  Phi [<<L0>>,<<Add1:j\d+>>] loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>  Phi [<<L1>>,<<Add2:j\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2>>       Add [<<Phi2>>,<<Get>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1>>       Add [<<Phi1>>,<<L1>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.longInductionReduction(long[]) loop_optimization (after)
  /// CHECK-DAG: <<L0:j\d+>>    LongConstant 0               loop:none
  /// CHECK-DAG: <<L1:j\d+>>    LongConstant 1               loop:none
  /// CHECK-DAG: <<L2:j\d+>>    LongConstant 2               loop:none
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0                loop:none
  /// CHECK-DAG: <<Get:j\d+>>   ArrayGet [{{l\d+}},<<I0>>]   loop:none
  /// CHECK-DAG: <<Rep:d\d+>>   VecReplicateScalar [<<Get>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>   VecSetScalars [<<L1>>]       loop:none
  /// CHECK-DAG: <<Phi1:j\d+>>  Phi [<<L0>>,{{j\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>  Phi [<<Set>>,{{d\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecAdd [<<Phi2>>,<<Rep>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                Add [<<Phi1>>,<<L2>>]        loop:<<Loop>>      outer_loop:none
  static long longInductionReduction(long[] y) {
    long x = 1;
    for (long i = 0; i < 10; i++) {
      x += y[0];
    }
    return x;
  }

  /// CHECK-START: void Main.intVectorLongInvariant(int[], long[]) loop_optimization (before)
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0                       loop:none
  /// CHECK-DAG: <<I1:i\d+>>    IntConstant 1                       loop:none
  /// CHECK-DAG: <<Get:j\d+>>   ArrayGet [{{l\d+}},<<I0>>]          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<I0>>,<<Add:i\d+>>]           loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Cnv:i\d+>>   TypeConversion [<<Get>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add>>        Add [<<Phi>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.intVectorLongInvariant(int[], long[]) loop_optimization (after)
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0                       loop:none
  /// CHECK-DAG: <<I1:i\d+>>    IntConstant 1                       loop:none
  /// CHECK-DAG: <<I4:i\d+>>    IntConstant 4                       loop:none
  /// CHECK-DAG: <<Get:j\d+>>   ArrayGet [{{l\d+}},<<I0>>]          loop:none
  /// CHECK-DAG: <<Cnv:i\d+>>   TypeConversion [<<Get>>]            loop:none
  /// CHECK-DAG: <<Rep:d\d+>>   VecReplicateScalar [<<Cnv>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<I0>>,{{i\d+}}]               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                VecStore [{{l\d+}},<<Phi>>,<<Rep>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                Add [<<Phi>>,<<I4>>]                loop:<<Loop>>      outer_loop:none
  static void intVectorLongInvariant(int[] x, long[] y) {
    for (int i = 0; i < 100; i++) {
      x[i] = (int) y[0];
    }
  }

  /// CHECK-START: void Main.longCanBeDoneWithInt(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0                        loop:none
  /// CHECK-DAG: <<I1:i\d+>>    IntConstant 1                        loop:none
  /// CHECK-DAG: <<L1:j\d+>>    LongConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<I0>>,<<Add:i\d+>>]            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>  TypeConversion [<<Get>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddL:j\d+>>  Add [<<Cnv1>>,<<L1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:i\d+>>  TypeConversion [<<AddL>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [{{l\d+}},<<Phi>>,<<Cnv2>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add>>        Add [<<Phi>>,<<I1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: void Main.longCanBeDoneWithInt(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<I0:i\d+>>    IntConstant 0                       loop:none
  /// CHECK-DAG: <<I4:i\d+>>    IntConstant 4                       loop:none
  /// CHECK-DAG: <<L1:j\d+>>    LongConstant 1                      loop:none
  /// CHECK-DAG: <<Cnv:i\d+>>   TypeConversion [<<L1>>]             loop:none
  /// CHECK-DAG: <<Rep:d\d+>>   VecReplicateScalar [<<Cnv>>]        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<I0>>,{{i\d+}}]               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Rep>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [{{l\d+}},<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                Add [<<Phi>>,<<I4>>]                loop:<<Loop>>      outer_loop:none
  static void longCanBeDoneWithInt(int[] x, int[] y) {
    for (int i = 0; i < 100; i++) {
      x[i] = (int) (y[i] + 1L);
    }
  }

  static void testUnroll() {
    float[] x = new float[100];
    float[] y = new float[100];
    for (int i = 0; i < 100; i++) {
      x[i] = 0.0f;
      y[i] = 2.0f;
    }
    unroll(x, y);
    for (int i = 0; i < 100; i++) {
      expectEquals(5.0f, x[i]);
      expectEquals(2.0f, y[i]);
    }
  }

  static void testStencil1() {
    int[] a = new int[100];
    int[] b = new int[100];
    for (int i = 0; i < 100; i++) {
      a[i] = 0;
      b[i] = i;
    }
    stencil(a, b, 100);
    for (int i = 1; i < 99; i++) {
      int e = i + i + i;
      expectEquals(e, a[i]);
      expectEquals(i, b[i]);
    }
  }

  static void testStencil2() {
    int[] a = new int[100];
    int[] b = new int[100];
    for (int i = 0; i < 100; i++) {
      a[i] = 0;
      b[i] = i;
    }
    stencilSubInt(a, b, 100);
    for (int i = 1; i < 99; i++) {
      int e = i + i + i;
      expectEquals(e, a[i]);
      expectEquals(i, b[i]);
    }
  }

  static void testStencil3() {
    int[] a = new int[100];
    int[] b = new int[100];
    for (int i = 0; i < 100; i++) {
      a[i] = 0;
      b[i] = i;
    }
    stencilAddInt(a, b, 100);
    for (int i = 1; i < 99; i++) {
      int e = i + i + i;
      expectEquals(e, a[i]);
      expectEquals(i, b[i]);
    }
  }

  static void testTypes() {
    int[] a = new int[100];
    int[] b = new int[100];
    long[] l = { 3 };
    expectEquals(31, longInductionReduction(l));
    intVectorLongInvariant(a, l);
    for (int i = 0; i < 100; i++) {
      expectEquals(3, a[i]);
    }
    longCanBeDoneWithInt(b, a);
    for (int i = 0; i < 100; i++) {
      expectEquals(4, b[i]);
    }
  }

  public static void main(String[] args) {
    testUnroll();
    testStencil1();
    testStencil2();
    testStencil3();
    testTypes();
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
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
