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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

class Main1 {
  void foo(int i) {
    if (i != 1) {
      printError("error1");
    }
  }

  void printError(String msg) {
    System.out.println(msg);
  }
}

class Main2 extends Main1 {
  void foo(int i) {
    if (i != 2) {
      printError("error2");
    }
  }
}

class Proxied implements Runnable {
  public void run() {
    synchronized(Main.class) {
      Main.sOtherThreadStarted = true;
      // Wait for Main2 to be linked and deoptimization is triggered.
      try {
        Main.class.wait();
      } catch (Exception e) {
      }
    }
  }
}

class MyInvocationHandler implements InvocationHandler {
  private final Proxied proxied;

  public MyInvocationHandler(Proxied proxied) {
    this.proxied = proxied;
  }

  public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
    return method.invoke(proxied, args);
  }
}

public class Main {
  static Main1 sMain1;
  static Main1 sMain2;
  static volatile boolean sOtherThreadStarted;

  // sMain1.foo() will be always be Main1.foo() before Main2 is loaded/linked.
  // So sMain1.foo() can be devirtualized to Main1.foo() and be inlined.
  // After Dummy.createMain2() which links in Main2, live testOverride() on stack
  // should be deoptimized.
  static void testOverride() {
    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);

    // Wait for the other thread to start.
    while (!sOtherThreadStarted);
    // Create an Main2 instance and assign it to sMain2.
    // sMain1 is kept the same.
    sMain2 = Dummy.createMain2();
    // Wake up the other thread.
    synchronized(Main.class) {
      Main.class.notify();
    }

    // There should be a deoptimization here right after Main2 is linked by
    // calling Dummy.createMain2(), even though sMain1 didn't change.
    // The behavior here would be different if inline-cache is used, which
    // doesn't deoptimize since sMain1 still hits the type cache.
    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);
    if (sMain2 != null) {
      sMain2.foo(sMain2.getClass() == Main1.class ? 1 : 2);
    }
  }

  // Test scenarios under which CHA-based devirtualization happens,
  // and class loading that overrides a method can invalidate compiled code.
  // Also create a proxy method such that a proxy method's frame is visited
  // during stack walking.
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    // sMain1 is an instance of Main1. Main2 hasn't bee loaded yet.
    sMain1 = new Main1();

    // Create another thread that calls a proxy method.
    new Thread() {
      public void run() {
        Runnable proxy = (Runnable)Proxy.newProxyInstance(
            Proxied.class.getClassLoader(),
            new Class[] { Runnable.class },
            new MyInvocationHandler(new Proxied()));
        proxy.run();
      }
    }.start();

    ensureJitCompiled(Main.class, "testOverride");
    // This will create Main2 instance in the middle of testOverride().
    testOverride();
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
}

// Put createMain2() in another class to avoid class loading due to verifier.
class Dummy {
  static Main1 createMain2() {
    return new Main2();
  }
}
