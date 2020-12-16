/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.invoke.WrongMethodTypeException;

public final class Main {
    static class TestSetupError extends Error {
        TestSetupError(String message, Throwable cause) {
            super(message, cause);
        }
    }

    private static void failAssertion(String message) {
        StringBuilder sb = new StringBuilder();
        sb.append("Test failure: ");
        sb.append(message);
        throw new AssertionError(sb.toString());
    }

    private static void assertUnreachable() throws Throwable {
        failAssertion("Unreachable");
    }

    private static void failAssertEquals(Object expected, Object actual) {
        StringBuilder sb = new StringBuilder();
        sb.append(expected);
        sb.append(" != ");
        sb.append(actual);
        failAssertion(sb.toString());
    }

    private static void assertEquals(int expected, int actual) {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    private static void assertEquals(float expected, float actual) {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    private static void assertEquals(double expected, double actual) {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    static class FieldVarHandleExactInvokerTest {
        private static final VarHandle fieldVarHandle;
        int field;

        static {
            try {
                fieldVarHandle =
                        MethodHandles.lookup()
                                .findVarHandle(
                                        FieldVarHandleExactInvokerTest.class, "field", int.class);
            } catch (Exception e) {
                throw new TestSetupError("Failed to lookup of field", e);
            }
        }

        void run() throws Throwable {
            System.out.println("fieldVarHandleExactInvokerTest");

            MethodHandle invokerMethodHandle =
                    MethodHandles.varHandleExactInvoker(
                            VarHandle.AccessMode.GET_AND_SET,
                            MethodType.methodType(
                                    int.class, FieldVarHandleExactInvokerTest.class, int.class));

            field = 3;
            assertEquals(3, (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, 4));
            assertEquals(4, field);

            //
            // Check invocations with MethodHandle.invokeExact()
            //
            try {
                // Check for unboxing
                int i =
                        (int)
                                invokerMethodHandle.invokeExact(
                                        fieldVarHandle, this, Integer.valueOf(3));
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4, field);
            }
            try {
                // Check for widening conversion
                int i = (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, (short) 3);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4, field);
            }
            try {
                // Check for acceptance of void return type
                invokerMethodHandle.invokeExact(fieldVarHandle, this, 77);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4, field);
            }
            try {
                // Check for wider return type
                long l = (long) invokerMethodHandle.invokeExact(fieldVarHandle, this, 77);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4, field);
            }
            try {
                // Check null VarHandle instance fails
                VarHandle vhNull = null;
                int i = (int) invokerMethodHandle.invokeExact(vhNull, this, 777);
                assertUnreachable();
            } catch (NullPointerException expected) {
                assertEquals(4, field);
            }

            //
            // Check invocations with MethodHandle.invoke()
            //

            // Check for unboxing
            int i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, Integer.valueOf(3));
            assertEquals(3, field);

            // Check for unboxing
            i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, Short.valueOf((short) 4));
            assertEquals(4, field);

            // Check for widening conversion
            i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, (short) 23);
            assertEquals(23, field);

            // Check for acceptance of void return type
            invokerMethodHandle.invoke(fieldVarHandle, this, 77);
            assertEquals(77, field);

            // Check for wider return type
            long l = (long) invokerMethodHandle.invoke(fieldVarHandle, this, 88);
            assertEquals(88, field);

            try {
                // Check null VarHandle instance fails
                VarHandle vhNull = null;
                i = (int) invokerMethodHandle.invoke(vhNull, this, 888);
                assertUnreachable();
            } catch (NullPointerException expected) {
                assertEquals(88, field);
            }
        }
    }

    static class FieldVarHandleInvokerTest {
        private static final VarHandle fieldVarHandle;
        int field;

        static {
            try {
                fieldVarHandle =
                        MethodHandles.lookup()
                                .findVarHandle(FieldVarHandleInvokerTest.class, "field", int.class);
            } catch (Exception e) {
                throw new TestSetupError("Failed to lookup of field", e);
            }
        }

        void run() throws Throwable {
            System.out.println("fieldVarHandleInvokerTest");
            MethodHandle invokerMethodHandle =
                    MethodHandles.varHandleInvoker(
                            VarHandle.AccessMode.GET_AND_SET,
                            MethodType.methodType(
                                    int.class, FieldVarHandleInvokerTest.class, int.class));

            field = 3;
            int oldField = (int) invokerMethodHandle.invoke(fieldVarHandle, this, 4);
            assertEquals(3, oldField);
            assertEquals(4, field);

            //
            // Check invocations with MethodHandle.invoke()
            //

            // Check for unboxing
            int i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, Integer.valueOf(3));
            assertEquals(3, field);

            // Check for widening conversion
            i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, (short) 33);
            assertEquals(33, field);

            // Check for widening conversion
            i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, Byte.valueOf((byte) 34));
            assertEquals(34, field);

            // Check for acceptance of void return type
            invokerMethodHandle.invoke(fieldVarHandle, this, 77);
            assertEquals(77, field);

            // Check for wider return type
            long l = (long) invokerMethodHandle.invoke(fieldVarHandle, this, 88);
            assertEquals(88, field);
            try {
                // Check narrowing conversion fails
                i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, 3.0);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
            }
            try {
                // Check reference type fails
                i = (int) invokerMethodHandle.invoke(fieldVarHandle, this, "Bad");
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
            }
            try {
                // Check null VarHandle instance fails
                VarHandle vhNull = null;
                i = (int) invokerMethodHandle.invoke(vhNull, this, 888);
                assertUnreachable();
            } catch (NullPointerException expected) {
                assertEquals(88, field);
            }

            //
            // Check invocations with MethodHandle.invokeExact()
            //
            field = -1;
            try {
                // Check for unboxing
                i = (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, Integer.valueOf(3));
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(-1, field);
            }
            try {
                // Check for widening conversion
                i = (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, (short) 33);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(-1, field);
            }
            try {
                // Check for acceptance of void return type
                invokerMethodHandle.invokeExact(fieldVarHandle, this, 77);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(-1, field);
            }
            try {
                // Check for wider return type
                l = (long) invokerMethodHandle.invokeExact(fieldVarHandle, this, 78);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(-1, field);
            }
            try {
                // Check narrowing conversion fails
                i = (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, 3.0);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
            }
            try {
                // Check reference type fails
                i = (int) invokerMethodHandle.invokeExact(fieldVarHandle, this, "Bad");
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
            }
            try {
                // Check null VarHandle instance fails
                VarHandle vhNull = null;
                i = (int) invokerMethodHandle.invokeExact(vhNull, this, 888);
                assertUnreachable();
            } catch (NullPointerException expected) {
                assertEquals(-1, field);
            }
        }
    }

    static class DivergenceExactInvokerTest {
        private static final VarHandle floatsArrayVarHandle;

        static {
            try {
                floatsArrayVarHandle = MethodHandles.arrayElementVarHandle(float[].class);
            } catch (Exception e) {
                throw new TestSetupError("Failed to create VarHandle", e);
            }
        }

        void run() throws Throwable {
            System.out.println("DivergenceExactInvokerTest");
            float[] floatsArray = new float[4];
            // Exact invoker of an accessor having the form:
            //  float accessor(float[] values, int index, Float current, float replacement)
            MethodHandle exactInvoker =
                    MethodHandles.varHandleExactInvoker(
                            VarHandle.AccessMode.COMPARE_AND_EXCHANGE,
                            MethodType.methodType(
                                    float.class,
                                    float[].class,
                                    int.class,
                                    Float.class,
                                    float.class));
            floatsArray[2] = Float.valueOf(4.0f);
            // Callsite that is an exact match with exactInvoker.type().
            try {
                // exactInvoker.type() is not compatible with floatsArrayVarHandle accessor.
                float old =
                        (float)
                                exactInvoker.invoke(
                                        floatsArrayVarHandle,
                                        floatsArray,
                                        2,
                                        Float.valueOf(4.0f),
                                        8.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4.0f, floatsArray[2]);
            }

            // Callsites that are exact matches with exactInvoker.type()
            try {
                // Mismatch between exactInvoker.type() and VarHandle type (Float != float)
                float old =
                        (float)
                                exactInvoker.invoke(
                                        floatsArrayVarHandle, floatsArray, 2, 8.0f, 16.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4.0f, floatsArray[2]);
            }
            try {
                // short not convertible to Float
                float old =
                        (float)
                                exactInvoker.invoke(
                                        floatsArrayVarHandle, floatsArray, 2, (short) 4, 13.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4.0f, floatsArray[2]);
            }
            try {
                // int not convertible to Float
                float old =
                        (float) exactInvoker.invoke(floatsArrayVarHandle, floatsArray, 2, 8, -8.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(4.0f, floatsArray[2]);
            }
        }
    }

    static class DivergenceInvokerTest {
        private static final VarHandle floatsArrayVarHandle;

        static {
            try {
                floatsArrayVarHandle = MethodHandles.arrayElementVarHandle(float[].class);
            } catch (Exception e) {
                throw new TestSetupError("Failed to create VarHandle", e);
            }
        }

        void run() throws Throwable {
            System.out.println("DivergenceInvokerTest");
            float[] floatsArray = new float[4];
            // Invoker of an accessor having the form:
            //  float accessor(float[] values, int index, Float current, float replacement)
            MethodHandle invoker =
                    MethodHandles.varHandleInvoker(
                            VarHandle.AccessMode.COMPARE_AND_EXCHANGE,
                            MethodType.methodType(
                                    float.class,
                                    float[].class,
                                    int.class,
                                    Float.class,
                                    float.class));
            floatsArray[2] = Float.valueOf(4.0f);
            // Callsite that is an exact match with invoker.type()
            float old =
                    (float)
                            invoker.invoke(
                                    floatsArrayVarHandle,
                                    floatsArray,
                                    2,
                                    Float.valueOf(4.0f),
                                    8.0f);
            assertEquals(4.0f, old);
            assertEquals(8.0f, floatsArray[2]);

            // Callsite that is convertible match to invoker.type()
            old = (float) invoker.invoke(floatsArrayVarHandle, floatsArray, 2, 8.0f, 16.0f);
            assertEquals(8.0f, old);
            assertEquals(16.0f, floatsArray[2]);

            // Callsites that are not convertible to invoker.type().
            try {
                // short is not convertible to Float
                old = (float) invoker.invoke(floatsArrayVarHandle, floatsArray, 2, (short) 4, 8.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(16.0f, floatsArray[2]);
            }
            try {
                // int is not convertible to Float
                old = (float) invoker.invoke(floatsArrayVarHandle, floatsArray, 2, 8, -8.0f);
                assertUnreachable();
            } catch (WrongMethodTypeException expected) {
                assertEquals(16.0f, floatsArray[2]);
            }
        }
    }

    public static void main(String[] args) throws Throwable {
        new FieldVarHandleExactInvokerTest().run();
        new FieldVarHandleInvokerTest().run();
        new DivergenceExactInvokerTest().run();
        new DivergenceInvokerTest().run();
    }
}
