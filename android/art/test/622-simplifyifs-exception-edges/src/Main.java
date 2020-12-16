/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
    public static void main(String[] args) throws Exception {
        Class<?> c = Class.forName("Test");
        Method test = c.getDeclaredMethod("test", int[].class);
        assertIntEquals(-2, (int)test.invoke(null, new Object[] { null }));
        assertIntEquals(-1, (int)test.invoke(null, new Object[] { new int[0] }));
        assertIntEquals(42, (int)test.invoke(null, new Object[] { new int[] { 42 } }));

        Method test2 = c.getDeclaredMethod("test2", int[].class, int.class);
        assertIntEquals(-2, (int)test2.invoke(null, new Object[] { null, 0 }));
        assertIntEquals(-1, (int)test2.invoke(null, new Object[] { new int[0], 0 }));
        assertIntEquals(-1, (int)test2.invoke(null, new Object[] { new int[0], 1 }));
        assertIntEquals(3, (int)test2.invoke(null, new Object[] { new int[] { 42 }, 0 }));
    }

    public static void assertIntEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }

    // Workaround for non-zero field ids offset in dex file with no fields. Bug: 18051191
    static final boolean dummy = false;
}
