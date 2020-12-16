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

/**
 * Common superclass for test cases.
 */

import java.util.Arrays;

public abstract class TestCase {
  public static void assertSame(Object expected, Object value) {
    if (expected != value) {
      throw new AssertionError("Objects are not the same: expected " +
          String.valueOf(expected) + ", got " + String.valueOf(value));
    }
  }

  public static void assertNotSame(Object expected, Object value) {
    if (expected == value) {
      throw new AssertionError(
          "Objects are the same: " + String.valueOf(expected));
    }
  }

  public static void assertEquals(String message, int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertTrue(String message, boolean condition) {
    if (!condition) {
      throw new AssertionError(message);
    }
  }

  public static void assertTrue(boolean condition) {
    assertTrue("Expected true", condition);
  }

  public static void assertFalse(String message, boolean condition) {
    if (condition) {
      throw new AssertionError(message);
    }
  }

  public static void assertFalse(boolean condition) {
    assertFalse("Expected false", condition);
  }

  public static void assertEquals(Object expected, Object actual) {
    if (!expected.equals(actual)) {
      String msg = "Expected \"" + expected + "\" but got \"" + actual + "\"";
      throw new AssertionError(msg);
    }
  }

  public static void assertNotEquals(int expected, int actual) {
    if (expected == actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertNotEquals(Object expected, Object actual) {
    if (expected.equals(actual)) {
      String msg = "Objects are the same: " + String.valueOf(expected);
      throw new AssertionError(msg);
    }
  }

  public static <T> void assertArrayEquals(T[] actual, T... expected) {
      assertTrue(Arrays.equals(expected, actual));
  }

  public static void assertEquals(
      String message, Object expected, Object actual) {
    if (!expected.equals(actual)) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(
      String message, long expected, long actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(long expected, long actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertEquals(
      String message, boolean expected, boolean actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(boolean expected, boolean actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertEquals(
      String message, float expected, float actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(float expected, float actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertEquals(float expected, float actual,
                                  float tolerance) {
    if ((actual < expected - tolerance) || (expected + tolerance < actual)) {
      throw new AssertionError("Expected " + expected + " got " + actual +
          " tolerance " + tolerance);
    }
  }

  public static void assertEquals(
      String message, double expected, double actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertEquals(double expected, double actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }

  public static void assertEquals(double expected, double actual,
                                  double tolerance) {
    if ((actual < expected - tolerance) || (expected + tolerance < actual)) {
      throw new AssertionError("Expected " + expected + " got " + actual +
          " tolerance " + tolerance);
    }
  }

  public static void assertSame(
      String message, Object expected, Object actual) {
    if (expected != actual) {
      throw new AssertionError(message);
    }
  }

  public static void assertNull(String message, Object object) {
    if (object != null) {
      throw new AssertionError(message);
    }
  }

  public static void assertNull(Object object) {
    assertNull("Expected null", object);
  }

  public static void assertNotNull(String message, Object object) {
    if (object == null) {
      throw new AssertionError(message);
    }
  }

  public static void assertNotNull(Object object) {
    assertNotNull("Expected non-null", object);
  }

  public static void fail(String msg) {
    throw new AssertionError(msg);
  }
}
