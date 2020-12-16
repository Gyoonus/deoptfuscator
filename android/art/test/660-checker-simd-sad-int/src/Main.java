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
 * Tests for SAD (sum of absolute differences).
 */
public class Main {

  /// CHECK-START: int Main.sadInt2Int(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.sadInt2Int(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]                   loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]                     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Ld1:d\d+>>    VecLoad [{{l\d+}},<<I:i\d+>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Ld2:d\d+>>    VecLoad [{{l\d+}},<<I>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi>>,<<Ld1>>,<<Ld2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]                       loop:<<Loop>> outer_loop:none
  private static int sadInt2Int(int[] x, int[] y) {
    int min_length = Math.min(x.length, y.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(x[i] - y[i]);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadInt2IntAlt(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                       loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub1:i\d+>>   Sub [<<Get2>>,<<Get1>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub2:i\d+>>   Sub [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Select:i\d+>> Select [<<Sub2>>,<<Sub1>>,{{z\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Select>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]            loop:<<Loop>>      outer_loop:none
  //
  // No ABS? No SAD!
  //
  /// CHECK-START: int Main.sadInt2IntAlt(int[], int[]) loop_optimization (after)
  /// CHECK-NOT: VecSADAccumulate
  private static int sadInt2IntAlt(int[] x, int[] y) {
    int min_length = Math.min(x.length, y.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      int s = x[i];
      int p = y[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  /// CHECK-START: int Main.sadInt2IntAlt2(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.sadInt2IntAlt2(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]                   loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]                     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Ld1:d\d+>>    VecLoad [{{l\d+}},<<I:i\d+>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Ld2:d\d+>>    VecLoad [{{l\d+}},<<I>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi>>,<<Ld1>>,<<Ld2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]                       loop:<<Loop>> outer_loop:none
  private static int sadInt2IntAlt2(int[] x, int[] y) {
    int min_length = Math.min(x.length, y.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      int s = x[i];
      int p = y[i];
      int m = s - p;
      if (m < 0) m = -m;
      sad += m;
    }
    return sad;
  }

  /// CHECK-START: long Main.sadInt2Long(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadInt2Long(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons4:i\d+>>  IntConstant 4                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons4>>]       loop:<<Loop>>      outer_loop:none
  private static long sadInt2Long(int[] x, int[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      long s = x[i];
      long p = y[i];
      sad += Math.abs(s - p);
    }
    return sad;
  }

  /// CHECK-START: long Main.sadInt2LongAt1(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadInt2LongAt1(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons4:i\d+>>  IntConstant 4                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons4>>]       loop:<<Loop>>      outer_loop:none
  private static long sadInt2LongAt1(int[] x, int[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 1;  // starts at 1
    for (int i = 0; i < min_length; i++) {
      long s = x[i];
      long p = y[i];
      sad += Math.abs(s - p);
    }
    return sad;
  }

  public static void main(String[] args) {
    // Cross-test the two most extreme values individually.
    int[] x = { 0, Integer.MAX_VALUE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int[] y = { 0, Integer.MIN_VALUE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    expectEquals(1, sadInt2Int(x, y));
    expectEquals(1, sadInt2Int(y, x));
    expectEquals(-1, sadInt2IntAlt(x, y));
    expectEquals(-1, sadInt2IntAlt(y, x));
    expectEquals(1, sadInt2IntAlt2(x, y));
    expectEquals(1, sadInt2IntAlt2(y, x));
    expectEquals(4294967295L, sadInt2Long(x, y));
    expectEquals(4294967295L, sadInt2Long(y, x));
    expectEquals(4294967296L, sadInt2LongAt1(x, y));
    expectEquals(4294967296L, sadInt2LongAt1(y, x));

    // Use cross-values for the interesting values.
    int[] interesting = {
      0x00000000, 0x00000001, 0x00007fff, 0x00008000, 0x00008001, 0x0000ffff,
      0x00010000, 0x00010001, 0x00017fff, 0x00018000, 0x00018001, 0x0001ffff,
      0x7fff0000, 0x7fff0001, 0x7fff7fff, 0x7fff8000, 0x7fff8001, 0x7fffffff,
      0x80000000, 0x80000001, 0x80007fff, 0x80008000, 0x80008001, 0x8000ffff,
      0x80010000, 0x80010001, 0x80017fff, 0x80018000, 0x80018001, 0x8001ffff,
      0xffff0000, 0xffff0001, 0xffff7fff, 0xffff8000, 0xffff8001, 0xffffffff
    };
    int n = interesting.length;
    int m = n * n + 1;
    x = new int[m];
    y = new int[m];
    int k = 0;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        x[k] = interesting[i];
        y[k] = interesting[j];
        k++;
      }
    }
    x[k] = 10;
    y[k] = 2;
    expectEquals(8, sadInt2Int(x, y));
    expectEquals(-13762600, sadInt2IntAlt(x, y));
    expectEquals(8, sadInt2IntAlt2(x, y));
    expectEquals(2010030931928L, sadInt2Long(x, y));
    expectEquals(2010030931929L, sadInt2LongAt1(x, y));

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
}
