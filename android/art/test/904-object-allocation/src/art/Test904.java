/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.util.ArrayList;
import java.util.Arrays;

public class Test904 {
  public static void run() throws Exception {
    // Use a list to ensure objects must be allocated.
    ArrayList<Object> l = new ArrayList<>(100);

    prefetchClassNames();

    doTest(l);
  }

  // Pre-resolve class names so the strings don't have to be allocated as a side effect of
  // callback printing.
  private static void prefetchClassNames() {
      Object.class.getName();
      Integer.class.getName();
      Float.class.getName();
      Short.class.getName();
      Byte.class.getName();
      Double.class.getName();
  }

  public static void doTest(ArrayList<Object> l) throws Exception {
    // Disable the global registration from OnLoad, to get into a known state.
    enableAllocationTracking(null, false);

    // Enable actual logging callback.
    setupObjectAllocCallback(true);

    System.out.println(Arrays.toString(getTrackingEventMessages()));

    enableAllocationTracking(null, true);

    l.add(new Object());
    l.add(new Integer(1));

    enableAllocationTracking(null, false);

    l.add(new Float(1.0f));

    enableAllocationTracking(Thread.currentThread(), true);

    l.add(new Short((short)0));

    enableAllocationTracking(Thread.currentThread(), false);

    l.add(new Byte((byte)0));

    System.out.println(Arrays.toString(getTrackingEventMessages()));
    System.out.println("Tracking on same thread");

    testThread(l, true, true);

    l.add(new Byte((byte)0));

    System.out.println(Arrays.toString(getTrackingEventMessages()));
    System.out.println("Tracking on same thread, not disabling tracking");

    testThread(l, true, false);

    System.out.println(Arrays.toString(getTrackingEventMessages()));
    System.out.println("Tracking on different thread");

    testThread(l, false, true);

    l.add(new Byte((byte)0));

    // Disable actual logging callback and re-enable tracking, so we can keep the event enabled and
    // check that shutdown works correctly.
    setupObjectAllocCallback(false);

    System.out.println(Arrays.toString(getTrackingEventMessages()));

    enableAllocationTracking(null, true);
  }

  private static void testThread(final ArrayList<Object> l, final boolean sameThread,
      final boolean disableTracking) throws Exception {
    final SimpleBarrier startBarrier = new SimpleBarrier(1);
    final SimpleBarrier trackBarrier = new SimpleBarrier(1);
    final SimpleBarrier disableBarrier = new SimpleBarrier(1);

    final Thread thisThread = Thread.currentThread();

    Thread t = new Thread() {
      public void run() {
        try {
          startBarrier.dec();
          trackBarrier.waitFor();
        } catch (Exception e) {
          e.printStackTrace(System.out);
          System.exit(1);
        }

        l.add(new Double(0.0));

        if (disableTracking) {
          enableAllocationTracking(sameThread ? this : thisThread, false);
        }
      }
    };

    t.start();
    startBarrier.waitFor();
    enableAllocationTracking(sameThread ? t : Thread.currentThread(), true);
    trackBarrier.dec();

    t.join();
  }

  private static class SimpleBarrier {
    int count;

    public SimpleBarrier(int i) {
      count = i;
    }

    public synchronized void dec() throws Exception {
      count--;
      notifyAll();
    }

    public synchronized void waitFor() throws Exception  {
      while (count != 0) {
        wait();
      }
    }
  }

  private static native void setupObjectAllocCallback(boolean enable);
  private static native void enableAllocationTracking(Thread thread, boolean enable);
  private static native String[] getTrackingEventMessages();
}
