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

import java.util.concurrent.Semaphore;
import java.util.Objects;

public class Test1934 {
  private final static boolean isDalvik = System.getProperty("java.vm.name").equals("Dalvik");

  public static final boolean PRINT_STACK_TRACE = false;

  public static void run() throws Exception {
    System.out.println("Interrupt before start");
    testInterruptBeforeStart();

    System.out.println("Stop before start");
    testStopBeforeStart();

    System.out.println("Interrupt recur");
    testInterruptRecur();

    System.out.println("Stop Recur");
    testStopRecur();

    System.out.println("Interrupt spinning");
    testInterruptSpinning();

    System.out.println("Stop spinning");
    testStopSpinning();

    System.out.println("Interrupt wait");
    testInterruptWait();

    System.out.println("Stop wait");
    testStopWait();

    System.out.println("Stop in native");
    testStopInNative();
  }

  public static void testStopBeforeStart() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Object tst = new Object();
    Thread target = new Thread(() -> { while (true) { } }, "waiting thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    System.out.println("stopping other thread before starting");
    try {
      Threads.stopThread(target, new Error("AWESOME"));
      target.start();
      target.join();
      System.out.println("Other thread Stopped by: " + out_err[0]);
      if (PRINT_STACK_TRACE && out_err[0] != null) {
        out_err[0].printStackTrace();
      }
    } catch (Exception e) {
      System.out.println("Caught exception " + e);
    }
  }

  public static void testInterruptBeforeStart() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Object tst = new Object();
    Thread target = new Thread(() -> { while (true) { } }, "waiting thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    System.out.println("interrupting other thread before starting");
    try {
      Threads.interruptThread(target);
      target.start();
      target.join();
      System.out.println("Other thread interrupted. err: " + out_err[0]);
      if (PRINT_STACK_TRACE && out_err[0] != null) {
        out_err[0].printStackTrace();
      }
    } catch (Exception e) {
      System.out.println("Caught exception " + e);
    }
  }

  public static void testStopWait() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Object tst = new Object();
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      while (true) {
        try {
          synchronized (tst) {
            tst.wait();
          }
        } catch (InterruptedException e) { throw new Error("Interrupted!", e); }
      }
    }, "waiting thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    while (!Objects.equals(Monitors.getCurrentContendedMonitor(target), tst)) {}
    System.out.println("stopping other thread waiting");
    Threads.stopThread(target, new Error("AWESOME"));
    target.join();
    System.out.println("Other thread Stopped by: " + out_err[0]);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
  }

  public static void testInterruptWait() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Object tst = new Object();
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      while (true) {
        try {
          synchronized (tst) {
            tst.wait();
          }
        } catch (InterruptedException e) { throw new Error("Interrupted!", e); }
      }
    }, "waiting thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    while (!Objects.equals(Monitors.getCurrentContendedMonitor(target), tst)) {}
    System.out.println("interrupting other thread waiting");
    Threads.interruptThread(target);
    target.join();
    System.out.println("Other thread interrupted. err: " + out_err[0]);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
  }

  public static void doNothing() {}
  public static native long allocNativeMonitor();
  public static native void nativeWaitForOtherThread(long id);
  public static native void nativeDoInterleaved(long id, Runnable op);
  public static native void destroyNativeMonitor(long id);
  public static void testStopInNative() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final long native_monitor_id = allocNativeMonitor();
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      nativeWaitForOtherThread(native_monitor_id);
      // We need to make sure we do something that can get the exception to be actually noticed.
      doNothing();
    }, "native waiting thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    System.out.println("stopping other thread");
    nativeDoInterleaved(
        native_monitor_id,
        () -> { Threads.stopThread(target, new Error("AWESOME")); });
    target.join();

    String out_err_msg;
    if (isDalvik || out_err[0] != null) {
      out_err_msg = out_err[0].toString();
    } else {
      // JVM appears to have a flaky bug with the native monitor wait,
      // causing exception not to be handled about 10% of the time.
      out_err_msg = "java.lang.Error: AWESOME";
    }
    System.out.println("Other thread Stopped by: " + out_err_msg);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
    destroyNativeMonitor(native_monitor_id);
  }

  public static void doRecurCnt(Runnable r, int cnt) {
    if (r != null) {
      r.run();
    }
    if (cnt != 0) {
      doRecurCnt(r, cnt - 1);
    }
  }

  public static void testStopRecur() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      while (true) {
        doRecurCnt(null, 50);
      }
    }, "recuring thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    System.out.println("stopping other thread recurring");
    Threads.stopThread(target, new Error("AWESOME!"));
    target.join();
    System.out.println("Other thread Stopped by: " + out_err[0]);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
  }

  public static void testInterruptRecur() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      while (true) {
        doRecurCnt(() -> {
          if (Thread.currentThread().isInterrupted()) { throw new Error("Interrupted!"); }
        }, 50);
      }
    }, "recuring thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    System.out.println("Interrupting other thread recurring");
    Threads.interruptThread(target);
    target.join();
    System.out.println("Other thread Interrupted. err: " + out_err[0]);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
  }

  public static void testStopSpinning() throws Exception {
    final Throwable[] out_err = new Throwable[] { null, };
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> { sem.release(); while (true) {} }, "Spinning thread!");
    target.setUncaughtExceptionHandler((t, e) -> { out_err[0] = e; });
    target.start();
    sem.acquire();
    System.out.println("stopping other thread spinning");
    Threads.stopThread(target, new Error("AWESOME!"));
    target.join();
    System.out.println("Other thread Stopped by: " + out_err[0]);
    if (PRINT_STACK_TRACE && out_err[0] != null) {
      out_err[0].printStackTrace();
    }
  }

  public static void testInterruptSpinning() throws Exception {
    final Semaphore sem = new Semaphore(0);
    Thread target = new Thread(() -> {
      sem.release();
      while (!Thread.currentThread().isInterrupted()) { }
    }, "Spinning thread!");
    target.start();
    sem.acquire();
    System.out.println("Interrupting other thread spinning");
    Threads.interruptThread(target);
    target.join();
    System.out.println("Other thread Interrupted.");
  }
}
