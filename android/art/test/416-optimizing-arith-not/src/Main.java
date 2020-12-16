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

import java.lang.reflect.Method;

public class Main {

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) throws Exception {
    notInt();
    notLong();
  }

  private static void notInt() throws Exception {
    expectEquals(1, smaliNotInt(-2));
    expectEquals(0, smaliNotInt(-1));
    expectEquals(-1, smaliNotInt(0));
    expectEquals(-2, smaliNotInt(1));
    expectEquals(2147483647, smaliNotInt(-2147483648));  // -(2^31)
    expectEquals(2147483646, smaliNotInt(-2147483647));  // -(2^31 - 1)
    expectEquals(-2147483647, smaliNotInt(2147483646));  // 2^31 - 2
    expectEquals(-2147483648, smaliNotInt(2147483647));  // 2^31 - 1
  }

  private static void notLong() throws Exception {
    expectEquals(1L, smaliNotLong(-2L));
    expectEquals(0L, smaliNotLong(-1L));
    expectEquals(-1L, smaliNotLong(0L));
    expectEquals(-2L, smaliNotLong(1L));
    expectEquals(2147483647L, smaliNotLong(-2147483648L));  // -(2^31)
    expectEquals(2147483646L, smaliNotLong(-2147483647L));  // -(2^31 - 1)
    expectEquals(-2147483647L, smaliNotLong(2147483646L));  // 2^31 - 2
    expectEquals(-2147483648L, smaliNotLong(2147483647L));  // 2^31 - 1
    expectEquals(9223372036854775807L, smaliNotLong(-9223372036854775808L));  // -(2^63)
    expectEquals(9223372036854775806L, smaliNotLong(-9223372036854775807L));  // -(2^63 - 1)
    expectEquals(-9223372036854775807L, smaliNotLong(9223372036854775806L));  // 2^63 - 2
    expectEquals(-9223372036854775808L, smaliNotLong(9223372036854775807L));  // 2^63 - 1
  }

  // Wrappers around methods located in file not.smali.

  private static int smaliNotInt(int a) throws Exception {
    Class<?> c = Class.forName("TestNot");
    Method m = c.getMethod("$opt$NotInt", int.class);
    int result = (Integer)m.invoke(null, a);
    return result;
  }

  private static long smaliNotLong(long a) throws Exception {
    Class<?> c = Class.forName("TestNot");
    Method m = c.getMethod("$opt$NotLong", long.class);
    long result = (Long)m.invoke(null, a);
    return result;
  }
}
