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

public class VarHandleBadCoordinateTests {
    public static class FieldCoordinateTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        public static class A {
            public byte field;
        }

        public static class B extends A {
            private byte other_field;
        }

        public static class C {}

        static {
            try {
                vh = MethodHandles.lookup().findVarHandle(A.class, "field", byte.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            vh.compareAndSet(new A(), (byte) 0, (byte) 3);
            vh.compareAndSet(new B(), (byte) 0, (byte) 3);
            try {
                vh.compareAndSet(new C(), (byte) 0, (byte) 3);
                failUnreachable();
            } catch (ClassCastException ex) {
            }
            try {
                vh.compareAndSet(0xbad0bad0, (byte) 0, (byte) 3);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.compareAndSet(0xbad0bad0, (byte) 0, Integer.MAX_VALUE);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.compareAndSet(0xbad0bad0, (byte) 0);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.compareAndSet(new A(), (byte) 0, Integer.MAX_VALUE);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
            try {
                vh.compareAndSet((A) null, (byte) 0, (byte) 3);
                failUnreachable();
            } catch (NullPointerException ex) {
            }
        }

        public static void main(String[] args) {
            new FieldCoordinateTypeTest().run();
        }
    }

    public static class ArrayElementOutOfBoundsIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            try {
                vh.get(values, -1);
                failUnreachable();
            } catch (ArrayIndexOutOfBoundsException ex) {
            }
            try {
                vh.get(values, values.length);
                failUnreachable();
            } catch (ArrayIndexOutOfBoundsException ex) {
            }
            try {
                vh.get(values, Integer.MAX_VALUE - 1);
                failUnreachable();
            } catch (ArrayIndexOutOfBoundsException ex) {
            }
        }

        public static void main(String[] args) {
            new ArrayElementOutOfBoundsIndexTest().run();
        }
    }

    public static class ArrayElementBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            vh.set(values, Integer.valueOf(3), Long.MIN_VALUE);
            vh.set(values, Byte.valueOf((byte) 0), Long.MIN_VALUE);
            try {
                vh.set(values, 3.3f, Long.MAX_VALUE);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new ArrayElementBadIndexTypeTest().run();
        }
    }

    public static class ArrayElementNullArrayTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = null;
            try {
                vh.get(values);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new ArrayElementNullArrayTest().run();
        }
    }

    public static class ArrayElementWrongArrayTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            try {
                vh.get(new char[10], 0);
                failUnreachable();
            } catch (ClassCastException ex) {
            }
        }

        public static void main(String[] args) {
            new ArrayElementWrongArrayTypeTest().run();
        }
    }

    public static class ArrayElementMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.arrayElementVarHandle(long[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            long[] values = new long[33];
            try {
                vh.get(values);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new ArrayElementMissingIndexTest().run();
        }
    }

    public static class ByteArrayViewOutOfBoundsIndexTest extends VarHandleUnitTest {
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
            byte[] bytes = new byte[16];
            try {
                vh.get(bytes, -1);
                failUnreachable();
            } catch (IndexOutOfBoundsException ex) {
            }
            try {
                vh.get(bytes, bytes.length);
                failUnreachable();
            } catch (IndexOutOfBoundsException ex) {
            }
            try {
                vh.get(bytes, Integer.MAX_VALUE - 1);
                failUnreachable();
            } catch (IndexOutOfBoundsException ex) {
            }
            try {
                vh.get(bytes, bytes.length - Integer.SIZE / 8 + 1);
                failUnreachable();
            } catch (IndexOutOfBoundsException ex) {
            }
            vh.get(bytes, bytes.length - Integer.SIZE / 8);
        }

        public static void main(String[] args) {
            new ByteArrayViewOutOfBoundsIndexTest().run();
        }
    }

    public static class ByteArrayViewUnalignedAccessesIndexTest extends VarHandleUnitTest {
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
            byte[] bytes = new byte[33];

            int alignedIndex = VarHandleUnitTestHelpers.alignedOffset_int(bytes, 0);
            for (int i = alignedIndex; i < Integer.SIZE / 8; ++i) {
                // No exceptions are expected for GET and SET
                // accessors irrespective of the access alignment.
                vh.set(bytes, i, 380);
                vh.get(bytes, i);
                // Other accessors raise an IllegalStateException if
                // the access is unaligned.
                try {
                    vh.compareAndExchange(bytes, i, 777, 320);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.compareAndExchangeAcquire(bytes, i, 320, 767);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.compareAndExchangeRelease(bytes, i, 767, 321);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.compareAndSet(bytes, i, 767, 321);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAcquire(bytes, i);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndAdd(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndAddAcquire(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndAddRelease(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseAnd(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseAndAcquire(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseAndRelease(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseOr(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseOrAcquire(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseOrRelease(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseXor(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseXorAcquire(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndBitwiseXorRelease(bytes, i, 118);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndSet(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndSetAcquire(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getAndSetRelease(bytes, i, 117);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getOpaque(bytes, i);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.getVolatile(bytes, i);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.setOpaque(bytes, i, 777);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.setRelease(bytes, i, 319);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.setVolatile(bytes, i, 787);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.weakCompareAndSet(bytes, i, 787, 340);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.weakCompareAndSetAcquire(bytes, i, 787, 340);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.weakCompareAndSetPlain(bytes, i, 787, 340);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
                try {
                    vh.weakCompareAndSetRelease(bytes, i, 787, 340);
                    assertTrue(i == alignedIndex);
                } catch (IllegalStateException ex) {
                    assertFalse(i == alignedIndex);
                }
            }
        }

        public static void main(String[] args) {
            new ByteArrayViewUnalignedAccessesIndexTest().run();
        }
    }

    public static class ByteArrayViewBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[16];
            // Boxed index goes through argument conversion so no exception expected.
            vh.get(bytes, Integer.valueOf(3));
            vh.get(bytes, Short.valueOf((short) 3));

            try {
                vh.get(bytes, System.out);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new ByteArrayViewBadIndexTypeTest().run();
        }
    }

    public static class ByteArrayViewMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = new byte[16];
            try {
                vh.get(bytes);
                failUnreachable();
            } catch (WrongMethodTypeException ex) {
            }
        }

        public static void main(String[] args) {
            new ByteArrayViewMissingIndexTest().run();
        }
    }

    public static class ByteArrayViewBadByteArrayTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            byte[] bytes = null;
            try {
                vh.get(bytes, Integer.valueOf(3));
                failUnreachable();
            } catch (NullPointerException ex) {
            }
            try {
                vh.get(System.err, Integer.valueOf(3));
                failUnreachable();
            } catch (ClassCastException ex) {
            }
        }

        public static void main(String[] args) {
            new ByteArrayViewBadByteArrayTest().run();
        }
    }

    public static class ByteBufferViewOutOfBoundsIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer[] buffers =
                    new ByteBuffer[] {
                        ByteBuffer.allocateDirect(16),
                        ByteBuffer.allocate(37),
                        ByteBuffer.wrap(new byte[27], 3, 27 - 3)
                    };
            for (ByteBuffer buffer : buffers) {
                try {
                    vh.get(buffer, -1);
                    failUnreachable();
                } catch (IndexOutOfBoundsException ex) {
                }
                try {
                    vh.get(buffer, buffer.limit());
                    failUnreachable();
                } catch (IndexOutOfBoundsException ex) {
                }
                try {
                    vh.get(buffer, Integer.MAX_VALUE - 1);
                    failUnreachable();
                } catch (IndexOutOfBoundsException ex) {
                }
                try {
                    vh.get(buffer, buffer.limit() - Integer.SIZE / 8 + 1);
                    failUnreachable();
                } catch (IndexOutOfBoundsException ex) {
                }
                vh.get(buffer, buffer.limit() - Integer.SIZE / 8);
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewOutOfBoundsIndexTest().run();
        }
    }

    public static class ByteBufferViewUnalignedAccessesIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer[] buffers =
                    new ByteBuffer[] {
                        ByteBuffer.allocateDirect(16),
                        ByteBuffer.allocate(37),
                        ByteBuffer.wrap(new byte[27], 3, 27 - 3)
                    };

            for (ByteBuffer buffer : buffers) {
                int alignedIndex = VarHandleUnitTestHelpers.alignedOffset_int(buffer, 0);
                for (int i = alignedIndex; i < Integer.SIZE / 8; ++i) {
                    // No exceptions are expected for GET and SET
                    // accessors irrespective of the access alignment.
                    vh.set(buffer, i, 380);
                    vh.get(buffer, i);
                    // Other accessors raise an IllegalStateException if
                    // the access is unaligned.
                    try {
                        vh.compareAndExchange(buffer, i, 777, 320);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.compareAndExchangeAcquire(buffer, i, 320, 767);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.compareAndExchangeRelease(buffer, i, 767, 321);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.compareAndSet(buffer, i, 767, 321);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAcquire(buffer, i);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndAdd(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndAddAcquire(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndAddRelease(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseAnd(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseAndAcquire(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseAndRelease(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseOr(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseOrAcquire(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseOrRelease(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseXor(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseXorAcquire(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndBitwiseXorRelease(buffer, i, 118);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndSet(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndSetAcquire(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getAndSetRelease(buffer, i, 117);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getOpaque(buffer, i);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.getVolatile(buffer, i);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.setOpaque(buffer, i, 777);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.setRelease(buffer, i, 319);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.setVolatile(buffer, i, 787);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.weakCompareAndSet(buffer, i, 787, 340);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.weakCompareAndSetAcquire(buffer, i, 787, 340);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.weakCompareAndSetPlain(buffer, i, 787, 340);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                    try {
                        vh.weakCompareAndSetRelease(buffer, i, 787, 340);
                        assertTrue(i == alignedIndex);
                    } catch (IllegalStateException ex) {
                        assertFalse(i == alignedIndex);
                    }
                }
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewUnalignedAccessesIndexTest().run();
        }
    }

    public static class ByteBufferViewBadIndexTypeTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer[] buffers =
                    new ByteBuffer[] {
                        ByteBuffer.allocateDirect(16),
                        ByteBuffer.allocate(16),
                        ByteBuffer.wrap(new byte[32], 4, 32 - 4)
                    };

            for (ByteBuffer buffer : buffers) {
                // Boxed index goes through argument conversion so no exception expected.
                vh.get(buffer, Integer.valueOf(3));
                vh.get(buffer, Short.valueOf((short) 3));
                vh.get(buffer, Byte.valueOf((byte) 7));
                try {
                    vh.get(buffer, System.out);
                    failUnreachable();
                } catch (WrongMethodTypeException ex) {
                }
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewBadIndexTypeTest().run();
        }
    }

    public static class ByteBufferViewMissingIndexTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            ByteBuffer[] buffers =
                    new ByteBuffer[] {
                        ByteBuffer.allocateDirect(16),
                        ByteBuffer.allocate(16),
                        ByteBuffer.wrap(new byte[32], 4, 32 - 4)
                    };
            for (ByteBuffer buffer : buffers) {
                try {
                    vh.get(buffer);
                    failUnreachable();
                } catch (WrongMethodTypeException ex) {
                }
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewMissingIndexTest().run();
        }
    }

    public static class ByteBufferViewBadByteBufferTest extends VarHandleUnitTest {
        private static final VarHandle vh;

        static {
            try {
                vh = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() {
            if (VarHandleUnitTestHelpers.isRunningOnAndroid()) {
                ByteBuffer buffer = null;
                // The RI does not like this test
                try {
                    vh.get(buffer, 3);
                    failUnreachable();
                } catch (NullPointerException ex) {
                }
            }
            try {
                vh.get(System.err, 3);
                failUnreachable();
            } catch (ClassCastException ex) {
            }
        }

        public static void main(String[] args) {
            new ByteBufferViewBadByteBufferTest().run();
        }
    }

    public static void main(String[] args) {
        FieldCoordinateTypeTest.main(args);

        ArrayElementOutOfBoundsIndexTest.main(args);
        ArrayElementBadIndexTypeTest.main(args);
        ArrayElementNullArrayTest.main(args);
        ArrayElementWrongArrayTypeTest.main(args);
        ArrayElementMissingIndexTest.main(args);

        ByteArrayViewOutOfBoundsIndexTest.main(args);
        ByteArrayViewUnalignedAccessesIndexTest.main(args);
        ByteArrayViewBadIndexTypeTest.main(args);
        ByteArrayViewMissingIndexTest.main(args);
        ByteArrayViewBadByteArrayTest.main(args);

        ByteBufferViewOutOfBoundsIndexTest.main(args);
        ByteBufferViewUnalignedAccessesIndexTest.main(args);
        ByteBufferViewBadIndexTypeTest.main(args);
        ByteBufferViewMissingIndexTest.main(args);
        ByteBufferViewBadByteBufferTest.main(args);
    }
}
