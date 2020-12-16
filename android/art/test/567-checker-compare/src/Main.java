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

  public static boolean doThrow = false;

  /// CHECK-START: void Main.$opt$noinline$testReplaceInputWithItself(int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<ArgX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
  /// CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Cmp:i\d+>>    InvokeStaticOrDirect [<<ArgX>>,<<Zero>>,<<Method>>] intrinsic:IntegerCompare
  /// CHECK-DAG:                     GreaterThanOrEqual [<<Cmp>>,<<Zero>>]

  /// CHECK-START: void Main.$opt$noinline$testReplaceInputWithItself(int) instruction_simplifier (after)
  /// CHECK-DAG:     <<ArgX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
  /// CHECK-DAG:                     GreaterThanOrEqual [<<ArgX>>,<<Zero>>]

  public static void $opt$noinline$testReplaceInputWithItself(int x) {
    if (doThrow) { throw new Error(); }

    // The instruction simplifier first replaces Integer.compare(x, 0) with Compare HIR
    // and then merges the Compare into the GreaterThanOrEqual. This is a regression
    // test that to check that it is allowed to replace the second input of the
    // GreaterThanOrEqual, i.e. <<Zero>>, with the very same instruction.
    if (Integer.compare(x, 0) < 0) {
      System.out.println("OOOPS");
    }
  }

  /// CHECK-START: int Main.compareBooleans(boolean, boolean) select_generator (after)
  /// CHECK-NOT:                     Phi

  /// CHECK-START: int Main.compareBooleans(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK:         <<ArgX:z\d+>>   ParameterValue
  /// CHECK:         <<ArgY:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<Result:i\d+>> Compare [<<ArgX>>,<<ArgY>>]
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareBooleans(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                     Select

  private static int compareBooleans(boolean x, boolean y) {
    return Integer.compare((x ? 1 : 0), (y ? 1 : 0));
  }

  private static int compareBooleansSmali(boolean x, boolean y) throws Exception {
    Class<?> c = Class.forName("Smali");
    Method m = c.getMethod("compareBooleans", boolean.class, boolean.class);
    return (Integer) m.invoke(null, x, y);
  }

  /// CHECK-START: int Main.compareBytes(byte, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareBytes(byte, byte) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareBytes(byte, byte) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  private static int compareBytes(byte x, byte y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareShorts(short, short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShorts(short, short) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShorts(short, short) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  private static int compareShorts(short x, short y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareChars(char, char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareChars(char, char) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareChars(char, char) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  private static int compareChars(char x, char y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareInts(int, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareInts(int, int) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareInts(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  private static int compareInts(int x, int y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareLongs(long, long) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:LongCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareLongs(long, long) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareLongs(long, long) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  private static int compareLongs(long x, long y) {
    return Long.compare(x, y);
  }


  /// CHECK-START: int Main.compareByteShort(byte, short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteShort(byte, short) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteShort(byte, short) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareByteShort(byte x, short y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareByteChar(byte, char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteChar(byte, char) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteChar(byte, char) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareByteChar(byte x, char y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareByteInt(byte, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteInt(byte, int) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareByteInt(byte, int) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareByteInt(byte x, int y) {
    return Integer.compare(x, y);
  }


  /// CHECK-START: int Main.compareShortByte(short, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortByte(short, byte) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortByte(short, byte) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareShortByte(short x, byte y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareShortChar(short, char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortChar(short, char) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortChar(short, char) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareShortChar(short x, char y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareShortInt(short, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortInt(short, int) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareShortInt(short, int) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareShortInt(short x, int y) {
    return Integer.compare(x, y);
  }


  /// CHECK-START: int Main.compareCharByte(char, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharByte(char, byte) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharByte(char, byte) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareCharByte(char x, byte y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareCharShort(char, short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharShort(char, short) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharShort(char, short) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareCharShort(char x, short y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareCharInt(char, int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharInt(char, int) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareCharInt(char, int) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareCharInt(char x, int y) {
    return Integer.compare(x, y);
  }


  /// CHECK-START: int Main.compareIntByte(int, byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntByte(int, byte) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntByte(int, byte) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareIntByte(int x, byte y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareIntShort(int, short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntShort(int, short) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntShort(int, short) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareIntShort(int x, short y) {
    return Integer.compare(x, y);
  }

  /// CHECK-START: int Main.compareIntChar(int, char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect intrinsic:IntegerCompare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntChar(int, char) instruction_simplifier (after)
  /// CHECK-DAG:     <<Result:i\d+>> Compare
  /// CHECK-DAG:                     Return [<<Result>>]

  /// CHECK-START: int Main.compareIntChar(int, char) instruction_simplifier (after)
  /// CHECK-NOT:                     InvokeStaticOrDirect

  public static int compareIntChar(int x, char y) {
    return Integer.compare(x, y);
  }


  public static void testCompareBooleans() throws Exception {
    expectEquals(-1, compareBooleans(false, true));
    expectEquals(-1, compareBooleansSmali(false, true));

    expectEquals(0, compareBooleans(false, false));
    expectEquals(0, compareBooleans(true, true));
    expectEquals(0, compareBooleansSmali(false, false));
    expectEquals(0, compareBooleansSmali(true, true));

    expectEquals(1, compareBooleans(true, false));
    expectEquals(1, compareBooleansSmali(true, false));
  }

  public static void testCompareBytes() {
    expectEquals(-1, compareBytes(Byte.MIN_VALUE, (byte)(Byte.MIN_VALUE + 1)));
    expectEquals(-1, compareBytes(Byte.MIN_VALUE, (byte)-1));
    expectEquals(-1, compareBytes(Byte.MIN_VALUE, (byte)0));
    expectEquals(-1, compareBytes(Byte.MIN_VALUE, (byte)1));
    expectEquals(-1, compareBytes(Byte.MIN_VALUE, Byte.MAX_VALUE));
    expectEquals(-1, compareBytes((byte)-1, (byte)0));
    expectEquals(-1, compareBytes((byte)-1, (byte)1));
    expectEquals(-1, compareBytes((byte)0, (byte)1));

    expectEquals(0, compareBytes(Byte.MIN_VALUE, Byte.MIN_VALUE));
    expectEquals(0, compareBytes((byte)-1, (byte)-1));
    expectEquals(0, compareBytes((byte)0, (byte)0));
    expectEquals(0, compareBytes((byte)1, (byte)1));
    expectEquals(0, compareBytes(Byte.MAX_VALUE, Byte.MAX_VALUE));

    expectEquals(1, compareBytes((byte)0, (byte)-1));
    expectEquals(1, compareBytes((byte)1, (byte)-1));
    expectEquals(1, compareBytes((byte)1, (byte)0));
    expectEquals(1, compareBytes(Byte.MAX_VALUE, Byte.MIN_VALUE));
    expectEquals(1, compareBytes(Byte.MAX_VALUE, (byte)-1));
    expectEquals(1, compareBytes(Byte.MAX_VALUE, (byte)0));
    expectEquals(1, compareBytes(Byte.MAX_VALUE, (byte)1));
    expectEquals(1, compareBytes(Byte.MAX_VALUE, (byte)(Byte.MAX_VALUE - 1)));

    for (byte i = -11; i <= 11; i++) {
      for (byte j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareBytes(i, j));
      }
    }
  }

  public static void testCompareShorts() {
    expectEquals(-1, compareShorts(Short.MIN_VALUE, (short)(Short.MIN_VALUE + 1)));
    expectEquals(-1, compareShorts(Short.MIN_VALUE, (short)-1));
    expectEquals(-1, compareShorts(Short.MIN_VALUE, (short)0));
    expectEquals(-1, compareShorts(Short.MIN_VALUE, (short)1));
    expectEquals(-1, compareShorts(Short.MIN_VALUE, (short)Short.MAX_VALUE));
    expectEquals(-1, compareShorts((short)-1, (short)0));
    expectEquals(-1, compareShorts((short)-1, (short)1));
    expectEquals(-1, compareShorts((short)0, (short)1));

    expectEquals(0, compareShorts(Short.MIN_VALUE, Short.MIN_VALUE));
    expectEquals(0, compareShorts((short)-1, (short)-1));
    expectEquals(0, compareShorts((short)0, (short)0));
    expectEquals(0, compareShorts((short)1, (short)1));
    expectEquals(0, compareShorts(Short.MAX_VALUE, Short.MAX_VALUE));

    expectEquals(1, compareShorts((short)0, (short)-1));
    expectEquals(1, compareShorts((short)1, (short)-1));
    expectEquals(1, compareShorts((short)1, (short)0));
    expectEquals(1, compareShorts(Short.MAX_VALUE, Short.MIN_VALUE));
    expectEquals(1, compareShorts(Short.MAX_VALUE, (short)-1));
    expectEquals(1, compareShorts(Short.MAX_VALUE, (short)0));
    expectEquals(1, compareShorts(Short.MAX_VALUE, (short)1));
    expectEquals(1, compareShorts(Short.MAX_VALUE, (short)(Short.MAX_VALUE - 1)));

    for (short i = -11; i <= 11; i++) {
      for (short j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareShorts(i, j));
      }
    }
  }

  public static void testCompareChars() {
    expectEquals(-1, compareChars((char)0, Character.MAX_VALUE));
    expectEquals(-1, compareChars((char)0, (char)1));

    expectEquals(0, compareChars((char)0, (char)0));
    expectEquals(0, compareChars((char)1, (char)1));
    expectEquals(0, compareChars(Character.MAX_VALUE, Character.MAX_VALUE));

    expectEquals(1, compareChars((char)1, (char)0));
    expectEquals(1, compareChars(Character.MAX_VALUE, (char)0));
    expectEquals(1, compareChars(Character.MAX_VALUE, (char)1));
    expectEquals(1, compareChars(Character.MAX_VALUE, (char)(Character.MAX_VALUE - 1)));

    for (char i = 0; i <= 11; i++) {
      for (char j = 0; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareChars(i, j));
      }
    }
  }

  public static void testCompareInts() {
    expectEquals(-1, compareInts(Integer.MIN_VALUE, Integer.MIN_VALUE + 1));
    expectEquals(-1, compareInts(Integer.MIN_VALUE, -1));
    expectEquals(-1, compareInts(Integer.MIN_VALUE, 0));
    expectEquals(-1, compareInts(Integer.MIN_VALUE, 1));
    expectEquals(-1, compareInts(Integer.MIN_VALUE, Integer.MAX_VALUE));
    expectEquals(-1, compareInts(-1, 0));
    expectEquals(-1, compareInts(-1, 1));
    expectEquals(-1, compareInts(0, 1));

    expectEquals(0, compareInts(Integer.MIN_VALUE, Integer.MIN_VALUE));
    expectEquals(0, compareInts(-1, -1));
    expectEquals(0, compareInts(0, 0));
    expectEquals(0, compareInts(1, 1));
    expectEquals(0, compareInts(Integer.MAX_VALUE, Integer.MAX_VALUE));

    expectEquals(1, compareInts(0, -1));
    expectEquals(1, compareInts(1, -1));
    expectEquals(1, compareInts(1, 0));
    expectEquals(1, compareInts(Integer.MAX_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareInts(Integer.MAX_VALUE, -1));
    expectEquals(1, compareInts(Integer.MAX_VALUE, 0));
    expectEquals(1, compareInts(Integer.MAX_VALUE, 1));
    expectEquals(1, compareInts(Integer.MAX_VALUE, Integer.MAX_VALUE - 1));

    for (int i = -11; i <= 11; i++) {
      for (int j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareInts(i, j));
      }
    }
  }

  public static void testCompareLongs() {
    expectEquals(-1, compareLongs(Long.MIN_VALUE, Long.MIN_VALUE + 1L));
    expectEquals(-1, compareLongs(Long.MIN_VALUE, -1L));
    expectEquals(-1, compareLongs(Long.MIN_VALUE, 0L));
    expectEquals(-1, compareLongs(Long.MIN_VALUE, 1L));
    expectEquals(-1, compareLongs(Long.MIN_VALUE, Long.MAX_VALUE));
    expectEquals(-1, compareLongs(-1L, 0L));
    expectEquals(-1, compareLongs(-1L, 1L));
    expectEquals(-1, compareLongs(0L, 1L));

    expectEquals(0, compareLongs(Long.MIN_VALUE, Long.MIN_VALUE));
    expectEquals(0, compareLongs(-1L, -1L));
    expectEquals(0, compareLongs(0L, 0L));
    expectEquals(0, compareLongs(1L, 1L));
    expectEquals(0, compareLongs(Long.MAX_VALUE, Long.MAX_VALUE));

    expectEquals(1, compareLongs(0L, -1L));
    expectEquals(1, compareLongs(1L, -1L));
    expectEquals(1, compareLongs(1L, 0L));
    expectEquals(1, compareLongs(Long.MAX_VALUE, Long.MIN_VALUE));
    expectEquals(1, compareLongs(Long.MAX_VALUE, -1L));
    expectEquals(1, compareLongs(Long.MAX_VALUE, 0L));
    expectEquals(1, compareLongs(Long.MAX_VALUE, 1L));
    expectEquals(1, compareLongs(Long.MAX_VALUE, Long.MAX_VALUE - 1L));

    expectEquals(-1, compareLongs(0x111111117FFFFFFFL, 0x11111111FFFFFFFFL));
    expectEquals(0, compareLongs(0x111111117FFFFFFFL, 0x111111117FFFFFFFL));
    expectEquals(1, compareLongs(0x11111111FFFFFFFFL, 0x111111117FFFFFFFL));

    for (long i = -11L; i <= 11L; i++) {
      for (long j = -11L; j <= 11L; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareLongs(i, j));
      }
    }

    for (long i = Long.MIN_VALUE; i <= Long.MIN_VALUE + 11L; i++) {
      expectEquals(-1, compareLongs(i, 0));
    }

    for (long i = Long.MAX_VALUE; i >= Long.MAX_VALUE - 11L; i--) {
      expectEquals(1, compareLongs(i, 0));
    }
  }


  public static void testCompareByteShort() {
    expectEquals(-1, compareByteShort(Byte.MIN_VALUE, (short)-1));
    expectEquals(-1, compareByteShort(Byte.MIN_VALUE, (short)0));
    expectEquals(-1, compareByteShort(Byte.MIN_VALUE, (short)1));
    expectEquals(-1, compareByteShort(Byte.MIN_VALUE, Short.MAX_VALUE));
    expectEquals(-1, compareByteShort((byte)-1, (short)0));
    expectEquals(-1, compareByteShort((byte)-1, (short)1));
    expectEquals(-1, compareByteShort((byte)0, (short)1));
    expectEquals(-1, compareByteShort(Byte.MAX_VALUE, (short)(Short.MAX_VALUE - 1)));
    expectEquals(-1, compareByteShort(Byte.MAX_VALUE, Short.MAX_VALUE));

    expectEquals(0, compareByteShort((byte)-1, (short)-1));
    expectEquals(0, compareByteShort((byte)0, (short)0));
    expectEquals(0, compareByteShort((byte)1, (short)1));

    expectEquals(1, compareByteShort(Byte.MIN_VALUE, Short.MIN_VALUE));
    expectEquals(1, compareByteShort(Byte.MIN_VALUE, (short)(Short.MIN_VALUE + 1)));
    expectEquals(1, compareByteShort((byte)0, (short)-1));
    expectEquals(1, compareByteShort((byte)1, (short)-1));
    expectEquals(1, compareByteShort((byte)1, (short)0));
    expectEquals(1, compareByteShort(Byte.MAX_VALUE, Short.MIN_VALUE));
    expectEquals(1, compareByteShort(Byte.MAX_VALUE, (short)-1));
    expectEquals(1, compareByteShort(Byte.MAX_VALUE, (short)0));
    expectEquals(1, compareByteShort(Byte.MAX_VALUE, (short)1));

    for (byte i = -11; i <= 11; i++) {
      for (short j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareByteShort(i, j));
      }
    }
  }

  public static void testCompareByteChar() {
    expectEquals(-1, compareByteChar(Byte.MIN_VALUE, (char)0));
    expectEquals(-1, compareByteChar(Byte.MIN_VALUE, (char)1));
    expectEquals(-1, compareByteChar(Byte.MIN_VALUE, Character.MAX_VALUE));
    expectEquals(-1, compareByteChar((byte)-1, (char)0));
    expectEquals(-1, compareByteChar((byte)-1, (char)1));
    expectEquals(-1, compareByteChar((byte)0, (char)1));
    expectEquals(-1, compareByteChar(Byte.MAX_VALUE, (char)(Character.MAX_VALUE - 1)));
    expectEquals(-1, compareByteChar(Byte.MAX_VALUE, Character.MAX_VALUE));

    expectEquals(0, compareByteChar((byte)0, (char)0));
    expectEquals(0, compareByteChar((byte)1, (char)1));

    expectEquals(1, compareByteChar((byte)1, (char)0));
    expectEquals(1, compareByteChar(Byte.MAX_VALUE, (char)0));
    expectEquals(1, compareByteChar(Byte.MAX_VALUE, (char)1));

    for (byte i = -11; i <= 11; i++) {
      for (char j = 0; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareByteChar(i, j));
      }
    }
  }

  public static void testCompareByteInt() {
    expectEquals(-1, compareByteInt(Byte.MIN_VALUE, -1));
    expectEquals(-1, compareByteInt(Byte.MIN_VALUE, 0));
    expectEquals(-1, compareByteInt(Byte.MIN_VALUE, 1));
    expectEquals(-1, compareByteInt(Byte.MIN_VALUE, Integer.MAX_VALUE));
    expectEquals(-1, compareByteInt((byte)-1, 0));
    expectEquals(-1, compareByteInt((byte)-1, 1));
    expectEquals(-1, compareByteInt((byte)0, 1));
    expectEquals(-1, compareByteInt(Byte.MAX_VALUE, Integer.MAX_VALUE - 1));
    expectEquals(-1, compareByteInt(Byte.MAX_VALUE, Integer.MAX_VALUE));

    expectEquals(0, compareByteInt((byte)-1, -1));
    expectEquals(0, compareByteInt((byte)0, 0));
    expectEquals(0, compareByteInt((byte)1, 1));

    expectEquals(1, compareByteInt(Byte.MIN_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareByteInt(Byte.MIN_VALUE, Integer.MIN_VALUE + 1));
    expectEquals(1, compareByteInt((byte)0, -1));
    expectEquals(1, compareByteInt((byte)1, -1));
    expectEquals(1, compareByteInt((byte)1, 0));
    expectEquals(1, compareByteInt(Byte.MAX_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareByteInt(Byte.MAX_VALUE, -1));
    expectEquals(1, compareByteInt(Byte.MAX_VALUE, 0));
    expectEquals(1, compareByteInt(Byte.MAX_VALUE, 1));

    for (byte i = -11; i <= 11; i++) {
      for (int j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareByteInt(i, j));
      }
    }
  }


  public static void testCompareShortByte() {
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, Byte.MIN_VALUE));
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, (byte)(Byte.MIN_VALUE + 1)));
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, (byte)-1));
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, (byte)0));
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, (byte)1));
    expectEquals(-1, compareShortByte(Short.MIN_VALUE, Byte.MAX_VALUE));
    expectEquals(-1, compareShortByte((short)-1, (byte)0));
    expectEquals(-1, compareShortByte((short)-1, (byte)1));
    expectEquals(-1, compareShortByte((short)0, (byte)1));

    expectEquals(0, compareShortByte((short)-1, (byte)-1));
    expectEquals(0, compareShortByte((short)0, (byte)0));
    expectEquals(0, compareShortByte((short)1, (byte)1));

    expectEquals(1, compareShortByte((short)0, (byte)-1));
    expectEquals(1, compareShortByte((short)1, (byte)-1));
    expectEquals(1, compareShortByte((short)1, (byte)0));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, Byte.MIN_VALUE));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, (byte)-1));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, (byte)0));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, (byte)1));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, (byte)(Byte.MAX_VALUE - 1)));
    expectEquals(1, compareShortByte(Short.MAX_VALUE, Byte.MAX_VALUE));

    for (short i = -11; i <= 11; i++) {
      for (byte j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareShortByte(i, j));
      }
    }
  }

  public static void testCompareShortChar() {
    expectEquals(-1, compareShortChar(Short.MIN_VALUE, (char)0));
    expectEquals(-1, compareShortChar(Short.MIN_VALUE, (char)1));
    expectEquals(-1, compareShortChar(Short.MIN_VALUE, Character.MAX_VALUE));
    expectEquals(-1, compareShortChar((short)-1, (char)0));
    expectEquals(-1, compareShortChar((short)-1, (char)1));
    expectEquals(-1, compareShortChar((short)0, (char)1));
    expectEquals(-1, compareShortChar(Short.MAX_VALUE, (char)(Character.MAX_VALUE - 1)));
    expectEquals(-1, compareShortChar(Short.MAX_VALUE, Character.MAX_VALUE));

    expectEquals(0, compareShortChar((short)0, (char)0));
    expectEquals(0, compareShortChar((short)1, (char)1));

    expectEquals(1, compareShortChar((short)1, (char)0));
    expectEquals(1, compareShortChar(Short.MAX_VALUE, (char)0));
    expectEquals(1, compareShortChar(Short.MAX_VALUE, (char)1));

    for (short i = -11; i <= 11; i++) {
      for (char j = 0; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareShortChar(i, j));
      }
    }
  }

  public static void testCompareShortInt() {
    expectEquals(-1, compareShortInt(Short.MIN_VALUE, -1));
    expectEquals(-1, compareShortInt(Short.MIN_VALUE, 0));
    expectEquals(-1, compareShortInt(Short.MIN_VALUE, 1));
    expectEquals(-1, compareShortInt(Short.MIN_VALUE, Integer.MAX_VALUE));
    expectEquals(-1, compareShortInt((short)-1, 0));
    expectEquals(-1, compareShortInt((short)-1, 1));
    expectEquals(-1, compareShortInt((short)0, 1));
    expectEquals(-1, compareShortInt(Short.MAX_VALUE, Integer.MAX_VALUE - 1));
    expectEquals(-1, compareShortInt(Short.MAX_VALUE, Integer.MAX_VALUE));

    expectEquals(0, compareShortInt((short)-1, -1));
    expectEquals(0, compareShortInt((short)0, 0));
    expectEquals(0, compareShortInt((short)1, 1));

    expectEquals(1, compareShortInt(Short.MIN_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareShortInt(Short.MIN_VALUE, Integer.MIN_VALUE + 1));
    expectEquals(1, compareShortInt((short)0, -1));
    expectEquals(1, compareShortInt((short)1, -1));
    expectEquals(1, compareShortInt((short)1, 0));
    expectEquals(1, compareShortInt(Short.MAX_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareShortInt(Short.MAX_VALUE, -1));
    expectEquals(1, compareShortInt(Short.MAX_VALUE, 0));
    expectEquals(1, compareShortInt(Short.MAX_VALUE, 1));

    for (short i = -11; i <= 11; i++) {
      for (int j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareShortInt(i, j));
      }
    }
  }


  public static void testCompareCharByte() {
    expectEquals(-1, compareCharByte((char)0, (byte)1));
    expectEquals(-1, compareCharByte((char)0, Byte.MAX_VALUE));

    expectEquals(0, compareCharByte((char)0, (byte)0));
    expectEquals(0, compareCharByte((char)1, (byte)1));

    expectEquals(1, compareCharByte((char)0, Byte.MIN_VALUE));
    expectEquals(1, compareCharByte((char)0, (byte)(Byte.MIN_VALUE + 1)));
    expectEquals(1, compareCharByte((char)0, (byte)-1));
    expectEquals(1, compareCharByte((char)1, (byte)-1));
    expectEquals(1, compareCharByte((char)1, (byte)0));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, Byte.MIN_VALUE));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, (byte)-1));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, (byte)0));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, (byte)1));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, (byte)(Byte.MAX_VALUE - 1)));
    expectEquals(1, compareCharByte(Character.MAX_VALUE, Byte.MAX_VALUE));

    for (char i = 0; i <= 11; i++) {
      for (byte j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareCharByte(i, j));
      }
    }
  }

  public static void testCompareCharShort() {
    expectEquals(-1, compareCharShort((char)0, (short)1));
    expectEquals(-1, compareCharShort((char)0, Short.MAX_VALUE));

    expectEquals(0, compareCharShort((char)0, (short)0));
    expectEquals(0, compareCharShort((char)1, (short)1));

    expectEquals(1, compareCharShort((char)0, Short.MIN_VALUE));
    expectEquals(1, compareCharShort((char)0, (short)(Short.MIN_VALUE + 1)));
    expectEquals(1, compareCharShort((char)0, (short)-1));
    expectEquals(1, compareCharShort((char)1, (short)-1));
    expectEquals(1, compareCharShort((char)1, (short)0));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, Short.MIN_VALUE));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, (short)-1));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, (short)0));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, (short)1));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, (short)(Short.MAX_VALUE - 1)));
    expectEquals(1, compareCharShort(Character.MAX_VALUE, Short.MAX_VALUE));

    for (char i = 0; i <= 11; i++) {
      for (short j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareCharShort(i, j));
      }
    }
  }

  public static void testCompareCharInt() {
    expectEquals(-1, compareCharInt((char)0, 1));
    expectEquals(-1, compareCharInt((char)0, Integer.MAX_VALUE));
    expectEquals(-1, compareCharInt(Character.MAX_VALUE, Integer.MAX_VALUE - 1));
    expectEquals(-1, compareCharInt(Character.MAX_VALUE, Integer.MAX_VALUE));

    expectEquals(0, compareCharInt((char)0, 0));
    expectEquals(0, compareCharInt((char)1, 1));

    expectEquals(1, compareCharInt((char)0, Integer.MIN_VALUE));
    expectEquals(1, compareCharInt((char)0, Integer.MIN_VALUE + 1));
    expectEquals(1, compareCharInt((char)0, -1));
    expectEquals(1, compareCharInt((char)1, -1));
    expectEquals(1, compareCharInt((char)1, 0));
    expectEquals(1, compareCharInt(Character.MAX_VALUE, Integer.MIN_VALUE));
    expectEquals(1, compareCharInt(Character.MAX_VALUE, -1));
    expectEquals(1, compareCharInt(Character.MAX_VALUE, 0));
    expectEquals(1, compareCharInt(Character.MAX_VALUE, 1));

    for (char i = 0; i <= 11; i++) {
      for (int j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareCharInt(i, j));
      }
    }
  }


  public static void testCompareIntByte() {
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, Byte.MIN_VALUE));
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, (byte)(Byte.MIN_VALUE + 1)));
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, (byte)-1));
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, (byte)0));
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, (byte)1));
    expectEquals(-1, compareIntByte(Integer.MIN_VALUE, Byte.MAX_VALUE));
    expectEquals(-1, compareIntByte(-1, (byte)0));
    expectEquals(-1, compareIntByte(-1, (byte)1));
    expectEquals(-1, compareIntByte(0, (byte)1));

    expectEquals(0, compareIntByte(-1, (byte)-1));
    expectEquals(0, compareIntByte(0, (byte)0));
    expectEquals(0, compareIntByte(1, (byte)1));

    expectEquals(1, compareIntByte(0, (byte)-1));
    expectEquals(1, compareIntByte(1, (byte)-1));
    expectEquals(1, compareIntByte(1, (byte)0));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, Byte.MIN_VALUE));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, (byte)-1));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, (byte)0));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, (byte)1));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, (byte)(Byte.MAX_VALUE - 1)));
    expectEquals(1, compareIntByte(Integer.MAX_VALUE, Byte.MAX_VALUE));

    for (int i = -11; i <= 11; i++) {
      for (byte j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareIntByte(i, j));
      }
    }
  }

  public static void testCompareIntShort() {
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, Short.MIN_VALUE));
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, (short)(Short.MIN_VALUE + 1)));
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, (short)-1));
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, (short)0));
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, (short)1));
    expectEquals(-1, compareIntShort(Integer.MIN_VALUE, Short.MAX_VALUE));
    expectEquals(-1, compareIntShort(-1, (short)0));
    expectEquals(-1, compareIntShort(-1, (short)1));
    expectEquals(-1, compareIntShort(0, (short)1));

    expectEquals(0, compareIntShort(-1, (short)-1));
    expectEquals(0, compareIntShort(0, (short)0));
    expectEquals(0, compareIntShort(1, (short)1));

    expectEquals(1, compareIntShort(0, (short)-1));
    expectEquals(1, compareIntShort(1, (short)-1));
    expectEquals(1, compareIntShort(1, (short)0));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, Short.MIN_VALUE));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, (short)-1));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, (short)0));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, (short)1));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, (short)(Short.MAX_VALUE - 1)));
    expectEquals(1, compareIntShort(Integer.MAX_VALUE, Short.MAX_VALUE));

    for (int i = -11; i <= 11; i++) {
      for (short j = -11; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareIntShort(i, j));
      }
    }
  }

  public static void testCompareIntChar() {
    expectEquals(-1, compareIntChar(Integer.MIN_VALUE, (char)0));
    expectEquals(-1, compareIntChar(Integer.MIN_VALUE, (char)1));
    expectEquals(-1, compareIntChar(Integer.MIN_VALUE, Character.MAX_VALUE));
    expectEquals(-1, compareIntChar(-1, (char)0));
    expectEquals(-1, compareIntChar(-1, (char)1));
    expectEquals(-1, compareIntChar(0, (char)1));

    expectEquals(0, compareIntChar(0, (char)0));
    expectEquals(0, compareIntChar(1, (char)1));

    expectEquals(1, compareIntChar(1, (char)0));
    expectEquals(1, compareIntChar(Integer.MAX_VALUE, (char)0));
    expectEquals(1, compareIntChar(Integer.MAX_VALUE, (char)1));
    expectEquals(1, compareIntChar(Integer.MAX_VALUE, (char)(Character.MAX_VALUE - 1)));
    expectEquals(1, compareIntChar(Integer.MAX_VALUE, Character.MAX_VALUE));

    for (int i = -11; i <= 11; i++) {
      for (char j = 0; j <= 11; j++) {
        int expected = 0;
        if (i < j) expected = -1;
        else if (i > j) expected = 1;
        expectEquals(expected, compareIntChar(i, j));
      }
    }
  }


  public static void main(String args[]) throws Exception {
    $opt$noinline$testReplaceInputWithItself(42);

    testCompareBooleans();
    testCompareBytes();
    testCompareShorts();
    testCompareChars();
    testCompareInts();
    testCompareLongs();

    testCompareByteShort();
    testCompareByteChar();
    testCompareByteInt();

    testCompareShortByte();
    testCompareShortChar();
    testCompareShortInt();

    testCompareCharByte();
    testCompareCharShort();
    testCompareCharInt();

    testCompareIntByte();
    testCompareIntShort();
    testCompareIntChar();

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
