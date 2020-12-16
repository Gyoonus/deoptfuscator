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

public class Test1943 {
  public static void run() throws Exception {
    final Thread target_thread = new Thread(() -> {
      nativeRun();
    }, "target_thread");

    target_thread.start();

    // wait for the other thread to spin holding lock.
    waitForPause();

    // Ensure that the other thread is in a wait.
    grabRawMonitor();
    System.out.println("target_thread is sleeping in a wait.");

    // Suspend it from java.
    System.out.println("Suspend target_thread.");
    Suspension.suspend(target_thread);

    // Let it try to unlock the monitor.
    System.out.println("Wake up the target_thread.");
    // Let the thread try to lock the monitor.
    nativeNotify();

    // Ensure that the other thread is suspended without the monitor.
    grabRawMonitor();
    System.out.println("target_thread is sleeping in suspend without lock.");

    // Check other thread is still alive
    System.out.println("target_thread.isAlive() = " + target_thread.isAlive());

    // Resume it from java
    System.out.println("resumed target_thread");
    Suspension.resume(target_thread);

    // Make sure the monitor is gone.
    grabRawMonitor();
    System.out.println("target_thread doesn't hold lock!");

    target_thread.join();
  }

  public static native void nativeRun();
  public static native void waitForPause();
  public static native void nativeNotify();
  // Gets then releases raw monitor.
  public static native void grabRawMonitor();
}
