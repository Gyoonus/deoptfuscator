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

package art;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;

public class Test1909 {

  public static class ThreadHolder {
    public Thread thr;
    public ThreadHolder(Thread thr) {
      this.thr = thr;
    }

    public long getTLS(long jvmtienv) {
      return Test1909.getTLS(jvmtienv, this.thr);
    }
    public void setTLS(long jvmtienv, long ptr) {
      Test1909.setTLS(jvmtienv, this.thr, ptr);
    }
  }

  public static class ThreadWaiter {
    public boolean exit;
    public Thread thr;
    public final Object lock;

    public ThreadWaiter() {
      this.exit = false;
      this.lock = new Object();
      this.thr = new Thread(() -> {
        try {
          synchronized (lock) {
            while (!this.exit) {
              this.lock.wait();
            }
          }
        } catch (Exception e) {
          e.printStackTrace();
        }
      });
      // Kill threads if we exit.
      thr.setDaemon(true);
      thr.start();
    }

    public void cleanup() throws Exception {
      synchronized (lock) {
        exit = true;
        lock.notifyAll();
      }
      thr.join();
    }
    public long getTLS(long jvmtienv) {
      return Test1909.getTLS(jvmtienv, this.thr);
    }
    public void setTLS(long jvmtienv, long ptr) {
      Test1909.setTLS(jvmtienv, this.thr, ptr);
    }
  }

  public static void checkEq(long a, long b) {
    if (a != b) {
      throw new Error("Expected: " + a + " got: " + b);
    }
  }

  public static void run() throws Exception {
    ThreadHolder tc = new ThreadHolder(Thread.currentThread());
    ThreadWaiter t1 = new ThreadWaiter();
    long e1 = newJvmtiEnv();
    long e2 = newJvmtiEnv();

    // Everything should be 0
    checkEq(0, tc.getTLS(e1));
    checkEq(0, t1.getTLS(e1));
    checkEq(0, tc.getTLS(e2));
    checkEq(0, t1.getTLS(e2));

    // Set in one pair.
    tc.setTLS(e1, 1);
    checkEq(1, tc.getTLS(e1));
    checkEq(0, t1.getTLS(e1));
    checkEq(0, tc.getTLS(e2));
    checkEq(0, t1.getTLS(e2));

    // Set in another pair.
    t1.setTLS(e1, 2);
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(0, tc.getTLS(e2));
    checkEq(0, t1.getTLS(e2));

    // Set in third pair.
    tc.setTLS(e2, 3);
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(3, tc.getTLS(e2));
    checkEq(0, t1.getTLS(e2));

    // Set in fourth pair.
    t1.setTLS(e2, 4);
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(3, tc.getTLS(e2));
    checkEq(4, t1.getTLS(e2));

    // Create a new thread and make sure everything is 0.
    ThreadWaiter t2 = new ThreadWaiter();
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(0, t2.getTLS(e1));
    checkEq(3, tc.getTLS(e2));
    checkEq(4, t1.getTLS(e2));
    checkEq(0, t2.getTLS(e2));

    // Create a new jvmtienv and make sure everything is 0.
    long e3 = newJvmtiEnv();
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(0, t2.getTLS(e1));
    checkEq(3, tc.getTLS(e2));
    checkEq(4, t1.getTLS(e2));
    checkEq(0, t2.getTLS(e2));
    checkEq(0, tc.getTLS(e3));
    checkEq(0, t1.getTLS(e3));
    checkEq(0, t2.getTLS(e3));

    // Delete an env without data and make sure everything is still there.
    destroyJvmtiEnv(e3);
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(0, t2.getTLS(e1));
    checkEq(3, tc.getTLS(e2));
    checkEq(4, t1.getTLS(e2));
    checkEq(0, t2.getTLS(e2));

    // Delete an env with data and make sure everything is still there.
    destroyJvmtiEnv(e2);
    checkEq(1, tc.getTLS(e1));
    checkEq(2, t1.getTLS(e1));
    checkEq(0, t2.getTLS(e1));

    // Delete a thread. Make sure other thread still has data.
    t1.cleanup();
    checkEq(1, tc.getTLS(e1));
    checkEq(0, t2.getTLS(e1));

    t2.cleanup();

    System.out.println("Test passed");
  }

  public static native long getTLS(long jvmtienv, Thread thr);
  public static native void setTLS(long jvmtienv, Thread thr, long ptr);
  public static native long newJvmtiEnv();
  public static native void destroyJvmtiEnv(long jvmtienv);
}
