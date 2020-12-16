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

  /// CHECK-START: void Main.InlineVoid() inliner (before)
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:                      InvokeStaticOrDirect
  /// CHECK-DAG:                      InvokeStaticOrDirect [<<Const42>>{{(,[ij]\d+)?}}]

  /// CHECK-START: void Main.InlineVoid() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static void InlineVoid() {
    returnVoid();
    returnVoidWithOneParameter(42);
  }

  /// CHECK-START: int Main.InlineParameter(int) inliner (before)
  /// CHECK-DAG:     <<Param:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect [<<Param>>{{(,[ij]\d+)?}}]
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.InlineParameter(int) inliner (after)
  /// CHECK-DAG:     <<Param:i\d+>>  ParameterValue
  /// CHECK-DAG:                     Return [<<Param>>]

  public static int InlineParameter(int a) {
    return returnParameter(a);
  }

  /// CHECK-START: long Main.InlineWideParameter(long) inliner (before)
  /// CHECK-DAG:     <<Param:j\d+>>  ParameterValue
  /// CHECK-DAG:     <<Result:j\d+>> InvokeStaticOrDirect [<<Param>>{{(,[ij]\d+)?}}]
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: long Main.InlineWideParameter(long) inliner (after)
  /// CHECK-DAG:     <<Param:j\d+>>  ParameterValue
  /// CHECK-DAG:                     Return [<<Param>>]

  public static long InlineWideParameter(long a) {
    return returnWideParameter(a);
  }

  /// CHECK-START: java.lang.Object Main.InlineReferenceParameter(java.lang.Object) inliner (before)
  /// CHECK-DAG:     <<Param:l\d+>>  ParameterValue
  /// CHECK-DAG:     <<Result:l\d+>> InvokeStaticOrDirect [<<Param>>{{(,[ij]\d+)?}}]
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: java.lang.Object Main.InlineReferenceParameter(java.lang.Object) inliner (after)
  /// CHECK-DAG:     <<Param:l\d+>>  ParameterValue
  /// CHECK-DAG:                     Return [<<Param>>]

  public static Object InlineReferenceParameter(Object o) {
    return returnReferenceParameter(o);
  }

  /// CHECK-START: int Main.InlineInt() inliner (before)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.InlineInt() inliner (after)
  /// CHECK-DAG:     <<Const4:i\d+>> IntConstant 4
  /// CHECK-DAG:                     Return [<<Const4>>]

  public static int InlineInt() {
    return returnInt();
  }

  /// CHECK-START: long Main.InlineWide() inliner (before)
  /// CHECK-DAG:     <<Result:j\d+>> InvokeStaticOrDirect
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: long Main.InlineWide() inliner (after)
  /// CHECK-DAG:     <<Const8:j\d+>> LongConstant 8
  /// CHECK-DAG:                     Return [<<Const8>>]

  public static long InlineWide() {
    return returnWide();
  }

  /// CHECK-START: int Main.InlineAdd() inliner (before)
  /// CHECK-DAG:     <<Const3:i\d+>> IntConstant 3
  /// CHECK-DAG:     <<Const5:i\d+>> IntConstant 5
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.InlineAdd() inliner (after)
  /// CHECK-DAG:     <<Const8:i\d+>> IntConstant 8
  /// CHECK-DAG:                     Return [<<Const8>>]

  public static int InlineAdd() {
    return returnAdd(3, 5);
  }

  /// CHECK-START: int Main.InlineFieldAccess() inliner (before)
  /// CHECK-DAG:     <<After:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                     Return [<<After>>]

  /// CHECK-START: int Main.InlineFieldAccess() inliner (after)
  /// CHECK-DAG:     <<Const1:i\d+>> IntConstant 1
  /// CHECK-DAG:     <<Before:i\d+>> StaticFieldGet
  /// CHECK-DAG:     <<After:i\d+>>  Add [<<Before>>,<<Const1>>]
  /// CHECK-DAG:                     StaticFieldSet [{{l\d+}},<<After>>]
  /// CHECK-DAG:                     Return [<<After>>]

  /// CHECK-START: int Main.InlineFieldAccess() inliner (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int InlineFieldAccess() {
    return incCounter();
  }

  public static int InlineWithControlFlow(boolean cond) {
    try {
      Class<?> c = Class.forName("Smali");
      Method m = c.getMethod("InlineWithControlFlow", boolean.class);
      return (Integer) m.invoke(null, cond);
    } catch (Throwable t) {
      throw new RuntimeException(t);
    }
  }

  /// CHECK-START: int Main.returnAbs(int) intrinsics_recognition (before)
  /// CHECK-DAG:     <<Result:i\d+>>      InvokeStaticOrDirect
  /// CHECK-DAG:                          Return [<<Result>>]

  /// CHECK-START: int Main.returnAbs(int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>      InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                          Return [<<Result>>]

  private static int returnAbs(int i) {
    return Math.abs(i);
  }

  /// CHECK-START: int Main.InlinedIntrinsicsAreStillIntrinsic() inliner (before)
  /// CHECK-DAG:     <<ConstMinus1:i\d+>> IntConstant -1
  /// CHECK-DAG:     <<Result:i\d+>>      InvokeStaticOrDirect
  /// CHECK-DAG:                          Return [<<Result>>]

  /// CHECK-START: int Main.InlinedIntrinsicsAreStillIntrinsic() inliner (after)
  /// CHECK-DAG:     <<ConstMinus1:i\d+>> IntConstant -1
  /// CHECK-DAG:     <<Result:i\d+>>      InvokeStaticOrDirect intrinsic:MathAbsInt
  /// CHECK-DAG:                          Return [<<Result>>]

  public static int InlinedIntrinsicsAreStillIntrinsic() {
    return returnAbs(-1);
  }

  private static void returnVoid() {
    return;
  }

  private static void returnVoidWithOneParameter(int a) {
    return;
  }

  private static int returnParameter(int a) {
    return a;
  }

  private static long returnWideParameter(long a) {
    return a;
  }

  private static Object returnReferenceParameter(Object o) {
    return o;
  }

  private static int returnInt() {
    return 4;
  }

  private static long returnWide() {
    return 8L;
  }

  private static int returnAdd(int a, int b) {
    return a + b;
  }

  private static int returnSub(int a, int b) {
    return a - b;
  }

  private static int counter = 42;

  private static int incCounter() {
    return ++counter;
  }

  public static void main(String[] args) {
    InlineVoid();

    if (InlineInt() != 4) {
      throw new Error();
    }

    if (InlineWide() != 8L) {
      throw new Error();
    }

    if (InlineParameter(42) != 42) {
      throw new Error();
    }

    if (InlineWideParameter(0x100000001L) != 0x100000001L) {
      throw new Error();
    }

    if (InlineReferenceParameter(Main.class) != Main.class) {
      throw new Error();
    }

    if (InlineAdd() != 8) {
      throw new Error();
    }

    if (InlineFieldAccess() != 43 || InlineFieldAccess() != 44) {
      throw new Error();
    }

    if (InlineWithControlFlow(true) != 4) {
      throw new Error();
    }

    if (InlineWithControlFlow(false) != 2) {
      throw new Error();
    }

    if (InlinedIntrinsicsAreStillIntrinsic() != 1) {
      throw new Error();
    }

    if (returnAbs(-1) != 1) {
      throw new Error();
    }
  }
}
