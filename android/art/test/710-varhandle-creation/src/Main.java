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
import java.lang.invoke.VarHandle.AccessMode;
import java.lang.reflect.Field;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;

public final class Main {
    // Mutable fields
    boolean z;
    byte b;
    char c;
    short s;
    int i;
    long j;
    float f;
    double d;
    Object o;

    // Final fields
    final boolean fz = true;
    final byte fb = (byte) 2;
    final char fc = 'c';
    final short fs = (short) 3;
    final int fi = 4;
    final long fj = 5;
    final float ff = 6.0f;
    final double fd = 7.0;
    final Object fo = "Hello";
    final String fss = "Boo";

    // Static fields
    static boolean sz;
    static byte sb;
    static char sc;
    static short ss;
    static int si;
    static long sj;
    static float sf;
    static double sd;
    static Object so;

    // VarHandle instances for mutable fields
    static final VarHandle vz;
    static final VarHandle vb;
    static final VarHandle vc;
    static final VarHandle vs;
    static final VarHandle vi;
    static final VarHandle vj;
    static final VarHandle vf;
    static final VarHandle vd;
    static final VarHandle vo;

    // VarHandle instances for final fields
    static final VarHandle vfz;
    static final VarHandle vfb;
    static final VarHandle vfc;
    static final VarHandle vfs;
    static final VarHandle vfi;
    static final VarHandle vfj;
    static final VarHandle vff;
    static final VarHandle vfd;
    static final VarHandle vfo;
    static final VarHandle vfss;

    // VarHandle instances for static fields
    static final VarHandle vsz;
    static final VarHandle vsb;
    static final VarHandle vsc;
    static final VarHandle vss;
    static final VarHandle vsi;
    static final VarHandle vsj;
    static final VarHandle vsf;
    static final VarHandle vsd;
    static final VarHandle vso;

    // VarHandle instances for array elements
    static final VarHandle vaz;
    static final VarHandle vab;
    static final VarHandle vac;
    static final VarHandle vas;
    static final VarHandle vai;
    static final VarHandle vaj;
    static final VarHandle vaf;
    static final VarHandle vad;
    static final VarHandle vao;

    // VarHandle instances for byte array view
    static final VarHandle vbaz;
    static final VarHandle vbab;
    static final VarHandle vbac;
    static final VarHandle vbas;
    static final VarHandle vbai;
    static final VarHandle vbaj;
    static final VarHandle vbaf;
    static final VarHandle vbad;
    static final VarHandle vbao;

    // VarHandle instances for byte buffer view
    static final VarHandle vbbz;
    static final VarHandle vbbb;
    static final VarHandle vbbc;
    static final VarHandle vbbs;
    static final VarHandle vbbi;
    static final VarHandle vbbj;
    static final VarHandle vbbf;
    static final VarHandle vbbd;
    static final VarHandle vbbo;

    static {
        try {
            vz = MethodHandles.lookup().findVarHandle(Main.class, "z", boolean.class);
            vb = MethodHandles.lookup().findVarHandle(Main.class, "b", byte.class);
            vc = MethodHandles.lookup().findVarHandle(Main.class, "c", char.class);
            vs = MethodHandles.lookup().findVarHandle(Main.class, "s", short.class);
            vi = MethodHandles.lookup().findVarHandle(Main.class, "i", int.class);
            vj = MethodHandles.lookup().findVarHandle(Main.class, "j", long.class);
            vf = MethodHandles.lookup().findVarHandle(Main.class, "f", float.class);
            vd = MethodHandles.lookup().findVarHandle(Main.class, "d", double.class);
            vo = MethodHandles.lookup().findVarHandle(Main.class, "o", Object.class);

            vfz = MethodHandles.lookup().findVarHandle(Main.class, "fz", boolean.class);
            vfb = MethodHandles.lookup().findVarHandle(Main.class, "fb", byte.class);
            vfc = MethodHandles.lookup().findVarHandle(Main.class, "fc", char.class);
            vfs = MethodHandles.lookup().findVarHandle(Main.class, "fs", short.class);
            vfi = MethodHandles.lookup().findVarHandle(Main.class, "fi", int.class);
            vfj = MethodHandles.lookup().findVarHandle(Main.class, "fj", long.class);
            vff = MethodHandles.lookup().findVarHandle(Main.class, "ff", float.class);
            vfd = MethodHandles.lookup().findVarHandle(Main.class, "fd", double.class);
            vfo = MethodHandles.lookup().findVarHandle(Main.class, "fo", Object.class);
            vfss = MethodHandles.lookup().findVarHandle(Main.class, "fss", String.class);

            vsz = MethodHandles.lookup().findStaticVarHandle(Main.class, "sz", boolean.class);
            vsb = MethodHandles.lookup().findStaticVarHandle(Main.class, "sb", byte.class);
            vsc = MethodHandles.lookup().findStaticVarHandle(Main.class, "sc", char.class);
            vss = MethodHandles.lookup().findStaticVarHandle(Main.class, "ss", short.class);
            vsi = MethodHandles.lookup().findStaticVarHandle(Main.class, "si", int.class);
            vsj = MethodHandles.lookup().findStaticVarHandle(Main.class, "sj", long.class);
            vsf = MethodHandles.lookup().findStaticVarHandle(Main.class, "sf", float.class);
            vsd = MethodHandles.lookup().findStaticVarHandle(Main.class, "sd", double.class);
            vso = MethodHandles.lookup().findStaticVarHandle(Main.class, "so", Object.class);

            vaz = MethodHandles.arrayElementVarHandle(boolean[].class);
            vab = MethodHandles.arrayElementVarHandle(byte[].class);
            vac = MethodHandles.arrayElementVarHandle(char[].class);
            vas = MethodHandles.arrayElementVarHandle(short[].class);
            vai = MethodHandles.arrayElementVarHandle(int[].class);
            vaj = MethodHandles.arrayElementVarHandle(long[].class);
            vaf = MethodHandles.arrayElementVarHandle(float[].class);
            vad = MethodHandles.arrayElementVarHandle(double[].class);
            vao = MethodHandles.arrayElementVarHandle(Object[].class);

            try {
                MethodHandles.byteArrayViewVarHandle(boolean[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbaz = null;
            }
            try {
                MethodHandles.byteArrayViewVarHandle(byte[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbab = null;
            }
            vbac = MethodHandles.byteArrayViewVarHandle(char[].class, ByteOrder.LITTLE_ENDIAN);
            vbas = MethodHandles.byteArrayViewVarHandle(short[].class, ByteOrder.BIG_ENDIAN);
            vbai = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            vbaj = MethodHandles.byteArrayViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
            vbaf = MethodHandles.byteArrayViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
            vbad = MethodHandles.byteArrayViewVarHandle(double[].class, ByteOrder.BIG_ENDIAN);
            try {
                MethodHandles.byteArrayViewVarHandle(Object[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbao = null;
            }

            try {
                MethodHandles.byteBufferViewVarHandle(boolean[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbbz = null;
            }
            try {
                MethodHandles.byteBufferViewVarHandle(byte[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbbb = null;
            }
            vbbc = MethodHandles.byteBufferViewVarHandle(char[].class, ByteOrder.LITTLE_ENDIAN);
            vbbs = MethodHandles.byteBufferViewVarHandle(short[].class, ByteOrder.BIG_ENDIAN);
            vbbi = MethodHandles.byteBufferViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
            vbbj = MethodHandles.byteBufferViewVarHandle(long[].class, ByteOrder.LITTLE_ENDIAN);
            vbbf = MethodHandles.byteBufferViewVarHandle(float[].class, ByteOrder.LITTLE_ENDIAN);
            vbbd = MethodHandles.byteBufferViewVarHandle(double[].class, ByteOrder.BIG_ENDIAN);
            try {
                MethodHandles.byteBufferViewVarHandle(Object[].class, ByteOrder.LITTLE_ENDIAN);
                throw new RuntimeException("Unexpected instantiation");
            } catch (UnsupportedOperationException e) {
                vbbo = null;
            }
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static void fail(String reason) {
        throw new RuntimeException("FAIL: " + reason);
    }

    private static void checkNull(VarHandle v) {
        if (v != null) {
            fail("Instance unexpectedly not null:" + v);
        }
    }

    private static void checkNotNull(VarHandle v) {
        if (v == null) {
            fail("Instance unexpectedly null:" + v);
        }
    }

    private static void checkVarType(VarHandle v, Class<?> expectedVarType) {
        if (v.varType() != expectedVarType) {
            fail("varType " + v.varType() + " != " + expectedVarType);
        }
    }

    private static void checkCoordinateTypes(VarHandle v, String expectedCoordinateTypes) {
        String actualCoordinateTypes = Arrays.toString(v.coordinateTypes().toArray());
        if (!actualCoordinateTypes.equals(expectedCoordinateTypes)) {
            fail("coordinateTypes " + actualCoordinateTypes + " != " + expectedCoordinateTypes);
        }
    }

    private static void checkVarHandleAccessMode(VarHandle v, VarHandle.AccessMode accessMode,
                                                 boolean expectedSupported, String expectedAccessModeType) {
        boolean actualSupported = v.isAccessModeSupported(accessMode);
        if (actualSupported != expectedSupported) {
            fail("isAccessModeSupported(" + accessMode + ") is " +
                 actualSupported + " != " + expectedSupported);
        }

        String actualAccessModeType = v.accessModeType(accessMode).toString();
        if (!actualAccessModeType.equals(expectedAccessModeType)) {
            fail("accessModeType(" + accessMode + ") is " +
                 actualAccessModeType + " != " + expectedAccessModeType);
        }
    }

    private static void checkInstantiatedVarHandles() {
        System.out.print("checkInstantiatedVarHandles...");

        System.out.print("vz...");
        checkNotNull(vz);
        checkVarType(vz, boolean.class);
        checkCoordinateTypes(vz, "[class Main]");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET, true, "(Main)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.SET, true, "(Main,boolean)void");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,boolean)void");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.SET_RELEASE, true, "(Main,boolean)void");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,boolean)void");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_SET, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,boolean)boolean");

        System.out.print("vb...");
        checkNotNull(vb);
        checkVarType(vb, byte.class);
        checkCoordinateTypes(vb, "[class Main]");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET, true, "(Main)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.SET, true, "(Main,byte)void");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,byte)void");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.SET_RELEASE, true, "(Main,byte)void");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,byte)void");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_SET, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,byte)byte");
        checkVarHandleAccessMode(vb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,byte)byte");

        System.out.print("vc...");
        checkNotNull(vc);
        checkVarType(vc, char.class);
        checkCoordinateTypes(vc, "[class Main]");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET, true, "(Main)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.SET, true, "(Main,char)void");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,char)void");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.SET_RELEASE, true, "(Main,char)void");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,char)void");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,char,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,char,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,char,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_SET, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,char)char");
        checkVarHandleAccessMode(vc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,char)char");

        System.out.print("vs...");
        checkNotNull(vs);
        checkVarType(vs, short.class);
        checkCoordinateTypes(vs, "[class Main]");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET, true, "(Main)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.SET, true, "(Main,short)void");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,short)void");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.SET_RELEASE, true, "(Main,short)void");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,short)void");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,short,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,short,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,short,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_SET, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,short)short");
        checkVarHandleAccessMode(vs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,short)short");

        System.out.print("vi...");
        checkNotNull(vi);
        checkVarType(vi, int.class);
        checkCoordinateTypes(vi, "[class Main]");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET, true, "(Main)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.SET, true, "(Main,int)void");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,int)void");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.SET_RELEASE, true, "(Main,int)void");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,int)void");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,int,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,int,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,int,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_SET, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,int)int");
        checkVarHandleAccessMode(vi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,int)int");

        System.out.print("vj...");
        checkNotNull(vj);
        checkVarType(vj, long.class);
        checkCoordinateTypes(vj, "[class Main]");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET, true, "(Main)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.SET, true, "(Main,long)void");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,long)void");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.SET_RELEASE, true, "(Main,long)void");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,long)void");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,long,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,long,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,long,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_SET, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(Main,long)long");
        checkVarHandleAccessMode(vj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(Main,long)long");

        System.out.print("vf...");
        checkNotNull(vf);
        checkVarType(vf, float.class);
        checkCoordinateTypes(vf, "[class Main]");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET, true, "(Main)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.SET, true, "(Main,float)void");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,float)void");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.SET_RELEASE, true, "(Main,float)void");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,float)void");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,float,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,float,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,float,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_SET, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,float)float");

        System.out.print("vd...");
        checkNotNull(vd);
        checkVarType(vd, double.class);
        checkCoordinateTypes(vd, "[class Main]");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET, true, "(Main)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.SET, true, "(Main,double)void");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,double)void");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.SET_RELEASE, true, "(Main,double)void");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,double)void");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,double,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,double,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,double,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_SET, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_ADD, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,double)double");

        System.out.print("vo...");
        checkNotNull(vo);
        checkVarType(vo, Object.class);
        checkCoordinateTypes(vo, "[class Main]");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET, true, "(Main)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.SET, true, "(Main,Object)void");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.SET_VOLATILE, true, "(Main,Object)void");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.SET_RELEASE, true, "(Main,Object)void");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.SET_OPAQUE, true, "(Main,Object)void");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_SET, true, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vo, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,Object)Object");

        System.out.print("vfz...");
        checkNotNull(vfz);
        checkVarType(vfz, boolean.class);
        checkCoordinateTypes(vfz, "[class Main]");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET, true, "(Main)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.SET, false, "(Main,boolean)void");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,boolean)void");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.SET_RELEASE, false, "(Main,boolean)void");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,boolean)void");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,boolean,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_SET, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,boolean)boolean");
        checkVarHandleAccessMode(vfz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,boolean)boolean");

        System.out.print("vfb...");
        checkNotNull(vfb);
        checkVarType(vfb, byte.class);
        checkCoordinateTypes(vfb, "[class Main]");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET, true, "(Main)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.SET, false, "(Main,byte)void");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,byte)void");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.SET_RELEASE, false, "(Main,byte)void");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,byte)void");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,byte,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,byte,byte)boolean");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_SET, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,byte)byte");
        checkVarHandleAccessMode(vfb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,byte)byte");

        System.out.print("vfc...");
        checkNotNull(vfc);
        checkVarType(vfc, char.class);
        checkCoordinateTypes(vfc, "[class Main]");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET, true, "(Main)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.SET, false, "(Main,char)void");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,char)void");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.SET_RELEASE, false, "(Main,char)void");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,char)void");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,char,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,char,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,char,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,char,char)boolean");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_SET, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,char)char");
        checkVarHandleAccessMode(vfc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,char)char");

        System.out.print("vfs...");
        checkNotNull(vfs);
        checkVarType(vfs, short.class);
        checkCoordinateTypes(vfs, "[class Main]");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET, true, "(Main)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.SET, false, "(Main,short)void");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,short)void");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.SET_RELEASE, false, "(Main,short)void");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,short)void");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,short,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,short,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,short,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,short,short)boolean");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_SET, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,short)short");
        checkVarHandleAccessMode(vfs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,short)short");

        System.out.print("vfi...");
        checkNotNull(vfi);
        checkVarType(vfi, int.class);
        checkCoordinateTypes(vfi, "[class Main]");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET, true, "(Main)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.SET, false, "(Main,int)void");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,int)void");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.SET_RELEASE, false, "(Main,int)void");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,int)void");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,int,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,int,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,int,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,int,int)boolean");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_SET, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,int)int");
        checkVarHandleAccessMode(vfi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,int)int");

        System.out.print("vfj...");
        checkNotNull(vfj);
        checkVarType(vfj, long.class);
        checkCoordinateTypes(vfj, "[class Main]");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET, true, "(Main)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.SET, false, "(Main,long)void");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,long)void");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.SET_RELEASE, false, "(Main,long)void");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,long)void");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,long,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,long,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,long,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,long,long)boolean");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_SET, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,long)long");
        checkVarHandleAccessMode(vfj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,long)long");

        System.out.print("vff...");
        checkNotNull(vff);
        checkVarType(vff, float.class);
        checkCoordinateTypes(vff, "[class Main]");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET, true, "(Main)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.SET, false, "(Main,float)void");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,float)void");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.SET_RELEASE, false, "(Main,float)void");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,float)void");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,float,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,float,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,float,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,float,float)boolean");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_SET, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,float)float");
        checkVarHandleAccessMode(vff, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,float)float");

        System.out.print("vfd...");
        checkNotNull(vfd);
        checkVarType(vfd, double.class);
        checkCoordinateTypes(vfd, "[class Main]");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET, true, "(Main)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.SET, false, "(Main,double)void");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,double)void");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.SET_RELEASE, false, "(Main,double)void");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,double)void");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,double,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,double,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,double,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,double,double)boolean");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_SET, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,double)double");
        checkVarHandleAccessMode(vfd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,double)double");

        System.out.print("vfo...");
        checkNotNull(vfo);
        checkVarType(vfo, Object.class);
        checkCoordinateTypes(vfo, "[class Main]");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET, true, "(Main)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.SET, false, "(Main,Object)void");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,Object)void");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.SET_RELEASE, false, "(Main,Object)void");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,Object)void");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,Object,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,Object,Object)boolean");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_SET, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,Object)Object");
        checkVarHandleAccessMode(vfo, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,Object)Object");

        System.out.print("vfss...");
        checkNotNull(vfss);
        checkVarType(vfss, java.lang.String.class);
        checkCoordinateTypes(vfss, "[class Main]");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET, true, "(Main)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.SET, false, "(Main,String)void");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_VOLATILE, true, "(Main)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.SET_VOLATILE, false, "(Main,String)void");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_ACQUIRE, true, "(Main)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.SET_RELEASE, false, "(Main,String)void");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_OPAQUE, true, "(Main)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.SET_OPAQUE, false, "(Main,String)void");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.COMPARE_AND_SET, false, "(Main,String,String)boolean");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(Main,String,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(Main,String,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(Main,String,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(Main,String,String)boolean");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(Main,String,String)boolean");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(Main,String,String)boolean");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(Main,String,String)boolean");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_SET, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_ADD, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Main,String)String");
        checkVarHandleAccessMode(vfss, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Main,String)String");

        System.out.print("vsz...");
        checkNotNull(vsz);
        checkVarType(vsz, boolean.class);
        checkCoordinateTypes(vsz, "[]");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET, true, "()boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.SET, true, "(boolean)void");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_VOLATILE, true, "()boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.SET_VOLATILE, true, "(boolean)void");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_ACQUIRE, true, "()boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.SET_RELEASE, true, "(boolean)void");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_OPAQUE, true, "()boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.SET_OPAQUE, true, "(boolean)void");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.COMPARE_AND_SET, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(boolean,boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_SET, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_ADD, false, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(boolean)boolean");
        checkVarHandleAccessMode(vsz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(boolean)boolean");

        System.out.print("vsb...");
        checkNotNull(vsb);
        checkVarType(vsb, byte.class);
        checkCoordinateTypes(vsb, "[]");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET, true, "()byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.SET, true, "(byte)void");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_VOLATILE, true, "()byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.SET_VOLATILE, true, "(byte)void");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_ACQUIRE, true, "()byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.SET_RELEASE, true, "(byte)void");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_OPAQUE, true, "()byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.SET_OPAQUE, true, "(byte)void");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte,byte)boolean");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte,byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte,byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte,byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte,byte)boolean");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte,byte)boolean");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte,byte)boolean");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte,byte)boolean");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_SET, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_ADD, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(byte)byte");
        checkVarHandleAccessMode(vsb, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(byte)byte");

        System.out.print("vsc...");
        checkNotNull(vsc);
        checkVarType(vsc, char.class);
        checkCoordinateTypes(vsc, "[]");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET, true, "()char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.SET, true, "(char)void");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_VOLATILE, true, "()char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.SET_VOLATILE, true, "(char)void");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_ACQUIRE, true, "()char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.SET_RELEASE, true, "(char)void");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_OPAQUE, true, "()char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.SET_OPAQUE, true, "(char)void");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.COMPARE_AND_SET, true, "(char,char)boolean");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(char,char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(char,char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(char,char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(char,char)boolean");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(char,char)boolean");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(char,char)boolean");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(char,char)boolean");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_SET, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_ADD, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(char)char");
        checkVarHandleAccessMode(vsc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(char)char");

        System.out.print("vss...");
        checkNotNull(vss);
        checkVarType(vss, short.class);
        checkCoordinateTypes(vss, "[]");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET, true, "()short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.SET, true, "(short)void");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_VOLATILE, true, "()short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.SET_VOLATILE, true, "(short)void");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_ACQUIRE, true, "()short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.SET_RELEASE, true, "(short)void");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_OPAQUE, true, "()short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.SET_OPAQUE, true, "(short)void");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.COMPARE_AND_SET, true, "(short,short)boolean");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(short,short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(short,short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(short,short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(short,short)boolean");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(short,short)boolean");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(short,short)boolean");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(short,short)boolean");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_SET, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_ADD, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(short)short");
        checkVarHandleAccessMode(vss, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(short)short");

        System.out.print("vsi...");
        checkNotNull(vsi);
        checkVarType(vsi, int.class);
        checkCoordinateTypes(vsi, "[]");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET, true, "()int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.SET, true, "(int)void");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_VOLATILE, true, "()int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.SET_VOLATILE, true, "(int)void");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_ACQUIRE, true, "()int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.SET_RELEASE, true, "(int)void");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_OPAQUE, true, "()int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.SET_OPAQUE, true, "(int)void");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.COMPARE_AND_SET, true, "(int,int)boolean");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(int,int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(int,int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(int,int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(int,int)boolean");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(int,int)boolean");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(int,int)boolean");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(int,int)boolean");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_SET, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_ADD, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(int)int");
        checkVarHandleAccessMode(vsi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(int)int");

        System.out.print("vsj...");
        checkNotNull(vsj);
        checkVarType(vsj, long.class);
        checkCoordinateTypes(vsj, "[]");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET, true, "()long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.SET, true, "(long)void");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_VOLATILE, true, "()long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.SET_VOLATILE, true, "(long)void");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_ACQUIRE, true, "()long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.SET_RELEASE, true, "(long)void");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_OPAQUE, true, "()long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.SET_OPAQUE, true, "(long)void");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.COMPARE_AND_SET, true, "(long,long)boolean");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(long,long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(long,long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(long,long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(long,long)boolean");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(long,long)boolean");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(long,long)boolean");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(long,long)boolean");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_SET, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_ADD, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(long)long");
        checkVarHandleAccessMode(vsj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(long)long");

        System.out.print("vsf...");
        checkNotNull(vsf);
        checkVarType(vsf, float.class);
        checkCoordinateTypes(vsf, "[]");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET, true, "()float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.SET, true, "(float)void");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_VOLATILE, true, "()float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.SET_VOLATILE, true, "(float)void");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_ACQUIRE, true, "()float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.SET_RELEASE, true, "(float)void");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_OPAQUE, true, "()float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.SET_OPAQUE, true, "(float)void");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.COMPARE_AND_SET, true, "(float,float)boolean");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(float,float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(float,float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(float,float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(float,float)boolean");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(float,float)boolean");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(float,float)boolean");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(float,float)boolean");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_SET, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_ADD, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(float)float");
        checkVarHandleAccessMode(vsf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(float)float");

        System.out.print("vsd...");
        checkNotNull(vsd);
        checkVarType(vsd, double.class);
        checkCoordinateTypes(vsd, "[]");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET, true, "()double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.SET, true, "(double)void");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_VOLATILE, true, "()double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.SET_VOLATILE, true, "(double)void");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_ACQUIRE, true, "()double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.SET_RELEASE, true, "(double)void");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_OPAQUE, true, "()double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.SET_OPAQUE, true, "(double)void");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.COMPARE_AND_SET, true, "(double,double)boolean");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(double,double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(double,double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(double,double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(double,double)boolean");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(double,double)boolean");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(double,double)boolean");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(double,double)boolean");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_SET, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_ADD, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(double)double");
        checkVarHandleAccessMode(vsd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(double)double");

        System.out.print("vso...");
        checkNotNull(vso);
        checkVarType(vso, Object.class);
        checkCoordinateTypes(vso, "[]");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET, true, "()Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.SET, true, "(Object)void");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_VOLATILE, true, "()Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.SET_VOLATILE, true, "(Object)void");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_ACQUIRE, true, "()Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.SET_RELEASE, true, "(Object)void");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_OPAQUE, true, "()Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.SET_OPAQUE, true, "(Object)void");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Object,Object)boolean");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Object,Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Object,Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Object,Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Object,Object)boolean");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Object,Object)boolean");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Object,Object)boolean");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Object,Object)boolean");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_SET, true, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_ADD, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Object)Object");
        checkVarHandleAccessMode(vso, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Object)Object");

        System.out.print("vaz...");
        checkNotNull(vaz);
        checkVarType(vaz, boolean.class);
        checkCoordinateTypes(vaz, "[class [Z, int]");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET, true, "(boolean[],int)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.SET, true, "(boolean[],int,boolean)void");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_VOLATILE, true, "(boolean[],int)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.SET_VOLATILE, true, "(boolean[],int,boolean)void");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_ACQUIRE, true, "(boolean[],int)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.SET_RELEASE, true, "(boolean[],int,boolean)void");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_OPAQUE, true, "(boolean[],int)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.SET_OPAQUE, true, "(boolean[],int,boolean)void");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.COMPARE_AND_SET, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(boolean[],int,boolean,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_SET, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_ADD, false, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(boolean[],int,boolean)boolean");
        checkVarHandleAccessMode(vaz, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(boolean[],int,boolean)boolean");

        System.out.print("vab...");
        checkNotNull(vab);
        checkVarType(vab, byte.class);
        checkCoordinateTypes(vab, "[class [B, int]");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET, true, "(byte[],int)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.SET, true, "(byte[],int,byte)void");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,byte)void");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,byte)void");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,byte)void");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte[],int,byte,byte)boolean");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte[],int,byte,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte[],int,byte,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte[],int,byte,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte[],int,byte,byte)boolean");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte[],int,byte,byte)boolean");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte[],int,byte,byte)boolean");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte[],int,byte,byte)boolean");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_SET, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_ADD, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(byte[],int,byte)byte");
        checkVarHandleAccessMode(vab, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(byte[],int,byte)byte");

        System.out.print("vac...");
        checkNotNull(vac);
        checkVarType(vac, char.class);
        checkCoordinateTypes(vac, "[class [C, int]");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET, true, "(char[],int)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.SET, true, "(char[],int,char)void");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_VOLATILE, true, "(char[],int)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.SET_VOLATILE, true, "(char[],int,char)void");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_ACQUIRE, true, "(char[],int)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.SET_RELEASE, true, "(char[],int,char)void");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_OPAQUE, true, "(char[],int)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.SET_OPAQUE, true, "(char[],int,char)void");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.COMPARE_AND_SET, true, "(char[],int,char,char)boolean");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(char[],int,char,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(char[],int,char,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(char[],int,char,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(char[],int,char,char)boolean");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(char[],int,char,char)boolean");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(char[],int,char,char)boolean");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(char[],int,char,char)boolean");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_SET, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_ADD, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(char[],int,char)char");
        checkVarHandleAccessMode(vac, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(char[],int,char)char");

        System.out.print("vas...");
        checkNotNull(vas);
        checkVarType(vas, short.class);
        checkCoordinateTypes(vas, "[class [S, int]");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET, true, "(short[],int)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.SET, true, "(short[],int,short)void");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_VOLATILE, true, "(short[],int)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.SET_VOLATILE, true, "(short[],int,short)void");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_ACQUIRE, true, "(short[],int)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.SET_RELEASE, true, "(short[],int,short)void");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_OPAQUE, true, "(short[],int)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.SET_OPAQUE, true, "(short[],int,short)void");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.COMPARE_AND_SET, true, "(short[],int,short,short)boolean");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(short[],int,short,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(short[],int,short,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(short[],int,short,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(short[],int,short,short)boolean");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(short[],int,short,short)boolean");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(short[],int,short,short)boolean");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(short[],int,short,short)boolean");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_SET, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_ADD, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(short[],int,short)short");
        checkVarHandleAccessMode(vas, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(short[],int,short)short");

        System.out.print("vai...");
        checkNotNull(vai);
        checkVarType(vai, int.class);
        checkCoordinateTypes(vai, "[class [I, int]");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET, true, "(int[],int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.SET, true, "(int[],int,int)void");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_VOLATILE, true, "(int[],int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.SET_VOLATILE, true, "(int[],int,int)void");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_ACQUIRE, true, "(int[],int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.SET_RELEASE, true, "(int[],int,int)void");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_OPAQUE, true, "(int[],int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.SET_OPAQUE, true, "(int[],int,int)void");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.COMPARE_AND_SET, true, "(int[],int,int,int)boolean");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(int[],int,int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(int[],int,int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(int[],int,int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(int[],int,int,int)boolean");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(int[],int,int,int)boolean");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(int[],int,int,int)boolean");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(int[],int,int,int)boolean");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_SET, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_ADD, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(int[],int,int)int");
        checkVarHandleAccessMode(vai, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(int[],int,int)int");

        System.out.print("vaj...");
        checkNotNull(vaj);
        checkVarType(vaj, long.class);
        checkCoordinateTypes(vaj, "[class [J, int]");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET, true, "(long[],int)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.SET, true, "(long[],int,long)void");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_VOLATILE, true, "(long[],int)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.SET_VOLATILE, true, "(long[],int,long)void");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_ACQUIRE, true, "(long[],int)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.SET_RELEASE, true, "(long[],int,long)void");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_OPAQUE, true, "(long[],int)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.SET_OPAQUE, true, "(long[],int,long)void");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.COMPARE_AND_SET, true, "(long[],int,long,long)boolean");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(long[],int,long,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(long[],int,long,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(long[],int,long,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(long[],int,long,long)boolean");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(long[],int,long,long)boolean");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(long[],int,long,long)boolean");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(long[],int,long,long)boolean");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_SET, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_ADD, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(long[],int,long)long");
        checkVarHandleAccessMode(vaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(long[],int,long)long");

        System.out.print("vaf...");
        checkNotNull(vaf);
        checkVarType(vaf, float.class);
        checkCoordinateTypes(vaf, "[class [F, int]");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET, true, "(float[],int)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.SET, true, "(float[],int,float)void");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_VOLATILE, true, "(float[],int)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.SET_VOLATILE, true, "(float[],int,float)void");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_ACQUIRE, true, "(float[],int)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.SET_RELEASE, true, "(float[],int,float)void");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_OPAQUE, true, "(float[],int)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.SET_OPAQUE, true, "(float[],int,float)void");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.COMPARE_AND_SET, true, "(float[],int,float,float)boolean");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(float[],int,float,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(float[],int,float,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(float[],int,float,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(float[],int,float,float)boolean");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(float[],int,float,float)boolean");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(float[],int,float,float)boolean");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(float[],int,float,float)boolean");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_SET, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_ADD, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(float[],int,float)float");
        checkVarHandleAccessMode(vaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(float[],int,float)float");

        System.out.print("vad...");
        checkNotNull(vad);
        checkVarType(vad, double.class);
        checkCoordinateTypes(vad, "[class [D, int]");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET, true, "(double[],int)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.SET, true, "(double[],int,double)void");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_VOLATILE, true, "(double[],int)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.SET_VOLATILE, true, "(double[],int,double)void");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_ACQUIRE, true, "(double[],int)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.SET_RELEASE, true, "(double[],int,double)void");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_OPAQUE, true, "(double[],int)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.SET_OPAQUE, true, "(double[],int,double)void");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.COMPARE_AND_SET, true, "(double[],int,double,double)boolean");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(double[],int,double,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(double[],int,double,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(double[],int,double,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(double[],int,double,double)boolean");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(double[],int,double,double)boolean");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(double[],int,double,double)boolean");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(double[],int,double,double)boolean");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_SET, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_ADD, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(double[],int,double)double");
        checkVarHandleAccessMode(vad, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(double[],int,double)double");

        System.out.print("vao...");
        checkNotNull(vao);
        checkVarType(vao, Object.class);
        checkCoordinateTypes(vao, "[class [Ljava.lang.Object;, int]");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET, true, "(Object[],int)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.SET, true, "(Object[],int,Object)void");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_VOLATILE, true, "(Object[],int)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.SET_VOLATILE, true, "(Object[],int,Object)void");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_ACQUIRE, true, "(Object[],int)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.SET_RELEASE, true, "(Object[],int,Object)void");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_OPAQUE, true, "(Object[],int)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.SET_OPAQUE, true, "(Object[],int,Object)void");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.COMPARE_AND_SET, true, "(Object[],int,Object,Object)boolean");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(Object[],int,Object,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(Object[],int,Object,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(Object[],int,Object,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(Object[],int,Object,Object)boolean");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(Object[],int,Object,Object)boolean");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(Object[],int,Object,Object)boolean");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(Object[],int,Object,Object)boolean");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_SET, true, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_ADD, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(Object[],int,Object)Object");
        checkVarHandleAccessMode(vao, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(Object[],int,Object)Object");

        System.out.print("vbaz...");
        checkNull(vbaz);

        System.out.print("vbab...");
        checkNull(vbab);

        System.out.print("vbac...");
        checkNotNull(vbac);
        checkVarType(vbac, char.class);
        checkCoordinateTypes(vbac, "[class [B, int]");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET, true, "(byte[],int)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.SET, true, "(byte[],int,char)void");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,char)void");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,char)void");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,char)void");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.COMPARE_AND_SET, false, "(byte[],int,char,char)boolean");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(byte[],int,char,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(byte[],int,char,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(byte[],int,char,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(byte[],int,char,char)boolean");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(byte[],int,char,char)boolean");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(byte[],int,char,char)boolean");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(byte[],int,char,char)boolean");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_SET, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_ADD, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(byte[],int,char)char");
        checkVarHandleAccessMode(vbac, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(byte[],int,char)char");

        System.out.print("vbas...");
        checkNotNull(vbas);
        checkVarType(vbas, short.class);
        checkCoordinateTypes(vbas, "[class [B, int]");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET, true, "(byte[],int)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.SET, true, "(byte[],int,short)void");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,short)void");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,short)void");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,short)void");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.COMPARE_AND_SET, false, "(byte[],int,short,short)boolean");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(byte[],int,short,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(byte[],int,short,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(byte[],int,short,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(byte[],int,short,short)boolean");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(byte[],int,short,short)boolean");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(byte[],int,short,short)boolean");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(byte[],int,short,short)boolean");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_SET, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_ADD, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(byte[],int,short)short");
        checkVarHandleAccessMode(vbas, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(byte[],int,short)short");

        System.out.print("vbai...");
        checkNotNull(vbai);
        checkVarType(vbai, int.class);
        checkCoordinateTypes(vbai, "[class [B, int]");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET, true, "(byte[],int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.SET, true, "(byte[],int,int)void");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,int)void");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,int)void");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,int)void");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte[],int,int,int)boolean");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte[],int,int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte[],int,int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte[],int,int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte[],int,int,int)boolean");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte[],int,int,int)boolean");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte[],int,int,int)boolean");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte[],int,int,int)boolean");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_SET, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_ADD, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(byte[],int,int)int");
        checkVarHandleAccessMode(vbai, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(byte[],int,int)int");

        System.out.print("vbaj...");
        checkNotNull(vbaj);
        checkVarType(vbaj, long.class);
        checkCoordinateTypes(vbaj, "[class [B, int]");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET, true, "(byte[],int)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.SET, true, "(byte[],int,long)void");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,long)void");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,long)void");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,long)void");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte[],int,long,long)boolean");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte[],int,long,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte[],int,long,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte[],int,long,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte[],int,long,long)boolean");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte[],int,long,long)boolean");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte[],int,long,long)boolean");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte[],int,long,long)boolean");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_SET, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_ADD, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(byte[],int,long)long");
        checkVarHandleAccessMode(vbaj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(byte[],int,long)long");

        System.out.print("vbaf...");
        checkNotNull(vbaf);
        checkVarType(vbaf, float.class);
        checkCoordinateTypes(vbaf, "[class [B, int]");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET, true, "(byte[],int)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.SET, true, "(byte[],int,float)void");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,float)void");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,float)void");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,float)void");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte[],int,float,float)boolean");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte[],int,float,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte[],int,float,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte[],int,float,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte[],int,float,float)boolean");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte[],int,float,float)boolean");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte[],int,float,float)boolean");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte[],int,float,float)boolean");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_SET, true, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_ADD, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(byte[],int,float)float");
        checkVarHandleAccessMode(vbaf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(byte[],int,float)float");

        System.out.print("vbad...");
        checkNotNull(vbad);
        checkVarType(vbad, double.class);
        checkCoordinateTypes(vbad, "[class [B, int]");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET, true, "(byte[],int)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.SET, true, "(byte[],int,double)void");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_VOLATILE, true, "(byte[],int)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.SET_VOLATILE, true, "(byte[],int,double)void");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_ACQUIRE, true, "(byte[],int)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.SET_RELEASE, true, "(byte[],int,double)void");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_OPAQUE, true, "(byte[],int)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.SET_OPAQUE, true, "(byte[],int,double)void");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.COMPARE_AND_SET, true, "(byte[],int,double,double)boolean");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(byte[],int,double,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(byte[],int,double,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(byte[],int,double,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(byte[],int,double,double)boolean");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(byte[],int,double,double)boolean");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(byte[],int,double,double)boolean");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(byte[],int,double,double)boolean");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_SET, true, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_ADD, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(byte[],int,double)double");
        checkVarHandleAccessMode(vbad, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(byte[],int,double)double");

        System.out.print("vbao...");
        checkNull(vbao);

        System.out.print("vbbz...");
        checkNull(vbbz);

        System.out.print("vbbb...");
        checkNull(vbbb);

        System.out.print("vbbc...");
        checkNotNull(vbbc);
        checkVarType(vbbc, char.class);
        checkCoordinateTypes(vbbc, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,char)void");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,char)void");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,char)void");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,char)void");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.COMPARE_AND_SET, false, "(ByteBuffer,int,char,char)boolean");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(ByteBuffer,int,char,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(ByteBuffer,int,char,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(ByteBuffer,int,char,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(ByteBuffer,int,char,char)boolean");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(ByteBuffer,int,char,char)boolean");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(ByteBuffer,int,char,char)boolean");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(ByteBuffer,int,char,char)boolean");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_SET, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_ADD, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(ByteBuffer,int,char)char");
        checkVarHandleAccessMode(vbbc, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(ByteBuffer,int,char)char");

        System.out.print("vbbs...");
        checkNotNull(vbbs);
        checkVarType(vbbs, short.class);
        checkCoordinateTypes(vbbs, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,short)void");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,short)void");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,short)void");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,short)void");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.COMPARE_AND_SET, false, "(ByteBuffer,int,short,short)boolean");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, false, "(ByteBuffer,int,short,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, false, "(ByteBuffer,int,short,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, false, "(ByteBuffer,int,short,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, false, "(ByteBuffer,int,short,short)boolean");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, false, "(ByteBuffer,int,short,short)boolean");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, false, "(ByteBuffer,int,short,short)boolean");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, false, "(ByteBuffer,int,short,short)boolean");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_SET, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_SET_RELEASE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_ADD, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(ByteBuffer,int,short)short");
        checkVarHandleAccessMode(vbbs, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(ByteBuffer,int,short)short");

        System.out.print("vbbi...");
        checkNotNull(vbbi);
        checkVarType(vbbi, int.class);
        checkCoordinateTypes(vbbi, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,int)void");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,int)void");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,int)void");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,int)void");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.COMPARE_AND_SET, true, "(ByteBuffer,int,int,int)boolean");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(ByteBuffer,int,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(ByteBuffer,int,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(ByteBuffer,int,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(ByteBuffer,int,int,int)boolean");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(ByteBuffer,int,int,int)boolean");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(ByteBuffer,int,int,int)boolean");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(ByteBuffer,int,int,int)boolean");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_SET, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_ADD, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(ByteBuffer,int,int)int");
        checkVarHandleAccessMode(vbbi, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(ByteBuffer,int,int)int");

        System.out.print("vbbj...");
        checkNotNull(vbbj);
        checkVarType(vbbj, long.class);
        checkCoordinateTypes(vbbj, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,long)void");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,long)void");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,long)void");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,long)void");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.COMPARE_AND_SET, true, "(ByteBuffer,int,long,long)boolean");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(ByteBuffer,int,long,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(ByteBuffer,int,long,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(ByteBuffer,int,long,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(ByteBuffer,int,long,long)boolean");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(ByteBuffer,int,long,long)boolean");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(ByteBuffer,int,long,long)boolean");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(ByteBuffer,int,long,long)boolean");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_SET, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_ADD, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_ADD_RELEASE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_OR, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_AND, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_XOR, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, true, "(ByteBuffer,int,long)long");
        checkVarHandleAccessMode(vbbj, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, true, "(ByteBuffer,int,long)long");

        System.out.print("vbbf...");
        checkNotNull(vbbf);
        checkVarType(vbbf, float.class);
        checkCoordinateTypes(vbbf, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,float)void");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,float)void");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,float)void");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,float)void");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.COMPARE_AND_SET, true, "(ByteBuffer,int,float,float)boolean");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(ByteBuffer,int,float,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(ByteBuffer,int,float,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(ByteBuffer,int,float,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(ByteBuffer,int,float,float)boolean");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(ByteBuffer,int,float,float)boolean");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(ByteBuffer,int,float,float)boolean");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(ByteBuffer,int,float,float)boolean");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_SET, true, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_ADD, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(ByteBuffer,int,float)float");
        checkVarHandleAccessMode(vbbf, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(ByteBuffer,int,float)float");

        System.out.print("vbbd...");
        checkNotNull(vbbd);
        checkVarType(vbbd, double.class);
        checkCoordinateTypes(vbbd, "[class java.nio.ByteBuffer, int]");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET, true, "(ByteBuffer,int)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.SET, true, "(ByteBuffer,int,double)void");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_VOLATILE, true, "(ByteBuffer,int)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.SET_VOLATILE, true, "(ByteBuffer,int,double)void");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_ACQUIRE, true, "(ByteBuffer,int)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.SET_RELEASE, true, "(ByteBuffer,int,double)void");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_OPAQUE, true, "(ByteBuffer,int)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.SET_OPAQUE, true, "(ByteBuffer,int,double)void");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.COMPARE_AND_SET, true, "(ByteBuffer,int,double,double)boolean");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE, true, "(ByteBuffer,int,double,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, true, "(ByteBuffer,int,double,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, true, "(ByteBuffer,int,double,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, true, "(ByteBuffer,int,double,double)boolean");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET, true, "(ByteBuffer,int,double,double)boolean");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, true, "(ByteBuffer,int,double,double)boolean");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, true, "(ByteBuffer,int,double,double)boolean");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_SET, true, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_SET_ACQUIRE, true, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_SET_RELEASE, true, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_ADD, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_ADD_RELEASE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_OR, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_AND, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_XOR, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, false, "(ByteBuffer,int,double)double");
        checkVarHandleAccessMode(vbbd, VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, false, "(ByteBuffer,int,double)double");

        System.out.print("vbbo...");
        checkNull(vbbo);

        System.out.println("PASS");
    }

    private static void checkAccessMode(final VarHandle.AccessMode accessMode,
                                        final String expectedName,
                                        final String expectedMethodName,
                                        final int expectedOrdinal) {
        final String actualName = accessMode.toString();
        if (!actualName.equals(expectedName)) {
            fail("AccessMode " + actualName + " != " + expectedName);
        }

        final String actualMethodName = accessMode.methodName();
        if (!actualMethodName.equals(expectedMethodName)) {
            fail("AccessMode " + actualName + " method name " + actualMethodName + " != " +
                 expectedMethodName);
        }

        final int actualOrdinal = accessMode.ordinal();
        if (actualOrdinal != expectedOrdinal) {
            fail("AccessMode " + accessMode + " ordinal " + actualOrdinal + " != " +
                 expectedOrdinal);
        }

        VarHandle.AccessMode accessModeByName = VarHandle.AccessMode.valueOf(expectedName);
        if (accessModeByName != accessMode) {
            fail("AccessMode.valueOf(" + expectedName + ") returned " + accessModeByName);
        }
    }

    private static void checkAccessModes() {
        System.out.print("checkAccessModes...");
        final int expectedLength = 31;
        // Check we're not missing tests if the number of access modes ever changes.
        if (VarHandle.AccessMode.values().length != expectedLength) {
            fail("VarHandle.AccessMode.value().length != " + expectedLength);
        }
        checkAccessMode(VarHandle.AccessMode.GET, "GET", "get", 0);
        checkAccessMode(VarHandle.AccessMode.SET, "SET", "set", 1);
        checkAccessMode(VarHandle.AccessMode.GET_VOLATILE, "GET_VOLATILE", "getVolatile", 2);
        checkAccessMode(VarHandle.AccessMode.SET_VOLATILE, "SET_VOLATILE", "setVolatile", 3);
        checkAccessMode(VarHandle.AccessMode.GET_ACQUIRE, "GET_ACQUIRE", "getAcquire", 4);
        checkAccessMode(VarHandle.AccessMode.SET_RELEASE, "SET_RELEASE", "setRelease", 5);
        checkAccessMode(VarHandle.AccessMode.GET_OPAQUE, "GET_OPAQUE", "getOpaque", 6);
        checkAccessMode(VarHandle.AccessMode.SET_OPAQUE, "SET_OPAQUE", "setOpaque", 7);
        checkAccessMode(VarHandle.AccessMode.COMPARE_AND_SET, "COMPARE_AND_SET", "compareAndSet", 8);
        checkAccessMode(VarHandle.AccessMode.COMPARE_AND_EXCHANGE, "COMPARE_AND_EXCHANGE", "compareAndExchange", 9);
        checkAccessMode(VarHandle.AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE, "COMPARE_AND_EXCHANGE_ACQUIRE", "compareAndExchangeAcquire", 10);
        checkAccessMode(VarHandle.AccessMode.COMPARE_AND_EXCHANGE_RELEASE, "COMPARE_AND_EXCHANGE_RELEASE", "compareAndExchangeRelease", 11);
        checkAccessMode(VarHandle.AccessMode.WEAK_COMPARE_AND_SET_PLAIN, "WEAK_COMPARE_AND_SET_PLAIN", "weakCompareAndSetPlain", 12);
        checkAccessMode(VarHandle.AccessMode.WEAK_COMPARE_AND_SET, "WEAK_COMPARE_AND_SET", "weakCompareAndSet", 13);
        checkAccessMode(VarHandle.AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE, "WEAK_COMPARE_AND_SET_ACQUIRE", "weakCompareAndSetAcquire", 14);
        checkAccessMode(VarHandle.AccessMode.WEAK_COMPARE_AND_SET_RELEASE, "WEAK_COMPARE_AND_SET_RELEASE", "weakCompareAndSetRelease", 15);
        checkAccessMode(VarHandle.AccessMode.GET_AND_SET, "GET_AND_SET", "getAndSet", 16);
        checkAccessMode(VarHandle.AccessMode.GET_AND_SET_ACQUIRE, "GET_AND_SET_ACQUIRE", "getAndSetAcquire", 17);
        checkAccessMode(VarHandle.AccessMode.GET_AND_SET_RELEASE, "GET_AND_SET_RELEASE", "getAndSetRelease", 18);
        checkAccessMode(VarHandle.AccessMode.GET_AND_ADD, "GET_AND_ADD", "getAndAdd", 19);
        checkAccessMode(VarHandle.AccessMode.GET_AND_ADD_ACQUIRE, "GET_AND_ADD_ACQUIRE", "getAndAddAcquire", 20);
        checkAccessMode(VarHandle.AccessMode.GET_AND_ADD_RELEASE, "GET_AND_ADD_RELEASE", "getAndAddRelease", 21);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_OR, "GET_AND_BITWISE_OR", "getAndBitwiseOr", 22);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_OR_RELEASE, "GET_AND_BITWISE_OR_RELEASE", "getAndBitwiseOrRelease", 23);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_OR_ACQUIRE, "GET_AND_BITWISE_OR_ACQUIRE", "getAndBitwiseOrAcquire", 24);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_AND, "GET_AND_BITWISE_AND", "getAndBitwiseAnd", 25);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_AND_RELEASE, "GET_AND_BITWISE_AND_RELEASE", "getAndBitwiseAndRelease", 26);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_AND_ACQUIRE, "GET_AND_BITWISE_AND_ACQUIRE", "getAndBitwiseAndAcquire", 27);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_XOR, "GET_AND_BITWISE_XOR", "getAndBitwiseXor", 28);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_XOR_RELEASE, "GET_AND_BITWISE_XOR_RELEASE", "getAndBitwiseXorRelease", 29);
        checkAccessMode(VarHandle.AccessMode.GET_AND_BITWISE_XOR_ACQUIRE, "GET_AND_BITWISE_XOR_ACQUIRE", "getAndBitwiseXorAcquire", 30);
        System.out.println("PASS");
    }

    public static class LookupCheckA {
        public String fieldA = "123";
        public Object fieldB = "123";
        protected int fieldC = 0;
        private int fieldD = 0;

        public static String staticFieldA = "123";
        public static Object staticFieldB = "123";
        protected static int staticFieldC = 0;
        private static int staticFieldD = 0;

        private static final VarHandle vhA;
        private static final VarHandle vhB;
        private static final VarHandle vhC;
        private static final VarHandle vhD;

        private static final VarHandle vhsA;
        private static final VarHandle vhsB;
        private static final VarHandle vhsC;
        private static final VarHandle vhsD;

        static {
            try {
                // Instance fields
                try {
                    // Mis-spelling field name
                    MethodHandles.lookup().findVarHandle(LookupCheckA.class, "feldA", Object.class);
                    fail("Misspelled field name succeeded.");
                } catch (NoSuchFieldException e) {}

                try {
                    // Using wrong field type
                    MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldA", Float.class);
                    fail("Misspelled field name succeeded.");
                } catch (NoSuchFieldException e) {}

                try {
                    // Using wrong field type
                    MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldB", Float.class);
                    fail("Wrong field type succeeded.");
                } catch (NoSuchFieldException e) {}

                try {
                    // Looking up static field
                    MethodHandles.lookup().findVarHandle(LookupCheckA.class, "staticFieldA", String.class);
                    fail("Static field resolved as instance field.");
                } catch (IllegalAccessException e) {}

                vhA = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldA", String.class);
                vhB = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldB", Object.class);
                vhC = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldC", int.class);
                vhD = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldD", int.class);

                // Static fields
                try {
                    // Mis-spelling field name
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFeldA", Object.class);
                    fail("Misspelled field name succeeded.");
                } catch (NoSuchFieldException e) {}

                try {
                    // Using wrong field type
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldA", Float.class);
                    fail("Misspelled field name succeeded.");
                } catch (NoSuchFieldException e) {}

                try {
                    // Using wrong field type
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldB", Float.class);
                    fail("Wrong field type succeeded");
                } catch (NoSuchFieldException e) {}

                try {
                    // Looking up instance field
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "fieldA", String.class);
                    fail("Instance field resolved as static field");
                } catch (IllegalAccessException e) {}

                vhsA = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldA", String.class);
                vhsB = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldB", Object.class);
                vhsC = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldC", int.class);
                vhsD = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldD", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        protected static void fail(String reason) {
            Main.fail(reason);
        }

        public static void run() {
            System.out.print("LookupCheckA...");
            if (vhA == null) fail("vhA is null");
            if (vhB == null) fail("vhB is null");
            if (vhC == null) fail("vhC is null");
            if (vhD == null) fail("vhD is null");
            if (vhsA == null) fail("vhsA is null");
            if (vhsB == null) fail("vhsB is null");
            if (vhsC == null) fail("vhsC is null");
            if (vhsD == null) fail("vhsD is null");
            System.out.println("PASS");
        }
    }

    final static class LookupCheckB extends LookupCheckA {
        private static final VarHandle vhA;
        private static final VarHandle vhB;
        private static final VarHandle vhC;

        private static final VarHandle vhsA;
        private static final VarHandle vhsB;
        private static final VarHandle vhsC;

        static {
            try {
                vhA = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldA", String.class);
                MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldA", String.class);

                vhB = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldB", Object.class);
                MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldB", Object.class);

                vhC = MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldC", int.class);
                MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldC", int.class);

                try {
                    MethodHandles.lookup().findVarHandle(LookupCheckA.class, "fieldD", int.class);
                    fail("Accessing private field");
                } catch (IllegalAccessException e) {}

                vhsA = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldA", String.class);
                MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldA", String.class);

                vhsB = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldB", Object.class);
                MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldB", Object.class);

                vhsC = MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldC", int.class);
                MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldC", int.class);

                try {
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckA.class, "staticFieldD", int.class);
                    fail("Accessing private field");
                } catch (IllegalAccessException e) {}
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void run() {
            // Testing access
            System.out.print("LookupCheckB...");
            if (vhA == null) fail("vhA is null");
            if (vhB == null) fail("vhB is null");
            if (vhC == null) fail("vhC is null");
            if (vhsA == null) fail("vhsA is null");
            if (vhsB == null) fail("vhsB is null");
            if (vhsC == null) fail("vhsC is null");
            System.out.println("PASS");
        }
    }

    public static class LookupCheckC {
        private static final VarHandle vhA;
        private static final VarHandle vhB;
        private static final VarHandle vhC;
        private static final VarHandle vhsA;
        private static final VarHandle vhsB;
        private static final VarHandle vhsC;

        static {
            try {
                vhA = MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldA", String.class);
                try {
                    MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldA", Float.class);
                } catch (NoSuchFieldException e) {}
                vhB = MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldB", Object.class);
                try {
                    MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldB", int.class);
                } catch (NoSuchFieldException e) {}
                vhC = MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldC", int.class);
                try {
                    MethodHandles.lookup().findVarHandle(LookupCheckB.class, "fieldD", int.class);
                    fail("Accessing private field in unrelated class");
                } catch (IllegalAccessException e) {}

                vhsA = MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldA", String.class);
                try {
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldA", Float.class);
                } catch (NoSuchFieldException e) {}
                vhsB = MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldB", Object.class);
                try {
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldB", int.class);
                } catch (NoSuchFieldException e) {}
                vhsC = MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldC", int.class);
                try {
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "staticFieldD", int.class);
                    fail("Accessing private field in unrelated class");
                } catch (IllegalAccessException e) {}

                try {
                    MethodHandles.lookup().findStaticVarHandle(LookupCheckB.class, "fieldA", String.class);
                    fail("Found instance field looking for static");
                } catch (IllegalAccessException e) {}
                try {
                    MethodHandles.lookup().findVarHandle(LookupCheckB.class, "staticFieldA", String.class);
                    fail("Found static field looking for instance");
                } catch (IllegalAccessException e) {}
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void run() {
            System.out.print("UnreflectCheck...");
            if (vhA == null) fail("vhA is null");
            if (vhB == null) fail("vhB is null");
            if (vhsA == null) fail("vhsA is null");
            if (vhsB == null) fail("vhsB is null");
            System.out.println("PASS");
        }
    }

    public static final class UnreflectCheck {
        private static final VarHandle vhA;
        private static final VarHandle vhsA;

        static {
            try {
                Field publicField = LookupCheckA.class.getField("fieldA");
                vhA = MethodHandles.lookup().unreflectVarHandle(publicField);
                try {
                    Field protectedField = LookupCheckA.class.getField("fieldC");
                    MethodHandles.lookup().unreflectVarHandle(protectedField);
                    fail("Unreflected protected field");
                } catch (NoSuchFieldException e) {}
                try {
                    Field privateField = LookupCheckA.class.getField("fieldD");
                    MethodHandles.lookup().unreflectVarHandle(privateField);
                    fail("Unreflected private field");
                } catch (NoSuchFieldException e) {}
                try {
                    Field privateField = LookupCheckA.class.getField("fieldD");
                    privateField.setAccessible(true);
                    MethodHandles.lookup().unreflectVarHandle(privateField);
                    fail("Unreflected private field");
                } catch (NoSuchFieldException e) {}

                Field staticPublicField = LookupCheckA.class.getField("staticFieldA");
                vhsA = MethodHandles.lookup().unreflectVarHandle(staticPublicField);
                try {
                    Field protectedField = LookupCheckA.class.getField("staticFieldC");
                    MethodHandles.lookup().unreflectVarHandle(protectedField);
                    fail("Unreflected protected field");
                } catch (NoSuchFieldException e) {}
                try {
                    Field privateField = LookupCheckA.class.getField("staticFieldD");
                    MethodHandles.lookup().unreflectVarHandle(privateField);
                    fail("Unreflected private field");
                } catch (NoSuchFieldException e) {}
                try {
                    Field privateField = LookupCheckA.class.getField("staticFieldD");
                    privateField.setAccessible(true);
                    MethodHandles.lookup().unreflectVarHandle(privateField);
                    fail("Unreflected private field");
                } catch (NoSuchFieldException e) {}
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static void run() {
            System.out.print("LookupCheckC...");
            if (vhA == null) fail("vhA is null");
            if (vhsA == null) fail("vhsA is null");
            System.out.println("PASS");
        }
    }

    public static void main(String[] args) {
        checkAccessModes();
        checkInstantiatedVarHandles();
        LookupCheckA.run();
        LookupCheckB.run();
        LookupCheckC.run();
        UnreflectCheck.run();
    }
}

