/*
 * Copyright (C) 2007 The Android Open Source Project
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

import junit.framework.Assert;
import java.util.Arrays;
import java.lang.reflect.Method;

public class Main {
  public static void main(String args[]) throws Exception {
    test_Double_doubleToRawLongBits();
    test_Double_longBitsToDouble();
    test_Float_floatToRawIntBits();
    test_Float_intBitsToFloat();
    test_Math_abs_I();
    test_Math_abs_J();
    test_Math_min_I();
    test_Math_max_I();
    test_Math_min_J();
    test_Math_max_J();
    test_Math_min_F();
    test_Math_max_F();
    test_Math_min_D();
    test_Math_max_D();
    test_Math_sqrt();
    test_Math_ceil();
    test_Math_floor();
    test_Math_rint();
    test_Math_round_D();
    test_Math_round_F();
    test_Math_isNaN_D();
    test_Math_isNaN_F();
    test_Math_isInfinite_D();
    test_Math_isInfinite_F();
    test_Short_reverseBytes();
    test_Integer_reverseBytes();
    test_Long_reverseBytes();
    test_Integer_reverse();
    test_Long_reverse();
    test_Integer_numberOfLeadingZeros();
    test_Long_numberOfLeadingZeros();
    test_StrictMath_abs_I();
    test_StrictMath_abs_J();
    test_StrictMath_min_I();
    test_StrictMath_max_I();
    test_StrictMath_min_J();
    test_StrictMath_max_J();
    test_StrictMath_min_F();
    test_StrictMath_max_F();
    test_StrictMath_min_D();
    test_StrictMath_max_D();
    test_StrictMath_sqrt();
    test_StrictMath_ceil();
    test_StrictMath_floor();
    test_StrictMath_rint();
    test_StrictMath_round_D();
    test_StrictMath_round_F();
    test_String_charAt();
    test_String_compareTo();
    test_String_indexOf();
    test_String_isEmpty();
    test_String_length();
    test_Thread_currentThread();
    initSupportMethodsForPeekPoke();
    test_Memory_peekByte();
    test_Memory_peekShort();
    test_Memory_peekInt();
    test_Memory_peekLong();
    test_Memory_pokeByte();
    test_Memory_pokeShort();
    test_Memory_pokeInt();
    test_Memory_pokeLong();
    test_Integer_numberOfTrailingZeros();
    test_Long_numberOfTrailingZeros();
    test_Integer_rotateRight();
    test_Long_rotateRight();
    test_Integer_rotateLeft();
    test_Long_rotateLeft();
    test_Integer_rotateRightLeft();
    test_Long_rotateRightLeft();
  }

  /**
   * Will test inlining Thread.currentThread().
   */
  public static void test_Thread_currentThread() {
    // 1. Do not use result.
    Thread.currentThread();

    // 2. Result should not be null.
    Assert.assertNotNull(Thread.currentThread());
  }

  public static void test_String_length() {
    String str0 = "";
    String str1 = "x";
    String str80 = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";

    Assert.assertEquals(str0.length(), 0);
    Assert.assertEquals(str1.length(), 1);
    Assert.assertEquals(str80.length(), 80);

    String strNull = null;
    try {
      strNull.length();
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  public static void test_String_isEmpty() {
    String str0 = "";
    String str1 = "x";

    Assert.assertTrue(str0.isEmpty());
    Assert.assertFalse(str1.isEmpty());

    String strNull = null;
    try {
      strNull.isEmpty();
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  // Break up the charAt tests. The optimizing compiler doesn't optimize methods with try-catch yet,
  // so we need to separate out the tests that are expected to throw exception

  public static void test_String_charAt() {
    String testStr = "Now is the time to test some stuff";

    Assert.assertEquals(testStr.length() - 1, 33);  // 33 = testStr.length()-1 as a constant.
    Assert.assertEquals('f', testStr.charAt(33));

    test_String_charAt(testStr, 'N', 'o', ' ', 'f');
    test_String_charAt(testStr.substring(3,15), ' ', 'i', 'm', 'e');
  }
  public static void test_String_charAt(String testStr, char a, char b, char c, char d) {
    Assert.assertEquals(a, testStr.charAt(0));
    Assert.assertEquals(b, testStr.charAt(1));
    Assert.assertEquals(c, testStr.charAt(10));
    Assert.assertEquals(d, testStr.charAt(testStr.length()-1));

    test_String_charAtExc(testStr);
    test_String_charAtExc2(testStr);
  }

  private static void test_String_charAtExc(String testStr) {
    try {
      testStr.charAt(-1);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      testStr.charAt(80);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      if (testStr.length() == 34) {
          testStr.charAt(34);  // 34 = "Now is the time to test some stuff".length()
      } else {
          Assert.assertEquals(testStr.length(), 12);  // 12 = " is the time".length()
          testStr.charAt(12);
      }
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      test_String_charAt_inner(testStr, -1);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      test_String_charAt_inner(testStr, 80);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      if (testStr.length() == 34) {
        // 34 = "Now is the time to test some stuff".length()
        test_String_charAt_inner(testStr, 34);
      } else {
        Assert.assertEquals(testStr.length(), 12);  // 12 = " is the time".length()
        test_String_charAt_inner(testStr, 12);
      }
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }

    String strEmpty = "";
    try {
      strEmpty.charAt(0);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }

    String strNull = null;
    try {
      strNull.charAt(0);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  private static char test_String_charAt_inner(String s, int index) {
    // Using non-constant index here (assuming that this method wasn't inlined).
    return s.charAt(index);
  }

  private static void test_String_charAtExc2(String testStr) {
    try {
      test_String_charAtExc3(testStr);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      test_String_charAtExc4(testStr);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
  }

  private static void test_String_charAtExc3(String testStr) {
    Assert.assertEquals('N', testStr.charAt(-1));
  }

  private static void test_String_charAtExc4(String testStr) {
    Assert.assertEquals('N', testStr.charAt(100));
  }

  static int start;
  private static int[] negIndex = { -100000 };
  public static void test_String_indexOf() {
    String str0 = "";
    String str1 = "/";
    String str3 = "abc";
    String str10 = "abcdefghij";
    String str40 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabc";

    Assert.assertEquals(str0.indexOf('a'), -1);
    Assert.assertEquals(str3.indexOf('a'), 0);
    Assert.assertEquals(str3.indexOf('b'), 1);
    Assert.assertEquals(str3.indexOf('c'), 2);
    Assert.assertEquals(str10.indexOf('j'), 9);
    Assert.assertEquals(str40.indexOf('a'), 0);
    Assert.assertEquals(str40.indexOf('b'), 38);
    Assert.assertEquals(str40.indexOf('c'), 39);
    Assert.assertEquals(str0.indexOf('a',20), -1);
    Assert.assertEquals(str0.indexOf('a',0), -1);
    Assert.assertEquals(str0.indexOf('a',-1), -1);
    Assert.assertEquals(str1.indexOf('/',++start), -1);
    Assert.assertEquals(str1.indexOf('a',negIndex[0]), -1);
    Assert.assertEquals(str3.indexOf('a',0), 0);
    Assert.assertEquals(str3.indexOf('a',1), -1);
    Assert.assertEquals(str3.indexOf('a',1234), -1);
    Assert.assertEquals(str3.indexOf('b',0), 1);
    Assert.assertEquals(str3.indexOf('b',1), 1);
    Assert.assertEquals(str3.indexOf('c',2), 2);
    Assert.assertEquals(str10.indexOf('j',5), 9);
    Assert.assertEquals(str10.indexOf('j',9), 9);
    Assert.assertEquals(str40.indexOf('a',10), 10);
    Assert.assertEquals(str40.indexOf('b',40), -1);

    testIndexOfNull();

    // Same data as above, but stored so it's not a literal in the next test. -2 stands for
    // indexOf(I) instead of indexOf(II).
    start--;
    int[][] searchData = {
        { 'a', -2, -1 },
        { 'a', -2, 0 },
        { 'b', -2, 1 },
        { 'c', -2, 2 },
        { 'j', -2, 9 },
        { 'a', -2, 0 },
        { 'b', -2, 38 },
        { 'c', -2, 39 },
        { 'a', 20, -1 },
        { 'a', 0, -1 },
        { 'a', -1, -1 },
        { '/', ++start, -1 },
        { 'a', negIndex[0], -1 },
        { 'a', 0, 0 },
        { 'a', 1, -1 },
        { 'a', 1234, -1 },
        { 'b', 0, 1 },
        { 'b', 1, 1 },
        { 'c', 2, 2 },
        { 'j', 5, 9 },
        { 'j', 9, 9 },
        { 'a', 10, 10 },
        { 'b', 40, -1 },
    };
    testStringIndexOfChars(searchData);

    testSurrogateIndexOf();
  }

  private static void testStringIndexOfChars(int[][] searchData) {
    // Use a try-catch to avoid inlining.
    try {
      testStringIndexOfCharsImpl(searchData);
    } catch (Exception e) {
      System.out.println("Unexpected exception");
    }
  }

  private static void testStringIndexOfCharsImpl(int[][] searchData) {
    String str0 = "";
    String str1 = "/";
    String str3 = "abc";
    String str10 = "abcdefghij";
    String str40 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabc";

    Assert.assertEquals(str0.indexOf(searchData[0][0]), searchData[0][2]);
    Assert.assertEquals(str3.indexOf(searchData[1][0]), searchData[1][2]);
    Assert.assertEquals(str3.indexOf(searchData[2][0]), searchData[2][2]);
    Assert.assertEquals(str3.indexOf(searchData[3][0]), searchData[3][2]);
    Assert.assertEquals(str10.indexOf(searchData[4][0]), searchData[4][2]);
    Assert.assertEquals(str40.indexOf(searchData[5][0]), searchData[5][2]);
    Assert.assertEquals(str40.indexOf(searchData[6][0]), searchData[6][2]);
    Assert.assertEquals(str40.indexOf(searchData[7][0]), searchData[7][2]);
    Assert.assertEquals(str0.indexOf(searchData[8][0], searchData[8][1]), searchData[8][2]);
    Assert.assertEquals(str0.indexOf(searchData[9][0], searchData[9][1]), searchData[9][2]);
    Assert.assertEquals(str0.indexOf(searchData[10][0], searchData[10][1]), searchData[10][2]);
    Assert.assertEquals(str1.indexOf(searchData[11][0], searchData[11][1]), searchData[11][2]);
    Assert.assertEquals(str1.indexOf(searchData[12][0], searchData[12][1]), searchData[12][2]);
    Assert.assertEquals(str3.indexOf(searchData[13][0], searchData[13][1]), searchData[13][2]);
    Assert.assertEquals(str3.indexOf(searchData[14][0], searchData[14][1]), searchData[14][2]);
    Assert.assertEquals(str3.indexOf(searchData[15][0], searchData[15][1]), searchData[15][2]);
    Assert.assertEquals(str3.indexOf(searchData[16][0], searchData[16][1]), searchData[16][2]);
    Assert.assertEquals(str3.indexOf(searchData[17][0], searchData[17][1]), searchData[17][2]);
    Assert.assertEquals(str3.indexOf(searchData[18][0], searchData[18][1]), searchData[18][2]);
    Assert.assertEquals(str10.indexOf(searchData[19][0], searchData[19][1]), searchData[19][2]);
    Assert.assertEquals(str10.indexOf(searchData[20][0], searchData[20][1]), searchData[20][2]);
    Assert.assertEquals(str40.indexOf(searchData[21][0], searchData[21][1]), searchData[21][2]);
    Assert.assertEquals(str40.indexOf(searchData[22][0], searchData[22][1]), searchData[22][2]);
  }

  private static void testSurrogateIndexOf() {
    int supplementaryChar = 0x20b9f;
    String surrogatePair = "\ud842\udf9f";
    String stringWithSurrogates = "hello " + surrogatePair + " world";

    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar), "hello ".length());
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 2), "hello ".length());
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 6), 6);
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 7), -1);

    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar - 0x10000), -1);
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar | 0x80000000), -1);
  }

  private static void testIndexOfNull() {
    String strNull = null;
    try {
      testNullIndex(strNull, 'a');
      Assert.fail();
    } catch (NullPointerException expected) {
    }
    try {
      testNullIndex(strNull, 'a', 0);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
    try {
        testNullIndex(strNull, 'a', -1);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  private static int testNullIndex(String strNull, int c) {
    return strNull.indexOf(c);
  }

  private static int testNullIndex(String strNull, int c, int startIndex) {
    return strNull.indexOf(c, startIndex);
  }

  public static void test_String_compareTo() {
    String test = "0123456789";
    String test1 = new String("0123456789");    // different object
    String test2 = new String("0123456780");    // different value
    String offset = new String("xxx0123456789yyy");
    String sub = offset.substring(3, 13);
    String str32 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    String str33 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy";
    String lc = "abcdefg";
    String uc = "ABCDEFG";
    Object blah = new Object();

    Assert.assertTrue(lc.toUpperCase().equals(uc));

    Assert.assertEquals(str32.compareTo(str33), -1);
    Assert.assertEquals(str33.compareTo(str32), 1);

    Assert.assertTrue(test.equals(test));
    Assert.assertTrue(test.equals(test1));
    Assert.assertFalse(test.equals(test2));

    Assert.assertEquals(test.compareTo(test1), 0);
    Assert.assertTrue(test1.compareTo(test2) > 0);
    Assert.assertTrue(test2.compareTo(test1) < 0);

    // Compare string with a nonzero offset, in left/right side.
    Assert.assertEquals(test.compareTo(sub), 0);
    Assert.assertEquals(sub.compareTo(test), 0);
    Assert.assertTrue(test.equals(sub));
    Assert.assertTrue(sub.equals(test));
    // Same base, one is a substring.
    Assert.assertFalse(offset.equals(sub));
    Assert.assertFalse(sub.equals(offset));
    // Wrong class.
    Assert.assertFalse(test.equals(blah));

    // Null lhs - throw.
    try {
      test.compareTo(null);
      Assert.fail("didn't get expected npe");
    } catch (NullPointerException npe) {
    }
    // Null rhs - okay.
    Assert.assertFalse(test.equals(null));

    test = test.substring(1);
    Assert.assertTrue(test.equals("123456789"));
    Assert.assertFalse(test.equals(test1));

    test = test.substring(1);
    Assert.assertTrue(test.equals("23456789"));

    test = test.substring(1);
    Assert.assertTrue(test.equals("3456789"));

    test = test.substring(1);
    Assert.assertTrue(test.equals("456789"));

    test = test.substring(3,5);
    Assert.assertTrue(test.equals("78"));

    test = "this/is/a/path";
    String[] strings = test.split("/");
    Assert.assertEquals(4, strings.length);

    Assert.assertEquals("this is a path", test.replaceAll("/", " "));
    Assert.assertEquals("this is a path", test.replace("/", " "));
  }

  public static void test_Math_abs_I() {
    Math.abs(-1);
    Assert.assertEquals(Math.abs(0), 0);
    Assert.assertEquals(Math.abs(123), 123);
    Assert.assertEquals(Math.abs(-123), 123);
    Assert.assertEquals(Math.abs(Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE), Integer.MIN_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE - 1), Integer.MAX_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE + 1), Integer.MAX_VALUE);
  }

  public static void test_Math_abs_J() {
    Math.abs(-1L);
    Assert.assertEquals(Math.abs(0L), 0L);
    Assert.assertEquals(Math.abs(123L), 123L);
    Assert.assertEquals(Math.abs(-123L), 123L);
    Assert.assertEquals(Math.abs(Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(Math.abs(Long.MIN_VALUE), Long.MIN_VALUE);
    Assert.assertEquals(Math.abs(Long.MIN_VALUE - 1), Long.MAX_VALUE);
    Assert.assertEquals(Math.abs(2147483648L), 2147483648L);
  }

  public static void test_Math_min_I() {
    Math.min(1, 0);
    Assert.assertEquals(Math.min(0, 0), 0);
    Assert.assertEquals(Math.min(1, 0), 0);
    Assert.assertEquals(Math.min(0, 1), 0);
    Assert.assertEquals(Math.min(0, Integer.MAX_VALUE), 0);
    Assert.assertEquals(Math.min(Integer.MIN_VALUE, 0), Integer.MIN_VALUE);
    Assert.assertEquals(Math.min(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MIN_VALUE);
  }

  public static void test_Math_max_I() {
    Math.max(1, 0);
    Assert.assertEquals(Math.max(0, 0), 0);
    Assert.assertEquals(Math.max(1, 0), 1);
    Assert.assertEquals(Math.max(0, 1), 1);
    Assert.assertEquals(Math.max(0, Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Math.max(Integer.MIN_VALUE, 0), 0);
    Assert.assertEquals(Math.max(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MAX_VALUE);
  }

  public static void test_Math_min_J() {
    Math.min(1L, 0L);
    Assert.assertEquals(Math.min(0L, 0L), 0L);
    Assert.assertEquals(Math.min(1L, 0L), 0L);
    Assert.assertEquals(Math.min(0L, 1L), 0L);
    Assert.assertEquals(Math.min(0L, Long.MAX_VALUE), 0L);
    Assert.assertEquals(Math.min(Long.MIN_VALUE, 0L), Long.MIN_VALUE);
    Assert.assertEquals(Math.min(Long.MIN_VALUE, Long.MAX_VALUE), Long.MIN_VALUE);
  }

  public static void test_Math_max_J() {
    Math.max(1L, 0L);
    Assert.assertEquals(Math.max(0L, 0L), 0L);
    Assert.assertEquals(Math.max(1L, 0L), 1L);
    Assert.assertEquals(Math.max(0L, 1L), 1L);
    Assert.assertEquals(Math.max(0L, Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(Math.max(Long.MIN_VALUE, 0L), 0L);
    Assert.assertEquals(Math.max(Long.MIN_VALUE, Long.MAX_VALUE), Long.MAX_VALUE);
  }

  public static void test_Math_min_F() {
    Math.min(1.0f, Float.NaN);
    Assert.assertTrue(Float.isNaN(Math.min(1.0f, Float.NaN)));
    Assert.assertTrue(Float.isNaN(Math.min(Float.NaN, 1.0f)));
    Assert.assertEquals(Math.min(-0.0f, 0.0f), -0.0f);
    Assert.assertEquals(Math.min(0.0f, -0.0f), -0.0f);
    Assert.assertEquals(Math.min(-0.0f, -0.0f), -0.0f);
    Assert.assertEquals(Math.min(0.0f, 0.0f), 0.0f);
    Assert.assertEquals(Math.min(1.0f, 0.0f), 0.0f);
    Assert.assertEquals(Math.min(0.0f, 1.0f), 0.0f);
    Assert.assertEquals(Math.min(0.0f, Float.MAX_VALUE), 0.0f);
    Assert.assertEquals(Math.min(Float.MIN_VALUE, 0.0f), 0.0f);
    Assert.assertEquals(Math.min(Float.MIN_VALUE, Float.MAX_VALUE), Float.MIN_VALUE);
    // Should not have flush-to-zero behavior.
    Assert.assertEquals(Math.min(Float.MIN_VALUE, Float.MIN_VALUE), Float.MIN_VALUE);
  }

  public static void test_Math_max_F() {
    Math.max(1.0f, Float.NaN);
    Assert.assertTrue(Float.isNaN(Math.max(1.0f, Float.NaN)));
    Assert.assertTrue(Float.isNaN(Math.max(Float.NaN, 1.0f)));
    Assert.assertEquals(Math.max(-0.0f, 0.0f), 0.0f);
    Assert.assertEquals(Math.max(0.0f, -0.0f), 0.0f);
    Assert.assertEquals(Math.max(-0.0f, -0.0f), -0.0f);
    Assert.assertEquals(Math.max(0.0f, 0.0f), 0.0f);
    Assert.assertEquals(Math.max(1.0f, 0.0f), 1.0f);
    Assert.assertEquals(Math.max(0.0f, 1.0f), 1.0f);
    Assert.assertEquals(Math.max(0.0f, Float.MAX_VALUE), Float.MAX_VALUE);
    Assert.assertEquals(Math.max(Float.MIN_VALUE, Float.MAX_VALUE), Float.MAX_VALUE);
    // Should not have flush-to-zero behavior.
    Assert.assertEquals(Math.max(Float.MIN_VALUE, 0.0f), Float.MIN_VALUE);
    Assert.assertEquals(Math.max(Float.MIN_VALUE, Float.MIN_VALUE), Float.MIN_VALUE);
  }

  public static void test_Math_min_D() {
    Math.min(1.0d, Double.NaN);
    Assert.assertTrue(Double.isNaN(Math.min(1.0d, Double.NaN)));
    Assert.assertTrue(Double.isNaN(Math.min(Double.NaN, 1.0d)));
    Assert.assertEquals(Math.min(-0.0d, 0.0d), -0.0d);
    Assert.assertEquals(Math.min(0.0d, -0.0d), -0.0d);
    Assert.assertEquals(Math.min(-0.0d, -0.0d), -0.0d);
    Assert.assertEquals(Math.min(0.0d, 0.0d), 0.0d);
    Assert.assertEquals(Math.min(1.0d, 0.0d), 0.0d);
    Assert.assertEquals(Math.min(0.0d, 1.0d), 0.0d);
    Assert.assertEquals(Math.min(0.0d, Double.MAX_VALUE), 0.0d);
    Assert.assertEquals(Math.min(Double.MIN_VALUE, 0.0d), 0.0d);
    Assert.assertEquals(Math.min(Double.MIN_VALUE, Double.MAX_VALUE), Double.MIN_VALUE);
    // Should not have flush-to-zero behavior.
    Assert.assertEquals(Math.min(Double.MIN_VALUE, Double.MIN_VALUE), Double.MIN_VALUE);
  }

  public static void test_Math_max_D() {
    Math.max(1.0d, Double.NaN);
    Assert.assertTrue(Double.isNaN(Math.max(1.0d, Double.NaN)));
    Assert.assertTrue(Double.isNaN(Math.max(Double.NaN, 1.0d)));
    Assert.assertEquals(Math.max(-0.0d, 0.0d), 0.0d);
    Assert.assertEquals(Math.max(0.0d, -0.0d), 0.0d);
    Assert.assertEquals(Math.max(-0.0d, -0.0d), -0.0d);
    Assert.assertEquals(Math.max(0.0d, 0.0d), 0.0d);
    Assert.assertEquals(Math.max(1.0d, 0.0d), 1.0d);
    Assert.assertEquals(Math.max(0.0d, 1.0d), 1.0d);
    Assert.assertEquals(Math.max(0.0d, Double.MAX_VALUE), Double.MAX_VALUE);
    Assert.assertEquals(Math.max(Double.MIN_VALUE, 0.0d), Double.MIN_VALUE);
    Assert.assertEquals(Math.max(Double.MIN_VALUE, Double.MAX_VALUE), Double.MAX_VALUE);
    // Should not have flush-to-zero behavior.
    Assert.assertEquals(Math.max(Double.MIN_VALUE, 0.0d), Double.MIN_VALUE);
    Assert.assertEquals(Math.max(Double.MIN_VALUE, Double.MIN_VALUE), Double.MIN_VALUE);
  }

  public static void test_Math_sqrt() {
    Math.sqrt(+4.0);
    Assert.assertEquals(Math.sqrt(+4.0), +2.0d, 0.0);
    Assert.assertEquals(Math.sqrt(+49.0), +7.0d, 0.0);
    Assert.assertEquals(Math.sqrt(+1.44), +1.2d, 0.0);
  }

  public static void test_Math_ceil() {
    Math.ceil(-0.9);
    Assert.assertEquals(Math.ceil(+0.0), +0.0d, 0.0);
    Assert.assertEquals(Math.ceil(-0.0), -0.0d, 0.0);
    Assert.assertEquals(Math.ceil(-0.9), -0.0d, 0.0);
    Assert.assertEquals(Math.ceil(-0.5), -0.0d, 0.0);
    Assert.assertEquals(Math.ceil(0.0), -0.0d, 0.0);
    Assert.assertEquals(Math.ceil(+2.0), +2.0d, 0.0);
    Assert.assertEquals(Math.ceil(+2.1), +3.0d, 0.0);
    Assert.assertEquals(Math.ceil(+2.5), +3.0d, 0.0);
    Assert.assertEquals(Math.ceil(+2.9), +3.0d, 0.0);
    Assert.assertEquals(Math.ceil(+3.0), +3.0d, 0.0);
    Assert.assertEquals(Math.ceil(-2.0), -2.0d, 0.0);
    Assert.assertEquals(Math.ceil(-2.1), -2.0d, 0.0);
    Assert.assertEquals(Math.ceil(-2.5), -2.0d, 0.0);
    Assert.assertEquals(Math.ceil(-2.9), -2.0d, 0.0);
    Assert.assertEquals(Math.ceil(-3.0), -3.0d, 0.0);
    // 2^52 - 1.5
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x432FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0x432FFFFFFFFFFFFEl), 0.0);
    // 2^52 - 0.5
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x432FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x4330000000000000l), 0.0);
    // 2^52
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x4330000000000000l)),
                        Double.longBitsToDouble(0x4330000000000000l), 0.0);
    // 2^53 - 1
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x433FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x433FFFFFFFFFFFFFl), 0.0);
    // 2^53
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x4340000000000000l)),
                        Double.longBitsToDouble(0x4340000000000000l), 0.0);
    // 2^63 - 2^10
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl), 0.0);
    // 2^63
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x43E0000000000000l)),
                        Double.longBitsToDouble(0x43E0000000000000l), 0.0);
    // 2^64
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0x43F0000000000000l)),
                        Double.longBitsToDouble(0x43F0000000000000l), 0.0);
    // -(2^52 - 1.5)
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC32FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0xC32FFFFFFFFFFFFCl), 0.0);
    // -(2^52 - 0.5)
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC32FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC32FFFFFFFFFFFFEl), 0.0);
    // -2^52
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC330000000000000l)),
                        Double.longBitsToDouble(0xC330000000000000l), 0.0);
    // -(2^53 - 1)
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl), 0.0);
    // -2^53
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC340000000000000l)),
                        Double.longBitsToDouble(0xC340000000000000l), 0.0);
    // -(2^63 - 2^10)
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl), 0.0);
    // -2^63
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC3E0000000000000l)),
                        Double.longBitsToDouble(0xC3E0000000000000l), 0.0);
    // -2^64
    Assert.assertEquals(Math.ceil(Double.longBitsToDouble(0xC3F0000000000000l)),
                        Double.longBitsToDouble(0xC3F0000000000000l), 0.0);
    Assert.assertEquals(Math.ceil(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(Math.ceil(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(Math.ceil(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_Math_floor() {
    Math.floor(+2.1);
    Assert.assertEquals(Math.floor(+0.0), +0.0d, 0.0);
    Assert.assertEquals(Math.floor(-0.0), -0.0d, 0.0);
    Assert.assertEquals(Math.floor(+2.0), +2.0d, 0.0);
    Assert.assertEquals(Math.floor(+2.1), +2.0d, 0.0);
    Assert.assertEquals(Math.floor(+2.5), +2.0d, 0.0);
    Assert.assertEquals(Math.floor(+2.9), +2.0d, 0.0);
    Assert.assertEquals(Math.floor(+3.0), +3.0d, 0.0);
    Assert.assertEquals(Math.floor(-2.0), -2.0d, 0.0);
    Assert.assertEquals(Math.floor(-2.1), -3.0d, 0.0);
    Assert.assertEquals(Math.floor(-2.5), -3.0d, 0.0);
    Assert.assertEquals(Math.floor(-2.9), -3.0d, 0.0);
    Assert.assertEquals(Math.floor(-3.0), -3.0d, 0.0);
    // 2^52 - 1.5
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x432FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0x432FFFFFFFFFFFFCl), 0.0);
    // 2^52 - 0.5
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x432FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x432FFFFFFFFFFFFEl), 0.0);
    // 2^52
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x4330000000000000l)),
                        Double.longBitsToDouble(0x4330000000000000l), 0.0);
    // 2^53 - 1
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x433FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x433FFFFFFFFFFFFFl), 0.0);
    // 2^53
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x4340000000000000l)),
                        Double.longBitsToDouble(0x4340000000000000l), 0.0);
    // 2^63 - 2^10
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl), 0.0);
    // 2^63
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x43E0000000000000l)),
                        Double.longBitsToDouble(0x43E0000000000000l), 0.0);
    // 2^64
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0x43F0000000000000l)),
                        Double.longBitsToDouble(0x43F0000000000000l), 0.0);
    // -(2^52 - 1.5)
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC32FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0xC32FFFFFFFFFFFFEl), 0.0);
    // -(2^52 - 0.5)
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC32FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC330000000000000l), 0.0);
    // -2^52
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC330000000000000l)),
                        Double.longBitsToDouble(0xC330000000000000l), 0.0);
    // -(2^53 - 1)
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl), 0.0);
    // -2^53
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC340000000000000l)),
                        Double.longBitsToDouble(0xC340000000000000l), 0.0);
    // -(2^63 - 2^10)
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl), 0.0);
    // -2^63
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC3E0000000000000l)),
                        Double.longBitsToDouble(0xC3E0000000000000l), 0.0);
    // -2^64
    Assert.assertEquals(Math.floor(Double.longBitsToDouble(0xC3F0000000000000l)),
                        Double.longBitsToDouble(0xC3F0000000000000l), 0.0);
    Assert.assertEquals(Math.floor(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(Math.floor(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(Math.floor(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_Math_rint() {
    Math.rint(+2.1);
    Assert.assertEquals(Math.rint(+0.0), +0.0d, 0.0);
    Assert.assertEquals(Math.rint(-0.0), -0.0d, 0.0);
    Assert.assertEquals(Math.rint(+0.5), +0.0d, 0.0);  // expects tie-to-even
    Assert.assertEquals(Math.rint(+2.0), +2.0d, 0.0);
    Assert.assertEquals(Math.rint(+2.1), +2.0d, 0.0);
    Assert.assertEquals(Math.rint(+2.5), +2.0d, 0.0);  // expects tie-to-even
    Assert.assertEquals(Math.rint(+2.9), +3.0d, 0.0);
    Assert.assertEquals(Math.rint(+3.0), +3.0d, 0.0);
    Assert.assertEquals(Math.rint(+3.5), +4.0d, 0.0);  // expects tie-to-even
    Assert.assertEquals(Math.rint(-2.0), -2.0d, 0.0);
    Assert.assertEquals(Math.rint(-2.1), -2.0d, 0.0);
    Assert.assertEquals(Math.rint(-2.5), -2.0d, 0.0);  // expects tie-to-even
    Assert.assertEquals(Math.rint(-2.9), -3.0d, 0.0);
    Assert.assertEquals(Math.rint(-3.0), -3.0d, 0.0);
    Assert.assertEquals(Math.rint(-3.5), -4.0d, 0.0);  // expects tie-to-even
    // 2^52 - 1.5
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x432FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0x432FFFFFFFFFFFFCl), 0.0);
    // 2^52 - 0.5
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x432FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x4330000000000000l), 0.0);
    // 2^52
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x4330000000000000l)),
                        Double.longBitsToDouble(0x4330000000000000l), 0.0);
    // 2^53 - 1
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x433FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x433FFFFFFFFFFFFFl), 0.0);
    // 2^53
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x4340000000000000l)),
                        Double.longBitsToDouble(0x4340000000000000l), 0.0);
    // 2^63 - 2^10
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0x43DFFFFFFFFFFFFFl), 0.0);
    // 2^63
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x43E0000000000000l)),
                        Double.longBitsToDouble(0x43E0000000000000l), 0.0);
    // 2^64
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0x43F0000000000000l)),
                        Double.longBitsToDouble(0x43F0000000000000l), 0.0);
    // -(2^52 - 1.5)
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC32FFFFFFFFFFFFDl)),
                        Double.longBitsToDouble(0xC32FFFFFFFFFFFFCl), 0.0);
    // -(2^52 - 0.5)
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC32FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC330000000000000l), 0.0);
    // -2^52
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC330000000000000l)),
                        Double.longBitsToDouble(0xC330000000000000l), 0.0);
    // -(2^53 - 1)
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC33FFFFFFFFFFFFFl), 0.0);
    // -2^53
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC340000000000000l)),
                        Double.longBitsToDouble(0xC340000000000000l), 0.0);
    // -(2^63 - 2^10)
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl)),
                        Double.longBitsToDouble(0xC3DFFFFFFFFFFFFFl), 0.0);
    // -2^63
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC3E0000000000000l)),
                        Double.longBitsToDouble(0xC3E0000000000000l), 0.0);
    // -2^64
    Assert.assertEquals(Math.rint(Double.longBitsToDouble(0xC3F0000000000000l)),
                        Double.longBitsToDouble(0xC3F0000000000000l), 0.0);
    Assert.assertEquals(Math.rint(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(Math.rint(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(Math.rint(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_Math_round_D() {
    Math.round(2.1d);
    Assert.assertEquals(Math.round(+0.0d), (long)+0.0);
    Assert.assertEquals(Math.round(-0.0d), (long)+0.0);
    Assert.assertEquals(Math.round(2.0d), 2l);
    Assert.assertEquals(Math.round(2.1d), 2l);
    Assert.assertEquals(Math.round(2.5d), 3l);
    Assert.assertEquals(Math.round(2.9d), 3l);
    Assert.assertEquals(Math.round(3.0d), 3l);
    Assert.assertEquals(Math.round(-2.0d), -2l);
    Assert.assertEquals(Math.round(-2.1d), -2l);
    Assert.assertEquals(Math.round(-2.5d), -2l);
    Assert.assertEquals(Math.round(-2.9d), -3l);
    Assert.assertEquals(Math.round(-3.0d), -3l);
    Assert.assertEquals(Math.round(0.49999999999999994d), 0l);
    Assert.assertEquals(Math.round(4503599627370495.0d), 4503599627370495l);  // 2^52 - 1
    Assert.assertEquals(Math.round(4503599627370495.5d), 4503599627370496l);  // 2^52 - 0.5
    Assert.assertEquals(Math.round(4503599627370496.0d), 4503599627370496l);  // 2^52
    Assert.assertEquals(Math.round(-4503599627370495.0d), -4503599627370495l);  // -(2^52 - 1)
    Assert.assertEquals(Math.round(-4503599627370495.5d), -4503599627370495l);  // -(2^52 - 0.5)
    Assert.assertEquals(Math.round(-4503599627370496.0d), -4503599627370496l);  // -2^52
    Assert.assertEquals(Math.round(9007199254740991.0d), 9007199254740991l);  // 2^53 - 1
    Assert.assertEquals(Math.round(-9007199254740991.0d), -9007199254740991l);  // -(2^53 - 1)
    Assert.assertEquals(Math.round(Double.NaN), (long)+0.0d);
    Assert.assertEquals(Math.round(Long.MAX_VALUE + 1.0d), Long.MAX_VALUE);
    Assert.assertEquals(Math.round(Long.MIN_VALUE - 1.0d), Long.MIN_VALUE);
    Assert.assertEquals(Math.round(Double.longBitsToDouble(0x43F0000000000000l)),
                        Long.MAX_VALUE); // 2^64
    Assert.assertEquals(Math.round(Double.longBitsToDouble(0xC3F0000000000000l)),
                        Long.MIN_VALUE); // -2^64
    Assert.assertEquals(Math.round(Double.POSITIVE_INFINITY), Long.MAX_VALUE);
    Assert.assertEquals(Math.round(Double.NEGATIVE_INFINITY), Long.MIN_VALUE);
  }

  public static void test_Math_round_F() {
    Math.round(2.1f);
    Assert.assertEquals(Math.round(+0.0f), (int)+0.0);
    Assert.assertEquals(Math.round(-0.0f), (int)+0.0);
    Assert.assertEquals(Math.round(2.0f), 2);
    Assert.assertEquals(Math.round(2.1f), 2);
    Assert.assertEquals(Math.round(2.5f), 3);
    Assert.assertEquals(Math.round(2.9f), 3);
    Assert.assertEquals(Math.round(3.0f), 3);
    Assert.assertEquals(Math.round(-2.0f), -2);
    Assert.assertEquals(Math.round(-2.1f), -2);
    Assert.assertEquals(Math.round(-2.5f), -2);
    Assert.assertEquals(Math.round(-2.9f), -3);
    Assert.assertEquals(Math.round(-3.0f), -3);
    // 0.4999999701976776123046875
    Assert.assertEquals(Math.round(Float.intBitsToFloat(0x3EFFFFFF)), (int)+0.0f);
    Assert.assertEquals(Math.round(8388607.0f), 8388607);  // 2^23 - 1
    Assert.assertEquals(Math.round(8388607.5f), 8388608);  // 2^23 - 0.5
    Assert.assertEquals(Math.round(8388608.0f), 8388608);  // 2^23
    Assert.assertEquals(Math.round(-8388607.0f), -8388607);  // -(2^23 - 1)
    Assert.assertEquals(Math.round(-8388607.5f), -8388607);  // -(2^23 - 0.5)
    Assert.assertEquals(Math.round(-8388608.0f), -8388608);  // -2^23
    Assert.assertEquals(Math.round(16777215.0f), 16777215);  // 2^24 - 1
    Assert.assertEquals(Math.round(16777216.0f), 16777216);  // 2^24
    Assert.assertEquals(Math.round(-16777215.0f), -16777215);  // -(2^24 - 1)
    Assert.assertEquals(Math.round(-16777216.0f), -16777216);  // -2^24
    Assert.assertEquals(Math.round(Float.NaN), (int)+0.0f);
    Assert.assertEquals(Math.round(Integer.MAX_VALUE + 1.0f), Integer.MAX_VALUE);
    Assert.assertEquals(Math.round(Integer.MIN_VALUE - 1.0f), Integer.MIN_VALUE);
    Assert.assertEquals(Math.round(Float.intBitsToFloat(0x4F800000)),
                        Integer.MAX_VALUE); // 2^32
    Assert.assertEquals(Math.round(Float.intBitsToFloat(0xCF800000)),
                        Integer.MIN_VALUE); // -2^32
    Assert.assertEquals(Math.round(Float.POSITIVE_INFINITY), Integer.MAX_VALUE);
    Assert.assertEquals(Math.round(Float.NEGATIVE_INFINITY), Integer.MIN_VALUE);
  }

  public static void test_Math_isNaN_D() {
    // Quiet NaN.
    Assert.assertTrue(Double.isNaN(Double.longBitsToDouble(0x7FF4000000000000l)));
    Assert.assertTrue(Double.isNaN(Double.longBitsToDouble(0xFFF4000000000000l)));
    // Signaling NaN.
    Assert.assertTrue(Double.isNaN(Double.longBitsToDouble(0x7FF8000000000000l)));
    Assert.assertTrue(Double.isNaN(Double.longBitsToDouble(0xFFF8000000000000l)));
    // Distinct from +/- infinity.
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x7FF0000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0xFFF0000000000000l)));
    // Distinct from normal numbers.
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x7FE0000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0xFFE0000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x0010000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x8010000000000000l)));
    // Distinct from +/- zero.
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x0000000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x8000000000000000l)));
    // Distinct from subnormal numbers.
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x0008000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x8008000000000000l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x0000000000000001l)));
    Assert.assertFalse(Double.isNaN(Double.longBitsToDouble(0x8000000000000001l)));
  }

  public static void test_Math_isNaN_F() {
    // Quiet NaN.
    Assert.assertTrue(Float.isNaN(Float.intBitsToFloat(0x7FA00000)));
    Assert.assertTrue(Float.isNaN(Float.intBitsToFloat(0xFFA00000)));
    // Signaling NaN.
    Assert.assertTrue(Float.isNaN(Float.intBitsToFloat(0x7FC00000)));
    Assert.assertTrue(Float.isNaN(Float.intBitsToFloat(0xFFC00000)));
    // Distinct from +/- infinity.
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x7F800000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0xFF800000)));
    // Distinct from normal numbers.
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x7F000000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0xFF000000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x00800000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x80800000)));
    // Distinct from +/- zero.
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x00000000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x80000000)));
    // Distinct from subnormal numbers.
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x00400000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x80400000)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x00000001)));
    Assert.assertFalse(Float.isNaN(Float.intBitsToFloat(0x80000001)));
  }

  public static void test_Math_isInfinite_D() {
    // Distinct from Quiet NaN.
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x7FF4000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0xFFF4000000000000l)));
    // Distinct from Signaling NaN.
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x7FF8000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0xFFF8000000000000l)));
    // +/- infinity.
    Assert.assertTrue(Double.isInfinite(Double.longBitsToDouble(0x7FF0000000000000l)));
    Assert.assertTrue(Double.isInfinite(Double.longBitsToDouble(0xFFF0000000000000l)));
    // Distinct from normal numbers.
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x7FE0000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0xFFE0000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x0010000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x8010000000000000l)));
    // Distinct from +/- zero.
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x0000000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x8000000000000000l)));
    // Distinct from subnormal numbers.
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x0008000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x8008000000000000l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x0000000000000001l)));
    Assert.assertFalse(Double.isInfinite(Double.longBitsToDouble(0x8000000000000001l)));
  }

  public static void test_Math_isInfinite_F() {
    // Distinct from Quiet NaN.
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x7FA00000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0xFFA00000)));
    // Distinct from Signaling NaN.
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x7FC00000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0xFFC00000)));
    // +/- infinity.
    Assert.assertTrue(Float.isInfinite(Float.intBitsToFloat(0x7F800000)));
    Assert.assertTrue(Float.isInfinite(Float.intBitsToFloat(0xFF800000)));
    // Distinct from normal numbers.
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x7F000000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0xFF000000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x00800000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x80800000)));
    // Distinct from +/- zero.
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x00000000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x80000000)));
    // Distinct from subnormal numbers.
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x00400000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x80400000)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x00000001)));
    Assert.assertFalse(Float.isInfinite(Float.intBitsToFloat(0x80000001)));
  }

  public static void test_StrictMath_abs_I() {
    StrictMath.abs(-1);
    Assert.assertEquals(StrictMath.abs(0), 0);
    Assert.assertEquals(StrictMath.abs(123), 123);
    Assert.assertEquals(StrictMath.abs(-123), 123);
    Assert.assertEquals(StrictMath.abs(Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE), Integer.MIN_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE - 1), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE + 1), Integer.MAX_VALUE);
  }

  public static void test_StrictMath_abs_J() {
    StrictMath.abs(-1L);
    Assert.assertEquals(StrictMath.abs(0L), 0L);
    Assert.assertEquals(StrictMath.abs(123L), 123L);
    Assert.assertEquals(StrictMath.abs(-123L), 123L);
    Assert.assertEquals(StrictMath.abs(Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Long.MIN_VALUE), Long.MIN_VALUE);
    Assert.assertEquals(StrictMath.abs(Long.MIN_VALUE - 1), Long.MAX_VALUE);
  }

  public static void test_StrictMath_min_I() {
    StrictMath.min(1, 0);
    Assert.assertEquals(StrictMath.min(0, 0), 0);
    Assert.assertEquals(StrictMath.min(1, 0), 0);
    Assert.assertEquals(StrictMath.min(0, 1), 0);
    Assert.assertEquals(StrictMath.min(0, Integer.MAX_VALUE), 0);
    Assert.assertEquals(StrictMath.min(Integer.MIN_VALUE, 0), Integer.MIN_VALUE);
    Assert.assertEquals(StrictMath.min(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MIN_VALUE);
  }

  public static void test_StrictMath_max_I() {
    StrictMath.max(1, 0);
    Assert.assertEquals(StrictMath.max(0, 0), 0);
    Assert.assertEquals(StrictMath.max(1, 0), 1);
    Assert.assertEquals(StrictMath.max(0, 1), 1);
    Assert.assertEquals(StrictMath.max(0, Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.max(Integer.MIN_VALUE, 0), 0);
    Assert.assertEquals(StrictMath.max(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MAX_VALUE);
  }

  public static void test_StrictMath_min_J() {
    StrictMath.min(1L, 0L);
    Assert.assertEquals(StrictMath.min(0L, 0L), 0L);
    Assert.assertEquals(StrictMath.min(1L, 0L), 0L);
    Assert.assertEquals(StrictMath.min(0L, 1L), 0L);
    Assert.assertEquals(StrictMath.min(0L, Long.MAX_VALUE), 0L);
    Assert.assertEquals(StrictMath.min(Long.MIN_VALUE, 0L), Long.MIN_VALUE);
    Assert.assertEquals(StrictMath.min(Long.MIN_VALUE, Long.MAX_VALUE), Long.MIN_VALUE);
  }

  public static void test_StrictMath_max_J() {
    StrictMath.max(1L, 0L);
    Assert.assertEquals(StrictMath.max(0L, 0L), 0L);
    Assert.assertEquals(StrictMath.max(1L, 0L), 1L);
    Assert.assertEquals(StrictMath.max(0L, 1L), 1L);
    Assert.assertEquals(StrictMath.max(0L, Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(StrictMath.max(Long.MIN_VALUE, 0L), 0L);
    Assert.assertEquals(StrictMath.max(Long.MIN_VALUE, Long.MAX_VALUE), Long.MAX_VALUE);
  }

  public static void test_StrictMath_min_F() {
    StrictMath.min(1.0f, Float.NaN);
    Assert.assertTrue(Float.isNaN(StrictMath.min(1.0f, Float.NaN)));
    Assert.assertTrue(Float.isNaN(StrictMath.min(Float.NaN, 1.0f)));
    Assert.assertEquals(StrictMath.min(-0.0f, 0.0f), -0.0f);
    Assert.assertEquals(StrictMath.min(0.0f, -0.0f), -0.0f);
    Assert.assertEquals(StrictMath.min(-0.0f, -0.0f), -0.0f);
    Assert.assertEquals(StrictMath.min(0.0f, 0.0f), 0.0f);
    Assert.assertEquals(StrictMath.min(1.0f, 0.0f), 0.0f);
    Assert.assertEquals(StrictMath.min(0.0f, 1.0f), 0.0f);
    Assert.assertEquals(StrictMath.min(0.0f, Float.MAX_VALUE), 0.0f);
    Assert.assertEquals(StrictMath.min(Float.MIN_VALUE, 0.0f), 0.0f);
    Assert.assertEquals(StrictMath.min(Float.MIN_VALUE, Float.MAX_VALUE), Float.MIN_VALUE);
  }

  public static void test_StrictMath_max_F() {
    StrictMath.max(1.0f, Float.NaN);
    Assert.assertTrue(Float.isNaN(StrictMath.max(1.0f, Float.NaN)));
    Assert.assertTrue(Float.isNaN(StrictMath.max(Float.NaN, 1.0f)));
    Assert.assertEquals(StrictMath.max(-0.0f, 0.0f), 0.0f);
    Assert.assertEquals(StrictMath.max(0.0f, -0.0f), 0.0f);
    Assert.assertEquals(StrictMath.max(-0.0f, -0.0f), -0.0f);
    Assert.assertEquals(StrictMath.max(0.0f, 0.0f), 0.0f);
    Assert.assertEquals(StrictMath.max(1.0f, 0.0f), 1.0f);
    Assert.assertEquals(StrictMath.max(0.0f, 1.0f), 1.0f);
    Assert.assertEquals(StrictMath.max(0.0f, Float.MAX_VALUE), Float.MAX_VALUE);
    Assert.assertEquals(StrictMath.max(Float.MIN_VALUE, 0.0f), Float.MIN_VALUE);
    Assert.assertEquals(StrictMath.max(Float.MIN_VALUE, Float.MAX_VALUE), Float.MAX_VALUE);
  }

  public static void test_StrictMath_min_D() {
    StrictMath.min(1.0d, Double.NaN);
    Assert.assertTrue(Double.isNaN(StrictMath.min(1.0d, Double.NaN)));
    Assert.assertTrue(Double.isNaN(StrictMath.min(Double.NaN, 1.0d)));
    Assert.assertEquals(StrictMath.min(-0.0d, 0.0d), -0.0d);
    Assert.assertEquals(StrictMath.min(0.0d, -0.0d), -0.0d);
    Assert.assertEquals(StrictMath.min(-0.0d, -0.0d), -0.0d);
    Assert.assertEquals(StrictMath.min(0.0d, 0.0d), 0.0d);
    Assert.assertEquals(StrictMath.min(1.0d, 0.0d), 0.0d);
    Assert.assertEquals(StrictMath.min(0.0d, 1.0d), 0.0d);
    Assert.assertEquals(StrictMath.min(0.0d, Double.MAX_VALUE), 0.0d);
    Assert.assertEquals(StrictMath.min(Double.MIN_VALUE, 0.0d), 0.0d);
    Assert.assertEquals(StrictMath.min(Double.MIN_VALUE, Double.MAX_VALUE), Double.MIN_VALUE);
  }

  public static void test_StrictMath_max_D() {
    StrictMath.max(1.0d, Double.NaN);
    Assert.assertTrue(Double.isNaN(StrictMath.max(1.0d, Double.NaN)));
    Assert.assertTrue(Double.isNaN(StrictMath.max(Double.NaN, 1.0d)));
    Assert.assertEquals(StrictMath.max(-0.0d, 0.0d), 0.0d);
    Assert.assertEquals(StrictMath.max(0.0d, -0.0d), 0.0d);
    Assert.assertEquals(StrictMath.max(-0.0d, -0.0d), -0.0d);
    Assert.assertEquals(StrictMath.max(0.0d, 0.0d), 0.0d);
    Assert.assertEquals(StrictMath.max(1.0d, 0.0d), 1.0d);
    Assert.assertEquals(StrictMath.max(0.0d, 1.0d), 1.0d);
    Assert.assertEquals(StrictMath.max(0.0d, Double.MAX_VALUE), Double.MAX_VALUE);
    Assert.assertEquals(StrictMath.max(Double.MIN_VALUE, 0.0d), Double.MIN_VALUE);
    Assert.assertEquals(StrictMath.max(Double.MIN_VALUE, Double.MAX_VALUE), Double.MAX_VALUE);
  }

  public static void test_StrictMath_sqrt() {
    StrictMath.sqrt(+4.0);
    Assert.assertEquals(StrictMath.sqrt(+4.0), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.sqrt(+49.0), +7.0d, 0.0);
    Assert.assertEquals(StrictMath.sqrt(+1.44), +1.2d, 0.0);
  }

  public static void test_StrictMath_ceil() {
    StrictMath.ceil(-0.9);
    Assert.assertEquals(StrictMath.ceil(+0.0), +0.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-0.0), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-0.9), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-0.5), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(0.0), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(+2.0), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(+2.1), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(+2.5), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(+2.9), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(+3.0), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-2.0), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-2.1), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-2.5), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-2.9), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(-3.0), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.ceil(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(StrictMath.ceil(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(StrictMath.ceil(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_StrictMath_floor() {
    StrictMath.floor(+2.1);
    Assert.assertEquals(StrictMath.floor(+0.0), +0.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-0.0), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(+2.0), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(+2.1), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(+2.5), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(+2.9), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(+3.0), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-2.0), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-2.1), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-2.5), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-2.9), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(-3.0), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.floor(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(StrictMath.floor(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(StrictMath.floor(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_StrictMath_rint() {
    StrictMath.rint(+2.1);
    Assert.assertEquals(StrictMath.rint(+0.0), +0.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-0.0), -0.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(+2.0), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(+2.1), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(+2.5), +2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(+2.9), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(+3.0), +3.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-2.0), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-2.1), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-2.5), -2.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-2.9), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(-3.0), -3.0d, 0.0);
    Assert.assertEquals(StrictMath.rint(Double.NaN), Double.NaN, 0.0);
    Assert.assertEquals(StrictMath.rint(Double.POSITIVE_INFINITY), Double.POSITIVE_INFINITY, 0.0);
    Assert.assertEquals(StrictMath.rint(Double.NEGATIVE_INFINITY), Double.NEGATIVE_INFINITY, 0.0);
  }

  public static void test_StrictMath_round_D() {
    StrictMath.round(2.1d);
    Assert.assertEquals(StrictMath.round(+0.0d), (long)+0.0);
    Assert.assertEquals(StrictMath.round(-0.0d), (long)+0.0);
    Assert.assertEquals(StrictMath.round(2.0d), 2l);
    Assert.assertEquals(StrictMath.round(2.1d), 2l);
    Assert.assertEquals(StrictMath.round(2.5d), 3l);
    Assert.assertEquals(StrictMath.round(2.9d), 3l);
    Assert.assertEquals(StrictMath.round(3.0d), 3l);
    Assert.assertEquals(StrictMath.round(-2.0d), -2l);
    Assert.assertEquals(StrictMath.round(-2.1d), -2l);
    Assert.assertEquals(StrictMath.round(-2.5d), -2l);
    Assert.assertEquals(StrictMath.round(-2.9d), -3l);
    Assert.assertEquals(StrictMath.round(-3.0d), -3l);
    Assert.assertEquals(StrictMath.round(0.49999999999999994d), 0l);
    Assert.assertEquals(StrictMath.round(4503599627370495.0d), 4503599627370495l);  // 2^52 - 1
    Assert.assertEquals(StrictMath.round(4503599627370495.5d), 4503599627370496l);  // 2^52 - 0.5
    Assert.assertEquals(StrictMath.round(4503599627370496.0d), 4503599627370496l);  // 2^52
    Assert.assertEquals(StrictMath.round(-4503599627370495.0d), -4503599627370495l);  // -(2^52 - 1)
    Assert.assertEquals(StrictMath.round(-4503599627370495.5d), -4503599627370495l);  // -(2^52 - 0.5)
    Assert.assertEquals(StrictMath.round(-4503599627370496.0d), -4503599627370496l);  // -2^52
    Assert.assertEquals(StrictMath.round(9007199254740991.0d), 9007199254740991l);  // 2^53 - 1
    Assert.assertEquals(StrictMath.round(-9007199254740991.0d), -9007199254740991l);  // -(2^53 - 1)
    Assert.assertEquals(StrictMath.round(Double.NaN), (long)+0.0d);
    Assert.assertEquals(StrictMath.round(Long.MAX_VALUE + 1.0d), Long.MAX_VALUE);
    Assert.assertEquals(StrictMath.round(Long.MIN_VALUE - 1.0d), Long.MIN_VALUE);
    Assert.assertEquals(StrictMath.round(Double.longBitsToDouble(0x43F0000000000000l)),
                        Long.MAX_VALUE); // 2^64
    Assert.assertEquals(StrictMath.round(Double.longBitsToDouble(0xC3F0000000000000l)),
                        Long.MIN_VALUE); // -2^64
    Assert.assertEquals(StrictMath.round(Double.POSITIVE_INFINITY), Long.MAX_VALUE);
    Assert.assertEquals(StrictMath.round(Double.NEGATIVE_INFINITY), Long.MIN_VALUE);
  }

  public static void test_StrictMath_round_F() {
    StrictMath.round(2.1f);
    Assert.assertEquals(StrictMath.round(+0.0f), (int)+0.0);
    Assert.assertEquals(StrictMath.round(-0.0f), (int)+0.0);
    Assert.assertEquals(StrictMath.round(2.0f), 2);
    Assert.assertEquals(StrictMath.round(2.1f), 2);
    Assert.assertEquals(StrictMath.round(2.5f), 3);
    Assert.assertEquals(StrictMath.round(2.9f), 3);
    Assert.assertEquals(StrictMath.round(3.0f), 3);
    Assert.assertEquals(StrictMath.round(-2.0f), -2);
    Assert.assertEquals(StrictMath.round(-2.1f), -2);
    Assert.assertEquals(StrictMath.round(-2.5f), -2);
    Assert.assertEquals(StrictMath.round(-2.9f), -3);
    Assert.assertEquals(StrictMath.round(-3.0f), -3);
    // 0.4999999701976776123046875
    Assert.assertEquals(StrictMath.round(Float.intBitsToFloat(0x3EFFFFFF)), (int)+0.0f);
    Assert.assertEquals(StrictMath.round(8388607.0f), 8388607);  // 2^23 - 1
    Assert.assertEquals(StrictMath.round(8388607.5f), 8388608);  // 2^23 - 0.5
    Assert.assertEquals(StrictMath.round(8388608.0f), 8388608);  // 2^23
    Assert.assertEquals(StrictMath.round(-8388607.0f), -8388607);  // -(2^23 - 1)
    Assert.assertEquals(StrictMath.round(-8388607.5f), -8388607);  // -(2^23 - 0.5)
    Assert.assertEquals(StrictMath.round(-8388608.0f), -8388608);  // -2^23
    Assert.assertEquals(StrictMath.round(16777215.0f), 16777215);  // 2^24 - 1
    Assert.assertEquals(StrictMath.round(16777216.0f), 16777216);  // 2^24
    Assert.assertEquals(StrictMath.round(-16777215.0f), -16777215);  // -(2^24 - 1)
    Assert.assertEquals(StrictMath.round(-16777216.0f), -16777216);  // -2^24
    Assert.assertEquals(StrictMath.round(Float.NaN), (int)+0.0f);
    Assert.assertEquals(StrictMath.round(Integer.MAX_VALUE + 1.0f), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.round(Integer.MIN_VALUE - 1.0f), Integer.MIN_VALUE);
    Assert.assertEquals(StrictMath.round(Float.intBitsToFloat(0x4F800000)),
                        Integer.MAX_VALUE); // 2^32
    Assert.assertEquals(StrictMath.round(Float.intBitsToFloat(0xCF800000)),
                        Integer.MIN_VALUE); // -2^32
    Assert.assertEquals(StrictMath.round(Float.POSITIVE_INFINITY), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.round(Float.NEGATIVE_INFINITY), Integer.MIN_VALUE);
  }

  public static void test_Float_floatToRawIntBits() {
    Float.floatToRawIntBits(-1.0f);
    Assert.assertEquals(Float.floatToRawIntBits(-1.0f), 0xbf800000);
    Assert.assertEquals(Float.floatToRawIntBits(0.0f), 0);
    Assert.assertEquals(Float.floatToRawIntBits(1.0f), 0x3f800000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.NaN), 0x7fc00000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.POSITIVE_INFINITY), 0x7f800000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.NEGATIVE_INFINITY), 0xff800000);
  }

  public static void test_Float_intBitsToFloat() {
    Float.intBitsToFloat(0xbf800000);
    Assert.assertEquals(Float.intBitsToFloat(0xbf800000), -1.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x00000000), 0.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x3f800000), 1.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x7fc00000), Float.NaN);
    Assert.assertEquals(Float.intBitsToFloat(0x7f800000), Float.POSITIVE_INFINITY);
    Assert.assertEquals(Float.intBitsToFloat(0xff800000), Float.NEGATIVE_INFINITY);
  }

  public static void test_Double_doubleToRawLongBits() {
    Double.doubleToRawLongBits(-1.0);
    Assert.assertEquals(Double.doubleToRawLongBits(-1.0), 0xbff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(0.0), 0x0000000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(1.0), 0x3ff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.NaN), 0x7ff8000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.POSITIVE_INFINITY), 0x7ff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.NEGATIVE_INFINITY), 0xfff0000000000000L);
  }

  public static void test_Double_longBitsToDouble() {
    Double.longBitsToDouble(0xbff0000000000000L);
    Assert.assertEquals(Double.longBitsToDouble(0xbff0000000000000L), -1.0);
    Assert.assertEquals(Double.longBitsToDouble(0x0000000000000000L), 0.0);
    Assert.assertEquals(Double.longBitsToDouble(0x3ff0000000000000L), 1.0);
    Assert.assertEquals(Double.longBitsToDouble(0x7ff8000000000000L), Double.NaN);
    Assert.assertEquals(Double.longBitsToDouble(0x7ff0000000000000L), Double.POSITIVE_INFINITY);
    Assert.assertEquals(Double.longBitsToDouble(0xfff0000000000000L), Double.NEGATIVE_INFINITY);
  }

  public static void test_Short_reverseBytes() {
      Short.reverseBytes((short)0x1357);
      Assert.assertEquals(Short.reverseBytes((short)0x0000), (short)0x0000);
      Assert.assertEquals(Short.reverseBytes((short)0xffff), (short)0xffff);
      Assert.assertEquals(Short.reverseBytes((short)0x8000), (short)0x0080);
      Assert.assertEquals(Short.reverseBytes((short)0x0080), (short)0x8000);
      Assert.assertEquals(Short.reverseBytes((short)0x0123), (short)0x2301);
      Assert.assertEquals(Short.reverseBytes((short)0x4567), (short)0x6745);
      Assert.assertEquals(Short.reverseBytes((short)0x89ab), (short)0xab89);
      Assert.assertEquals(Short.reverseBytes((short)0xcdef), (short)0xefcd);
  }

  public static void test_Integer_reverseBytes() {
      Integer.reverseBytes(0x13579bdf);
      Assert.assertEquals(Integer.reverseBytes(0x00000000), 0x00000000);
      Assert.assertEquals(Integer.reverseBytes(0xffffffff), 0xffffffff);
      Assert.assertEquals(Integer.reverseBytes(0x80000000), 0x00000080);
      Assert.assertEquals(Integer.reverseBytes(0x00000080), 0x80000000);
      Assert.assertEquals(Integer.reverseBytes(0x01234567), 0x67452301);
      Assert.assertEquals(Integer.reverseBytes(0x89abcdef), 0xefcdab89);
  }

  public static void test_Long_reverseBytes() {
      Long.reverseBytes(0x13579bdf2468ace0L);
      Assert.assertEquals(Long.reverseBytes(0x0000000000000000L), 0x0000000000000000L);
      Assert.assertEquals(Long.reverseBytes(0xffffffffffffffffL), 0xffffffffffffffffL);
      Assert.assertEquals(Long.reverseBytes(0x8000000000000000L), 0x0000000000000080L);
      Assert.assertEquals(Long.reverseBytes(0x0000000000000080L), 0x8000000000000000L);
      Assert.assertEquals(Long.reverseBytes(0x0123456789abcdefL), 0xefcdab8967452301L);
  }

  public static void test_Integer_reverse() {
    Integer.reverse(0x12345678);
    Assert.assertEquals(Integer.reverse(1), 0x80000000);
    Assert.assertEquals(Integer.reverse(-1), 0xffffffff);
    Assert.assertEquals(Integer.reverse(0), 0);
    Assert.assertEquals(Integer.reverse(0x12345678), 0x1e6a2c48);
    Assert.assertEquals(Integer.reverse(0x87654321), 0x84c2a6e1);
    Assert.assertEquals(Integer.reverse(Integer.MAX_VALUE), 0xfffffffe);
    Assert.assertEquals(Integer.reverse(Integer.MIN_VALUE), 1);
  }

  public static void test_Long_reverse() {
    Long.reverse(0x1234567812345678L);
    Assert.assertEquals(Long.reverse(1L), 0x8000000000000000L);
    Assert.assertEquals(Long.reverse(-1L), 0xffffffffffffffffL);
    Assert.assertEquals(Long.reverse(0L), 0L);
    Assert.assertEquals(Long.reverse(0x1234567812345678L), 0x1e6a2c481e6a2c48L);
    Assert.assertEquals(Long.reverse(0x8765432187654321L), 0x84c2a6e184c2a6e1L);
    Assert.assertEquals(Long.reverse(Long.MAX_VALUE), 0xfffffffffffffffeL);
    Assert.assertEquals(Long.reverse(Long.MIN_VALUE), 1L);

    Assert.assertEquals(test_Long_reverse_b22324327(0xaaaaaaaaaaaaaaaaL, 0x5555555555555555L),
            157472205507277347L);
  }

  // A bit more complicated than the above. Use local variables to stress register allocation.
  private static long test_Long_reverse_b22324327(long l1, long l2) {
    // A couple of local integers. Use them in a loop, so they get promoted.
    int i1 = 0, i2 = 1, i3 = 2, i4 = 3, i5 = 4, i6 = 5, i7 = 6, i8 = 7;
    for (int k = 0; k < 10; k++) {
      i1 += 1;
      i2 += 2;
      i3 += 3;
      i4 += 4;
      i5 += 5;
      i6 += 6;
      i7 += 7;
      i8 += 8;
    }

    // Do the Long.reverse() calls, save the results.
    long r1 = Long.reverse(l1);
    long r2 = Long.reverse(l2);

    // Some more looping with the ints.
    for (int k = 0; k < 10; k++) {
      i1 += 1;
      i2 += 2;
      i3 += 3;
      i4 += 4;
      i5 += 5;
      i6 += 6;
      i7 += 7;
      i8 += 8;
    }

    // Include everything in the result, so things are kept live. Try to be a little bit clever to
    // avoid things being folded somewhere.
    return (r1 / i1) + (r2 / i2) + i3 + i4 + i5 + i6 + i7 + i8;
  }

  public static boolean doThrow = false;

  public static int $noinline$return_int_zero() {
    if (doThrow) {
      throw new Error();
    }
    return 0;
  }

  public static void test_Integer_numberOfLeadingZeros() {
    Assert.assertEquals(Integer.numberOfLeadingZeros(0), Integer.SIZE);
    Assert.assertEquals(Integer.numberOfLeadingZeros(1), Integer.SIZE - 1);
    Assert.assertEquals(Integer.numberOfLeadingZeros(1 << (Integer.SIZE-1)), 0);
    Assert.assertEquals(Integer.numberOfLeadingZeros($noinline$return_int_zero()), Integer.SIZE);
    for (int i = 0; i < Integer.SIZE; i++) {
        Assert.assertEquals(Integer.numberOfLeadingZeros(1 << i), Integer.SIZE - 1 - i);
        Assert.assertEquals(Integer.numberOfLeadingZeros((1 << i) | 1), Integer.SIZE - 1 - i);
        Assert.assertEquals(Integer.numberOfLeadingZeros(0xFFFFFFFF >>> i), i);
    }
  }

  public static long $noinline$return_long_zero() {
    if (doThrow) {
      throw new Error();
    }
    return 0;
  }

  public static void test_Long_numberOfLeadingZeros() {
    Assert.assertEquals(Long.numberOfLeadingZeros(0L), Long.SIZE);
    Assert.assertEquals(Long.numberOfLeadingZeros(1L), Long.SIZE - 1);
    Assert.assertEquals(Long.numberOfLeadingZeros(1L << ((Long.SIZE/2)-1)), Long.SIZE/2);
    Assert.assertEquals(Long.numberOfLeadingZeros(1L << (Long.SIZE-1)), 0);
    Assert.assertEquals(Long.numberOfLeadingZeros($noinline$return_long_zero()), Long.SIZE);
    for (int i = 0; i < Long.SIZE; i++) {
        Assert.assertEquals(Long.numberOfLeadingZeros(1L << i), Long.SIZE - 1 - i);
        Assert.assertEquals(Long.numberOfLeadingZeros((1L << i) | 1L), Long.SIZE - 1 - i);
        Assert.assertEquals(Long.numberOfLeadingZeros(0xFFFFFFFFFFFFFFFFL >>> i), i);
    }
  }

  static Object runtime;
  static Method address_of;
  static Method new_non_movable_array;
  static Method peek_byte;
  static Method peek_short;
  static Method peek_int;
  static Method peek_long;
  static Method poke_byte;
  static Method poke_short;
  static Method poke_int;
  static Method poke_long;

  public static void initSupportMethodsForPeekPoke() throws Exception {
    Class<?> vm_runtime = Class.forName("dalvik.system.VMRuntime");
    Method get_runtime = vm_runtime.getDeclaredMethod("getRuntime");
    runtime = get_runtime.invoke(null);
    address_of = vm_runtime.getDeclaredMethod("addressOf", Object.class);
    new_non_movable_array = vm_runtime.getDeclaredMethod("newNonMovableArray", Class.class, Integer.TYPE);

    Class<?> io_memory = Class.forName("libcore.io.Memory");
    peek_byte = io_memory.getDeclaredMethod("peekByte", Long.TYPE);
    peek_int = io_memory.getDeclaredMethod("peekInt", Long.TYPE, Boolean.TYPE);
    peek_short = io_memory.getDeclaredMethod("peekShort", Long.TYPE, Boolean.TYPE);
    peek_long = io_memory.getDeclaredMethod("peekLong", Long.TYPE, Boolean.TYPE);
    poke_byte = io_memory.getDeclaredMethod("pokeByte", Long.TYPE, Byte.TYPE);
    poke_short = io_memory.getDeclaredMethod("pokeShort", Long.TYPE, Short.TYPE, Boolean.TYPE);
    poke_int = io_memory.getDeclaredMethod("pokeInt", Long.TYPE, Integer.TYPE, Boolean.TYPE);
    poke_long = io_memory.getDeclaredMethod("pokeLong", Long.TYPE, Long.TYPE, Boolean.TYPE);
  }

  public static void test_Memory_peekByte() throws Exception {
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 2);
    b[0] = 0x12;
    b[1] = 0x11;
    long address = (long)address_of.invoke(runtime, b);
    Assert.assertEquals((byte)peek_byte.invoke(null, address), 0x12);
    Assert.assertEquals((byte)peek_byte.invoke(null, address + 1), 0x11);
  }

  public static void test_Memory_peekShort() throws Exception {
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 3);
    b[0] = 0x13;
    b[1] = 0x12;
    b[2] = 0x11;
    long address = (long)address_of.invoke(runtime, b);
    peek_short.invoke(null, address, false);
    Assert.assertEquals((short)peek_short.invoke(null, address, false), 0x1213);  // Aligned read
    Assert.assertEquals((short)peek_short.invoke(null, address + 1, false), 0x1112);  // Unaligned read
  }

  public static void test_Memory_peekInt() throws Exception {
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 5);
    b[0] = 0x15;
    b[1] = 0x14;
    b[2] = 0x13;
    b[3] = 0x12;
    b[4] = 0x11;
    long address = (long)address_of.invoke(runtime, b);
    peek_int.invoke(null, address, false);
    Assert.assertEquals((int)peek_int.invoke(null, address, false), 0x12131415);
    Assert.assertEquals((int)peek_int.invoke(null, address + 1, false), 0x11121314);
  }

  public static void test_Memory_peekLong() throws Exception {
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 9);
    b[0] = 0x19;
    b[1] = 0x18;
    b[2] = 0x17;
    b[3] = 0x16;
    b[4] = 0x15;
    b[5] = 0x14;
    b[6] = 0x13;
    b[7] = 0x12;
    b[8] = 0x11;
    long address = (long)address_of.invoke(runtime, b);
    peek_long.invoke(null, address, false);
    Assert.assertEquals((long)peek_long.invoke(null, address, false), 0x1213141516171819L);
    Assert.assertEquals((long)peek_long.invoke(null, address + 1, false), 0x1112131415161718L);
  }

  public static void test_Memory_pokeByte() throws Exception {
    byte[] r = {0x11, 0x12};
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 2);
    long address = (long)address_of.invoke(runtime, b);
    poke_byte.invoke(null, address, (byte)0x11);
    poke_byte.invoke(null, address + 1, (byte)0x12);
    Assert.assertTrue(Arrays.equals(r, b));
  }

  public static void test_Memory_pokeShort() throws Exception {
    byte[] ra = {0x12, 0x11, 0x13};
    byte[] ru = {0x12, 0x22, 0x21};
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 3);
    long address = (long)address_of.invoke(runtime, b);

    // Aligned write
    b[2] = 0x13;
    poke_short.invoke(null, address, (short)0x1112, false);
    Assert.assertTrue(Arrays.equals(ra, b));

    // Unaligned write
    poke_short.invoke(null, address + 1, (short)0x2122, false);
    Assert.assertTrue(Arrays.equals(ru, b));
  }

  public static void test_Memory_pokeInt() throws Exception {
    byte[] ra = {0x14, 0x13, 0x12, 0x11, 0x15};
    byte[] ru = {0x14, 0x24, 0x23, 0x22, 0x21};
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 5);
    long address = (long)address_of.invoke(runtime, b);

    b[4] = 0x15;
    poke_int.invoke(null, address, (int)0x11121314, false);
    Assert.assertTrue(Arrays.equals(ra, b));

    poke_int.invoke(null, address + 1, (int)0x21222324, false);
    Assert.assertTrue(Arrays.equals(ru, b));
  }

  public static void test_Memory_pokeLong() throws Exception {
    byte[] ra = {0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x19};
    byte[] ru = {0x18, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21};
    byte[] b = (byte[])new_non_movable_array.invoke(runtime, Byte.TYPE, 9);
    long address = (long)address_of.invoke(runtime, b);

    b[8] = 0x19;
    poke_long.invoke(null, address, (long)0x1112131415161718L, false);
    Assert.assertTrue(Arrays.equals(ra, b));

    poke_long.invoke(null, address + 1, (long)0x2122232425262728L, false);
    Assert.assertTrue(Arrays.equals(ru, b));
  }

  public static void test_Integer_numberOfTrailingZeros() {
    Assert.assertEquals(Integer.numberOfTrailingZeros(0), Integer.SIZE);
    for (int i = 0; i < Integer.SIZE; i++) {
      Assert.assertEquals(
        Integer.numberOfTrailingZeros(0x80000000 >> i),
        Integer.SIZE - 1 - i);
      Assert.assertEquals(
        Integer.numberOfTrailingZeros((0x80000000 >> i) | 0x80000000),
        Integer.SIZE - 1 - i);
      Assert.assertEquals(Integer.numberOfTrailingZeros(1 << i), i);
    }
  }

  public static void test_Long_numberOfTrailingZeros() {
    Assert.assertEquals(Long.numberOfTrailingZeros(0), Long.SIZE);
    for (int i = 0; i < Long.SIZE; i++) {
      Assert.assertEquals(
        Long.numberOfTrailingZeros(0x8000000000000000L >> i),
        Long.SIZE - 1 - i);
      Assert.assertEquals(
        Long.numberOfTrailingZeros((0x8000000000000000L >> i) | 0x8000000000000000L),
        Long.SIZE - 1 - i);
      Assert.assertEquals(Long.numberOfTrailingZeros(1L << i), i);
    }
  }

  public static void test_Integer_rotateRight() throws Exception {
    Assert.assertEquals(Integer.rotateRight(0x11, 0), 0x11);

    Assert.assertEquals(Integer.rotateRight(0x11, 1), 0x80000008);
    Assert.assertEquals(Integer.rotateRight(0x11, Integer.SIZE - 1), 0x22);
    Assert.assertEquals(Integer.rotateRight(0x11, Integer.SIZE), 0x11);
    Assert.assertEquals(Integer.rotateRight(0x11, Integer.SIZE + 1), 0x80000008);

    Assert.assertEquals(Integer.rotateRight(0x11, -1), 0x22);
    Assert.assertEquals(Integer.rotateRight(0x11, -(Integer.SIZE - 1)), 0x80000008);
    Assert.assertEquals(Integer.rotateRight(0x11, -Integer.SIZE), 0x11);
    Assert.assertEquals(Integer.rotateRight(0x11, -(Integer.SIZE + 1)), 0x22);

    Assert.assertEquals(Integer.rotateRight(0x80000000, 1), 0x40000000);

    for (int i = 0; i < Integer.SIZE; i++) {
      Assert.assertEquals(
        Integer.rotateRight(0xBBAAAADD, i),
        (0xBBAAAADD >>> i) | (0xBBAAAADD << (Integer.SIZE - i)));
    }
  }

  public static void test_Long_rotateRight() throws Exception {
    Assert.assertEquals(Long.rotateRight(0x11, 0), 0x11);

    Assert.assertEquals(Long.rotateRight(0x11, 1), 0x8000000000000008L);
    Assert.assertEquals(Long.rotateRight(0x11, Long.SIZE - 1), 0x22);
    Assert.assertEquals(Long.rotateRight(0x11, Long.SIZE), 0x11);
    Assert.assertEquals(Long.rotateRight(0x11, Long.SIZE + 1), 0x8000000000000008L);

    Assert.assertEquals(Long.rotateRight(0x11, -1), 0x22);
    Assert.assertEquals(Long.rotateRight(0x11, -(Long.SIZE - 1)), 0x8000000000000008L);
    Assert.assertEquals(Long.rotateRight(0x11, -Long.SIZE), 0x11);
    Assert.assertEquals(Long.rotateRight(0x11, -(Long.SIZE + 1)), 0x22);

    Assert.assertEquals(Long.rotateRight(0x8000000000000000L, 1), 0x4000000000000000L);

    for (int i = 0; i < Long.SIZE; i++) {
      Assert.assertEquals(
        Long.rotateRight(0xBBAAAADDFF0000DDL, i),
        (0xBBAAAADDFF0000DDL >>> i) | (0xBBAAAADDFF0000DDL << (Long.SIZE - i)));
    }
  }

  public static void test_Integer_rotateLeft() throws Exception {
    Assert.assertEquals(Integer.rotateLeft(0x11, 0), 0x11);

    Assert.assertEquals(Integer.rotateLeft(0x11, 1), 0x22);
    Assert.assertEquals(Integer.rotateLeft(0x11, Integer.SIZE - 1), 0x80000008);
    Assert.assertEquals(Integer.rotateLeft(0x11, Integer.SIZE), 0x11);
    Assert.assertEquals(Integer.rotateLeft(0x11, Integer.SIZE + 1), 0x22);

    Assert.assertEquals(Integer.rotateLeft(0x11, -1), 0x80000008);
    Assert.assertEquals(Integer.rotateLeft(0x11, -(Integer.SIZE - 1)), 0x22);
    Assert.assertEquals(Integer.rotateLeft(0x11, -Integer.SIZE), 0x11);
    Assert.assertEquals(Integer.rotateLeft(0x11, -(Integer.SIZE + 1)), 0x80000008);

    Assert.assertEquals(Integer.rotateLeft(0xC0000000, 1), 0x80000001);

    for (int i = 0; i < Integer.SIZE; i++) {
      Assert.assertEquals(
        Integer.rotateLeft(0xBBAAAADD, i),
        (0xBBAAAADD << i) | (0xBBAAAADD >>> (Integer.SIZE - i)));
    }
  }

  public static void test_Long_rotateLeft() throws Exception {
    Assert.assertEquals(Long.rotateLeft(0x11, 0), 0x11);

    Assert.assertEquals(Long.rotateLeft(0x11, 1), 0x22);
    Assert.assertEquals(Long.rotateLeft(0x11, Long.SIZE - 1), 0x8000000000000008L);
    Assert.assertEquals(Long.rotateLeft(0x11, Long.SIZE), 0x11);
    Assert.assertEquals(Long.rotateLeft(0x11, Long.SIZE + 1), 0x22);

    Assert.assertEquals(Long.rotateLeft(0x11, -1), 0x8000000000000008L);
    Assert.assertEquals(Long.rotateLeft(0x11, -(Long.SIZE - 1)), 0x22);
    Assert.assertEquals(Long.rotateLeft(0x11, -Long.SIZE), 0x11);
    Assert.assertEquals(Long.rotateLeft(0x11, -(Long.SIZE + 1)), 0x8000000000000008L);

    Assert.assertEquals(Long.rotateLeft(0xC000000000000000L, 1), 0x8000000000000001L);

    for (int i = 0; i < Long.SIZE; i++) {
      Assert.assertEquals(
        Long.rotateLeft(0xBBAAAADDFF0000DDL, i),
        (0xBBAAAADDFF0000DDL << i) | (0xBBAAAADDFF0000DDL >>> (Long.SIZE - i)));
    }
  }

  public static void test_Integer_rotateRightLeft() throws Exception {
    for (int i = 0; i < Integer.SIZE * 2; i++) {
      Assert.assertEquals(Integer.rotateLeft(0xBBAAAADD, i),
                          Integer.rotateRight(0xBBAAAADD, -i));
      Assert.assertEquals(Integer.rotateLeft(0xBBAAAADD, -i),
                          Integer.rotateRight(0xBBAAAADD, i));
    }
  }

  public static void test_Long_rotateRightLeft() throws Exception {
    for (int i = 0; i < Long.SIZE * 2; i++) {
      Assert.assertEquals(Long.rotateLeft(0xBBAAAADDFF0000DDL, i),
                          Long.rotateRight(0xBBAAAADDFF0000DDL, -i));
      Assert.assertEquals(Long.rotateLeft(0xBBAAAADDFF0000DDL, -i),
                          Long.rotateRight(0xBBAAAADDFF0000DDL, i));
    }
  }
}
