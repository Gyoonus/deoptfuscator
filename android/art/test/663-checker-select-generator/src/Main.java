/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.lang.reflect.Method;

public class Main {
  public static class Super {}
  public static class Sub1 {}
  public static class Sub2 {}

  public static void assertTrue(boolean result) {
    if (!result) {
      throw new Error("Expected true");
    }
  }

  public static void assertFalse(boolean result) {
    if (result) {
      throw new Error("Expected false");
    }
  }

  public static void assertInstanceOfSub1(Object result) {
    if (!(result instanceof Sub1)) {
      throw new Error("Expected instance of Sub1");
    }
  }

  public static void assertInstanceOfSub2(Object result) {
    if (!(result instanceof Sub2)) {
      throw new Error("Expected instance of Sub2");
    }
  }

  public static void main(String[] args) throws Throwable {
    Class<?> c = Class.forName("TestCase");
    Method m = c.getMethod("testCase", boolean.class);
    Method m2 = c.getMethod("referenceTypeTestCase", Sub1.class, Sub2.class, boolean.class);

    try {
      assertTrue((Boolean) m.invoke(null, true));
      assertFalse((Boolean) m.invoke(null, false));
      assertInstanceOfSub1(m2.invoke(null, new Sub1(), new Sub2(), true));
      assertInstanceOfSub2(m2.invoke(null, new Sub1(), new Sub2(), false));
    } catch (Exception e) {
      throw new Error(e);
    }
  }
}
