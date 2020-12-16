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

interface Iface {
  public void foo(int i);
}

abstract class Base implements Iface {
  // Iface.foo(int) will be added as a miranda method.

  void printError(String msg) {
    System.out.println(msg);
  }
}

class Main1 extends Base {
  public void foo(int i) {
    if (i != 1) {
      printError("error1");
    }
  }
}

class Main2 extends Main1 {
  public void foo(int i) {
    if (i != 2) {
      printError("error2");
    }
  }
}

public class Main {
  static Base sMain1;
  static Base sMain2;

  static boolean sIsOptimizing = true;
  static boolean sHasJIT = true;
  static volatile boolean sOtherThreadStarted;

  private static void assertSingleImplementation(Class<?> clazz, String method_name, boolean b) {
    if (hasSingleImplementation(clazz, method_name) != b) {
      System.out.println(clazz + "." + method_name +
          " doesn't have single implementation value of " + b);
    }
  }

  // sMain1.foo() will be always be Main1.foo() before Main2 is loaded/linked.
  // So sMain1.foo() can be devirtualized to Main1.foo() and be inlined.
  // After Dummy.createMain2() which links in Main2, live testOverride() on stack
  // should be deoptimized.
  static void testOverride(boolean createMain2, boolean wait, boolean setHasJIT) {
    if (setHasJIT) {
      if (isInterpreted()) {
        sHasJIT = false;
      }
      return;
    }

    if (createMain2 && (sIsOptimizing || sHasJIT)) {
      assertIsManaged();
    }

    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);

    if (createMain2) {
      // Wait for the other thread to start.
      while (!sOtherThreadStarted);
      // Create an Main2 instance and assign it to sMain2.
      // sMain1 is kept the same.
      sMain2 = Dummy.createMain2();
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

    // There should be a deoptimization here right after Main2 is linked by
    // calling Dummy.createMain2(), even though sMain1 didn't change.
    // The behavior here would be different if inline-cache is used, which
    // doesn't deoptimize since sMain1 still hits the type cache.
    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);
    if ((createMain2 || wait) && sHasJIT && !sIsOptimizing) {
      // This method should be deoptimized right after Main2 is created.
      assertIsInterpreted();
    }

    if (sMain2 != null) {
      sMain2.foo(sMain2.getClass() == Main1.class ? 1 : 2);
    }
  }

  // Test scenarios under which CHA-based devirtualization happens,
  // and class loading that overrides a method can invalidate compiled code.
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    if (isInterpreted()) {
      sIsOptimizing = false;
    }

    // sMain1 is an instance of Main1. Main2 hasn't bee loaded yet.
    sMain1 = new Main1();

    ensureJitCompiled(Main.class, "testOverride");
    testOverride(false, false, true);

    if (sHasJIT && !sIsOptimizing) {
      assertSingleImplementation(Base.class, "foo", true);
      assertSingleImplementation(Main1.class, "foo", true);
    } else {
      // Main2 is verified ahead-of-time so it's linked in already.
    }

    // Create another thread that also calls sMain1.foo().
    // Try to test suspend and deopt another thread.
    new Thread() {
      public void run() {
        testOverride(false, true, false);
      }
    }.start();

    // This will create Main2 instance in the middle of testOverride().
    testOverride(true, false, false);
    assertSingleImplementation(Base.class, "foo", false);
    assertSingleImplementation(Main1.class, "foo", false);
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
  private static native void assertIsInterpreted();
  private static native void assertIsManaged();
  private static native boolean isInterpreted();
  private static native boolean hasSingleImplementation(Class<?> clazz, String method_name);
}

// Put createMain2() in another class to avoid class loading due to verifier.
class Dummy {
  static Main1 createMain2() {
    return new Main2();
  }
}
