/*
 * Copyright (C) 2011 The Android Open Source Project
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

public class Test1936 {
  public static void foo() {}

  public static void NotifyThreadEnd(Thread me) {
    // Don't actually do anything.
    foo();
  }

  public static void NotifyMethodEntry(Object o) {
    System.out.println("Entered " + o.toString());
    Thread me = Thread.currentThread();
    System.out.println(String.format(
        "Thread: %s\n" +
        "  | alive: %b\n" +
        "  | interrupted: %b\n" +
        "  | daemon: %b\n" +
        "  | group: %s\n",
        me.getName(), me.isAlive(), me.isInterrupted(), me.isDaemon(), me.getThreadGroup()));
  }

  public static native void waitForever();
  private static void setupTracing(Thread target) throws Exception {
    Trace.disableTracing(target);
    Trace.enableTracing2(
        Test1936.class,
        Test1936.class.getDeclaredMethod("NotifyMethodEntry", Object.class),
        /*exit*/null,
        /*field_access*/null,
        /*field_modify*/null,
        /*single_step*/null,
        /*thread_start*/null,
        Test1936.class.getDeclaredMethod("NotifyThreadEnd", Thread.class),
        target);
  }


  public static void run() throws Exception {
    Thread t = new Thread(() -> {
      try {
        setupTracing(Thread.currentThread());
        foo();
      } catch (Exception e) {
        System.out.println("Caught exception " + e + "!");
        e.printStackTrace();
      }
    }, "test-thread");
    t.start();
    t.join();
  }
}
