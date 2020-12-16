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

import java.util.concurrent.CountDownLatch;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

public class Test923 {
  public static void run() throws Exception {
    doTest();
  }

  private static void doTest() throws Exception {
    // Start a watchdog, to make sure on deadlocks etc the test dies.
    startWatchdog();

    sharedId = createRawMonitor();

    output = new ArrayList<String>(100);

    simpleTests(sharedId);

    for (String s : output) {
      System.out.println(s);
    }
    output.clear();

    threadTests(sharedId);

    destroyRawMonitor(sharedId);
  }

  private static void simpleTests(long id) {
    unlock(id);  // Should fail.

    lock(id);
    unlock(id);
    unlock(id);  // Should fail.

    lock(id);
    lock(id);
    unlock(id);
    unlock(id);
    unlock(id);  // Should fail.

    rawWait(id, 0);   // Should fail.
    rawWait(id, -1);  // Should fail.
    rawWait(id, 1);   // Should fail.

    lock(id);
    rawWait(id, 50);
    unlock(id);
    unlock(id);  // Should fail.

    rawNotify(id);  // Should fail.
    lock(id);
    rawNotify(id);
    unlock(id);
    unlock(id);  // Should fail.

    rawNotifyAll(id);  // Should fail.
    lock(id);
    rawNotifyAll(id);
    unlock(id);
    unlock(id);  // Should fail.
  }

  private static void threadTests(final long id) throws Exception {
    final int N = 10;

    final CountDownLatch waitLatch = new CountDownLatch(N);
    final CountDownLatch wait2Latch = new CountDownLatch(1);

    Runnable r = new Runnable() {
      @Override
      public void run() {
        lock(id);
        waitLatch.countDown();
        rawWait(id, 0);
        firstAwakened = Thread.currentThread();
        appendToLog("Awakened");
        unlock(id);
        wait2Latch.countDown();
      }
    };

    List<Thread> threads = new ArrayList<Thread>();
    for (int i = 0; i < N; i++) {
      Thread t = new Thread(r);
      threads.add(t);
      t.start();
    }

    // Wait till all threads have been started.
    waitLatch.await();

    // Hopefully enough time for all the threads to progress into wait.
    Thread.yield();
    Thread.sleep(500);

    // Wake up one.
    lock(id);
    rawNotify(id);
    unlock(id);

    wait2Latch.await();

    // Wait a little bit more to see stragglers. This is flaky - spurious wakeups could
    // make the test fail.
    Thread.yield();
    Thread.sleep(500);
    if (firstAwakened != null) {
      firstAwakened.join();
    }

    // Wake up everyone else.
    lock(id);
    rawNotifyAll(id);
    unlock(id);

    // Wait for everyone to die.
    for (Thread t : threads) {
      t.join();
    }

    // Check threaded output.
    Iterator<String> it = output.iterator();
    // 1) Start with N locks and Waits.
    {
      int locks = 0;
      int waits = 0;
      for (int i = 0; i < 2*N; i++) {
        String s = it.next();
        if (s.equals("Lock")) {
          locks++;
        } else if (s.equals("Wait")) {
          if (locks <= waits) {
            System.out.println(output);
            throw new RuntimeException("Wait before Lock");
          }
          waits++;
        } else {
          System.out.println(output);
          throw new RuntimeException("Unexpected operation: " + s);
        }
      }
    }

    // 2) Expect Lock + Notify + Unlock.
    expect("Lock", it, output);
    expect("Notify", it, output);
    expect("Unlock", it, output);

    // 3) A single thread wakes up, runs, and dies.
    expect("Awakened", it, output);
    expect("Unlock", it, output);

    // 4) Expect Lock + NotifyAll + Unlock.
    expect("Lock", it, output);
    expect("NotifyAll", it, output);
    expect("Unlock", it, output);

    // 5) N-1 threads wake up, run, and die.
    {
      int expectedUnlocks = 0;
      int ops = 2 * (N-1);
      for (int i = 0; i < ops; i++) {
        String s = it.next();
        if (s.equals("Awakened")) {
          expectedUnlocks++;
        } else if (s.equals("Unlock")) {
          expectedUnlocks--;
          if (expectedUnlocks < 0) {
            System.out.println(output);
            throw new RuntimeException("Unexpected unlock");
          }
        }
      }
    }

    // 6) That should be it.
    if (it.hasNext()) {
      System.out.println(output);
      throw new RuntimeException("Unexpected trailing output, starting with " + it.next());
    }

    output.clear();
    System.out.println("Done");
  }

  private static void expect(String s, Iterator<String> it, List<String> output) {
    String t = it.next();
    if (!s.equals(t)) {
      System.out.println(output);
      throw new RuntimeException("Expected " + s + " but got " + t);
    }
  }

  private static void lock(long id) {
    appendToLog("Lock");
    rawMonitorEnter(id);
  }

  private static void unlock(long id) {
    appendToLog("Unlock");
    try {
      rawMonitorExit(id);
    } catch (RuntimeException e) {
      appendToLog(e.getMessage());
    }
  }

  private static void rawWait(long id, long millis) {
    appendToLog("Wait");
    try {
      rawMonitorWait(id, millis);
    } catch (RuntimeException e) {
      appendToLog(e.getMessage());
    }
  }

  private static void rawNotify(long id) {
    appendToLog("Notify");
    try {
      rawMonitorNotify(id);
    } catch (RuntimeException e) {
      appendToLog(e.getMessage());
    }
  }

  private static void rawNotifyAll(long id) {
    appendToLog("NotifyAll");
    try {
      rawMonitorNotifyAll(id);
    } catch (RuntimeException e) {
      appendToLog(e.getMessage());
    }
  }

  private static synchronized void appendToLog(String s) {
    output.add(s);
  }

  private static void startWatchdog() {
    Runnable r = new Runnable() {
      @Override
      public void run() {
        long start = System.currentTimeMillis();
        // Give it a minute.
        long end = 60 * 1000 + start;
        for (;;) {
          long delta = end - System.currentTimeMillis();
          if (delta <= 0) {
            break;
          }

          try {
            Thread.currentThread().sleep(delta);
          } catch (Exception e) {
          }
        }
        System.out.println("TIMEOUT!");
        System.exit(1);
      }
    };
    Thread t = new Thread(r);
    t.setDaemon(true);
    t.start();
  }

  static volatile long sharedId;
  static List<String> output;
  static Thread firstAwakened;

  private static native long createRawMonitor();
  private static native void destroyRawMonitor(long id);
  private static native void rawMonitorEnter(long id);
  private static native void rawMonitorExit(long id);
  private static native void rawMonitorWait(long id, long millis);
  private static native void rawMonitorNotify(long id);
  private static native void rawMonitorNotifyAll(long id);
}
