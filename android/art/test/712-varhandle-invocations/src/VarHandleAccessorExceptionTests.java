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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.invoke.WrongMethodTypeException;

// These tests cover DoVarHandleInvokeCommon in interpreter_common.cc.

public class VarHandleAccessorExceptionTests {
    public static class NullReceiverTest extends VarHandleUnitTest {
        private static final VarHandle vh = null;

        @Override
        protected void doTest() {
            try {
                vh.set(3);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new NullReceiverTest().run();
        }
    }

    public static class UnsupportedAccessModeTest extends VarHandleUnitTest {
        private static final boolean b = true;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = UnsupportedAccessModeTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "b", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            // A final field should not support an VarHandle access modes which can update it
            boolean isSupported =
                    vh.isAccessModeSupported(VarHandle.AccessMode.GET_AND_BITWISE_AND);
            assertFalse(isSupported);
            try {
                vh.getAndBitwiseAnd(true);
                failUnreachable();
            } catch (UnsupportedOperationException ex) {
            }
        }

        public static void main(String[] args) {
            new UnsupportedAccessModeTest().run();
        }
    }

    public static class WrongArgumentTypeCausingWrongMethodTypeTest extends VarHandleUnitTest {
        private short s;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WrongArgumentTypeCausingWrongMethodTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "s", short.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.set(this, (short) 0xcafe);
            try {
                vh.setVolatile(this, System.out); // System.out is a PrintStream, not short!
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WrongArgumentTypeCausingWrongMethodTypeTest().run();
        }
    }

    // Too many arguments causing WMTE
    public static class TooManyArgumentsCausingWrongMethodTypeTest extends VarHandleUnitTest {
        private int i;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = TooManyArgumentsCausingWrongMethodTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "i", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.set(this, 0x12345678);
            try {
                vh.setVolatile(this, 0x5a5a55aa, 0xc3c30f0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new TooManyArgumentsCausingWrongMethodTypeTest().run();
        }
    }

    public static class TooFewArgumentsCausingWrongMethodTypeTest extends VarHandleUnitTest {
        private int i;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = TooFewArgumentsCausingWrongMethodTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "i", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            i = 33;
            vh.compareAndSet(this, 33, 44);
            boolean updated = false;
            try {
                updated = (boolean) vh.compareAndSet(this, 44);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            assertFalse(updated); // Should have failed too few arguments
        }

        public static void main(String[] args) {
            new TooFewArgumentsCausingWrongMethodTypeTest().run();
        }
    }

    public static class ReturnTypeCausingWrongMethodTypeTest extends VarHandleUnitTest {
        private int i;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = ReturnTypeCausingWrongMethodTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "i", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            i = 33;
            vh.getAndSet(this, 44);
            Runtime runtime = null;
            try {
                runtime = (Runtime) vh.getAndSet(this, 44);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            assertEquals(null, runtime);
        }

        public static void main(String[] args) {
            new ReturnTypeCausingWrongMethodTypeTest().run();
        }
    }

    public static class UnsupportedAccessModePreemptsWrongMethodTypeExceptionTest
            extends VarHandleUnitTest {
        private static final boolean b = true;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = UnsupportedAccessModePreemptsWrongMethodTypeExceptionTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "b", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            // A final field should not support an VarHandle access modes which can update it
            boolean supported = vh.isAccessModeSupported(VarHandle.AccessMode.GET_AND_BITWISE_AND);
            assertFalse(supported);
            try {
                // The following is both unsupported and a wrong method type...
                vh.getAndBitwiseAnd(System.out);
                failUnreachable();
            } catch (UnsupportedOperationException ex) {
            }
        }

        public static void main(String[] args) {
            new UnsupportedAccessModePreemptsWrongMethodTypeExceptionTest().run();
        }
    }

    public static void main(String[] args) {
        NullReceiverTest.main(args);
        UnsupportedAccessModeTest.main(args);
        WrongArgumentTypeCausingWrongMethodTypeTest.main(args);
        TooManyArgumentsCausingWrongMethodTypeTest.main(args);
        TooFewArgumentsCausingWrongMethodTypeTest.main(args);
        ReturnTypeCausingWrongMethodTypeTest.main(args);
        UnsupportedAccessModePreemptsWrongMethodTypeExceptionTest.main(args);
    }
}
