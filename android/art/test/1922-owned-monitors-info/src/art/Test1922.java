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

import java.util.Arrays;
import java.util.List;
import java.util.ListIterator;
import java.util.function.Consumer;
import java.util.function.Function;

public class Test1922 {
  // Set to true to run with all combinations of locks. This isn't really needed for the test to be
  // useful and fully representative.
  public final static boolean ALL_COMBOS = false;

  // A runnable that lets us know when a different thread is paused.
  public static class ThreadPauser implements Runnable {
    private boolean suspend;
    private volatile Thread paused_thread;
    public ThreadPauser(boolean suspend) {
      paused_thread = null;
      this.suspend = suspend;
    }
    public ThreadPauser() {
      paused_thread = null;
      this.suspend = false;
    }

    @Override
    public void run() {
      this.paused_thread = Thread.currentThread();
      if (suspend) {
        Suspension.suspend(paused_thread);
      }
      while (this.paused_thread != null) {}
    }

    public void waitForOtherThreadToPause() {
      while (this.paused_thread == null) {}
      if (suspend) {
        while (!Suspension.isSuspended(this.paused_thread)) {}
      }
    }

    public void wakeupOtherThread() {
      if (this.paused_thread == null) {
        throw new Error("Other thread is not paused!");
      }
      if (suspend) {
        Suspension.resume(this.paused_thread);
        while (Suspension.isSuspended(this.paused_thread)) {}
      }
      this.paused_thread = null;
    }
  }

  // A class with a number of monitor operations in its methods.
  public static class Target {
    public String name;
    public Target(String name) { this.name = name; }
    public String toString() { return "Target(\"" + name + "\")"; }

    // synchronize on Target.class
    public void lockClass(Runnable safepoint) {
      synchronized (this.getClass()) {
        safepoint.run();
      }
    }

    // use java synchronized method.
    public synchronized void lockSync(Runnable safepoint) {
      safepoint.run();
    }

    // use java synchronized method and synchronize on another object.
    public synchronized void lockExtra(Object l, Runnable safepoint) {
      synchronized (l) {
        safepoint.run();
      }
    }

    // monitor enter the object 'l' in native code.
    public native void lockNative(Object l, Runnable safepoint);

    // monitor enter 'this' in native code.
    public native void lockThisNative(Runnable safepoint);

    // synchronize on 'l'
    public void lockOther(Object l, Runnable safepoint) {
      synchronized (l) {
        safepoint.run();
      }
    }

    // Don't do anything. Just call the next method.
    public void callSafepoint(Runnable safepoint) {
      safepoint.run();
    }
  }

  // A lock with a toString.
  public static class NamedLock {
    public String name;
    public NamedLock(String name) { this.name = name; }
    public String toString() { return "NamedLock(\"" + name + "\")"; }
  }

  private static Object[] sortByString(Object[] arr) {
    Arrays.sort(arr, (a, b) -> a.toString().compareTo(b.toString()));
    return arr;
  }

  public static class PrintOwnedMonitorsStackDepthRunnable implements Runnable {
    public final Thread target;
    public PrintOwnedMonitorsStackDepthRunnable(Thread target) {
      this.target = target;
    }
    public void run() {
      System.out.println("Owned monitors: " +
          Arrays.toString(sortByString(getOwnedMonitorStackDepthInfo(target))));
    }
  }

  public static class PrintOwnedMonitorsRunnable implements Runnable {
    public final Thread target;
    public PrintOwnedMonitorsRunnable(Thread target) {
      this.target = target;
    }
    public void run() {
      System.out.println("Owned monitors: " +
          Arrays.toString(sortByString(getOwnedMonitors(target))));
    }
  }

  public static void run() throws Exception {
    setupTest();

    System.out.println("owner-monitors, This thread");
    runTestsCurrentThread("owned-monitor",
        new PrintOwnedMonitorsRunnable(Thread.currentThread()));

    System.out.println("owner-monitors, no suspend, Other thread");
    runTestsOtherThread("owned-monitor", false,
        (t) -> { new PrintOwnedMonitorsRunnable(t).run(); });

    System.out.println("owner-monitors, suspend, Other thread");
    runTestsOtherThread("owned-monitor", true,
        (t) -> { new PrintOwnedMonitorsRunnable(t).run(); });

    System.out.println("owner-monitors-stack-depth, This thread");
    runTestsCurrentThread("owned-stack-depth",
        new PrintOwnedMonitorsStackDepthRunnable(Thread.currentThread()));

    System.out.println("owner-monitors-stack-depth, no suspend, other thread");
    runTestsOtherThread("owned-stack-depth", false,
        (t) -> { new PrintOwnedMonitorsStackDepthRunnable(t).run(); });

    System.out.println("owner-monitors-stack-depth, suspend, other thread");
    runTestsOtherThread("owned-stack-depth", true,
        (t) -> { new PrintOwnedMonitorsStackDepthRunnable(t).run(); });
  }

  public static void runTestsOtherThread(String name, boolean suspend, Consumer<Thread> printer) {
    final Target t = new Target("Other thread test (suspend: " + suspend + "): " + name);
    final NamedLock l1 = new NamedLock("Lock 1");
    final NamedLock l2 = new NamedLock("Lock 2");
    final NamedLock l3 = new NamedLock("Lock 3");

    List<Function<Runnable, Runnable>> MkSafepoints = Arrays.asList(
      (r) -> { return new CallLockOther(t, l1, r); },
      (r) -> { return new CallLockExtra(t, l2, r); },
      (r) -> { return new CallLockNative(t, l3, r); },
      (r) -> { return new CallLockThisNative(t, r); },
      (r) -> { return new CallLockClass(t, r); },
      (r) -> { return new CallLockSync(t, r); },
      (r) -> { return new CallSafepoint(t, r); }
    );
    // Use ListIterators so we can have elements in the test multiple times.
    ListIterator<Function<Runnable, Runnable>> li1 = MkSafepoints.listIterator();
    for (Function<Runnable, Runnable> r1 = li1.next(); li1.hasNext(); r1 = li1.next()) {
      ListIterator<Function<Runnable, Runnable>> li2 =
          MkSafepoints.listIterator(ALL_COMBOS ? 0 : li1.previousIndex());
      for (Function<Runnable, Runnable> r2 = li2.next(); li2.hasNext(); r2 = li2.next()) {
        ListIterator<Function<Runnable, Runnable>> li3 =
            MkSafepoints.listIterator(ALL_COMBOS ? 0 : li2.previousIndex());
        for (Function<Runnable, Runnable> r3 = li3.next(); li3.hasNext(); r3 = li3.next()) {
          System.out.println("Running: " + Arrays.toString(
              new Object[] {
                r1.apply(null).getClass(),
                r2.apply(null).getClass(),
                r3.apply(null).getClass(),
              }));
          try {
            final ThreadPauser pause = new ThreadPauser(suspend);
            final Thread thr = new Thread(r1.apply(r2.apply(r3.apply(pause))));
            thr.start();
            pause.waitForOtherThreadToPause();
            printer.accept(thr);
            pause.wakeupOtherThread();
            thr.join();
          } catch (Exception e) {
            throw new Error("Exception in test." , e);
          }
        }
      }
    }
  }
  public static void runTestsCurrentThread(String name, Runnable printer) {
    final Target t = new Target("Current thread test: " + name);
    final NamedLock l1 = new NamedLock("Lock 1");
    final NamedLock l2 = new NamedLock("Lock 2");
    final NamedLock l3 = new NamedLock("Lock 3");

    List<Function<Runnable, Runnable>> MkSafepoints = Arrays.asList(
      (r) -> { return new CallLockOther(t, l1, r); },
      (r) -> { return new CallLockExtra(t, l2, r); },
      (r) -> { return new CallLockNative(t, l3, r); },
      (r) -> { return new CallLockThisNative(t, r); },
      (r) -> { return new CallLockClass(t, r); },
      (r) -> { return new CallLockSync(t, r); },
      (r) -> { return new CallSafepoint(t, r); }
    );
    ListIterator<Function<Runnable, Runnable>> li1 = MkSafepoints.listIterator();
    for (Function<Runnable, Runnable> r1 = li1.next(); li1.hasNext(); r1 = li1.next()) {
      ListIterator<Function<Runnable, Runnable>> li2 =
          MkSafepoints.listIterator(ALL_COMBOS ? 0 : li1.previousIndex());
      for (Function<Runnable, Runnable> r2 = li2.next(); li2.hasNext(); r2 = li2.next()) {
        ListIterator<Function<Runnable, Runnable>> li3 =
            MkSafepoints.listIterator(ALL_COMBOS ? 0 : li2.previousIndex());
        for (Function<Runnable, Runnable> r3 = li3.next(); li3.hasNext(); r3 = li3.next()) {
          System.out.println("Running: " + Arrays.toString(
              new Object[] {
                r1.apply(null).getClass(),
                r2.apply(null).getClass(),
                r3.apply(null).getClass(),
              }));
          r1.apply(r2.apply(r3.apply(printer))).run();
        }
      }
    }
  }

  public static native void setupTest();
  public static native Object[] getOwnedMonitors(Thread thr);
  public static native MonitorStackDepthInfo[] getOwnedMonitorStackDepthInfo(Thread thr);
  public static class MonitorStackDepthInfo {
    public final int depth;
    public final Object monitor;
    public MonitorStackDepthInfo(int depth, Object monitor) {
      this.depth = depth;
      this.monitor = monitor;
    }
    public String toString() {
      return "{ depth: " + depth + ", monitor: \"" + monitor.toString() + "\" }";
    }
  }

  // We want to avoid synthetic methods that would mess up our stack-depths so we make everything
  // explicit here.
  public static class CallSafepoint implements Runnable {
    public final Target target;
    public final Runnable safepoint;
    public CallSafepoint(Target target, Runnable safepoint) {
      this.target = target;
      this.safepoint = safepoint;
    }
    public void run() {
      target.callSafepoint(safepoint);
    }
  }
  public static class CallLockOther implements Runnable {
    public final Target target;
    public final Object l;
    public final Runnable safepoint;
    public CallLockOther(Target target, Object l, Runnable safepoint) {
      this.target = target;
      this.l = l;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockOther(l, safepoint);
    }
  }
  public static class CallLockExtra implements Runnable {
    public final Target target;
    public final Object l;
    public final Runnable safepoint;
    public CallLockExtra(Target target, Object l, Runnable safepoint) {
      this.target = target;
      this.l = l;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockExtra(l, safepoint);
    }
  }
  public static class CallLockThisNative implements Runnable {
    public final Target target;
    public final Runnable safepoint;
    public CallLockThisNative(Target target, Runnable safepoint) {
      this.target = target;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockThisNative(safepoint);
    }
  }
  public static class CallLockNative implements Runnable {
    public final Target target;
    public final Object l;
    public final Runnable safepoint;
    public CallLockNative(Target target, Object l, Runnable safepoint) {
      this.target = target;
      this.l = l;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockNative(l, safepoint);
    }
  }
  public static class CallLockClass implements Runnable {
    public final Target target;
    public final Runnable safepoint;
    public CallLockClass(Target target, Runnable safepoint) {
      this.target = target;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockClass(safepoint);
    }
  }
  public static class CallLockSync implements Runnable {
    public final Target target;
    public final Runnable safepoint;
    public CallLockSync(Target target, Runnable safepoint) {
      this.target = target;
      this.safepoint = safepoint;
    }
    public void run() {
      target.lockSync(safepoint);
    }
  }
}
