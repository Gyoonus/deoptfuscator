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

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.
public class Main {

  public static void assertByteEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertShortEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
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

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      // Values are cast to int to display numeric values instead of
      // (UTF-16 encoded) characters.
      throw new Error("Expected: " + (int)expected + ", found: " + (int)result);
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

  public static void assertFloatIsNaN(float result) {
    if (!Float.isNaN(result)) {
      throw new Error("Expected: NaN, found: " + result);
    }
  }

  public static void assertDoubleIsNaN(double result) {
    if (!Double.isNaN(result)) {
      throw new Error("Expected: NaN, found: " + result);
    }
  }


  public static void main(String[] args) {
    // Generate, compile and check int-to-long Dex instructions.
    byteToLong();
    shortToLong();
    intToLong();
    charToLong();

    // Generate, compile and check int-to-float Dex instructions.
    byteToFloat();
    shortToFloat();
    intToFloat();
    charToFloat();

    // Generate, compile and check int-to-double Dex instructions.
    byteToDouble();
    shortToDouble();
    intToDouble();
    charToDouble();

    // Generate, compile and check long-to-int Dex instructions.
    longToInt();

    // Generate, compile and check long-to-float Dex instructions.
    longToFloat();

    // Generate, compile and check long-to-double Dex instructions.
    longToDouble();

    // Generate, compile and check float-to-int Dex instructions.
    floatToInt();

    // Generate, compile and check float-to-long Dex instructions.
    floatToLong();

    // Generate, compile and check float-to-double Dex instructions.
    floatToDouble();

    // Generate, compile and check double-to-int Dex instructions.
    doubleToInt();

    // Generate, compile and check double-to-long Dex instructions.
    doubleToLong();

    // Generate, compile and check double-to-float Dex instructions.
    doubleToFloat();

    // Generate, compile and check int-to-byte Dex instructions.
    shortToByte();
    intToByte();
    charToByte();

    // Generate, compile and check int-to-short Dex instructions.
    byteToShort();
    intToShort();
    charToShort();

    // Generate, compile and check int-to-char Dex instructions.
    byteToChar();
    shortToChar();
    intToChar();
  }

  private static void byteToLong() {
    assertLongEquals(1L, $opt$noinline$ByteToLong((byte)1));
    assertLongEquals(0L, $opt$noinline$ByteToLong((byte)0));
    assertLongEquals(-1L, $opt$noinline$ByteToLong((byte)-1));
    assertLongEquals(51L, $opt$noinline$ByteToLong((byte)51));
    assertLongEquals(-51L, $opt$noinline$ByteToLong((byte)-51));
    assertLongEquals(127L, $opt$noinline$ByteToLong((byte)127));  // 2^7 - 1
    assertLongEquals(-127L, $opt$noinline$ByteToLong((byte)-127));  // -(2^7 - 1)
    assertLongEquals(-128L, $opt$noinline$ByteToLong((byte)-128));  // -(2^7)
  }

  private static void shortToLong() {
    assertLongEquals(1L, $opt$noinline$ShortToLong((short)1));
    assertLongEquals(0L, $opt$noinline$ShortToLong((short)0));
    assertLongEquals(-1L, $opt$noinline$ShortToLong((short)-1));
    assertLongEquals(51L, $opt$noinline$ShortToLong((short)51));
    assertLongEquals(-51L, $opt$noinline$ShortToLong((short)-51));
    assertLongEquals(32767L, $opt$noinline$ShortToLong((short)32767));  // 2^15 - 1
    assertLongEquals(-32767L, $opt$noinline$ShortToLong((short)-32767));  // -(2^15 - 1)
    assertLongEquals(-32768L, $opt$noinline$ShortToLong((short)-32768));  // -(2^15)
  }

  private static void intToLong() {
    assertLongEquals(1L, $opt$noinline$IntToLong(1));
    assertLongEquals(0L, $opt$noinline$IntToLong(0));
    assertLongEquals(-1L, $opt$noinline$IntToLong(-1));
    assertLongEquals(51L, $opt$noinline$IntToLong(51));
    assertLongEquals(-51L, $opt$noinline$IntToLong(-51));
    assertLongEquals(2147483647L, $opt$noinline$IntToLong(2147483647));  // 2^31 - 1
    assertLongEquals(-2147483647L, $opt$noinline$IntToLong(-2147483647));  // -(2^31 - 1)
    assertLongEquals(-2147483648L, $opt$noinline$IntToLong(-2147483648));  // -(2^31)
  }

  private static void charToLong() {
    assertLongEquals(1L, $opt$noinline$CharToLong((char)1));
    assertLongEquals(0L, $opt$noinline$CharToLong((char)0));
    assertLongEquals(51L, $opt$noinline$CharToLong((char)51));
    assertLongEquals(32767L, $opt$noinline$CharToLong((char)32767));  // 2^15 - 1
    assertLongEquals(65535L, $opt$noinline$CharToLong((char)65535));  // 2^16 - 1
    assertLongEquals(65535L, $opt$noinline$CharToLong((char)-1));
    assertLongEquals(65485L, $opt$noinline$CharToLong((char)-51));
    assertLongEquals(32769L, $opt$noinline$CharToLong((char)-32767));  // -(2^15 - 1)
    assertLongEquals(32768L, $opt$noinline$CharToLong((char)-32768));  // -(2^15)
  }

  private static void byteToFloat() {
    assertFloatEquals(1F, $opt$noinline$ByteToFloat((byte)1));
    assertFloatEquals(0F, $opt$noinline$ByteToFloat((byte)0));
    assertFloatEquals(-1F, $opt$noinline$ByteToFloat((byte)-1));
    assertFloatEquals(51F, $opt$noinline$ByteToFloat((byte)51));
    assertFloatEquals(-51F, $opt$noinline$ByteToFloat((byte)-51));
    assertFloatEquals(127F, $opt$noinline$ByteToFloat((byte)127));  // 2^7 - 1
    assertFloatEquals(-127F, $opt$noinline$ByteToFloat((byte)-127));  // -(2^7 - 1)
    assertFloatEquals(-128F, $opt$noinline$ByteToFloat((byte)-128));  // -(2^7)
  }

  private static void shortToFloat() {
    assertFloatEquals(1F, $opt$noinline$ShortToFloat((short)1));
    assertFloatEquals(0F, $opt$noinline$ShortToFloat((short)0));
    assertFloatEquals(-1F, $opt$noinline$ShortToFloat((short)-1));
    assertFloatEquals(51F, $opt$noinline$ShortToFloat((short)51));
    assertFloatEquals(-51F, $opt$noinline$ShortToFloat((short)-51));
    assertFloatEquals(32767F, $opt$noinline$ShortToFloat((short)32767));  // 2^15 - 1
    assertFloatEquals(-32767F, $opt$noinline$ShortToFloat((short)-32767));  // -(2^15 - 1)
    assertFloatEquals(-32768F, $opt$noinline$ShortToFloat((short)-32768));  // -(2^15)
  }

  private static void intToFloat() {
    assertFloatEquals(1F, $opt$noinline$IntToFloat(1));
    assertFloatEquals(0F, $opt$noinline$IntToFloat(0));
    assertFloatEquals(-1F, $opt$noinline$IntToFloat(-1));
    assertFloatEquals(51F, $opt$noinline$IntToFloat(51));
    assertFloatEquals(-51F, $opt$noinline$IntToFloat(-51));
    assertFloatEquals(16777215F, $opt$noinline$IntToFloat(16777215));  // 2^24 - 1
    assertFloatEquals(-16777215F, $opt$noinline$IntToFloat(-16777215));  // -(2^24 - 1)
    assertFloatEquals(16777216F, $opt$noinline$IntToFloat(16777216));  // 2^24
    assertFloatEquals(-16777216F, $opt$noinline$IntToFloat(-16777216));  // -(2^24)
    assertFloatEquals(2147483647F, $opt$noinline$IntToFloat(2147483647));  // 2^31 - 1
    assertFloatEquals(-2147483648F, $opt$noinline$IntToFloat(-2147483648));  // -(2^31)
  }

  private static void charToFloat() {
    assertFloatEquals(1F, $opt$noinline$CharToFloat((char)1));
    assertFloatEquals(0F, $opt$noinline$CharToFloat((char)0));
    assertFloatEquals(51F, $opt$noinline$CharToFloat((char)51));
    assertFloatEquals(32767F, $opt$noinline$CharToFloat((char)32767));  // 2^15 - 1
    assertFloatEquals(65535F, $opt$noinline$CharToFloat((char)65535));  // 2^16 - 1
    assertFloatEquals(65535F, $opt$noinline$CharToFloat((char)-1));
    assertFloatEquals(65485F, $opt$noinline$CharToFloat((char)-51));
    assertFloatEquals(32769F, $opt$noinline$CharToFloat((char)-32767));  // -(2^15 - 1)
    assertFloatEquals(32768F, $opt$noinline$CharToFloat((char)-32768));  // -(2^15)
  }

  private static void byteToDouble() {
    assertDoubleEquals(1D, $opt$noinline$ByteToDouble((byte)1));
    assertDoubleEquals(0D, $opt$noinline$ByteToDouble((byte)0));
    assertDoubleEquals(-1D, $opt$noinline$ByteToDouble((byte)-1));
    assertDoubleEquals(51D, $opt$noinline$ByteToDouble((byte)51));
    assertDoubleEquals(-51D, $opt$noinline$ByteToDouble((byte)-51));
    assertDoubleEquals(127D, $opt$noinline$ByteToDouble((byte)127));  // 2^7 - 1
    assertDoubleEquals(-127D, $opt$noinline$ByteToDouble((byte)-127));  // -(2^7 - 1)
    assertDoubleEquals(-128D, $opt$noinline$ByteToDouble((byte)-128));  // -(2^7)
  }

  private static void shortToDouble() {
    assertDoubleEquals(1D, $opt$noinline$ShortToDouble((short)1));
    assertDoubleEquals(0D, $opt$noinline$ShortToDouble((short)0));
    assertDoubleEquals(-1D, $opt$noinline$ShortToDouble((short)-1));
    assertDoubleEquals(51D, $opt$noinline$ShortToDouble((short)51));
    assertDoubleEquals(-51D, $opt$noinline$ShortToDouble((short)-51));
    assertDoubleEquals(32767D, $opt$noinline$ShortToDouble((short)32767));  // 2^15 - 1
    assertDoubleEquals(-32767D, $opt$noinline$ShortToDouble((short)-32767));  // -(2^15 - 1)
    assertDoubleEquals(-32768D, $opt$noinline$ShortToDouble((short)-32768));  // -(2^15)
  }

  private static void intToDouble() {
    assertDoubleEquals(1D, $opt$noinline$IntToDouble(1));
    assertDoubleEquals(0D, $opt$noinline$IntToDouble(0));
    assertDoubleEquals(-1D, $opt$noinline$IntToDouble(-1));
    assertDoubleEquals(51D, $opt$noinline$IntToDouble(51));
    assertDoubleEquals(-51D, $opt$noinline$IntToDouble(-51));
    assertDoubleEquals(16777216D, $opt$noinline$IntToDouble(16777216));  // 2^24
    assertDoubleEquals(-16777216D, $opt$noinline$IntToDouble(-16777216));  // -(2^24)
    assertDoubleEquals(2147483647D, $opt$noinline$IntToDouble(2147483647));  // 2^31 - 1
    assertDoubleEquals(-2147483648D, $opt$noinline$IntToDouble(-2147483648));  // -(2^31)
  }

  private static void charToDouble() {
    assertDoubleEquals(1D, $opt$noinline$CharToDouble((char)1));
    assertDoubleEquals(0D, $opt$noinline$CharToDouble((char)0));
    assertDoubleEquals(51D, $opt$noinline$CharToDouble((char)51));
    assertDoubleEquals(32767D, $opt$noinline$CharToDouble((char)32767));  // 2^15 - 1
    assertDoubleEquals(65535D, $opt$noinline$CharToDouble((char)65535));  // 2^16 - 1
    assertDoubleEquals(65535D, $opt$noinline$CharToDouble((char)-1));
    assertDoubleEquals(65485D, $opt$noinline$CharToDouble((char)-51));
    assertDoubleEquals(32769D, $opt$noinline$CharToDouble((char)-32767));  // -(2^15 - 1)
    assertDoubleEquals(32768D, $opt$noinline$CharToDouble((char)-32768));  // -(2^15)
  }

  private static void longToInt() {
    assertIntEquals(1, $opt$noinline$LongToInt(1L));
    assertIntEquals(0, $opt$noinline$LongToInt(0L));
    assertIntEquals(-1, $opt$noinline$LongToInt(-1L));
    assertIntEquals(51, $opt$noinline$LongToInt(51L));
    assertIntEquals(-51, $opt$noinline$LongToInt(-51L));
    assertIntEquals(2147483647, $opt$noinline$LongToInt(2147483647L));  // 2^31 - 1
    assertIntEquals(-2147483647, $opt$noinline$LongToInt(-2147483647L));  // -(2^31 - 1)
    assertIntEquals(-2147483648, $opt$noinline$LongToInt(-2147483648L));  // -(2^31)
    assertIntEquals(-2147483648, $opt$noinline$LongToInt(2147483648L));  // (2^31)
    assertIntEquals(2147483647, $opt$noinline$LongToInt(-2147483649L));  // -(2^31 + 1)
    assertIntEquals(-1, $opt$noinline$LongToInt(9223372036854775807L));  // 2^63 - 1
    assertIntEquals(1, $opt$noinline$LongToInt(-9223372036854775807L));  // -(2^63 - 1)
    assertIntEquals(0, $opt$noinline$LongToInt(-9223372036854775808L));  // -(2^63)

    assertIntEquals(42, $opt$noinline$LongLiteralToInt());

    // Ensure long-to-int conversions truncates values as expected.
    assertLongEquals(1L, $opt$noinline$IntToLong($opt$noinline$LongToInt(4294967297L)));  // 2^32 + 1
    assertLongEquals(0L, $opt$noinline$IntToLong($opt$noinline$LongToInt(4294967296L)));  // 2^32
    assertLongEquals(-1L, $opt$noinline$IntToLong($opt$noinline$LongToInt(4294967295L)));  // 2^32 - 1
    assertLongEquals(0L, $opt$noinline$IntToLong($opt$noinline$LongToInt(0L)));
    assertLongEquals(1L, $opt$noinline$IntToLong($opt$noinline$LongToInt(-4294967295L)));  // -(2^32 - 1)
    assertLongEquals(0L, $opt$noinline$IntToLong($opt$noinline$LongToInt(-4294967296L)));  // -(2^32)
    assertLongEquals(-1, $opt$noinline$IntToLong($opt$noinline$LongToInt(-4294967297L)));  // -(2^32 + 1)
  }

  private static void longToFloat() {
    assertFloatEquals(1F, $opt$noinline$LongToFloat(1L));
    assertFloatEquals(0F, $opt$noinline$LongToFloat(0L));
    assertFloatEquals(-1F, $opt$noinline$LongToFloat(-1L));
    assertFloatEquals(51F, $opt$noinline$LongToFloat(51L));
    assertFloatEquals(-51F, $opt$noinline$LongToFloat(-51L));
    assertFloatEquals(2147483647F, $opt$noinline$LongToFloat(2147483647L));  // 2^31 - 1
    assertFloatEquals(-2147483647F, $opt$noinline$LongToFloat(-2147483647L));  // -(2^31 - 1)
    assertFloatEquals(-2147483648F, $opt$noinline$LongToFloat(-2147483648L));  // -(2^31)
    assertFloatEquals(2147483648F, $opt$noinline$LongToFloat(2147483648L));  // (2^31)
    assertFloatEquals(-2147483649F, $opt$noinline$LongToFloat(-2147483649L));  // -(2^31 + 1)
    assertFloatEquals(4294967296F, $opt$noinline$LongToFloat(4294967296L));  // (2^32)
    assertFloatEquals(-4294967296F, $opt$noinline$LongToFloat(-4294967296L));  // -(2^32)
    assertFloatEquals(140739635871745F, $opt$noinline$LongToFloat(140739635871745L));  // 1 + 2^15 + 2^31 + 2^47
    assertFloatEquals(-140739635871745F, $opt$noinline$LongToFloat(-140739635871745L));  // -(1 + 2^15 + 2^31 + 2^47)
    assertFloatEquals(9223372036854775807F, $opt$noinline$LongToFloat(9223372036854775807L));  // 2^63 - 1
    assertFloatEquals(-9223372036854775807F, $opt$noinline$LongToFloat(-9223372036854775807L));  // -(2^63 - 1)
    assertFloatEquals(-9223372036854775808F, $opt$noinline$LongToFloat(-9223372036854775808L));  // -(2^63)
  }

  private static void longToDouble() {
    assertDoubleEquals(1D, $opt$noinline$LongToDouble(1L));
    assertDoubleEquals(0D, $opt$noinline$LongToDouble(0L));
    assertDoubleEquals(-1D, $opt$noinline$LongToDouble(-1L));
    assertDoubleEquals(51D, $opt$noinline$LongToDouble(51L));
    assertDoubleEquals(-51D, $opt$noinline$LongToDouble(-51L));
    assertDoubleEquals(2147483647D, $opt$noinline$LongToDouble(2147483647L));  // 2^31 - 1
    assertDoubleEquals(-2147483647D, $opt$noinline$LongToDouble(-2147483647L));  // -(2^31 - 1)
    assertDoubleEquals(-2147483648D, $opt$noinline$LongToDouble(-2147483648L));  // -(2^31)
    assertDoubleEquals(2147483648D, $opt$noinline$LongToDouble(2147483648L));  // (2^31)
    assertDoubleEquals(-2147483649D, $opt$noinline$LongToDouble(-2147483649L));  // -(2^31 + 1)
    assertDoubleEquals(4294967296D, $opt$noinline$LongToDouble(4294967296L));  // (2^32)
    assertDoubleEquals(-4294967296D, $opt$noinline$LongToDouble(-4294967296L));  // -(2^32)
    assertDoubleEquals(140739635871745D, $opt$noinline$LongToDouble(140739635871745L));  // 1 + 2^15 + 2^31 + 2^47
    assertDoubleEquals(-140739635871745D, $opt$noinline$LongToDouble(-140739635871745L));  // -(1 + 2^15 + 2^31 + 2^47)
    assertDoubleEquals(9223372036854775807D, $opt$noinline$LongToDouble(9223372036854775807L));  // 2^63 - 1
    assertDoubleEquals(-9223372036854775807D, $opt$noinline$LongToDouble(-9223372036854775807L));  // -(2^63 - 1)
    assertDoubleEquals(-9223372036854775808D, $opt$noinline$LongToDouble(-9223372036854775808L));  // -(2^63)
  }

  private static void floatToInt() {
    assertIntEquals(1, $opt$noinline$FloatToInt(1F));
    assertIntEquals(0, $opt$noinline$FloatToInt(0F));
    assertIntEquals(0, $opt$noinline$FloatToInt(-0F));
    assertIntEquals(-1, $opt$noinline$FloatToInt(-1F));
    assertIntEquals(51, $opt$noinline$FloatToInt(51F));
    assertIntEquals(-51, $opt$noinline$FloatToInt(-51F));
    assertIntEquals(0, $opt$noinline$FloatToInt(0.5F));
    assertIntEquals(0, $opt$noinline$FloatToInt(0.4999999F));
    assertIntEquals(0, $opt$noinline$FloatToInt(-0.4999999F));
    assertIntEquals(0, $opt$noinline$FloatToInt(-0.5F));
    assertIntEquals(42, $opt$noinline$FloatToInt(42.199F));
    assertIntEquals(-42, $opt$noinline$FloatToInt(-42.199F));
    assertIntEquals(2147483647, $opt$noinline$FloatToInt(2147483647F));  // 2^31 - 1
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(-2147483647F));  // -(2^31 - 1)
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(-2147483648F));  // -(2^31)
    assertIntEquals(2147483647, $opt$noinline$FloatToInt(2147483648F));  // (2^31)
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(-2147483649F));  // -(2^31 + 1)
    assertIntEquals(2147483647, $opt$noinline$FloatToInt(9223372036854775807F));  // 2^63 - 1
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(-9223372036854775807F));  // -(2^63 - 1)
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(-9223372036854775808F));  // -(2^63)
    assertIntEquals(0, $opt$noinline$FloatToInt(Float.NaN));
    assertIntEquals(2147483647, $opt$noinline$FloatToInt(Float.POSITIVE_INFINITY));
    assertIntEquals(-2147483648, $opt$noinline$FloatToInt(Float.NEGATIVE_INFINITY));
  }

  private static void floatToLong() {
    assertLongEquals(1L, $opt$noinline$FloatToLong(1F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(0F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(-0F));
    assertLongEquals(-1L, $opt$noinline$FloatToLong(-1F));
    assertLongEquals(51L, $opt$noinline$FloatToLong(51F));
    assertLongEquals(-51L, $opt$noinline$FloatToLong(-51F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(0.5F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(0.4999999F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(-0.4999999F));
    assertLongEquals(0L, $opt$noinline$FloatToLong(-0.5F));
    assertLongEquals(42L, $opt$noinline$FloatToLong(42.199F));
    assertLongEquals(-42L, $opt$noinline$FloatToLong(-42.199F));
    assertLongEquals(2147483648L, $opt$noinline$FloatToLong(2147483647F));  // 2^31 - 1
    assertLongEquals(-2147483648L, $opt$noinline$FloatToLong(-2147483647F));  // -(2^31 - 1)
    assertLongEquals(-2147483648L, $opt$noinline$FloatToLong(-2147483648F));  // -(2^31)
    assertLongEquals(2147483648L, $opt$noinline$FloatToLong(2147483648F));  // (2^31)
    assertLongEquals(-2147483648L, $opt$noinline$FloatToLong(-2147483649F));  // -(2^31 + 1)
    assertLongEquals(9223372036854775807L, $opt$noinline$FloatToLong(9223372036854775807F));  // 2^63 - 1
    assertLongEquals(-9223372036854775808L, $opt$noinline$FloatToLong(-9223372036854775807F));  // -(2^63 - 1)
    assertLongEquals(-9223372036854775808L, $opt$noinline$FloatToLong(-9223372036854775808F));  // -(2^63)
    assertLongEquals(9223371487098961920L, $opt$noinline$FloatToLong(9223371487098961920F));  // Math.nextAfter(2F^63, 0)
    assertLongEquals(-9223371487098961920L, $opt$noinline$FloatToLong(-9223371487098961920F));  // Math.nextAfter(-2F^63, 0)
    assertLongEquals(0L, $opt$noinline$FloatToLong(Float.NaN));
    assertLongEquals(9223372036854775807L, $opt$noinline$FloatToLong(Float.POSITIVE_INFINITY));
    assertLongEquals(-9223372036854775808L, $opt$noinline$FloatToLong(Float.NEGATIVE_INFINITY));
  }

  private static void floatToDouble() {
    assertDoubleEquals(1D, $opt$noinline$FloatToDouble(1F));
    assertDoubleEquals(0D, $opt$noinline$FloatToDouble(0F));
    assertDoubleEquals(0D, $opt$noinline$FloatToDouble(-0F));
    assertDoubleEquals(-1D, $opt$noinline$FloatToDouble(-1F));
    assertDoubleEquals(51D, $opt$noinline$FloatToDouble(51F));
    assertDoubleEquals(-51D, $opt$noinline$FloatToDouble(-51F));
    assertDoubleEquals(0.5D, $opt$noinline$FloatToDouble(0.5F));
    assertDoubleEquals(0.49999991059303284D, $opt$noinline$FloatToDouble(0.4999999F));
    assertDoubleEquals(-0.49999991059303284D, $opt$noinline$FloatToDouble(-0.4999999F));
    assertDoubleEquals(-0.5D, $opt$noinline$FloatToDouble(-0.5F));
    assertDoubleEquals(42.19900131225586D, $opt$noinline$FloatToDouble(42.199F));
    assertDoubleEquals(-42.19900131225586D, $opt$noinline$FloatToDouble(-42.199F));
    assertDoubleEquals(2147483648D, $opt$noinline$FloatToDouble(2147483647F));  // 2^31 - 1
    assertDoubleEquals(-2147483648D, $opt$noinline$FloatToDouble(-2147483647F));  // -(2^31 - 1)
    assertDoubleEquals(-2147483648D, $opt$noinline$FloatToDouble(-2147483648F));  // -(2^31)
    assertDoubleEquals(2147483648D, $opt$noinline$FloatToDouble(2147483648F));  // (2^31)
    assertDoubleEquals(-2147483648D, $opt$noinline$FloatToDouble(-2147483649F));  // -(2^31 + 1)
    assertDoubleEquals(9223372036854775807D, $opt$noinline$FloatToDouble(9223372036854775807F));  // 2^63 - 1
    assertDoubleEquals(-9223372036854775807D, $opt$noinline$FloatToDouble(-9223372036854775807F));  // -(2^63 - 1)
    assertDoubleEquals(-9223372036854775808D, $opt$noinline$FloatToDouble(-9223372036854775808F));  // -(2^63)
    assertDoubleIsNaN($opt$noinline$FloatToDouble(Float.NaN));
    assertDoubleEquals(Double.POSITIVE_INFINITY, $opt$noinline$FloatToDouble(Float.POSITIVE_INFINITY));
    assertDoubleEquals(Double.NEGATIVE_INFINITY, $opt$noinline$FloatToDouble(Float.NEGATIVE_INFINITY));
  }

  private static void doubleToInt() {
    assertIntEquals(1, $opt$noinline$DoubleToInt(1D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(0D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(-0D));
    assertIntEquals(-1, $opt$noinline$DoubleToInt(-1D));
    assertIntEquals(51, $opt$noinline$DoubleToInt(51D));
    assertIntEquals(-51, $opt$noinline$DoubleToInt(-51D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(0.5D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(0.4999999D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(-0.4999999D));
    assertIntEquals(0, $opt$noinline$DoubleToInt(-0.5D));
    assertIntEquals(42, $opt$noinline$DoubleToInt(42.199D));
    assertIntEquals(-42, $opt$noinline$DoubleToInt(-42.199D));
    assertIntEquals(2147483647, $opt$noinline$DoubleToInt(2147483647D));  // 2^31 - 1
    assertIntEquals(-2147483647, $opt$noinline$DoubleToInt(-2147483647D));  // -(2^31 - 1)
    assertIntEquals(-2147483648, $opt$noinline$DoubleToInt(-2147483648D));  // -(2^31)
    assertIntEquals(2147483647, $opt$noinline$DoubleToInt(2147483648D));  // (2^31)
    assertIntEquals(-2147483648, $opt$noinline$DoubleToInt(-2147483649D));  // -(2^31 + 1)
    assertIntEquals(2147483647, $opt$noinline$DoubleToInt(9223372036854775807D));  // 2^63 - 1
    assertIntEquals(-2147483648, $opt$noinline$DoubleToInt(-9223372036854775807D));  // -(2^63 - 1)
    assertIntEquals(-2147483648, $opt$noinline$DoubleToInt(-9223372036854775808D));  // -(2^63)
    assertIntEquals(0, $opt$noinline$DoubleToInt(Double.NaN));
    assertIntEquals(2147483647, $opt$noinline$DoubleToInt(Double.POSITIVE_INFINITY));
    assertIntEquals(-2147483648, $opt$noinline$DoubleToInt(Double.NEGATIVE_INFINITY));
  }

  private static void doubleToLong() {
    assertLongEquals(1L, $opt$noinline$DoubleToLong(1D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(0D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(-0D));
    assertLongEquals(-1L, $opt$noinline$DoubleToLong(-1D));
    assertLongEquals(51L, $opt$noinline$DoubleToLong(51D));
    assertLongEquals(-51L, $opt$noinline$DoubleToLong(-51D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(0.5D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(0.4999999D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(-0.4999999D));
    assertLongEquals(0L, $opt$noinline$DoubleToLong(-0.5D));
    assertLongEquals(42L, $opt$noinline$DoubleToLong(42.199D));
    assertLongEquals(-42L, $opt$noinline$DoubleToLong(-42.199D));
    assertLongEquals(2147483647L, $opt$noinline$DoubleToLong(2147483647D));  // 2^31 - 1
    assertLongEquals(-2147483647L, $opt$noinline$DoubleToLong(-2147483647D));  // -(2^31 - 1)
    assertLongEquals(-2147483648L, $opt$noinline$DoubleToLong(-2147483648D));  // -(2^31)
    assertLongEquals(2147483648L, $opt$noinline$DoubleToLong(2147483648D));  // (2^31)
    assertLongEquals(-2147483649L, $opt$noinline$DoubleToLong(-2147483649D));  // -(2^31 + 1)
    assertLongEquals(9223372036854775807L, $opt$noinline$DoubleToLong(9223372036854775807D));  // 2^63 - 1
    assertLongEquals(-9223372036854775808L, $opt$noinline$DoubleToLong(-9223372036854775807D));  // -(2^63 - 1)
    assertLongEquals(-9223372036854775808L, $opt$noinline$DoubleToLong(-9223372036854775808D));  // -(2^63)
    assertLongEquals(0L, $opt$noinline$DoubleToLong(Double.NaN));
    assertLongEquals(9223372036854774784L, $opt$noinline$DoubleToLong(9223372036854774784D));  // Math.nextAfter(2D^63, 0)
    assertLongEquals(-9223372036854774784L, $opt$noinline$DoubleToLong(-9223372036854774784D));  // Math.nextAfter(-2D^63, 0)
    assertLongEquals(9223372036854775807L, $opt$noinline$DoubleToLong(Double.POSITIVE_INFINITY));
    assertLongEquals(-9223372036854775808L, $opt$noinline$DoubleToLong(Double.NEGATIVE_INFINITY));
  }

  private static void doubleToFloat() {
    assertFloatEquals(1F, $opt$noinline$DoubleToFloat(1D));
    assertFloatEquals(0F, $opt$noinline$DoubleToFloat(0D));
    assertFloatEquals(0F, $opt$noinline$DoubleToFloat(-0D));
    assertFloatEquals(-1F, $opt$noinline$DoubleToFloat(-1D));
    assertFloatEquals(51F, $opt$noinline$DoubleToFloat(51D));
    assertFloatEquals(-51F, $opt$noinline$DoubleToFloat(-51D));
    assertFloatEquals(0.5F, $opt$noinline$DoubleToFloat(0.5D));
    assertFloatEquals(0.4999999F, $opt$noinline$DoubleToFloat(0.4999999D));
    assertFloatEquals(-0.4999999F, $opt$noinline$DoubleToFloat(-0.4999999D));
    assertFloatEquals(-0.5F, $opt$noinline$DoubleToFloat(-0.5D));
    assertFloatEquals(42.199F, $opt$noinline$DoubleToFloat(42.199D));
    assertFloatEquals(-42.199F, $opt$noinline$DoubleToFloat(-42.199D));
    assertFloatEquals(2147483648F, $opt$noinline$DoubleToFloat(2147483647D));  // 2^31 - 1
    assertFloatEquals(-2147483648F, $opt$noinline$DoubleToFloat(-2147483647D));  // -(2^31 - 1)
    assertFloatEquals(-2147483648F, $opt$noinline$DoubleToFloat(-2147483648D));  // -(2^31)
    assertFloatEquals(2147483648F, $opt$noinline$DoubleToFloat(2147483648D));  // (2^31)
    assertFloatEquals(-2147483648F, $opt$noinline$DoubleToFloat(-2147483649D));  // -(2^31 + 1)
    assertFloatEquals(9223372036854775807F, $opt$noinline$DoubleToFloat(9223372036854775807D));  // 2^63 - 1
    assertFloatEquals(-9223372036854775807F, $opt$noinline$DoubleToFloat(-9223372036854775807D));  // -(2^63 - 1)
    assertFloatEquals(-9223372036854775808F, $opt$noinline$DoubleToFloat(-9223372036854775808D));  // -(2^63)
    assertFloatIsNaN($opt$noinline$DoubleToFloat(Float.NaN));
    assertFloatEquals(Float.POSITIVE_INFINITY, $opt$noinline$DoubleToFloat(Double.POSITIVE_INFINITY));
    assertFloatEquals(Float.NEGATIVE_INFINITY, $opt$noinline$DoubleToFloat(Double.NEGATIVE_INFINITY));
  }

  private static void shortToByte() {
    assertByteEquals((byte)1, $opt$noinline$ShortToByte((short)1));
    assertByteEquals((byte)0, $opt$noinline$ShortToByte((short)0));
    assertByteEquals((byte)-1, $opt$noinline$ShortToByte((short)-1));
    assertByteEquals((byte)51, $opt$noinline$ShortToByte((short)51));
    assertByteEquals((byte)-51, $opt$noinline$ShortToByte((short)-51));
    assertByteEquals((byte)127, $opt$noinline$ShortToByte((short)127));  // 2^7 - 1
    assertByteEquals((byte)-127, $opt$noinline$ShortToByte((short)-127));  // -(2^7 - 1)
    assertByteEquals((byte)-128, $opt$noinline$ShortToByte((short)-128));  // -(2^7)
    assertByteEquals((byte)-128, $opt$noinline$ShortToByte((short)128));  // 2^7
    assertByteEquals((byte)127, $opt$noinline$ShortToByte((short)-129));  // -(2^7 + 1)
    assertByteEquals((byte)-1, $opt$noinline$ShortToByte((short)32767));  // 2^15 - 1
    assertByteEquals((byte)0, $opt$noinline$ShortToByte((short)-32768));  // -(2^15)
  }

  private static void intToByte() {
    assertByteEquals((byte)1, $opt$noinline$IntToByte(1));
    assertByteEquals((byte)0, $opt$noinline$IntToByte(0));
    assertByteEquals((byte)-1, $opt$noinline$IntToByte(-1));
    assertByteEquals((byte)51, $opt$noinline$IntToByte(51));
    assertByteEquals((byte)-51, $opt$noinline$IntToByte(-51));
    assertByteEquals((byte)127, $opt$noinline$IntToByte(127));  // 2^7 - 1
    assertByteEquals((byte)-127, $opt$noinline$IntToByte(-127));  // -(2^7 - 1)
    assertByteEquals((byte)-128, $opt$noinline$IntToByte(-128));  // -(2^7)
    assertByteEquals((byte)-128, $opt$noinline$IntToByte(128));  // 2^7
    assertByteEquals((byte)127, $opt$noinline$IntToByte(-129));  // -(2^7 + 1)
    assertByteEquals((byte)-1, $opt$noinline$IntToByte(2147483647));  // 2^31 - 1
    assertByteEquals((byte)0, $opt$noinline$IntToByte(-2147483648));  // -(2^31)
  }

  private static void charToByte() {
    assertByteEquals((byte)1, $opt$noinline$CharToByte((char)1));
    assertByteEquals((byte)0, $opt$noinline$CharToByte((char)0));
    assertByteEquals((byte)51, $opt$noinline$CharToByte((char)51));
    assertByteEquals((byte)127, $opt$noinline$CharToByte((char)127));  // 2^7 - 1
    assertByteEquals((byte)-128, $opt$noinline$CharToByte((char)128));  // 2^7
    assertByteEquals((byte)-1, $opt$noinline$CharToByte((char)32767));  // 2^15 - 1
    assertByteEquals((byte)-1, $opt$noinline$CharToByte((char)65535));  // 2^16 - 1
    assertByteEquals((byte)-1, $opt$noinline$CharToByte((char)-1));
    assertByteEquals((byte)-51, $opt$noinline$CharToByte((char)-51));
    assertByteEquals((byte)-127, $opt$noinline$CharToByte((char)-127));  // -(2^7 - 1)
    assertByteEquals((byte)-128, $opt$noinline$CharToByte((char)-128));  // -(2^7)
    assertByteEquals((byte)127, $opt$noinline$CharToByte((char)-129));  // -(2^7 + 1)
  }

  private static void byteToShort() {
    assertShortEquals((short)1, $opt$noinline$ByteToShort((byte)1));
    assertShortEquals((short)0, $opt$noinline$ByteToShort((byte)0));
    assertShortEquals((short)-1, $opt$noinline$ByteToShort((byte)-1));
    assertShortEquals((short)51, $opt$noinline$ByteToShort((byte)51));
    assertShortEquals((short)-51, $opt$noinline$ByteToShort((byte)-51));
    assertShortEquals((short)127, $opt$noinline$ByteToShort((byte)127));  // 2^7 - 1
    assertShortEquals((short)-127, $opt$noinline$ByteToShort((byte)-127));  // -(2^7 - 1)
    assertShortEquals((short)-128, $opt$noinline$ByteToShort((byte)-128));  // -(2^7)
  }

  private static void intToShort() {
    assertShortEquals((short)1, $opt$noinline$IntToShort(1));
    assertShortEquals((short)0, $opt$noinline$IntToShort(0));
    assertShortEquals((short)-1, $opt$noinline$IntToShort(-1));
    assertShortEquals((short)51, $opt$noinline$IntToShort(51));
    assertShortEquals((short)-51, $opt$noinline$IntToShort(-51));
    assertShortEquals((short)32767, $opt$noinline$IntToShort(32767));  // 2^15 - 1
    assertShortEquals((short)-32767, $opt$noinline$IntToShort(-32767));  // -(2^15 - 1)
    assertShortEquals((short)-32768, $opt$noinline$IntToShort(-32768));  // -(2^15)
    assertShortEquals((short)-32768, $opt$noinline$IntToShort(32768));  // 2^15
    assertShortEquals((short)32767, $opt$noinline$IntToShort(-32769));  // -(2^15 + 1)
    assertShortEquals((short)-1, $opt$noinline$IntToShort(2147483647));  // 2^31 - 1
    assertShortEquals((short)0, $opt$noinline$IntToShort(-2147483648));  // -(2^31)
  }

  private static void charToShort() {
    assertShortEquals((short)1, $opt$noinline$CharToShort((char)1));
    assertShortEquals((short)0, $opt$noinline$CharToShort((char)0));
    assertShortEquals((short)51, $opt$noinline$CharToShort((char)51));
    assertShortEquals((short)32767, $opt$noinline$CharToShort((char)32767));  // 2^15 - 1
    assertShortEquals((short)-32768, $opt$noinline$CharToShort((char)32768));  // 2^15
    assertShortEquals((short)-32767, $opt$noinline$CharToShort((char)32769));  // 2^15
    assertShortEquals((short)-1, $opt$noinline$CharToShort((char)65535));  // 2^16 - 1
    assertShortEquals((short)-1, $opt$noinline$CharToShort((char)-1));
    assertShortEquals((short)-51, $opt$noinline$CharToShort((char)-51));
    assertShortEquals((short)-32767, $opt$noinline$CharToShort((char)-32767));  // -(2^15 - 1)
    assertShortEquals((short)-32768, $opt$noinline$CharToShort((char)-32768));  // -(2^15)
    assertShortEquals((short)32767, $opt$noinline$CharToShort((char)-32769));  // -(2^15 + 1)
  }

  private static void byteToChar() {
    assertCharEquals((char)1, $opt$noinline$ByteToChar((byte)1));
    assertCharEquals((char)0, $opt$noinline$ByteToChar((byte)0));
    assertCharEquals((char)65535, $opt$noinline$ByteToChar((byte)-1));
    assertCharEquals((char)51, $opt$noinline$ByteToChar((byte)51));
    assertCharEquals((char)65485, $opt$noinline$ByteToChar((byte)-51));
    assertCharEquals((char)127, $opt$noinline$ByteToChar((byte)127));  // 2^7 - 1
    assertCharEquals((char)65409, $opt$noinline$ByteToChar((byte)-127));  // -(2^7 - 1)
    assertCharEquals((char)65408, $opt$noinline$ByteToChar((byte)-128));  // -(2^7)
  }

  private static void shortToChar() {
    assertCharEquals((char)1, $opt$noinline$ShortToChar((short)1));
    assertCharEquals((char)0, $opt$noinline$ShortToChar((short)0));
    assertCharEquals((char)65535, $opt$noinline$ShortToChar((short)-1));
    assertCharEquals((char)51, $opt$noinline$ShortToChar((short)51));
    assertCharEquals((char)65485, $opt$noinline$ShortToChar((short)-51));
    assertCharEquals((char)32767, $opt$noinline$ShortToChar((short)32767));  // 2^15 - 1
    assertCharEquals((char)32769, $opt$noinline$ShortToChar((short)-32767));  // -(2^15 - 1)
    assertCharEquals((char)32768, $opt$noinline$ShortToChar((short)-32768));  // -(2^15)
  }

  private static void intToChar() {
    assertCharEquals((char)1, $opt$noinline$IntToChar(1));
    assertCharEquals((char)0, $opt$noinline$IntToChar(0));
    assertCharEquals((char)65535, $opt$noinline$IntToChar(-1));
    assertCharEquals((char)51, $opt$noinline$IntToChar(51));
    assertCharEquals((char)65485, $opt$noinline$IntToChar(-51));
    assertCharEquals((char)32767, $opt$noinline$IntToChar(32767));  // 2^15 - 1
    assertCharEquals((char)32769, $opt$noinline$IntToChar(-32767));  // -(2^15 - 1)
    assertCharEquals((char)32768, $opt$noinline$IntToChar(32768));  // 2^15
    assertCharEquals((char)32768, $opt$noinline$IntToChar(-32768));  // -(2^15)
    assertCharEquals((char)65535, $opt$noinline$IntToChar(65535));  // 2^16 - 1
    assertCharEquals((char)1, $opt$noinline$IntToChar(-65535));  // -(2^16 - 1)
    assertCharEquals((char)0, $opt$noinline$IntToChar(65536));  // 2^16
    assertCharEquals((char)0, $opt$noinline$IntToChar(-65536));  // -(2^16)
    assertCharEquals((char)65535, $opt$noinline$IntToChar(2147483647));  // 2^31 - 1
    assertCharEquals((char)0, $opt$noinline$IntToChar(-2147483648));  // -(2^31)
  }

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  // These methods produce int-to-long Dex instructions.
  static long $opt$noinline$ByteToLong(byte a) { if (doThrow) throw new Error(); return (long)a; }
  static long $opt$noinline$ShortToLong(short a) { if (doThrow) throw new Error(); return (long)a; }
  static long $opt$noinline$IntToLong(int a) { if (doThrow) throw new Error(); return (long)a; }
  static long $opt$noinline$CharToLong(int a) { if (doThrow) throw new Error(); return (long)a; }

  // These methods produce int-to-float Dex instructions.
  static float $opt$noinline$ByteToFloat(byte a) { if (doThrow) throw new Error(); return (float)a; }
  static float $opt$noinline$ShortToFloat(short a) { if (doThrow) throw new Error(); return (float)a; }
  static float $opt$noinline$IntToFloat(int a) { if (doThrow) throw new Error(); return (float)a; }
  static float $opt$noinline$CharToFloat(char a) { if (doThrow) throw new Error(); return (float)a; }

  // These methods produce int-to-double Dex instructions.
  static double $opt$noinline$ByteToDouble(byte a) { if (doThrow) throw new Error(); return (double)a; }
  static double $opt$noinline$ShortToDouble(short a) { if (doThrow) throw new Error(); return (double)a; }
  static double $opt$noinline$IntToDouble(int a) { if (doThrow) throw new Error(); return (double)a; }
  static double $opt$noinline$CharToDouble(int a) { if (doThrow) throw new Error(); return (double)a; }

  // These methods produce long-to-int Dex instructions.
  static int $opt$noinline$LongToInt(long a) { if (doThrow) throw new Error(); return (int)a; }
  static int $opt$noinline$LongLiteralToInt() { if (doThrow) throw new Error(); return (int)42L; }

  // This method produces a long-to-float Dex instruction.
  static float $opt$noinline$LongToFloat(long a) { if (doThrow) throw new Error(); return (float)a; }

  // This method produces a long-to-double Dex instruction.
  static double $opt$noinline$LongToDouble(long a) { if (doThrow) throw new Error(); return (double)a; }

  // This method produces a float-to-int Dex instruction.
  static int $opt$noinline$FloatToInt(float a) { if (doThrow) throw new Error(); return (int)a; }

  // This method produces a float-to-long Dex instruction.
  static long $opt$noinline$FloatToLong(float a){ if (doThrow) throw new Error(); return (long)a; }

  // This method produces a float-to-double Dex instruction.
  static double $opt$noinline$FloatToDouble(float a) { if (doThrow) throw new Error(); return (double)a; }

  // This method produces a double-to-int Dex instruction.
  static int $opt$noinline$DoubleToInt(double a){ if (doThrow) throw new Error(); return (int)a; }

  // This method produces a double-to-long Dex instruction.
  static long $opt$noinline$DoubleToLong(double a){ if (doThrow) throw new Error(); return (long)a; }

  // This method produces a double-to-float Dex instruction.
  static float $opt$noinline$DoubleToFloat(double a) { if (doThrow) throw new Error(); return (float)a; }

  // These methods produce int-to-byte Dex instructions.
  static byte $opt$noinline$ShortToByte(short a) { if (doThrow) throw new Error(); return (byte)a; }
  static byte $opt$noinline$IntToByte(int a) { if (doThrow) throw new Error(); return (byte)a; }
  static byte $opt$noinline$CharToByte(char a) { if (doThrow) throw new Error(); return (byte)a; }

  // These methods produce int-to-short Dex instructions.
  static short $opt$noinline$ByteToShort(byte a) { if (doThrow) throw new Error(); return (short)a; }
  static short $opt$noinline$IntToShort(int a) { if (doThrow) throw new Error(); return (short)a; }
  static short $opt$noinline$CharToShort(char a) { if (doThrow) throw new Error(); return (short)a; }

  // These methods produce int-to-char Dex instructions.
  static char $opt$noinline$ByteToChar(byte a) { if (doThrow) throw new Error(); return (char)a; }
  static char $opt$noinline$ShortToChar(short a) { if (doThrow) throw new Error(); return (char)a; }
  static char $opt$noinline$IntToChar(int a) { if (doThrow) throw new Error(); return (char)a; }
}
