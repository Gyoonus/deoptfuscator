/*
 * Copyright (C) 2017 The Android Open Source Project
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
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class TestLinkerMethodMinimalArguments extends TestBase {
    private static int forceFailureType = 0;

    static final int FAILURE_TYPE_NONE = 0;
    static final int FAILURE_TYPE_LINKER_METHOD_RETURNS_NULL = 1;
    static final int FAILURE_TYPE_LINKER_METHOD_THROWS = 2;
    static final int FAILURE_TYPE_TARGET_METHOD_THROWS = 3;

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestLinkerMethodMinimalArguments.class,
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
                    name = "linkerMethod"
                ),
        fieldOrMethodName = "_add",
        returnType = int.class,
        parameterTypes = {int.class, int.class}
    )
    private static int add(int a, int b) {
        assertNotReached();
        return -1;
    }

    @SuppressWarnings("unused")
    static int _add(int a, int b) {
        if (forceFailureType == FAILURE_TYPE_TARGET_METHOD_THROWS) {
            System.out.println("Throwing ArithmeticException in add()");
            throw new ArithmeticException("add");
        }
        return a + b;
    }

    @SuppressWarnings("unused")
    private static CallSite linkerMethod(
            MethodHandles.Lookup caller, String name, MethodType methodType) throws Throwable {
        System.out.println("linkerMethod failure type " + forceFailureType);
        MethodHandle mh_add =
                caller.findStatic(TestLinkerMethodMinimalArguments.class, name, methodType);
        switch (forceFailureType) {
            case FAILURE_TYPE_LINKER_METHOD_RETURNS_NULL:
                System.out.println(
                        "Returning null instead of CallSite for " + name + " " + methodType);
                return null;
            case FAILURE_TYPE_LINKER_METHOD_THROWS:
                System.out.println("Throwing InstantiationException in linkerMethod()");
                throw new InstantiationException("linkerMethod");
            default:
                return new ConstantCallSite(mh_add);
        }
    }

    public static void test(int failureType, int x, int y) throws Throwable {
        assertTrue(failureType >= FAILURE_TYPE_NONE);
        assertTrue(failureType <= FAILURE_TYPE_TARGET_METHOD_THROWS);
        forceFailureType = failureType;
        assertEquals(x + y, add(x, y));
        System.out.println("Failure Type + " + failureType + " (" + x + y + ")");
    }
}
