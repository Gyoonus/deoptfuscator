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
import java.util.Arrays;
import java.util.Objects;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.*;
import java.util.ListIterator;
import java.util.function.Consumer;
import java.util.function.Function;

public class Test1931 {
  public static void printStackTrace(Throwable t) {
    System.out.println("Caught exception: " + t);
    for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
      System.out.println("\tCaused by: " +
          (Test1931.class.getPackage().equals(c.getClass().getPackage())
           ? c.toString() : c.getClass().toString()));
    }
  }

  public static void handleMonitorEnter(Thread thd, Object lock) {
    System.out.println(thd.getName() + " contended-LOCKING " + lock);
  }

  public static void handleMonitorEntered(Thread thd, Object lock) {
    System.out.println(thd.getName() + " LOCKED " + lock);
  }
  public static void handleMonitorWait(Thread thd, Object lock, long timeout) {
    System.out.println(thd.getName() + " start-monitor-wait " + lock + " timeout: " + timeout);
  }

  public static void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {
    System.out.println(thd.getName() + " monitor-waited " + lock + " timed_out: " + timed_out);
  }

  public static void run() throws Exception {
    Monitors.setupMonitorEvents(
        Test1931.class,
        Test1931.class.getDeclaredMethod("handleMonitorEnter", Thread.class, Object.class),
        Test1931.class.getDeclaredMethod("handleMonitorEntered", Thread.class, Object.class),
        Test1931.class.getDeclaredMethod("handleMonitorWait",
          Thread.class, Object.class, Long.TYPE),
        Test1931.class.getDeclaredMethod("handleMonitorWaited",
          Thread.class, Object.class, Boolean.TYPE),
        Monitors.NamedLock.class,
        null);

    System.out.println("Testing contended locking.");
    testLock(new Monitors.NamedLock("Lock testLock"));

    System.out.println("Testing monitor wait.");
    testWait(new Monitors.NamedLock("Lock testWait"));

    System.out.println("Testing monitor timed wait.");
    testTimedWait(new Monitors.NamedLock("Lock testTimedWait"));

    System.out.println("Testing monitor timed with timeout.");
    testTimedWaitTimeout(new Monitors.NamedLock("Lock testTimedWaitTimeout"));

    // TODO It would be good (but annoying) to do this with jasmin/smali in order to test if it's
    // different without the reflection.
    System.out.println("Waiting on an unlocked monitor.");
    testUnlockedWait(new Monitors.NamedLock("Lock testUnlockedWait"));

    System.out.println("Waiting with an illegal argument (negative timeout)");
    testIllegalWait(new Monitors.NamedLock("Lock testIllegalWait"));

    System.out.println("Interrupt a monitor being waited on.");
    testInteruptWait(new Monitors.NamedLock("Lock testInteruptWait"));
  }

  public static void testInteruptWait(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoWait();
    controller1.waitForNotifySleep();
    try {
      controller1.interruptWorker();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
    }
    controller1.DoCleanup();
  }

  public static void testIllegalWait(final Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk, /*timed_wait time*/-100);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller1.DoTimedWait();
      controller1.waitForNotifySleep();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
    }
    controller1.DoCleanup();
  }

  public static void testUnlockedWait(final Monitors.NamedLock lk) throws Exception {
    synchronized (lk) {
      Thread thd = new Thread(() -> {
        try {
          Method m = Object.class.getDeclaredMethod("wait");
          m.invoke(lk);
        } catch (Exception e) {
          printStackTrace(e);
        }
      }, "Unlocked wait thread:");
      thd.start();
      thd.join();
    }
  }

  public static void testLock(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller2.DoLock();
    if (controller2.IsLocked()) {
      throw new Exception("c2 was able to gain lock while it was held by c1");
    }
    controller2.waitForContendedSleep();
    controller1.DoUnlock();
    controller2.waitForLockToBeHeld();
    controller2.DoUnlock();
  }

  public static void testWait(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
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

  public static void testTimedWait(Monitors.NamedLock lk) throws Exception {
    // Time to wait (1 hour). We will wake it up before timeout.
    final long millis =  60l * 60l * 1000l;
    Monitors.LockController controller1 = new Monitors.LockController(lk, millis);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller1.DoTimedWait();
    controller1.waitForNotifySleep();
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    controller2.DoNotifyAll();
    controller2.DoUnlock();
    controller1.waitForLockToBeHeld();
    controller1.DoUnlock();
  }

  public static void testTimedWaitTimeout(Monitors.NamedLock lk) throws Exception {
    // Time to wait (10 seconds). We will wait for the timeout.
    final long millis =  10l * 1000l;
    Monitors.LockController controller1 = new Monitors.LockController(lk, millis);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    System.out.println("Waiting for 10 seconds.");
    controller1.DoTimedWait();
    controller1.waitForNotifySleep();
    controller1.DoUnlock();
    System.out.println("Wait finished with timeout.");
  }
}
