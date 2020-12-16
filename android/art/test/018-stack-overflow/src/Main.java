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

/**
 * generate a stack overflow condition and catch it
 */
public class Main {
    public static void main(String args[]) {
        testSelfRecursion();
        testMutualRecursion();
        System.out.println("SOE test done");
    }

    private static void testSelfRecursion() {
//        try {
//            stackOverflowTestSub0();
//        }
//        catch (StackOverflowError soe) {
//            System.out.println("caught SOE0 in testSelfRecursion");
//        }
        try {
            stackOverflowTestSub3(0.0, 1.0, 2.0);
        }
        catch (StackOverflowError soe) {
            System.out.println("caught SOE3 in testSelfRecursion");
        }
        try {
            stackOverflowTestSub10(0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
        }
        catch (StackOverflowError soe) {
            System.out.println("caught SOE10 in testSelfRecursion");
        }
    }

    private static void stackOverflowTestSub0() {
        stackOverflowTestSub0();
    }

    private static void stackOverflowTestSub3(double pad1, double pad2, double pad3) {
        stackOverflowTestSub3(pad1, pad2, pad3);
    }

    private static void stackOverflowTestSub10(double pad1, double pad2, double pad3, double pad4,
                                               double pad5, double pad6, double pad7, double pad8,
                                               double pad9, double pad10) {
        stackOverflowTestSub10(pad1, pad2, pad3, pad4, pad5, pad6, pad7, pad8, pad9, pad10);
    }

    private static void testMutualRecursion() {
        try {
            foo(0.0, 0.0, 0.0);
        }
        catch (StackOverflowError soe) {
            System.out.println("caught SOE in testMutualRecursion");
        }
    }

    private static void foo(double pad1, double pad2, double pad3) {
        bar(pad1, pad2, pad3);
    }

    private static void bar(double pad1, double pad2, double pad3) {
        baz(pad1, pad2, pad3);
    }

    private static void baz(double pad1, double pad2, double pad3) {
        qux(pad1, pad2, pad3);
    }

    private static void qux(double pad1, double pad2, double pad3) {
        foo(pad1, pad2, pad3);
    }
}
