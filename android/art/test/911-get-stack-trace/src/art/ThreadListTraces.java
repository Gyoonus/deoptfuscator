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

public class ThreadListTraces {
  public static void doTest() throws Exception {
    System.out.println("########################################");
    System.out.println("### Other select threads (suspended) ###");
    System.out.println("########################################");

    final int N = 10;

    final ControlData data = new ControlData(N);
    data.waitFor = new Object();

    Thread threads[] = new Thread[N];

    Thread list[] = new Thread[N/2 + 1];

    for (int i = 0; i < N; i++) {
      Thread t = new Thread("ThreadListTraces Thread " + i) {
        public void run() {
          Recurse.foo(4, 0, 0, data);
        }
      };
      t.start();
      threads[i] = t;
      if (i % 2 == 0) {
        list[i/2] = t;
      }
    }
    list[list.length - 1] = Thread.currentThread();

    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    printList(list, 0);

    printList(list, 5);

    printList(list, 25);

    // Let the thread make progress and die.
    synchronized(data.waitFor) {
      data.waitFor.notifyAll();
    }
    for (int i = 0; i < N; i++) {
      threads[i].join();
    }
  }

  public static void printList(Thread[] threads, int max) {
    PrintThread.printAll(getThreadListStackTraces(threads, max));
  }

  // Similar to getAllStackTraces, but restricted to the given threads.
  public static native Object[][] getThreadListStackTraces(Thread threads[], int max);
}
