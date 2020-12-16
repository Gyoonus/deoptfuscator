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

public class Test1933 {
  public static void run() throws Exception {
    System.out.println("No contention");
    testNoContention(new Monitors.NamedLock("test testNoContention"));

    System.out.println("Normal contended monitor");
    testNormalContendedMonitor(new Monitors.NamedLock("test testNormalContendedMonitor"));

    System.out.println("Waiting on a monitor");
    testNormalWaitMonitor(new Monitors.NamedLock("test testNormalWaitMonitor"));
  }

  public static void testNormalWaitMonitor(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
     controller1.DoLock();
     controller1.waitForLockToBeHeld();
     controller1.DoWait();
     controller1.waitForNotifySleep();
     // Spurious wakeups can hurt us here. Just retry until we get the result we expect. The test
     // will timeout eventually.
     Object mon = controller1.getWorkerContendedMonitor();
     for (; mon == null; mon = controller1.getWorkerContendedMonitor()) { Thread.yield(); }
     System.out.println("c1 is contending for monitor: " + mon);
     synchronized (lk) {
       lk.DoNotifyAll();
     }
     controller1.DoUnlock();
  }

  public static void testNormalContendedMonitor(final Monitors.NamedLock lk) throws Exception {
     final Monitors.LockController controller1 = new Monitors.LockController(lk);
     final Monitors.LockController controller2 = new Monitors.LockController(lk);
     controller1.DoLock();
     controller1.waitForLockToBeHeld();
     controller2.DoLock();
     controller2.waitForContendedSleep();
     System.out.println("c2 is contending for monitor: " + controller2.getWorkerContendedMonitor());
     controller1.DoUnlock();
     controller2.waitForLockToBeHeld();
     controller2.DoUnlock();
  }

  public static void testNoContention(final Monitors.NamedLock lk) throws Exception {
    synchronized (lk) {
      System.out.println("current thread is contending for monitor: " +
          Monitors.getCurrentContendedMonitor(null));
    }
  }
}
