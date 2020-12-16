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

public class OtherThread {
  public static void doTestOtherThreadWait() throws Exception {
    System.out.println("################################");
    System.out.println("### Other thread (suspended) ###");
    System.out.println("################################");
    final ControlData data = new ControlData();
    data.waitFor = new Object();
    Thread t = new Thread("OtherThread doTestOtherThreadWait") {
      public void run() {
        Recurse.foo(4, 0, 0, data);
      }
    };
    t.start();
    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    System.out.println("From top");
    PrintThread.print(t, 0, 25);
    PrintThread.print(t, 1, 25);
    PrintThread.print(t, 0, 5);
    PrintThread.print(t, 2, 5);

    System.out.println("From bottom");
    PrintThread.print(t, -1, 25);
    PrintThread.print(t, -5, 5);
    PrintThread.print(t, -7, 5);

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
    Thread t = new Thread("OtherThread doTestOtherThreadBusyLoop") {
      public void run() {
        Recurse.foo(4, 0, 0, data);
      }
    };
    t.start();
    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    System.out.println("From top");
    PrintThread.print(t, 0, 25);
    PrintThread.print(t, 1, 25);
    PrintThread.print(t, 0, 5);
    PrintThread.print(t, 2, 5);

    System.out.println("From bottom");
    PrintThread.print(t, -1, 25);
    PrintThread.print(t, -5, 5);
    PrintThread.print(t, -7, 5);

    // Let the thread stop looping and die.
    data.stop = true;
    t.join();
  }
}
