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

public class Test1932 {
  public static final boolean PRINT_FULL_STACK_TRACE = false;
  public static final boolean INCLUDE_ANDROID_ONLY_TESTS = false;

  public static interface MonitorHandler {
    public default void handleMonitorEnter(Thread thd, Object lock) {}
    public default void handleMonitorEntered(Thread thd, Object lock) {}
    public default void handleMonitorWait(Thread thd, Object lock, long timeout) {}
    public default void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {}
  }

  public static volatile MonitorHandler HANDLER = null;

  public static void run() throws Exception {
    Monitors.setupMonitorEvents(
        Test1932.class,
        Test1932.class.getDeclaredMethod("handleMonitorEnter", Thread.class, Object.class),
        Test1932.class.getDeclaredMethod("handleMonitorEntered", Thread.class, Object.class),
        Test1932.class.getDeclaredMethod("handleMonitorWait",
          Thread.class, Object.class, Long.TYPE),
        Test1932.class.getDeclaredMethod("handleMonitorWaited",
          Thread.class, Object.class, Boolean.TYPE),
        Monitors.NamedLock.class,
        null);

    System.out.println("Testing contended locking where lock is released before callback ends.");
    testLockUncontend(new Monitors.NamedLock("Lock testLockUncontend"));

    System.out.println("Testing throwing exceptions in monitor_enter");
    testLockThrowEnter(new Monitors.NamedLock("Lock testLockThrowEnter"));

    System.out.println("Testing throwing exceptions in monitor_entered");
    testLockThrowEntered(new Monitors.NamedLock("Lock testLockThrowEntered"));

    System.out.println("Testing throwing exceptions in both monitorEnter & MonitorEntered");
    testLockThrowBoth(new Monitors.NamedLock("Lock testLockThrowBoth"));

    // This exposes a difference between the RI and ART. On the RI this test will cause a
    // JVMTI_EVENT_MONITOR_WAITED event to be sent even though we threw an exception during the
    // JVMTI_EVENT_MONITOR_WAIT. See b/65558434.
    System.out.println("Testing throwing exception in MonitorWait event");
    testThrowWait(new Monitors.NamedLock("Lock testThrowWait"));

    System.out.println("Testing throwing exception in MonitorWait event with illegal aruments");
    testThrowIllegalWait(new Monitors.NamedLock("Lock testThrowIllegalWait"));

    System.out.println("Testing throwing exception in MonitorWaited event");
    testThrowWaited(new Monitors.NamedLock("Lock testThrowWaited"));

    System.out.println("Testing throwing exception in MonitorWaited event caused by timeout");
    testThrowWaitedTimeout(new Monitors.NamedLock("Lock testThrowWaitedTimeout"));

    System.out.println("Testing throwing exception in MonitorWaited event caused by interrupt");
    testThrowWaitedInterrupt(new Monitors.NamedLock("Lock testThrowWaitedInterrupt"));

    System.out.println("Testing ObjectMonitorInfo inside of events");
    testMonitorInfoInEvents(new Monitors.NamedLock("Lock testMonitorInfoInEvents"));

    System.out.println("Testing that the monitor can be stolen during the MonitorWaited event.");
    testWaitEnterInterleaving(new Monitors.NamedLock("test testWaitEnterInterleaving"));

    // TODO We keep this here since it works on android but it's not clear it's behavior we want to
    // support long term or at all.
    if (INCLUDE_ANDROID_ONLY_TESTS) {
      System.out.println(
          "Testing that the monitor can be still held by notifier during the MonitorWaited " +
          "event. NB This doesn't work on the RI.");
      testWaitExitInterleaving(new Monitors.NamedLock("test testWaitExitInterleaving"));
    }

    System.out.println(
        "Testing that we can lock and release the monitor in the MonitorWait event");
    testWaitMonitorEnter(new Monitors.NamedLock("test testWaitMonitorEnter"));

    System.out.println(
        "Testing that we can lock and release the monitor in the MonitorWaited event");
    testWaitedMonitorEnter(new Monitors.NamedLock("test testWaitedMonitorEnter"));

    System.out.println("Testing we can perform recursive lock in MonitorEntered");
    testRecursiveMontiorEnteredLock(new Monitors.NamedLock("test testRecursiveMontiorEnteredLock"));

    System.out.println("Testing the lock state if MonitorEnter throws in a native method");
    testNativeLockStateThrowEnter(new Monitors.NamedLock("test testNativeLockStateThrowEnter"));

    System.out.println("Testing the lock state if MonitorEntered throws in a native method");
    testNativeLockStateThrowEntered(new Monitors.NamedLock("test testNativeLockStateThrowEntered"));
  }

  public static native void doNativeLockPrint(Monitors.NamedLock lk);
  public static void printLockState(Monitors.NamedLock lk, Object exception, int res) {
    System.out.println(
        "MonitorEnter returned: " + res + "\n" +
        "Lock state is: " + Monitors.getObjectMonitorUsage(lk));
    printExceptions((Throwable)exception);
  }

  public static void testNativeLockStateThrowEnter(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Unlocking controller1 in MonitorEnter");
        try {
          controller1.DoUnlock();
        } catch (Exception e) {
          throw new Monitors.TestException("Exception unlocking monitor in MonitorEnter " + l, e);
        }
        System.out.println("Throwing exception in MonitorEnter");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    Thread native_thd = new Thread(() -> {
      try {
        doNativeLockPrint(lk);
      } catch (Throwable e) {
        System.out.println("Unhandled exception: " + e);
        e.printStackTrace();
      }
    }, "NativeLockStateThrowEnter thread");
    native_thd.start();
    native_thd.join();
  }

  public static void testNativeLockStateThrowEntered(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Unlocking controller1 in MonitorEnter");
        try {
          controller1.DoUnlock();
        } catch (Exception e) {
          throw new Monitors.TestException("Exception unlocking monitor in MonitorEnter " + l, e);
        }
      }
      @Override public void handleMonitorEntered(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEntered");
        throw new Monitors.TestException("throwing exception during monitorEntered of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    Thread native_thd = new Thread(() -> {
      try {
        doNativeLockPrint(lk);
      } catch (Throwable e) {
        System.out.println("Unhandled exception: " + e);
        e.printStackTrace();
      }
    }, "NativeLockStateThrowEntered thread");
    native_thd.start();
    native_thd.join();
  }

  public static void testRecursiveMontiorEnteredLock(final Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEntered(Thread thd, Object l) {
        try {
          System.out.println("In MonitorEntered usage: " + Monitors.getObjectMonitorUsage(lk));
          synchronized (lk) {
            System.out.println("In MonitorEntered sync: " + Monitors.getObjectMonitorUsage(lk));
          }
        } catch (Exception e) {
          throw new Monitors.TestException("error while recursive locking!", e);
        }
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller2.DoLock();
    controller2.waitForContendedSleep();
    controller1.DoUnlock();
    controller2.waitForLockToBeHeld();
    controller2.DoUnlock();
  }

  public static void testWaitedMonitorEnter(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        try {
          // make sure that controller2 has acutally unlocked everything, we can be sent earlier
          // than that on ART.
          while (controller2.IsLocked()) {}
          System.out.println("In waited monitor usage: " + Monitors.getObjectMonitorUsage(lk));
          synchronized (lk) {
            System.out.println(
                "In waited monitor usage sync: " + Monitors.getObjectMonitorUsage(lk));
          }
        } catch (Exception e) {
          throw new Monitors.TestException("error while doing unlock in other thread!", e);
        }
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoWait();
    controller1.waitForNotifySleep();
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoNotifyAll();
    controller2.DoUnlock();
    controller1.waitForLockToBeHeld();
    controller1.DoUnlock();
  }

  public static void testWaitMonitorEnter(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWait(Thread thd, Object l, long timeout) {
        try {
          System.out.println("In wait monitor usage: " + Monitors.getObjectMonitorUsage(lk));
          synchronized (lk) {
            System.out.println("In wait monitor usage sync: " + Monitors.getObjectMonitorUsage(lk));
          }
        } catch (Exception e) {
          throw new Monitors.TestException("error while doing unlock in other thread!", e);
        }
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoWait();
    controller1.waitForNotifySleep();
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoNotifyAll();
    controller2.DoUnlock();
    controller1.waitForLockToBeHeld();
    controller1.DoUnlock();
  }

  // NB This test cannot be run on the RI. It deadlocks. Leaving for documentation.
  public static void testWaitExitInterleaving(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        System.out.println("un-locking controller1 in controller2 MonitorWaited event");
        try {
          controller1.DoUnlock();
        } catch (Exception e) {
          throw new Monitors.TestException("error while doing unlock in other thread!", e);
        }
      }
    };
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoWait();
    controller2.waitForNotifySleep();
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoNotifyAll();
    controller2.waitForLockToBeHeld();
    controller2.DoUnlock();
  }

  public static void testWaitEnterInterleaving(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    final Monitors.LockController controller3 = new Monitors.LockController(lk);
    final Semaphore unlocked_sem = new Semaphore(0);
    final Semaphore continue_sem = new Semaphore(0);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        System.out.println("locking controller3 in controller2 MonitorWaited event");
        try {
          unlocked_sem.acquire();
          controller3.DoLock();
          controller3.waitForLockToBeHeld();
          System.out.println(
              "Controller3 now holds the lock the monitor wait will try to re-acquire");
          continue_sem.release();
        } catch (Exception e) {
          throw new Monitors.TestException("error while doing unlock in other thread!", e);
        }
      }
    };
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoWait();
    controller2.waitForNotifySleep();
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoNotifyAll();
    controller1.DoUnlock();
    // Wait for controller3 to have locked.
    // We cannot use waitForLockToBeHeld since we could race with the HANDLER waitForLockToBeHeld
    // function.
    unlocked_sem.release();
    continue_sem.acquire();
    controller3.DoUnlock();
    controller2.waitForLockToBeHeld();
    controller2.DoUnlock();
  }

  public static void testMonitorInfoInEvents(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread thd, Object l) {
        System.out.println("Monitor usage in MonitorEnter: " + Monitors.getObjectMonitorUsage(l));
      }
      @Override public void handleMonitorEntered(Thread thd, Object l) {
        System.out.println("Monitor usage in MonitorEntered: " + Monitors.getObjectMonitorUsage(l));
      }
      @Override public void handleMonitorWait(Thread thd, Object l, long timeout) {
        System.out.println("Monitor usage in MonitorWait: " + Monitors.getObjectMonitorUsage(l));
      }
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        // make sure that controller1 has acutally unlocked everything, we can be sent earlier than
        // that on ART.
        while (controller1.IsLocked()) {}
        System.out.println("Monitor usage in MonitorWaited: " + Monitors.getObjectMonitorUsage(l));
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller2.DoLock();
    controller2.waitForContendedSleep();
    controller1.DoUnlock();
    controller2.waitForLockToBeHeld();
    controller2.DoWait();
    controller2.waitForNotifySleep();
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoNotifyAll();
    controller1.DoUnlock();
    controller2.waitForLockToBeHeld();
    controller2.DoUnlock();
  }

  public static void testThrowWaitedInterrupt(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        System.out.println("Throwing exception in MonitorWaited");
        throw new Monitors.TestException("throwing exception during monitorWaited of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller1.DoWait();
      controller1.waitForNotifySleep();
      controller1.interruptWorker();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller1.DoCleanup();
    }
  }

  public static void testThrowWaitedTimeout(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk, 5 * 1000);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        System.out.println("Throwing exception in MonitorWaited");
        throw new Monitors.TestException("throwing exception during monitorWaited of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller1.DoTimedWait();
      controller1.waitForNotifySleep();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller1.DoCleanup();
    }
  }

  public static void testThrowWaited(Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWaited(Thread thd, Object l, boolean timeout) {
        System.out.println("Throwing exception in MonitorWaited");
        throw new Monitors.TestException("throwing exception during monitorWaited of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoWait();
    controller1.waitForNotifySleep();

    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoNotifyAll();
    controller2.DoUnlock();
    try {
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller1.DoCleanup();
    }
  }

  public static void testThrowWait(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWait(Thread thd, Object l, long timeout) {
        System.out.println("Throwing exception in MonitorWait");
        throw new Monitors.TestException("throwing exception during MonitorWait of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller1.DoWait();
      controller1.waitForNotifySleep();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller1.DoCleanup();
    }
  }

  public static void testThrowIllegalWait(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk, -100000);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorWait(Thread thd, Object l, long timeout) {
        System.out.println("Throwing exception in MonitorWait timeout = " + timeout);
        throw new Monitors.TestException("throwing exception during monitorWait of " + l);
      }
    };
    try {
      controller1.DoLock();
      controller1.waitForLockToBeHeld();
      controller1.DoTimedWait();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller1.DoCleanup();
    }
  }

  public static void testLockUncontend(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread thd, Object lock) {
        if (controller1.IsLocked()) {
          System.out.println("Releasing " + lk + " during monitorEnter event.");
          try {
            controller1.DoUnlock();
          } catch (Exception e) {
            throw new Error("Unable to unlock controller1", e);
          }
        } else {
          throw new Error("controller1 does not seem to hold the lock!");
        }
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    // This will call handleMonitorEnter but will release during the callback.
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    if (controller1.IsLocked()) {
      throw new Error("controller1 still holds the lock somehow!");
    }
    controller2.DoUnlock();
  }

  public static void testLockThrowEnter(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEnter");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller2.DoCleanup();
    }
  }

  public static void testLockThrowEntered(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEntered(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEntered");
        throw new Monitors.TestException("throwing exception during monitorEntered of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller2.DoCleanup();
    }
  }

  public static void testLockThrowBoth(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      @Override public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEnter");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
      @Override public void handleMonitorEntered(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEntered");
        throw new Monitors.TestException("throwing exception during monitorEntered of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printExceptions(e);
      System.out.println("lock state is: " + Monitors.getObjectMonitorUsage(lk));
      controller2.DoCleanup();
    }
  }

  public static void printExceptions(Throwable t) {
    System.out.println("Caught exception: " + t);
    for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
      System.out.println("\tCaused by: " +
          (Test1932.class.getPackage().equals(c.getClass().getPackage())
           ? c.toString() : c.getClass().toString()));
    }
    if (PRINT_FULL_STACK_TRACE) {
      t.printStackTrace();
    }
  }

  public static void handleMonitorEnter(Thread thd, Object lock) {
    System.out.println(thd.getName() + " contended-LOCKING " + lock);
    if (HANDLER != null) {
      HANDLER.handleMonitorEnter(thd, lock);
    }
  }

  public static void handleMonitorEntered(Thread thd, Object lock) {
    System.out.println(thd.getName() + " LOCKED " + lock);
    if (HANDLER != null) {
      HANDLER.handleMonitorEntered(thd, lock);
    }
  }
  public static void handleMonitorWait(Thread thd, Object lock, long timeout) {
    System.out.println(thd.getName() + " start-monitor-wait " + lock + " timeout: " + timeout);
    if (HANDLER != null) {
      HANDLER.handleMonitorWait(thd, lock, timeout);
    }
  }

  public static void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {
    System.out.println(thd.getName() + " monitor-waited " + lock + " timed_out: " + timed_out);
    if (HANDLER != null) {
      HANDLER.handleMonitorWaited(thd, lock, timed_out);
    }
  }
}
