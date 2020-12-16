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

    static double dPi = Math.PI;
    static float  fPi = (float)Math.PI;

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

    public static void divDoubleTest() {
        double d1 = 0x1.0p1023;
        double d2 = -2.0;
        double d3 = 0.0;
        double d4 = Double.MIN_NORMAL;
        double d5 = Double.POSITIVE_INFINITY;
        double d6 = Double.NEGATIVE_INFINITY;
        double d7 = -0.0;
        double d8 = Double.MAX_VALUE;
        double d9 = Double.MIN_VALUE;
        double dNaN = Double.NaN;

        expectEquals(Double.doubleToRawLongBits(dPi/d1), 0x1921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(dPi/d2), 0xbff921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(dPi/d3), 0x7ff0000000000000L);
        expectEquals(Double.doubleToRawLongBits(dPi/d4), 0x7fe921fb54442d18L);
        expectEquals(Double.doubleToRawLongBits(dPi/d5), 0x0L);
        expectEquals(Double.doubleToRawLongBits(dPi/d6), 0x8000000000000000L);
        expectEquals(Double.doubleToRawLongBits(dPi/d7), 0xfff0000000000000L);

        expectEquals(Double.doubleToRawLongBits(dPi/d8), 0xc90fdaa22168cL);
        expectEquals(Double.doubleToRawLongBits(dPi/d9), 0x7ff0000000000000L);

        // Not-a-number computation. Use doubleToLongBits to get canonical NaN. The literal value
        // is the canonical NaN (see Double.doubleToLongBits).
        expectEquals(Double.doubleToLongBits(dPi/dNaN), 0x7ff8000000000000L);
    }

    public static void divFloatTest() {
        float f1 = 0x1.0p127f;
        float f2 = -2.0f;
        float f3 = 0.0f;
        float f4 = Float.MIN_NORMAL;
        float f5 = Float.POSITIVE_INFINITY;
        float f6 = Float.NEGATIVE_INFINITY;
        float f7 = -0.0f;
        float f8 = Float.MAX_VALUE;
        float f9 = Float.MIN_VALUE;
        float fNaN = Float.NaN;

        expectEquals(Float.floatToRawIntBits(fPi/f1), 0xc90fdb);
        expectEquals(Float.floatToRawIntBits(fPi/f2), 0xbfc90fdb);
        expectEquals(Float.floatToRawIntBits(fPi/f3), 0x7f800000);
        expectEquals(Float.floatToRawIntBits(fPi/f4), 0x7f490fdb);
        expectEquals(Float.floatToRawIntBits(fPi/f5), 0x0);
        expectEquals(Float.floatToRawIntBits(fPi/f6), 0x80000000);
        expectEquals(Float.floatToRawIntBits(fPi/f7), 0xff800000);

        expectEquals(Float.floatToRawIntBits(fPi/f8), 0x6487ee);
        expectEquals(Float.floatToRawIntBits(fPi/f9), 0x7f800000);

        // Not-a-number computation. Use floatToIntBits to get canonical NaN. The literal value
        // is the canonical NaN (see Float.floatToIntBits).
        expectEquals(Float.floatToIntBits(fPi/fNaN), 0x7fc00000);
    }

    public static void main(String[] args) {
        divDoubleTest();
        divFloatTest();
        System.out.println("Done!");
    }

}
