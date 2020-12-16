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

public class Test1939 {
  public static interface SafepointFunction {
    public void invoke(
        Thread thread,
        Method target,
        int depth) throws Exception;
  }

  public static interface GetterFunction {
    public Object GetVar(Thread t, int depth);
  }

  public static String SafeToString(Object o) {
    if (o instanceof Method && Proxy.isProxyClass(((Method)o).getDeclaringClass())) {
      // TODO This currently only really works on ART. It would be good if we could make it work for
      // the RI as well.
      return o.toString().replaceFirst("Proxy[0-9]+", "__PROXY__");
    } else {
      return o.toString();
    }
  }

  public static SafepointFunction NamedGet(final String type, final GetterFunction get) {
    return new SafepointFunction() {
      public void invoke(Thread t, Method method, int depth) {
        try {
          Object res = get.GetVar(t, depth);
          System.out.println(this + " on " + method + " got value: " + SafeToString(res));
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
        Test1939.class.getClassLoader(),
        k,
        (p, m, a) -> {
          if (m.getName().equals("toString")) {
            return "Proxy for " + Arrays.toString(k);
          } else {
            ((Runnable)a[0]).run();
            return null;
          }
        });
  }

  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    TestCase test = new TestCase(
        getProxyObject(Foo.class), getMethod(Foo.class, "InterfaceProxyMethod"));
    test.exec(NamedGet("This", Locals::GetLocalInstance));
    test.exec(NamedGet("LocalReference0", (t, d) -> Locals.GetLocalVariableObject(t, d, 0)));
    test.exec(NamedGet("ProxyFrameLocation", (t, d) -> Long.valueOf(GetFrameLocation(t, d))));
    test.exec(NamedGet("ProxyFrameMethod", Test1939::GetFrameMethod));
  }

  public static native long GetFrameLocation(Thread thr, int depth);
  public static native Executable GetFrameMethod(Thread thr, int depth);
}

