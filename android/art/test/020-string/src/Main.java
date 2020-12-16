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

import java.nio.charset.Charset;
import java.io.UnsupportedEncodingException;

/**
 * Simple string test.
 */
public class Main {
    public static void main(String args[]) {
        basicTest();
        indexTest();
        constructorTest();
        copyTest();
    }

    public static void basicTest() {
        String baseStr = "*** This is a very nice string!!!";
        String testStr;
        int i;

        testStr = baseStr.substring(4, baseStr.length() - 3);
        System.out.println("testStr is '" + testStr + "'");

        /* sloppy for loop */
        for (i = 0; i < testStr.length(); i++)
            System.out.print(testStr.charAt(i));
        System.out.print("\n");

        String testStr2 = "This is a very nice strinG";
        if (testStr.length() != testStr2.length())
            System.out.println("WARNING: stringTest length mismatch");

        int compareResult = testStr.compareTo(testStr2);
        if (compareResult > 0) {
          System.out.println("Compare result is greater than zero");
        } else if (compareResult == 0) {
          System.out.println("Compare result is equal to zero");
        } else {
          System.out.println("Compare result is less than zero");
        }

        // expected: -65302
        String s1 = "\u0c6d\u0cb6\u0d00\u0000\u0080\u0080\u0080\u0000\u0002\u0002\u0002\u0000\u00e9\u00e9\u00e9";
        String s2 = "\u0c6d\u0cb6\u0d00\u0000\u0080\u0080\u0080\u0000\u0002\u0002\u0002\u0000\uffff\uffff\uffff\u00e9\u00e9\u00e9";
        System.out.println("Compare unicode: " + s1.compareTo(s2));

        try {
            testStr.charAt(500);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }
    }

    public static void indexTest() {
        String baseStr = "The quick brown fox jumps over the lazy dog!";
        String subStr;

        subStr = baseStr.substring(5, baseStr.length() - 4);
        System.out.println("subStr is '" + subStr + "'");

        System.out.println("Indexes are: " +
            baseStr.indexOf('T') + ":" +
            subStr.indexOf('T') + ":" +
            subStr.indexOf('u') + ":" +
            baseStr.indexOf('!') + ":" +
            subStr.indexOf('y') + ":" +
            subStr.indexOf('d') + ":" +
            baseStr.indexOf('x') + ":" +
            subStr.indexOf('x', 0) + ":" +
            subStr.indexOf('x', -1) + ":" +
            subStr.indexOf('x', 200) + ":" +
            baseStr.indexOf('x', 17) + ":" +
            baseStr.indexOf('x', 18) + ":" +
            baseStr.indexOf('x', 19) + ":" +
            subStr.indexOf('x', 13) + ":" +
            subStr.indexOf('x', 14) + ":" +
            subStr.indexOf('&') + ":" +
            baseStr.indexOf(0x12341234));
    }

    public static void constructorTest() {
        byte[] byteArray = "byteArray".getBytes();
        char[] charArray = new char[] { 'c', 'h', 'a', 'r', 'A', 'r', 'r', 'a', 'y' };
        String charsetName = "US-ASCII";
        Charset charset = Charset.forName("UTF-8");
        String string = "string";
        StringBuffer stringBuffer = new StringBuffer("stringBuffer");
        int [] codePoints = new int[] { 65, 66, 67, 68, 69 };
        StringBuilder stringBuilder = new StringBuilder("stringBuilder");

        String s1 = new String();
        String s2 = new String(byteArray);
        String s3 = new String(byteArray, 1);
        String s4 = new String(byteArray, 0, 4);
        String s5 = new String(byteArray, 2, 4, 5);

        try {
            String s6 = new String(byteArray, 2, 4, charsetName);
            String s7 = new String(byteArray, charsetName);
        } catch (UnsupportedEncodingException e) {
            System.out.println("Got unexpected UnsupportedEncodingException");
        }
        String s8 = new String(byteArray, 3, 3, charset);
        String s9 = new String(byteArray, charset);
        String s10 = new String(charArray);
        String s11 = new String(charArray, 0, 4);
        String s12 = new String(string);
        String s13 = new String(stringBuffer);
        String s14 = new String(codePoints, 1, 3);
        String s15 = new String(stringBuilder);
    }

    public static void copyTest() {
        String src = new String("Hello Android");
        char[] dst = new char[7];
        char[] tmp = null;

        try {
            src.getChars(2, 9, tmp, 0);
            System.out.println("GLITCH: expected exception");
        } catch (NullPointerException npe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(-1, 9, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 19, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 1, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (StringIndexOutOfBoundsException sioobe) {
            System.out.println("Got expected exception");
        }

        try {
            src.getChars(2, 10, dst, 0);
            System.out.println("GLITCH: expected exception");
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            System.out.println("Got expected exception");
        }

        src.getChars(2, 9, dst, 0);
        System.out.println(new String(dst));
    }
}
