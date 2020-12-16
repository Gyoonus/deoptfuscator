/*
 * Copyright (C) 2011 The Android Open Source Project
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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.CyclicBarrier;

public class Main implements Runnable {

    // Timeout in minutes. Make it larger than the run-test timeout to get a native thread dump by
    // ART on timeout when running on the host.
    private final static long TIMEOUT_VALUE = 7;

    private final static long MAX_SIZE = 1000;  // Maximum size of array-list to allocate.

    private final static int THREAD_COUNT = 16;

    // Use a couple of different forms of synchronizing to test some of these...
    private final static AtomicInteger counter = new AtomicInteger();
    private final static Object gate = new Object();
    private volatile static int waitCount = 0;

    public static void main(String[] args) throws Exception {
        Thread[] threads = new Thread[THREAD_COUNT];

        // This barrier is used to synchronize the threads starting to allocate.
        // Note: Even though a barrier is not allocation-free, this one is fine, as it will be used
        //       before filling the heap.
        CyclicBarrier startBarrier = new CyclicBarrier(threads.length);

        for (int i = 0; i < threads.length; i++) {
            threads[i] = new Thread(new Main(startBarrier));
            threads[i].start();
        }

        // Wait for the threads to finish.
        for (Thread thread : threads) {
            thread.join();
        }

        // Allocate objects to definitely run GC before quitting.
        allocateObjectsToRunGc();

        new ArrayList<Object>(50);
    }

    private static void allocateObjectsToRunGc() {
      ArrayList<Object> l = new ArrayList<Object>();
      try {
          for (int i = 0; i < 100000; i++) {
              l.add(new ArrayList<Object>(i));
          }
      } catch (OutOfMemoryError oom) {
      }
    }

    private Main(CyclicBarrier startBarrier) {
        this.startBarrier = startBarrier;
    }

    private ArrayList<Object> store;
    private CyclicBarrier startBarrier;

    public void run() {
        try {
            work();
        } catch (Throwable t) {
            // Any exception or error getting here is bad.
            try {
                // May need allocations...
                t.printStackTrace(System.out);
            } catch (Throwable tInner) {
            }
            System.exit(1);
        }
    }

    private void work() throws Exception {
        // Any exceptions except an OOME in the allocation loop are bad and handed off to the
        // caller which should abort the whole runtime.

        ArrayList<Object> l = new ArrayList<Object>();
        store = l;  // Keep it alive.

        // Wait for the start signal.
        startBarrier.await(TIMEOUT_VALUE, java.util.concurrent.TimeUnit.MINUTES);

        // Allocate.
        try {
            for (int i = 0; i < MAX_SIZE; i++) {
                l.add(new ArrayList<Object>(i));
            }
        } catch (OutOfMemoryError oome) {
            // Fine, we're done.
        }

        // Atomically increment the counter and check whether we were last.
        int number = counter.incrementAndGet();

        if (number < THREAD_COUNT) {
            // Not last.
            synchronized (gate) {
                // Increment the wait counter.
                waitCount++;
                gate.wait(TIMEOUT_VALUE * 1000 * 60);
            }
        } else {
            // Last. Wait until waitCount == THREAD_COUNT - 1.
            for (int loops = 0; ; loops++) {
                synchronized (gate) {
                    if (waitCount == THREAD_COUNT - 1) {
                        // OK, everyone's waiting. Notify and break out.
                        gate.notifyAll();
                        break;
                    } else if (loops > 40) {
                        // 1s wait, too many tries.
                        System.out.println("Waited too long for the last thread.");
                        System.exit(1);
                    }
                }
                // Wait a bit.
                Thread.sleep(25);
            }
        }

        store = null;  // Allow GC to reclaim it.
    }
}
