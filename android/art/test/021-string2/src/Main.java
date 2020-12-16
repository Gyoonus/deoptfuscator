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
import java.lang.reflect.Method;
import java.util.Locale;

/**
 * more string tests
 */
public class Main {
    public static void main(String args[]) throws Exception {
        String test = "0123456789";
        String test1 = new String("0123456789");    // different object
        String test2 = new String("0123456780");    // different value
        String offset = new String("xxx0123456789yyy");
        String sub = offset.substring(3, 13);
        Object blah = new Object();

        Assert.assertTrue(test.equals(test));
        Assert.assertTrue(test.equals(test1));
        Assert.assertFalse(test.equals(test2));

        Assert.assertEquals(test.compareTo(test1), 0);
        Assert.assertTrue(test1.compareTo(test2) > 0);
        Assert.assertTrue(test2.compareTo(test1) < 0);

        Assert.assertEquals("".compareTo(""), 0);
        Assert.assertTrue(test.compareTo("") > 0);
        Assert.assertTrue("".compareTo(test) < 0);

        /* compare string with a nonzero offset, in left/right side */
        Assert.assertEquals(test.compareTo(sub), 0);
        Assert.assertEquals(sub.compareTo(test), 0);
        Assert.assertTrue(test.equals(sub));
        Assert.assertTrue(sub.equals(test));
        /* same base, one is a substring */
        Assert.assertFalse(offset.equals(sub));
        Assert.assertFalse(sub.equals(offset));
        /* wrong class */
        Assert.assertFalse(test.equals(blah));

        /* null ptr - throw */
        try {
            test.compareTo(null);
            Assert.fail("didn't get expected npe");
        } catch (NullPointerException npe) {
            System.out.println("Got expected npe");
        }
        /* null ptr - ok */
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

        Class<?> Strings = Class.forName("com.android.org.bouncycastle.util.Strings");
        Method fromUTF8ByteArray = Strings.getDeclaredMethod("fromUTF8ByteArray", byte[].class);
        String result = (String) fromUTF8ByteArray.invoke(null, new byte[] {'O', 'K'});
        System.out.println(result);

        testCompareToAndEquals();
        testIndexOf();

        String s0_0 = "\u0000";
        String s0_1 = new String(s0_0);
        String s0_2 = new String(new char[] { '\u0000' });
        String s0_3 = s0_0 + "";
        System.out.println(
            " " + $noinline$equals(s0_0, s0_0) +
            " " + $noinline$equals(s0_0, s0_1) +
            " " + $noinline$equals(s0_0, s0_2) +
            " " + $noinline$equals(s0_0, s0_3));
        System.out.println(
            " " + $noinline$equals(s0_1, s0_0) +
            " " + $noinline$equals(s0_1, s0_1) +
            " " + $noinline$equals(s0_1, s0_2) +
            " " + $noinline$equals(s0_1, s0_3));
        System.out.println(
            " " + $noinline$equals(s0_2, s0_0) +
            " " + $noinline$equals(s0_2, s0_1) +
            " " + $noinline$equals(s0_2, s0_2) +
            " " + $noinline$equals(s0_2, s0_3));
        System.out.println(
            " " + $noinline$equals(s0_3, s0_0) +
            " " + $noinline$equals(s0_3, s0_1) +
            " " + $noinline$equals(s0_3, s0_2) +
            " " + $noinline$equals(s0_3, s0_3));

        testEqualsConstString();
        testConstStringEquals();

        // Regression tests for String.setCharAt() breaking string compression invariants.
        Locale en_US = new Locale("en", "US");
        Assert.assertEquals("I", /* Small latin dotless i */ "\u0131".toUpperCase());
        Assert.assertEquals("abc", "a\u0131c".replace('\u0131', 'b'));
        Assert.assertEquals("a\u0131c", "abc".replace('b', '\u0131'));

        // Regression test for scratch register exhaustion in String.equals() intrinsic on arm64.
        Assert.assertFalse(result.equals("Very long constant string, so that the known constant count field cannot be embedded in a CMP immediate instruction on arm64. Since it can hold 12-bit values, optionally shifted left by 12, let's go somewhere over 2^12, i.e. 4096. That should trigger the bug with or without string compression. 0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/"));
    }

    public static void testCompareToAndEquals() {
        String[] strings = {
                // Special: empty string.
                "",
                // Category 0, ASCII strings:
                //     "0123456789abcdef".substring(0, index + 1)
                "0",
                "01",
                "012",
                "0123",
                "01234",
                "012345",
                "0123456",
                "01234567",
                "012345678",
                "0123456789",
                "0123456789a",
                "0123456789ab",
                "0123456789abc",
                "0123456789abcd",
                "0123456789abcde",
                "0123456789abcdef",
                // Category 1, ASCII strings:
                //     "0123456789abcdef".substring(0, index) + "x"
                "x",
                "0x",
                "01x",
                "012x",
                "0123x",
                "01234x",
                "012345x",
                "0123456x",
                "01234567x",
                "012345678x",
                "0123456789x",
                "0123456789ax",
                "0123456789abx",
                "0123456789abcx",
                "0123456789abcdx",
                "0123456789abcdex",
                // Category 2, ASCII strings,
                //     "0123456789abcdef".substring(0, index) + "x" +
                //     "0123456789abcdef".substring(index + 1)
                "x123456789abcdef",
                "0x23456789abcdef",
                "01x3456789abcdef",
                "012x456789abcdef",
                "0123x56789abcdef",
                "01234x6789abcdef",
                "012345x789abcdef",
                "0123456x89abcdef",
                "01234567x9abcdef",
                "012345678xabcdef",
                "0123456789xbcdef",
                "0123456789axcdef",
                "0123456789abxdef",
                "0123456789abcxef",
                "0123456789abcdxf",
                "0123456789abcdex",
                // Category 3, ASCII strings:
                //     "z" + "0123456789abcdef".substring(1, index + 1)
                "z",
                "z1",
                "z12",
                "z123",
                "z1234",
                "z12345",
                "z123456",
                "z1234567",
                "z12345678",
                "z123456789",
                "z123456789a",
                "z123456789ab",
                "z123456789abc",
                "z123456789abcd",
                "z123456789abcde",
                "z123456789abcdef",
                // Category 4, non-ASCII strings:
                //     "0123456789abcdef".substring(0, index) + "\u0440"
                "\u0440",
                "0\u0440",
                "01\u0440",
                "012\u0440",
                "0123\u0440",
                "01234\u0440",
                "012345\u0440",
                "0123456\u0440",
                "01234567\u0440",
                "012345678\u0440",
                "0123456789\u0440",
                "0123456789a\u0440",
                "0123456789ab\u0440",
                "0123456789abc\u0440",
                "0123456789abcd\u0440",
                "0123456789abcde\u0440",
                // Category 5, non-ASCII strings:
                //     "0123456789abcdef".substring(0, index) + "\u0440" +
                //     "0123456789abcdef".substring(index + 1)
                "\u0440123456789abcdef",
                "0\u044023456789abcdef",
                "01\u04403456789abcdef",
                "012\u0440456789abcdef",
                "0123\u044056789abcdef",
                "01234\u04406789abcdef",
                "012345\u0440789abcdef",
                "0123456\u044089abcdef",
                "01234567\u04409abcdef",
                "012345678\u0440abcdef",
                "0123456789\u0440bcdef",
                "0123456789a\u0440cdef",
                "0123456789ab\u0440def",
                "0123456789abc\u0440ef",
                "0123456789abcd\u0440f",
                "0123456789abcde\u0440",
                // Category 6, ASCII strings:
                //     "\u0443" + "0123456789abcdef".substring(1, index + 1)
                "\u0443",
                "\u04431",
                "\u044312",
                "\u0443123",
                "\u04431234",
                "\u044312345",
                "\u0443123456",
                "\u04431234567",
                "\u044312345678",
                "\u0443123456789",
                "\u0443123456789a",
                "\u0443123456789ab",
                "\u0443123456789abc",
                "\u0443123456789abcd",
                "\u0443123456789abcde",
                "\u0443123456789abcdef",
                // Category 7, non-ASCII strings:
                //     "0123456789abcdef".substring(0, index) + "\u0482"
                "\u0482",
                "0\u0482",
                "01\u0482",
                "012\u0482",
                "0123\u0482",
                "01234\u0482",
                "012345\u0482",
                "0123456\u0482",
                "01234567\u0482",
                "012345678\u0482",
                "0123456789\u0482",
                "0123456789a\u0482",
                "0123456789ab\u0482",
                "0123456789abc\u0482",
                "0123456789abcd\u0482",
                "0123456789abcde\u0482",
                // Category 8, non-ASCII strings:
                //     "0123456789abcdef".substring(0, index) + "\u0482" +
                //     "0123456789abcdef".substring(index + 1)
                "\u0482123456789abcdef",
                "0\u048223456789abcdef",
                "01\u04823456789abcdef",
                "012\u0482456789abcdef",
                "0123\u048256789abcdef",
                "01234\u04826789abcdef",
                "012345\u0482789abcdef",
                "0123456\u048289abcdef",
                "01234567\u04829abcdef",
                "012345678\u0482abcdef",
                "0123456789\u0482bcdef",
                "0123456789a\u0482cdef",
                "0123456789ab\u0482def",
                "0123456789abc\u0482ef",
                "0123456789abcd\u0482f",
                "0123456789abcde\u0482",
                // Category 9, ASCII strings:
                //     "\u0489" + "0123456789abcdef".substring(1, index + 1)
                "\u0489",
                "\u04891",
                "\u048912",
                "\u0489123",
                "\u04891234",
                "\u048912345",
                "\u0489123456",
                "\u04891234567",
                "\u048912345678",
                "\u0489123456789",
                "\u0489123456789a",
                "\u0489123456789ab",
                "\u0489123456789abc",
                "\u0489123456789abcd",
                "\u0489123456789abcde",
                "\u0489123456789abcdef",
        };
        int length = strings.length;
        Assert.assertEquals(1 + 16 * 10, length);
        for (int i = 0; i != length; ++i) {
            String lhs = strings[i];
            for (int j = 0; j != length; ++j) {
                String rhs = strings[j];
                int result = $noinline$compareTo(lhs, rhs);
                final int expected;
                if (i == 0 || j == 0 || i == j) {
                    // One of the strings is empty or the strings are the same.
                    expected = lhs.length() - rhs.length();
                } else {
                    int i_category = (i - 1) / 16;
                    int i_index = (i - 1) % 16;
                    int j_category = (j - 1) / 16;
                    int j_index = (j - 1) % 16;
                    int min_ij_index = (i_index < j_index) ? i_index : j_index;
                    if (i_category == j_category) {
                        switch (i_category) {
                            case 0: case 3: case 6: case 9:
                                // Differs in length.
                                expected = lhs.length() - rhs.length();
                                break;
                            case 1: case 2: case 4: case 5: case 7: case 8:
                                // Differs in charAt(min_ij_index).
                                expected = lhs.charAt(min_ij_index) - rhs.charAt(min_ij_index);
                                break;
                            default: throw new Error("Unexpected category.");
                      }
                    } else if (i_category == 3 || i_category == 6 || i_category == 9 ||
                               j_category == 3 || j_category == 6 || j_category == 9) {
                        // In these categories, charAt(0) differs from other categories' strings.
                        expected = lhs.charAt(0) - rhs.charAt(0);
                    } else if (// Category 0 string is a prefix to any longer string in
                               // remaining categories.
                               (i_category == 0 && i_index < j_index) ||
                               (j_category == 0 && j_index < i_index) ||
                               // Category 2 string is a prefix to category 3 string at the same
                               // index. Similar for categories 4 and 5 and also 7 and 8.
                               // This includes matching last strings of these pairs of categories.
                               (i_index == j_index &&
                                   ((i_category == 1 && j_category == 2) ||
                                    (i_category == 2 && j_category == 1) ||
                                    (i_category == 4 && j_category == 5) ||
                                    (i_category == 5 && j_category == 4) ||
                                    (i_category == 7 && j_category == 8) ||
                                    (i_category == 8 && j_category == 7)))) {
                        // Differs in length.
                        expected = lhs.length() - rhs.length();
                    } else {
                        // The remaining cases differ in charAt(min_ij_index), the characters
                        // before that are "0123456789abcdef".substring(0, min_ij_index).
                        for (int k = 0; k < min_ij_index; ++k) {
                          Assert.assertEquals("0123456789abcdef".charAt(k), lhs.charAt(k));
                          Assert.assertEquals("0123456789abcdef".charAt(k), rhs.charAt(k));
                        }
                        expected = lhs.charAt(min_ij_index) - rhs.charAt(min_ij_index);
                        Assert.assertFalse(expected == 0);
                    }
                }
                if (expected != result) {
                  throw new Error(
                      "Mismatch at i=" + i + ", j=" + j + ", expected=" + expected +
                      ", result=" + result);
                }
                boolean equalsExpected =
                    (i == j) ||
                    // Last string in categories 1 and 2.
                    (i == 32 && j == 48) || (i == 48 && j == 32) ||
                    // Last string in categories 4 and 5.
                    (i == 80 && j == 96) || (i == 96 && j == 80) ||
                    // Last string in categories 7 and 8.
                    (i == 128 && j == 144) || (i == 144 && j == 128);
                Assert.assertEquals(equalsExpected, $noinline$equals(lhs, rhs));
            }
        }

        try {
            $noinline$compareTo("", null);
            Assert.fail();
        } catch (NullPointerException expected) {
        }
        try {
            $noinline$compareTo(null, "");
            Assert.fail();
        } catch (NullPointerException expected) {
        }

        Assert.assertFalse($noinline$equals("", null));
        try {
            $noinline$equals(null, "");
            Assert.fail();
        } catch (NullPointerException expected) {
        }
    }

    public static void testIndexOf() {
        String[] prefixes = {
                "",
                "0",
                "01",
                "012",
                "0123",
                "01234",
                "012345",
                "0123456",
                "01234567",
                "012345678",
                "0123456789",
                "0123456789a",
                "0123456789ab",
                "0123456789abc",
                "0123456789abcd",
                "0123456789abcdef",
        };
        String[] cores = {
                "",
                "x",
                "xx",
                "xxx",
                "xxxx",
                "xxxxx",
                "xxxxxx",
                "xxxxxxx",
                "xxxxxxxx",
                "xzx",
                "xxzx",
                "xxxzx",
                "xxxxzx",
                "xxxxxzx",
                "xxxxxxzx",
                "xxxxxxxzx",
                "xxxxxxxxzx",
                "\u0440",
                "\u0440\u0440",
                "\u0440\u0440\u0440",
                "\u0440\u0440\u0440\u0440",
                "\u0440\u0440\u0440\u0440\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440\u0440\u0440",
                "\u0440z\u0440",
                "\u0440\u0440z\u0440",
                "\u0440\u0440\u0440z\u0440",
                "\u0440\u0440\u0440\u0440z\u0440",
                "\u0440\u0440\u0440\u0440\u0440z\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440z\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440\u0440z\u0440",
                "\u0440\u0440\u0440\u0440\u0440\u0440\u0440\u0440z\u0440",
                "\u0000",
                "\u0000\u0000",
                "\u0000\u0000\u0000",
                "\u0000\u0000\u0000\u0000",
                "\u0000\u0000\u0000\u0000\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000",
                "\u0000z\u0000",
                "\u0000\u0000z\u0000",
                "\u0000\u0000\u0000z\u0000",
                "\u0000\u0000\u0000\u0000z\u0000",
                "\u0000\u0000\u0000\u0000\u0000z\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000z\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000\u0000z\u0000",
                "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000z\u0000",
        };
        String[] suffixes = {
                "",
                "y",
                "yy",
                "yyy",
                "yyyy",
                "yyyyy",
                "yyyyyy",
                "yyyyyyy",
                "yyyyyyyy",
                "\u0441",
                "y\u0441",
                "yy\u0441",
                "yyy\u0441",
                "yyyy\u0441",
                "yyyyy\u0441",
                "yyyyyy\u0441",
                "yyyyyyy\u0441",
                "yyyyyyyy\u0441",
        };
        for (String p : prefixes) {
            for (String c : cores) {
                for (String s : suffixes) {
                    String full = p + c + s;
                    int expX = (c.isEmpty() || c.charAt(0) != 'x') ? -1 : p.length();
                    int exp0440 = (c.isEmpty() || c.charAt(0) != '\u0440') ? -1 : p.length();
                    int exp0000 = (c.isEmpty() || c.charAt(0) != '\u0000') ? -1 : p.length();
                    Assert.assertEquals(expX, $noinline$indexOf(full, 'x'));
                    Assert.assertEquals(exp0440, $noinline$indexOf(full, '\u0440'));
                    Assert.assertEquals(exp0000, $noinline$indexOf(full, '\u0000'));
                    Assert.assertEquals(expX, $noinline$indexOf(full, 'x', -1));
                    Assert.assertEquals(exp0440, $noinline$indexOf(full, '\u0440', -1));
                    Assert.assertEquals(exp0000, $noinline$indexOf(full, '\u0000', -1));
                    Assert.assertEquals(-1, $noinline$indexOf(full, 'x', full.length() + 1));
                    Assert.assertEquals(-1, $noinline$indexOf(full, '\u0440', full.length() + 1));
                    Assert.assertEquals(-1, $noinline$indexOf(full, '\u0000', full.length() + 1));
                    for (int from = 0; from != full.length(); ++from) {
                        final int eX;
                        final int e0440;
                        final int e0000;
                        if (from <= p.length()) {
                            eX = expX;
                            e0440 = exp0440;
                            e0000 = exp0000;
                        } else if (from >= p.length() + c.length()) {
                            eX = -1;
                            e0440 = -1;
                            e0000 = -1;
                        } else if (full.charAt(from) == 'z') {
                            eX = (full.charAt(from + 1) != 'x') ? -1 : from + 1;
                            e0440 = (full.charAt(from + 1) != '\u0440') ? -1 : from + 1;
                            e0000 = (full.charAt(from + 1) != '\u0000') ? -1 : from + 1;
                        } else {
                            eX = (full.charAt(from) != 'x') ? -1 : from;
                            e0440 = (full.charAt(from) != '\u0440') ? -1 : from;
                            e0000 = (full.charAt(from) != '\u0000') ? -1 : from;
                        }
                        Assert.assertEquals(eX, $noinline$indexOf(full, 'x', from));
                        Assert.assertEquals(e0440, $noinline$indexOf(full, '\u0440', from));
                        Assert.assertEquals(e0000, $noinline$indexOf(full, '\u0000', from));
                    }
                }
            }
        }
    }

    public static void testEqualsConstString() {
        Assert.assertTrue($noinline$equalsConstString0(""));
        Assert.assertFalse($noinline$equalsConstString0("1"));

        Assert.assertTrue($noinline$equalsConstString3("012"));
        Assert.assertFalse($noinline$equalsConstString3("01"));
        Assert.assertFalse($noinline$equalsConstString3("0123"));
        Assert.assertFalse($noinline$equalsConstString3("01x"));
        Assert.assertFalse($noinline$equalsConstString3("01\u0440"));

        Assert.assertTrue($noinline$equalsConstString7("0123456"));
        Assert.assertFalse($noinline$equalsConstString7("012345"));
        Assert.assertFalse($noinline$equalsConstString7("01234567"));
        Assert.assertFalse($noinline$equalsConstString7("012345x"));
        Assert.assertFalse($noinline$equalsConstString7("012345\u0440"));

        Assert.assertTrue($noinline$equalsConstString12("012345678901"));
        Assert.assertFalse($noinline$equalsConstString12("01234567890"));
        Assert.assertFalse($noinline$equalsConstString12("0123456789012"));
        Assert.assertFalse($noinline$equalsConstString12("01234567890x"));
        Assert.assertFalse($noinline$equalsConstString12("01234567890\u0440"));

        Assert.assertTrue($noinline$equalsConstString14("01234567890123"));
        Assert.assertFalse($noinline$equalsConstString14("0123456789012"));
        Assert.assertFalse($noinline$equalsConstString14("012345678901234"));
        Assert.assertFalse($noinline$equalsConstString14("0123456789012x"));
        Assert.assertFalse($noinline$equalsConstString14("0123456789012\u0440"));

        Assert.assertTrue($noinline$equalsConstString24("012345678901234567890123"));
        Assert.assertFalse($noinline$equalsConstString24("01234567890123456789012"));
        Assert.assertFalse($noinline$equalsConstString24("0123456789012345678901234"));
        Assert.assertFalse($noinline$equalsConstString24("01234567890123456789012x"));
        Assert.assertFalse($noinline$equalsConstString24("01234567890123456789012\u0440"));

        Assert.assertTrue($noinline$equalsConstString29("01234567890123456789012345678"));
        Assert.assertFalse($noinline$equalsConstString29("0123456789012345678901234567"));
        Assert.assertFalse($noinline$equalsConstString29("012345678901234567890123456789"));
        Assert.assertFalse($noinline$equalsConstString29("0123456789012345678901234567x"));
        Assert.assertFalse($noinline$equalsConstString29("0123456789012345678901234567\u0440"));

        Assert.assertTrue($noinline$equalsConstString35("01234567890123456789012345678901234"));
        Assert.assertFalse($noinline$equalsConstString35("0123456789012345678901234567890123"));
        Assert.assertFalse($noinline$equalsConstString35("012345678901234567890123456789012345"));
        Assert.assertFalse($noinline$equalsConstString35("0123456789012345678901234567890123x"));
        Assert.assertFalse(
            $noinline$equalsConstString35("0123456789012345678901234567890123\u0440"));

        Assert.assertTrue($noinline$equalsConstNonAsciiString3("\u044012"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString3("\u04401"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString3("\u0440123"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString3("\u04401x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString3("012"));

        Assert.assertTrue($noinline$equalsConstNonAsciiString7("\u0440123456"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString7("\u044012345"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString7("\u04401234567"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString7("\u044012345x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString7("0123456"));

        Assert.assertTrue($noinline$equalsConstNonAsciiString12("\u044012345678901"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString12("\u04401234567890"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString12("\u0440123456789012"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString12("\u04401234567890x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString12("012345678901"));

        Assert.assertTrue($noinline$equalsConstNonAsciiString14("\u04401234567890123"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString14("\u0440123456789012"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString14("\u044012345678901234"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString14("\u0440123456789012x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString14("01234567890123"));

        Assert.assertTrue($noinline$equalsConstNonAsciiString24("\u044012345678901234567890123"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString24("\u04401234567890123456789012"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString24("\u0440123456789012345678901234"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString24("\u04401234567890123456789012x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString24("\012345678901234567890123"));

        Assert.assertTrue(
            $noinline$equalsConstNonAsciiString29("\u04401234567890123456789012345678"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString29("\u0440123456789012345678901234567"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString29("\u044012345678901234567890123456789"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString29("\u0440123456789012345678901234567x"));
        Assert.assertFalse($noinline$equalsConstNonAsciiString29("01234567890123456789012345678"));

        Assert.assertTrue(
            $noinline$equalsConstNonAsciiString35("\u04401234567890123456789012345678901234"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString35("\u0440123456789012345678901234567890123"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString35("\u044012345678901234567890123456789012345"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString35("\u0440123456789012345678901234567890123x"));
        Assert.assertFalse(
            $noinline$equalsConstNonAsciiString35("01234567890123456789012345678901234"));
    }

    public static void testConstStringEquals() {
        Assert.assertTrue($noinline$constString0Equals(""));
        Assert.assertFalse($noinline$constString0Equals("1"));

        Assert.assertTrue($noinline$constString3Equals("012"));
        Assert.assertFalse($noinline$constString3Equals("01"));
        Assert.assertFalse($noinline$constString3Equals("0123"));
        Assert.assertFalse($noinline$constString3Equals("01x"));
        Assert.assertFalse($noinline$constString3Equals("01\u0440"));

        Assert.assertTrue($noinline$constString7Equals("0123456"));
        Assert.assertFalse($noinline$constString7Equals("012345"));
        Assert.assertFalse($noinline$constString7Equals("01234567"));
        Assert.assertFalse($noinline$constString7Equals("012345x"));
        Assert.assertFalse($noinline$constString7Equals("012345\u0440"));

        Assert.assertTrue($noinline$constString12Equals("012345678901"));
        Assert.assertFalse($noinline$constString12Equals("01234567890"));
        Assert.assertFalse($noinline$constString12Equals("0123456789012"));
        Assert.assertFalse($noinline$constString12Equals("01234567890x"));
        Assert.assertFalse($noinline$constString12Equals("01234567890\u0440"));

        Assert.assertTrue($noinline$constString14Equals("01234567890123"));
        Assert.assertFalse($noinline$constString14Equals("0123456789012"));
        Assert.assertFalse($noinline$constString14Equals("012345678901234"));
        Assert.assertFalse($noinline$constString14Equals("0123456789012x"));
        Assert.assertFalse($noinline$constString14Equals("0123456789012\u0440"));

        Assert.assertTrue($noinline$constString24Equals("012345678901234567890123"));
        Assert.assertFalse($noinline$constString24Equals("01234567890123456789012"));
        Assert.assertFalse($noinline$constString24Equals("0123456789012345678901234"));
        Assert.assertFalse($noinline$constString24Equals("01234567890123456789012x"));
        Assert.assertFalse($noinline$constString24Equals("01234567890123456789012\u0440"));

        Assert.assertTrue($noinline$constString29Equals("01234567890123456789012345678"));
        Assert.assertFalse($noinline$constString29Equals("0123456789012345678901234567"));
        Assert.assertFalse($noinline$constString29Equals("012345678901234567890123456789"));
        Assert.assertFalse($noinline$constString29Equals("0123456789012345678901234567x"));
        Assert.assertFalse($noinline$constString29Equals("0123456789012345678901234567\u0440"));

        Assert.assertTrue($noinline$constString35Equals("01234567890123456789012345678901234"));
        Assert.assertFalse($noinline$constString35Equals("0123456789012345678901234567890123"));
        Assert.assertFalse($noinline$constString35Equals("012345678901234567890123456789012345"));
        Assert.assertFalse($noinline$constString35Equals("0123456789012345678901234567890123x"));
        Assert.assertFalse(
            $noinline$constString35Equals("0123456789012345678901234567890123\u0040"));

        Assert.assertTrue($noinline$constNonAsciiString3Equals("\u044012"));
        Assert.assertFalse($noinline$constNonAsciiString3Equals("\u04401"));
        Assert.assertFalse($noinline$constNonAsciiString3Equals("\u0440123"));
        Assert.assertFalse($noinline$constNonAsciiString3Equals("\u04401x"));
        Assert.assertFalse($noinline$constNonAsciiString3Equals("0123456"));

        Assert.assertTrue($noinline$constNonAsciiString7Equals("\u0440123456"));
        Assert.assertFalse($noinline$constNonAsciiString7Equals("\u044012345"));
        Assert.assertFalse($noinline$constNonAsciiString7Equals("\u04401234567"));
        Assert.assertFalse($noinline$constNonAsciiString7Equals("\u044012345x"));
        Assert.assertFalse($noinline$constNonAsciiString7Equals("0123456"));

        Assert.assertTrue($noinline$constNonAsciiString12Equals("\u044012345678901"));
        Assert.assertFalse($noinline$constNonAsciiString12Equals("\u04401234567890"));
        Assert.assertFalse($noinline$constNonAsciiString12Equals("\u0440123456789012"));
        Assert.assertFalse($noinline$constNonAsciiString12Equals("\u04401234567890x"));
        Assert.assertFalse($noinline$constNonAsciiString12Equals("012345678901"));

        Assert.assertTrue($noinline$constNonAsciiString14Equals("\u04401234567890123"));
        Assert.assertFalse($noinline$constNonAsciiString14Equals("\u0440123456789012"));
        Assert.assertFalse($noinline$constNonAsciiString14Equals("\u044012345678901234"));
        Assert.assertFalse($noinline$constNonAsciiString14Equals("\u0440123456789012x"));
        Assert.assertFalse($noinline$constNonAsciiString14Equals("01234567890123"));

        Assert.assertTrue($noinline$constNonAsciiString24Equals("\u044012345678901234567890123"));
        Assert.assertFalse($noinline$constNonAsciiString24Equals("\u04401234567890123456789012"));
        Assert.assertFalse($noinline$constNonAsciiString24Equals("\u0440123456789012345678901234"));
        Assert.assertFalse($noinline$constNonAsciiString24Equals("\u04401234567890123456789012x"));
        Assert.assertFalse($noinline$constNonAsciiString24Equals("\012345678901234567890123"));

        Assert.assertTrue(
            $noinline$constNonAsciiString29Equals("\u04401234567890123456789012345678"));
        Assert.assertFalse(
            $noinline$constNonAsciiString29Equals("\u0440123456789012345678901234567"));
        Assert.assertFalse(
            $noinline$constNonAsciiString29Equals("\u044012345678901234567890123456789"));
        Assert.assertFalse(
            $noinline$constNonAsciiString29Equals("\u0440123456789012345678901234567x"));
        Assert.assertFalse($noinline$constNonAsciiString29Equals("01234567890123456789012345678"));

        Assert.assertTrue(
            $noinline$constNonAsciiString35Equals("\u04401234567890123456789012345678901234"));
        Assert.assertFalse(
            $noinline$constNonAsciiString35Equals("\u0440123456789012345678901234567890123"));
        Assert.assertFalse(
            $noinline$constNonAsciiString35Equals("\u044012345678901234567890123456789012345"));
        Assert.assertFalse(
            $noinline$constNonAsciiString35Equals("\u0440123456789012345678901234567890123x"));
        Assert.assertFalse(
            $noinline$constNonAsciiString35Equals("01234567890123456789012345678901234"));

        // Regression test for incorrectly creating an uncompressed string when the
        // string should be compressed. Only the low 8 bits are relevant but the whole
        // `hibyte` was erroneously tested. Bug: 63661357
        Assert.assertTrue("A".equals(new String(new byte[] { (byte)'A' }, /* hibyte */ 0x100)));
    }

    public static boolean $noinline$equalsConstString0(String s) {
        return s.equals("");
    }

    public static boolean $noinline$equalsConstString3(String s) {
        return s.equals("012");
    }

    public static boolean $noinline$equalsConstString7(String s) {
        return s.equals("0123456");
    }

    public static boolean $noinline$equalsConstString12(String s) {
        return s.equals("012345678901");
    }

    public static boolean $noinline$equalsConstString14(String s) {
        return s.equals("01234567890123");
    }

    public static boolean $noinline$equalsConstString24(String s) {
        return s.equals("012345678901234567890123");
    }

    public static boolean $noinline$equalsConstString29(String s) {
        return s.equals("01234567890123456789012345678");
    }

    public static boolean $noinline$equalsConstString35(String s) {
        return s.equals("01234567890123456789012345678901234");
    }

    public static boolean $noinline$equalsConstNonAsciiString3(String s) {
        return s.equals("\u044012");
    }

    public static boolean $noinline$equalsConstNonAsciiString7(String s) {
        return s.equals("\u0440123456");
    }

    public static boolean $noinline$equalsConstNonAsciiString12(String s) {
        return s.equals("\u044012345678901");
    }

    public static boolean $noinline$equalsConstNonAsciiString14(String s) {
        return s.equals("\u04401234567890123");
    }

    public static boolean $noinline$equalsConstNonAsciiString24(String s) {
        return s.equals("\u044012345678901234567890123");
    }

    public static boolean $noinline$equalsConstNonAsciiString29(String s) {
        return s.equals("\u04401234567890123456789012345678");
    }

    public static boolean $noinline$equalsConstNonAsciiString35(String s) {
        return s.equals("\u04401234567890123456789012345678901234");
    }

    public static boolean $noinline$constString0Equals(String s) {
        return s.equals("");
    }

    public static boolean $noinline$constString3Equals(String s) {
        return "012".equals(s);
    }

    public static boolean $noinline$constString7Equals(String s) {
        return "0123456".equals(s);
    }

    public static boolean $noinline$constString12Equals(String s) {
        return "012345678901".equals(s);
    }

    public static boolean $noinline$constString14Equals(String s) {
        return "01234567890123".equals(s);
    }

    public static boolean $noinline$constString24Equals(String s) {
        return "012345678901234567890123".equals(s);
    }

    public static boolean $noinline$constString29Equals(String s) {
        return "01234567890123456789012345678".equals(s);
    }

    public static boolean $noinline$constString35Equals(String s) {
        return "01234567890123456789012345678901234".equals(s);
    }

    public static boolean $noinline$constNonAsciiString3Equals(String s) {
        return "\u044012".equals(s);
    }

    public static boolean $noinline$constNonAsciiString7Equals(String s) {
        return "\u0440123456".equals(s);
    }

    public static boolean $noinline$constNonAsciiString12Equals(String s) {
        return "\u044012345678901".equals(s);
    }

    public static boolean $noinline$constNonAsciiString14Equals(String s) {
        return "\u04401234567890123".equals(s);
    }

    public static boolean $noinline$constNonAsciiString24Equals(String s) {
        return "\u044012345678901234567890123".equals(s);
    }

    public static boolean $noinline$constNonAsciiString29Equals(String s) {
        return "\u04401234567890123456789012345678".equals(s);
    }

    public static boolean $noinline$constNonAsciiString35Equals(String s) {
        return "\u04401234567890123456789012345678901234".equals(s);
    }

    public static int $noinline$compareTo(String lhs, String rhs) {
        return lhs.compareTo(rhs);
    }

    public static boolean $noinline$equals(String lhs, String rhs) {
        return lhs.equals(rhs);
    }

    public static int $noinline$indexOf(String lhs, int ch) {
        return lhs.indexOf(ch);
    }

    public static int $noinline$indexOf(String lhs, int ch, int fromIndex) {
        return lhs.indexOf(ch, fromIndex);
    }
}
