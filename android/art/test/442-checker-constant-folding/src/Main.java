/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.lang.reflect.Method;

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  public static void assertFalse(boolean condition) {
    if (condition) {
      throw new Error();
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertFloatEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertDoubleEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }


  // Wrappers around methods located in file TestCmp.smali.

  public int smaliCmpLongConstants() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLongConstants");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpGtFloatConstants() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtFloatConstants");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtFloatConstants() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtFloatConstants");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpGtDoubleConstants() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtDoubleConstants");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtDoubleConstants() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtDoubleConstants");
    return (Integer)m.invoke(null);
  }

  public int smaliCmpLongSameConstant() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLongSameConstant");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpGtFloatSameConstant() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtFloatSameConstant");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtFloatSameConstant() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtFloatSameConstant");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpGtDoubleSameConstant() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtDoubleSameConstant");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtDoubleSameConstant() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtDoubleSameConstant");
    return (Integer)m.invoke(null);
  }

  public int smaliCmpGtFloatConstantWithNaN() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtFloatConstantWithNaN");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtFloatConstantWithNaN() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtFloatConstantWithNaN");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpGtDoubleConstantWithNaN() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpGtDoubleConstantWithNaN");
    return (Integer)m.invoke(null);
  }
  public int smaliCmpLtDoubleConstantWithNaN() throws Exception {
    Method m = testCmp.getMethod("$opt$CmpLtDoubleConstantWithNaN");
    return (Integer)m.invoke(null);
  }

  public static int smaliIntAddition2() throws Exception {
    Method m = Class.forName("TestCmp").getMethod("IntAddition2");
    return (Integer)m.invoke(null);
  }
  public static int smaliIntAddition2AddAndMove() throws Exception {
    Method m = Class.forName("TestCmp").getMethod("IntAddition2AddAndMove");
    return (Integer)m.invoke(null);
  }
  public static int smaliJumpsAndConditionals(boolean cond) throws Exception {
    Method m = Class.forName("TestCmp").getMethod("JumpsAndConditionals", boolean.class);
    return (Integer)m.invoke(null, cond);
  }


  /**
   * Exercise constant folding on negation.
   */

  /// CHECK-START: int Main.IntNegation() constant_folding (before)
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Neg:i\d+>>      Neg [<<Const42>>]
  /// CHECK-DAG:                       Return [<<Neg>>]

  /// CHECK-START: int Main.IntNegation() constant_folding (after)
  /// CHECK-DAG:     <<ConstN42:i\d+>> IntConstant -42
  /// CHECK-DAG:                       Return [<<ConstN42>>]

  /// CHECK-START: int Main.IntNegation() constant_folding (after)
  /// CHECK-NOT:                       Neg

  public static int IntNegation() {
    int x, y;
    x = 42;
    y = -x;
    return y;
  }

  /// CHECK-START: long Main.LongNegation() constant_folding (before)
  /// CHECK-DAG:     <<Const42:j\d+>>  LongConstant 42
  /// CHECK-DAG:     <<Neg:j\d+>>      Neg [<<Const42>>]
  /// CHECK-DAG:                       Return [<<Neg>>]

  /// CHECK-START: long Main.LongNegation() constant_folding (after)
  /// CHECK-DAG:     <<ConstN42:j\d+>> LongConstant -42
  /// CHECK-DAG:                       Return [<<ConstN42>>]

  /// CHECK-START: long Main.LongNegation() constant_folding (after)
  /// CHECK-NOT:                       Neg

  public static long LongNegation() {
    long x, y;
    x = 42L;
    y = -x;
    return y;
  }

  /// CHECK-START: float Main.FloatNegation() constant_folding (before)
  /// CHECK-DAG:     <<Const42:f\d+>>  FloatConstant 42
  /// CHECK-DAG:     <<Neg:f\d+>>      Neg [<<Const42>>]
  /// CHECK-DAG:                       Return [<<Neg>>]

  /// CHECK-START: float Main.FloatNegation() constant_folding (after)
  /// CHECK-DAG:     <<ConstN42:f\d+>> FloatConstant -42
  /// CHECK-DAG:                       Return [<<ConstN42>>]

  /// CHECK-START: float Main.FloatNegation() constant_folding (after)
  /// CHECK-NOT:                       Neg

  public static float FloatNegation() {
    float x, y;
    x = 42F;
    y = -x;
    return y;
  }

  /// CHECK-START: double Main.DoubleNegation() constant_folding (before)
  /// CHECK-DAG:     <<Const42:d\d+>>  DoubleConstant 42
  /// CHECK-DAG:     <<Neg:d\d+>>      Neg [<<Const42>>]
  /// CHECK-DAG:                       Return [<<Neg>>]

  /// CHECK-START: double Main.DoubleNegation() constant_folding (after)
  /// CHECK-DAG:     <<ConstN42:d\d+>> DoubleConstant -42
  /// CHECK-DAG:                       Return [<<ConstN42>>]

  /// CHECK-START: double Main.DoubleNegation() constant_folding (after)
  /// CHECK-NOT:                       Neg

  public static double DoubleNegation() {
    double x, y;
    x = 42D;
    y = -x;
    return y;
  }


  /**
   * Exercise constant folding on addition.
   */

  /// CHECK-START: int Main.IntAddition1() constant_folding (before)
  /// CHECK-DAG:     <<Const1:i\d+>>  IntConstant 1
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Const1>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Add>>]

  /// CHECK-START: int Main.IntAddition1() constant_folding (after)
  /// CHECK-DAG:     <<Const3:i\d+>>  IntConstant 3
  /// CHECK-DAG:                      Return [<<Const3>>]

  /// CHECK-START: int Main.IntAddition1() constant_folding (after)
  /// CHECK-NOT:                      Add

  public static int IntAddition1() {
    int a, b, c;
    a = 1;
    b = 2;
    c = a + b;
    return c;
  }

  /// CHECK-START: int Main.IntAddition2() constant_folding (before)
  /// CHECK-DAG:     <<Const1:i\d+>>  IntConstant 1
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Const5:i\d+>>  IntConstant 5
  /// CHECK-DAG:     <<Const6:i\d+>>  IntConstant 6
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<Const1>>,<<Const2>>]
  /// CHECK-DAG:                      Add [<<Const5>>,<<Const6>>]

  /// CHECK-START: int Main.IntAddition2() constant_folding (after)
  /// CHECK-DAG:     <<Const14:i\d+>> IntConstant 14
  /// CHECK-DAG:                      Return [<<Const14>>]

  /// CHECK-START: int Main.IntAddition2() constant_folding (after)
  /// CHECK-NOT:                      Add

  public static int IntAddition2() {
    int a, b, c;
    a = 1;
    b = 2;
    a += b;
    b = 5;
    c = 6;
    b += c;
    c = a + b;
    return c;
  }

  /// CHECK-START: long Main.LongAddition() constant_folding (before)
  /// CHECK-DAG:     <<Const1:j\d+>>  LongConstant 1
  /// CHECK-DAG:     <<Const2:j\d+>>  LongConstant 2
  /// CHECK-DAG:     <<Add:j\d+>>     Add [<<Const1>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Add>>]

  /// CHECK-START: long Main.LongAddition() constant_folding (after)
  /// CHECK-DAG:     <<Const3:j\d+>>  LongConstant 3
  /// CHECK-DAG:                      Return [<<Const3>>]

  /// CHECK-START: long Main.LongAddition() constant_folding (after)
  /// CHECK-NOT:                      Add

  public static long LongAddition() {
    long a, b, c;
    a = 1L;
    b = 2L;
    c = a + b;
    return c;
  }

  /// CHECK-START: float Main.FloatAddition() constant_folding (before)
  /// CHECK-DAG:     <<Const1:f\d+>>  FloatConstant 1
  /// CHECK-DAG:     <<Const2:f\d+>>  FloatConstant 2
  /// CHECK-DAG:     <<Add:f\d+>>     Add [<<Const1>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Add>>]

  /// CHECK-START: float Main.FloatAddition() constant_folding (after)
  /// CHECK-DAG:     <<Const3:f\d+>>  FloatConstant 3
  /// CHECK-DAG:                      Return [<<Const3>>]

  /// CHECK-START: float Main.FloatAddition() constant_folding (after)
  /// CHECK-NOT:                      Add

  public static float FloatAddition() {
    float a, b, c;
    a = 1F;
    b = 2F;
    c = a + b;
    return c;
  }

  /// CHECK-START: double Main.DoubleAddition() constant_folding (before)
  /// CHECK-DAG:     <<Const1:d\d+>>  DoubleConstant 1
  /// CHECK-DAG:     <<Const2:d\d+>>  DoubleConstant 2
  /// CHECK-DAG:     <<Add:d\d+>>     Add [<<Const1>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Add>>]

  /// CHECK-START: double Main.DoubleAddition() constant_folding (after)
  /// CHECK-DAG:     <<Const3:d\d+>>  DoubleConstant 3
  /// CHECK-DAG:                      Return [<<Const3>>]

  /// CHECK-START: double Main.DoubleAddition() constant_folding (after)
  /// CHECK-NOT:                      Add

  public static double DoubleAddition() {
    double a, b, c;
    a = 1D;
    b = 2D;
    c = a + b;
    return c;
  }


  /**
   * Exercise constant folding on subtraction.
   */

  /// CHECK-START: int Main.IntSubtraction() constant_folding (before)
  /// CHECK-DAG:     <<Const6:i\d+>>  IntConstant 6
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Sub:i\d+>>     Sub [<<Const6>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Sub>>]

  /// CHECK-START: int Main.IntSubtraction() constant_folding (after)
  /// CHECK-DAG:     <<Const4:i\d+>>  IntConstant 4
  /// CHECK-DAG:                      Return [<<Const4>>]

  /// CHECK-START: int Main.IntSubtraction() constant_folding (after)
  /// CHECK-NOT:                      Sub

  public static int IntSubtraction() {
    int a, b, c;
    a = 6;
    b = 2;
    c = a - b;
    return c;
  }

  /// CHECK-START: long Main.LongSubtraction() constant_folding (before)
  /// CHECK-DAG:     <<Const6:j\d+>>  LongConstant 6
  /// CHECK-DAG:     <<Const2:j\d+>>  LongConstant 2
  /// CHECK-DAG:     <<Sub:j\d+>>     Sub [<<Const6>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Sub>>]

  /// CHECK-START: long Main.LongSubtraction() constant_folding (after)
  /// CHECK-DAG:     <<Const4:j\d+>>  LongConstant 4
  /// CHECK-DAG:                      Return [<<Const4>>]

  /// CHECK-START: long Main.LongSubtraction() constant_folding (after)
  /// CHECK-NOT:                      Sub

  public static long LongSubtraction() {
    long a, b, c;
    a = 6L;
    b = 2L;
    c = a - b;
    return c;
  }

  /// CHECK-START: float Main.FloatSubtraction() constant_folding (before)
  /// CHECK-DAG:     <<Const6:f\d+>>  FloatConstant 6
  /// CHECK-DAG:     <<Const2:f\d+>>  FloatConstant 2
  /// CHECK-DAG:     <<Sub:f\d+>>     Sub [<<Const6>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Sub>>]

  /// CHECK-START: float Main.FloatSubtraction() constant_folding (after)
  /// CHECK-DAG:     <<Const4:f\d+>>  FloatConstant 4
  /// CHECK-DAG:                      Return [<<Const4>>]

  /// CHECK-START: float Main.FloatSubtraction() constant_folding (after)
  /// CHECK-NOT:                      Sub

  public static float FloatSubtraction() {
    float a, b, c;
    a = 6F;
    b = 2F;
    c = a - b;
    return c;
  }

  /// CHECK-START: double Main.DoubleSubtraction() constant_folding (before)
  /// CHECK-DAG:     <<Const6:d\d+>>  DoubleConstant 6
  /// CHECK-DAG:     <<Const2:d\d+>>  DoubleConstant 2
  /// CHECK-DAG:     <<Sub:d\d+>>     Sub [<<Const6>>,<<Const2>>]
  /// CHECK-DAG:                      Return [<<Sub>>]

  /// CHECK-START: double Main.DoubleSubtraction() constant_folding (after)
  /// CHECK-DAG:     <<Const4:d\d+>>  DoubleConstant 4
  /// CHECK-DAG:                      Return [<<Const4>>]

  /// CHECK-START: double Main.DoubleSubtraction() constant_folding (after)
  /// CHECK-NOT:                      Sub

  public static double DoubleSubtraction() {
    double a, b, c;
    a = 6D;
    b = 2D;
    c = a - b;
    return c;
  }


  /**
   * Exercise constant folding on multiplication.
   */

  /// CHECK-START: int Main.IntMultiplication() constant_folding (before)
  /// CHECK-DAG:     <<Const7:i\d+>>  IntConstant 7
  /// CHECK-DAG:     <<Const3:i\d+>>  IntConstant 3
  /// CHECK-DAG:     <<Mul:i\d+>>     Mul [<<Const7>>,<<Const3>>]
  /// CHECK-DAG:                      Return [<<Mul>>]

  /// CHECK-START: int Main.IntMultiplication() constant_folding (after)
  /// CHECK-DAG:     <<Const21:i\d+>> IntConstant 21
  /// CHECK-DAG:                      Return [<<Const21>>]

  /// CHECK-START: int Main.IntMultiplication() constant_folding (after)
  /// CHECK-NOT:                      Mul

  public static int IntMultiplication() {
    int a, b, c;
    a = 7;
    b = 3;
    c = a * b;
    return c;
  }

  /// CHECK-START: long Main.LongMultiplication() constant_folding (before)
  /// CHECK-DAG:     <<Const7:j\d+>>  LongConstant 7
  /// CHECK-DAG:     <<Const3:j\d+>>  LongConstant 3
  /// CHECK-DAG:     <<Mul:j\d+>>     Mul [<<Const7>>,<<Const3>>]
  /// CHECK-DAG:                      Return [<<Mul>>]

  /// CHECK-START: long Main.LongMultiplication() constant_folding (after)
  /// CHECK-DAG:     <<Const21:j\d+>> LongConstant 21
  /// CHECK-DAG:                      Return [<<Const21>>]

  /// CHECK-START: long Main.LongMultiplication() constant_folding (after)
  /// CHECK-NOT:                      Mul

  public static long LongMultiplication() {
    long a, b, c;
    a = 7L;
    b = 3L;
    c = a * b;
    return c;
  }

  /// CHECK-START: float Main.FloatMultiplication() constant_folding (before)
  /// CHECK-DAG:     <<Const7:f\d+>>  FloatConstant 7
  /// CHECK-DAG:     <<Const3:f\d+>>  FloatConstant 3
  /// CHECK-DAG:     <<Mul:f\d+>>     Mul [<<Const7>>,<<Const3>>]
  /// CHECK-DAG:                      Return [<<Mul>>]

  /// CHECK-START: float Main.FloatMultiplication() constant_folding (after)
  /// CHECK-DAG:     <<Const21:f\d+>> FloatConstant 21
  /// CHECK-DAG:                      Return [<<Const21>>]

  /// CHECK-START: float Main.FloatMultiplication() constant_folding (after)
  /// CHECK-NOT:                      Mul

  public static float FloatMultiplication() {
    float a, b, c;
    a = 7F;
    b = 3F;
    c = a * b;
    return c;
  }

  /// CHECK-START: double Main.DoubleMultiplication() constant_folding (before)
  /// CHECK-DAG:     <<Const7:d\d+>>  DoubleConstant 7
  /// CHECK-DAG:     <<Const3:d\d+>>  DoubleConstant 3
  /// CHECK-DAG:     <<Mul:d\d+>>     Mul [<<Const7>>,<<Const3>>]
  /// CHECK-DAG:                      Return [<<Mul>>]

  /// CHECK-START: double Main.DoubleMultiplication() constant_folding (after)
  /// CHECK-DAG:     <<Const21:d\d+>> DoubleConstant 21
  /// CHECK-DAG:                      Return [<<Const21>>]

  /// CHECK-START: double Main.DoubleMultiplication() constant_folding (after)
  /// CHECK-NOT:                      Mul

  public static double DoubleMultiplication() {
    double a, b, c;
    a = 7D;
    b = 3D;
    c = a * b;
    return c;
  }


  /**
   * Exercise constant folding on division.
   */

  /// CHECK-START: int Main.IntDivision() constant_folding (before)
  /// CHECK-DAG:     <<Const8:i\d+>>   IntConstant 8
  /// CHECK-DAG:     <<Const3:i\d+>>   IntConstant 3
  /// CHECK-DAG:     <<Div0Chk:i\d+>>  DivZeroCheck [<<Const3>>]
  /// CHECK-DAG:     <<Div:i\d+>>      Div [<<Const8>>,<<Div0Chk>>]
  /// CHECK-DAG:                       Return [<<Div>>]

  /// CHECK-START: int Main.IntDivision() constant_folding (after)
  /// CHECK-DAG:     <<Const2:i\d+>>   IntConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: int Main.IntDivision() constant_folding (after)
  /// CHECK-NOT:                       DivZeroCheck
  /// CHECK-NOT:                       Div

  public static int IntDivision() {
    int a, b, c;
    a = 8;
    b = 3;
    c = a / b;
    return c;
  }

  /// CHECK-START: long Main.LongDivision() constant_folding (before)
  /// CHECK-DAG:     <<Const8:j\d+>>   LongConstant 8
  /// CHECK-DAG:     <<Const3:j\d+>>   LongConstant 3
  /// CHECK-DAG:     <<Div0Chk:j\d+>>  DivZeroCheck [<<Const3>>]
  /// CHECK-DAG:     <<Div:j\d+>>      Div [<<Const8>>,<<Div0Chk>>]
  /// CHECK-DAG:                       Return [<<Div>>]

  /// CHECK-START: long Main.LongDivision() constant_folding (after)
  /// CHECK-DAG:     <<Const2:j\d+>>   LongConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: long Main.LongDivision() constant_folding (after)
  /// CHECK-NOT:                       DivZeroCheck
  /// CHECK-NOT:                       Div

  public static long LongDivision() {
    long a, b, c;
    a = 8L;
    b = 3L;
    c = a / b;
    return c;
  }

  /// CHECK-START: float Main.FloatDivision() constant_folding (before)
  /// CHECK-DAG:     <<Const8:f\d+>>   FloatConstant 8
  /// CHECK-DAG:     <<Const2P5:f\d+>> FloatConstant 2.5
  /// CHECK-DAG:     <<Div:f\d+>>      Div [<<Const8>>,<<Const2P5>>]
  /// CHECK-DAG:                       Return [<<Div>>]

  /// CHECK-START: float Main.FloatDivision() constant_folding (after)
  /// CHECK-DAG:     <<Const3P2:f\d+>> FloatConstant 3.2
  /// CHECK-DAG:                       Return [<<Const3P2>>]

  /// CHECK-START: float Main.FloatDivision() constant_folding (after)
  /// CHECK-NOT:                       Div

  public static float FloatDivision() {
    float a, b, c;
    a = 8F;
    b = 2.5F;
    c = a / b;
    return c;
  }

  /// CHECK-START: double Main.DoubleDivision() constant_folding (before)
  /// CHECK-DAG:     <<Const8:d\d+>>   DoubleConstant 8
  /// CHECK-DAG:     <<Const2P5:d\d+>> DoubleConstant 2.5
  /// CHECK-DAG:     <<Div:d\d+>>      Div [<<Const8>>,<<Const2P5>>]
  /// CHECK-DAG:                       Return [<<Div>>]

  /// CHECK-START: double Main.DoubleDivision() constant_folding (after)
  /// CHECK-DAG:     <<Const3P2:d\d+>> DoubleConstant 3.2
  /// CHECK-DAG:                       Return [<<Const3P2>>]

  /// CHECK-START: double Main.DoubleDivision() constant_folding (after)
  /// CHECK-NOT:                       Div

  public static double DoubleDivision() {
    double a, b, c;
    a = 8D;
    b = 2.5D;
    c = a / b;
    return c;
  }


  /**
   * Exercise constant folding on remainder.
   */

  /// CHECK-START: int Main.IntRemainder() constant_folding (before)
  /// CHECK-DAG:     <<Const8:i\d+>>   IntConstant 8
  /// CHECK-DAG:     <<Const3:i\d+>>   IntConstant 3
  /// CHECK-DAG:     <<Div0Chk:i\d+>>  DivZeroCheck [<<Const3>>]
  /// CHECK-DAG:     <<Rem:i\d+>>      Rem [<<Const8>>,<<Div0Chk>>]
  /// CHECK-DAG:                       Return [<<Rem>>]

  /// CHECK-START: int Main.IntRemainder() constant_folding (after)
  /// CHECK-DAG:     <<Const2:i\d+>>   IntConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: int Main.IntRemainder() constant_folding (after)
  /// CHECK-NOT:                       DivZeroCheck
  /// CHECK-NOT:                       Rem

  public static int IntRemainder() {
    int a, b, c;
    a = 8;
    b = 3;
    c = a % b;
    return c;
  }

  /// CHECK-START: long Main.LongRemainder() constant_folding (before)
  /// CHECK-DAG:     <<Const8:j\d+>>   LongConstant 8
  /// CHECK-DAG:     <<Const3:j\d+>>   LongConstant 3
  /// CHECK-DAG:     <<Div0Chk:j\d+>>  DivZeroCheck [<<Const3>>]
  /// CHECK-DAG:     <<Rem:j\d+>>      Rem [<<Const8>>,<<Div0Chk>>]
  /// CHECK-DAG:                       Return [<<Rem>>]

  /// CHECK-START: long Main.LongRemainder() constant_folding (after)
  /// CHECK-DAG:     <<Const2:j\d+>>   LongConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: long Main.LongRemainder() constant_folding (after)
  /// CHECK-NOT:                       DivZeroCheck
  /// CHECK-NOT:                       Rem

  public static long LongRemainder() {
    long a, b, c;
    a = 8L;
    b = 3L;
    c = a % b;
    return c;
  }

  /// CHECK-START: float Main.FloatRemainder() constant_folding (before)
  /// CHECK-DAG:     <<Const8:f\d+>>   FloatConstant 8
  /// CHECK-DAG:     <<Const2P5:f\d+>> FloatConstant 2.5
  /// CHECK-DAG:     <<Rem:f\d+>>      Rem [<<Const8>>,<<Const2P5>>]
  /// CHECK-DAG:                       Return [<<Rem>>]

  /// CHECK-START: float Main.FloatRemainder() constant_folding (after)
  /// CHECK-DAG:     <<Const0P5:f\d+>> FloatConstant 0.5
  /// CHECK-DAG:                       Return [<<Const0P5>>]

  /// CHECK-START: float Main.FloatRemainder() constant_folding (after)
  /// CHECK-NOT:                       Rem

  public static float FloatRemainder() {
    float a, b, c;
    a = 8F;
    b = 2.5F;
    c = a % b;
    return c;
  }

  /// CHECK-START: double Main.DoubleRemainder() constant_folding (before)
  /// CHECK-DAG:     <<Const8:d\d+>>   DoubleConstant 8
  /// CHECK-DAG:     <<Const2P5:d\d+>> DoubleConstant 2.5
  /// CHECK-DAG:     <<Rem:d\d+>>      Rem [<<Const8>>,<<Const2P5>>]
  /// CHECK-DAG:                       Return [<<Rem>>]

  /// CHECK-START: double Main.DoubleRemainder() constant_folding (after)
  /// CHECK-DAG:     <<Const0P5:d\d+>> DoubleConstant 0.5
  /// CHECK-DAG:                       Return [<<Const0P5>>]

  /// CHECK-START: double Main.DoubleRemainder() constant_folding (after)
  /// CHECK-NOT:                       Rem

  public static double DoubleRemainder() {
    double a, b, c;
    a = 8D;
    b = 2.5D;
    c = a % b;
    return c;
  }


  /**
   * Exercise constant folding on left shift.
   */

  /// CHECK-START: int Main.ShlIntLong() constant_folding (before)
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Const2L:j\d+>>  LongConstant 2
  /// CHECK-DAG:     <<TypeConv:i\d+>> TypeConversion [<<Const2L>>]
  /// CHECK-DAG:     <<Shl:i\d+>>      Shl [<<Const1>>,<<TypeConv>>]
  /// CHECK-DAG:                       Return [<<Shl>>]

  /// CHECK-START: int Main.ShlIntLong() constant_folding (after)
  /// CHECK-DAG:     <<Const4:i\d+>>   IntConstant 4
  /// CHECK-DAG:                       Return [<<Const4>>]

  /// CHECK-START: int Main.ShlIntLong() constant_folding (after)
  /// CHECK-NOT:                       Shl

  public static int ShlIntLong() {
    int lhs = 1;
    long rhs = 2;
    return lhs << rhs;
  }

  /// CHECK-START: long Main.ShlLongInt() constant_folding (before)
  /// CHECK-DAG:     <<Const3L:j\d+>>  LongConstant 3
  /// CHECK-DAG:     <<Const2:i\d+>>   IntConstant 2
  /// CHECK-DAG:     <<Shl:j\d+>>      Shl [<<Const3L>>,<<Const2>>]
  /// CHECK-DAG:                       Return [<<Shl>>]

  /// CHECK-START: long Main.ShlLongInt() constant_folding (after)
  /// CHECK-DAG:     <<Const12L:j\d+>> LongConstant 12
  /// CHECK-DAG:                       Return [<<Const12L>>]

  /// CHECK-START: long Main.ShlLongInt() constant_folding (after)
  /// CHECK-NOT:                       Shl

  public static long ShlLongInt() {
    long lhs = 3;
    int rhs = 2;
    return lhs << rhs;
  }


  /**
   * Exercise constant folding on right shift.
   */

  /// CHECK-START: int Main.ShrIntLong() constant_folding (before)
  /// CHECK-DAG:     <<Const7:i\d+>>   IntConstant 7
  /// CHECK-DAG:     <<Const2L:j\d+>>  LongConstant 2
  /// CHECK-DAG:     <<TypeConv:i\d+>> TypeConversion [<<Const2L>>]
  /// CHECK-DAG:     <<Shr:i\d+>>      Shr [<<Const7>>,<<TypeConv>>]
  /// CHECK-DAG:                       Return [<<Shr>>]

  /// CHECK-START: int Main.ShrIntLong() constant_folding (after)
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:                       Return [<<Const1>>]

  /// CHECK-START: int Main.ShrIntLong() constant_folding (after)
  /// CHECK-NOT:                       Shr

  public static int ShrIntLong() {
    int lhs = 7;
    long rhs = 2;
    return lhs >> rhs;
  }

  /// CHECK-START: long Main.ShrLongInt() constant_folding (before)
  /// CHECK-DAG:     <<Const9L:j\d+>>  LongConstant 9
  /// CHECK-DAG:     <<Const2:i\d+>>   IntConstant 2
  /// CHECK-DAG:     <<Shr:j\d+>>      Shr [<<Const9L>>,<<Const2>>]
  /// CHECK-DAG:                       Return [<<Shr>>]

  /// CHECK-START: long Main.ShrLongInt() constant_folding (after)
  /// CHECK-DAG:     <<Const2L:j\d+>>  LongConstant 2
  /// CHECK-DAG:                       Return [<<Const2L>>]

  /// CHECK-START: long Main.ShrLongInt() constant_folding (after)
  /// CHECK-NOT:                       Shr

  public static long ShrLongInt() {
    long lhs = 9;
    int rhs = 2;
    return lhs >> rhs;
  }


  /**
   * Exercise constant folding on unsigned right shift.
   */

  /// CHECK-START: int Main.UShrIntLong() constant_folding (before)
  /// CHECK-DAG:     <<ConstM7:i\d+>>  IntConstant -7
  /// CHECK-DAG:     <<Const2L:j\d+>>  LongConstant 2
  /// CHECK-DAG:     <<TypeConv:i\d+>> TypeConversion [<<Const2L>>]
  /// CHECK-DAG:     <<UShr:i\d+>>     UShr [<<ConstM7>>,<<TypeConv>>]
  /// CHECK-DAG:                       Return [<<UShr>>]

  /// CHECK-START: int Main.UShrIntLong() constant_folding (after)
  /// CHECK-DAG:     <<ConstRes:i\d+>> IntConstant 1073741822
  /// CHECK-DAG:                       Return [<<ConstRes>>]

  /// CHECK-START: int Main.UShrIntLong() constant_folding (after)
  /// CHECK-NOT:                       UShr

  public static int UShrIntLong() {
    int lhs = -7;
    long rhs = 2;
    return lhs >>> rhs;
  }

  /// CHECK-START: long Main.UShrLongInt() constant_folding (before)
  /// CHECK-DAG:     <<ConstM9L:j\d+>> LongConstant -9
  /// CHECK-DAG:     <<Const2:i\d+>>   IntConstant 2
  /// CHECK-DAG:     <<UShr:j\d+>>     UShr [<<ConstM9L>>,<<Const2>>]
  /// CHECK-DAG:                       Return [<<UShr>>]

  /// CHECK-START: long Main.UShrLongInt() constant_folding (after)
  /// CHECK-DAG:     <<ConstRes:j\d+>> LongConstant 4611686018427387901
  /// CHECK-DAG:                       Return [<<ConstRes>>]

  /// CHECK-START: long Main.UShrLongInt() constant_folding (after)
  /// CHECK-NOT:                       UShr

  public static long UShrLongInt() {
    long lhs = -9;
    int rhs = 2;
    return lhs >>> rhs;
  }


  /**
   * Exercise constant folding on logical and.
   */

  /// CHECK-START: long Main.AndIntLong() constant_folding (before)
  /// CHECK-DAG:     <<Const10:i\d+>>  IntConstant 10
  /// CHECK-DAG:     <<Const3L:j\d+>>  LongConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const10>>]
  /// CHECK-DAG:     <<And:j\d+>>      And [<<TypeConv>>,<<Const3L>>]
  /// CHECK-DAG:                       Return [<<And>>]

  /// CHECK-START: long Main.AndIntLong() constant_folding (after)
  /// CHECK-DAG:     <<Const2:j\d+>>   LongConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: long Main.AndIntLong() constant_folding (after)
  /// CHECK-NOT:                       And

  public static long AndIntLong() {
    int lhs = 10;
    long rhs = 3;
    return lhs & rhs;
  }

  /// CHECK-START: long Main.AndLongInt() constant_folding (before)
  /// CHECK-DAG:     <<Const10L:j\d+>> LongConstant 10
  /// CHECK-DAG:     <<Const3:i\d+>>   IntConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const3>>]
  /// CHECK-DAG:     <<And:j\d+>>      And [<<TypeConv>>,<<Const10L>>]
  /// CHECK-DAG:                       Return [<<And>>]

  /// CHECK-START: long Main.AndLongInt() constant_folding (after)
  /// CHECK-DAG:     <<Const2:j\d+>>   LongConstant 2
  /// CHECK-DAG:                       Return [<<Const2>>]

  /// CHECK-START: long Main.AndLongInt() constant_folding (after)
  /// CHECK-NOT:                       And

  public static long AndLongInt() {
    long lhs = 10;
    int rhs = 3;
    return lhs & rhs;
  }


  /**
   * Exercise constant folding on logical or.
   */

  /// CHECK-START: long Main.OrIntLong() constant_folding (before)
  /// CHECK-DAG:     <<Const10:i\d+>>  IntConstant 10
  /// CHECK-DAG:     <<Const3L:j\d+>>  LongConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const10>>]
  /// CHECK-DAG:     <<Or:j\d+>>       Or [<<TypeConv>>,<<Const3L>>]
  /// CHECK-DAG:                       Return [<<Or>>]

  /// CHECK-START: long Main.OrIntLong() constant_folding (after)
  /// CHECK-DAG:     <<Const11:j\d+>>  LongConstant 11
  /// CHECK-DAG:                       Return [<<Const11>>]

  /// CHECK-START: long Main.OrIntLong() constant_folding (after)
  /// CHECK-NOT:                       Or

  public static long OrIntLong() {
    int lhs = 10;
    long rhs = 3;
    return lhs | rhs;
  }

  /// CHECK-START: long Main.OrLongInt() constant_folding (before)
  /// CHECK-DAG:     <<Const10L:j\d+>> LongConstant 10
  /// CHECK-DAG:     <<Const3:i\d+>>   IntConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const3>>]
  /// CHECK-DAG:     <<Or:j\d+>>       Or [<<TypeConv>>,<<Const10L>>]
  /// CHECK-DAG:                       Return [<<Or>>]

  /// CHECK-START: long Main.OrLongInt() constant_folding (after)
  /// CHECK-DAG:     <<Const11:j\d+>>  LongConstant 11
  /// CHECK-DAG:                       Return [<<Const11>>]

  /// CHECK-START: long Main.OrLongInt() constant_folding (after)
  /// CHECK-NOT:                       Or

  public static long OrLongInt() {
    long lhs = 10;
    int rhs = 3;
    return lhs | rhs;
  }


  /**
   * Exercise constant folding on logical exclusive or.
   */

  /// CHECK-START: long Main.XorIntLong() constant_folding (before)
  /// CHECK-DAG:     <<Const10:i\d+>>  IntConstant 10
  /// CHECK-DAG:     <<Const3L:j\d+>>  LongConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const10>>]
  /// CHECK-DAG:     <<Xor:j\d+>>      Xor [<<TypeConv>>,<<Const3L>>]
  /// CHECK-DAG:                       Return [<<Xor>>]

  /// CHECK-START: long Main.XorIntLong() constant_folding (after)
  /// CHECK-DAG:     <<Const9:j\d+>>   LongConstant 9
  /// CHECK-DAG:                       Return [<<Const9>>]

  /// CHECK-START: long Main.XorIntLong() constant_folding (after)
  /// CHECK-NOT:                       Xor

  public static long XorIntLong() {
    int lhs = 10;
    long rhs = 3;
    return lhs ^ rhs;
  }

  /// CHECK-START: long Main.XorLongInt() constant_folding (before)
  /// CHECK-DAG:     <<Const10L:j\d+>> LongConstant 10
  /// CHECK-DAG:     <<Const3:i\d+>>   IntConstant 3
  /// CHECK-DAG:     <<TypeConv:j\d+>> TypeConversion [<<Const3>>]
  /// CHECK-DAG:     <<Xor:j\d+>>      Xor [<<TypeConv>>,<<Const10L>>]
  /// CHECK-DAG:                       Return [<<Xor>>]

  /// CHECK-START: long Main.XorLongInt() constant_folding (after)
  /// CHECK-DAG:     <<Const9:j\d+>>   LongConstant 9
  /// CHECK-DAG:                       Return [<<Const9>>]

  /// CHECK-START: long Main.XorLongInt() constant_folding (after)
  /// CHECK-NOT:                       Xor

  public static long XorLongInt() {
    long lhs = 10;
    int rhs = 3;
    return lhs ^ rhs;
  }


  /**
   * Exercise constant folding on constant (static) condition.
   */

  /// CHECK-START: int Main.StaticCondition() constant_folding (before)
  /// CHECK-DAG:     <<Const7:i\d+>>  IntConstant 7
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Cond:z\d+>>    GreaterThanOrEqual [<<Const7>>,<<Const2>>]
  /// CHECK-DAG:                      If [<<Cond>>]

  /// CHECK-START: int Main.StaticCondition() constant_folding (after)
  /// CHECK-DAG:     <<Const1:i\d+>>  IntConstant 1
  /// CHECK-DAG:                      If [<<Const1>>]

  /// CHECK-START: int Main.StaticCondition() constant_folding (after)
  /// CHECK-NOT:                      GreaterThanOrEqual

  public static int StaticCondition() {
    int a, b, c;
    a = 7;
    b = 2;
    if (a < b)
      c = a + b;
    else
      c = a - b;
    return c;
  }


  /**
   * Exercise constant folding on constant (static) condition for null references.
   */

  /// CHECK-START: int Main.StaticConditionNulls() constant_folding$after_inlining (before)
  /// CHECK-DAG:     <<Null:l\d+>>    NullConstant
  /// CHECK-DAG:     <<Cond:z\d+>>    NotEqual [<<Null>>,<<Null>>]
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<Cond>>]

  /// CHECK-START: int Main.StaticConditionNulls() constant_folding$after_inlining (after)
  /// CHECK-DAG:     <<Const0:i\d+>>  IntConstant 0
  /// CHECK-DAG:                      Select [{{i\d+}},{{i\d+}},<<Const0>>]

  /// CHECK-START: int Main.StaticConditionNulls() constant_folding$after_inlining (after)
  /// CHECK-NOT:                      NotEqual

  private static Object getNull() {
    return null;
  }

  public static int StaticConditionNulls() {
    Object a = getNull();
    Object b = getNull();
    return (a == b) ? 5 : 2;
  }


  /**
   * Exercise constant folding on a program with condition
   * (i.e. jumps) leading to the creation of many blocks.
   *
   * The intent of this test is to ensure that all constant expressions
   * are actually evaluated at compile-time, thanks to the reverse
   * (forward) post-order traversal of the the dominator tree.
   */

  /// CHECK-START: int Main.JumpsAndConditionals(boolean) constant_folding (before)
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Const5:i\d+>>  IntConstant 5
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Const5>>,<<Const2>>]
  /// CHECK-DAG:     <<Sub:i\d+>>     Sub [<<Const5>>,<<Const2>>]
  /// CHECK-DAG:     <<Phi:i\d+>>     Phi [<<Add>>,<<Sub>>]
  /// CHECK-DAG:                      Return [<<Phi>>]

  /// CHECK-START: int Main.JumpsAndConditionals(boolean) constant_folding (after)
  /// CHECK-DAG:     <<Const3:i\d+>>  IntConstant 3
  /// CHECK-DAG:     <<Const7:i\d+>>  IntConstant 7
  /// CHECK-DAG:     <<Phi:i\d+>>     Phi [<<Const7>>,<<Const3>>]
  /// CHECK-DAG:                      Return [<<Phi>>]

  /// CHECK-START: int Main.JumpsAndConditionals(boolean) constant_folding (after)
  /// CHECK-NOT:                      Add
  /// CHECK-NOT:                      Sub

  public static int JumpsAndConditionals(boolean cond) {
    int a, b, c;
    a = 5;
    b = 2;
    if (cond)
      c = a + b;
    else
      c = a - b;
    return c;
  }


  /**
   * Test optimizations of arithmetic identities yielding a constant result.
   */

  /// CHECK-START: int Main.And0(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<And:i\d+>>      And [<<Arg>>,<<Const0>>]
  /// CHECK-DAG:                       Return [<<And>>]

  /// CHECK-START: int Main.And0(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.And0(int) constant_folding (after)
  /// CHECK-NOT:                       And

  public static int And0(int arg) {
    return arg & 0;
  }

  /// CHECK-START: long Main.Mul0(long) constant_folding (before)
  /// CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:     <<Mul:j\d+>>      Mul [<<Const0>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Mul>>]

  /// CHECK-START: long Main.Mul0(long) constant_folding (after)
  /// CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: long Main.Mul0(long) constant_folding (after)
  /// CHECK-NOT:                       Mul

  public static long Mul0(long arg) {
    return arg * 0;
  }

  /// CHECK-START: int Main.OrAllOnes(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<ConstF:i\d+>>   IntConstant -1
  /// CHECK-DAG:     <<Or:i\d+>>       Or [<<Arg>>,<<ConstF>>]
  /// CHECK-DAG:                       Return [<<Or>>]

  /// CHECK-START: int Main.OrAllOnes(int) constant_folding (after)
  /// CHECK-DAG:     <<ConstF:i\d+>>   IntConstant -1
  /// CHECK-DAG:                       Return [<<ConstF>>]

  /// CHECK-START: int Main.OrAllOnes(int) constant_folding (after)
  /// CHECK-NOT:                       Or

  public static int OrAllOnes(int arg) {
    return arg | -1;
  }

  /// CHECK-START: long Main.Rem0(long) constant_folding (before)
  /// CHECK-DAG:     <<Arg:j\d+>>           ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>        LongConstant 0
  /// CHECK-DAG:     <<DivZeroCheck:j\d+>>  DivZeroCheck [<<Arg>>]
  /// CHECK-DAG:     <<Rem:j\d+>>           Rem [<<Const0>>,<<DivZeroCheck>>]
  /// CHECK-DAG:                            Return [<<Rem>>]

  /// CHECK-START: long Main.Rem0(long) constant_folding (after)
  /// CHECK-DAG:     <<Const0:j\d+>>        LongConstant 0
  /// CHECK-DAG:                            Return [<<Const0>>]

  /// CHECK-START: long Main.Rem0(long) constant_folding (after)
  /// CHECK-NOT:                            Rem

  public static long Rem0(long arg) {
    return 0 % arg;
  }

  /// CHECK-START: int Main.Rem1(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Rem:i\d+>>      Rem [<<Arg>>,<<Const1>>]
  /// CHECK-DAG:                       Return [<<Rem>>]

  /// CHECK-START: int Main.Rem1(int) constant_folding (after)
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.Rem1(int) constant_folding (after)
  /// CHECK-NOT:                       Rem

  public static int Rem1(int arg) {
    return arg % 1;
  }

  /// CHECK-START: long Main.RemN1(long) constant_folding (before)
  /// CHECK-DAG:     <<Arg:j\d+>>           ParameterValue
  /// CHECK-DAG:     <<ConstN1:j\d+>>       LongConstant -1
  /// CHECK-DAG:     <<DivZeroCheck:j\d+>>  DivZeroCheck [<<ConstN1>>]
  /// CHECK-DAG:     <<Rem:j\d+>>           Rem [<<Arg>>,<<DivZeroCheck>>]
  /// CHECK-DAG:                            Return [<<Rem>>]

  /// CHECK-START: long Main.RemN1(long) constant_folding (after)
  /// CHECK-DAG:     <<Const0:j\d+>>        LongConstant 0
  /// CHECK-DAG:                            Return [<<Const0>>]

  /// CHECK-START: long Main.RemN1(long) constant_folding (after)
  /// CHECK-NOT:                            Rem

  public static long RemN1(long arg) {
    return arg % -1;
  }

  /// CHECK-START: int Main.Shl0(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Shl:i\d+>>      Shl [<<Const0>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Shl>>]

  /// CHECK-START: int Main.Shl0(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.Shl0(int) constant_folding (after)
  /// CHECK-NOT:                       Shl

  public static int Shl0(int arg) {
    return 0 << arg;
  }

  /// CHECK-START: long Main.ShlLong0WithInt(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0L:j\d+>>  LongConstant 0
  /// CHECK-DAG:     <<Shl:j\d+>>      Shl [<<Const0L>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Shl>>]

  /// CHECK-START: long Main.ShlLong0WithInt(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0L:j\d+>>  LongConstant 0
  /// CHECK-DAG:                       Return [<<Const0L>>]

  /// CHECK-START: long Main.ShlLong0WithInt(int) constant_folding (after)
  /// CHECK-NOT:                       Shl

  public static long ShlLong0WithInt(int arg) {
    long long_zero = 0;
    return long_zero << arg;
  }

  /// CHECK-START: long Main.Shr0(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:     <<Shr:j\d+>>      Shr [<<Const0>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Shr>>]

  /// CHECK-START: long Main.Shr0(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: long Main.Shr0(int) constant_folding (after)
  /// CHECK-NOT:                       Shr

  public static long Shr0(int arg) {
    return (long)0 >> arg;
  }

  /// CHECK-START: long Main.SubSameLong(long) constant_folding (before)
  /// CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
  /// CHECK-DAG:     <<Sub:j\d+>>      Sub [<<Arg>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Sub>>]

  /// CHECK-START: long Main.SubSameLong(long) constant_folding (after)
  /// CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: long Main.SubSameLong(long) constant_folding (after)
  /// CHECK-NOT:                       Sub

  public static long SubSameLong(long arg) {
    return arg - arg;
  }

  /// CHECK-START: int Main.UShr0(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<UShr:i\d+>>     UShr [<<Const0>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<UShr>>]

  /// CHECK-START: int Main.UShr0(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.UShr0(int) constant_folding (after)
  /// CHECK-NOT:                       UShr

  public static int UShr0(int arg) {
    return 0 >>> arg;
  }

  /// CHECK-START: int Main.XorSameInt(int) constant_folding (before)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Xor:i\d+>>      Xor [<<Arg>>,<<Arg>>]
  /// CHECK-DAG:                       Return [<<Xor>>]

  /// CHECK-START: int Main.XorSameInt(int) constant_folding (after)
  /// CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.XorSameInt(int) constant_folding (after)
  /// CHECK-NOT:                       Xor

  public static int XorSameInt(int arg) {
    return arg ^ arg;
  }

  /// CHECK-START: boolean Main.CmpFloatGreaterThanNaN(float) constant_folding (before)
  /// CHECK-DAG:     <<Arg:f\d+>>      ParameterValue
  /// CHECK-DAG:     <<ConstNan:f\d+>> FloatConstant nan
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       IntConstant 1
  /// CHECK-DAG:     <<Cmp:i\d+>>      Compare [<<Arg>>,<<ConstNan>>]
  /// CHECK-DAG:     <<Le:z\d+>>       LessThanOrEqual [<<Cmp>>,<<Const0>>]
  /// CHECK-DAG:                       If [<<Le>>]

  /// CHECK-START: boolean Main.CmpFloatGreaterThanNaN(float) constant_folding (after)
  /// CHECK-DAG:                       ParameterValue
  /// CHECK-DAG:                       FloatConstant nan
  /// CHECK-DAG:                       IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:                       If [<<Const1>>]

  /// CHECK-START: boolean Main.CmpFloatGreaterThanNaN(float) constant_folding (after)
  /// CHECK-NOT:                       Compare
  /// CHECK-NOT:                       LessThanOrEqual

  public static boolean CmpFloatGreaterThanNaN(float arg) {
    return arg > Float.NaN;
  }

  /// CHECK-START: boolean Main.CmpDoubleLessThanNaN(double) constant_folding (before)
  /// CHECK-DAG:     <<Arg:d\d+>>      ParameterValue
  /// CHECK-DAG:     <<ConstNan:d\d+>> DoubleConstant nan
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       IntConstant 1
  /// CHECK-DAG:     <<Cmp:i\d+>>      Compare [<<Arg>>,<<ConstNan>>]
  /// CHECK-DAG:     <<Ge:z\d+>>       GreaterThanOrEqual [<<Cmp>>,<<Const0>>]
  /// CHECK-DAG:                       If [<<Ge>>]

  /// CHECK-START: boolean Main.CmpDoubleLessThanNaN(double) constant_folding (after)
  /// CHECK-DAG:                       ParameterValue
  /// CHECK-DAG:                       DoubleConstant nan
  /// CHECK-DAG:                       IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:                       If [<<Const1>>]

  /// CHECK-START: boolean Main.CmpDoubleLessThanNaN(double) constant_folding (after)
  /// CHECK-NOT:                       Compare
  /// CHECK-NOT:                       GreaterThanOrEqual

  public static boolean CmpDoubleLessThanNaN(double arg) {
    return arg < Double.NaN;
  }


  /**
   * Exercise constant folding on type conversions.
   */

  /// CHECK-START: int Main.ReturnInt33() constant_folding (before)
  /// CHECK-DAG:     <<Const33:j\d+>>  LongConstant 33
  /// CHECK-DAG:     <<Convert:i\d+>>  TypeConversion [<<Const33>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: int Main.ReturnInt33() constant_folding (after)
  /// CHECK-DAG:     <<Const33:i\d+>>  IntConstant 33
  /// CHECK-DAG:                       Return [<<Const33>>]

  /// CHECK-START: int Main.ReturnInt33() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static int ReturnInt33() {
    long imm = 33L;
    return (int) imm;
  }

  /// CHECK-START: int Main.ReturnIntMax() constant_folding (before)
  /// CHECK-DAG:     <<ConstMax:f\d+>> FloatConstant 1e+34
  /// CHECK-DAG:     <<Convert:i\d+>>  TypeConversion [<<ConstMax>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: int Main.ReturnIntMax() constant_folding (after)
  /// CHECK-DAG:     <<ConstMax:i\d+>> IntConstant 2147483647
  /// CHECK-DAG:                       Return [<<ConstMax>>]

  /// CHECK-START: int Main.ReturnIntMax() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static int ReturnIntMax() {
    float imm = 1.0e34f;
    return (int) imm;
  }

  /// CHECK-START: int Main.ReturnInt0() constant_folding (before)
  /// CHECK-DAG:     <<ConstNaN:d\d+>> DoubleConstant nan
  /// CHECK-DAG:     <<Convert:i\d+>>  TypeConversion [<<ConstNaN>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: int Main.ReturnInt0() constant_folding (after)
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: int Main.ReturnInt0() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static int ReturnInt0() {
    double imm = Double.NaN;
    return (int) imm;
  }

  /// CHECK-START: long Main.ReturnLong33() constant_folding (before)
  /// CHECK-DAG:     <<Const33:i\d+>>  IntConstant 33
  /// CHECK-DAG:     <<Convert:j\d+>>  TypeConversion [<<Const33>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: long Main.ReturnLong33() constant_folding (after)
  /// CHECK-DAG:     <<Const33:j\d+>>  LongConstant 33
  /// CHECK-DAG:                       Return [<<Const33>>]

  /// CHECK-START: long Main.ReturnLong33() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static long ReturnLong33() {
    int imm = 33;
    return (long) imm;
  }

  /// CHECK-START: long Main.ReturnLong34() constant_folding (before)
  /// CHECK-DAG:     <<Const34:f\d+>>  FloatConstant 34
  /// CHECK-DAG:     <<Convert:j\d+>>  TypeConversion [<<Const34>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: long Main.ReturnLong34() constant_folding (after)
  /// CHECK-DAG:     <<Const34:j\d+>>  LongConstant 34
  /// CHECK-DAG:                       Return [<<Const34>>]

  /// CHECK-START: long Main.ReturnLong34() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static long ReturnLong34() {
    float imm = 34.0f;
    return (long) imm;
  }

  /// CHECK-START: long Main.ReturnLong0() constant_folding (before)
  /// CHECK-DAG:     <<ConstNaN:d\d+>> DoubleConstant nan
  /// CHECK-DAG:     <<Convert:j\d+>>  TypeConversion [<<ConstNaN>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: long Main.ReturnLong0() constant_folding (after)
  /// CHECK-DAG:     <<Const0:j\d+>>   LongConstant 0
  /// CHECK-DAG:                       Return [<<Const0>>]

  /// CHECK-START: long Main.ReturnLong0() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static long ReturnLong0() {
    double imm = -Double.NaN;
    return (long) imm;
  }

  /// CHECK-START: float Main.ReturnFloat33() constant_folding (before)
  /// CHECK-DAG:     <<Const33:i\d+>>  IntConstant 33
  /// CHECK-DAG:     <<Convert:f\d+>>  TypeConversion [<<Const33>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: float Main.ReturnFloat33() constant_folding (after)
  /// CHECK-DAG:     <<Const33:f\d+>>  FloatConstant 33
  /// CHECK-DAG:                       Return [<<Const33>>]

  /// CHECK-START: float Main.ReturnFloat33() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static float ReturnFloat33() {
    int imm = 33;
    return (float) imm;
  }

  /// CHECK-START: float Main.ReturnFloat34() constant_folding (before)
  /// CHECK-DAG:     <<Const34:j\d+>>  LongConstant 34
  /// CHECK-DAG:     <<Convert:f\d+>>  TypeConversion [<<Const34>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: float Main.ReturnFloat34() constant_folding (after)
  /// CHECK-DAG:     <<Const34:f\d+>>  FloatConstant 34
  /// CHECK-DAG:                       Return [<<Const34>>]

  /// CHECK-START: float Main.ReturnFloat34() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static float ReturnFloat34() {
    long imm = 34L;
    return (float) imm;
  }

  /// CHECK-START: float Main.ReturnFloat99P25() constant_folding (before)
  /// CHECK-DAG:     <<Const:d\d+>>    DoubleConstant 99.25
  /// CHECK-DAG:     <<Convert:f\d+>>  TypeConversion [<<Const>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: float Main.ReturnFloat99P25() constant_folding (after)
  /// CHECK-DAG:     <<Const:f\d+>>    FloatConstant 99.25
  /// CHECK-DAG:                       Return [<<Const>>]

  /// CHECK-START: float Main.ReturnFloat99P25() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static float ReturnFloat99P25() {
    double imm = 99.25;
    return (float) imm;
  }

  /// CHECK-START: double Main.ReturnDouble33() constant_folding (before)
  /// CHECK-DAG:     <<Const33:i\d+>>  IntConstant 33
  /// CHECK-DAG:     <<Convert:d\d+>>  TypeConversion [<<Const33>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: double Main.ReturnDouble33() constant_folding (after)
  /// CHECK-DAG:     <<Const33:d\d+>>  DoubleConstant 33
  /// CHECK-DAG:                       Return [<<Const33>>]

  public static double ReturnDouble33() {
    int imm = 33;
    return (double) imm;
  }

  /// CHECK-START: double Main.ReturnDouble34() constant_folding (before)
  /// CHECK-DAG:     <<Const34:j\d+>>  LongConstant 34
  /// CHECK-DAG:     <<Convert:d\d+>>  TypeConversion [<<Const34>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: double Main.ReturnDouble34() constant_folding (after)
  /// CHECK-DAG:     <<Const34:d\d+>>  DoubleConstant 34
  /// CHECK-DAG:                       Return [<<Const34>>]

  /// CHECK-START: double Main.ReturnDouble34() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static double ReturnDouble34() {
    long imm = 34L;
    return (double) imm;
  }

  /// CHECK-START: double Main.ReturnDouble99P25() constant_folding (before)
  /// CHECK-DAG:     <<Const:f\d+>>    FloatConstant 99.25
  /// CHECK-DAG:     <<Convert:d\d+>>  TypeConversion [<<Const>>]
  /// CHECK-DAG:                       Return [<<Convert>>]

  /// CHECK-START: double Main.ReturnDouble99P25() constant_folding (after)
  /// CHECK-DAG:     <<Const:d\d+>>    DoubleConstant 99.25
  /// CHECK-DAG:                       Return [<<Const>>]

  /// CHECK-START: double Main.ReturnDouble99P25() constant_folding (after)
  /// CHECK-NOT:                       TypeConversion

  public static double ReturnDouble99P25() {
    float imm = 99.25f;
    return (double) imm;
  }


  public static void main(String[] args) throws Exception {
    assertIntEquals(-42, IntNegation());
    assertLongEquals(-42L, LongNegation());
    assertFloatEquals(-42F, FloatNegation());
    assertDoubleEquals(-42D, DoubleNegation());

    assertIntEquals(3, IntAddition1());
    assertIntEquals(14, IntAddition2());
    assertIntEquals(14, smaliIntAddition2());
    assertIntEquals(14, smaliIntAddition2AddAndMove());
    assertLongEquals(3L, LongAddition());
    assertFloatEquals(3F, FloatAddition());
    assertDoubleEquals(3D, DoubleAddition());

    assertIntEquals(4, IntSubtraction());
    assertLongEquals(4L, LongSubtraction());
    assertFloatEquals(4F, FloatSubtraction());
    assertDoubleEquals(4D, DoubleSubtraction());

    assertIntEquals(21, IntMultiplication());
    assertLongEquals(21L, LongMultiplication());
    assertFloatEquals(21F, FloatMultiplication());
    assertDoubleEquals(21D, DoubleMultiplication());

    assertIntEquals(2, IntDivision());
    assertLongEquals(2L, LongDivision());
    assertFloatEquals(3.2F, FloatDivision());
    assertDoubleEquals(3.2D, DoubleDivision());

    assertIntEquals(2, IntRemainder());
    assertLongEquals(2L, LongRemainder());
    assertFloatEquals(0.5F, FloatRemainder());
    assertDoubleEquals(0.5D, DoubleRemainder());

    assertIntEquals(4, ShlIntLong());
    assertLongEquals(12L, ShlLongInt());

    assertIntEquals(1, ShrIntLong());
    assertLongEquals(2L, ShrLongInt());

    assertIntEquals(1073741822, UShrIntLong());
    assertLongEquals(4611686018427387901L, UShrLongInt());

    assertLongEquals(2, AndIntLong());
    assertLongEquals(2, AndLongInt());

    assertLongEquals(11, OrIntLong());
    assertLongEquals(11, OrLongInt());

    assertLongEquals(9, XorIntLong());
    assertLongEquals(9, XorLongInt());

    assertIntEquals(5, StaticCondition());
    assertIntEquals(5, StaticConditionNulls());

    assertIntEquals(7, JumpsAndConditionals(true));
    assertIntEquals(3, JumpsAndConditionals(false));
    assertIntEquals(7, smaliJumpsAndConditionals(true));
    assertIntEquals(3, smaliJumpsAndConditionals(false));

    int arbitrary = 123456;  // Value chosen arbitrarily.

    assertIntEquals(0, And0(arbitrary));
    assertLongEquals(0, Mul0(arbitrary));
    assertIntEquals(-1, OrAllOnes(arbitrary));
    assertLongEquals(0, Rem0(arbitrary));
    assertIntEquals(0, Rem1(arbitrary));
    assertLongEquals(0, RemN1(arbitrary));
    assertIntEquals(0, Shl0(arbitrary));
    assertLongEquals(0, ShlLong0WithInt(arbitrary));
    assertLongEquals(0, Shr0(arbitrary));
    assertLongEquals(0, SubSameLong(arbitrary));
    assertIntEquals(0, UShr0(arbitrary));
    assertIntEquals(0, XorSameInt(arbitrary));

    assertFalse(CmpFloatGreaterThanNaN(arbitrary));
    assertFalse(CmpDoubleLessThanNaN(arbitrary));

    Main main = new Main();
    assertIntEquals(1, main.smaliCmpLongConstants());
    assertIntEquals(-1, main.smaliCmpGtFloatConstants());
    assertIntEquals(-1, main.smaliCmpLtFloatConstants());
    assertIntEquals(-1, main.smaliCmpGtDoubleConstants());
    assertIntEquals(-1, main.smaliCmpLtDoubleConstants());

    assertIntEquals(0, main.smaliCmpLongSameConstant());
    assertIntEquals(0, main.smaliCmpGtFloatSameConstant());
    assertIntEquals(0, main.smaliCmpLtFloatSameConstant());
    assertIntEquals(0, main.smaliCmpGtDoubleSameConstant());
    assertIntEquals(0, main.smaliCmpLtDoubleSameConstant());

    assertIntEquals(1, main.smaliCmpGtFloatConstantWithNaN());
    assertIntEquals(-1, main.smaliCmpLtFloatConstantWithNaN());
    assertIntEquals(1, main.smaliCmpGtDoubleConstantWithNaN());
    assertIntEquals(-1, main.smaliCmpLtDoubleConstantWithNaN());

    assertIntEquals(33, ReturnInt33());
    assertIntEquals(2147483647, ReturnIntMax());
    assertIntEquals(0, ReturnInt0());

    assertLongEquals(33, ReturnLong33());
    assertLongEquals(34, ReturnLong34());
    assertLongEquals(0, ReturnLong0());

    assertFloatEquals(33, ReturnFloat33());
    assertFloatEquals(34, ReturnFloat34());
    assertFloatEquals(99.25f, ReturnFloat99P25());

    assertDoubleEquals(33, ReturnDouble33());
    assertDoubleEquals(34, ReturnDouble34());
    assertDoubleEquals(99.25, ReturnDouble99P25());
  }

  Main() throws ClassNotFoundException {
    testCmp = Class.forName("TestCmp");
  }

  private Class<?> testCmp;
}
