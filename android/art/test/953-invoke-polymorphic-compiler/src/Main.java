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
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class Main {
  public static void assertTrue(boolean value) {
    if (!value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertFalse(boolean value) {
    if (value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertEquals(int i1, int i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals i1: " + i1 + ", i2: " + i2);
  }

  public static void assertEquals(long i1, long i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals l1: " + i1 + ", l2: " + i2);
  }

  public static void assertEquals(Object o, Object p) {
    if (o == p) { return; }
    if (o != null && p != null && o.equals(p)) { return; }
    throw new AssertionError("assertEquals: o1: " + o + ", o2: " + p);
  }

  public static void assertEquals(String s1, String s2) {
    if (s1 == s2) {
      return;
    }

    if (s1 != null && s2 != null && s1.equals(s2)) {
      return;
    }

    throw new AssertionError("assertEquals s1: " + s1 + ", s2: " + s2);
  }

  public static void fail() {
    System.out.println("fail");
    Thread.dumpStack();
  }

  public static void fail(String message) {
    System.out.println("fail: " + message);
    Thread.dumpStack();
  }

  public static int Min2Print2(int a, int b) {
    int[] values = new int[] { a, b };
    System.out.println("Running Main.Min2Print2(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print3(int a, int b, int c) {
    int[] values = new int[] { a, b, c };
    System.out.println("Running Main.Min2Print3(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print6(int a, int b, int c, int d, int e, int f) {
    int[] values = new int[] { a, b, c, d, e, f };
    System.out.println("Running Main.Min2Print6(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print26(int a, int b, int c, int d,
                                int e, int f, int g, int h,
                                int i, int j, int k, int l,
                                int m, int n, int o, int p,
                                int q, int r, int s, int t,
                                int u, int v, int w, int x,
                                int y, int z) {
    int[] values = new int[] { a, b, c, d, e, f, g, h, i, j, k, l, m,
                               n, o, p, q, r, s, t, u, v, w, x, y, z };
    System.out.println("Running Main.Min2Print26(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static void $opt$BasicTest() throws Throwable {
    MethodHandle mh;
    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print2", MethodType.methodType(int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4), 33);
    assertEquals((int) mh.invokeExact(-4, 33), 33);

    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print3",
        MethodType.methodType(int.class, int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4, 17), 33);
    assertEquals((int) mh.invokeExact(-4, 17, 33), 17);
    assertEquals((int) mh.invokeExact(17, 33, -4), 33);

    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print6",
        MethodType.methodType(
            int.class, int.class, int.class, int.class, int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4, 77, 88, 99, 111), 33);
    try {
        // Too few arguments
        assertEquals((int) mh.invokeExact(33, -4, 77, 88), 33);
        fail("No WMTE for too few arguments");
    } catch (WrongMethodTypeException e) {}
    try {
        // Too many arguments
        assertEquals((int) mh.invokeExact(33, -4, 77, 88, 89, 90, 91), 33);
        fail("No WMTE for too many arguments");
    } catch (WrongMethodTypeException e) {}
    assertEquals((int) mh.invokeExact(-4, 77, 88, 99, 111, 33), 77);
    assertEquals((int) mh.invokeExact(77, 88, 99, 111, 33, -4), 88);
    assertEquals((int) mh.invokeExact(88, 99, 111, 33, -4, 77), 99);
    assertEquals((int) mh.invokeExact(99, 111, 33, -4, 77, 88), 111);
    assertEquals((int) mh.invokeExact(111, 33, -4, 77, 88, 99), 111);

    // A preposterous number of arguments.
    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print26",
        MethodType.methodType(
            // Return-type
            int.class,
            // Arguments
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class));
    assertEquals(1, (int) mh.invokeExact(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25));
    assertEquals(25, (int) mh.invokeExact(25, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24));
    assertEquals(25, (int) mh.invokeExact(24, 25, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23));

    try {
        // Wrong argument type
        mh.invokeExact("a");
        fail("No WMTE for wrong arguments");
    } catch (WrongMethodTypeException wmte) {}

    try {
        // Invoke on null handle.
        MethodHandle mh0 = null;
        mh0.invokeExact("bad");
        fail("No NPE for you");
    } catch (NullPointerException npe) {}

    System.out.println("BasicTest done.");
  }

  private static boolean And(boolean lhs, boolean rhs) {
    return lhs & rhs;
  }

  private static boolean Xor(boolean lhs, boolean rhs) {
    return lhs ^ rhs;
  }

  private static String Multiply(String value, int n) {
    String result = "";
    for (int i = 0; i < n; ++i) {
      result = value + result;
    }
    return result;
  }

  private static byte Multiply(byte value, byte n) {
    return (byte)(value * n);
  }

  private static short Multiply(short value, short n) {
    return (short)(value * n);
  }

  private static int Multiply(int value, int n) {
    return value * n;
  }

  private static long Multiply(long value, long n) {
    return value * n;
  }

  private static float Multiply(float value, float n) {
    return value * n;
  }

  private static double Multiply(double value, double n) {
    return value * n;
  }

  private static char Next(char c) {
    return (char)(c + 1);
  }

  public static void $opt$ReturnBooleanTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh =
            lookup.findStatic(Main.class, "And",
                              MethodType.methodType(boolean.class, boolean.class, boolean.class));
    assertEquals(true, (boolean) mh.invokeExact(true, true));
    assertEquals(false, (boolean) mh.invokeExact(true, false));
    assertEquals(false, (boolean) mh.invokeExact(false, true));
    assertEquals(false, (boolean) mh.invokeExact(false, false));
    assertEquals(true, (boolean) mh.invoke(true, true));
    assertEquals(false, (boolean) mh.invoke(true, false));
    assertEquals(false, (boolean) mh.invoke(false, true));
    assertEquals(false, (boolean) mh.invoke(false, false));

    mh = lookup.findStatic(Main.class, "Xor",
                           MethodType.methodType(boolean.class, boolean.class, boolean.class));
    assertEquals(false, (boolean) mh.invokeExact(true, true));
    assertEquals(true, (boolean) mh.invokeExact(true, false));
    assertEquals(true, (boolean) mh.invokeExact(false, true));
    assertEquals(false, (boolean) mh.invokeExact(false, false));
    assertEquals(false, (boolean) mh.invoke(true, true));
    assertEquals(true, (boolean) mh.invoke(true, false));
    assertEquals(true, (boolean) mh.invoke(false, true));
    assertEquals(false, (boolean) mh.invoke(false, false));

    System.out.println("$opt$ReturnBooleanTest done.");
  }

  public static void $opt$ReturnCharTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Next",
                           MethodType.methodType(char.class, char.class));
    assertEquals('B', (char) mh.invokeExact('A'));
    assertEquals((char) -55, (char) mh.invokeExact((char) -56));
    System.out.println("$opt$ReturnCharTest done.");
  }

  public static void $opt$ReturnByteTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                                         MethodType.methodType(byte.class, byte.class, byte.class));
    assertEquals((byte) 30, (byte) mh.invokeExact((byte) 10, (byte) 3));
    assertEquals((byte) -90, (byte) mh.invoke((byte) -10, (byte) 9));
    System.out.println("$opt$ReturnByteTest done.");
  }

  public static void $opt$ReturnShortTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(short.class, short.class, short.class));
    assertEquals((short) 3000, (short) mh.invokeExact((short) 1000, (short) 3));
    assertEquals((short) -3000, (short) mh.invoke((short) -1000, (short) 3));
    System.out.println("$opt$ReturnShortTest done.");
  }

  public static void $opt$ReturnIntTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(int.class, int.class, int.class));
    assertEquals(3_000_000, (int) mh.invokeExact(1_000_000, 3));
    assertEquals(-3_000_000, (int) mh.invoke(-1_000, 3_000));
    System.out.println("$opt$ReturnIntTest done.");
  }

  public static void $opt$ReturnLongTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(long.class, long.class, long.class));
    assertEquals(4_294_967_295_000L, (long) mh.invokeExact(1000L, 4_294_967_295L));
    assertEquals(-4_294_967_295_000L, (long) mh.invoke(-1000L, 4_294_967_295L));
    System.out.println("$opt$ReturnLongTest done.");
  }

  public static void $opt$ReturnFloatTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(float.class, float.class, float.class));
    assertEquals(3.0F, (float) mh.invokeExact(1000.0F, 3e-3F));
    assertEquals(-3.0F, (float) mh.invoke(-1000.0F, 3e-3F));
    System.out.println("$opt$ReturnFloatTest done.");
  }

  public static void $opt$ReturnDoubleTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(double.class, double.class, double.class));
    assertEquals(3033000.0, (double) mh.invokeExact(1000.0, 3.033e3));
    assertEquals(-3033000.0, (double) mh.invoke(-1000.0, 3.033e3));
    System.out.println("$opt$ReturnDoubleTest done.");
  }

  public static void $opt$ReturnStringTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();
    MethodHandle mh = lookup.findStatic(Main.class, "Multiply",
                           MethodType.methodType(String.class, String.class, int.class));
    assertEquals("100010001000", (String) mh.invokeExact("1000", 3));
    assertEquals("100010001000", (String) mh.invoke("1000", 3));
    System.out.println("$opt$ReturnStringTest done.");
  }

  public static void ReturnValuesTest() throws Throwable {
    $opt$ReturnBooleanTest();
    $opt$ReturnCharTest();
    $opt$ReturnByteTest();
    $opt$ReturnShortTest();
    $opt$ReturnIntTest();
    $opt$ReturnLongTest();
    $opt$ReturnFloatTest();
    $opt$ReturnDoubleTest();
    $opt$ReturnStringTest();
    System.out.println("ReturnValuesTest done.");
  }

  static class ValueHolder {
    public boolean m_z;
    public static boolean s_z;
  }

  public static void $opt$AccessorsTest() throws Throwable {
    ValueHolder valueHolder = new ValueHolder();
    MethodHandles.Lookup lookup = MethodHandles.lookup();

    MethodHandle setMember = lookup.findSetter(ValueHolder.class, "m_z", boolean.class);
    MethodHandle getMember = lookup.findGetter(ValueHolder.class, "m_z", boolean.class);
    MethodHandle setStatic = lookup.findStaticSetter(ValueHolder.class, "s_z", boolean.class);
    MethodHandle getStatic = lookup.findStaticGetter(ValueHolder.class, "s_z", boolean.class);

    boolean [] values = { false, true, false, true, false };
    for (boolean value : values) {
      assertEquals((boolean) getStatic.invoke(), ValueHolder.s_z);
      setStatic.invoke(value);
      ValueHolder.s_z = value;
      assertEquals(ValueHolder.s_z, value);
      assertEquals((boolean) getStatic.invoke(), value);

      assertEquals((boolean) getMember.invoke(valueHolder), valueHolder.m_z);
      setMember.invoke(valueHolder, value);
      valueHolder.m_z = value;
      assertEquals(valueHolder.m_z, value);
      assertEquals((boolean) getMember.invoke(valueHolder), value);
    }
  }

  public static void main(String[] args) throws Throwable {
    $opt$BasicTest();
    ReturnValuesTest();
    $opt$AccessorsTest();
  }
}
