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
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.Vector;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.function.Supplier;
import java.util.function.Consumer;

public class Test1917 {
  public final static boolean TEST_PRINT_ALL = false;

  public static class ThreadPauser implements Runnable {
    public Semaphore sem_wakeup_main = new Semaphore(0);
    public Semaphore sem_wait = new Semaphore(0);

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
      while (!sem_wait.hasQueuedThreads()) {}
    }

    public void wakeupOtherThread() throws Exception {
      sem_wait.release();
    }
  }

  public static class StackTraceGenerator implements Runnable {
    private final Thread thr;
    private final Consumer<StackTrace.StackFrameData> con;
    public StackTraceGenerator(Thread thr, Consumer<StackTrace.StackFrameData> con) {
      this.thr = thr;
      this.con = con;
    }

    public StackTraceGenerator(Consumer<StackTrace.StackFrameData> con) {
      this(null, con);
    }

    public Thread getThread() {
      if (thr == null) {
        return Thread.currentThread();
      } else {
        return thr;
      }
    }
    public void run() {
      for (StackTrace.StackFrameData s : StackTrace.GetStackTrace(getThread())) {
        con.accept(s);
      }
    }
  }

  public static class RecurCount implements Runnable {
    private final int cnt;
    private final Runnable then;
    public RecurCount(int cnt, Runnable then) {
      this.cnt = cnt;
      this.then = then;
    }

    public void run() {
      doRecur(0);
    }

    public void doRecur(int n) {
      if (n < cnt) {
        doRecur(n + 1);
      } else {
        then.run();
      }
    }
  }

  public static Consumer<StackTrace.StackFrameData> makePrintStackFramesConsumer()
      throws Exception {
    final Method end_method = Test1917.class.getDeclaredMethod("run");
    return new Consumer<StackTrace.StackFrameData>() {
      public void accept(StackTrace.StackFrameData data) {
        if (TEST_PRINT_ALL) {
          System.out.println(data);
        } else {
          Package p = data.method.getDeclaringClass().getPackage();
          // Filter out anything to do with the testing harness.
          if (p != null && p.equals(Test1917.class.getPackage())) {
            System.out.printf("'%s' line: %d\n",
                data.method,
                Breakpoint.locationToLine(data.method, data.current_location));
          } else if (data.method.getDeclaringClass().equals(Semaphore.class)) {
            System.out.printf("'%s' line: <NOT-DETERMINISTIC>\n", data.method);
          }
        }
      }
    };
  }

  public static void run() throws Exception {
    System.out.println("Recurring 5 times");
    new RecurCount(5, new StackTraceGenerator(makePrintStackFramesConsumer())).run();

    System.out.println("Recurring 5 times on another thread");
    Thread thr = new Thread(
        new RecurCount(5, new StackTraceGenerator(makePrintStackFramesConsumer())));
    thr.start();
    thr.join();

    System.out.println("Recurring 5 times on another thread. Stack trace from main thread!");
    ThreadPauser pause = new ThreadPauser();
    Thread thr2 = new Thread(new RecurCount(5, pause));
    thr2.start();
    pause.waitForOtherThreadToPause();
    new StackTraceGenerator(thr2, makePrintStackFramesConsumer()).run();
    pause.wakeupOtherThread();
    thr2.join();
  }
}
