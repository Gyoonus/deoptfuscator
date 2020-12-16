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

  // Note #1: `javac` flips the conditions of If statements.
  // Note #2: In the optimizing compiler, the first input of Phi is always
  //          the fall-through path, i.e. the false branch.

  public static void assertBoolEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /*
   * Elementary test negating a boolean. Verifies that blocks are merged and
   * empty branches removed.
   */

  /// CHECK-START: boolean Main.BooleanNot(boolean) select_generator (before)
  /// CHECK-DAG:     <<Param:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:                       If [<<Param>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: boolean Main.BooleanNot(boolean) select_generator (before)
  /// CHECK:                           Goto
  /// CHECK:                           Goto
  /// CHECK:                           Goto
  /// CHECK-NOT:                       Goto

  /// CHECK-START: boolean Main.BooleanNot(boolean) select_generator (after)
  /// CHECK-DAG:     <<Param:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<NotParam:i\d+>> Select [<<Const1>>,<<Const0>>,<<Param>>]
  /// CHECK-DAG:                       Return [<<NotParam>>]

  /// CHECK-START: boolean Main.BooleanNot(boolean) select_generator (after)
  /// CHECK-NOT:                       If
  /// CHECK-NOT:                       Phi

  /// CHECK-START: boolean Main.BooleanNot(boolean) select_generator (after)
  /// CHECK:                           Goto
  /// CHECK-NOT:                       Goto

  public static boolean BooleanNot(boolean x) {
    return !x;
  }

  /*
   * Program which only delegates the condition, i.e. returns 1 when True
   * and 0 when False.
   */

  /// CHECK-START: boolean Main.GreaterThan(int, int) select_generator (before)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:                       If [<<Cond>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const0>>,<<Const1>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: boolean Main.GreaterThan(int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const0>>,<<Const1>>,<<Cond>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  public static boolean GreaterThan(int x, int y) {
    return (x <= y) ? false : true;
  }

  /*
   * Program which negates a condition, i.e. returns 0 when True
   * and 1 when False.
   */

  /// CHECK-START: boolean Main.LessThan(int, int) select_generator (before)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThanOrEqual [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:                       If [<<Cond>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: boolean Main.LessThan(int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThanOrEqual [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const1>>,<<Const0>>,<<Cond>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  public static boolean LessThan(int x, int y) {
    return (x < y) ? true : false;
  }

  /*
   * Program which further uses negated conditions.
   * Note that Phis are discovered retrospectively.
   */

  /// CHECK-START: boolean Main.ValuesOrdered(int, int, int) select_generator (before)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<CondXY:z\d+>>   GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:                       If [<<CondXY>>]
  /// CHECK-DAG:     <<CondYZ:z\d+>>   GreaterThan [<<ParamY>>,<<ParamZ>>]
  /// CHECK-DAG:                       If [<<CondYZ>>]
  /// CHECK-DAG:     <<CondXYZ:z\d+>>  NotEqual [<<PhiXY:i\d+>>,<<PhiYZ:i\d+>>]
  /// CHECK-DAG:                       If [<<CondXYZ>>]
  /// CHECK-DAG:                       Return [<<PhiXYZ:i\d+>>]
  /// CHECK-DAG:     <<PhiXY>>         Phi [<<Const1>>,<<Const0>>]
  /// CHECK-DAG:     <<PhiYZ>>         Phi [<<Const1>>,<<Const0>>]
  /// CHECK-DAG:     <<PhiXYZ>>        Phi [<<Const1>>,<<Const0>>]

  /// CHECK-START: boolean Main.ValuesOrdered(int, int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<CmpXY:z\d+>>    GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:     <<SelXY:i\d+>>    Select [<<Const1>>,<<Const0>>,<<CmpXY>>]
  /// CHECK-DAG:     <<CmpYZ:z\d+>>    GreaterThan [<<ParamY>>,<<ParamZ>>]
  /// CHECK-DAG:     <<SelYZ:i\d+>>    Select [<<Const1>>,<<Const0>>,<<CmpYZ>>]
  /// CHECK-DAG:     <<CmpXYZ:z\d+>>   NotEqual [<<SelXY>>,<<SelYZ>>]
  /// CHECK-DAG:     <<SelXYZ:i\d+>>   Select [<<Const1>>,<<Const0>>,<<CmpXYZ>>]
  /// CHECK-DAG:                       Return [<<SelXYZ>>]

  public static boolean ValuesOrdered(int x, int y, int z) {
    return (x <= y) == (y <= z);
  }

  /// CHECK-START: int Main.NegatedCondition(boolean) select_generator (before)
  /// CHECK-DAG:     <<Param:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:                       If [<<Param>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const42>>,<<Const43>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: int Main.NegatedCondition(boolean) select_generator (after)
  /// CHECK-DAG:     <<Param:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const43>>,<<Const42>>,<<Param>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.NegatedCondition(boolean) select_generator (after)
  /// CHECK-NOT:                       BooleanNot

  public static int NegatedCondition(boolean x) {
    if (x != false) {
      return 42;
    } else {
      return 43;
    }
  }

  /// CHECK-START: int Main.SimpleTrueBlock(boolean, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<Add:i\d+>>      Add [<<ParamY>>,<<Const42>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const43>>,<<Add>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleTrueBlock(boolean, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleTrueBlock(boolean x, int y) {
    return x ? y + 42 : 43;
  }

  /// CHECK-START: int Main.SimpleFalseBlock(boolean, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<Add:i\d+>>      Add [<<ParamY>>,<<Const43>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Add>>,<<Const42>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleFalseBlock(boolean, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleFalseBlock(boolean x, int y) {
    return x ? 42 : y + 43;
  }

  /// CHECK-START: int Main.SimpleBothBlocks(boolean, int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<AddTrue:i\d+>>  Add [<<ParamY>>,<<Const42>>]
  /// CHECK-DAG:     <<AddFalse:i\d+>> Add [<<ParamZ>>,<<Const43>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<AddFalse>>,<<AddTrue>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleBothBlocks(boolean, int, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleBothBlocks(boolean x, int y, int z) {
    return x ? y + 42 : z + 43;
  }

  /// CHECK-START: int Main.ThreeBlocks(boolean, boolean) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<ParamY:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
  /// CHECK-DAG:     <<Const2:i\d+>>    IntConstant 2
  /// CHECK-DAG:     <<Const3:i\d+>>    IntConstant 3
  /// CHECK-DAG:     <<Select23:i\d+>>  Select [<<Const3>>,<<Const2>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select123:i\d+>> Select [<<Select23>>,<<Const1>>,<<ParamX>>]
  /// CHECK-DAG:                        Return [<<Select123>>]

  public static int ThreeBlocks(boolean x, boolean y) {
    if (x) {
      return 1;
    } else if (y) {
      return 2;
    } else {
      return 3;
    }
  }

  /// CHECK-START: int Main.MultiplePhis() select_generator (before)
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Const13:i\d+>>  IntConstant 13
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<PhiX:i\d+>>     Phi [<<Const0>>,<<Const13>>,<<Const42>>]
  /// CHECK-DAG:     <<PhiY:i\d+>>     Phi [<<Const1>>,<<Add:i\d+>>,<<Add>>]
  /// CHECK-DAG:     <<Add>>           Add [<<PhiY>>,<<Const1>>]
  /// CHECK-DAG:     <<Cond:z\d+>>     LessThanOrEqual [<<Add>>,<<Const1>>]
  /// CHECK-DAG:                       If [<<Cond>>]
  /// CHECK-DAG:                       Return [<<PhiX>>]

  /// CHECK-START: int Main.MultiplePhis() select_generator (after)
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Const13:i\d+>>  IntConstant 13
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<PhiX:i\d+>>     Phi [<<Const0>>,<<Select:i\d+>>]
  /// CHECK-DAG:     <<PhiY:i\d+>>     Phi [<<Const1>>,<<Add:i\d+>>]
  /// CHECK-DAG:     <<Add>>           Add [<<PhiY>>,<<Const1>>]
  /// CHECK-DAG:     <<Cond:z\d+>>     LessThanOrEqual [<<Add>>,<<Const1>>]
  /// CHECK-DAG:     <<Select>>        Select [<<Const13>>,<<Const42>>,<<Cond>>]
  /// CHECK-DAG:                       Return [<<PhiX>>]

  public static int MultiplePhis() {
    int x = 0;
    int y = 1;
    while (y++ < 10) {
      if (y > 1) {
        x = 13;
      } else {
        x = 42;
      }
    }
    return x;
  }

  /// CHECK-START: int Main.TrueBlockWithTooManyInstructions(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:     <<Iget:i\d+>>    InstanceFieldGet [<<This>>]
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Iget>>,<<Const2>>]
  /// CHECK-DAG:                      Phi [<<Add>>,<<Const43>>]

  /// CHECK-START: int Main.TrueBlockWithTooManyInstructions(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int TrueBlockWithTooManyInstructions(boolean x) {
    return x ? (read_field + 2) : 43;
  }

  /// CHECK-START: int Main.FalseBlockWithTooManyInstructions(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const3:i\d+>>  IntConstant 3
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:     <<Iget:i\d+>>    InstanceFieldGet [<<This>>]
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Iget>>,<<Const3>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Add>>]

  /// CHECK-START: int Main.FalseBlockWithTooManyInstructions(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int FalseBlockWithTooManyInstructions(boolean x) {
    return x ? 42 : (read_field + 3);
  }

  /// CHECK-START: int Main.TrueBlockWithSideEffects(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<This>>,<<Const42>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Const43>>]

  /// CHECK-START: int Main.TrueBlockWithSideEffects(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int TrueBlockWithSideEffects(boolean x) {
    return x ? (write_field = 42) : 43;
  }

  /// CHECK-START: int Main.FalseBlockWithSideEffects(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<This>>,<<Const43>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Const43>>]

  /// CHECK-START: int Main.FalseBlockWithSideEffects(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int FalseBlockWithSideEffects(boolean x) {
    return x ? 42 : (write_field = 43);
  }

  public static void main(String[] args) throws Exception {
    assertBoolEquals(false, BooleanNot(true));
    assertBoolEquals(true, BooleanNot(false));
    assertBoolEquals(true, GreaterThan(10, 5));
    assertBoolEquals(false, GreaterThan(10, 10));
    assertBoolEquals(false, GreaterThan(5, 10));
    assertBoolEquals(true, LessThan(5, 10));
    assertBoolEquals(false, LessThan(10, 10));
    assertBoolEquals(false, LessThan(10, 5));
    assertBoolEquals(true, ValuesOrdered(1, 3, 5));
    assertBoolEquals(true, ValuesOrdered(5, 3, 1));
    assertBoolEquals(false, ValuesOrdered(1, 3, 2));
    assertBoolEquals(false, ValuesOrdered(2, 3, 1));
    assertBoolEquals(true, ValuesOrdered(3, 3, 3));
    assertBoolEquals(true, ValuesOrdered(3, 3, 5));
    assertBoolEquals(false, ValuesOrdered(5, 5, 3));
    assertIntEquals(42, NegatedCondition(true));
    assertIntEquals(43, NegatedCondition(false));
    assertIntEquals(46, SimpleTrueBlock(true, 4));
    assertIntEquals(43, SimpleTrueBlock(false, 4));
    assertIntEquals(42, SimpleFalseBlock(true, 7));
    assertIntEquals(50, SimpleFalseBlock(false, 7));
    assertIntEquals(48, SimpleBothBlocks(true, 6, 2));
    assertIntEquals(45, SimpleBothBlocks(false, 6, 2));
    assertIntEquals(1, ThreeBlocks(true, true));
    assertIntEquals(1, ThreeBlocks(true, false));
    assertIntEquals(2, ThreeBlocks(false, true));
    assertIntEquals(3, ThreeBlocks(false, false));
    assertIntEquals(13, MultiplePhis());

    Main m = new Main();
    assertIntEquals(42, m.TrueBlockWithTooManyInstructions(true));
    assertIntEquals(43, m.TrueBlockWithTooManyInstructions(false));
    assertIntEquals(42, m.FalseBlockWithTooManyInstructions(true));
    assertIntEquals(43, m.FalseBlockWithTooManyInstructions(false));
    assertIntEquals(42, m.TrueBlockWithSideEffects(true));
    assertIntEquals(43, m.TrueBlockWithSideEffects(false));
    assertIntEquals(42, m.FalseBlockWithSideEffects(true));
    assertIntEquals(43, m.FalseBlockWithSideEffects(false));
  }

  // These need to be instance fields so as to not generate a LoadClass for iget/iput.
  public int read_field = 40;
  public int write_field = 42;
}
