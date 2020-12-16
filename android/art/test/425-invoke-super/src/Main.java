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
  static class A {
    public int foo() { return 1; }
  }

  static class B extends A {
    public int $opt$bar() { return super.foo(); }
  }

  static class C extends B {
    public int foo() { return 42; }
  }

  static class D extends C {
  }

  static void assertEquals(int expected, int value) {
    if (expected != value) {
      throw new Error("Expected " + expected + ", got " + value);
    }
  }

  public static void main(String[] args) throws Exception {
    // Workaround for b/18051191.
    System.out.println("Test started");
    assertEquals(1, new B().$opt$bar());
    assertEquals(1, new C().$opt$bar());
    assertEquals(1, new D().$opt$bar());

    Class<?> c = Class.forName("InvokeSuper");
    Method m = c.getMethod("run");
    assertEquals(42, ((Integer)m.invoke(c.newInstance(), new Object[0])).intValue());

    c = Class.forName("SubClass");
    assertEquals(42, ((Integer)m.invoke(c.newInstance(), new Object[0])).intValue());
  }
}
