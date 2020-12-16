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

  /// CHECK-START: long Main.sadLong2Long(long[], long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadLong2Long(long[], long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons2>>]       loop:<<Loop>>      outer_loop:none
  private static long sadLong2Long(long[] x, long[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(x[i] - y[i]);
    }
    return sad;
  }

  /// CHECK-START: long Main.sadLong2LongAlt(long[], long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                       loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                       loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub1:j\d+>>   Sub [<<Get2>>,<<Get1>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub2:j\d+>>   Sub [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Select:j\d+>> Select [<<Sub2>>,<<Sub1>>,{{z\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Select>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]            loop:<<Loop>>      outer_loop:none
  //
  // No ABS? No SAD!
  //
  /// CHECK-START: long Main.sadLong2LongAlt(long[], long[]) loop_optimization (after)
  /// CHECK-NOT: VecSADAccumulate
  private static long sadLong2LongAlt(long[] x, long[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      long s = x[i];
      long p = y[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  /// CHECK-START: long Main.sadLong2LongAlt2(long[], long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadLong2LongAlt2(long[], long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons2>>]       loop:<<Loop>>      outer_loop:none
  private static long sadLong2LongAlt2(long[] x, long[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      long s = x[i];
      long p = y[i];
      long m = s - p;
      if (m < 0) m = -m;
      sad += m;
    }
    return sad;
  }

  /// CHECK-START: long Main.sadLong2LongAt1(long[], long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:j\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadLong2LongAt1(long[], long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons2>>]       loop:<<Loop>>      outer_loop:none
  private static long sadLong2LongAt1(long[] x, long[] y) {
    int min_length = Math.min(x.length, y.length);
    long sad = 1;  // starts at 1
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(x[i] - y[i]);
    }
    return sad;
  }

  public static void main(String[] args) {
    // Cross-test the two most extreme values individually.
    long[] x = { 0, Long.MIN_VALUE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    long[] y = { 0, Long.MAX_VALUE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    expectEquals(1L, sadLong2Long(x, y));
    expectEquals(1L, sadLong2Long(y, x));
    expectEquals(-1L, sadLong2LongAlt(x, y));
    expectEquals(-1L, sadLong2LongAlt(y, x));
    expectEquals(1L, sadLong2LongAlt2(x, y));
    expectEquals(1L, sadLong2LongAlt2(y, x));
    expectEquals(2L, sadLong2LongAt1(x, y));
    expectEquals(2L, sadLong2LongAt1(y, x));

    // Use cross-values for the interesting values.
    long[] interesting = {
      0x0000000000000000L, 0x0000000000000001L, 0x000000007fffffffL,
      0x0000000080000000L, 0x0000000080000001L, 0x00000000ffffffffL,
      0x0000000100000000L, 0x0000000100000001L, 0x000000017fffffffL,
      0x0000000180000000L, 0x0000000180000001L, 0x00000001ffffffffL,
      0x7fffffff00000000L, 0x7fffffff00000001L, 0x7fffffff7fffffffL,
      0x7fffffff80000000L, 0x7fffffff80000001L, 0x7fffffffffffffffL,
      0x8000000000000000L, 0x8000000000000001L, 0x800000007fffffffL,
      0x8000000080000000L, 0x8000000080000001L, 0x80000000ffffffffL,
      0x8000000100000000L, 0x8000000100000001L, 0x800000017fffffffL,
      0x8000000180000000L, 0x8000000180000001L, 0x80000001ffffffffL,
      0xffffffff00000000L, 0xffffffff00000001L, 0xffffffff7fffffffL,
      0xffffffff80000000L, 0xffffffff80000001L, 0xffffffffffffffffL
    };
    int n = interesting.length;
    int m = n * n + 1;
    x = new long[m];
    y = new long[m];
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
    expectEquals(8L, sadLong2Long(x, y));
    expectEquals(-901943132200L, sadLong2LongAlt(x, y));
    expectEquals(8L, sadLong2LongAlt2(x, y));
    expectEquals(9L, sadLong2LongAt1(x, y));

    System.out.println("passed");
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
