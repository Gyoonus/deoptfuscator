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

    static int imax = Integer.MAX_VALUE;
    static int imin = Integer.MIN_VALUE;
    static long lmax = Long.MAX_VALUE;
    static long lmin = Long.MIN_VALUE;
    static CA ca;

    public static void expectEquals(long expected, long result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    public static void expectEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    public static void test_int() {
        int result = 0;
        int a = imax;
        int b = imin;
        int c = 10;
        int d = c;
        int tmp = 0;
        int [] ia = new int[5];
        for (int i = 0; i < 100; i++) {
            tmp = i*c;
            result += i*i;
            result = i - tmp;
        }
        expectEquals(result, -891);

        result = c*c + (result - c);
        expectEquals(result, -801);

        result = a + a*a;
        expectEquals(result, -2147483648);

        result = b + b*b;
        expectEquals(result, -2147483648);

        result = b - a*a;
        expectEquals(result, 2147483647);

        result = d*d;
        d++;
        result += result;
        expectEquals(result, 200);

        result = c*c;
        tmp++;
        result += result;
        expectEquals(result, 200);

        result = 0;
        try {
            result = c*c;
            ia[c] = d;  // array out of bound.
            result += d;
        } catch (Exception e) {
        }
        expectEquals(result, 100);

        CA obj = new CA();
        result = a*c + obj.ia;
        expectEquals(result, 2);

        result = 0;
        obj = ca;
        try {
            result = a*c;
            tmp = obj.ia;
            result = result + tmp;
        } catch (Exception e) {
        }
        expectEquals(result, -10);
    }

    public static void test_long() {
        long result = 0;
        long a = lmax;
        long b = lmin;
        long c = 10;
        long d = c;
        long tmp = 0;
        int [] ia = new int[5];
        for (long i = 0; i < 100; i++) {
            tmp = i*c;
            result += i*i;
            result = i - tmp;
        }
        expectEquals(result, -891L);

        result = c*c + (result - c);
        expectEquals(result, -801L);

        result = a + a*a;
        expectEquals(result, -9223372036854775808L);

        result = b + b*b;
        expectEquals(result, -9223372036854775808L);

        result = b - a*a;
        expectEquals(result, 9223372036854775807L);

        result = d*d;
        d++;
        result += result;
        expectEquals(result, 200L);

        result = c*c;
        tmp++;
        result += result;
        expectEquals(result, 200L);

        result = 0;
        int index = 10;
        try {
            result = c*c;
            ia[index] = 10;  // array out of bound.
            result += d;
        } catch (Exception e) {
        }
        expectEquals(result, 100L);

        CA obj = new CA();
        result = a*c + obj.la;
        expectEquals(result, 113L);

        result = 0;
        obj = ca;
        try {
            result = a*c;
            tmp = obj.la;
            result = result + tmp;
        } catch (Exception e) {
        }
        expectEquals(result, -10L);
    }

    public static void main(String[] args) {
        test_int();
        test_long();
        System.out.println("Done!");
    }

}

class CA {
    public int ia = 12;
    public long la = 123L;
}
