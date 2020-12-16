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

public class Test1900 {
  public static void checkLE(long exp, long o) {
    if (exp > o) {
      throw new Error("Expected: " + exp + " Got: " + o);
    }
  }
  public static void checkEq(long exp, long o) {
    if (exp != o) {
      throw new Error("Expected: " + exp + " Got: " + o);
    }
  }

  public static void runConcurrent(Runnable... rs) throws Exception {
    final CountDownLatch latch = new CountDownLatch(rs.length);
    Thread[] thrs = new Thread[rs.length];
    for (int i = 0; i < rs.length; i++) {
      final Runnable r = rs[i];
      thrs[i] = new Thread(() -> {
        latch.countDown();
        r.run();
      });
      thrs[i].start();
    }
    for (Thread thr : thrs) {
      thr.join();
    }
  }
  static class Holder {
    public long val;
  }

  public static void run() throws Exception {
    initializeTest();
    // Get the overhead for the native part of this test.
    final long base_state = getAmountAllocated();

    // Basic alloc-dealloc
    checkEq(base_state + 0, getAmountAllocated());
    long abc = doAllocate(10);
    checkLE(base_state + 10, getAmountAllocated());
    long def = doAllocate(10);
    checkLE(base_state + 20, getAmountAllocated());
    doDeallocate(abc);
    checkLE(base_state + 10, getAmountAllocated());

    doDeallocate(def);

    checkEq(base_state + 0, getAmountAllocated());

    // Try doing it concurrently.
    Runnable add10 = () -> { long x = doAllocate(10); doDeallocate(x); };
    Runnable[] rs = new Runnable[100];
    Arrays.fill(rs, add10);
    runConcurrent(rs);
    checkEq(base_state + 0, getAmountAllocated());

    // Try doing it concurrently with different threads to allocate and deallocate.
    final Semaphore sem = new Semaphore(0);
    final Holder h = new Holder();
    runConcurrent(
        () -> {
          try {
            h.val = doAllocate(100);
            checkLE(base_state + 100, getAmountAllocated());
            sem.release();
          } catch (Exception e) { throw new Error("exception!", e); }
        },
        () -> {
          try {
            sem.acquire();
            long after_acq = getAmountAllocated();
            doDeallocate(h.val);
            checkLE(base_state + 100, after_acq);
          } catch (Exception e) { throw new Error("exception!", e); }
        }
    );
    checkEq(base_state + 0, getAmountAllocated());

    // Try doing it with multiple jvmtienvs.
    long env1 = newJvmtiEnv();
    long env2 = newJvmtiEnv();

    final long new_base_state = getAmountAllocated();
    // new jvmtienvs shouldn't save us memory.
    checkLE(base_state, new_base_state);
    // Make sure we track both.
    abc = doAllocate(env1, 10);
    checkLE(new_base_state + 10, getAmountAllocated());
    def = doAllocate(env2, 10);
    checkLE(new_base_state + 20, getAmountAllocated());
    doDeallocate(env1, abc);
    checkLE(new_base_state + 10, getAmountAllocated());

    doDeallocate(env2, def);

    checkEq(new_base_state + 0, getAmountAllocated());

    destroyJvmtiEnv(env1);
    destroyJvmtiEnv(env2);

    // Back to normal after getting rid of the envs.
    checkEq(base_state + 0, getAmountAllocated());

    // Try adding some tags
    Object a = new Object();
    Object b = new Object();
    Main.setTag(a, 100);
    Main.setTag(b, 200);

    // tags should be counted and should have some data associated with them.
    checkLE(base_state + 1, getAmountAllocated());
  }

  private static native long doAllocate(long jvmtienv, long size);
  private static long doAllocate(long size) {
    return doAllocate(getDefaultJvmtiEnv(), size);
  }

  private static native void doDeallocate(long jvmtienv, long ptr);
  private static void doDeallocate(long size) {
    doDeallocate(getDefaultJvmtiEnv(), size);
  }

  private static native long getDefaultJvmtiEnv();
  private static native long newJvmtiEnv();
  private static native void destroyJvmtiEnv(long jvmtienv);
  private static native long getAmountAllocated();
  private static native void initializeTest();
}
