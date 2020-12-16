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
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.concurrent.CountDownLatch;
import java.util.function.Function;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class Test924 {
  public static void run() throws Exception {
    // Run the test on its own thread, so we have a known state for the "current" thread.
    Thread t = new Thread("TestThread") {
      @Override
      public void run() {
        try {
          doTest();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };
    t.start();
    t.join();
  }

  private static void doTest() throws Exception {
    Thread t1 = Thread.currentThread();
    Thread t2 = getCurrentThread();

    // Need to adjust priority, as on-device this may be unexpected (and we prefer not
    // to special-case this.)
    t1.setPriority(5);

    if (t1 != t2) {
      throw new RuntimeException("Expected " + t1 + " but got " + t2);
    }
    System.out.println("currentThread OK");

    printThreadInfo(t1);
    printThreadInfo(null);

    Thread t3 = new Thread("Daemon Thread");
    t3.setDaemon(true);
    // Do not start this thread, yet.
    printThreadInfo(t3);
    // Start, and wait for it to die.
    t3.start();
    t3.join();
    Thread.sleep(500);  // Wait a little bit.
    // Thread has died, check that we can still get info.
    printThreadInfo(t3);

    // Try a subclass of thread.
    Thread t4 = new Thread("Subclass") {
    };
    printThreadInfo(t4);

    doCurrentThreadStateTests();
    doStateTests(Thread::new);
    doStateTests(ExtThread::new);

    doAllThreadsTests();

    doTLSTests();

    doTestEvents();
  }

  private static final class ExtThread extends Thread {
    public ExtThread(Runnable r) { super(r); }
  }

  private static class Holder {
    volatile boolean flag = false;
  }

  private static void doCurrentThreadStateTests() throws Exception {
    System.out.println(Integer.toHexString(getThreadState(null)));
    System.out.println(Integer.toHexString(getThreadState(Thread.currentThread())));
  }

  private static void doStateTests(Function<Runnable, Thread> mkThread) throws Exception {
    final CountDownLatch cdl1 = new CountDownLatch(1);
    final CountDownLatch cdl2 = new CountDownLatch(1);
    final CountDownLatch cdl3_1 = new CountDownLatch(1);
    final CountDownLatch cdl3_2 = new CountDownLatch(1);
    final CountDownLatch cdl4 = new CountDownLatch(1);
    final CountDownLatch cdl5 = new CountDownLatch(1);
    final Holder h = new Holder();
    final NativeWaiter w = new NativeWaiter();
    Runnable r = new Runnable() {
      @Override
      public void run() {
        try {
          cdl1.countDown();
          synchronized(cdl1) {
            cdl1.wait();
          }

          cdl2.countDown();
          synchronized(cdl2) {
            cdl2.wait(1000);  // Wait a second.
          }

          cdl3_1.await();
          cdl3_2.countDown();
          synchronized(cdl3_2) {
            // Nothing, just wanted to block on cdl3.
          }

          cdl4.countDown();
          Thread.sleep(1000);

          cdl5.countDown();
          while (!h.flag) {
            // Busy-loop.
          }

          nativeLoop(w.struct);
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };

    Thread t = mkThread.apply(r);
    System.out.println("Thread type is " + t.getClass());
    printThreadState(t);
    t.start();

    // Waiting.
    cdl1.await();
    Thread.yield();
    Thread.sleep(100);
    printThreadState(t);
    synchronized(cdl1) {
      cdl1.notifyAll();
    }

    // Timed waiting.
    cdl2.await();
    Thread.yield();
    Thread.sleep(100);
    printThreadState(t);
    synchronized(cdl2) {
      cdl2.notifyAll();
    }

    // Blocked on monitor.
    synchronized(cdl3_2) {
      cdl3_1.countDown();
      cdl3_2.await();
      // While the latch improves the chances to make good progress, scheduling might still be
      // messy. Wait till we get the right Java-side Thread state.
      do {
        Thread.yield();
      } while (t.getState() != Thread.State.BLOCKED);
      // Since internal thread suspension (For GC or other cases) can happen at any time and changes
      // the thread state we just have it print the majority thread state across 11 calls over 55
      // milliseconds.
      printMajorityThreadState(t, 11, 5);
    }

    // Sleeping.
    cdl4.await();
    Thread.yield();
    Thread.sleep(100);
    printThreadState(t);

    // Running.
    cdl5.await();
    Thread.yield();
    Thread.sleep(100);
    printThreadState(t);
    h.flag = true;

    // Native
    w.waitForNative();
    printThreadState(t);
    w.finish();

    // Dying.
    t.join();
    Thread.yield();
    Thread.sleep(100);

    printThreadState(t);
  }

  private static void doAllThreadsTests() {
    Thread[] threads = getAllThreads();
    List<Thread> threadList = new ArrayList<>(Arrays.asList(threads));

    // Filter out JIT thread. It may or may not be there depending on configuration.
    Iterator<Thread> it = threadList.iterator();
    while (it.hasNext()) {
      Thread t = it.next();
      if (t.getName().startsWith("Jit thread pool worker")) {
        it.remove();
        break;
      }
    }

    Collections.sort(threadList, THREAD_COMP);

    List<Thread> expectedList = new ArrayList<>();
    Set<Thread> threadsFromTraces = Thread.getAllStackTraces().keySet();

    expectedList.add(findThreadByName(threadsFromTraces, "FinalizerDaemon"));
    expectedList.add(findThreadByName(threadsFromTraces, "FinalizerWatchdogDaemon"));
    expectedList.add(findThreadByName(threadsFromTraces, "HeapTaskDaemon"));
    expectedList.add(findThreadByName(threadsFromTraces, "ReferenceQueueDaemon"));
    // We can't get the signal catcher through getAllStackTraces. So ignore it.
    // expectedList.add(findThreadByName(threadsFromTraces, "Signal Catcher"));
    expectedList.add(findThreadByName(threadsFromTraces, "TestThread"));
    expectedList.add(findThreadByName(threadsFromTraces, "main"));

    if (!threadList.containsAll(expectedList)) {
      throw new RuntimeException("Expected " + expectedList + " as subset, got " + threadList);
    }
    System.out.println(expectedList);
  }

  private static Thread findThreadByName(Set<Thread> threads, String name) {
    for (Thread t : threads) {
        if (t.getName().equals(name)) {
            return t;
        }
    }
    throw new RuntimeException("Did not find thread " + name + ": " + threads);
  }

  private static void doTLSTests() throws Exception {
    doTLSNonLiveTests();
    doTLSLiveTests();
  }

  private static void doTLSNonLiveTests() throws Exception {
    Thread t = new Thread();
    try {
      setTLS(t, 1);
      System.out.println("Expected failure setting TLS for non-live thread");
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
    t.start();
    t.join();
    try {
      setTLS(t, 1);
      System.out.println("Expected failure setting TLS for non-live thread");
    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }

  private static void doTLSLiveTests() throws Exception {
    setTLS(Thread.currentThread(), 1);

    long l = getTLS(Thread.currentThread());
    if (l != 1) {
      throw new RuntimeException("Unexpected TLS value: " + l);
    };

    final CountDownLatch cdl1 = new CountDownLatch(1);
    final CountDownLatch cdl2 = new CountDownLatch(1);

    Runnable r = new Runnable() {
      @Override
      public void run() {
        try {
          cdl1.countDown();
          cdl2.await();
          setTLS(Thread.currentThread(), 2);
          if (getTLS(Thread.currentThread()) != 2) {
            throw new RuntimeException("Different thread issue");
          }
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };

    Thread t = new Thread(r);
    t.start();
    cdl1.await();
    setTLS(Thread.currentThread(), 1);
    cdl2.countDown();

    t.join();
    if (getTLS(Thread.currentThread()) != 1) {
      throw new RuntimeException("Got clobbered");
    }
  }

  private static void doTestEvents() throws Exception {
    enableThreadEvents(true);

    final CountDownLatch cdl1 = new CountDownLatch(1);
    final CountDownLatch cdl2 = new CountDownLatch(1);

    Runnable r = new Runnable() {
      @Override
      public void run() {
        try {
          cdl1.countDown();
          cdl2.await();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };
    Thread t = new Thread(r, "EventTestThread");

    System.out.println("Constructed thread");
    Thread.yield();
    Thread.sleep(100);
    System.out.println(Arrays.toString(getThreadEventMessages()));

    t.start();
    cdl1.await();

    System.out.println(Arrays.toString(getThreadEventMessages()));

    cdl2.countDown();
    t.join();
    System.out.println(Arrays.toString(getThreadEventMessages()));

    System.out.println("Thread joined");

    enableThreadEvents(false);
  }

  private final static Comparator<Thread> THREAD_COMP = new Comparator<Thread>() {
    public int compare(Thread o1, Thread o2) {
      return o1.getName().compareTo(o2.getName());
    }
  };

  private final static Map<Integer, String> STATE_NAMES = new HashMap<Integer, String>();
  private final static List<Integer> STATE_KEYS = new ArrayList<Integer>();
  static {
    STATE_NAMES.put(0x1, "ALIVE");
    STATE_NAMES.put(0x2, "TERMINATED");
    STATE_NAMES.put(0x4, "RUNNABLE");
    STATE_NAMES.put(0x400, "BLOCKED_ON_MONITOR_ENTER");
    STATE_NAMES.put(0x80, "WAITING");
    STATE_NAMES.put(0x10, "WAITING_INDEFINITELY");
    STATE_NAMES.put(0x20, "WAITING_WITH_TIMEOUT");
    STATE_NAMES.put(0x40, "SLEEPING");
    STATE_NAMES.put(0x100, "IN_OBJECT_WAIT");
    STATE_NAMES.put(0x200, "PARKED");
    STATE_NAMES.put(0x100000, "SUSPENDED");
    STATE_NAMES.put(0x200000, "INTERRUPTED");
    STATE_NAMES.put(0x400000, "IN_NATIVE");
    STATE_KEYS.addAll(STATE_NAMES.keySet());
    Collections.sort(STATE_KEYS);
  }

  // Call getThreadState 'votes' times waiting 'wait' millis between calls and print the most common
  // result.
  private static void printMajorityThreadState(Thread t, int votes, int wait) throws Exception {
    Map<Integer, Integer> states = new HashMap<>();
    for (int i = 0; i < votes; i++) {
      int cur_state = getThreadState(t);
      states.put(cur_state, states.getOrDefault(cur_state, 0) + 1);
      Thread.sleep(wait);  // Wait a little bit.
    }
    int best_state = -1;
    int highest_count = 0;
    for (Map.Entry<Integer, Integer> e : states.entrySet()) {
      if (e.getValue() > highest_count) {
        highest_count = e.getValue();
        best_state = e.getKey();
      }
    }
    printThreadState(best_state);
  }

  private static void printThreadState(Thread t) {
    printThreadState(getThreadState(t));
  }

  private static void printThreadState(int state) {
    StringBuilder sb = new StringBuilder();

    for (Integer i : STATE_KEYS) {
      if ((state & i) != 0) {
        if (sb.length()>0) {
          sb.append('|');
        }
        sb.append(STATE_NAMES.get(i));
      }
    }

    if (sb.length() == 0) {
      sb.append("NEW");
    }

    System.out.println(Integer.toHexString(state) + " = " + sb.toString());
  }

  private static void printThreadInfo(Thread t) {
    Object[] threadInfo = getThreadInfo(t);
    if (threadInfo == null || threadInfo.length != 5) {
      System.out.println(Arrays.toString(threadInfo));
      throw new RuntimeException("threadInfo length wrong");
    }

    System.out.println(threadInfo[0]);  // Name
    System.out.println(threadInfo[1]);  // Priority
    System.out.println(threadInfo[2]);  // Daemon
    System.out.println(threadInfo[3]);  // Threadgroup
    System.out.println(threadInfo[4] == null ? "null" : threadInfo[4].getClass());  // Context CL.
  }

  public static final class NativeWaiter {
    public long struct;
    public NativeWaiter() {
      struct = nativeWaiterStructAlloc();
    }
    public void waitForNative() {
      if (struct == 0l) {
        throw new Error("Already resumed from native!");
      }
      nativeWaiterStructWaitForNative(struct);
    }
    public void finish() {
      if (struct == 0l) {
        throw new Error("Already resumed from native!");
      }
      nativeWaiterStructFinish(struct);
      struct = 0;
    }
  }

  private static native long nativeWaiterStructAlloc();
  private static native void nativeWaiterStructWaitForNative(long struct);
  private static native void nativeWaiterStructFinish(long struct);
  private static native void nativeLoop(long w);

  private static native Thread getCurrentThread();
  private static native Object[] getThreadInfo(Thread t);
  private static native int getThreadState(Thread t);
  private static native Thread[] getAllThreads();
  private static native void setTLS(Thread t, long l);
  private static native long getTLS(Thread t);
  private static native void enableThreadEvents(boolean b);
  private static native String[] getThreadEventMessages();
}
