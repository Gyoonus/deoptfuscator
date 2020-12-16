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

package art;

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Set;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.function.Consumer;

public class Test1914 {
  public static final String TARGET_VAR = "TARGET";

  public static void reportValue(Object val) {
    System.out.println("\tValue is '" + val + "' (class: "
        + (val != null ? (val instanceof Proxy ? "PROXY CLASS" : val.getClass()) : "NULL") + ")");
  }

  public static void StaticMethod(Runnable safepoint) {
    safepoint.run();
    reportValue(null);
  }

  public static native void NativeStaticMethod(Runnable safepoint);

  public static class TargetClass {
    public String id;
    public String toString() { return String.format("TargetClass(\"%s\")", id); }
    public TargetClass(String id) { this.id = id; }

    public void InstanceMethod(Runnable safepoint) {
      safepoint.run();
      reportValue(this);
    }

    public native void NativeInstanceMethod(Runnable safepoint);
  }

  public static interface SafepointFunction {
    public void invoke(
        Thread thread,
        Method target,
        int depth) throws Exception;
  }

  public static interface GetterFunction {
    public Object GetVar(Thread t, int depth);
  }

  public static SafepointFunction NamedGet(final String type, final GetterFunction get) {
    return new SafepointFunction() {
      public void invoke(Thread t, Method method, int depth) {
        try {
          Object res = get.GetVar(t, depth);
          System.out.println(this + " on " + method + " got value: " + res);
        } catch (Exception e) {
          System.out.println(this + " on " + method + " failed due to " + e.getMessage());
        }
      }
      public String toString() {
        return "\"Get" + type + "\"";
      }
    };
  }

  public static class TestCase {
    public final Object thiz;
    public final Method target;

    public TestCase(Method target) {
      this(null, target);
    }
    public TestCase(Object thiz, Method target) {
      this.thiz = thiz;
      this.target = target;
    }

    public static class ThreadPauser implements Runnable {
      public final Semaphore sem_wakeup_main;
      public final Semaphore sem_wait;

      public ThreadPauser() {
        sem_wakeup_main = new Semaphore(0);
        sem_wait = new Semaphore(0);
      }

      public void run() {
        try {
          sem_wakeup_main.release();
          sem_wait.acquire();
        } catch (Exception e) {
          throw new Error("Error with semaphores!", e);
        }
      }

      public void waitForOtherThreadToPause() throws Exception {
        sem_wakeup_main.acquire();
      }

      public void wakeupOtherThread() throws Exception {
        sem_wait.release();
      }
    }

    public void exec(final SafepointFunction safepoint) throws Exception {
      System.out.println("Running " + target + " with " + safepoint + " on remote thread.");
      final ThreadPauser pause = new ThreadPauser();
      Thread remote = new Thread(
          () -> {
            try {
              target.invoke(thiz, pause);
            } catch (Exception e) {
              throw new Error("Error invoking remote thread " + Thread.currentThread(), e);
            }
          },
          "remote thread for " + target + " with " + safepoint);
      remote.start();
      pause.waitForOtherThreadToPause();
      try {
        Suspension.suspend(remote);
        StackTrace.StackFrameData frame = findStackFrame(remote);
        safepoint.invoke(remote, target, frame.depth);
      } finally {
        Suspension.resume(remote);
        pause.wakeupOtherThread();
        remote.join();
      }
    }

    private StackTrace.StackFrameData findStackFrame(Thread thr) {
      for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(thr)) {
        if (frame.method.equals(target) ||
            (frame.method.getName().equals(target.getName()) &&
             Arrays.deepEquals(frame.method.getParameterTypes(), target.getParameterTypes()) &&
             ((Method)frame.method).getReturnType().equals(target.getReturnType()))) {
          return frame;
        }
      }
      throw new Error("Unable to find stack frame in method " + target + " on thread " + thr);
    }
  }

  public static Method getMethod(Class<?> klass, String name) throws Exception {
    return klass.getDeclaredMethod(name, Runnable.class);
  }

  public static interface Foo {
    public void InterfaceProxyMethod(Runnable r);
  }

  public static Object getProxyObject(final Class... k) {
    return Proxy.newProxyInstance(
        Test1914.class.getClassLoader(),
        k,
        (p, m, a) -> {
          if (m.getName().equals("toString")) {
            return "Proxy for " + Arrays.toString(k);
          } else {
            ((Runnable)a[0]).run();
            reportValue(p);
            return null;
          }
        });
  }

  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    final TestCase[] MAIN_TEST_CASES = new TestCase[] {
      new TestCase(null, getMethod(Test1914.class, "StaticMethod")),
      new TestCase(null, getMethod(Test1914.class, "NativeStaticMethod")),
      new TestCase(new TargetClass("InstanceMethodObject"),
                   getMethod(TargetClass.class, "InstanceMethod")),
      new TestCase(new TargetClass("NativeInstanceMethodObject"),
                   getMethod(TargetClass.class, "NativeInstanceMethod")),
      new TestCase(getProxyObject(Foo.class),
                   getMethod(Foo.class, "InterfaceProxyMethod")),
    };

    for (TestCase t: MAIN_TEST_CASES) {
      t.exec(NamedGet("This", Locals::GetLocalInstance));
    }
  }
}

