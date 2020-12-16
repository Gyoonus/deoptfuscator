/*
 * Copyright (C) 2011 The Android Open Source Project
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

class GetMethodSignature {
    Float m1(int a, double b, long c, Object d) { return null; }
    GetMethodSignature m2(boolean x, short y, char z) { return null; }
    void m3() { }
    void m4(int i) { }
    void m5(int i, int j) { }
    void m6(int i, int j, int[][] array1) { }
    void m7(int i, int j, int[][] array1, Object o) { }
    void m8(int i, int j, int[][] array1, Object o, Object[][] array2) { }
    int m9() { return 0; }
    int[][] mA() { return null; }
    Object[][] mB() { return null; }
}
