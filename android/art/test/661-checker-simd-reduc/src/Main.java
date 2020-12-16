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
 * Tests for simple integral reductions: same type for accumulator and data.
 */
public class Main {

  static final int N = 500;
  static final int M = 100;

  //
  // Basic reductions in loops.
  //

  // TODO: vectorize these (second step of b/64091002 plan)

  private static byte reductionByte(byte[] x) {
    byte sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static short reductionShort(short[] x) {
    short sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static char reductionChar(char[] x) {
    char sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  /// CHECK-START: int Main.reductionInt(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.reductionInt(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}           loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:i\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static int reductionInt(int[] x) {
    int sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  /// CHECK-START: int Main.reductionIntChain() loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons1>>,{{i\d+}}]      loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>   ArrayGet [{{l\d+}},<<Phi2>>]  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Get1>>]       loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Cons1>>]      loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi4:i\d+>>   Phi [<<Phi1>>,{{i\d+}}]       loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>   ArrayGet [{{l\d+}},<<Phi3>>]  loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi4>>,<<Get2>>]       loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi3>>,<<Cons1>>]      loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi4>>]             loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.reductionIntChain() loop_optimization (after)
  /// CHECK-DAG: <<Set1:d\d+>>   VecSetScalars [{{i\d+}}]       loop:none
  /// CHECK-DAG: <<Phi1:d\d+>>   Phi [<<Set1>>,{{d\d+}}]        loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<I1:i\d+>>] loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi1>>,<<Load1>>]    loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I1>>,{{i\d+}}]          loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Red1:d\d+>>   VecReduce [<<Phi1>>]           loop:none
  /// CHECK-DAG: <<Extr1:i\d+>>  VecExtractScalar [<<Red1>>]    loop:none
  /// CHECK-DAG: <<Set2:d\d+>>   VecSetScalars [{{i\d+}}]       loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set2>>,{{d\d+}}]        loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<I2:i\d+>>] loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi2>>,<<Load2>>]    loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I2>>,{{i\d+}}]          loop:<<Loop2>>      outer_loop:none
  /// CHECK-DAG: <<Red2:d\d+>>   VecReduce [<<Phi2>>]           loop:none
  /// CHECK-DAG: <<Extr2:i\d+>>  VecExtractScalar [<<Red2>>]    loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  // NOTE: pattern is robust with respect to vector loop unrolling and peeling.
  private static int reductionIntChain() {
    int[] x = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    int r = 1;
    for (int i = 0; i < 16; i++) {
      r += x[i];
    }
    for (int i = 0; i < 16; i++) {
      r += x[i];
    }
    return r;
  }

  /// CHECK-START: int Main.reductionIntToLoop(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Get>>]        loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>   Phi [<<Phi2>>,{{i\d+}}]       loop:<<Loop2:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi4:i\d+>>   Phi [<<Phi2>>,{{i\d+}}]       loop:<<Loop2>>      outer_loop:none
  //
  /// CHECK-EVAL: "<<Loop1>>" != "<<Loop2>>"
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.reductionIntToLoop(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}           loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi>>,<<Load>>]     loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]          loop:<<Loop1>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:i\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static int reductionIntToLoop(int[] x) {
    int r = 0;
    for (int i = 0; i < 8; i++) {
      r += x[i];
    }
    for (int i = r; i < 16; i++) {
      r += i;
    }
    return r;
  }

  /// CHECK-START: long Main.reductionLong(long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Long0:j\d+>>  LongConstant 0                loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<Long0>>,{{j\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:j\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.reductionLong(long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{j\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons2>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:j\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static long reductionLong(long[] x) {
    long sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static byte reductionByteM1(byte[] x) {
    byte sum = -1;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static short reductionShortM1(short[] x) {
    short sum = -1;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static char reductionCharM1(char[] x) {
    char sum = 0xffff;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  /// CHECK-START: int Main.reductionIntM1(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<ConsM1:i\d+>> IntConstant -1                loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<ConsM1>>,{{i\d+}}]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.reductionIntM1(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}           loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:i\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static int reductionIntM1(int[] x) {
    int sum = -1;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  /// CHECK-START: long Main.reductionLongM1(long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<LongM1:j\d+>> LongConstant -1               loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<LongM1>>,{{j\d+}}]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:j\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.reductionLongM1(long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{j\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecAdd [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons2>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:j\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static long reductionLongM1(long[] x) {
    long sum = -1L;
    for (int i = 0; i < x.length; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static byte reductionMinusByte(byte[] x) {
    byte sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum -= x[i];
    }
    return sum;
  }

  private static short reductionMinusShort(short[] x) {
    short sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum -= x[i];
    }
    return sum;
  }

  private static char reductionMinusChar(char[] x) {
    char sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum -= x[i];
    }
    return sum;
  }

  /// CHECK-START: int Main.reductionMinusInt(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Sub [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: int Main.reductionMinusInt(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons:i\d+>>   IntConstant {{2|4}}           loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecSub [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:i\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static int reductionMinusInt(int[] x) {
    int sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum -= x[i];
    }
    return sum;
  }

  /// CHECK-START: long Main.reductionMinusLong(long[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                 loop:none
  /// CHECK-DAG: <<Long0:j\d+>>  LongConstant 0                loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<Long0>>,{{j\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:j\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Sub [<<Phi2>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Return [<<Phi2>>]             loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.reductionMinusLong(long[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons2:i\d+>>  IntConstant 2                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [{{j\d+}}]      loop:none
  /// CHECK-DAG: <<Phi:d\d+>>    Phi [<<Set>>,{{d\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<I:i\d+>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 VecSub [<<Phi>>,<<Load>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<I>>,<<Cons2>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>    VecReduce [<<Phi>>]           loop:none
  /// CHECK-DAG: <<Extr:j\d+>>   VecExtractScalar [<<Red>>]    loop:none
  private static long reductionMinusLong(long[] x) {
    long sum = 0;
    for (int i = 0; i < x.length; i++) {
      sum -= x[i];
    }
    return sum;
  }

  //
  // A few special cases.
  //

  // TODO: consider unrolling

  private static int reductionInt10(int[] x) {
    int sum = 0;
    // Amenable to complete unrolling.
    for (int i = 10; i <= 10; i++) {
      sum += x[i];
    }
    return sum;
  }

  private static int reductionMinusInt10(int[] x) {
    int sum = 0;
    // Amenable to complete unrolling.
    for (int i = 10; i <= 10; i++) {
      sum -= x[i];
    }
    return sum;
  }

  //
  // Main driver.
  //

  public static void main(String[] args) {
    byte[] xb = new byte[N];
    short[] xs = new short[N];
    char[] xc = new char[N];
    int[] xi = new int[N];
    long[] xl = new long[N];
    for (int i = 0, k = -17; i < N; i++, k += 3) {
      xb[i] = (byte) k;
      xs[i] = (short) k;
      xc[i] = (char) k;
      xi[i] = k;
      xl[i] = k;
    }

    // Arrays with all positive elements.
    byte[] xpb = new byte[M];
    short[] xps = new short[M];
    char[] xpc = new char[M];
    int[] xpi = new int[M];
    long[] xpl = new long[M];
    for (int i = 0, k = 3; i < M; i++, k++) {
      xpb[i] = (byte) k;
      xps[i] = (short) k;
      xpc[i] = (char) k;
      xpi[i] = k;
      xpl[i] = k;
    }

    // Arrays with all negative elements.
    byte[] xnb = new byte[M];
    short[] xns = new short[M];
    int[] xni = new int[M];
    long[] xnl = new long[M];
    for (int i = 0, k = -103; i < M; i++, k++) {
      xnb[i] = (byte) k;
      xns[i] = (short) k;
      xni[i] = k;
      xnl[i] = k;
    }

    // Test various reductions in loops.
    int[] x0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int[] x1 = { 0, 0, 0, 1, 0, 0, 0, 0 };
    int[] x2 = { 1, 1, 1, 1, 0, 0, 0, 0 };
    expectEquals(-74, reductionByte(xb));
    expectEquals(-27466, reductionShort(xs));
    expectEquals(38070, reductionChar(xc));
    expectEquals(365750, reductionInt(xi));
    expectEquals(273, reductionIntChain());
    expectEquals(120, reductionIntToLoop(x0));
    expectEquals(121, reductionIntToLoop(x1));
    expectEquals(118, reductionIntToLoop(x2));
    expectEquals(-1310, reductionIntToLoop(xi));
    expectEquals(365750L, reductionLong(xl));
    expectEquals(-75, reductionByteM1(xb));
    expectEquals(-27467, reductionShortM1(xs));
    expectEquals(38069, reductionCharM1(xc));
    expectEquals(365749, reductionIntM1(xi));
    expectEquals(365749L, reductionLongM1(xl));
    expectEquals(74, reductionMinusByte(xb));
    expectEquals(27466, reductionMinusShort(xs));
    expectEquals(27466, reductionMinusChar(xc));
    expectEquals(-365750, reductionMinusInt(xi));
    expectEquals(365750L, reductionLong(xl));
    expectEquals(-75, reductionByteM1(xb));
    expectEquals(-27467, reductionShortM1(xs));
    expectEquals(38069, reductionCharM1(xc));
    expectEquals(365749, reductionIntM1(xi));
    expectEquals(365749L, reductionLongM1(xl));
    expectEquals(74, reductionMinusByte(xb));
    expectEquals(27466, reductionMinusShort(xs));
    expectEquals(27466, reductionMinusChar(xc));
    expectEquals(-365750, reductionMinusInt(xi));
    expectEquals(-365750L, reductionMinusLong(xl));

    // Test special cases.
    expectEquals(13, reductionInt10(xi));
    expectEquals(-13, reductionMinusInt10(xi));

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
