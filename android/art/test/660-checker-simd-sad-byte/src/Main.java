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

  // TODO: lower precision still coming, b/64091002

  private static byte sadByte2Byte(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    byte sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(b1[i] - b2[i]);
    }
    return sad;
  }

  private static byte sadByte2ByteAlt(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    byte sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  private static byte sadByte2ByteAlt2(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    byte sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      int x = s - p;
      if (x < 0) x = -x;
      sad += x;
    }
    return sad;
  }

  private static short sadByte2Short(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(b1[i] - b2[i]);
    }
    return sad;
  }

  private static short sadByte2ShortAlt(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  private static short sadByte2ShortAlt2(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      int x = s - p;
      if (x < 0) x = -x;
      sad += x;
    }
    return sad;
  }

  /// CHECK-START: int Main.sadByte2Int(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadByte2Int(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons16:i\d+>> IntConstant 16                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons16>>]      loop:<<Loop>>      outer_loop:none
  private static int sadByte2Int(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(b1[i] - b2[i]);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadByte2IntAlt(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get2>>,<<Get1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadByte2IntAlt(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons16:i\d+>> IntConstant 16                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load2>>,<<Load1>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons16>>]      loop:<<Loop>>      outer_loop:none
  private static int sadByte2IntAlt(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  /// CHECK-START: int Main.sadByte2IntAlt2(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadByte2IntAlt2(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons16:i\d+>> IntConstant 16                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons16>>]      loop:<<Loop>>      outer_loop:none
  private static int sadByte2IntAlt2(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      byte s = b1[i];
      byte p = b2[i];
      int x = s - p;
      if (x < 0) x = -x;
      sad += x;
    }
    return sad;
  }

  /// CHECK-START: long Main.sadByte2Long(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadByte2Long(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons16:i\d+>> IntConstant 16                 loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons16>>]      loop:<<Loop>>      outer_loop:none
  private static long sadByte2Long(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      long x = b1[i];
      long y = b2[i];
      sad += Math.abs(x - y);
    }
    return sad;
  }

  /// CHECK-START: long Main.sadByte2LongAt1(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadByte2LongAt1(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons16:i\d+>> IntConstant 16                 loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons16>>]      loop:<<Loop>>      outer_loop:none
  private static long sadByte2LongAt1(byte[] b1, byte[] b2) {
    int min_length = Math.min(b1.length, b2.length);
    long sad = 1;  // starts at 1
    for (int i = 0; i < min_length; i++) {
      long x = b1[i];
      long y = b2[i];
      sad += Math.abs(x - y);
    }
    return sad;
  }

  public static void main(String[] args) {
    // Cross-test the two most extreme values individually.
    byte[] b1 = { 0, -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    byte[] b2 = { 0,  127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    expectEquals(-1, sadByte2Byte(b1, b2));
    expectEquals(-1, sadByte2Byte(b2, b1));
    expectEquals(-1, sadByte2ByteAlt(b1, b2));
    expectEquals(-1, sadByte2ByteAlt(b2, b1));
    expectEquals(-1, sadByte2ByteAlt2(b1, b2));
    expectEquals(-1, sadByte2ByteAlt2(b2, b1));
    expectEquals(255, sadByte2Short(b1, b2));
    expectEquals(255, sadByte2Short(b2, b1));
    expectEquals(255, sadByte2ShortAlt(b1, b2));
    expectEquals(255, sadByte2ShortAlt(b2, b1));
    expectEquals(255, sadByte2ShortAlt2(b1, b2));
    expectEquals(255, sadByte2ShortAlt2(b2, b1));
    expectEquals(255, sadByte2Int(b1, b2));
    expectEquals(255, sadByte2Int(b2, b1));
    expectEquals(255, sadByte2IntAlt(b1, b2));
    expectEquals(255, sadByte2IntAlt(b2, b1));
    expectEquals(255, sadByte2IntAlt2(b1, b2));
    expectEquals(255, sadByte2IntAlt2(b2, b1));
    expectEquals(255, sadByte2Long(b1, b2));
    expectEquals(255L, sadByte2Long(b2, b1));
    expectEquals(256L, sadByte2LongAt1(b1, b2));
    expectEquals(256L, sadByte2LongAt1(b2, b1));

    // Use cross-values to test all cases.
    // One for scalar cleanup.
    int n = 256;
    int m = n * n + 1;
    int k = 0;
    b1 = new byte[m];
    b2 = new byte[m];
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        b1[k] = (byte) i;
        b2[k] = (byte) j;
        k++;
      }
    }
    b1[k] = 10;
    b2[k] = 2;
    expectEquals(8, sadByte2Byte(b1, b2));
    expectEquals(8, sadByte2ByteAlt(b1, b2));
    expectEquals(8, sadByte2ByteAlt2(b1, b2));
    expectEquals(21768, sadByte2Short(b1, b2));
    expectEquals(21768, sadByte2ShortAlt(b1, b2));
    expectEquals(21768, sadByte2ShortAlt2(b1, b2));
    expectEquals(5592328, sadByte2Int(b1, b2));
    expectEquals(5592328, sadByte2IntAlt(b1, b2));
    expectEquals(5592328, sadByte2IntAlt2(b1, b2));
    expectEquals(5592328L, sadByte2Long(b1, b2));
    expectEquals(5592329L, sadByte2LongAt1(b1, b2));

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
