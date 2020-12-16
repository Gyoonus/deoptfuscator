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

public class Test1907 {
  public static final Object lock = new Object();

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
      Thread thrd = new Thread(
          () -> {
            try {
              // Put self twice in the suspend list
              System.out.println("Suspend self twice returned: " +
                  Arrays.toString(
                      Suspension.suspendList(Thread.currentThread(), Thread.currentThread())));
            } catch (Throwable t) {
              System.out.println("Unexpected error occurred " + t);
              t.printStackTrace();
              Runtime.getRuntime().halt(2);
            }
          },
          "TARGET THREAD");
      try {
        thrd.start();

        // Wait for at least one suspend to happen.
        waitForSuspension(thrd);

        // Wake it up.
        Suspension.resume(thrd);
        waitFor(1000);

        // Is it suspended.
        if (Suspension.isSuspended(thrd)) {
          Suspension.resume(thrd);
          thrd.join();
          System.out.println("Thread was still suspended after one resume.");
        } else {
          thrd.join();
          System.out.println("Thread was no longer suspended after one resume.");
        }

      } catch (Throwable t) {
        System.out.println("something was thrown. Runtime might be in unrecoverable state: " + t);
        t.printStackTrace();
        Runtime.getRuntime().halt(2);
      }
    }
  }
}
