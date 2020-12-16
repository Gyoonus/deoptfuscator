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

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.concurrent.Semaphore;
import java.util.Arrays;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.function.IntUnaryOperator;
import java.util.function.Function;

public class Test1926 {
  public static void handleFramePop(Executable m, boolean exception, long location) {
    System.out.println(
        m + " pop. Line=" + Breakpoint.locationToLine(m, location) + " exception:" + exception);
  }

  public static void recurTimesA(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesB(times - 1, safepoint);
  }

  public static void recurTimesB(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesC(times - 1, safepoint);
  }

  public static void recurTimesC(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesD(times - 1, safepoint);
  }

  public static void recurTimesD(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesE(times - 1, safepoint);
  }

  public static void recurTimesE(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesF(times - 1, safepoint);
  }

  public static void recurTimesF(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesG(times - 1, safepoint);
  }

  public static void recurTimesG(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesH(times - 1, safepoint);
  }

  public static void recurTimesH(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesI(times - 1, safepoint);
  }

  public static void recurTimesI(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesJ(times - 1, safepoint);
  }

  public static void recurTimesJ(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesK(times - 1, safepoint);
  }

  public static void recurTimesK(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    recurTimesL(times - 1, safepoint);
  }

  public static class RecursionError extends Error {
    public RecursionError(String s) { super(s); }
  }

  public static void recurTimesL(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    safepoint.run();
    throw new RecursionError("Unable recur further. Still " + times + " outstanding!");
  }

  public static class ThreadPauser implements Runnable {
    public final Semaphore sem_wakeup_main;
    public final Semaphore sem_wait;

    public ThreadPauser() {
      sem_wakeup_main = new Semaphore(0);
      sem_wait = new Semaphore(0);
    }

    public void run() {
      try {
        sem_wakeup_main.release();
        sem_wait.acquire();
      } catch (Exception e) {
        throw new Error("Error with semaphores!", e);
      }
    }

    public void waitForOtherThreadToPause() throws Exception {
      sem_wakeup_main.acquire();
    }

    public void wakeupOtherThread() throws Exception {
      sem_wait.release();
    }
  }

  public static void doRecurTestWith(final int times, int watch_frame) throws Exception {
    final String target_method_name_start = "recurTimes";
    final ThreadPauser safepoint = new ThreadPauser();
    // We need to make the stack bigger since ASAN causes SOE if we don't.
    Thread target = new Thread(
        /*thread Group*/ null,
        () -> {
          recurTimesA(times, () -> {
            safepoint.run();
            disableFramePop(null);
          });
          System.out.println("Ran recurTimes(" + times + ") without errors after disabling " +
              "frame pop event!");
          System.out.println("renabling frame pop event with similar stack.");
          recurTimesB(times, () -> { reenableFramePop(null); });
          System.out.println("Ran recurTimes(" + times + ") without errors!");
        },
        "Test1926-Thread",
        /*10 mb stack*/10 * 1024 * 1024);
    target.start();
    safepoint.waitForOtherThreadToPause();
    Suspension.suspend(target);
    // Safe block
    int cnt = 0;
    StackTrace.StackFrameData target_frame = null;
    for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(target)) {
      if (frame.method.getName().startsWith(target_method_name_start)) {
        if (times - cnt == watch_frame) {
          target_frame = frame;
          break;
        } else {
          cnt++;
        }
      }
    }
    if (target_frame != null) {
      FramePop.notifyFramePop(target, target_frame.depth);
    } else {
      System.out.println(
          "Unable to find stack frame for " + watch_frame + " depth of "
          + target_method_name_start);
    }
    Suspension.resume(target);
    safepoint.wakeupOtherThread();
    target.join();
  }

  public static void run() throws Exception {
    // Listen for events on all threads.
    FramePop.enableFramePopEvent(
        Test1926.class,
        Test1926.class.getDeclaredMethod(
            "handleFramePop", Executable.class, Boolean.TYPE, Long.TYPE),
        null);
    doRecurTestWith(10, 0);
    doRecurTestWith(10, 5);
    doRecurTestWith(10, 10);
  }

  public static native void disableFramePop(Thread thr);
  public static native void reenableFramePop(Thread thr);
}
