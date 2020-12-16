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

public class Frames {
  public static void doTest() throws Exception {
    doTestSameThread();

    System.out.println();

    doTestOtherThreadWait();

    System.out.println();

    doTestOtherThreadBusyLoop();
  }

  public static void doTestSameThread() {
    System.out.println("###################");
    System.out.println("### Same thread ###");
    System.out.println("###################");

    Thread t = Thread.currentThread();

    int count = getFrameCount(t);
    System.out.println(count);
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, -1)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
    for (int i = 0; i < count; i++) {
      System.out.println(Arrays.toString(getFrameLocation(t, i)));
    }
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, count)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
  }

  public static void doTestOtherThreadWait() throws Exception {
    System.out.println("################################");
    System.out.println("### Other thread (suspended) ###");
    System.out.println("################################");
    final ControlData data = new ControlData();
    data.waitFor = new Object();
    Thread t = new Thread("Frames doTestOtherThreadWait") {
      public void run() {
        Recurse.foo(4, 0, 0, data);
      }
    };
    t.start();
    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    int count = getFrameCount(t);
    System.out.println(count);
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, -1)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
    for (int i = 0; i < count; i++) {
      System.out.println(Arrays.toString(getFrameLocation(t, i)));
    }
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, count)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    // Let the thread make progress and die.
    synchronized(data.waitFor) {
      data.waitFor.notifyAll();
    }
    t.join();
  }

  public static void doTestOtherThreadBusyLoop() throws Exception {
    System.out.println("###########################");
    System.out.println("### Other thread (live) ###");
    System.out.println("###########################");
    final ControlData data = new ControlData();
    Thread t = new Thread("Frames doTestOtherThreadBusyLoop") {
      public void run() {
        Recurse.foo(4, 0, 0, data);
      }
    };
    t.start();
    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    int count = getFrameCount(t);
    System.out.println(count);
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, -1)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
    for (int i = 0; i < count; i++) {
      System.out.println(Arrays.toString(getFrameLocation(t, i)));
    }
    try {
      System.out.println(Arrays.toString(getFrameLocation(t, count)));
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }

    // Let the thread stop looping and die.
    data.stop = true;
    t.join();
  }

  public static native int getFrameCount(Thread thread);
  public static native Object[] getFrameLocation(Thread thread, int depth);
}
