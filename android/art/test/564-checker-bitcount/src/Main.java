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

  // TODO: make something like this work when b/26700769 is done.
  // CHECK-START-X86_64: int Main.bits32(int) disassembly (after)
  // CHECK-DAG: popcnt


  /// CHECK-START: int Main.$noinline$BitCountBoolean(boolean) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountBoolean(boolean x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Integer.bitCount(x ? 1 : 0);
  }

  /// CHECK-START: int Main.$noinline$BitCountByte(byte) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountByte(byte x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.$noinline$BitCountShort(short) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountShort(short x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.$noinline$BitCountChar(char) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountChar(char x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.$noinline$BitCountInt(int) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:IntegerBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountInt(int x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Integer.bitCount(x);
  }

  /// CHECK-START: int Main.$noinline$BitCountLong(long) intrinsics_recognition (after)
  /// CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect intrinsic:LongBitCount
  /// CHECK-DAG:                      Return [<<Result>>]
  private static int $noinline$BitCountLong(long x) {
    if (doThrow) { throw new Error(); }  // Try defeating inlining.
    return Long.bitCount(x);
  }

  public static void testBitCountBoolean() {
    expectEqualsInt($noinline$BitCountBoolean(false), 0);
    expectEqualsInt($noinline$BitCountBoolean(true), 1);
  }

  public static void testBitCountByte() {
    // Number of bits in an 32-bit integer representing the sign
    // extension of a byte value widened to an int.
    int signExtensionSize = Integer.SIZE - Byte.SIZE;
    // Sign bit position in a byte.
    int signBit = Byte.SIZE - 1;

    expectEqualsInt($noinline$BitCountByte((byte) 0x00), 0);
    expectEqualsInt($noinline$BitCountByte((byte) 0x01), 1);
    expectEqualsInt($noinline$BitCountByte((byte) 0x10), 1);
    expectEqualsInt($noinline$BitCountByte((byte) 0x11), 2);
    expectEqualsInt($noinline$BitCountByte((byte) 0x03), 2);
    expectEqualsInt($noinline$BitCountByte((byte) 0x70), 3);
    expectEqualsInt($noinline$BitCountByte((byte) 0xF0), 4 + signExtensionSize);
    expectEqualsInt($noinline$BitCountByte((byte) 0x0F), 4);
    expectEqualsInt($noinline$BitCountByte((byte) 0x12), 2);
    expectEqualsInt($noinline$BitCountByte((byte) 0x9A), 4 + signExtensionSize);
    expectEqualsInt($noinline$BitCountByte((byte) 0xFF), 8 + signExtensionSize);

    for (int i = 0; i < Byte.SIZE; i++) {
      expectEqualsInt($noinline$BitCountByte((byte) (1 << i)),
                      (i < signBit) ? 1 : 1 + signExtensionSize);
    }
  }

  public static void testBitCountShort() {
    // Number of bits in an 32-bit integer representing the sign
    // extension of a short value widened to an int.
    int signExtensionSize = Integer.SIZE - Short.SIZE;
    // Sign bit position in a short.
    int signBit = Short.SIZE - 1;

    expectEqualsInt($noinline$BitCountShort((short) 0x0000), 0);
    expectEqualsInt($noinline$BitCountShort((short) 0x0001), 1);
    expectEqualsInt($noinline$BitCountShort((short) 0x1000), 1);
    expectEqualsInt($noinline$BitCountShort((short) 0x1001), 2);
    expectEqualsInt($noinline$BitCountShort((short) 0x0003), 2);
    expectEqualsInt($noinline$BitCountShort((short) 0x7000), 3);
    expectEqualsInt($noinline$BitCountShort((short) 0x0F00), 4);
    expectEqualsInt($noinline$BitCountShort((short) 0x0011), 2);
    expectEqualsInt($noinline$BitCountShort((short) 0x1100), 2);
    expectEqualsInt($noinline$BitCountShort((short) 0x1111), 4);
    expectEqualsInt($noinline$BitCountShort((short) 0x1234), 5);
    expectEqualsInt($noinline$BitCountShort((short) 0x9ABC), 9 + signExtensionSize);
    expectEqualsInt($noinline$BitCountShort((short) 0xFFFF), 16 + signExtensionSize);

    for (int i = 0; i < Short.SIZE; i++) {
      expectEqualsInt($noinline$BitCountShort((short) (1 << i)),
                      (i < signBit) ? 1 : 1 + signExtensionSize);
    }
  }

  public static void testBitCountChar() {
    expectEqualsInt($noinline$BitCountChar((char) 0x0000), 0);
    expectEqualsInt($noinline$BitCountChar((char) 0x0001), 1);
    expectEqualsInt($noinline$BitCountChar((char) 0x1000), 1);
    expectEqualsInt($noinline$BitCountChar((char) 0x1001), 2);
    expectEqualsInt($noinline$BitCountChar((char) 0x0003), 2);
    expectEqualsInt($noinline$BitCountChar((char) 0x7000), 3);
    expectEqualsInt($noinline$BitCountChar((char) 0x0F00), 4);
    expectEqualsInt($noinline$BitCountChar((char) 0x0011), 2);
    expectEqualsInt($noinline$BitCountChar((char) 0x1100), 2);
    expectEqualsInt($noinline$BitCountChar((char) 0x1111), 4);
    expectEqualsInt($noinline$BitCountChar((char) 0x1234), 5);
    expectEqualsInt($noinline$BitCountChar((char) 0x9ABC), 9);
    expectEqualsInt($noinline$BitCountChar((char) 0xFFFF), 16);

    for (int i = 0; i < Character.SIZE; i++) {
      expectEqualsInt($noinline$BitCountChar((char) (1 << i)), 1);
    }
  }

  public static void testBitCountInt() {
    expectEqualsInt($noinline$BitCountInt(0x00000000), 0);
    expectEqualsInt($noinline$BitCountInt(0x00000001), 1);
    expectEqualsInt($noinline$BitCountInt(0x10000000), 1);
    expectEqualsInt($noinline$BitCountInt(0x10000001), 2);
    expectEqualsInt($noinline$BitCountInt(0x00000003), 2);
    expectEqualsInt($noinline$BitCountInt(0x70000000), 3);
    expectEqualsInt($noinline$BitCountInt(0x000F0000), 4);
    expectEqualsInt($noinline$BitCountInt(0x00001111), 4);
    expectEqualsInt($noinline$BitCountInt(0x11110000), 4);
    expectEqualsInt($noinline$BitCountInt(0x11111111), 8);
    expectEqualsInt($noinline$BitCountInt(0x12345678), 13);
    expectEqualsInt($noinline$BitCountInt(0x9ABCDEF0), 19);
    expectEqualsInt($noinline$BitCountInt(0xFFFFFFFF), 32);

    for (int i = 0; i < Integer.SIZE; i++) {
      expectEqualsInt($noinline$BitCountInt(1 << i), 1);
    }
  }

  public static void testBitCountLong() {
    expectEqualsInt($noinline$BitCountLong(0x0000000000000000L), 0);
    expectEqualsInt($noinline$BitCountLong(0x0000000000000001L), 1);
    expectEqualsInt($noinline$BitCountLong(0x1000000000000000L), 1);
    expectEqualsInt($noinline$BitCountLong(0x1000000000000001L), 2);
    expectEqualsInt($noinline$BitCountLong(0x0000000000000003L), 2);
    expectEqualsInt($noinline$BitCountLong(0x7000000000000000L), 3);
    expectEqualsInt($noinline$BitCountLong(0x000F000000000000L), 4);
    expectEqualsInt($noinline$BitCountLong(0x0000000011111111L), 8);
    expectEqualsInt($noinline$BitCountLong(0x1111111100000000L), 8);
    expectEqualsInt($noinline$BitCountLong(0x1111111111111111L), 16);
    expectEqualsInt($noinline$BitCountLong(0x123456789ABCDEF1L), 33);
    expectEqualsInt($noinline$BitCountLong(0xFFFFFFFFFFFFFFFFL), 64);

    for (int i = 0; i < Long.SIZE; i++) {
      expectEqualsInt($noinline$BitCountLong(1L << i), 1);
    }
  }

  public static void main(String args[]) {
    testBitCountBoolean();
    testBitCountByte();
    testBitCountShort();
    testBitCountChar();
    testBitCountInt();
    testBitCountLong();

    System.out.println("passed");
  }

  private static void expectEqualsInt(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static boolean doThrow = false;
}
