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

  private static final int N = 256;
  private static final int M = N * N + 15;

  static byte[] sB1 = new byte[M];
  static byte[] sB2 = new byte[M];
  static byte[] sBo = new byte[M];

  /// CHECK-START: void Main.halving_add_signed(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.halving_add_signed(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Int8 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void halving_add_signed(byte[] b1, byte[] b2, byte[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) ((b1[i] + b2[i]) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_unsigned(byte[], byte[], byte[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<I255:i\d+>> IntConstant 255                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<I255>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<I255>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<And1>>,<<And2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.halving_add_unsigned(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.halving_add_unsigned(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Uint8 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void halving_add_unsigned(byte[] b1, byte[] b2, byte[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) (((b1[i] & 0xff) + (b2[i] & 0xff)) >> 1);
    }
  }

  /// CHECK-START: void Main.rounding_halving_add_signed(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.rounding_halving_add_signed(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>] packed_type:Int8 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void rounding_halving_add_signed(byte[] b1, byte[] b2, byte[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) ((b1[i] + b2[i] + 1) >> 1);
    }
  }

  /// CHECK-START: void Main.rounding_halving_add_unsigned(byte[], byte[], byte[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<I255:i\d+>> IntConstant 255                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<I255>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<I255>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<And1>>,<<And2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.rounding_halving_add_unsigned(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>> Add [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Add1>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add2>>,<<I1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.rounding_halving_add_unsigned(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get1>>,<<Get2>>]  packed_type:Uint8 rounded:true loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void rounding_halving_add_unsigned(byte[] b1, byte[] b2, byte[] bo) {
    int min_length = Math.min(bo.length, Math.min(b1.length, b2.length));
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) (((b1[i] & 0xff) + (b2[i] & 0xff) + 1) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_signed_constant(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<I127:i\d+>> IntConstant 127                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:b\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<I127>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.halving_add_signed_constant(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<I127:i\d+>> IntConstant 127                       loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<I127>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Int8 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void halving_add_signed_constant(byte[] b1, byte[] bo) {
    int min_length = Math.min(bo.length, b1.length);
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) ((b1[i] + 0x7f) >> 1);
    }
  }

  /// CHECK-START: void Main.halving_add_unsigned_constant(byte[], byte[]) instruction_simplifier (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<I255:i\d+>> IntConstant 255                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:b\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And:i\d+>>  And [<<Get>>,<<I255>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<And>>,<<I255>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.halving_add_unsigned_constant(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<I1:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<I255:i\d+>> IntConstant 255                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:a\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<I255>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Shr:i\d+>>  Shr [<<Add>>,<<I1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:b\d+>>  TypeConversion [<<Shr>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void Main.halving_add_unsigned_constant(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<I255:i\d+>> IntConstant 255                       loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<I255>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<HAdd:d\d+>> VecHalvingAdd [<<Get>>,<<Repl>>] packed_type:Uint8 rounded:false loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<HAdd>>] loop:<<Loop>>      outer_loop:none
  private static void halving_add_unsigned_constant(byte[] b1, byte[] bo) {
    int min_length = Math.min(bo.length, b1.length);
    for (int i = 0; i < min_length; i++) {
      bo[i] = (byte) (((b1[i] & 0xff) + 0xff) >> 1);
    }
  }

  public static void main(String[] args) {
    // Initialize cross-values to test all cases, and also
    // set up some extra values to exercise the cleanup loop.
    int k = 0;
    for (int i = 0; i < N; i++) {
      for (int j = 0; j < N; j++) {
        sB1[k] = (byte) i;
        sB2[k] = (byte) j;
        k++;
      }
    }
    for (int i = 0; i < 15; i++) {
      sB1[k] = (byte) i;
      sB2[k] = 100;
      k++;
    }
    expectEquals(k, M);

    // Test halving add idioms.
    halving_add_signed(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) ((sB1[i] + sB2[i]) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) (((sB1[i] & 0xff) + (sB2[i] & 0xff)) >> 1);
      expectEquals(e, sBo[i]);
    }
    rounding_halving_add_signed(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) ((sB1[i] + sB2[i] + 1) >> 1);
      expectEquals(e, sBo[i]);
    }
    rounding_halving_add_unsigned(sB1, sB2, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) (((sB1[i] & 0xff) + (sB2[i] & 0xff) + 1) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_signed_constant(sB1, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) ((sB1[i] + 0x7f) >> 1);
      expectEquals(e, sBo[i]);
    }
    halving_add_unsigned_constant(sB1, sBo);
    for (int i = 0; i < M; i++) {
      byte e = (byte) (((sB1[i] & 0xff) + 0xff) >> 1);
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
