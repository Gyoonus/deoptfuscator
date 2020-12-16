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
import java.lang.reflect.Field;

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  public static void main(String[] args) throws Exception {
    Class<?> c = Class.forName("StoreLoad");
    Method m = c.getMethod("test", int.class);
    int result = (Integer)m.invoke(null, 0x12345678);
    if (result != (0x78 + 0x78)) {
      throw new Error("Expected 240, got " + result);
    }
    m = c.getMethod("test2", int.class);
    result = (Integer)m.invoke(null, 0xdeadbeef);
    if (result != 0xdeadbeef) {
      throw new Error("Expected 0xdeadbeef, got " + result);
    }
    Field f = c.getDeclaredField("byteField");
    byte b = f.getByte(null);
    if (b != (byte)0xef) {
      throw new Error("Expected 0xef, got " + b);
    }
    f = c.getDeclaredField("byteField2");
    b = f.getByte(null);
    if (b != (byte)0x78) {
      throw new Error("Expected 0xef, got " + b);
    }
  }
}
