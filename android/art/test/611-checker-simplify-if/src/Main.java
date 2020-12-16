/*
 * Copyright (C) 2016 The Android Open Source Project
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

  public static void main(String[] args) {
    testNoInline(args);
    System.out.println(staticField);
    testInline(args);
    System.out.println(staticField);
    testNonConstantInputs(args);
    System.out.println(staticField);
    testNonConstantEqual(args);
    System.out.println(staticField);
    testGreaterCondition(args);
    System.out.println(staticField);
    testSwitch(args);
    System.out.println(staticField);
    testFP(args);
    System.out.println(staticField);
  }

  // Test when a condition is the input of the if.

  /// CHECK-START: void Main.testNoInline(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK: <<Const0:i\d+>>   IntConstant 0
  /// CHECK:                   If
  /// CHECK: <<Phi:i\d+>>      Phi
  /// CHECK: <<Equal:z\d+>>    Equal [<<Phi>>,<<Const0>>]
  /// CHECK:                   If [<<Equal>>]

  /// CHECK-START: void Main.testNoInline(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK:      If
  /// CHECK-NOT:  Phi
  /// CHECK-NOT:  Equal
  /// CHECK-NOT:  If
  public static void testNoInline(String[] args) {
    boolean myVar = false;
    if (args.length == 42) {
      myVar = true;
    } else {
      staticField = 32;
      myVar = false;
    }
    if (myVar) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test when the phi is the input of the if.

  /// CHECK-START: void Main.testInline(java.lang.String[]) dead_code_elimination$after_inlining (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:                   If
  /// CHECK-DAG: <<Phi:i\d+>>      Phi
  /// CHECK-DAG:                   If [<<Phi>>]

  /// CHECK-START: void Main.testInline(java.lang.String[]) dead_code_elimination$after_inlining (after)
  /// CHECK:      If
  /// CHECK-NOT:  Phi
  /// CHECK-NOT:  If
  public static void testInline(String[] args) {
    boolean myVar = $inline$doTest(args);
    if (myVar) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  public static boolean $inline$doTest(String[] args) {
    boolean myVar;
    if (args.length == 42) {
      myVar = true;
    } else {
      staticField = 32;
      myVar = false;
    }
    return myVar;
  }

  // Test when one input is not a constant. We can only optimize the constant input.

  /// CHECK-START: void Main.testNonConstantInputs(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Phi>>,<<Const42>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]

  /// CHECK-START: void Main.testNonConstantInputs(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-NOT:                          Phi
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<StaticFieldGet>>,<<Const42>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]
  public static void testNonConstantInputs(String[] args) {
    int a = 42;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = otherStaticField;
    }
    if (a == 42) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test with a condition.

  /// CHECK-START: void Main.testGreaterCondition(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const22:i\d+>>         IntConstant 22
  /// CHECK-DAG: <<Const25:i\d+>>         IntConstant 25
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<Const22>>]
  /// CHECK-DAG: <<GE:z\d+>>              GreaterThanOrEqual [<<Phi>>,<<Const25>>]
  /// CHECK-DAG:                          If [<<GE>>]

  /// CHECK-START: void Main.testGreaterCondition(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK-DAG:                          If
  /// CHECK-NOT:                          Phi
  /// CHECK-NOT:                          GreaterThanOrEqual
  /// CHECK-NOT:                          If
  public static void testGreaterCondition(String[] args) {
    int a = 42;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = 22;
    }
    if (a < 25) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Test when comparing non constants.

  /// CHECK-START: void Main.testNonConstantEqual(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG: <<Const42:i\d+>>         IntConstant 42
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-DAG: <<Phi:i\d+>>             Phi [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Phi>>,<<StaticFieldGet>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]

  /// CHECK-START: void Main.testNonConstantEqual(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK-DAG: <<Const34:i\d+>>         IntConstant 34
  /// CHECK-DAG:                          If
  /// CHECK-DAG: <<StaticFieldGet:i\d+>>  StaticFieldGet
  /// CHECK-NOT:                          Phi
  /// CHECK-DAG: <<NotEqual:z\d+>>        NotEqual [<<Const34>>,<<StaticFieldGet>>]
  /// CHECK-DAG:                          If [<<NotEqual>>]
  public static void testNonConstantEqual(String[] args) {
    int a = 42;
    int b = otherStaticField;
    if (args.length == 42) {
      a = 34;
    } else {
      staticField = 32;
      a = b;
    }
    if (a == b) {
      staticField = 12;
    } else {
      staticField = 54;
    }
  }

  // Make sure we don't "simplify" a loop and potentially turn it into
  // an irreducible loop. The suspend check at the loop header prevents
  // us from doing the simplification.

  /// CHECK-START: void Main.testLoop(boolean) disassembly (after)
  /// CHECK-DAG: SuspendCheck
  /// CHECK:     irreducible:false
  /// CHECK-NOT: irreducible:true
  public static void testLoop(boolean c) {
    while (true) {
      if (c) {
        if ($noinline$foo()) return;
        c = false;
      } else {
        $noinline$foo();
        c = true;
      }
    }
  }

  static boolean $noinline$foo() {
    if (doThrow) throw new Error("");
    return true;
  }

  /// CHECK-START: void Main.testSwitch(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK:      If
  /// CHECK:      If
  /// CHECK:      If

  /// CHECK-START: void Main.testSwitch(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK:      If
  /// CHECK:      If
  /// CHECK-NOT:  If
  public static void testSwitch(String[] args) {
    boolean cond = false;
    switch (args.length) {
      case 42:
        staticField = 11;
        cond = true;
        break;
      case 43:
        staticField = 33;
        cond = true;
        break;
      default:
        cond = false;
        break;
    }
    if (cond) {
      // Redirect case 42 and 43 here.
      staticField = 2;
    }
    // Redirect default here.
  }

  /// CHECK-START: void Main.testFP(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK:      If
  /// CHECK:      If

  /// CHECK-START: void Main.testFP(java.lang.String[]) dead_code_elimination$initial (after)
  /// CHECK:      If
  /// CHECK:      If
  public static void testFP(String[] args) {
    float f = 2.2f;
    float nan = $noinline$getNaN();
    if (args.length == 42) {
      f = 4.3f;
    } else {
      staticField = 33;
      f = nan;
    }
    if (f == nan) {
      staticField = 5;
    }
  }

  // No inline variant to avoid having the compiler see it's a NaN.
  static float $noinline$getNaN() {
    if (doThrow) throw new Error("");
    return Float.NaN;
  }

  static boolean doThrow;
  static int staticField;
  static int otherStaticField;
}
