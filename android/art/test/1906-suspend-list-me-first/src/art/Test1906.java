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

public class Test1906 {
  public static final Object lock = new Object();

  public static volatile boolean SECOND_THREAD_RUN = true;
  public static volatile boolean SECOND_THREAD_RUNNING = false;

  public static void waitFor(long millis) {
    try {
      lock.wait(millis);
    } catch (Exception e) {
      System.out.println("Unexpected error: " + e);
      e.printStackTrace();
    }
  }

  public static void waitForSuspension(Thread target) {
    while (!Suspension.isSuspended(target)) {
      waitFor(100);
    }
  }

  public static void run() {
    synchronized (lock) {
      final Thread second_thread = new Thread(
          () -> {
            while (SECOND_THREAD_RUN) { SECOND_THREAD_RUNNING = true; }
          },
          "SECONDARY THREAD");
      Thread self_thread = new Thread(
          () -> {
            try {
              // Wait for second thread to start doing stuff.
              while (!SECOND_THREAD_RUNNING) { }
              Suspension.suspendList(Thread.currentThread(), second_thread);
            } catch (Throwable t) {
              System.out.println("Unexpected error occurred " + t);
              t.printStackTrace();
              Runtime.getRuntime().halt(2);
            }
          },
          "TARGET THREAD");
      try {
        second_thread.start();
        self_thread.start();

        waitForSuspension(self_thread);

        // Wait to see if second thread is running.
        SECOND_THREAD_RUNNING = false;
        waitFor(1000);

        if (SECOND_THREAD_RUNNING) {
          System.out.println("Second thread running after first thread suspended self!");
        } else {
          System.out.println("Second thread suspended before first thread suspended self!");
        }

        Suspension.resume(self_thread);
        waitForSuspension(second_thread);
        Suspension.resume(second_thread);
        self_thread.join();
        SECOND_THREAD_RUN = false;
        second_thread.join();
      } catch (Throwable t) {
        System.out.println("something was thrown. Runtime might be in unrecoverable state: " + t);
        t.printStackTrace();
        Runtime.getRuntime().halt(2);
      }
    }
  }
}
