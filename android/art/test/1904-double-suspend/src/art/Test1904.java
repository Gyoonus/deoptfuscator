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

public class Test1904 {
  public static final Object lock = new Object();

  public static volatile boolean OTHER_THREAD_CONTINUE = true;
  public static volatile boolean OTHER_THREAD_DID_SOMETHING = true;
  public static volatile boolean OTHER_THREAD_STARTED = false;

  public static class OtherThread implements Runnable {
    @Override
    public void run() {
      OTHER_THREAD_STARTED = true;
      while (OTHER_THREAD_CONTINUE) {
        OTHER_THREAD_DID_SOMETHING = true;
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

        waitForStart();

        Suspension.suspend(other);

        waitForSuspension(other);
        OTHER_THREAD_DID_SOMETHING = false;
        // Wait a second to see if anything happens.
        waitFor(1000);

        if (OTHER_THREAD_DID_SOMETHING) {
          System.out.println("Looks like other thread did something while suspended!");
        }

        try {
          Suspension.suspend(other);
        } catch (Exception e) {
          System.out.println("Got exception " + e.getMessage());
        }

        // Resume always.
        Suspension.resume(other);

        // Wait another second.
        waitFor(1000);

        if (!OTHER_THREAD_DID_SOMETHING) {
          System.out.println("Doesn't look like the thread unsuspended!");
        }

        // Stop the other thread.
        OTHER_THREAD_CONTINUE = false;
        // Wait for 1 second for it to die.
        other.join(1000);

        if (other.isAlive()) {
          System.out.println("other thread didn't terminate in a reasonable time!");
          Runtime.getRuntime().halt(1);
        }
      } catch (Throwable t) {
        System.out.println("something was thrown. Runtime might be in unrecoverable state: " + t);
        t.printStackTrace();
        Runtime.getRuntime().halt(2);
      }
    }
  }
}
