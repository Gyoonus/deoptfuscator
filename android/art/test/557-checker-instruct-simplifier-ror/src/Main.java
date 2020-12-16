/*
 * Copyright (C) 2015 The Android Open Source Project
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

public class Main {

  public static void assertIntEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  public static void assertLongEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  /// CHECK-START: int Main.rotateIntegerRight(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Invoke:i\d+>>       InvokeStaticOrDirect intrinsic:IntegerRotateRight

  /// CHECK-START: int Main.rotateIntegerRight(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.rotateIntegerRight(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      LoadClass
  /// CHECK-NOT:      ClinitCheck
  /// CHECK-NOT:      InvokeStaticOrDirect
  public static int rotateIntegerRight(int value, int distance) {
    return java.lang.Integer.rotateRight(value, distance);
  }

  /// CHECK-START: int Main.rotateIntegerLeft(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Invoke:i\d+>>       InvokeStaticOrDirect intrinsic:IntegerRotateLeft

  /// CHECK-START: int Main.rotateIntegerLeft(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.rotateIntegerLeft(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      LoadClass
  /// CHECK-NOT:      ClinitCheck
  /// CHECK-NOT:      InvokeStaticOrDirect
  public static int rotateIntegerLeft(int value, int distance) {
    return java.lang.Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: long Main.rotateLongRight(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Invoke:j\d+>>       InvokeStaticOrDirect intrinsic:LongRotateRight

  /// CHECK-START: long Main.rotateLongRight(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rotateLongRight(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      LoadClass
  /// CHECK-NOT:      ClinitCheck
  /// CHECK-NOT:      InvokeStaticOrDirect
  public static long rotateLongRight(long value, int distance) {
    return java.lang.Long.rotateRight(value, distance);
  }

  /// CHECK-START: long Main.rotateLongLeft(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Invoke:j\d+>>       InvokeStaticOrDirect intrinsic:LongRotateLeft

  /// CHECK-START: long Main.rotateLongLeft(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rotateLongLeft(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      LoadClass
  /// CHECK-NOT:      ClinitCheck
  /// CHECK-NOT:      InvokeStaticOrDirect
  public static long rotateLongLeft(long value, int distance) {
    return java.lang.Long.rotateLeft(value, distance);
  }

  //  (i >>> #distance) | (i << #(reg_bits - distance))

  /// CHECK-START: int Main.ror_int_constant_c_c(int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Const30:i\d+>>      IntConstant 30
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Const30>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.ror_int_constant_c_c(int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_constant_c_c(int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_constant_c_c(int value) {
    return (value >>> 2) | (value << 30);
  }

  /// CHECK-START: int Main.ror_int_constant_c_c_0(int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_constant_c_c_0(int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_constant_c_c_0(int value) {
    return (value >>> 2) | (value << 62);
  }

  //  (j >>> #distance) | (j << #(reg_bits - distance))

  /// CHECK-START: long Main.ror_long_constant_c_c(long) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Const62:i\d+>>      IntConstant 62
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Const62>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.ror_long_constant_c_c(long) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.ror_long_constant_c_c(long) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static long ror_long_constant_c_c(long value) {
    return (value >>> 2) | (value << 62);
  }

  /// CHECK-START: long Main.ror_long_constant_c_c_0(long) instruction_simplifier (after)
  /// CHECK-NOT:      Ror
  public static long ror_long_constant_c_c_0(long value) {
    return (value >>> 2) | (value << 30);
  }

  //  (i >>> #distance) | (i << #-distance)

  /// CHECK-START: int Main.ror_int_constant_c_negc(int) instruction_simplifier$after_inlining (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<ConstNeg2:i\d+>>    IntConstant -2
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ConstNeg2>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.ror_int_constant_c_negc(int) instruction_simplifier$after_inlining (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_constant_c_negc(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_constant_c_negc(int value) {
    return (value >>> 2) | (value << $opt$inline$IntConstantM2());
  }

  // Hiding constants outside the range [0, 32) used for int shifts from Jack.
  // (Jack extracts only the low 5 bits.)
  public static int $opt$inline$IntConstantM2() { return -2; }

  //  (j >>> #distance) | (j << #-distance)

  /// CHECK-START: long Main.ror_long_constant_c_negc(long) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<ConstNeg2:i\d+>>    IntConstant -2
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Const2>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ConstNeg2>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.ror_long_constant_c_negc(long) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<Const2:i\d+>>       IntConstant 2
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Const2>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.ror_long_constant_c_negc(long) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static long ror_long_constant_c_negc(long value) {
    return (value >>> 2) | (value << -2);
  }

  //  (i >>> distance) | (i << (#reg_bits - distance)

  /// CHECK-START: int Main.ror_int_reg_v_csubv(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const32>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Sub>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.ror_int_reg_v_csubv(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_reg_v_csubv(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Sub
  public static int ror_int_reg_v_csubv(int value, int distance) {
    return (value >>> distance) | (value << (32 - distance));
  }

  //  (distance = x - y)
  //  (i >>> distance) | (i << (#reg_bits - distance)

  /// CHECK-START: int Main.ror_int_subv_csubv(int, int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK-DAG:      <<Sub32:i\d+>>        Sub [<<Const32>>,<<SubDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Sub32>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<SubDistance>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.ror_int_subv_csubv(int, int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<SubDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_subv_csubv(int, int, int) instruction_simplifier (after)
  /// CHECK:          Sub
  /// CHECK-NOT:      Sub

  /// CHECK-START: int Main.ror_int_subv_csubv(int, int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_subv_csubv(int value, int x, int y) {
    int distance = x - y;
    return (value >>> distance) | (value << (32 - distance));
  }

  /// CHECK-START: int Main.ror_int_subv_csubv_env(int, int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK-DAG:      <<Sub32:i\d+>>        Sub [<<Const32>>,<<SubDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<SubDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Sub32>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:          <<Add:i\d+>>          Add [<<Or>>,<<Sub32>>]
  /// CHECK:                                Return [<<Add>>]

  /// CHECK-START: int Main.ror_int_subv_csubv_env(int, int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK-DAG:      <<Sub32:i\d+>>        Sub [<<Const32>>,<<SubDistance>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<SubDistance>>]
  /// CHECK:          <<Add:i\d+>>          Add [<<Ror>>,<<Sub32>>]
  /// CHECK:                                Return [<<Add>>]

  /// CHECK-START: int Main.ror_int_subv_csubv_env(int, int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_subv_csubv_env(int value, int x, int y) {
    int distance = x - y;
    int bits_minus_dist = 32 - distance;
    return ((value >>> distance) | (value << bits_minus_dist)) + bits_minus_dist;
  }

  //  (j >>> distance) | (j << (#reg_bits - distance)

  /// CHECK-START: long Main.ror_long_reg_v_csubv(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const64:i\d+>>      IntConstant 64
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const64>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Sub>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.ror_long_reg_v_csubv(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.ror_long_reg_v_csubv(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Sub
  public static long ror_long_reg_v_csubv(long value, int distance) {
    return (value >>> distance) | (value << (64 - distance));
  }

  /// CHECK-START: long Main.ror_long_reg_v_csubv_0(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      Ror
  public static long ror_long_reg_v_csubv_0(long value, int distance) {
    return (value >>> distance) | (value << (32 - distance));
  }

  /// CHECK-START: long Main.ror_long_subv_csubv_0(long, int, int) instruction_simplifier (after)
  /// CHECK-NOT:      Ror
  public static long ror_long_subv_csubv_0(long value, int x, int y) {
    int distance = x - y;
    return (value >>> distance) | (value << (32 - distance));
  }

  //  (i >>> (#reg_bits - distance)) | (i << distance)

  /// CHECK-START: int Main.rol_int_reg_csubv_v(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const32>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Sub>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.rol_int_reg_csubv_v(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK:          <<Sub:i\d+>>          Sub [<<Const32>>,<<ArgDistance>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Sub>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.rol_int_reg_csubv_v(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int rol_int_reg_csubv_v(int value, int distance) {
    return (value >>> (32 - distance)) | (value << distance);
  }

  //  (distance = x - y)
  //  (i >>> (#reg_bits - distance)) | (i << distance)

  /// CHECK-START: int Main.rol_int_csubv_subv(int, int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK-DAG:      <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK-DAG:      <<Sub32:i\d+>>        Sub [<<Const32>>,<<SubDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<SubDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Sub32>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.rol_int_csubv_subv(int, int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgX:i\d+>>         ParameterValue
  /// CHECK:          <<ArgY:i\d+>>         ParameterValue
  /// CHECK:          <<Const32:i\d+>>      IntConstant 32
  /// CHECK:          <<SubDistance:i\d+>>  Sub [<<ArgX>>,<<ArgY>>]
  /// CHECK:          <<Sub:i\d+>>          Sub [<<Const32>>,<<SubDistance>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Sub>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.rol_int_csubv_subv(int, int, int) instruction_simplifier (after)
  /// CHECK:          Sub
  /// CHECK:          Sub

  /// CHECK-START: int Main.rol_int_csubv_subv(int, int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int rol_int_csubv_subv(int value, int x, int y) {
    int distance = x - y;
    return (value >>> (32 - distance)) | (value << distance);
  }

  //  (j >>> (#reg_bits - distance)) | (j << distance)

  /// CHECK-START: long Main.rol_long_reg_csubv_v(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const64:i\d+>>      IntConstant 64
  /// CHECK-DAG:      <<Sub:i\d+>>          Sub [<<Const64>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Sub>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.rol_long_reg_csubv_v(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Const64:i\d+>>      IntConstant 64
  /// CHECK:          <<Sub:i\d+>>          Sub [<<Const64>>,<<ArgDistance>>]
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Sub>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rol_long_reg_csubv_v(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static long rol_long_reg_csubv_v(long value, int distance) {
    return (value >>> (64 - distance)) | (value << distance);
  }

  /// CHECK-START: long Main.rol_long_reg_csubv_v_0(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      Ror
  public static long rol_long_reg_csubv_v_0(long value, int distance) {
    return (value >>> (32 - distance)) | (value << distance);
  }

  //  (i >>> distance) | (i << -distance) (i.e. libcore's Integer.rotateRight)

  /// CHECK-START: int Main.ror_int_reg_v_negv(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Neg>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.ror_int_reg_v_negv(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.ror_int_reg_v_negv(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Neg
  public static int ror_int_reg_v_negv(int value, int distance) {
    return (value >>> distance) | (value << -distance);
  }

  /// CHECK-START: int Main.ror_int_reg_v_negv_env(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<Neg>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:          <<Add:i\d+>>          Add [<<Or>>,<<Neg>>]
  /// CHECK:                                Return [<<Add>>]

  /// CHECK-START: int Main.ror_int_reg_v_negv_env(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Sub:i\d+>>          Sub [<<Ror>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Sub>>]

  /// CHECK-START: int Main.ror_int_reg_v_negv_env(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int ror_int_reg_v_negv_env(int value, int distance) {
    int neg_distance = -distance;
    return ((value >>> distance) | (value << neg_distance)) + neg_distance;
  }

  //  (j >>> distance) | (j << -distance) (i.e. libcore's Long.rotateRight)

  /// CHECK-START: long Main.ror_long_reg_v_negv(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<Neg>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<UShr>>,<<Shl>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.ror_long_reg_v_negv(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.ror_long_reg_v_negv(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  /// CHECK-NOT:      Neg
  public static long ror_long_reg_v_negv(long value, int distance) {
    return (value >>> distance) | (value << -distance);
  }

  //  (i << distance) | (i >>> -distance) (i.e. libcore's Integer.rotateLeft)

  /// CHECK-START: int Main.rol_int_reg_negv_v(int, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:i\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:i\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:i\d+>>           Or [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: int Main.rol_int_reg_negv_v(int, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:i\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:i\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: int Main.rol_int_reg_negv_v(int, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static int rol_int_reg_negv_v(int value, int distance) {
    return (value << distance) | (value >>> -distance);
  }

  //  (j << distance) | (j >>> -distance) (i.e. libcore's Long.rotateLeft)

  /// CHECK-START: long Main.rol_long_reg_negv_v(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Or:j\d+>>           Or [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Or>>]

  /// CHECK-START: long Main.rol_long_reg_negv_v(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rol_long_reg_negv_v(long, int) instruction_simplifier (after)
  /// CHECK-NOT:      UShr
  /// CHECK-NOT:      Shl
  public static long rol_long_reg_negv_v(long value, int distance) {
    return (value << distance) | (value >>> -distance);
  }

  //  (j << distance) + (j >>> -distance)

  /// CHECK-START: long Main.rol_long_reg_v_negv_add(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Add:j\d+>>          Add [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Add>>]

  /// CHECK-START: long Main.rol_long_reg_v_negv_add(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rol_long_reg_v_negv_add(long, int) instruction_simplifier (after)
  /// CHECK-NOT:  Add
  /// CHECK-NOT:  Shl
  /// CHECK-NOT:  UShr
  public static long rol_long_reg_v_negv_add(long value, int distance) {
    return (value << distance) + (value >>> -distance);
  }

  //  (j << distance) ^ (j >>> -distance)

  /// CHECK-START: long Main.rol_long_reg_v_negv_xor(long, int) instruction_simplifier (before)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK-DAG:      <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK-DAG:      <<UShr:j\d+>>         UShr [<<ArgValue>>,<<Neg>>]
  /// CHECK-DAG:      <<Shl:j\d+>>          Shl [<<ArgValue>>,<<ArgDistance>>]
  /// CHECK:          <<Xor:j\d+>>          Xor [<<Shl>>,<<UShr>>]
  /// CHECK:                                Return [<<Xor>>]

  /// CHECK-START: long Main.rol_long_reg_v_negv_xor(long, int) instruction_simplifier (after)
  /// CHECK:          <<ArgValue:j\d+>>     ParameterValue
  /// CHECK:          <<ArgDistance:i\d+>>  ParameterValue
  /// CHECK:          <<Neg:i\d+>>          Neg [<<ArgDistance>>]
  /// CHECK:          <<Ror:j\d+>>          Ror [<<ArgValue>>,<<Neg>>]
  /// CHECK:                                Return [<<Ror>>]

  /// CHECK-START: long Main.rol_long_reg_v_negv_xor(long, int) instruction_simplifier (after)
  /// CHECK-NOT:  Xor
  /// CHECK-NOT:  Shl
  /// CHECK-NOT:  UShr
  public static long rol_long_reg_v_negv_xor(long value, int distance) {
    return (value << distance) ^ (value >>> -distance);
  }

  public static void main(String[] args) {
    assertIntEquals(2, ror_int_constant_c_c(8));
    assertIntEquals(2, ror_int_constant_c_c_0(8));
    assertLongEquals(2L, ror_long_constant_c_c(8L));

    assertIntEquals(2, ror_int_constant_c_negc(8));
    assertLongEquals(2L, ror_long_constant_c_negc(8L));

    assertIntEquals(2, ror_int_reg_v_csubv(8, 2));
    assertLongEquals(2L, ror_long_reg_v_csubv(8L, 2));

    assertIntEquals(2, ror_int_subv_csubv(8, 2, 0));
    assertIntEquals(32, ror_int_subv_csubv_env(8, 2, 0));
    assertIntEquals(32, rol_int_csubv_subv(8, 2, 0));

    assertIntEquals(32, rol_int_reg_csubv_v(8, 2));
    assertLongEquals(32L, rol_long_reg_csubv_v(8L, 2));

    assertIntEquals(2, ror_int_reg_v_negv(8, 2));
    assertIntEquals(0, ror_int_reg_v_negv_env(8, 2));
    assertLongEquals(2L, ror_long_reg_v_negv(8L, 2));

    assertIntEquals(32, rol_int_reg_negv_v(8, 2));
    assertLongEquals(32L, rol_long_reg_negv_v(8L, 2));

    assertLongEquals(32L, rol_long_reg_v_negv_add(8L, 2));
    assertLongEquals(32L, rol_long_reg_v_negv_xor(8L, 2));
  }
}
