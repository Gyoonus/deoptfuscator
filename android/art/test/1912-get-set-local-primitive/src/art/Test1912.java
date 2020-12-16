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
import java.lang.reflect.Method;
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

// TODO Rename test to set-get-local-prim

public class Test1912 {
  public static final String TARGET_VAR = "TARGET";


  public static void reportValue(Object val) {
    if (val instanceof Character) {
      val = "<Char: " + Character.getNumericValue(((Character)val).charValue()) + ">";
    }
    System.out.println("\tValue is '" + val + "' (class: " + val.getClass() + ")");
  }

  public static void BooleanMethod(Runnable safepoint) {
    boolean TARGET = false;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void ByteMethod(Runnable safepoint) {
    byte TARGET = 8;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void CharMethod(Runnable safepoint) {
    char TARGET = 'q';
    safepoint.run();
    reportValue(TARGET);
  }
  public static void ShortMethod(Runnable safepoint) {
    short TARGET = 321;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void IntMethod(Runnable safepoint) {
    int TARGET = 42;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void LongMethod(Runnable safepoint) {
    long TARGET = 9001;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void FloatMethod(Runnable safepoint) {
    float TARGET = 1.618f;
    safepoint.run();
    reportValue(TARGET);
  }
  public static void DoubleMethod(Runnable safepoint) {
    double TARGET = 3.1415d;
    safepoint.run();
    reportValue(TARGET);
  }

  public static interface SafepointFunction {
    public void invoke(
        Thread thread,
        Method target,
        Locals.VariableDescription TARGET_desc,
        int depth) throws Exception;
  }

  public static interface SetterFunction {
    public void SetVar(Thread t, int depth, int slot, Object v);
  }

  public static interface GetterFunction {
    public Object GetVar(Thread t, int depth, int slot);
  }

  public static SafepointFunction NamedSet(
      final String type, final SetterFunction get, final Object v) {
    return new SafepointFunction() {
      public void invoke(Thread t, Method method, Locals.VariableDescription desc, int depth) {
        try {
          get.SetVar(t, depth, desc.slot, v);
          System.out.println(this + " on " + method + " set value: " + v);
        } catch (Exception e) {
          System.out.println(
              this + " on " + method + " failed to set value " + v + " due to " + e.getMessage());
        }
      }
      public String toString() {
        return "\"Set" + type + "\"";
      }
    };
  }

  public static SafepointFunction NamedGet(final String type, final GetterFunction get) {
    return new SafepointFunction() {
      public void invoke(Thread t, Method method, Locals.VariableDescription desc, int depth) {
        try {
          Object res = get.GetVar(t, depth, desc.slot);
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
    public final Method target;

    public TestCase(Method target) {
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
              target.invoke(null, pause);
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
        Locals.VariableDescription desc = findTargetVar(frame.current_location);
        safepoint.invoke(remote, target, desc, frame.depth);
      } finally {
        Suspension.resume(remote);
        pause.wakeupOtherThread();
        remote.join();
      }
    }

    private Locals.VariableDescription findTargetVar(long loc) {
      for (Locals.VariableDescription var : Locals.GetLocalVariableTable(target)) {
        if (var.start_location <= loc &&
            var.length + var.start_location > loc &&
            var.name.equals(TARGET_VAR)) {
          return var;
        }
      }
      throw new Error(
          "Unable to find variable " + TARGET_VAR + " in " + target + " at loc " + loc);
    }

    private StackTrace.StackFrameData findStackFrame(Thread thr) {
      for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(thr)) {
        if (frame.method.equals(target)) {
          return frame;
        }
      }
      throw new Error("Unable to find stack frame in method " + target + " on thread " + thr);
    }
  }
  public static Method getMethod(String name) throws Exception {
    return Test1912.class.getDeclaredMethod(name, Runnable.class);
  }

  public static void run() throws Exception {
    Locals.EnableLocalVariableAccess();
    final TestCase[] MAIN_TEST_CASES = new TestCase[] {
      new TestCase(getMethod("IntMethod")),
      new TestCase(getMethod("LongMethod")),
      new TestCase(getMethod("FloatMethod")),
      new TestCase(getMethod("DoubleMethod")),
    };

    final SafepointFunction[] SAFEPOINTS = new SafepointFunction[] {
      NamedGet("Int", Locals::GetLocalVariableInt),
      NamedGet("Long", Locals::GetLocalVariableLong),
      NamedGet("Float", Locals::GetLocalVariableFloat),
      NamedGet("Double", Locals::GetLocalVariableDouble),
      NamedSet("Int", Locals::SetLocalVariableInt, Integer.MAX_VALUE),
      NamedSet("Long", Locals::SetLocalVariableLong, Long.MAX_VALUE),
      NamedSet("Float", Locals::SetLocalVariableFloat, 9.2f),
      NamedSet("Double", Locals::SetLocalVariableDouble, 12.4d),
    };

    for (TestCase t: MAIN_TEST_CASES) {
      for (SafepointFunction s : SAFEPOINTS) {
        t.exec(s);
      }
    }

    // Test int for small values.
    new TestCase(getMethod("BooleanMethod")).exec(
        NamedSet("IntBoolSize", Locals::SetLocalVariableInt, 1));
    new TestCase(getMethod("ByteMethod")).exec(
      NamedSet("IntByteSize", Locals::SetLocalVariableInt, Byte.MAX_VALUE - 1));

    new TestCase(getMethod("CharMethod")).exec(
      NamedSet("IntCharSize", Locals::SetLocalVariableInt, Character.MAX_VALUE - 1));
    new TestCase(getMethod("ShortMethod")).exec(
      NamedSet("IntShortSize", Locals::SetLocalVariableInt, Short.MAX_VALUE - 1));
  }
}

