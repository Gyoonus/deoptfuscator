/*
 * Copyright (C) 2011 The Android Open Source Project
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

import java.lang.reflect.*;
import java.util.*;

class Main {
  private static boolean z = true;
  private static byte b = 8;
  private static char c = '\u2714';
  private static double d = Math.PI;
  private static float f = 3.14f;
  private static int i = 32;
  private static long j = 0x0123456789abcdefL;
  private static short s = 16;

  public static void testFieldReflection() throws Exception {
    Field f;

    f = Main.class.getDeclaredField("z");
    System.out.println(f.getBoolean(null));
    f = Main.class.getDeclaredField("b");
    System.out.println(f.getByte(null));
    f = Main.class.getDeclaredField("c");
    System.out.println(f.getChar(null));
    f = Main.class.getDeclaredField("d");
    System.out.println(f.getDouble(null));
    f = Main.class.getDeclaredField("f");
    System.out.println(f.getFloat(null));
    f = Main.class.getDeclaredField("i");
    System.out.println(f.getInt(null));
    f = Main.class.getDeclaredField("j");
    System.out.println(f.getLong(null));
    f = Main.class.getDeclaredField("s");
    System.out.println(f.getShort(null));

    f = Main.class.getDeclaredField("z");
    f.setBoolean(null, false);
    f = Main.class.getDeclaredField("b");
    f.setByte(null, (byte) 7);
    f = Main.class.getDeclaredField("c");
    f.setChar(null, 'y');
    f = Main.class.getDeclaredField("d");
    f.setDouble(null, 2.7);
    f = Main.class.getDeclaredField("f");
    f.setFloat(null, 2.7f);
    f = Main.class.getDeclaredField("i");
    f.setInt(null, 31);
    f = Main.class.getDeclaredField("j");
    f.setLong(null, 63);
    f = Main.class.getDeclaredField("s");
    f.setShort(null, (short) 15);

    f = Main.class.getDeclaredField("z");
    System.out.println(f.getBoolean(null));
    f = Main.class.getDeclaredField("b");
    System.out.println(f.getByte(null));
    f = Main.class.getDeclaredField("c");
    System.out.println(f.getChar(null));
    f = Main.class.getDeclaredField("d");
    System.out.println(f.getDouble(null));
    f = Main.class.getDeclaredField("f");
    System.out.println(f.getFloat(null));
    f = Main.class.getDeclaredField("i");
    System.out.println(f.getInt(null));
    f = Main.class.getDeclaredField("j");
    System.out.println(f.getLong(null));
    f = Main.class.getDeclaredField("s");
    System.out.println(f.getShort(null));

    f = Main.class.getDeclaredField("z");
    f.set(null, Boolean.valueOf(true));
    f = Main.class.getDeclaredField("b");
    f.set(null, Byte.valueOf((byte) 6));
    f = Main.class.getDeclaredField("c");
    f.set(null, Character.valueOf('z'));
    f = Main.class.getDeclaredField("d");
    f.set(null, Double.valueOf(1.3));
    f = Main.class.getDeclaredField("f");
    f.set(null, Float.valueOf(1.3f));
    f = Main.class.getDeclaredField("i");
    f.set(null, Integer.valueOf(30));
    f = Main.class.getDeclaredField("j");
    f.set(null, Long.valueOf(62));
    f.set(null, Integer.valueOf(62));
    f = Main.class.getDeclaredField("s");
    f.set(null, Short.valueOf((short) 14));

    f = Main.class.getDeclaredField("z");
    System.out.println(f.getBoolean(null));
    f = Main.class.getDeclaredField("b");
    System.out.println(f.getByte(null));
    f = Main.class.getDeclaredField("c");
    System.out.println(f.getChar(null));
    f = Main.class.getDeclaredField("d");
    System.out.println(f.getDouble(null));
    f = Main.class.getDeclaredField("f");
    System.out.println(f.getFloat(null));
    f = Main.class.getDeclaredField("i");
    System.out.println(f.getInt(null));
    f = Main.class.getDeclaredField("j");
    System.out.println(f.getLong(null));
    f = Main.class.getDeclaredField("s");
    System.out.println(f.getShort(null));

    try {
      f = Main.class.getDeclaredField("s");
      f.set(null, Integer.valueOf(14));
      System.out.println("************* should have thrown!");
    } catch (IllegalArgumentException expected) {
      System.out.println("got expected IllegalArgumentException");
    }

    f = Main.class.getDeclaredField("z");
    show(f.get(null));
    f = Main.class.getDeclaredField("b");
    show(f.get(null));
    f = Main.class.getDeclaredField("c");
    show(f.get(null));
    f = Main.class.getDeclaredField("d");
    show(f.get(null));
    f = Main.class.getDeclaredField("f");
    show(f.get(null));
    f = Main.class.getDeclaredField("i");
    show(f.get(null));
    f = Main.class.getDeclaredField("j");
    show(f.get(null));
    f = Main.class.getDeclaredField("s");
    show(f.get(null));

    /*
    private static boolean z = true;
    private static byte b = 8;
    private static char c = '\u2714';
    private static double d = Math.PI;
    private static float f = 3.14f;
    private static int i = 32;
    private static long j = 0x0123456789abcdefL;
    private static short s = 16;
    */
  }

  private static void show(Object o) {
    System.out.println(o + " (" + (o != null ? o.getClass() : "null") + ")");
  }

  /**
   * Sorts the input array using the comparator and returns the sorted array.
   */
  private static Object[] sort(Object[] objects, Comparator<Object> comp) {
    Arrays.sort(objects, comp);
    return objects;
  }

  public static void testMethodReflection() throws Exception {
    Comparator<Object> comp = new Comparator<Object>() {
      public int compare(Object a, Object b) {
        return a.toString().compareTo(b.toString());
      }
      public boolean equals(Object b) {
        return this == b;
      }
    };

    // Sort the return values by their string values since the order is undefined by the spec.
    System.out.println(Arrays.toString(sort(String.class.getDeclaredConstructors(), comp)));
    System.out.println(Arrays.toString(sort(String.class.getDeclaredFields(), comp)));
    System.out.println(Arrays.toString(sort(String.class.getDeclaredMethods(), comp)));

    System.out.println(Arrays.toString(Main.class.getInterfaces()));
    System.out.println(Arrays.toString(String.class.getInterfaces()));

    System.out.println(Main.class.getModifiers());
    System.out.println(String.class.getModifiers());

    System.out.println(String.class.isAssignableFrom(Object.class));
    System.out.println(Object.class.isAssignableFrom(String.class));

    System.out.println(String.class.isInstance("hello"));
    System.out.println(String.class.isInstance(123));

    Method m;

    m = Main.class.getDeclaredMethod("IV", int.class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, 4444));
    System.out.println(Arrays.toString(m.getParameterTypes()));

    m = Main.class.getDeclaredMethod("IIV", int.class, int.class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, 1111, 2222));

    m = Main.class.getDeclaredMethod("III", int.class, int.class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, 1111, 2222));

    m = Main.class.getDeclaredMethod("sumArray", int[].class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, new int[] { 1, 2, 3, 4 }));

    m = Main.class.getDeclaredMethod("concat", String[].class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, (Object) new String[] { "h", "e", "l", "l", "o" }));

    m = Main.class.getDeclaredMethod("ZBCDFIJSV", boolean.class, byte.class, char.class, double.class, float.class, int.class, long.class, short.class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, true, (byte) 0, '1', 2, 3, 4, 5, (short) 6));

    m = Main.class.getDeclaredMethod("ZBCDLFIJSV", boolean.class, byte.class, char.class, double.class, String.class, float.class, int.class, long.class, short.class);
    System.out.println(Arrays.toString(m.getParameterTypes()));
    show(m.invoke(null, true, (byte) 0, '1', 2, "hello world", 3, 4, 5, (short) 6));

    try {
      m = Main.class.getDeclaredMethod("thrower");
      System.out.println(Arrays.toString(m.getParameterTypes()));
      show(m.invoke(null));
      System.out.println("************* should have thrown!");
    } catch (InvocationTargetException expected) {
      System.out.println("got expected InvocationTargetException");
    }
  }

  private static void thrower() {
    throw new ArithmeticException("surprise!");
  }

  private static int sumArray(int[] xs) {
    int result = 0;
    for (int x : xs) {
      result += x;
    }
    return result;
  }

  private static String concat(String[] strings) {
    String result = "";
    for (String s : strings) {
      result += s;
    }
    return result;
  }

  private static void IV(int i) {
    System.out.println(i);
  }

  private static void IIV(int i, int j) {
    System.out.println(i + " " + j);
  }

  private static int III(int i, int j) {
    System.out.println(i + " " + j);
    return i + j;
  }

  private static void ZBCDFIJSV(boolean z, byte b, char c, double d, float f, int i, long l, short s) {
    System.out.println(z + " " + b + " " + c + " " + d + " " + f + " " + i + " " + l + " " + s);
  }

  private static void ZBCDLFIJSV(boolean z, byte b, char c, double d, String string, float f, int i, long l, short s) {
    System.out.println(z + " " + b + " " + c + " " + d + " " + " " + string + " " + f + " " + i + " " + l + " " + s);
  }

  public static void testConstructorReflection() throws Exception {
    Constructor<String> ctor = String.class.getConstructor();
    show(ctor.newInstance());

    ctor = String.class.getConstructor(char[].class, int.class, int.class);
    show(ctor.newInstance(new char[] { '\u2714', 'y', 'z', '!' }, 1, 2));
  }

  private static void testPackagePrivateConstructor() {
    try {
      Class<?> c = Class.forName("sub.PPClass");
      Constructor<?> cons = c.getConstructor();
      cons.newInstance();
      throw new RuntimeException("Expected IllegalAccessException.");
    } catch (IllegalAccessException e) {
      // Expected.
    } catch (Exception e) {
      // Error.
      e.printStackTrace(System.out);
    }
  }

  private static void testPackagePrivateAccessibleConstructor() {
    try {
      Class<?> c = Class.forName("sub.PPClass");
      Constructor<?> cons = c.getConstructor();
      cons.setAccessible(true);  // ensure we prevent IllegalAccessException
      cons.newInstance();
    } catch (Exception e) {
      // Error.
      e.printStackTrace(System.out);
    }
  }

  public static void main(String[] args) throws Exception {
    testFieldReflection();
    testMethodReflection();
    testConstructorReflection();
    testPackagePrivateConstructor();
    testPackagePrivateAccessibleConstructor();
  }
}
