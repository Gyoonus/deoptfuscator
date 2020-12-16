/*
 * Copyright (C) 2013 The Android Open Source Project
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
    static final long NUM_ITERATIONS = 50000;
    static volatile double negInfinity = Double.NEGATIVE_INFINITY;

    public static void main(String args[]) {
        test1();
        test2();
        test3();
    }

    public static void test1() {

        long sumInf = 0;
        long sumRes = 0;

        for (long i = 0 ; i < NUM_ITERATIONS ; i++) {
            //Every second iteration, sumInf becomes 0
            sumInf += (long) negInfinity;

            //Some extra work for compilers to make this
            //loop seem important
            if (sumInf == Long.MIN_VALUE) {
                sumRes++;
            }
        }

        if (sumRes == NUM_ITERATIONS / 2) {
            System.out.println("Iteration Result is as expected");
        } else {
            System.out.println("Conversions failed over " + NUM_ITERATIONS + " iterations");
        }
    }

    public static void test2() {
        long a = 1L;
        long b = 2L;

        float inter3 = a;
        float inter4 = b;
        System.out.println("inter4:" + inter4);
    }

    public static void test3() {
        double d = Long.MAX_VALUE;
        System.out.println("max_long:" + (long)d);
    }

}
