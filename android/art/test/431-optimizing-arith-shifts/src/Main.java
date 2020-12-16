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

public class Main {

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    testShlInt();
    testShlLong();
    testShrInt();
    testShrLong();
    testUShrInt();
    testUShrLong();
  }

  private static void testShlInt() {
    expectEquals(48, $opt$ShlIntConst2(12));
    expectEquals(12, $opt$ShlIntConst0(12));
    expectEquals(-48, $opt$ShlInt(-12, 2));
    expectEquals(1024, $opt$ShlInt(32, 5));

    expectEquals(7, $opt$ShlInt(7, 0));
    expectEquals(14, $opt$ShlInt(7, 1));
    expectEquals(0, $opt$ShlInt(0, 30));

    expectEquals(1073741824L, $opt$ShlInt(1, 30));
    expectEquals(Integer.MIN_VALUE, $opt$ShlInt(1, 31));  // overflow
    expectEquals(Integer.MIN_VALUE, $opt$ShlInt(1073741824, 1));  // overflow
    expectEquals(1073741824, $opt$ShlInt(268435456, 2));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$ShlInt(7, 32));  // 32 & 0x1f = 0
    expectEquals(14, $opt$ShlInt(7, 33));  // 33 & 0x1f = 1
    expectEquals(32, $opt$ShlInt(1, 101));  // 101 & 0x1f = 5

    expectEquals(Integer.MIN_VALUE, $opt$ShlInt(1, -1));  // -1 & 0x1f = 31
    expectEquals(14, $opt$ShlInt(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$ShlInt(7, -32));  // -32 & 0x1f = 0
    expectEquals(-536870912, $opt$ShlInt(7, -3));  // -3 & 0x1f = 29

    expectEquals(Integer.MIN_VALUE, $opt$ShlInt(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$ShlInt(7, Integer.MIN_VALUE));
  }

  private static void testShlLong() {
    expectEquals(48L, $opt$ShlLongConst2(12L));
    expectEquals(12L, $opt$ShlLongConst0(12L));
    expectEquals(-48L, $opt$ShlLong(-12L, 2));
    expectEquals(1024L, $opt$ShlLong(32L, 5));

    expectEquals(7L, $opt$ShlLong(7L, 0));
    expectEquals(14L, $opt$ShlLong(7L, 1));
    expectEquals(0L, $opt$ShlLong(0L, 30));

    expectEquals(1073741824L, $opt$ShlLong(1L, 30));
    expectEquals(2147483648L, $opt$ShlLong(1L, 31));
    expectEquals(2147483648L, $opt$ShlLong(1073741824L, 1));

    // Long shifts can use up to 6 lower bits.
    expectEquals(4294967296L, $opt$ShlLong(1L, 32));
    expectEquals(60129542144L, $opt$ShlLong(7L, 33));
    expectEquals(Long.MIN_VALUE, $opt$ShlLong(1L, 63));  // overflow

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$ShlLong(7L, 64));  // 64 & 0x3f = 0
    expectEquals(14L, $opt$ShlLong(7L, 65));  // 65 & 0x3f = 1
    expectEquals(137438953472L, $opt$ShlLong(1L, 101));  // 101 & 0x3f = 37

    expectEquals(Long.MIN_VALUE, $opt$ShlLong(1L, -1));  // -1 & 0x3f = 63
    expectEquals(14L, $opt$ShlLong(7L, -63));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$ShlLong(7L, -64));  // -64 & 0x3f = 0
    expectEquals(2305843009213693952L, $opt$ShlLong(1L, -3));  // -3 & 0x3f = 61

    expectEquals(Long.MIN_VALUE, $opt$ShlLong(7L, Integer.MAX_VALUE));
    expectEquals(7L, $opt$ShlLong(7L, Integer.MIN_VALUE));

    // Exercise some special cases handled by backends/simplifier.
    expectEquals(24L, $opt$ShlLongConst1(12L));
    expectEquals(0x2345678900000000L, $opt$ShlLongConst32(0x123456789L));
    expectEquals(0x2490249000000000L, $opt$ShlLongConst33(0x12481248L));
    expectEquals(0x4920492000000000L, $opt$ShlLongConst34(0x12481248L));
    expectEquals(0x9240924000000000L, $opt$ShlLongConst35(0x12481248L));
  }

  private static void testShrInt() {
    expectEquals(3, $opt$ShrIntConst2(12));
    expectEquals(12, $opt$ShrIntConst0(12));
    expectEquals(-3, $opt$ShrInt(-12, 2));
    expectEquals(1, $opt$ShrInt(32, 5));

    expectEquals(7, $opt$ShrInt(7, 0));
    expectEquals(3, $opt$ShrInt(7, 1));
    expectEquals(0, $opt$ShrInt(0, 30));
    expectEquals(0, $opt$ShrInt(1, 30));
    expectEquals(-1, $opt$ShrInt(-1, 30));

    expectEquals(0, $opt$ShrInt(Integer.MAX_VALUE, 31));
    expectEquals(-1, $opt$ShrInt(Integer.MIN_VALUE, 31));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$ShrInt(7, 32));  // 32 & 0x1f = 0
    expectEquals(3, $opt$ShrInt(7, 33));  // 33 & 0x1f = 1

    expectEquals(0, $opt$ShrInt(1, -1));  // -1 & 0x1f = 31
    expectEquals(3, $opt$ShrInt(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$ShrInt(7, -32));  // -32 & 0x1f = 0
    expectEquals(-4, $opt$ShrInt(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0, $opt$ShrInt(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$ShrInt(7, Integer.MIN_VALUE));
  }

  private static void testShrLong() {
    expectEquals(3L, $opt$ShrLongConst2(12L));
    expectEquals(12L, $opt$ShrLongConst0(12L));
    expectEquals(-3L, $opt$ShrLong(-12L, 2));
    expectEquals(1, $opt$ShrLong(32, 5));

    expectEquals(7L, $opt$ShrLong(7L, 0));
    expectEquals(3L, $opt$ShrLong(7L, 1));
    expectEquals(0L, $opt$ShrLong(0L, 30));
    expectEquals(0L, $opt$ShrLong(1L, 30));
    expectEquals(-1L, $opt$ShrLong(-1L, 30));

    expectEquals(1L, $opt$ShrLong(1073741824L, 30));
    expectEquals(1L, $opt$ShrLong(2147483648L, 31));
    expectEquals(1073741824L, $opt$ShrLong(2147483648L, 1));

    // Long shifts can use up to 6 lower bits.
    expectEquals(1L, $opt$ShrLong(4294967296L, 32));
    expectEquals(7L, $opt$ShrLong(60129542144L, 33));
    expectEquals(0L, $opt$ShrLong(Long.MAX_VALUE, 63));
    expectEquals(-1L, $opt$ShrLong(Long.MIN_VALUE, 63));

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$ShrLong(7L, 64));  // 64 & 0x3f = 0
    expectEquals(3L, $opt$ShrLong(7L, 65));  // 65 & 0x3f = 1

    expectEquals(-1L, $opt$ShrLong(Long.MIN_VALUE, -1));  // -1 & 0x3f = 63
    expectEquals(3L, $opt$ShrLong(7L, -63));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$ShrLong(7L, -64));  // -64 & 0x3f = 0
    expectEquals(1L, $opt$ShrLong(2305843009213693952L, -3));  // -3 & 0x3f = 61
    expectEquals(-1L, $opt$ShrLong(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0L, $opt$ShrLong(7L, Integer.MAX_VALUE));
    expectEquals(7L, $opt$ShrLong(7L, Integer.MIN_VALUE));
  }

  private static void testUShrInt() {
    expectEquals(3, $opt$UShrIntConst2(12));
    expectEquals(12, $opt$UShrIntConst0(12));
    expectEquals(1073741821, $opt$UShrInt(-12, 2));
    expectEquals(1, $opt$UShrInt(32, 5));

    expectEquals(7, $opt$UShrInt(7, 0));
    expectEquals(3, $opt$UShrInt(7, 1));
    expectEquals(0, $opt$UShrInt(0, 30));
    expectEquals(0, $opt$UShrInt(1, 30));
    expectEquals(3, $opt$UShrInt(-1, 30));

    expectEquals(0, $opt$UShrInt(Integer.MAX_VALUE, 31));
    expectEquals(1, $opt$UShrInt(Integer.MIN_VALUE, 31));

    // Only the 5 lower bits should be used for shifting (& 0x1f).
    expectEquals(7, $opt$UShrInt(7, 32));  // 32 & 0x1f = 0
    expectEquals(3, $opt$UShrInt(7, 33));  // 33 & 0x1f = 1

    expectEquals(0, $opt$UShrInt(1, -1));  // -1 & 0x1f = 31
    expectEquals(3, $opt$UShrInt(7, -31));  // -31 & 0x1f = 1
    expectEquals(7, $opt$UShrInt(7, -32));  // -32 & 0x1f = 0
    expectEquals(4, $opt$UShrInt(Integer.MIN_VALUE, -3));  // -3 & 0x1f = 29

    expectEquals(0, $opt$UShrInt(7, Integer.MAX_VALUE));
    expectEquals(7, $opt$UShrInt(7, Integer.MIN_VALUE));
  }

  private static void testUShrLong() {
    expectEquals(3L, $opt$UShrLongConst2(12L));
    expectEquals(12L, $opt$UShrLongConst0(12L));
    expectEquals(4611686018427387901L, $opt$UShrLong(-12L, 2));
    expectEquals(1, $opt$UShrLong(32, 5));

    expectEquals(7L, $opt$UShrLong(7L, 0));
    expectEquals(3L, $opt$UShrLong(7L, 1));
    expectEquals(0L, $opt$UShrLong(0L, 30));
    expectEquals(0L, $opt$UShrLong(1L, 30));
    expectEquals(17179869183L, $opt$UShrLong(-1L, 30));

    expectEquals(1L, $opt$UShrLong(1073741824L, 30));
    expectEquals(1L, $opt$UShrLong(2147483648L, 31));
    expectEquals(1073741824L, $opt$UShrLong(2147483648L, 1));

    // Long shifts can use use up to 6 lower bits.
    expectEquals(1L, $opt$UShrLong(4294967296L, 32));
    expectEquals(7L, $opt$UShrLong(60129542144L, 33));
    expectEquals(0L, $opt$UShrLong(Long.MAX_VALUE, 63));
    expectEquals(1L, $opt$UShrLong(Long.MIN_VALUE, 63));

    // Only the 6 lower bits should be used for shifting (& 0x3f).
    expectEquals(7L, $opt$UShrLong(7L, 64));  // 64 & 0x3f = 0
    expectEquals(3L, $opt$UShrLong(7L, 65));  // 65 & 0x3f = 1

    expectEquals(1L, $opt$UShrLong(Long.MIN_VALUE, -1));  // -1 & 0x3f = 63
    expectEquals(3L, $opt$UShrLong(7L, -63));  // -63 & 0x3f = 1
    expectEquals(7L, $opt$UShrLong(7L, -64));  // -64 & 0x3f = 0
    expectEquals(1L, $opt$UShrLong(2305843009213693952L, -3));  // -3 & 0x3f = 61
    expectEquals(4L, $opt$UShrLong(Long.MIN_VALUE, -3));  // -3 & 0x3f = 61

    expectEquals(0L, $opt$UShrLong(7L, Integer.MAX_VALUE));
    expectEquals(7L, $opt$UShrLong(7L, Integer.MIN_VALUE));
  }


  static int $opt$ShlInt(int value, int distance) {
    return value << distance;
  }

  static long $opt$ShlLong(long value, int distance) {
    return value << distance;
  }

  static int $opt$ShrInt(int value, int distance) {
    return value >> distance;
  }

  static long $opt$ShrLong(long value, int distance) {
    return value >> distance;
  }

  static int $opt$UShrInt(int value, int distance) {
    return value >>> distance;
  }

  static long $opt$UShrLong(long value, int distance) {
    return value >>> distance;
  }

  static int $opt$ShlIntConst2(int value) {
    return value << 2;
  }

  static long $opt$ShlLongConst2(long value) {
    return value << 2;
  }

  static int $opt$ShrIntConst2(int value) {
    return value >> 2;
  }

  static long $opt$ShrLongConst2(long value) {
    return value >> 2;
  }

  static int $opt$UShrIntConst2(int value) {
    return value >>> 2;
  }

  static long $opt$UShrLongConst2(long value) {
    return value >>> 2;
  }

  static int $opt$ShlIntConst0(int value) {
    return value << 0;
  }

  static long $opt$ShlLongConst0(long value) {
    return value << 0;
  }

  static int $opt$ShrIntConst0(int value) {
    return value >> 0;
  }

  static long $opt$ShrLongConst0(long value) {
    return value >> 0;
  }

  static int $opt$UShrIntConst0(int value) {
    return value >>> 0;
  }

  static long $opt$UShrLongConst0(long value) {
    return value >>> 0;
  }

  static long $opt$ShlLongConst1(long value) {
    return value << 1;
  }

  static long $opt$ShlLongConst32(long value) {
    return value << 32;
  }

  static long $opt$ShlLongConst33(long value) {
    return value << 33;
  }

  static long $opt$ShlLongConst34(long value) {
    return value << 34;
  }

  static long $opt$ShlLongConst35(long value) {
    return value << 35;
  }

}
