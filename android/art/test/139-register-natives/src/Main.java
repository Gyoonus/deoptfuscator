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

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    testRegistration1();
    testRegistration2();
    testRegistration3();
  }

  // Test that a subclass' method is registered instead of a superclass' method.
  private static void testRegistration1() {
    registerNatives(TestSub.class);

    expectNotThrows(new TestSub());
    expectThrows(new TestSuper());
  }

  // Test that a superclass' method is registered if the subclass doesn't have a matching method.
  private static void testRegistration2() {
    registerNatives(TestSub2.class);

    expectNotThrows(new TestSub2());
    expectNotThrows(new TestSuper2());
  }

  // Test that registration fails if the subclass has a matching non-native method.
  private static void testRegistration3() {
    try {
      registerNatives(TestSub3.class);
      System.out.println("Expected exception for registerNatives(TestSub3.class)");
    } catch (NoSuchMethodError ignored) {
    }
  }

  private native static int registerNatives(Class<?> c);

  private static void expectThrows(Base b) {
    try {
      b.callMyFoo();
      System.out.println("Expected exception for " + b.getClass().getName());
    } catch (Throwable ignored) {
    }
  }

  private static void expectNotThrows(Base b) {
    try {
      b.callMyFoo();
    } catch (Throwable t) {
      System.out.println("Did not expect an exception for " + b.getClass().getName());
      t.printStackTrace(System.out);
    }
  }
}

abstract class Base {
  public abstract void callMyFoo();
}

class TestSuper extends Base {
  private native void foo();

  @Override
  public void callMyFoo() {
    foo();
  }
}

class TestSub extends TestSuper {
  public native void foo();

  @Override
  public void callMyFoo() {
    foo();
  }
}

class TestSuper2 extends Base{
  public native void foo();

  @Override
  public void callMyFoo() {
    foo();
  }
}

class TestSub2 extends TestSuper2 {
}

class TestSuper3 extends Base {
  public native void foo();

  @Override
  public void callMyFoo() {
    foo();
  }
}

class TestSub3 extends TestSuper3 {
  public void foo() {
    System.out.println("TestSub3.foo()");
  }
}
