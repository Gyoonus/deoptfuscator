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

import java.lang.reflect.Method;

public class Main {

  public static void main(String args[]) {
    expectEqualsByte((byte)1, booleanToByte(true));
    expectEqualsShort((short)1, booleanToShort(true));
    expectEqualsChar((char)1, booleanToChar(true));
    expectEqualsInt(1, booleanToInt(true));
    expectEqualsLong(1L, booleanToLong(true));
    expectEqualsLong(1L, $noinline$runSmaliTest("booleanToLong", true));

    expectEqualsInt(1, longToIntOfBoolean());
    expectEqualsInt(1, $noinline$runSmaliTest("longToIntOfBoolean"));

    System.out.println("passed");
  }

  /// CHECK-START: byte Main.booleanToByte(boolean) instruction_simplifier$after_bce (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:                            Return [<<Arg>>]

  static byte booleanToByte(boolean b) {
    return (byte)(b ? 1 : 0);
  }

  /// CHECK-START: short Main.booleanToShort(boolean) instruction_simplifier$after_bce (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:                            Return [<<Arg>>]

  static short booleanToShort(boolean b) {
    return (short)(b ? 1 : 0);
  }

  /// CHECK-START: char Main.booleanToChar(boolean) instruction_simplifier$after_bce (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:                            Return [<<Arg>>]

  static char booleanToChar(boolean b) {
    return (char)(b ? 1 : 0);
  }

  /// CHECK-START: int Main.booleanToInt(boolean) instruction_simplifier$after_bce (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:                            Return [<<Arg>>]

  static int booleanToInt(boolean b) {
    return b ? 1 : 0;
  }

  /// CHECK-START: long Main.booleanToLong(boolean) builder (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:     <<IZero:i\d+>>         IntConstant 0
  /// CHECK-DAG:     <<Zero:j\d+>>          LongConstant 0
  /// CHECK-DAG:     <<One:j\d+>>           LongConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>          Equal [<<Arg>>,<<IZero>>]
  /// CHECK-DAG:                            If [<<Cond>>]
  /// CHECK-DAG:     <<Phi:j\d+>>           Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:                            Return [<<Phi>>]

  /// CHECK-START: long Main.booleanToLong(boolean) select_generator (after)
  /// CHECK-NOT:                            IntConstant
  /// CHECK-NOT:                            Equal
  /// CHECK-NOT:                            If
  /// CHECK-NOT:                            Phi

  /// CHECK-START: long Main.booleanToLong(boolean) select_generator (after)
  /// CHECK:         <<Arg:z\d+>>           ParameterValue
  /// CHECK-DAG:     <<Zero:j\d+>>          LongConstant 0
  /// CHECK-DAG:     <<One:j\d+>>           LongConstant 1
  /// CHECK-DAG:     <<Sel:j\d+>>           Select [<<Zero>>,<<One>>,<<Arg>>]
  /// CHECK-DAG:                            Return [<<Sel>>]

  // As of now, the code is not optimized any further than the above.
  // TODO: Re-enable checks below after simplifier is updated to handle this pattern: b/63064517

  // CHECK-START: long Main.booleanToLong(boolean) instruction_simplifier$after_bce (after)
  // CHECK:         <<Arg:z\d+>>           ParameterValue
  // CHECK-DAG:     <<ZToJ:j\d+>>          TypeConversion [<<Arg>>]
  // CHECK-DAG:                            Return [<<ZToJ>>]

  static long booleanToLong(boolean b) {
    return b ? 1 : 0;
  }

  /// CHECK-START: int Main.longToIntOfBoolean() builder (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
  /// CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
  /// CHECK-DAG:     <<ZToJ:j\d+>>          InvokeStaticOrDirect [<<Sget>>,<<Method>>]
  /// CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<ZToJ>>]
  /// CHECK-DAG:                            Return [<<JToI>>]

  /// CHECK-START: int Main.longToIntOfBoolean() inliner (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
  /// CHECK-DAG:     <<Zero:j\d+>>          LongConstant 0
  /// CHECK-DAG:     <<One:j\d+>>           LongConstant 1
  /// CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
  /// CHECK-DAG:                            If [<<Sget>>]
  /// CHECK-DAG:     <<Phi:j\d+>>           Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<Phi>>]
  /// CHECK-DAG:                            Return [<<JToI>>]

  /// CHECK-START: long Main.booleanToLong(boolean) select_generator (after)
  /// CHECK-NOT:                            IntConstant
  /// CHECK-NOT:                            Equal
  /// CHECK-NOT:                            If
  /// CHECK-NOT:                            Phi

  /// CHECK-START: int Main.longToIntOfBoolean() select_generator (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
  /// CHECK-DAG:     <<Zero:j\d+>>          LongConstant 0
  /// CHECK-DAG:     <<One:j\d+>>           LongConstant 1
  /// CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
  /// CHECK-DAG:     <<Sel:j\d+>>           Select [<<Zero>>,<<One>>,<<Sget>>]
  /// CHECK-DAG:     <<JToI:i\d+>>          TypeConversion [<<Sel>>]
  /// CHECK-DAG:                            Return [<<JToI>>]

  // As of now, the code is not optimized any further than the above.
  // TODO: Re-enable checks below after simplifier is updated to handle this pattern: b/63064517

  // CHECK-START: int Main.longToIntOfBoolean() instruction_simplifier$after_bce (after)
  // CHECK-DAG:     <<Method:[ij]\d+>>     CurrentMethod
  // CHECK-DAG:     <<Sget:z\d+>>          StaticFieldGet
  // CHECK-DAG:                            Return [<<Sget>>]

  static int longToIntOfBoolean() {
    long l = booleanToLong(booleanField);
    return (int) l;
  }


  private static void expectEqualsByte(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsShort(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsChar(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsInt(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualsLong(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static long $noinline$runSmaliTest(String name, boolean input) {
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name, boolean.class);
      return (Long) m.invoke(null, input);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  public static int $noinline$runSmaliTest(String name) {
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name);
      return (Integer) m.invoke(null);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }


  public static boolean booleanField = true;

}
