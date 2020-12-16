/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

// Note that $opt$ is a marker for the optimizing compiler to test
// it does compile the method.

public class Main extends TestCase {
  public static void main(String[] args) throws Exception {
    $opt$TestAllocations();
    $opt$TestWithInitializations();
    $opt$TestNegativeValueNewByteArray();
    $opt$TestNegativeValueNewCharArray();
    testNegativeArraySize();
    testSmaliFilledNewArray();
    testSmaliFillArrayData();
    testSmaliVerifyError();
  }

  static void $opt$TestAllocations() {
    float[] a = new float[1];
    assertEquals(1, a.length);

    double[] b = new double[2];
    assertEquals(2, b.length);

    long[] c = new long[3];
    assertEquals(3, c.length);

    int[] d = new int[4];
    assertEquals(4, d.length);

    short[] e = new short[5];
    assertEquals(5, e.length);

    char[] f = new char[6];
    assertEquals(6, f.length);

    byte[] g = new byte[7];
    assertEquals(7, g.length);

    boolean[] h = new boolean[8];
    assertEquals(8, h.length);

    Object[] i = new Object[9];
    assertEquals(9, i.length);
  }

  static void $opt$TestWithInitializations() {
    float[] a = { 1.2f };
    assertEquals(1, a.length);
    assertEquals(1.2f, a[0]);

    double[] b = { 4.3, 1.2 };
    assertEquals(2, b.length);
    assertEquals(4.3, b[0]);
    assertEquals(1.2, b[1]);

    long[] c = { 4L, 5L };
    assertEquals(2, c.length);
    assertEquals(4L, c[0]);
    assertEquals(5L, c[1]);

    int[] d = {1, 2, 3};
    assertEquals(3, d.length);
    assertEquals(1, d[0]);
    assertEquals(2, d[1]);
    assertEquals(3, d[2]);

    short[] e = {4, 5, 6};
    assertEquals(3, e.length);
    assertEquals(4, e[0]);
    assertEquals(5, e[1]);
    assertEquals(6, e[2]);

    char[] f = {'a', 'b'};
    assertEquals(2, f.length);
    assertEquals('a', f[0]);
    assertEquals('b', f[1]);

    byte[] g = {7, 8, 9};
    assertEquals(3, g.length);
    assertEquals(7, g[0]);
    assertEquals(8, g[1]);
    assertEquals(9, g[2]);

    boolean[] h = {true, false};
    assertEquals(2, h.length);
    assertEquals(true, h[0]);
    assertEquals(false, h[1]);

    Object obj1 = new Object();
    Object obj2 = new Object();
    Object[] i = {obj1, obj2};
    assertEquals(2, i.length);
    assertEquals(obj1, i[0]);
    assertEquals(obj2, i[1]);
  }

  static void $opt$TestNegativeValueNewByteArray() {
    // Use an array initializer to hint the use of filled-new-array.
    byte[] a = { (byte)0xa0, (byte)0xa1, (byte)0xa2, (byte)0xa3,
                 (byte)0xa4, (byte)0xa5, (byte)0xa6, (byte)0xa7 };
    for (int i = 0; i < a.length; i++) {
      assertEquals((byte)0xa0 + i, a[i]);
    }
  }

  static void $opt$TestNegativeValueNewCharArray() {
    // Use an array initializer to hint the use of filled-new-array.
    char[] a = { (char)0xa000, (char)0xa001, (char)0xa002, (char)0xa003,
                 (char)0xa004, (char)0xa005, (char)0xa006, (char)0xa007 };
    for (int i = 0; i < a.length; i++) {
      assertEquals((char)0xa000 + i, a[i]);
    }
  }

  static void testNegativeArraySize() {
    int i = 0;
    try {
      $opt$TestNegativeArraySize();
    } catch (NegativeArraySizeException e) {
      i = 1;
    }
    assertEquals(i, 1);
  }

  static int[] $opt$TestNegativeArraySize() {
    int[] array = new int[-1];
    return null;
  }

  public static void testSmaliFilledNewArray() throws Exception {
    Class<?> c = Class.forName("FilledNewArray");

    {
      Method m = c.getMethod("newInt", Integer.TYPE, Integer.TYPE, Integer.TYPE);
      Object[] args = {new Integer(1), new Integer(2), new Integer(3)};
      int[] result = (int[])m.invoke(null, args);
      assertEquals(3, result.length);
      assertEquals(1, result[0]);
      assertEquals(2, result[1]);
      assertEquals(3, result[2]);
    }

    {
      Method m = c.getMethod("newRef", Object.class, Object.class);
      Object[] args = {new Integer(1), new Integer(2)};
      Object[] result = (Object[])m.invoke(null, args);
      assertEquals(2, result.length);
      assertEquals(args[0], result[0]);
      assertEquals(args[1], result[1]);
    }

    {
      Method m = c.getMethod("newArray", int[].class, int[].class);
      Object[] args = {new int[0], new int[1]};
      Object[] result = (Object[])m.invoke(null, args);
      assertEquals(2, result.length);
      assertEquals(args[0], result[0]);
      assertEquals(args[1], result[1]);
    }

    {
      Method m = c.getMethod("newIntRange", Integer.TYPE, Integer.TYPE, Integer.TYPE);
      Object[] args = {new Integer(1), new Integer(2), new Integer(3)};
      int[] result = (int[])m.invoke(null, args);
      assertEquals(3, result.length);
      assertEquals(1, result[0]);
      assertEquals(2, result[1]);
      assertEquals(3, result[2]);
    }

    {
      Method m = c.getMethod("newRefRange", Object.class, Object.class);
      Object[] args = {new Integer(1), new Integer(2)};
      Object[] result = (Object[])m.invoke(null, args);
      assertEquals(2, result.length);
      assertEquals(args[0], result[0]);
      assertEquals(args[1], result[1]);
    }

    {
      Method m = c.getMethod("newArrayRange", int[].class, int[].class);
      Object[] args = {new int[0], new int[1]};
      Object[] result = (Object[])m.invoke(null, args);
      assertEquals(2, result.length);
      assertEquals(args[0], result[0]);
      assertEquals(args[1], result[1]);
    }
  }

  public static void testSmaliVerifyError() throws Exception {
    Error error = null;
    // Ensure the elements in filled-new-array must be assignable
    // to the array component type.
    try {
      Class.forName("FilledNewArrayVerifyError");
    } catch (VerifyError e) {
      error = e;
    }
    assertNotNull(error);
  }

  public static void testSmaliFillArrayData() throws Exception {
    Class<?> c = Class.forName("FillArrayData");
    {
      Method m = c.getMethod("emptyIntArray", int[].class);
      int[] array = new int[0];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(0, array.length);

      array = new int[2];
      args[0] = array;
      m.invoke(null, args);
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      array = new int[] { 42, -42 };
      args[0] = array;
      m.invoke(null, args);
      // Test that nothing has been written to the array.
      assertEquals(42, array[0]);
      assertEquals(-42, array[1]);

      Throwable exception = null;
      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("intArray", int[].class);
      int[] array = new int[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1, array[0]);
      assertEquals(2, array[1]);
      assertEquals(3, array[2]);
      assertEquals(4, array[3]);
      assertEquals(5, array[4]);
      assertEquals(0, array[5]);
      assertEquals(0, array[6]);

      array = new int[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("intArrayFillInstructionAfterData", int[].class);
      int[] array = new int[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1, array[0]);
      assertEquals(2, array[1]);
      assertEquals(3, array[2]);
      assertEquals(4, array[3]);
      assertEquals(5, array[4]);
      assertEquals(0, array[5]);
      assertEquals(0, array[6]);

      array = new int[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("shortArray", short[].class);
      short[] array = new short[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1, array[0]);
      assertEquals(2, array[1]);
      assertEquals(3, array[2]);
      assertEquals(4, array[3]);
      assertEquals(5, array[4]);
      assertEquals(0, array[5]);
      assertEquals(0, array[6]);

      array = new short[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("longArray", long[].class);
      long[] array = new long[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1L, array[0]);
      assertEquals(2L, array[1]);
      assertEquals(3L, array[2]);
      assertEquals(4L, array[3]);
      assertEquals(5L, array[4]);
      assertEquals(0L, array[5]);
      assertEquals(0L, array[6]);

      array = new long[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("charArray", char[].class);
      char[] array = new char[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1, array[0]);
      assertEquals(2, array[1]);
      assertEquals(3, array[2]);
      assertEquals(4, array[3]);
      assertEquals(5, array[4]);
      assertEquals(0, array[5]);
      assertEquals(0, array[6]);

      array = new char[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("byteArray", byte[].class);
      byte[] array = new byte[7];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(7, array.length);
      assertEquals(1, array[0]);
      assertEquals(2, array[1]);
      assertEquals(3, array[2]);
      assertEquals(4, array[3]);
      assertEquals(5, array[4]);
      assertEquals(0, array[5]);
      assertEquals(0, array[6]);

      array = new byte[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(0, array[0]);
      assertEquals(0, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }

    {
      Method m = c.getMethod("booleanArray", boolean[].class);
      boolean[] array = new boolean[5];
      Object[] args = { array };
      m.invoke(null, args);
      assertEquals(5, array.length);
      assertEquals(false, array[0]);
      assertEquals(true, array[1]);
      assertEquals(true, array[2]);
      assertEquals(false, array[3]);
      assertEquals(false, array[4]);

      array = new boolean[2];
      args[0] = array;
      Throwable exception = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof IndexOutOfBoundsException);
      }
      assertNotNull(exception);
      exception = null;
      // Test that nothing has been written to the array.
      assertEquals(false, array[0]);
      assertEquals(false, array[1]);

      args[0] = null;
      try {
        m.invoke(null, args);
      } catch (InvocationTargetException e) {
        exception = e.getCause();
        assertTrue(exception instanceof NullPointerException);
      }
      assertNotNull(exception);
    }
  }
}
