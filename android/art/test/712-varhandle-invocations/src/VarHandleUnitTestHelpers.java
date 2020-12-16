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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class VarHandleUnitTestHelpers {
    public static boolean isRunningOnAndroid() {
        return System.getProperty("java.vm.vendor").contains("Android");
    }

    public static boolean is64Bit() {
        // The behaviour of certain accessors depends on the ISA word size.
        if (isRunningOnAndroid()) {
            try {
                Class<?> runtimeClass = Class.forName("dalvik.system.VMRuntime");
                MethodHandle getRuntimeMH =
                        MethodHandles.lookup()
                                .findStatic(
                                        runtimeClass,
                                        "getRuntime",
                                        MethodType.methodType(runtimeClass));
                Object runtime = getRuntimeMH.invoke();
                MethodHandle is64BitMH =
                        MethodHandles.lookup()
                                .findVirtual(
                                        runtimeClass,
                                        "is64Bit",
                                        MethodType.methodType(boolean.class));
                return (boolean) is64BitMH.invoke(runtime);
            } catch (Throwable t) {
                throw new RuntimeException(t);
            }
        } else {
            return System.getProperty("sun.arch.data.model").equals("64");
        }
    }

    public static boolean getBytesAs_boolean(byte[] array, int index, ByteOrder order) {
        return getBytesAs_boolean(ByteBuffer.wrap(array), index, order);
    }

    public static byte getBytesAs_byte(byte[] array, int index, ByteOrder order) {
        return getBytesAs_byte(ByteBuffer.wrap(array), index, order);
    }

    public static char getBytesAs_char(byte[] array, int index, ByteOrder order) {
        return getBytesAs_char(ByteBuffer.wrap(array), index, order);
    }

    public static short getBytesAs_short(byte[] array, int index, ByteOrder order) {
        return getBytesAs_short(ByteBuffer.wrap(array), index, order);
    }

    public static int getBytesAs_int(byte[] array, int index, ByteOrder order) {
        return getBytesAs_int(ByteBuffer.wrap(array), index, order);
    }

    public static long getBytesAs_long(byte[] array, int index, ByteOrder order) {
        return getBytesAs_long(ByteBuffer.wrap(array), index, order);
    }

    public static float getBytesAs_float(byte[] array, int index, ByteOrder order) {
        return getBytesAs_float(ByteBuffer.wrap(array), index, order);
    }

    public static double getBytesAs_double(byte[] array, int index, ByteOrder order) {
        return getBytesAs_double(ByteBuffer.wrap(array), index, order);
    }

    public static boolean getBytesAs_boolean(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).get(index) != 0;
    }

    public static byte getBytesAs_byte(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).get(index);
    }

    public static char getBytesAs_char(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getChar(index);
    }

    public static short getBytesAs_short(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getShort(index);
    }

    public static int getBytesAs_int(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getInt(index);
    }

    public static long getBytesAs_long(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getLong(index);
    }

    public static float getBytesAs_float(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getFloat(index);
    }

    public static double getBytesAs_double(ByteBuffer buffer, int index, ByteOrder order) {
        return buffer.order(order).getDouble(index);
    }

    public static void setBytesAs_boolean(byte[] array, int index, boolean value, ByteOrder order) {
        setBytesAs_boolean(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_byte(byte[] array, int index, byte value, ByteOrder order) {
        setBytesAs_byte(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_char(byte[] array, int index, char value, ByteOrder order) {
        setBytesAs_char(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_short(byte[] array, int index, short value, ByteOrder order) {
        setBytesAs_short(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_int(byte[] array, int index, int value, ByteOrder order) {
        setBytesAs_int(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_long(byte[] array, int index, long value, ByteOrder order) {
        setBytesAs_long(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_float(byte[] array, int index, float value, ByteOrder order) {
        setBytesAs_float(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_double(byte[] array, int index, double value, ByteOrder order) {
        setBytesAs_double(ByteBuffer.wrap(array), index, value, order);
    }

    public static void setBytesAs_boolean(
            ByteBuffer buffer, int index, boolean value, ByteOrder order) {
        buffer.order(order).put(index, value ? (byte) 1 : (byte) 0);
    }

    public static void setBytesAs_byte(ByteBuffer buffer, int index, byte value, ByteOrder order) {
        buffer.order(order).put(index, value);
    }

    public static void setBytesAs_char(ByteBuffer buffer, int index, char value, ByteOrder order) {
        buffer.order(order).putChar(index, value);
    }

    public static void setBytesAs_short(
            ByteBuffer buffer, int index, short value, ByteOrder order) {
        buffer.order(order).putShort(index, value);
    }

    public static void setBytesAs_int(ByteBuffer buffer, int index, int value, ByteOrder order) {
        buffer.order(order).putInt(index, value);
    }

    public static void setBytesAs_long(ByteBuffer buffer, int index, long value, ByteOrder order) {
        buffer.order(order).putLong(index, value);
    }

    public static void setBytesAs_float(
            ByteBuffer buffer, int index, float value, ByteOrder order) {
        buffer.order(order).putFloat(index, value);
    }

    public static void setBytesAs_double(
            ByteBuffer buffer, int index, double value, ByteOrder order) {
        buffer.order(order).putDouble(index, value);
    }

    // Until ART is running on an OpenJDK9 based runtime, there are no
    // calls to help with alignment. OpenJDK9 introduces
    // ByteBuffer.alignedSlice() and ByteBuffer.alignmentOffset(). RI
    // and ART have different data structure alignments which may make
    // porting code interesting.

    public static int alignedOffset_char(ByteBuffer buffer, int start) {
        return alignedOffset_short(buffer, start);
    }

    public static int alignedOffset_short(ByteBuffer buffer, int start) {
        for (int i = 0; i < Short.SIZE; ++i) {
            try {
                vh_probe_short.getVolatile(buffer, start + i);
                return start + i;
            } catch (IllegalStateException e) {
                // Unaligned access.
            }
        }
        return start;
    }

    public static int alignedOffset_int(ByteBuffer buffer, int start) {
        for (int i = 0; i < Integer.SIZE; ++i) {
            try {
                vh_probe_int.getVolatile(buffer, start + i);
                return start + i;
            } catch (IllegalStateException e) {
                // Unaligned access.
            } catch (Exception e) {
                break;
            }
        }
        return start;
    }

    public static int alignedOffset_long(ByteBuffer buffer, int start) {
        for (int i = 0; i < Long.SIZE; ++i) {
            try {
                vh_probe_long.getVolatile(buffer, start + i);
                return start + i;
            } catch (IllegalStateException e) {
                // Unaligned access.
            } catch (UnsupportedOperationException e) {
                // 64-bit operation is not supported irrespective of alignment.
                break;
            }
        }
        return start;
    }

    public static int alignedOffset_float(ByteBuffer buffer, int start) {
        return alignedOffset_int(buffer, start);
    }

    public static int alignedOffset_double(ByteBuffer buffer, int start) {
        return alignedOffset_long(buffer, start);
    }

    public static int alignedOffset_char(byte[] array, int start) {
        return alignedOffset_char(ByteBuffer.wrap(array), start);
    }

    public static int alignedOffset_short(byte[] array, int start) {
        return alignedOffset_short(ByteBuffer.wrap(array), start);
    }

    public static int alignedOffset_int(byte[] array, int start) {
        return alignedOffset_int(ByteBuffer.wrap(array), start);
    }

    public static int alignedOffset_long(byte[] array, int start) {
        return alignedOffset_long(ByteBuffer.wrap(array), start);
    }

    public static int alignedOffset_float(byte[] array, int start) {
        return alignedOffset_float(ByteBuffer.wrap(array), start);
    }

    public static int alignedOffset_double(byte[] array, int start) {
        return alignedOffset_double(ByteBuffer.wrap(array), start);
    }

    static {
        ByteOrder order = ByteOrder.LITTLE_ENDIAN;
        vh_probe_short = MethodHandles.byteBufferViewVarHandle(short[].class, order);
        vh_probe_int = MethodHandles.byteBufferViewVarHandle(int[].class, order);
        vh_probe_long = MethodHandles.byteBufferViewVarHandle(long[].class, order);
    }

    private static final VarHandle vh_probe_short;
    private static final VarHandle vh_probe_int;
    private static final VarHandle vh_probe_long;
}
