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

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) select_generator (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) select_generator (after)
  /// CHECK-NOT:                      Phi

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                      Select

  private static int rotateLeftBoolean(boolean value, int distance) {
    return Integer.rotateLeft(value ? 1 : 0, distance);
  }

  /// CHECK-START: int Main.rotateLeftByte(byte, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:b\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftByte(byte, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:b\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftByte(byte, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateLeftByte(byte value, int distance) {
    return Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: int Main.rotateLeftShort(short, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:s\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftShort(short, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:s\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftShort(short, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateLeftShort(short value, int distance) {
    return Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: int Main.rotateLeftChar(char, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:c\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftChar(char, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:c\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftChar(char, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateLeftChar(char value, int distance) {
    return Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: int Main.rotateLeftInt(int, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftInt(int, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftInt(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateLeftInt(int value, int distance) {
    return Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: long Main.rotateLeftLong(long, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:j\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:j\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:LongRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: long Main.rotateLeftLong(long, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:j\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:j\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: long Main.rotateLeftLong(long, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static long rotateLeftLong(long value, int distance) {
    return Long.rotateLeft(value, distance);
  }


  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) select_generator (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
  /// CHECK-DAG:     <<One:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) select_generator (after)
  /// CHECK-NOT:                     Phi

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
  /// CHECK:         <<ArgVal:z\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                     Select

  private static int rotateRightBoolean(boolean value, int distance) {
    return Integer.rotateRight(value ? 1 : 0, distance);
  }

  /// CHECK-START: int Main.rotateRightByte(byte, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:b\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightByte(byte, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:b\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightByte(byte, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateRightByte(byte value, int distance) {
    return Integer.rotateRight(value, distance);
  }

  /// CHECK-START: int Main.rotateRightShort(short, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:s\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightShort(short, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:s\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightShort(short, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateRightShort(short value, int distance) {
    return Integer.rotateRight(value, distance);
  }

  /// CHECK-START: int Main.rotateRightChar(char, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:c\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightChar(char, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:c\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightChar(char, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateRightChar(char value, int distance) {
    return Integer.rotateRight(value, distance);
  }

  /// CHECK-START: int Main.rotateRightInt(int, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightInt(int, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightInt(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateRightInt(int value, int distance) {
    return Integer.rotateRight(value, distance);
  }

  /// CHECK-START: long Main.rotateRightLong(long, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:j\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:j\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:LongRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: long Main.rotateRightLong(long, int) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:j\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:i\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:j\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: long Main.rotateRightLong(long, int) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static long rotateRightLong(long value, int distance) {
    return Long.rotateRight(value, distance);
  }


  /// CHECK-START: int Main.rotateLeftIntWithByteDistance(int, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:b\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftIntWithByteDistance(int, byte) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:b\d+>> ParameterValue
  /// CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateLeftIntWithByteDistance(int, byte) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateLeftIntWithByteDistance(int value, byte distance) {
    return Integer.rotateLeft(value, distance);
  }

  /// CHECK-START: int Main.rotateRightIntWithByteDistance(int, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:b\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<ArgVal>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightIntWithByteDistance(int, byte) instruction_simplifier (after)
  /// CHECK:         <<ArgVal:i\d+>>  ParameterValue
  /// CHECK:         <<ArgDist:b\d+>> ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
  /// CHECK-DAG:                      Return [<<Result>>]

  /// CHECK-START: int Main.rotateRightIntWithByteDistance(int, byte) instruction_simplifier (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  private static int rotateRightIntWithByteDistance(int value, byte distance) {
    return Integer.rotateRight(value, distance);
  }


  public static void testRotateLeftBoolean() {
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0, rotateLeftBoolean(false, i));
      expectEqualsInt(1 << i, rotateLeftBoolean(true, i));
    }
  }

  public static void testRotateLeftByte() {
    expectEqualsInt(0x00000001, rotateLeftByte((byte)0x01, 0));
    expectEqualsInt(0x00000002, rotateLeftByte((byte)0x01, 1));
    expectEqualsInt(0x80000000, rotateLeftByte((byte)0x01, 31));
    expectEqualsInt(0x00000001, rotateLeftByte((byte)0x01, 32));  // overshoot
    expectEqualsInt(0xFFFFFF03, rotateLeftByte((byte)0x81, 1));
    expectEqualsInt(0xFFFFFE07, rotateLeftByte((byte)0x81, 2));
    expectEqualsInt(0x00000120, rotateLeftByte((byte)0x12, 4));
    expectEqualsInt(0xFFFF9AFF, rotateLeftByte((byte)0x9A, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateLeftByte((byte)0x0000, i));
      expectEqualsInt(0xFFFFFFFF, rotateLeftByte((byte)0xFFFF, i));
      expectEqualsInt((1 << j), rotateLeftByte((byte)0x0001, i));
      expectEqualsInt((0x12 << j) | (0x12 >>> -j), rotateLeftByte((byte)0x12, i));
    }
  }

  public static void testRotateLeftShort() {
    expectEqualsInt(0x00000001, rotateLeftShort((short)0x0001, 0));
    expectEqualsInt(0x00000002, rotateLeftShort((short)0x0001, 1));
    expectEqualsInt(0x80000000, rotateLeftShort((short)0x0001, 31));
    expectEqualsInt(0x00000001, rotateLeftShort((short)0x0001, 32));  // overshoot
    expectEqualsInt(0xFFFF0003, rotateLeftShort((short)0x8001, 1));
    expectEqualsInt(0xFFFE0007, rotateLeftShort((short)0x8001, 2));
    expectEqualsInt(0x00012340, rotateLeftShort((short)0x1234, 4));
    expectEqualsInt(0xFF9ABCFF, rotateLeftShort((short)0x9ABC, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateLeftShort((short)0x0000, i));
      expectEqualsInt(0xFFFFFFFF, rotateLeftShort((short)0xFFFF, i));
      expectEqualsInt((1 << j), rotateLeftShort((short)0x0001, i));
      expectEqualsInt((0x1234 << j) | (0x1234 >>> -j), rotateLeftShort((short)0x1234, i));
    }
  }

  public static void testRotateLeftChar() {
    expectEqualsInt(0x00000001, rotateLeftChar((char)0x0001, 0));
    expectEqualsInt(0x00000002, rotateLeftChar((char)0x0001, 1));
    expectEqualsInt(0x80000000, rotateLeftChar((char)0x0001, 31));
    expectEqualsInt(0x00000001, rotateLeftChar((char)0x0001, 32));  // overshoot
    expectEqualsInt(0x00010002, rotateLeftChar((char)0x8001, 1));
    expectEqualsInt(0x00020004, rotateLeftChar((char)0x8001, 2));
    expectEqualsInt(0x00012340, rotateLeftChar((char)0x1234, 4));
    expectEqualsInt(0x009ABC00, rotateLeftChar((char)0x9ABC, 8));
    expectEqualsInt(0x00FF0000, rotateLeftChar((char)0xFF00, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateLeftChar((char)0x0000, i));
      expectEqualsInt((1 << j), rotateLeftChar((char)0x0001, i));
      expectEqualsInt((0x1234 << j) | (0x1234 >>> -j), rotateLeftChar((char)0x1234, i));
    }
  }

  public static void testRotateLeftInt() {
    expectEqualsInt(0x00000001, rotateLeftInt(0x00000001, 0));
    expectEqualsInt(0x00000002, rotateLeftInt(0x00000001, 1));
    expectEqualsInt(0x80000000, rotateLeftInt(0x00000001, 31));
    expectEqualsInt(0x00000001, rotateLeftInt(0x00000001, 32));  // overshoot
    expectEqualsInt(0x00000003, rotateLeftInt(0x80000001, 1));
    expectEqualsInt(0x00000006, rotateLeftInt(0x80000001, 2));
    expectEqualsInt(0x23456781, rotateLeftInt(0x12345678, 4));
    expectEqualsInt(0xBCDEF09A, rotateLeftInt(0x9ABCDEF0, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateLeftInt(0x00000000, i));
      expectEqualsInt(0xFFFFFFFF, rotateLeftInt(0xFFFFFFFF, i));
      expectEqualsInt(1 << j, rotateLeftInt(0x00000001, i));
      expectEqualsInt((0x12345678 << j) | (0x12345678 >>> -j), rotateLeftInt(0x12345678, i));
    }
  }

  public static void testRotateLeftLong() {
    expectEqualsLong(0x0000000000000001L, rotateLeftLong(0x0000000000000001L, 0));
    expectEqualsLong(0x0000000000000002L, rotateLeftLong(0x0000000000000001L, 1));
    expectEqualsLong(0x8000000000000000L, rotateLeftLong(0x0000000000000001L, 63));
    expectEqualsLong(0x0000000000000001L, rotateLeftLong(0x0000000000000001L, 64));  // overshoot
    expectEqualsLong(0x0000000000000003L, rotateLeftLong(0x8000000000000001L, 1));
    expectEqualsLong(0x0000000000000006L, rotateLeftLong(0x8000000000000001L, 2));
    expectEqualsLong(0x23456789ABCDEF01L, rotateLeftLong(0x123456789ABCDEF0L, 4));
    expectEqualsLong(0x3456789ABCDEF012L, rotateLeftLong(0x123456789ABCDEF0L, 8));
    for (int i = 0; i < 70; i++) {  // overshoot a bit
      int j = i & 63;
      expectEqualsLong(0x0000000000000000L, rotateLeftLong(0x0000000000000000L, i));
      expectEqualsLong(0xFFFFFFFFFFFFFFFFL, rotateLeftLong(0xFFFFFFFFFFFFFFFFL, i));
      expectEqualsLong(1L << j, rotateLeftLong(0x0000000000000001, i));
      expectEqualsLong((0x123456789ABCDEF0L << j) | (0x123456789ABCDEF0L >>> -j),
                       rotateLeftLong(0x123456789ABCDEF0L, i));
    }
  }

  public static void testRotateRightBoolean() {
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0, rotateRightBoolean(false, i));
      expectEqualsInt(1 << (32 - i), rotateRightBoolean(true, i));
    }
  }

  public static void testRotateRightByte() {
    expectEqualsInt(0xFFFFFF80, rotateRightByte((byte)0x80, 0));
    expectEqualsInt(0x7FFFFFC0, rotateRightByte((byte)0x80, 1));
    expectEqualsInt(0xFFFFFF01, rotateRightByte((byte)0x80, 31));
    expectEqualsInt(0xFFFFFF80, rotateRightByte((byte)0x80, 32));  // overshoot
    expectEqualsInt(0xFFFFFFC0, rotateRightByte((byte)0x81, 1));
    expectEqualsInt(0x7FFFFFE0, rotateRightByte((byte)0x81, 2));
    expectEqualsInt(0x20000001, rotateRightByte((byte)0x12, 4));
    expectEqualsInt(0x9AFFFFFF, rotateRightByte((byte)0x9A, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateRightByte((byte)0x00, i));
      expectEqualsInt(0xFFFFFFFF, rotateRightByte((byte)0xFF, i));
      expectEqualsInt(1 << (32 - j), rotateRightByte((byte)0x01, i));
      expectEqualsInt((0x12 >>> j) | (0x12 << -j), rotateRightByte((byte)0x12, i));
    }
  }

  public static void testRotateRightShort() {
    expectEqualsInt(0xFFFF8000, rotateRightShort((short)0x8000, 0));
    expectEqualsInt(0x7FFFC000, rotateRightShort((short)0x8000, 1));
    expectEqualsInt(0xFFFF0001, rotateRightShort((short)0x8000, 31));
    expectEqualsInt(0xFFFF8000, rotateRightShort((short)0x8000, 32));  // overshoot
    expectEqualsInt(0xFFFFC000, rotateRightShort((short)0x8001, 1));
    expectEqualsInt(0x7FFFE000, rotateRightShort((short)0x8001, 2));
    expectEqualsInt(0x40000123, rotateRightShort((short)0x1234, 4));
    expectEqualsInt(0xBCFFFF9A, rotateRightShort((short)0x9ABC, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateRightShort((short)0x0000, i));
      expectEqualsInt(0xFFFFFFFF, rotateRightShort((short)0xFFFF, i));
      expectEqualsInt(1 << (32 - j), rotateRightShort((short)0x0001, i));
      expectEqualsInt((0x1234 >>> j) | (0x1234 << -j), rotateRightShort((short)0x1234, i));
    }
  }

  public static void testRotateRightChar() {
    expectEqualsInt(0x00008000, rotateRightChar((char)0x8000, 0));
    expectEqualsInt(0x00004000, rotateRightChar((char)0x8000, 1));
    expectEqualsInt(0x00010000, rotateRightChar((char)0x8000, 31));
    expectEqualsInt(0x00008000, rotateRightChar((char)0x8000, 32));  // overshoot
    expectEqualsInt(0x80004000, rotateRightChar((char)0x8001, 1));
    expectEqualsInt(0x40002000, rotateRightChar((char)0x8001, 2));
    expectEqualsInt(0x40000123, rotateRightChar((char)0x1234, 4));
    expectEqualsInt(0xBC00009A, rotateRightChar((char)0x9ABC, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateRightChar((char)0x0000, i));
      expectEqualsInt(1 << (32 - j), rotateRightChar((char)0x0001, i));
      expectEqualsInt((0x1234 >>> j) | (0x1234 << -j), rotateRightChar((char)0x1234, i));
    }
  }

  public static void testRotateRightInt() {
    expectEqualsInt(0x80000000, rotateRightInt(0x80000000, 0));
    expectEqualsInt(0x40000000, rotateRightInt(0x80000000, 1));
    expectEqualsInt(0x00000001, rotateRightInt(0x80000000, 31));
    expectEqualsInt(0x80000000, rotateRightInt(0x80000000, 32));  // overshoot
    expectEqualsInt(0xC0000000, rotateRightInt(0x80000001, 1));
    expectEqualsInt(0x60000000, rotateRightInt(0x80000001, 2));
    expectEqualsInt(0x81234567, rotateRightInt(0x12345678, 4));
    expectEqualsInt(0xF09ABCDE, rotateRightInt(0x9ABCDEF0, 8));
    for (int i = 0; i < 40; i++) {  // overshoot a bit
      int j = i & 31;
      expectEqualsInt(0x00000000, rotateRightInt(0x00000000, i));
      expectEqualsInt(0xFFFFFFFF, rotateRightInt(0xFFFFFFFF, i));
      expectEqualsInt(0x80000000 >>> j, rotateRightInt(0x80000000, i));
      expectEqualsInt((0x12345678 >>> j) | (0x12345678 << -j), rotateRightInt(0x12345678, i));
    }
  }

  public static void testRotateRightLong() {
    expectEqualsLong(0x8000000000000000L, rotateRightLong(0x8000000000000000L, 0));
    expectEqualsLong(0x4000000000000000L, rotateRightLong(0x8000000000000000L, 1));
    expectEqualsLong(0x0000000000000001L, rotateRightLong(0x8000000000000000L, 63));
    expectEqualsLong(0x8000000000000000L, rotateRightLong(0x8000000000000000L, 64));  // overshoot
    expectEqualsLong(0xC000000000000000L, rotateRightLong(0x8000000000000001L, 1));
    expectEqualsLong(0x6000000000000000L, rotateRightLong(0x8000000000000001L, 2));
    expectEqualsLong(0x0123456789ABCDEFL, rotateRightLong(0x123456789ABCDEF0L, 4));
    expectEqualsLong(0xF0123456789ABCDEL, rotateRightLong(0x123456789ABCDEF0L, 8));
    for (int i = 0; i < 70; i++) {  // overshoot a bit
      int j = i & 63;
      expectEqualsLong(0x0000000000000000L, rotateRightLong(0x0000000000000000L, i));
      expectEqualsLong(0xFFFFFFFFFFFFFFFFL, rotateRightLong(0xFFFFFFFFFFFFFFFFL, i));
      expectEqualsLong(0x8000000000000000L >>> j, rotateRightLong(0x8000000000000000L, i));
      expectEqualsLong((0x123456789ABCDEF0L >>> j) | (0x123456789ABCDEF0L << -j),
                       rotateRightLong(0x123456789ABCDEF0L, i));
    }
  }


  public static void testRotateLeftIntWithByteDistance() {
    expectEqualsInt(0x00000001, rotateLeftIntWithByteDistance(0x00000001, (byte)0));
    expectEqualsInt(0x00000002, rotateLeftIntWithByteDistance(0x00000001, (byte)1));
    expectEqualsInt(0x80000000, rotateLeftIntWithByteDistance(0x00000001, (byte)31));
    expectEqualsInt(0x00000001, rotateLeftIntWithByteDistance(0x00000001, (byte)32));  // overshoot
    expectEqualsInt(0x00000003, rotateLeftIntWithByteDistance(0x80000001, (byte)1));
    expectEqualsInt(0x00000006, rotateLeftIntWithByteDistance(0x80000001, (byte)2));
    expectEqualsInt(0x23456781, rotateLeftIntWithByteDistance(0x12345678, (byte)4));
    expectEqualsInt(0xBCDEF09A, rotateLeftIntWithByteDistance(0x9ABCDEF0, (byte)8));
    for (byte i = 0; i < 40; i++) {  // overshoot a bit
      byte j = (byte)(i & 31);
      expectEqualsInt(0x00000000, rotateLeftIntWithByteDistance(0x00000000, i));
      expectEqualsInt(0xFFFFFFFF, rotateLeftIntWithByteDistance(0xFFFFFFFF, i));
      expectEqualsInt(1 << j, rotateLeftIntWithByteDistance(0x00000001, i));
      expectEqualsInt((0x12345678 << j) | (0x12345678 >>> -j),
                      rotateLeftIntWithByteDistance(0x12345678, i));
    }
  }

  public static void testRotateRightIntWithByteDistance() {
    expectEqualsInt(0x80000000, rotateRightIntWithByteDistance(0x80000000, (byte)0));
    expectEqualsInt(0x40000000, rotateRightIntWithByteDistance(0x80000000, (byte)1));
    expectEqualsInt(0x00000001, rotateRightIntWithByteDistance(0x80000000, (byte)31));
    expectEqualsInt(0x80000000, rotateRightIntWithByteDistance(0x80000000, (byte)32));  // overshoot
    expectEqualsInt(0xC0000000, rotateRightIntWithByteDistance(0x80000001, (byte)1));
    expectEqualsInt(0x60000000, rotateRightIntWithByteDistance(0x80000001, (byte)2));
    expectEqualsInt(0x81234567, rotateRightIntWithByteDistance(0x12345678, (byte)4));
    expectEqualsInt(0xF09ABCDE, rotateRightIntWithByteDistance(0x9ABCDEF0, (byte)8));
    for (byte i = 0; i < 40; i++) {  // overshoot a bit
      byte j = (byte)(i & 31);
      expectEqualsInt(0x00000000, rotateRightIntWithByteDistance(0x00000000, i));
      expectEqualsInt(0xFFFFFFFF, rotateRightIntWithByteDistance(0xFFFFFFFF, i));
      expectEqualsInt(0x80000000 >>> j, rotateRightIntWithByteDistance(0x80000000, i));
      expectEqualsInt((0x12345678 >>> j) | (0x12345678 << -j),
                      rotateRightIntWithByteDistance(0x12345678, i));
    }
  }


  public static void main(String args[]) {
    testRotateLeftBoolean();
    testRotateLeftByte();
    testRotateLeftShort();
    testRotateLeftChar();
    testRotateLeftInt();
    testRotateLeftLong();

    testRotateRightBoolean();
    testRotateRightByte();
    testRotateRightShort();
    testRotateRightChar();
    testRotateRightInt();
    testRotateRightLong();

    // Also exercise distance values with types other than int.
    testRotateLeftIntWithByteDistance();
    testRotateRightIntWithByteDistance();

    System.out.println("passed");
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
}
