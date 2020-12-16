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

public class Main {
  public static void expectEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] unused) throws Exception {
    Class<?> cls = Class.forName("TestCase");

    cls.getMethod("setByteStaticField").invoke(null);
    expectEquals(1, cls.getField("staticByteField").getByte(null));

    cls.getMethod("setShortStaticField").invoke(null);
    expectEquals(0x101, cls.getField("staticShortField").getShort(null));

    cls.getMethod("setCharStaticField").invoke(null);
    expectEquals(0x101, cls.getField("staticCharField").getChar(null));

    {
      Object[] args = { new byte[2] };
      cls.getMethod("setByteArray", byte[].class).invoke(null, args);
      expectEquals(1, ((byte[])args[0])[0]);
    }
    {
      Object[] args = { new short[2] };
      cls.getMethod("setShortArray", short[].class).invoke(null, args);
      expectEquals(0x101, ((short[])args[0])[0]);
    }
    {
      Object[] args = { new char[2] };
      cls.getMethod("setCharArray", char[].class).invoke(null, args);
      expectEquals(0x101, ((char[])args[0])[0]);
    }
    {
      Object[] args = { cls.newInstance() };

      cls.getMethod("setByteInstanceField", cls).invoke(null, args);
      expectEquals(1, cls.getField("staticByteField").getByte(args[0]));

      cls.getMethod("setShortInstanceField", cls).invoke(null, args);
      expectEquals(0x101, cls.getField("staticShortField").getShort(args[0]));

      cls.getMethod("setCharInstanceField", cls).invoke(null, args);
      expectEquals(0x101, cls.getField("staticCharField").getChar(args[0]));
    }
  }
}
