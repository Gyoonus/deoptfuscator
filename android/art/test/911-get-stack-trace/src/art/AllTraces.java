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
import java.util.List;

public class AllTraces {
  private final static List<Object> RETAIN = new ArrayList<Object>();

  public static void doTest() throws Exception {
    System.out.println("################################");
    System.out.println("### Other threads (suspended) ###");
    System.out.println("################################");

    // Also create an unstarted and a dead thread.
    RETAIN.add(new Thread("UNSTARTED"));
    Thread deadThread = new Thread("DEAD");
    RETAIN.add(deadThread);
    deadThread.start();
    deadThread.join();

    final int N = 10;

    final ControlData data = new ControlData(N);
    data.waitFor = new Object();

    Thread threads[] = new Thread[N];

    for (int i = 0; i < N; i++) {
      Thread t = new Thread("AllTraces Thread " + i) {
        public void run() {
          Recurse.foo(4, 0, 0, data);
        }
      };
      t.start();
      threads[i] = t;
    }
    data.reached.await();
    Thread.yield();
    Thread.sleep(500);  // A little bit of time...

    printAll(0);

    printAll(5);

    printAll(25);

    // Let the thread make progress and die.
    synchronized(data.waitFor) {
      data.waitFor.notifyAll();
    }
    for (int i = 0; i < N; i++) {
      threads[i].join();
    }

    RETAIN.clear();
  }

  public static void printAll(int max) {
    PrintThread.printAll(getAllStackTraces(max));
  }

  // Get all stack traces. This will return an array with an element for each thread. The element
  // is an array itself with the first element being the thread, and the second element a nested
  // String array as in getStackTrace.
  public static native Object[][] getAllStackTraces(int max);
}
