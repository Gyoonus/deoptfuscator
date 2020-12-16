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

public class Main {
  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void assertEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) {
    assertEquals(0, $noinline$divInt(1));
    assertEquals(1, $noinline$remInt(1));

    assertEquals(0, $noinline$divInt(-1));
    assertEquals(-1, $noinline$remInt(-1));

    assertEquals(0, $noinline$divInt(0));
    assertEquals(0, $noinline$remInt(0));

    assertEquals(1, $noinline$divInt(Integer.MIN_VALUE));
    assertEquals(0, $noinline$remInt(Integer.MIN_VALUE));

    assertEquals(0, $noinline$divInt(Integer.MAX_VALUE));
    assertEquals(Integer.MAX_VALUE, $noinline$remInt(Integer.MAX_VALUE));

    assertEquals(0, $noinline$divInt(Integer.MAX_VALUE - 1));
    assertEquals(Integer.MAX_VALUE - 1, $noinline$remInt(Integer.MAX_VALUE - 1));

    assertEquals(0, $noinline$divInt(Integer.MIN_VALUE + 1));
    assertEquals(Integer.MIN_VALUE + 1, $noinline$remInt(Integer.MIN_VALUE + 1));

    assertEquals(0L, $noinline$divLong(1L));
    assertEquals(1L, $noinline$remLong(1L));

    assertEquals(0L, $noinline$divLong(-1L));
    assertEquals(-1L, $noinline$remLong(-1L));

    assertEquals(0L, $noinline$divLong(0L));
    assertEquals(0L, $noinline$remLong(0L));

    assertEquals(1L, $noinline$divLong(Long.MIN_VALUE));
    assertEquals(0L, $noinline$remLong(Long.MIN_VALUE));

    assertEquals(0L, $noinline$divLong(Long.MAX_VALUE));
    assertEquals(Long.MAX_VALUE, $noinline$remLong(Long.MAX_VALUE));

    assertEquals(0L, $noinline$divLong(Long.MAX_VALUE - 1));
    assertEquals(Long.MAX_VALUE - 1, $noinline$remLong(Long.MAX_VALUE - 1));

    assertEquals(0L, $noinline$divLong(Integer.MIN_VALUE + 1));
    assertEquals(Long.MIN_VALUE + 1, $noinline$remLong(Long.MIN_VALUE + 1));
  }

  public static int $noinline$divInt(int value) {
    if (doThrow) {
      throw new Error("");
    }
    return value / Integer.MIN_VALUE;
  }

  public static int $noinline$remInt(int value) {
    if (doThrow) {
      throw new Error("");
    }
    return value % Integer.MIN_VALUE;
  }

  public static long $noinline$divLong(long value) {
    if (doThrow) {
      throw new Error("");
    }
    return value / Long.MIN_VALUE;
  }

  public static long $noinline$remLong(long value) {
    if (doThrow) {
      throw new Error("");
    }
    return value % Long.MIN_VALUE;
  }

  static boolean doThrow = false;
}
