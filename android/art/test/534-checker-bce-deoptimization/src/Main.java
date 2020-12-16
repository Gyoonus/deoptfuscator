/*
 * Copyright (C) 2015 The Android Open Source Project
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
    public static void main(String[] args) {
        new Main().run();
        testPreserveFloat();
        testPreserveDouble();
        System.out.println("finish");
    }

    public void run() {
        double a[][] = new double[200][201];
        double b[] = new double[200];
        int n = 100;

        foo1(a, n, b);
    }

    void foo1(double a[][], int n, double b[]) {
        double t;
        int i,k;

        for (i = 0; i < n; i++) {
            k = n - (i + 1);
            b[k] /= a[k][k];
            t = -b[k];
            foo2(k + 1000, t, b);
        }
    }

    void foo2(int n, double c, double b[]) {
        try {
            foo3(n, c, b);
        } catch (Exception e) {
        }
    }

    void foo3(int n, double c, double b[]) {
        int i = 0;
        for (i = 0; i < n; i++) {
            b[i + 1] += c * b[i + 1];
        }
    }

    /*
     * Test that we correctly preserve floating point registers when we deoptimize.
     *
     * Note: These tests rely on the deoptimization happening before the loop,
     * so that the loop is interpreted and fills the provided arrays. However,
     * the BCE transformation can be modified to execute the loop as many times
     * as the compiler can guarantee no AIOOBE and only deoptimize thereafter,
     * just before the throwing iteration. Then the floating point registers
     * would no longer be used after the deoptimization and another approach
     * would be needed to test this.
     */

    static public void testPreserveFloat() {
        float[] array = new float[2];
        try {
            $noinline$FloatFill(1.125f, 2.5f, array, 3);
            throw new Error();
        } catch (ArrayIndexOutOfBoundsException expected) {
            System.out.println("array[0]=" + array[0] + "f");
            System.out.println("array[1]=" + array[1] + "f");
        }
    }

    /// CHECK-START: void Main.$noinline$FloatFill(float, float, float[], int) BCE (after)
    /// CHECK-DAG:          Deoptimize
    /// CHECK-DAG:          Deoptimize
    /// CHECK-DAG:          Deoptimize
    /// CHECK-NOT:          Deoptimize

    /// CHECK-START: void Main.$noinline$FloatFill(float, float, float[], int) BCE (after)
    /// CHECK-NOT:          BoundsCheck

    public static void $noinline$FloatFill(float f1, float f2, float[] array, int n) {
        if (doThrow) { throw new Error(); }
        for (int i = 0; i < n; ++i) {
            array[i] = ((i & 1) == 1) ? f1 : f2;
            f1 += 1.5f;
            f2 += 2.25f;
        }
    }

    static public void testPreserveDouble() {
        double[] array = new double[2];
        try {
            $noinline$DoubleFill(2.125, 3.5, array, 3);
            throw new Error();
        } catch (ArrayIndexOutOfBoundsException expected) {
            System.out.println("array[0]=" + array[0]);
            System.out.println("array[1]=" + array[1]);
        }
    }

    /// CHECK-START: void Main.$noinline$DoubleFill(double, double, double[], int) BCE (after)
    /// CHECK-DAG:          Deoptimize
    /// CHECK-DAG:          Deoptimize
    /// CHECK-DAG:          Deoptimize
    /// CHECK-NOT:          Deoptimize

    /// CHECK-START: void Main.$noinline$DoubleFill(double, double, double[], int) BCE (after)
    /// CHECK-NOT:          BoundsCheck

    public static void $noinline$DoubleFill(double d1, double d2, double[] array, int n) {
        if (doThrow) { throw new Error(); }
        for (int i = 0; i < n; ++i) {
            array[i] = ((i & 1) == 1) ? d1 : d2;
            d1 += 1.5;
            d2 += 2.25;
        }
    }

    public static boolean doThrow = false;
}

