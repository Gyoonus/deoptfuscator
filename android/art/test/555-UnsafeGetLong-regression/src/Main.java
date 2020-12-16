/*
 * Copyright (C) 2015 The Android Open Source Project
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
import sun.misc.Unsafe;

public class Main {
  private static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static Unsafe getUnsafe() throws Exception {
    Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
    Field f = unsafeClass.getDeclaredField("theUnsafe");
    f.setAccessible(true);
    return (Unsafe) f.get(null);
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Unsafe unsafe = getUnsafe();

    testUnsafeGetLong(unsafe);
  }

  public static void testUnsafeGetLong(Unsafe unsafe) throws Exception {
    TestClass test = new TestClass();
    Field longField = TestClass.class.getDeclaredField("longVar");
    long lvar = unsafe.objectFieldOffset(longField);
    lvar = unsafe.getLong(test, lvar);
    assertLongEquals(1122334455667788L, lvar);
  }

  private static class TestClass {
    public long longVar = 1122334455667788L;
  }
}
