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

import java.util.concurrent.Semaphore;

public class Main {
  static final Semaphore start = new Semaphore(0);
  static volatile boolean continue_loop = true;

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Thread t = new Thread(Main::runTargetThread, "Target Thread");

    t.start();
    // Wait for other thread to start.
    start.acquire();

    System.out.println("pushing checkpoints");
    pushCheckpoints(t);

    System.out.println("checkpoints pushed");
    continue_loop = false;

    t.join();

    checkCheckpointsRun();

    System.out.println("Passed!");
  }

  public static native void pushCheckpoints(Thread t);
  public static native boolean checkCheckpointsRun();

  public static void doNothing() {}
  public static void runTargetThread() {
    System.out.println("Other thread running");
    try {
      start.release();
      while (continue_loop) {
        doNothing();
      }
    } catch (Exception e) {
      throw new Error("Exception occurred!", e);
    }
  }
}
