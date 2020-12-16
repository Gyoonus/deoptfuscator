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

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
  // Workaround for b/18051191.
  class Inner {}

  public static native void assertIsInterpreted();

  private static void assertEqual(String expected, String actual) {
    if (!expected.equals(actual)) {
      throw new Error("Assertion failed: " + expected + " != " + actual);
    }
  }

  public static void main(String[] args) throws Throwable {
    System.loadLibrary(args[0]);
    Class<?> c = Class.forName("TestCase");
    int[] array = new int[1];

    {
      Method m = c.getMethod("testNoAlias", int[].class, String.class);
      try {
        m.invoke(null, new Object[] { array , "foo" });
        throw new Error("Expected AIOOBE");
      } catch (InvocationTargetException e) {
        if (!(e.getCause() instanceof ArrayIndexOutOfBoundsException)) {
          throw new Error("Expected AIOOBE");
        }
        // Ignore
      }
      Field field = c.getField("staticField");
      assertEqual("foo", (String)field.get(null));
    }

    {
      Method m = c.getMethod("testAlias", int[].class, String.class);
      try {
        m.invoke(null, new Object[] { array, "bar" });
        throw new Error("Expected AIOOBE");
      } catch (InvocationTargetException e) {
        if (!(e.getCause() instanceof ArrayIndexOutOfBoundsException)) {
          throw new Error("Expected AIOOBE");
        }
        // Ignore
      }
      Field field = c.getField("staticField");
      assertEqual("bar", (String)field.get(null));
    }
  }
}
