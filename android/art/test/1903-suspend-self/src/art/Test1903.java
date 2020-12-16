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

public class Test1903 {
  public static final Object lock = new Object();

  public static volatile boolean OTHER_THREAD_CONTINUE = true;
  public static volatile boolean OTHER_THREAD_DID_SOMETHING = true;
  public static volatile boolean OTHER_THREAD_STARTED = false;
  public static volatile boolean OTHER_THREAD_RESUMED = false;

  public static class OtherThread implements Runnable {
    @Override
    public void run() {
      // Wake up main thread.
      OTHER_THREAD_STARTED = true;
      try {
        Suspension.suspend(Thread.currentThread());
        OTHER_THREAD_RESUMED = true;
      } catch (Throwable t) {
        System.out.println("Unexpected error occurred " + t);
        t.printStackTrace();
        Runtime.getRuntime().halt(2);
      }
    }
  }

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

  public static void waitForStart() {
    while (!OTHER_THREAD_STARTED) {
      waitFor(100);
    }
  }

  public static void run() {
    synchronized (lock) {
      Thread other = new Thread(new OtherThread(), "TARGET THREAD");
      try {
        other.start();

        // Wait for the other thread to actually start doing things.

        waitForStart();
        waitForSuspension(other);

        Suspension.resume(other);
        for (int i = 0; i < 1000; i++) {
          waitFor(100);
          if (OTHER_THREAD_RESUMED) {
            return;
          }
        }
        System.out.println("Failed to resume thread!");
        Runtime.getRuntime().halt(4);
      } catch (Throwable t) {
        System.out.println("something was thrown. Runtime might be in unrecoverable state: " + t);
        t.printStackTrace();
        Runtime.getRuntime().halt(2);
      }
    }
  }
}
