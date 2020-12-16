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

class Main1 implements Base {
}

class Main2 extends Main1 {
  public void foobar() {}
}

class Main3 implements Base {
  public int foo(int i) {
    if (i != 3) {
      printError("error3");
    }
    return -(i + 10);
  }
}

public class Main {
  static Base sMain1;
  static Base sMain2;
  static Base sMain3;

  static boolean sIsOptimizing = true;
  static boolean sHasJIT = true;
  static volatile boolean sOtherThreadStarted;

  private static void assertSingleImplementation(Class<?> clazz, String method_name, boolean b) {
    if (hasSingleImplementation(clazz, method_name) != b) {
      System.out.println(clazz + "." + method_name +
          " doesn't have single implementation value of " + b);
    }
  }

  static int getValue(Class<?> cls) {
    if (cls == Main1.class || cls == Main2.class) {
      return 1;
    }
    return 3;
  }

  // sMain1.foo()/sMain2.foo() will be always be Base.foo() before Main3 is loaded/linked.
  // So sMain1.foo() can be devirtualized to Base.foo() and be inlined.
  // After Dummy.createMain3() which links in Main3, live testImplement() on stack
  // should be deoptimized.
  static void testImplement(boolean createMain3, boolean wait, boolean setHasJIT) {
    if (setHasJIT) {
      if (isInterpreted()) {
        sHasJIT = false;
      }
      return;
    }

    if (createMain3 && (sIsOptimizing || sHasJIT)) {
      assertIsManaged();
    }

    if (sMain1.foo(getValue(sMain1.getClass())) != 11) {
      System.out.println("11 expected.");
    }
    if (sMain1.$noinline$bar() != -1) {
      System.out.println("-1 expected.");
    }
    if (sMain2.foo(getValue(sMain2.getClass())) != 11) {
      System.out.println("11 expected.");
    }

    if (createMain3) {
      // Wait for the other thread to start.
      while (!sOtherThreadStarted);
      // Create an Main2 instance and assign it to sMain2.
      // sMain1 is kept the same.
      sMain3 = Dummy.createMain3();
      // Wake up the other thread.
      synchronized(Main.class) {
        Main.class.notify();
      }
    } else if (wait) {
      // This is the other thread.
      synchronized(Main.class) {
        sOtherThreadStarted = true;
        // Wait for Main2 to be linked and deoptimization is triggered.
        try {
          Main.class.wait();
        } catch (Exception e) {
        }
      }
    }

    // There should be a deoptimization here right after Main3 is linked by
    // calling Dummy.createMain3(), even though sMain1 didn't change.
    // The behavior here would be different if inline-cache is used, which
    // doesn't deoptimize since sMain1 still hits the type cache.
    if (sMain1.foo(getValue(sMain1.getClass())) != 11) {
      System.out.println("11 expected.");
    }
    if ((createMain3 || wait) && sHasJIT && !sIsOptimizing) {
      // This method should be deoptimized right after Main3 is created.
      assertIsInterpreted();
    }

    if (sMain3 != null) {
      if (sMain3.foo(getValue(sMain3.getClass())) != -13) {
        System.out.println("-13 expected.");
      }
    }
  }

  // Test scenarios under which CHA-based devirtualization happens,
  // and class loading that implements a method can invalidate compiled code.
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    if (isInterpreted()) {
      sIsOptimizing = false;
    }

    // sMain1 is an instance of Main1.
    // sMain2 is an instance of Main2.
    // Neither Main1 nor Main2 override default method Base.foo().
    // Main3 hasn't bee loaded yet.
    sMain1 = new Main1();
    sMain2 = new Main2();

    ensureJitCompiled(Main.class, "testImplement");
    testImplement(false, false, true);

    if (sHasJIT && !sIsOptimizing) {
      assertSingleImplementation(Base.class, "foo", true);
      assertSingleImplementation(Main1.class, "foo", true);
    } else {
      // Main3 is verified ahead-of-time so it's linked in already.
    }

    // Create another thread that also calls sMain1.foo().
    // Try to test suspend and deopt another thread.
    new Thread() {
      public void run() {
        testImplement(false, true, false);
      }
    }.start();

    // This will create Main3 instance in the middle of testImplement().
    testImplement(true, false, false);
    assertSingleImplementation(Base.class, "foo", false);
    assertSingleImplementation(Main1.class, "foo", true);
    assertSingleImplementation(sMain3.getClass(), "foo", true);
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
  private static native void assertIsInterpreted();
  private static native void assertIsManaged();
  private static native boolean isInterpreted();
  private static native boolean hasSingleImplementation(Class<?> clazz, String method_name);
}

// Put createMain3() in another class to avoid class loading due to verifier.
class Dummy {
  static Base createMain3() {
    return new Main3();
  }
}
