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

public class Test1942 {
  public static void run() throws Exception {
    final Thread target_thread = new Thread(() -> {
      nativeRun();
    }, "target_thread");

    target_thread.start();

    // wait for the other thread to spin holding lock.
    waitForPause();

    System.out.println("Initial state.");
    System.out.println("isLocked() = " + isLocked());
    System.out.println("isSuspended(target_thread) = " + Suspension.isSuspended(target_thread));

    // Suspend it from java.
    System.out.println("Suspend and sleep.");
    Suspension.suspend(target_thread);
    // Wait for the other thread to do something.
    try { Thread.sleep(1000); } catch (Exception e) {}

    System.out.println("isLocked() = " + isLocked());
    System.out.println("isSuspended(target_thread) = " + Suspension.isSuspended(target_thread));

    // Let it try to unlock the monitor.
    System.out.println("Let other thread release the raw monitor.");
    // Let the thread try to lock the monitor.
    resume();

    // Wait for the other thread to do something. It should exit by the time this is done if it
    // has not hit a suspend point.
    while (isLocked()) {
      try { Thread.sleep(1000); } catch (Exception e) {}
    }

    System.out.println("isLocked() = " + isLocked());
    System.out.println("isSuspended(target_thread) = " + Suspension.isSuspended(target_thread));

    // Make sure the monitor is gone.
    grabRawMonitor();
    System.out.println("other thread doesn't hold lock!");

    // Resume it from java
    System.out.println("resumed test thread");
    Suspension.resume(target_thread);
    target_thread.join();
  }

  public static native void nativeRun();
  public static native void waitForPause();
  public static native void resume();
  public static native boolean isLocked();
  // Gets then releases raw monitor.
  public static native void grabRawMonitor();
}
