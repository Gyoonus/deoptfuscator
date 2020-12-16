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
 * Some special cases: parameters, constants, invariants, casted computations.
 */
public class Main {

  /// CHECK-START: int Main.sadShort2IntParamRight(short[], short) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Param:s\d+>>  ParameterValue                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get>>,<<Param>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntParamRight(short[], short) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Param:s\d+>>  ParameterValue                 loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<Param>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load>>,<<Rep>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntParamRight(short[] s, short param) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(s[i] - param);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntParamLeft(short[], short) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Param:s\d+>>  ParameterValue                 loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Param>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntParamLeft(short[], short) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Param:s\d+>>  ParameterValue                 loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<Param>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Rep>>,<<Load>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntParamLeft(short[] s, short param) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(param - s[i]);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntConstRight(short[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant -32767             loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>    Add [<<Get>>,<<ConsI>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Add>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntConstRight(short[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 32767              loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<ConsI>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load>>,<<Rep>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntConstRight(short[] s) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(s[i] - 32767);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntConstLeft(short[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 32767              loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<ConsI>>,<<Get>>]        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntConstLeft(short[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 32767              loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<ConsI>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Rep>>,<<Load>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntConstLeft(short[] s) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(32767 - s[i]);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntInvariantRight(short[], int) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get>>,<<Conv>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntInvariantRight(short[], int) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<Conv>>]  loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load>>,<<Rep>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntInvariantRight(short[] s, int val) {
    int sad = 0;
    short x = (short) (val + 1);
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(s[i] - x);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntInvariantLeft(short[], int) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Conv>>,<<Get>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntInvariantLeft(short[], int) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [{{i\d+}}]      loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<Conv>>]  loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Rep>>,<<Load>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntInvariantLeft(short[] s, int val) {
    int sad = 0;
    short x = (short) (val + 1);
    for (int i = 0; i < s.length; i++) {
      sad += Math.abs(x - s[i]);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntCastedExprRight(short[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 110                loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>    [<<Get>>,<<ConsI>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [<<Add>>]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Get>>,<<Conv>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntCastedExprRight(short[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 110                loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<ConsI>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>    VecAdd [<<Load>>,<<Rep>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Load>>,<<Add>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntCastedExprRight(short[] s) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      short x = (short) (s[i] + 110);  // narrower part sign extends
      sad += Math.abs(s[i] - x);
    }
    return sad;
  }

  /// CHECK-START: int Main.sadShort2IntCastedExprLeft(short[]) loop_optimization (before)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons1:i\d+>>  IntConstant 1                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 110                loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>    [<<Get>>,<<ConsI>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>>   TypeConversion [<<Add>>]       loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>    Sub [<<Conv>>,<<Get>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Intrin:i\d+>> InvokeStaticOrDirect [<<Sub>>] intrinsic:MathAbsInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi2>>,<<Intrin>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons1>>]       loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM64,MIPS64}: int Main.sadShort2IntCastedExprLeft(short[]) loop_optimization (after)
  /// CHECK-DAG: <<Cons0:i\d+>>  IntConstant 0                  loop:none
  /// CHECK-DAG: <<Cons8:i\d+>>  IntConstant 8                  loop:none
  /// CHECK-DAG: <<ConsI:i\d+>>  IntConstant 110                loop:none
  /// CHECK-DAG: <<Rep:d\d+>>    VecReplicateScalar [<<ConsI>>] loop:none
  /// CHECK-DAG: <<Set:d\d+>>    VecSetScalars [<<Cons0>>]      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>   Phi [<<Cons0>>,{{i\d+}}]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>   Phi [<<Set>>,{{d\d+}}]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>    VecAdd [<<Load>>,<<Rep>>]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<SAD:d\d+>>    VecSADAccumulate [<<Phi2>>,<<Add>>,<<Load>>] loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                 Add [<<Phi1>>,<<Cons8>>]       loop:<<Loop>>      outer_loop:none
  private static int sadShort2IntCastedExprLeft(short[] s) {
    int sad = 0;
    for (int i = 0; i < s.length; i++) {
      short x = (short) (s[i] + 110);  // narrower part sign extends
      sad += Math.abs(x - s[i]);
    }
    return sad;
  }

  public static void main(String[] args) {
    short[] interesting = {
      (short) 0x0000,
      (short) 0x0001,
      (short) 0x0002,
      (short) 0x0003,
      (short) 0x0004,
      (short) 0x1234,
      (short) 0x8000,
      (short) 0x8001,
      (short) 0x8002,
      (short) 0x8003,
      (short) 0x8004,
      (short) 0x8004,
      (short) 0x7000,
      (short) 0x7fff,
      (short) 0xf000,
      (short) 0xffff
    };
    short[] s = new short[64];
    for (int i = 0; i < 64; i++) {
      s[i] = interesting[i % interesting.length];
    }

    expectEquals(1067200, sadShort2IntParamRight(s, (short)-1));
    expectEquals(1067200, sadShort2IntParamRight(s, (short) 0));
    expectEquals(1067208, sadShort2IntParamRight(s, (short) 1));
    expectEquals(1067224, sadShort2IntParamRight(s, (short) 2));
    expectEquals(2635416, sadShort2IntParamRight(s, (short) 0x7fff));
    expectEquals(1558824, sadShort2IntParamRight(s, (short) 0x8000));

    expectEquals(1067200, sadShort2IntParamLeft(s, (short)-1));
    expectEquals(1067200, sadShort2IntParamLeft(s, (short) 0));
    expectEquals(1067208, sadShort2IntParamLeft(s, (short) 1));
    expectEquals(1067224, sadShort2IntParamLeft(s, (short) 2));
    expectEquals(2635416, sadShort2IntParamLeft(s, (short) 0x7fff));
    expectEquals(1558824, sadShort2IntParamLeft(s, (short) 0x8000));

    expectEquals(2635416, sadShort2IntConstRight(s));
    expectEquals(2635416, sadShort2IntConstLeft(s));

    expectEquals(1067200, sadShort2IntInvariantRight(s, -2));
    expectEquals(1067200, sadShort2IntInvariantRight(s, -1));
    expectEquals(1067208, sadShort2IntInvariantRight(s, 0));
    expectEquals(1067224, sadShort2IntInvariantRight(s, 1));
    expectEquals(2635416, sadShort2IntInvariantRight(s, 0x7ffe));
    expectEquals(1558824, sadShort2IntInvariantRight(s, 0x7fff));

    expectEquals(1067200, sadShort2IntInvariantLeft(s, -2));
    expectEquals(1067200, sadShort2IntInvariantLeft(s, -1));
    expectEquals(1067208, sadShort2IntInvariantLeft(s, 0));
    expectEquals(1067224, sadShort2IntInvariantLeft(s, 1));
    expectEquals(2635416, sadShort2IntInvariantLeft(s, 0x7ffe));
    expectEquals(1558824, sadShort2IntInvariantLeft(s, 0x7fff));

    expectEquals(268304, sadShort2IntCastedExprLeft(s));
    expectEquals(268304, sadShort2IntCastedExprRight(s));

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
