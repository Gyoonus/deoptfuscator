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
 * Tests for halving-add idiomatic vectorization.
 */
public class Main {

  private static final int N = 64 * 1024;
  private static final int M = N + 31;

  static char[] sB1 = new char[M];
  static char[] sB2 = new char[M];
  static char[] sBo = new char[M];

  /// CHECK-START: void Main.halving_add_unsigned(char[], char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void halving_add_unsigned(char[] b1, char[] b2, char[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) ((b1[i] + b2[i]) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_also_unsigned(char[], char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<IMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<And1>>,<<And2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.halving_add_also_unsigned(char[], char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  // Note: HAnd has no impact (already a zero extension).
  //
  private static void halving_add_also_unsigned(char[] b1, char[] b2, char[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) (((b1[i] & 0xffff) + (b2[i] & 0xffff)) >> 1);
    }
  }

  /// CHECK-START: void Main.rounding_halving_add_unsigned(char[], char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.rounding_halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.rounding_halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.rounding_halving_add_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void rounding_halving_add_unsigned(char[] b1, char[] b2, char[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) ((b1[i] + b2[i] + 1) >> 1);
    }
  }

  /// CHECK-START: void Main.rounding_halving_add_also_unsigned(char[], char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<IMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<And1>>,<<And2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.rounding_halving_add_also_unsigned(char[], char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.rounding_halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.rounding_halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.rounding_halving_add_also_unsigned(char[], char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  // Note: HAnd has no impact (already a zero extension).
  //
  private static void rounding_halving_add_also_unsigned(char[] b1, char[] b2, char[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) (((b1[i] & 0xffff) + (b2[i] & 0xffff) + 1) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_unsigned_constant(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:c\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<UMAX>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.halving_add_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.halving_add_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.halving_add_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  private static void halving_add_unsigned_constant(char[] b1, char[] bo) {
    int min_length = Math.min(bo.length, b1.length);
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) ((b1[i] + 0xffff) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_also_unsigned_constant(char[], char[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:c\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And:i\d+>>  And [<<Get>>,<<UMAX>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<And>>,<<UMAX>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.halving_add_also_unsigned_constant(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:c\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<UMAX>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:c\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.halving_add_also_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.halving_add_also_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-MIPS64: void Main.halving_add_also_unsigned_constant(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<UMAX:i\d+>> IntConstant 65535                     loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<UMAX>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint16 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  //
  // Note: HAnd has no impact (already a zero extension).
  //
  private static void halving_add_also_unsigned_constant(char[] b1, char[] bo) {
    int min_length = Math.min(bo.length, b1.length);
    for (int i = 0; i < min_length; i++) {
      bo[i] = (char) (((b1[i] & 0xffff) + 0xffff) >> 1);
    }
  }

  public static void main(String[] args) {
    // Some interesting values.
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
    // Initialize cross-values to test all cases, and also
    // set up some extra values to exercise the cleanup loop.
    for (int i = 0; i < M; i++) {
      sB1[i] = (char) i;
      sB2[i] = interesting[i & 7];
    }

    // Test halving add idioms.
    halving_add_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + sB2[i]) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_also_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + sB2[i]) >> 1);
      expectEquals(e, sBo[i]);
    }
    rounding_halving_add_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + sB2[i] + 1) >> 1);
      expectEquals(e, sBo[i]);
    }
    rounding_halving_add_also_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + sB2[i] + 1) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_unsigned_constant(sB1, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + 0xffff) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_also_unsigned_constant(sB1, sBo);
    for (int i = 0; i < M; i++) {
      char e = (char) ((sB1[i] + 0xffff) >> 1);
      expectEquals(e, sBo[i]);
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
