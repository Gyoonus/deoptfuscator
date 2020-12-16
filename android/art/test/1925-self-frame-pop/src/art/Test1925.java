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

public class Test1925 {
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

  public static class RecursionError extends Error {
    public RecursionError(String s) { super(s); }
  }
  public static void recurTimesK(int times, Runnable safepoint) {
    if (times == 0) {
      safepoint.run();
      return;
    }
    safepoint.run();
    throw new RecursionError("Unable recur further. Still " + times + " outstanding!");
  }

  public static void doRecurTestWith(final int times, int watch_frame) throws Exception {
    final String target_method_name_start = "recurTimes";
    final Runnable safepoint = () -> {
      StackTrace.StackFrameData target_frame = null;
      int cnt = 0;
      for (StackTrace.StackFrameData frame : StackTrace.GetStackTrace(Thread.currentThread())) {
        if (frame.method.getName().startsWith(target_method_name_start)) {
          if (times - cnt == watch_frame) {
            target_frame = frame;
            break;
          } else {
            cnt++;
          }
        }
      }
      try {
        FramePop.notifyFramePop(null, target_frame.depth);
      } catch (Exception e) {
        throw new Error("Unexpected error in notifyFramePop!", e);
      }
    };
    try {
      recurTimesA(times, safepoint);
      System.out.println("Ran recurTimes(" + times + ") without errors!");
    } catch (Throwable e) {
      System.out.println("Caught exception " + e + " while running recurTimes(" + times + ")");
    }
  }

  public static void run() throws Exception {
    FramePop.enableFramePopEvent(
        Test1925.class,
        Test1925.class.getDeclaredMethod(
            "handleFramePop", Executable.class, Boolean.TYPE, Long.TYPE),
        null);
    doRecurTestWith(10, 5);
    doRecurTestWith(100, 95);
  }
}
