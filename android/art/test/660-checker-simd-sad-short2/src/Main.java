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
 *
 * Special case, char array that is first casted to short, forcing sign extension.
 */
public class Main {

  // TODO: lower precision still coming, b/64091002

  private static short sadCastedChar2Short(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(((short) s1[i]) - ((short) s2[i]));
    }
    return sad;
  }

  private static short sadCastedChar2ShortAlt(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      short s = (short) s1[i];
      short p = (short) s2[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  private static short sadCastedChar2ShortAlt2(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    short sad = 0;
    for (int i = 0; i < min_length; i++) {
      short s = (short) s1[i];
      short p = (short) s2[i];
      int x = s - p;
      if (x < 0) x = -x;
      sad += x;
    }
    return sad;
  }

  /// CHECK-START: int Main.sadCastedChar2Int(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC1:i\d+>>    BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC2:i\d+>>    BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<BC1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>   ArrayGet [{{l\d+}},<<BC2>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:s\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.sadCastedChar2Int(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadCastedChar2Int(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadCastedChar2Int(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      sad += Math.abs(((short) s1[i]) - ((short) s2[i]));
    }
    return sad;
  }

  /// CHECK-START: int Main.sadCastedChar2IntAlt(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC1:i\d+>>    BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC2:i\d+>>    BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<BC1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>   ArrayGet [{{l\d+}},<<BC2>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:s\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub1:i\d+>>   Sub [<<Cnv2>>,<<Cnv1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub2:i\d+>>   Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>   Phi [<<Sub2>>,<<Sub1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Phi3>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.sadCastedChar2IntAlt(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  // Note: Get1+Cnv1 not simplified yet due to env use of Get1 in NullCheck for s2[i].
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get2>>,<<Cnv1>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadCastedChar2IntAlt(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load2>>,<<Load1>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadCastedChar2IntAlt(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      short s = (short) s1[i];
      short p = (short) s2[i];
      sad += s >= p ? s - p : p - s;
    }
    return sad;
  }

  /// CHECK-START: int Main.sadCastedChar2IntAlt2(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC1:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC2:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<BC1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>   ArrayGet [{{l\d+}},<<BC2>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:s\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Neg:i\d+>>    Neg [<<Sub>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>   Phi [<<Sub>>,<<Neg>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Phi3>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.sadCastedChar2IntAlt2(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  // Note: Get1+Cnv1 not simplified yet due to env use of Get1 in NullCheck for s2[i].
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Cnv1>>,<<Get2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadCastedChar2IntAlt2(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadCastedChar2IntAlt2(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    int sad = 0;
    for (int i = 0; i < min_length; i++) {
      short s = (short) s1[i];
      short p = (short) s2[i];
      int x = s - p;
      if (x < 0) x = -x;
      sad += x;
    }
    return sad;
  }

  /// CHECK-START: long Main.sadCastedChar2Long(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC1:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC2:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<BC1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>   ArrayGet [{{l\d+}},<<BC2>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:s\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv3:j\d+>>   TypeConversion [<<Cnv1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv4:j\d+>>   TypeConversion [<<Cnv2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv3>>,<<Cnv4>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: long Main.sadCastedChar2Long(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadCastedChar2Long(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 0                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static long sadCastedChar2Long(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    long sad = 0;
    for (int i = 0; i < min_length; i++) {
      long x = (short) s1[i];
      long y = (short) s2[i];
      sad += Math.abs(x - y);
    }
    return sad;
  }

  /// CHECK-START: long Main.sadCastedChar2LongAt1(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC1:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<BC2:\i\d+>>   BoundsCheck [<<Phi1>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>   ArrayGet [{{l\d+}},<<BC1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>   ArrayGet [{{l\d+}},<<BC2>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:s\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:s\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv3:j\d+>>   TypeConversion [<<Cnv1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv4:j\d+>>   TypeConversion [<<Cnv2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv3>>,<<Cnv4>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: long Main.sadCastedChar2LongAt1(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:j\d+>>   Phi [<<ConsL>>,{{j\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>   ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv1:j\d+>>   TypeConversion [<<Get1>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv2:j\d+>>   TypeConversion [<<Get2>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:j\d+>>    Sub [<<Cnv1>>,<<Cnv2>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:j\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsLong loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: long Main.sadCastedChar2LongAt1(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsL:j\d+>>  LongConstant 1                 loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<ConsL>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>  VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load1>>,<<Load2>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static long sadCastedChar2LongAt1(char[] s1, char[] s2) {
    int min_length = Math.min(s1.length, s2.length);
    long sad = 1;  // starts at 1
    for (int i = 0; i < min_length; i++) {
      long x = (short) s1[i];
      long y = (short) s2[i];
      sad += Math.abs(x - y);
    }
    return sad;
  }

  public static void main(String[] args) {
    // Cross-test the two most extreme values individually.
    char[] s1 = { 0, 0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    char[] s2 = { 0, 0x7fff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    expectEquals(-1, sadCastedChar2Short(s1, s2));
    expectEquals(-1, sadCastedChar2Short(s2, s1));
    expectEquals(-1, sadCastedChar2ShortAlt(s1, s2));
    expectEquals(-1, sadCastedChar2ShortAlt(s2, s1));
    expectEquals(-1, sadCastedChar2ShortAlt2(s1, s2));
    expectEquals(-1, sadCastedChar2ShortAlt2(s2, s1));
    expectEquals(65535, sadCastedChar2Int(s1, s2));
    expectEquals(65535, sadCastedChar2Int(s2, s1));
    expectEquals(65535, sadCastedChar2IntAlt(s1, s2));
    expectEquals(65535, sadCastedChar2IntAlt(s2, s1));
    expectEquals(65535, sadCastedChar2IntAlt2(s1, s2));
    expectEquals(65535, sadCastedChar2IntAlt2(s2, s1));
    expectEquals(65535L, sadCastedChar2Long(s1, s2));
    expectEquals(65535L, sadCastedChar2Long(s2, s1));
    expectEquals(65536L, sadCastedChar2LongAt1(s1, s2));
    expectEquals(65536L, sadCastedChar2LongAt1(s2, s1));

    // Use cross-values to test all cases.
    char[] interesting = {
      (char) 0x0000,
      (char) 0x0001,
      (char) 0x0002,
      (char) 0x1234,
      (char) 0x8000,
      (char) 0x8001,
      (char) 0x7fff,
      (char) 0xffff
    };
    int n = interesting.length;
    int m = n * n + 1;
    s1 = new char[m];
    s2 = new char[m];
    int k = 0;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        s1[k] = interesting[i];
        s2[k] = interesting[j];
        k++;
      }
    }
    s1[k] = 10;
    s2[k] = 2;
    expectEquals(-18932, sadCastedChar2Short(s1, s2));
    expectEquals(-18932, sadCastedChar2ShortAlt(s1, s2));
    expectEquals(-18932, sadCastedChar2ShortAlt2(s1, s2));
    expectEquals(1291788, sadCastedChar2Int(s1, s2));
    expectEquals(1291788, sadCastedChar2IntAlt(s1, s2));
    expectEquals(1291788, sadCastedChar2IntAlt2(s1, s2));
    expectEquals(1291788L, sadCastedChar2Long(s1, s2));
    expectEquals(1291789L, sadCastedChar2LongAt1(s1, s2));

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
