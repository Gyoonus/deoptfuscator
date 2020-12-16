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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import annotations.Constant;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class TestLinkerMethodMultipleArgumentTypes extends TestBase {

    private static int bootstrapRunCount = 0;

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestLinkerMethodMultipleArgumentTypes.class,
                    name = "linkerMethod",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        int.class,
                        int.class,
                        int.class,
                        int.class,
                        float.class,
                        double.class,
                        String.class,
                        Class.class,
                        long.class
                    }
                ),
        fieldOrMethodName = "_add",
        returnType = int.class,
        parameterTypes = {int.class, int.class},
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = -1),
            @Constant(intValue = 1),
            @Constant(intValue = (int) 'a'),
            @Constant(intValue = 1024),
            @Constant(intValue = 1),
            @Constant(floatValue = 11.1f),
            @Constant(doubleValue = 2.2),
            @Constant(stringValue = "Hello"),
            @Constant(classValue = TestLinkerMethodMultipleArgumentTypes.class),
            @Constant(longValue = 123456789L)
        }
    )
    private static int add(int a, int b) {
        assertNotReached();
        return -1;
    }

    @SuppressWarnings("unused")
    private static int _add(int a, int b) {
        return a + b;
    }

    @SuppressWarnings("unused")
    private static CallSite linkerMethod(
            MethodHandles.Lookup caller,
            String name,
            MethodType methodType,
            int v1,
            int v2,
            int v3,
            int v4,
            int v5,
            float v6,
            double v7,
            String v8,
            Class<?> v9,
            long v10)
            throws Throwable {
        System.out.println("Linking " + name + " " + methodType);
        assertEquals(-1, v1);
        assertEquals(1, v2);
        assertEquals('a', v3);
        assertEquals(1024, v4);
        assertEquals(1, v5);
        assertEquals(11.1f, v6);
        assertEquals(2.2, v7);
        assertEquals("Hello", v8);
        assertEquals(TestLinkerMethodMultipleArgumentTypes.class, v9);
        assertEquals(123456789L, v10);
        MethodHandle mh_add =
                caller.findStatic(TestLinkerMethodMultipleArgumentTypes.class, name, methodType);
        return new ConstantCallSite(mh_add);
    }

    public int GetBootstrapRunCount() {
        return bootstrapRunCount;
    }

    public static void test(int x, int y) throws Throwable {
        assertEquals(x + y, add(x, y));
        System.out.println(x + y);
    }
}
