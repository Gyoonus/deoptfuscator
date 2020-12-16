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

import java.lang.reflect.Method;
import java.util.concurrent.atomic.*;
import java.util.function.Function;
import java.util.stream.Stream;
import java.util.Arrays;
import java.util.Objects;

public class Monitors {
  public native static void setupMonitorEvents(
      Class<?> method_klass,
      Method monitor_contended_enter_event,
      Method monitor_contended_entered_event,
      Method monitor_wait_event,
      Method monitor_waited_event,
      Class<?> lock_klass,
      Thread thr);
  public native static void stopMonitorEvents();

  public static class NamedLock {
    public final String name;
    private volatile int calledNotify;
    public NamedLock(String name) {
      this.name = name;
      calledNotify = 0;
    }

    public String toString() {
      return String.format("NamedLock[%s]", name);
    }

    public final void DoWait() throws Exception {
      final int v = calledNotify;
      while (v == calledNotify) {
        wait();
      }
    }

    public final void DoWait(long t) throws Exception {
      final int v = calledNotify;
      final long target = System.currentTimeMillis() + (t / 2);
      while (v == calledNotify && (t < 0 || System.currentTimeMillis() < target)) {
        wait(t);
      }
    }

    public final void DoNotifyAll() throws Exception {
      calledNotify++;
      notifyAll();
    }

    public final void DoNotify() throws Exception {
      calledNotify++;
      notify();
    }
  }

  public static final class MonitorUsage {
    public final Object monitor;
    public final Thread owner;
    public final int entryCount;
    public final Thread[] waiters;
    public final Thread[] notifyWaiters;

    public MonitorUsage(
        Object monitor,
        Thread owner,
        int entryCount,
        Thread[] waiters,
        Thread[] notifyWaiters) {
      this.monitor = monitor;
      this.entryCount = entryCount;
      this.owner = owner;
      this.waiters = waiters;
      this.notifyWaiters = notifyWaiters;
    }

    private static String toNameList(Thread[] ts) {
      return Arrays.toString(Arrays.stream(ts).map((Thread t) -> t.getName()).toArray());
    }

    public String toString() {
      return String.format(
          "MonitorUsage{ monitor: %s, owner: %s, entryCount: %d, waiters: %s, notify_waiters: %s }",
          monitor,
          (owner != null) ? owner.getName() : "<NULL>",
          entryCount,
          toNameList(waiters),
          toNameList(notifyWaiters));
    }
  }

  public static native MonitorUsage getObjectMonitorUsage(Object monitor);
  public static native Object getCurrentContendedMonitor(Thread thr);

  public static class TestException extends Error {
    public TestException() { super(); }
    public TestException(String s) { super(s); }
    public TestException(String s, Throwable c) { super(s, c); }
  }

  public static class LockController {
    private static enum Action { HOLD, RELEASE, NOTIFY, NOTIFY_ALL, WAIT, TIMED_WAIT }

    public final NamedLock lock;
    public final long timeout;
    private final AtomicStampedReference<Action> action;
    private volatile Thread runner = null;
    private volatile boolean started = false;
    private volatile boolean held = false;
    private static final AtomicInteger cnt = new AtomicInteger(0);
    private volatile Throwable exe;

    public LockController(NamedLock lock) {
      this(lock, 10 * 1000);
    }
    public LockController(NamedLock lock, long timeout) {
      this.lock = lock;
      this.timeout = timeout;
      this.action = new AtomicStampedReference(Action.HOLD, 0);
      this.exe = null;
    }

    public boolean IsWorkerThread(Thread thd) {
      return Objects.equals(runner, thd);
    }

    public boolean IsLocked() {
      checkException();
      return held;
    }

    public void checkException() {
      if (exe != null) {
        throw new TestException("Exception thrown by other thread!", exe);
      }
    }

    private void setAction(Action a) {
      int stamp = action.getStamp();
      // Wait for it to be HOLD before updating.
      while (!action.compareAndSet(Action.HOLD, a, stamp, stamp + 1)) {
        stamp = action.getStamp();
      }
    }

    public synchronized void suspendWorker() throws Exception {
      checkException();
      if (runner == null) {
        throw new TestException("We don't have any runner holding  " + lock);
      }
      Suspension.suspend(runner);
    }

    public Object getWorkerContendedMonitor() throws Exception {
      checkException();
      if (runner == null) {
        return null;
      }
      return getCurrentContendedMonitor(runner);
    }

    public synchronized void DoLock() {
      if (IsLocked()) {
        throw new Error("lock is already acquired or being acquired.");
      }
      if (runner != null) {
        throw new Error("Already have thread!");
      }
      runner = new Thread(() -> {
        started = true;
        try {
          synchronized (lock) {
            held = true;
            int[] stamp_h = new int[] { -1 };
            Action cur_action = Action.HOLD;
            try {
              while (true) {
                cur_action = action.get(stamp_h);
                int stamp = stamp_h[0];
                if (cur_action == Action.RELEASE) {
                  // The other thread will deal with reseting action.
                  break;
                }
                try {
                  switch (cur_action) {
                    case HOLD:
                      Thread.yield();
                      break;
                    case NOTIFY:
                      lock.DoNotify();
                      break;
                    case NOTIFY_ALL:
                      lock.DoNotifyAll();
                      break;
                    case TIMED_WAIT:
                      lock.DoWait(timeout);
                      break;
                    case WAIT:
                      lock.DoWait();
                      break;
                    default:
                      throw new Error("Unknown action " + action);
                  }
                } finally {
                  // reset action back to hold if it isn't something else.
                  action.compareAndSet(cur_action, Action.HOLD, stamp, stamp+1);
                }
              }
            } catch (Exception e) {
              throw new TestException("Got an error while performing action " + cur_action, e);
            }
          }
        } finally {
          held = false;
          started = false;
        }
      }, "Locker thread " + cnt.getAndIncrement() + " for " + lock);
      // Make sure we can get any exceptions this throws.
      runner.setUncaughtExceptionHandler((t, e) -> { exe = e; });
      runner.start();
    }

    public void waitForLockToBeHeld() throws Exception {
      while (true) {
        if (IsLocked() && Objects.equals(runner, Monitors.getObjectMonitorUsage(lock).owner)) {
          return;
        }
      }
    }

    public synchronized void waitForNotifySleep() throws Exception {
      if (runner == null) {
        throw new Error("No thread trying to lock!");
      }
      do {
        checkException();
      } while (!started ||
          !Arrays.asList(Monitors.getObjectMonitorUsage(lock).notifyWaiters).contains(runner));
    }

    public synchronized void waitForContendedSleep() throws Exception {
      if (runner == null) {
        throw new Error("No thread trying to lock!");
      }
      do {
        checkException();
      } while (!started ||
          runner.getState() != Thread.State.BLOCKED ||
          !Arrays.asList(Monitors.getObjectMonitorUsage(lock).waiters).contains(runner));
    }

    public synchronized void DoNotify() {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.NOTIFY);
    }

    public synchronized void DoNotifyAll() {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.NOTIFY_ALL);
    }

    public synchronized void DoTimedWait() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.TIMED_WAIT);
    }

    public synchronized void DoWait() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.WAIT);
    }

    public synchronized void interruptWorker() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      runner.interrupt();
    }

    public synchronized void waitForActionToFinish() throws Exception {
      checkException();
      while (action.getReference() != Action.HOLD) { checkException(); }
    }

    public synchronized void DoUnlock() throws Exception {
      Error throwing = null;
      if (!IsLocked()) {
        // We might just be racing some exception that was thrown by the worker thread. Cache the
        // exception, we will throw one from the worker before this one.
        throwing = new Error("Not locked!");
      }
      setAction(Action.RELEASE);
      Thread run = runner;
      runner = null;
      while (held) {}
      run.join();
      action.set(Action.HOLD, 0);
      // Make sure to throw any exception that occurred since it might not have unlocked due to our
      // request.
      checkException();
      DoCleanup();
      if (throwing != null) {
        throw throwing;
      }
    }

    public synchronized void DoCleanup() throws Exception {
      if (runner != null) {
        Thread run = runner;
        runner = null;
        while (held) {}
        run.join();
      }
      action.set(Action.HOLD, 0);
      exe = null;
    }
  }
}

