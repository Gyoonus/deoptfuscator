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
import java.util.Arrays;

public class Test1930 {
  public static final int NUM_RETRY = 100;
  private static void testSingleThread() {
    Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testSingleThread");
    executeLocked(() -> { printMonitorUsage(lk); }, lk);
  }
  private static void testSingleThreadNative() {
    Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testSingleThread");
    executeLockedNative(() -> { printMonitorUsage(lk); }, lk);
  }

  private static void testLockedTwice() {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testLockedTwice");
    executeLocked(() -> { executeLocked(() -> { printMonitorUsage(lk); }, lk); }, lk);
  }

  private static void testLockedTwiceNJ() {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testLockedTwiceNJ");
    executeLockedNative(() -> { executeLockedNative(() -> { printMonitorUsage(lk); }, lk); }, lk);
  }

  private static void testLockedTwiceJN() {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testLockedTwiceJN");
    executeLockedNative(() -> { executeLockedNative(() -> { printMonitorUsage(lk); }, lk); }, lk);
  }

  private static void testLockedTwiceNative() {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testLockedTwiceNative");
    executeLockedNative(() -> { executeLockedNative(() -> { printMonitorUsage(lk); }, lk); }, lk);
  }

  public final static class ThreadSignaler {
    public volatile boolean signal = false;
  }

  private static void testLockWait() throws Exception {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testLockWait");
    final Semaphore sem = new Semaphore(0);
    final Thread t = new Thread(() -> {
      sem.release();
      synchronized (lk) {
        printMonitorUsage(lk);
      }
    }, "Test1930 Thread - testLockWait");
    synchronized (lk) {
      t.start();
      // Wait for the other thread to actually start.
      sem.acquire();
      // Wait for the other thread to go to sleep trying to get the mutex. This might take a (short)
      // time since we try spinning first for better performance.
      boolean found_wait = false;
      for (long i = 0; i < NUM_RETRY; i++) {
        if (Arrays.asList(Monitors.getObjectMonitorUsage(lk).waiters).contains(t)) {
          found_wait = true;
          break;
        } else {
          Thread.sleep(500);
          Thread.yield();
        }
      }
      if (!found_wait) {
        System.out.println("other thread doesn't seem to be waiting.");
      }
      printMonitorUsage(lk);
    }
    t.join();
    printMonitorUsage(lk);
  }

  private static void testNotifyWait() throws Exception {
    final Monitors.NamedLock lk = new Monitors.NamedLock("Test1930 - testNotifyWait");
    final Semaphore sem = new Semaphore(0);
    Thread t = new Thread(() -> {
      synchronized (lk) {
        printMonitorUsage(lk);
        sem.release();
        try {
          lk.DoWait();
        } catch (Exception e) {
          throw new Error("Error waiting!", e);
        }
        printMonitorUsage(lk);
      }
    }, "Test1930 Thread - testLockWait");
    t.start();
    sem.acquire();
    synchronized (lk) {
      printMonitorUsage(lk);
      lk.DoNotifyAll();
    }
    t.join();
    printMonitorUsage(lk);
  }

  public static void run() throws Exception {
    // Single threaded tests.
    System.out.println("Running with single thread.");
    testSingleThread();
    System.out.println("Running with single thread in native.");
    testSingleThreadNative();
    System.out.println("Lock twice");
    testLockedTwice();
    System.out.println("Lock twice native");
    testLockedTwiceNative();
    System.out.println("Lock twice Java then native");
    testLockedTwiceJN();
    System.out.println("Lock twice native then Java");
    testLockedTwiceNJ();

    // Mutli threaded tests.
    System.out.println("lock with wait");
    testLockWait();
    System.out.println("Wait for notify.");
    testNotifyWait();
  }

  public static void printPreLock(Object lock) {
    System.out.println(String.format("Pre-lock[%s]: %s",
          Thread.currentThread().getName(), Monitors.getObjectMonitorUsage(lock)));
  }

  public static void executeLocked(Runnable r, Object lock) {
    printPreLock(lock);
    synchronized (lock) {
      r.run();
    }
  }

  public native static void executeLockedNative(Runnable r, Object m);
  public static void printMonitorUsage(Object m) {
    System.out.println(String.format("Thread[%s]: %s",
          Thread.currentThread().getName(), Monitors.getObjectMonitorUsage(m)));
  }
}
