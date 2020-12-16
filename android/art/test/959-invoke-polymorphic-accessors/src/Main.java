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
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.reflect.Field;

public class Main {

    public static class ValueHolder {
        public boolean m_z = false;
        public byte m_b = 0;
        public char m_c = 'a';
        public short m_s = 0;
        public int m_i = 0;
        public float m_f = 0.0f;
        public double m_d = 0.0;
        public long m_j = 0;
        public String m_l = "a";

        public static boolean s_z;
        public static byte s_b;
        public static char s_c;
        public static short s_s;
        public static int s_i;
        public static float s_f;
        public static double s_d;
        public static long s_j;
        public static String s_l;

        public final int m_fi = 0xa5a5a5a5;
        public static final int s_fi = 0x5a5a5a5a;

        private boolean m_pz;
        private static final boolean s_fz = false;
    }

    public static class Tester {
        public static void assertEquals(boolean expected, boolean actual) {
            if (actual != expected) {
                throw new AssertionError("Actual != Expected (" + actual + " != " + expected + ")");
            }
        }

        public static void assertEquals(char expected, char actual) {
            if (actual != expected) {
                throw new AssertionError("Actual != Expected (" + actual + " != " + expected + ")");
            }
        }

        public static void assertEquals(int expected, int actual) {
            if (actual != expected) {
                throw new AssertionError("Actual != Expected (" + actual + " != " + expected + ")");
            }
        }

        public static void assertTrue(boolean value) throws AssertionError {
            if (!value) {
                throw new AssertionError("Value is not true");
            }
        }

        public static void fail() throws Throwable{
            throw new Error("fail");
        }
    }

    public static class InvokeExactTester extends Tester {
        private enum PrimitiveType {
            Boolean,
            Byte,
            Char,
            Short,
            Int,
            Long,
            Float,
            Double,
            String,
        }

        private enum AccessorType {
            IPUT,
            SPUT,
            IGET,
            SGET,
        }

        static void setByte(MethodHandle m, ValueHolder v, byte value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setByte(MethodHandle m, byte value, boolean expectFailure) throws Throwable {
            setByte(m, null, value, expectFailure);
        }

        static void getByte(MethodHandle m, ValueHolder v, byte value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final byte got;
                if (v == null) {
                    got = (byte) m.invokeExact();
                } else {
                    got = (byte) m.invokeExact(v);
                }
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getByte(MethodHandle m, byte value, boolean expectFailure) throws Throwable {
            getByte(m, null, value, expectFailure);
        }

        static void setChar(MethodHandle m, ValueHolder v, char value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setChar(MethodHandle m, char value, boolean expectFailure) throws Throwable {
            setChar(m, null, value, expectFailure);
        }

        static void getChar(MethodHandle m, ValueHolder v, char value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final char got;
                if (v == null) {
                    got = (char) m.invokeExact();
                } else {
                    got = (char) m.invokeExact(v);
                }
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getChar(MethodHandle m, char value, boolean expectFailure) throws Throwable {
            getChar(m, null, value, expectFailure);
        }

        static void setShort(MethodHandle m, ValueHolder v, short value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setShort(MethodHandle m, short value, boolean expectFailure) throws Throwable {
            setShort(m, null, value, expectFailure);
        }

        static void getShort(MethodHandle m, ValueHolder v, short value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final short got = (v == null) ? (short) m.invokeExact() : (short) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getShort(MethodHandle m, short value, boolean expectFailure) throws Throwable {
            getShort(m, null, value, expectFailure);
        }

        static void setInt(MethodHandle m, ValueHolder v, int value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setInt(MethodHandle m, int value, boolean expectFailure) throws Throwable {
            setInt(m, null, value, expectFailure);
        }

        static void getInt(MethodHandle m, ValueHolder v, int value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final int got = (v == null) ? (int) m.invokeExact() : (int) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getInt(MethodHandle m, int value, boolean expectFailure) throws Throwable {
            getInt(m, null, value, expectFailure);
        }

        static void setLong(MethodHandle m, ValueHolder v, long value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setLong(MethodHandle m, long value, boolean expectFailure) throws Throwable {
            setLong(m, null, value, expectFailure);
        }

        static void getLong(MethodHandle m, ValueHolder v, long value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final long got = (v == null) ? (long) m.invokeExact() : (long) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getLong(MethodHandle m, long value, boolean expectFailure) throws Throwable {
            getLong(m, null, value, expectFailure);
        }

        static void setFloat(MethodHandle m, ValueHolder v, float value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setFloat(MethodHandle m, float value, boolean expectFailure) throws Throwable {
            setFloat(m, null, value, expectFailure);
        }

        static void getFloat(MethodHandle m, ValueHolder v, float value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final float got = (v == null) ? (float) m.invokeExact() : (float) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getFloat(MethodHandle m, float value, boolean expectFailure) throws Throwable {
            getFloat(m, null, value, expectFailure);
        }

        static void setDouble(MethodHandle m, ValueHolder v, double value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setDouble(MethodHandle m, double value, boolean expectFailure)
                throws Throwable {
            setDouble(m, null, value, expectFailure);
        }

        static void getDouble(MethodHandle m, ValueHolder v, double value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final double got = (v == null) ? (double) m.invokeExact() : (double) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getDouble(MethodHandle m, double value, boolean expectFailure)
                throws Throwable {
            getDouble(m, null, value, expectFailure);
        }

        static void setString(MethodHandle m, ValueHolder v, String value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setString(MethodHandle m, String value, boolean expectFailure)
                throws Throwable {
            setString(m, null, value, expectFailure);
        }

        static void getString(MethodHandle m, ValueHolder v, String value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final String got = (v == null) ? (String) m.invokeExact() : (String) m.invokeExact(v);
                assertTrue(got.equals(value));
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getString(MethodHandle m, String value, boolean expectFailure)
                throws Throwable {
            getString(m, null, value, expectFailure);
        }

        static void setBoolean(MethodHandle m, ValueHolder v, boolean value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                if (v == null) {
                    m.invokeExact(value);
                }
                else {
                    m.invokeExact(v, value);
                }
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void setBoolean(MethodHandle m, boolean value, boolean expectFailure)
                throws Throwable {
            setBoolean(m, null, value, expectFailure);
        }

        static void getBoolean(MethodHandle m, ValueHolder v, boolean value, boolean expectFailure)
                throws Throwable {
            boolean exceptionThrown = false;
            try {
                final boolean got =
                        (v == null) ? (boolean) m.invokeExact() : (boolean) m.invokeExact(v);
                assertTrue(got == value);
            }
            catch (WrongMethodTypeException e) {
                exceptionThrown = true;
            }
            assertEquals(expectFailure, exceptionThrown);
        }

        static void getBoolean(MethodHandle m, boolean value, boolean expectFailure)
                throws Throwable {
            getBoolean(m, null, value, expectFailure);
        }

        static boolean resultFor(PrimitiveType actualType, PrimitiveType expectedType,
                                 AccessorType actualAccessor,
                                 AccessorType expectedAccessor) {
            return (actualType != expectedType) || (actualAccessor != expectedAccessor);
        }

        static void tryAccessor(MethodHandle methodHandle,
                                ValueHolder valueHolder,
                                PrimitiveType primitive,
                                Object value,
                                AccessorType accessor) throws Throwable {
            boolean booleanValue =
                    value instanceof Boolean ? ((Boolean) value).booleanValue() : false;
            setBoolean(methodHandle, valueHolder, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.IPUT));
            setBoolean(methodHandle, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.SPUT));
            getBoolean(methodHandle, valueHolder, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.IGET));
            getBoolean(methodHandle, booleanValue,
                       resultFor(primitive, PrimitiveType.Boolean, accessor, AccessorType.SGET));

            byte byteValue = value instanceof Byte ? ((Byte) value).byteValue() : (byte) 0;
            setByte(methodHandle, valueHolder, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.IPUT));
            setByte(methodHandle, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.SPUT));
            getByte(methodHandle, valueHolder, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.IGET));
            getByte(methodHandle, byteValue,
                    resultFor(primitive, PrimitiveType.Byte, accessor, AccessorType.SGET));

            char charValue = value instanceof Character ? ((Character) value).charValue() : 'z';
            setChar(methodHandle, valueHolder, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.IPUT));
            setChar(methodHandle, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.SPUT));
            getChar(methodHandle, valueHolder, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.IGET));
            getChar(methodHandle, charValue,
                    resultFor(primitive, PrimitiveType.Char, accessor, AccessorType.SGET));

            short shortValue = value instanceof Short ? ((Short) value).shortValue() : (short) 0;
            setShort(methodHandle, valueHolder, shortValue,
                     resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.IPUT));
            setShort(methodHandle, shortValue,
                    resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.SPUT));
            getShort(methodHandle, valueHolder, shortValue,
                     resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.IGET));
            getShort(methodHandle, shortValue,
                    resultFor(primitive, PrimitiveType.Short, accessor, AccessorType.SGET));

            int intValue = value instanceof Integer ? ((Integer) value).intValue() : -1;
            setInt(methodHandle, valueHolder, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.IPUT));
            setInt(methodHandle, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.SPUT));
            getInt(methodHandle, valueHolder, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.IGET));
            getInt(methodHandle, intValue,
                   resultFor(primitive, PrimitiveType.Int, accessor, AccessorType.SGET));

            long longValue = value instanceof Long ? ((Long) value).longValue() : (long) -1;
            setLong(methodHandle, valueHolder, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.IPUT));
            setLong(methodHandle, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.SPUT));
            getLong(methodHandle, valueHolder, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.IGET));
            getLong(methodHandle, longValue,
                    resultFor(primitive, PrimitiveType.Long, accessor, AccessorType.SGET));

            float floatValue = value instanceof Float ? ((Float) value).floatValue() : -1.0f;
            setFloat(methodHandle, valueHolder, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.IPUT));
            setFloat(methodHandle, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.SPUT));
            getFloat(methodHandle, valueHolder, floatValue,
                    resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.IGET));
            getFloat(methodHandle, floatValue,
                     resultFor(primitive, PrimitiveType.Float, accessor, AccessorType.SGET));

            double doubleValue = value instanceof Double ? ((Double) value).doubleValue() : -1.0;
            setDouble(methodHandle, valueHolder, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.IPUT));
            setDouble(methodHandle, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.SPUT));
            getDouble(methodHandle, valueHolder, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.IGET));
            getDouble(methodHandle, doubleValue,
                      resultFor(primitive, PrimitiveType.Double, accessor, AccessorType.SGET));

            String stringValue = value instanceof String ? ((String) value) : "No Spock, no";
            setString(methodHandle, valueHolder, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.IPUT));
            setString(methodHandle, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.SPUT));
            getString(methodHandle, valueHolder, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.IGET));
            getString(methodHandle, stringValue,
                      resultFor(primitive, PrimitiveType.String, accessor, AccessorType.SGET));
        }

        public static void main() throws Throwable {
            ValueHolder valueHolder = new ValueHolder();
            MethodHandles.Lookup lookup = MethodHandles.lookup();

            boolean [] booleans = { false, true, false };
            for (boolean b : booleans) {
                Boolean boxed = new Boolean(b);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_z == b);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_z", boolean.class),
                            valueHolder, PrimitiveType.Boolean, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_z == b);
            }

            byte [] bytes = { (byte) 0x73, (byte) 0xfe };
            for (byte b : bytes) {
                Byte boxed = new Byte(b);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_b == b);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_b", byte.class),
                            valueHolder, PrimitiveType.Byte, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_b == b);
            }

            char [] chars = { 'a', 'b', 'c' };
            for (char c : chars) {
                Character boxed = new Character(c);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_c == c);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_c", char.class),
                            valueHolder, PrimitiveType.Char, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_c == c);
            }

            short [] shorts = { (short) 0x1234, (short) 0x4321 };
            for (short s : shorts) {
                Short boxed = new Short(s);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_s == s);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_s", short.class),
                            valueHolder, PrimitiveType.Short, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_s == s);
            }

            int [] ints = { -100000000, 10000000 };
            for (int i : ints) {
                Integer boxed = new Integer(i);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_i == i);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_i", int.class),
                            valueHolder, PrimitiveType.Int, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_i == i);
            }

            float [] floats = { 0.99f, -1.23e-17f };
            for (float f : floats) {
                Float boxed = Float.valueOf(f);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_f == f);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_f", float.class),
                            valueHolder, PrimitiveType.Float, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_f == f);
            }

            double [] doubles = { 0.44444444444e37, -0.555555555e-37 };
            for (double d : doubles) {
                Double boxed = Double.valueOf(d);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_d == d);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_d", double.class),
                            valueHolder, PrimitiveType.Double, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_d == d);
            }

            long [] longs = { 0x0123456789abcdefl, 0xfedcba9876543210l };
            for (long j : longs) {
                Long boxed = new Long(j);
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.IGET);
                assertTrue(valueHolder.m_j == j);
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_j", long.class),
                            valueHolder, PrimitiveType.Long, boxed, AccessorType.SGET);
                assertTrue(ValueHolder.s_j == j);
            }

            String [] strings = { "octopus", "crab" };
            for (String s : strings) {
                tryAccessor(lookup.findSetter(ValueHolder.class, "m_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.IPUT);
                tryAccessor(lookup.findGetter(ValueHolder.class, "m_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.IGET);
                assertTrue(s.equals(valueHolder.m_l));
                tryAccessor(lookup.findStaticSetter(ValueHolder.class, "s_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.SPUT);
                tryAccessor(lookup.findStaticGetter(ValueHolder.class, "s_l", String.class),
                            valueHolder, PrimitiveType.String, s, AccessorType.SGET);
                assertTrue(s.equals(ValueHolder.s_l));
            }

            System.out.println("Passed MethodHandle.invokeExact() tests for accessors.");
        }
    }

    public static class FindAccessorTester extends Tester {
        public static void main() throws Throwable {
            // NB having a static field test here is essential for
            // this test. MethodHandles need to ensure the class
            // (ValueHolder) is initialized. This happens in the
            // invoke-polymorphic dispatch.
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            {
                MethodHandle mh = lookup.findStaticGetter(ValueHolder.class, "s_fi", int.class);
                int initialValue = (int) mh.invokeExact();
                System.out.println(initialValue);
            }
            {
                MethodHandle mh = lookup.findStaticSetter(ValueHolder.class, "s_i", int.class);
                mh.invokeExact(0);
            }
            try {
                lookup.findStaticGetter(ValueHolder.class, "s_fi", byte.class);
                fail();
            } catch (NoSuchFieldException expected) {}
            try {
                lookup.findGetter(ValueHolder.class, "s_fi", byte.class);
                fail();
            } catch (NoSuchFieldException eexpected) {}
            try {
                lookup.findStaticSetter(ValueHolder.class, "s_fi", int.class);
                fail();
            } catch (IllegalAccessException expected) {}

            lookup.findGetter(ValueHolder.class, "m_fi", int.class);
            try {
                lookup.findGetter(ValueHolder.class, "m_fi", byte.class);
                fail();
            } catch (NoSuchFieldException expected) {}
            try {
                lookup.findStaticGetter(ValueHolder.class, "m_fi", byte.class);
                fail();
            } catch (NoSuchFieldException expected) {}
            try {
                lookup.findSetter(ValueHolder.class, "m_fi", int.class);
                fail();
            } catch (IllegalAccessException expected) {}

            System.out.println("Passed MethodHandles.Lookup tests for accessors.");
        }
    }

    public static class InvokeTester extends Tester {
        private static void testStaticGetter() throws Throwable {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            MethodHandle h0 = lookup.findStaticGetter(ValueHolder.class, "s_fi", int.class);
            h0.invoke();
            Number t = (Number) h0.invoke();
            int u = (int) h0.invoke();
            Integer v = (Integer) h0.invoke();
            long w = (long) h0.invoke();
            try {
                byte x = (byte) h0.invoke();
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                String y = (String) h0.invoke();
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                Long z = (Long) h0.invoke();
                fail();
            } catch (WrongMethodTypeException expected) {}
        }

        private static void testMemberGetter() throws Throwable {
            ValueHolder valueHolder = new ValueHolder();
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            MethodHandle h0 = lookup.findGetter(ValueHolder.class, "m_fi", int.class);
            h0.invoke(valueHolder);
            Number t = (Number) h0.invoke(valueHolder);
            int u = (int) h0.invoke(valueHolder);
            Integer v = (Integer) h0.invoke(valueHolder);
            long w = (long) h0.invoke(valueHolder);
            try {
                byte x = (byte) h0.invoke(valueHolder);
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                String y = (String) h0.invoke(valueHolder);
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                Long z = (Long) h0.invoke(valueHolder);
                fail();
            } catch (WrongMethodTypeException expected) {}
        }

        /*package*/ static Number getDoubleAsNumber() {
            return Double.valueOf(1.4e77);
        }
        /*package*/ static Number getFloatAsNumber() {
            return Float.valueOf(7.77f);
        }
        /*package*/ static Object getFloatAsObject() {
            return Float.valueOf(-7.77f);
        }

        private static void testMemberSetter() throws Throwable {
            ValueHolder valueHolder = new ValueHolder();
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            MethodHandle h0 = lookup.findSetter(ValueHolder.class, "m_f", float.class);
            MethodHandle s0 = lookup.findSetter(ValueHolder.class, "m_s", short.class);
            h0.invoke(valueHolder, 0.22f);
            h0.invoke(valueHolder, Float.valueOf(1.11f));
            Number floatNumber = getFloatAsNumber();
            h0.invoke(valueHolder, floatNumber);
            assertTrue(valueHolder.m_f == floatNumber.floatValue());
            Object objNumber = getFloatAsObject();
            h0.invoke(valueHolder, objNumber);
            assertTrue(valueHolder.m_f == ((Float) objNumber).floatValue());
            try {
              h0.invoke(valueHolder, (Float) null);
              fail();
            } catch (NullPointerException expected) {}

            // Test that type conversion checks work on small field types.
            short temp = (short) s0.invoke(valueHolder, new Byte((byte) 45));
            assertTrue(temp == 0);
            assertTrue(valueHolder.m_s == 45);

            h0.invoke(valueHolder, (byte) 1);
            h0.invoke(valueHolder, (short) 2);
            h0.invoke(valueHolder, 3);
            h0.invoke(valueHolder, 4l);

            assertTrue(null == (Object) h0.invoke(valueHolder, 33));
            assertTrue(0.0f == (float) h0.invoke(valueHolder, 33));
            assertTrue(0l == (long) h0.invoke(valueHolder, 33));

            try {
                h0.invoke(valueHolder, 0.33);
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                Number doubleNumber = getDoubleAsNumber();
                h0.invoke(valueHolder, doubleNumber);
                fail();
            } catch (ClassCastException expected) {}
            try {
                Number doubleNumber = null;
                h0.invoke(valueHolder, doubleNumber);
                fail();
            } catch (NullPointerException expected) {}
            {
                // Mismatched return type - float != void
                float tmp = (float) h0.invoke(valueHolder, 0.45f);
                assertTrue(tmp == 0.0);
            }
            try {
                h0.invoke(valueHolder, "bam");
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                String s = null;
                h0.invoke(valueHolder, s);
                fail();
            } catch (WrongMethodTypeException expected) {}
        }

        private static void testStaticSetter() throws Throwable {
            MethodHandles.Lookup lookup = MethodHandles.lookup();
            MethodHandle s0 = lookup.findStaticSetter(ValueHolder.class, "s_s", short.class);
            MethodHandle h0 = lookup.findStaticSetter(ValueHolder.class, "s_f", float.class);
            h0.invoke(0.22f);
            h0.invoke(Float.valueOf(1.11f));
            Number floatNumber = Float.valueOf(0.88f);
            h0.invoke(floatNumber);
            assertTrue(ValueHolder.s_f == floatNumber.floatValue());

            try {
              h0.invoke((Float) null);
              fail();
            } catch (NullPointerException expected) {}

            // Test that type conversion checks work on small field types.
            short temp = (short) s0.invoke(new Byte((byte) 45));
            assertTrue(temp == 0);
            assertTrue(ValueHolder.s_s == 45);

            h0.invoke((byte) 1);
            h0.invoke((short) 2);
            h0.invoke(3);
            h0.invoke(4l);

            assertTrue(null == (Object) h0.invoke(33));
            assertTrue(0.0f == (float) h0.invoke(33));
            assertTrue(0l == (long) h0.invoke(33));

            try {
                h0.invoke(0.33);
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                h0.invoke(Double.valueOf(0.33));
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                Number doubleNumber = getDoubleAsNumber();
                h0.invoke(doubleNumber);
                fail();
            } catch (ClassCastException expected) {}
            try {
                Number doubleNumber = Double.valueOf(1.01);
                doubleNumber = (doubleNumber.doubleValue() != 0.1) ? null : doubleNumber;
                h0.invoke(doubleNumber);
                fail();
            } catch (NullPointerException expected) {}
            try {
                // Mismatched return type - float != void
                float tmp = (float) h0.invoke(0.45f);
                assertTrue(tmp == 0.0);
            } catch (Exception e) { fail(); }
            try {
                h0.invoke("bam");
                fail();
            } catch (WrongMethodTypeException expected) {}
            try {
                String s = null;
                h0.invoke(s);
                fail();
            } catch (WrongMethodTypeException expected) {}
        }

        public static void main() throws Throwable{
            testStaticGetter();
            testMemberGetter();
            testStaticSetter();
            testMemberSetter();
            System.out.println("Passed MethodHandle.invoke() tests for accessors.");
        }
    }

    public static class UnreflectTester extends Tester {
        public static void main() throws Throwable {
            ValueHolder v = new ValueHolder();
            {
                // public field test
                Field f = ValueHolder.class.getDeclaredField("m_c");
                MethodHandles.lookup().unreflectSetter(f).invokeExact(v, 'z');
                assertEquals('z', (char) MethodHandles.lookup().unreflectGetter(f).invokeExact(v));
                MethodHandles.lookup().unreflectSetter(f).invokeExact(v, 'A');
                assertEquals('A', (char) MethodHandles.lookup().unreflectGetter(f).invokeExact(v));
            }
            {
                // public static final field test
                Field f = ValueHolder.class.getDeclaredField("s_fi");
                try {
                    MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                MethodHandles.lookup().unreflectGetter(f);
                f.setAccessible(true);
                int savedValue = (int) MethodHandles.lookup().unreflectGetter(f).invokeExact();
                int newValue = savedValue + 1;
                MethodHandles.lookup().unreflectSetter(f).invokeExact(newValue);
                assertEquals(newValue, (int) MethodHandles.lookup().unreflectGetter(f).invokeExact()
                );
                MethodHandles.lookup().unreflectSetter(f).invokeExact(savedValue);
                assertEquals(savedValue, (int) MethodHandles.lookup().unreflectGetter(f).invokeExact()
                );
                f.setAccessible(false);
                try {
                    MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                MethodHandles.lookup().unreflectGetter(f);
            }
            {
                // private field test
                Field f = ValueHolder.class.getDeclaredField("m_pz");
                try {
                    MethodHandle mh = MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                try {
                    MethodHandle mh = MethodHandles.lookup().unreflectGetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                f.setAccessible(true);
                MethodHandles.lookup().unreflectSetter(f).invokeExact(v, true);
                assertEquals(true, (boolean) MethodHandles.lookup().unreflectGetter(f).invokeExact(v)
                );
                MethodHandles.lookup().unreflectSetter(f).invokeExact(v, false);
                assertEquals(false, (boolean) MethodHandles.lookup().unreflectGetter(f).invokeExact(v)
                );
                f.setAccessible(false);
                try {
                    MethodHandle mh = MethodHandles.lookup().unreflectGetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                try {
                    MethodHandle mh = MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
            }
            {
                // private static final field test
                Field f = ValueHolder.class.getDeclaredField("s_fz");  // private static final field
                try {
                    MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                try {
                    MethodHandles.lookup().unreflectGetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                f.setAccessible(true);
                // Setter is okay despite being final because field isAccessible().
                MethodHandles.lookup().unreflectSetter(f).invokeExact(false);
                assertEquals(false, (boolean) MethodHandles.lookup().unreflectGetter(f).invokeExact()
                );
                MethodHandles.lookup().unreflectSetter(f).invokeExact(true);
                assertEquals(true, (boolean) MethodHandles.lookup().unreflectGetter(f).invokeExact()
                );
                f.setAccessible(false);
                try {
                    MethodHandles.lookup().unreflectSetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
                try {
                    MethodHandles.lookup().unreflectGetter(f);
                    fail();
                } catch (IllegalAccessException expected) {}
            }
            System.out.println("Passed MethodHandles.unreflect(Field) tests.");
        }
    }

    public static void main(String[] args) throws Throwable {
        // FindAccessor test should be the first test class in this
        // file to ensure class initialization test is run.
        FindAccessorTester.main();
        InvokeExactTester.main();
        InvokeTester.main();
        UnreflectTester.main();
    }
}
