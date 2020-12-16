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
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.concurrent.Semaphore;

public class Test1944 {
  // Just calculate fib forever.
  public static void fib(Semaphore started) {
    started.release();
    long a = 1;
    long b = 1;
    while (true) {
      long c = a + b;
      a = b;
      b = c;
    }
  }

  // Don't bother actually doing anything.
  public static void notifySingleStep(Thread thr, Executable e, long loc) { }

  public static native void exitNow();

  private static int num_threads = 10;

  public static void run() throws Exception {
    final Semaphore started = new Semaphore(-(num_threads - 1));

    Trace.enableSingleStepTracing(Test1944.class,
        Test1944.class.getDeclaredMethod(
            "notifySingleStep", Thread.class, Executable.class, Long.TYPE),
        null);

    Thread[] threads = new Thread[num_threads];
    for (int i = 0; i < num_threads; i++) {
      threads[i] = new Thread(() -> { fib(started); });
      // Make half daemons.
      threads[i].setDaemon(i % 2 == 0);
      threads[i].start();
    }
    // Wait for all threads to start.
    started.acquire();
    System.out.println("All threads started");
    // sleep a little
    Thread.sleep(10);
    // Die.
    System.out.println("Exiting suddenly");
    exitNow();
    System.out.println("FAILED: Should not reach here!");
  }
}
