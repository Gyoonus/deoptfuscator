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

// Simple test for array accesses.

public class Main extends TestCase {
  public static void main(String[] args) {
    $opt$testReads(new boolean[1], new byte[1], new char[1], new short[1],
                   new int[1], new Object[1], new long[1], new float[1], new double[1], 0);
    $opt$testWrites(new boolean[2], new byte[2], new char[2], new short[2],
                    new int[2], new Object[2], new long[2], new float[2], new double[2], 1);
    ensureThrows(new boolean[2], 2);
    ensureThrows(new boolean[2], 4);
    ensureThrows(new boolean[2], -1);
    ensureThrows(new boolean[2], Integer.MIN_VALUE);
    ensureThrows(new boolean[2], Integer.MAX_VALUE);
  }

  static void $opt$testReads(boolean[] bools, byte[] bytes, char[] chars, short[] shorts,
                             int[] ints, Object[] objects, long[] longs, float[] floats,
                             double[] doubles, int index) {
    assertEquals(false, bools[0]);
    assertEquals(false, bools[index]);

    assertEquals(0, bytes[0]);
    assertEquals(0, bytes[index]);

    assertEquals(0, chars[0]);
    assertEquals(0, chars[index]);

    assertEquals(0, shorts[0]);
    assertEquals(0, shorts[index]);

    assertEquals(0, ints[0]);
    assertEquals(0, ints[index]);

    assertNull(objects[0]);
    assertNull(objects[index]);

    assertEquals(0, longs[0]);
    assertEquals(0, longs[index]);

    assertEquals(0, floats[0]);
    assertEquals(0, floats[index]);

    assertEquals(0, doubles[0]);
    assertEquals(0, doubles[index]);
  }

  static void $opt$testWrites(boolean[] bools, byte[] bytes, char[] chars, short[] shorts,
                              int[] ints, Object[] objects, long[] longs, float[] floats,
                              double doubles[], int index) {
    bools[0] = true;
    assertEquals(true, bools[0]);
    bools[index] = true;
    assertEquals(true, bools[index]);

    bytes[0] = -4;
    assertEquals(-4, bytes[0]);
    bytes[index] = -8;
    assertEquals(-8, bytes[index]);

    chars[0] = 'c';
    assertEquals('c', chars[0]);
    chars[index] = 'd';
    assertEquals('d', chars[index]);

    chars[0] = 65535;
    assertEquals(65535, chars[0]);
    // Do an update between the two max value updates, to avoid
    // optimizing the second away.
    chars[index] = 0;
    assertEquals(0, chars[index]);
    chars[index] = 65535;
    assertEquals(65535, chars[index]);

    shorts[0] = -42;
    assertEquals(-42, shorts[0]);
    shorts[index] = -84;
    assertEquals(-84, shorts[index]);

    ints[0] = -32;
    assertEquals(-32, ints[0]);
    ints[index] = -64;
    assertEquals(-64, ints[index]);

    Object o1 = new Object();
    objects[0] = o1;
    assertEquals(o1, objects[0]);
    Object o2 = new Object();
    objects[index] = o2;
    assertEquals(o2, objects[index]);
    // Longs are initially not supported in the linear scan register allocator
    // on 32bits. So we call out a long helper to ensure this method gets
    // optimized.
    $opt$testLongWrites(longs, index);

    float f = 1.0F;
    floats[0] = f / (1 | 0);
    assertEquals(f, floats[0]);
    floats[index] = f / (1 | 0);
    assertEquals(f, floats[index]);

    double d = 1.0F;
    doubles[0] = d / (1 | 0);
    assertEquals(d, doubles[0]);
    doubles[index] = d / (1 | 0);
    assertEquals(d, doubles[index]);
  }

  public static void $opt$testLongWrites(long[] longs, int index) {
    long l = -21876876876876876L;
    longs[0] = l;
    assertEquals(l, longs[0]);
    l = -21876876876876877L;
    longs[index] = l;
    assertEquals(l, longs[index]);
  }

  public static void ensureThrows(boolean[] array, int index) {
    ArrayIndexOutOfBoundsException exception = null;
    try {
      $opt$doArrayLoad(array, index);
    } catch (ArrayIndexOutOfBoundsException e) {
      exception = e;
    }

    assertNotNull(exception);
    assertTrue(exception.toString().contains(Integer.toString(index)));

    exception = null;
    try {
      $opt$doArrayStore(array, index);
    } catch (ArrayIndexOutOfBoundsException e) {
      exception = e;
    }

    assertNotNull(exception);
    assertTrue(exception.toString().contains(Integer.toString(index)));
  }

  public static void $opt$doArrayLoad(boolean[] array, int index) {
    boolean res = array[index];
  }

  public static void $opt$doArrayStore(boolean[] array, int index) {
    array[index] = false;
  }
}
