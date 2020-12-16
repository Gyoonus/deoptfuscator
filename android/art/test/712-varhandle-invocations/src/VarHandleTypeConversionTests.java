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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class VarHandleTypeConversionTests {
    public static class VoidReturnTypeTest extends VarHandleUnitTest {
        private int i;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = VoidReturnTypeTest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "i", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            // Void is always okay for a return type.
            vh.setVolatile(this, 33);
            vh.get(this);
            vh.compareAndSet(this, 33, 44);
            vh.compareAndSet(this, 27, 16);
            vh.weakCompareAndSet(this, 17, 19);
            vh.getAndSet(this, 200000);
            vh.getAndBitwiseXor(this, 0x5a5a5a5a);
            vh.getAndAdd(this, 99);
        }

        public static void main(String[] args) {
            new VoidReturnTypeTest().run();
        }
    }

    //
    // Tests that a null reference as a boxed primitive type argument
    // throws a NullPointerException. These vary the VarHandle type
    // with each primitive for coverage.
    //

    public static class BoxedNullBooleanThrowsNPETest extends VarHandleUnitTest {
        private static boolean z;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullBooleanThrowsNPETest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "z", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            Boolean newValue = null;
            try {
                vh.getAndSet(newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullBooleanThrowsNPETest().run();
        }
    }

    public static class BoxedNullByteThrowsNPETest extends VarHandleUnitTest {
        private byte b;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullByteThrowsNPETest.class;
                vh = MethodHandles.lookup().findVarHandle(cls, "b", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            Byte newValue = null;
            try {
                vh.getAndSet(this, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullByteThrowsNPETest().run();
        }
    }

    public static class BoxedNullCharacterThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(char[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            char[] values = new char[3];
            Character newValue = null;
            try {
                vh.getAndSet(values, 0, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullCharacterThrowsNPETest().run();
        }
    }

    public static class BoxedNullShortThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullShortThrowsNPETest.class;
                vh = MethodHandles.byteArrayViewVarHandle(short[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[2 * Short.SIZE];
            int index = VarHandleUnitTestHelpers.alignedOffset_short(bytes, 0);
            Short newValue = null;
            try {
                vh.set(bytes, index, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullShortThrowsNPETest().run();
        }
    }

    public static class BoxedNullIntegerThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[2 * Integer.SIZE];
            int index = VarHandleUnitTestHelpers.alignedOffset_int(bytes, 0);
            Integer newValue = null;
            try {
                vh.setVolatile(bytes, index, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullIntegerThrowsNPETest().run();
        }
    }

    public static class BoxedNullLongThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullLongThrowsNPETest.class;
                vh = MethodHandles.byteBufferViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer bb = ByteBuffer.allocateDirect(2 * Long.SIZE);
            int index = VarHandleUnitTestHelpers.alignedOffset_long(bb, 0);
            Long newValue = null;
            try {
                vh.getAndAdd(bb, index, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullLongThrowsNPETest().run();
        }
    }

    public static class BoxedNullFloatThrowsNPETest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = BoxedNullFloatThrowsNPETest.class;
                vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer bb = ByteBuffer.allocate(2 * Float.SIZE);
            int index = VarHandleUnitTestHelpers.alignedOffset_float(bb, 0);
            Float newValue = null;
            try {
                vh.set(bb, index, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullFloatThrowsNPETest().run();
        }
    }

    public static class BoxedNullDoubleThrowsNPETest extends VarHandleUnitTest {
        private double d;
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(double[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[3 * Double.SIZE];
            int offset = 1;
            ByteBuffer bb = ByteBuffer.wrap(bytes, offset, bytes.length - offset);
            int index = VarHandleUnitTestHelpers.alignedOffset_double(bb, 0);
            Double newValue = null;
            try {
                vh.set(bb, index, newValue);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new BoxedNullDoubleThrowsNPETest().run();
        }
    }

    public static class WideningBooleanArgumentTest extends VarHandleUnitTest {
        private static boolean v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningBooleanArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.set(true);
            try {
                vh.set((byte) 3);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set('c');
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((short) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((int) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((long) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningBooleanArgumentTest().run();
        }
    }

    public static class WideningByteArgumentTest extends VarHandleUnitTest {
        private static byte v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningByteArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            try {
                vh.set('c');
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((short) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((int) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((long) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningByteArgumentTest().run();
        }
    }

    public static class WideningCharacterArgumentTest extends VarHandleUnitTest {
        private static char v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningCharacterArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", char.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((byte) 3);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set('c');
            try {
                vh.set((short) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((int) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((long) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningCharacterArgumentTest().run();
        }
    }

    public static class WideningShortArgumentTest extends VarHandleUnitTest {
        private static short v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningShortArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", short.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            try {
                vh.set('c');
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((short) 1);
            try {
                vh.set((int) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((long) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningShortArgumentTest().run();
        }
    }

    public static class WideningIntegerArgumentTest extends VarHandleUnitTest {
        private static int v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningIntegerArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            vh.set('c');
            vh.set((char) 0x8fff);
            assertEquals(0x8fff, v);
            vh.set((short) 1);
            vh.set((int) 1);
            try {
                vh.set((long) 1);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningIntegerArgumentTest().run();
        }
    }

    public static class WideningLongArgumentTest extends VarHandleUnitTest {
        private static long v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningLongArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", long.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            vh.set('c');
            vh.set((short) 1);
            vh.set((int) 1);
            vh.set((long) 1);
            try {
                vh.set((float) 1.0f);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningLongArgumentTest().run();
        }
    }

    public static class WideningFloatArgumentTest extends VarHandleUnitTest {
        private static float v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningFloatArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", float.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            vh.set('c');
            vh.set((short) 1);
            vh.set((int) 1);
            vh.set((long) 1);
            vh.set((float) 1.0f);
            try {
                vh.set((double) 1.0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningFloatArgumentTest().run();
        }
    }

    public static class WideningDoubleArgumentTest extends VarHandleUnitTest {
        private static double v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningDoubleArgumentTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", double.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            vh.set((byte) 3);
            vh.set('c');
            vh.set((short) 1);
            vh.set((int) 1);
            vh.set((long) 1);
            vh.set((double) 1.0f);
            vh.set((double) 1.0);
        }

        public static void main(String[] args) {
            new WideningDoubleArgumentTest().run();
        }
    }

    public static class WideningBooleanReturnValueTest extends VarHandleUnitTest {
        private static boolean v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningBooleanReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", boolean.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.set(true);
            vh.get();
            boolean z = (boolean) vh.get();
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                int i = (int) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                long j = (long) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                float f = (float) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                double d = (double) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new WideningBooleanReturnValueTest().run();
        }
    }

    public static class WideningByteReturnValueTest extends VarHandleUnitTest {
        private static byte v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningByteReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.set((byte) 3);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }

            byte b = (byte) vh.get();
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            short s = (short) vh.get();
            int i = (int) vh.get();
            long j = (long) vh.get();
            float f = (float) vh.get();
            double d = (double) vh.get();
        }

        public static void main(String[] args) {
            new WideningByteReturnValueTest().run();
        }
    }

    public static class WideningCharacterReturnValueTest extends VarHandleUnitTest {
        private static char v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningCharacterReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", char.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningCharacterReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set('c');
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            char c = (char) vh.get();
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            int i = (int) vh.get();
            long j = (long) vh.get();
            float f = (float) vh.get();
            double d = (double) vh.get();
        }
    }

    public static class WideningShortReturnValueTest extends VarHandleUnitTest {
        private static short v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningShortReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", short.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningShortReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set((short) 8888);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            short s = (short) vh.get();
            int i = (int) vh.get();
            long j = (long) vh.get();
            float f = (float) vh.get();
            double d = (double) vh.get();
        }
    }

    public static class WideningIntegerReturnValueTest extends VarHandleUnitTest {
        private static int v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningIntegerReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningIntegerReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set(0x1234fedc);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            int i = (int) vh.get();
            long j = (long) vh.get();
            float f = (float) vh.get();
            double d = (double) vh.get();
        }
    }

    public static class WideningLongReturnValueTest extends VarHandleUnitTest {
        private static long v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningLongReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", long.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningLongReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set(0xfedcba987654321l);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                int i = (int) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            long j = (long) vh.get();
            float f = (float) vh.get();
            double d = (double) vh.get();
        }
    }

    public static class WideningFloatReturnValueTest extends VarHandleUnitTest {
        private static float v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningFloatReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", float.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningFloatReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set(7.77e20f);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                int i = (int) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                long j = (long) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            float f = (float) vh.get();
            double d = (double) vh.get();
        }
    }

    public static class WideningDoubleReturnValueTest extends VarHandleUnitTest {
        private static double v;
        private static final VarHandle vh;

        static {
            try {
                Class<?> cls = WideningDoubleReturnValueTest.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "v", double.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new WideningDoubleReturnValueTest().run();
        }

        @Override
        protected void doTest() {
            vh.set(Math.E);
            vh.get();
            try {
                boolean z = (boolean) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                byte b = (byte) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                char c = (char) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                short s = (short) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                int i = (int) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                long j = (long) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                float f = (float) vh.get();
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            double d = (double) vh.get();
        }
    }

    public static class SubtypeTest extends VarHandleUnitTest {
        private static final Widget INITIAL_VALUE = Widget.ONE;
        private static final VarHandle vh;
        private Widget w = INITIAL_VALUE;

        static {
            try {
                vh = MethodHandles.lookup().findVarHandle(SubtypeTest.class, "w", Widget.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new SubtypeTest().run();
        }

        // A sub-type of the Widget class
        public static class WidgetChild extends Widget {
            private int weight;

            public WidgetChild(int requistionNumber, int weight) {
                super(requistionNumber);
                this.weight = weight;
            }

            @Override
            public boolean equals(Object o) {
                if (this == o) {
                    return true;
                }
                if (o instanceof WidgetChild == false) {
                    return false;
                }
                WidgetChild wc = (WidgetChild) o;
                return (requisitionNumber == wc.requisitionNumber && weight == wc.weight);
            }

            public static final WidgetChild ONE = new WidgetChild(1, 100);
            public static final WidgetChild TWO = new WidgetChild(2, 2000);
        }

        @Override
        public void doTest() {
            assertEquals(INITIAL_VALUE, vh.getVolatile(this));
            vh.setVolatile(this, null);
            Widget rw = (Widget) vh.compareAndExchange(this, null, WidgetChild.ONE);
            assertEquals(null, rw);
            assertEquals(WidgetChild.ONE, this.w);
            WidgetChild rwc =
                    (WidgetChild)
                            vh.compareAndExchangeRelease(this, WidgetChild.ONE, WidgetChild.TWO);
            assertEquals(WidgetChild.TWO, w);
            rwc = (WidgetChild) vh.compareAndExchangeAcquire(this, WidgetChild.TWO, Widget.ONE);
            assertEquals(Widget.ONE, w);
            assertEquals(false, (boolean) vh.compareAndSet(this, null, null));
            assertEquals(true, vh.compareAndSet(this, Widget.ONE, Widget.TWO));
            assertEquals(Widget.TWO, w);
            vh.set(this, null);
            assertEquals(null, (Widget) vh.get(this));
            vh.setRelease(this, WidgetChild.ONE);
            assertEquals(WidgetChild.ONE, (WidgetChild) vh.getAcquire(this));
            assertEquals(WidgetChild.ONE, w);
            vh.setOpaque(this, WidgetChild.TWO);
            assertEquals(WidgetChild.TWO, vh.getOpaque(this));
            assertEquals(WidgetChild.TWO, w);
            vh.setVolatile(this, null);
            assertEquals(null, (Widget) vh.getVolatile(this));
            assertEquals(null, w);
            assertEquals(null, (WidgetChild) vh.getAndSet(this, WidgetChild.ONE));
            assertEquals(WidgetChild.ONE, w);
            assertEquals(WidgetChild.ONE, (WidgetChild) vh.getAndSetRelease(this, WidgetChild.TWO));
            assertEquals(WidgetChild.TWO, (WidgetChild) vh.getAndSetAcquire(this, WidgetChild.ONE));
            try {
                WidgetChild result = (WidgetChild) vh.getAndAdd(this, WidgetChild.ONE);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndAddAcquire(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndAddRelease(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseAnd(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseAndAcquire(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseAndRelease(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseOr(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseOrAcquire(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseOrRelease(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseXor(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseXorAcquire(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
            try {
                WidgetChild result = (WidgetChild) vh.getAndBitwiseXorRelease(this, 1);
                failUnreachable();
            } catch (UnsupportedOperationException e) {
            }
        }
    }

    public static class SupertypeTest extends VarHandleUnitTest {
        private Widget w = null;
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.lookup().findVarHandle(SupertypeTest.class, "w", Widget.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new SupertypeTest().run();
        }

        @Override
        public void doTest() {
            assertEquals(null, (Object) vh.get(this));
            vh.set(this, Widget.ONE);
            assertEquals(Widget.ONE, vh.getVolatile(this));
            try {
                vh.setVolatile(this, new Object());
            } catch (ClassCastException e) {
            }
        }
    }

    public static class ImplicitBoxingIntegerTest extends VarHandleUnitTest {
        private static Integer field;
        private static final VarHandle vh;

        static {
            try {
                vh =
                        MethodHandles.lookup()
                                .findStaticVarHandle(
                                        ImplicitBoxingIntegerTest.class, "field", Integer.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void main(String[] args) {
            new ImplicitBoxingIntegerTest().run();
        }

        @Override
        public void doTest() {
            try {
                vh.set(true);
                failUnreachable();
            } catch (WrongMethodTypeException e) {
            }
            try {
                vh.set((byte) 0);
                failUnreachable();
            } catch (WrongMethodTypeException e) {
            }
            try {
                vh.set((short) 1);
                failUnreachable();
            } catch (WrongMethodTypeException e) {
            }
            try {
                vh.set('A');
                failUnreachable();
            } catch (WrongMethodTypeException e) {
            }
            vh.set(2);
            try {
                vh.setRelease(Long.MAX_VALUE);
            } catch (WrongMethodTypeException e) {
            }
            try {
                vh.setRelease(Float.MAX_VALUE);
            } catch (WrongMethodTypeException e) {
            }
            try {
                vh.setRelease(Double.MAX_VALUE);
            } catch (WrongMethodTypeException e) {
            }
            vh.set(null);
            vh.set(Integer.valueOf(Integer.MAX_VALUE));
        }
    }

    public static void main(String[] args) {
        VoidReturnTypeTest.main(args);

        BoxedNullBooleanThrowsNPETest.main(args);
        BoxedNullByteThrowsNPETest.main(args);
        BoxedNullCharacterThrowsNPETest.main(args);
        BoxedNullShortThrowsNPETest.main(args);
        BoxedNullIntegerThrowsNPETest.main(args);
        BoxedNullLongThrowsNPETest.main(args);
        BoxedNullFloatThrowsNPETest.main(args);
        BoxedNullDoubleThrowsNPETest.main(args);

        WideningBooleanArgumentTest.main(args);
        WideningByteArgumentTest.main(args);
        WideningCharacterArgumentTest.main(args);
        WideningShortArgumentTest.main(args);
        WideningIntegerArgumentTest.main(args);
        WideningLongArgumentTest.main(args);
        WideningFloatArgumentTest.main(args);
        WideningDoubleArgumentTest.main(args);

        WideningBooleanReturnValueTest.main(args);
        WideningByteReturnValueTest.main(args);
        WideningCharacterReturnValueTest.main(args);
        WideningShortReturnValueTest.main(args);
        WideningIntegerReturnValueTest.main(args);
        WideningLongReturnValueTest.main(args);
        WideningFloatReturnValueTest.main(args);
        WideningDoubleReturnValueTest.main(args);

        SubtypeTest.main(args);
        SupertypeTest.main(args);

        ImplicitBoxingIntegerTest.main(args);
    }
}
