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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.atomic.AtomicInteger;

public class TestInvokeCustomWithConcurrentThreads extends TestBase implements Runnable {
    private static final int NUMBER_OF_THREADS = 16;

    private static final AtomicInteger nextIndex = new AtomicInteger(0);

    private static final ThreadLocal<Integer> threadIndex =
            new ThreadLocal<Integer>() {
                @Override
                protected Integer initialValue() {
                    return nextIndex.getAndIncrement();
                }
            };

    // Array of call sites instantiated, one per thread
    private static final CallSite[] instantiated = new CallSite[NUMBER_OF_THREADS];

    // Array of counters for how many times each instantiated call site is called
    private static final AtomicInteger[] called = new AtomicInteger[NUMBER_OF_THREADS];

    // Array of call site indicies of which call site a thread invoked
    private static final AtomicInteger[] targetted = new AtomicInteger[NUMBER_OF_THREADS];

    // Synchronization barrier all threads will wait on in the bootstrap method.
    private static final CyclicBarrier barrier = new CyclicBarrier(NUMBER_OF_THREADS);

    private TestInvokeCustomWithConcurrentThreads() {}

    private static int getThreadIndex() {
        return threadIndex.get().intValue();
    }

    public static int notUsed(int x) {
        return x;
    }

    public void run() {
        int x = setCalled(-1 /* argument dropped */);
        notUsed(x);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestInvokeCustomWithConcurrentThreads.class,
                    name = "linkerMethod",
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class}
                ),
        fieldOrMethodName = "setCalled",
        returnType = int.class,
        parameterTypes = {int.class}
    )
    private static int setCalled(int index) {
        called[index].getAndIncrement();
        targetted[getThreadIndex()].set(index);
        return 0;
    }

    @SuppressWarnings("unused")
    private static CallSite linkerMethod(
            MethodHandles.Lookup caller, String name, MethodType methodType) throws Throwable {
        MethodHandle mh =
                caller.findStatic(TestInvokeCustomWithConcurrentThreads.class, name, methodType);
        assertEquals(methodType, mh.type());
        assertEquals(mh.type().parameterCount(), 1);
        mh = MethodHandles.insertArguments(mh, 0, getThreadIndex());
        mh = MethodHandles.dropArguments(mh, 0, int.class);
        assertEquals(mh.type().parameterCount(), 1);
        assertEquals(methodType, mh.type());

        // Wait for all threads to be in this method.
        // Multiple call sites should be created, but only one
        // invoked.
        barrier.await();

        instantiated[getThreadIndex()] = new ConstantCallSite(mh);
        return instantiated[getThreadIndex()];
    }

    public static void test() throws Throwable {
        // Initialize counters for which call site gets invoked
        for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
            called[i] = new AtomicInteger(0);
            targetted[i] = new AtomicInteger(0);
        }

        // Run threads that each invoke-custom the call site
        Thread[] threads = new Thread[NUMBER_OF_THREADS];
        for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
            threads[i] = new Thread(new TestInvokeCustomWithConcurrentThreads());
            threads[i].start();
        }

        // Wait for all threads to complete
        for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
            threads[i].join();
        }

        // Check one call site instance won
        int winners = 0;
        int votes = 0;
        for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
            assertNotEquals(instantiated[i], null);
            if (called[i].get() != 0) {
                winners++;
                votes += called[i].get();
            }
        }

        System.out.println("Winners " + winners + " Votes " + votes);

        // We assert this below but output details when there's an error as
        // it's non-deterministic.
        if (winners != 1) {
            System.out.println("Threads did not the same call-sites:");
            for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
                System.out.format(
                        " Thread % 2d invoked call site instance #%02d\n", i, targetted[i].get());
            }
        }

        // We assert this below but output details when there's an error as
        // it's non-deterministic.
        if (votes != NUMBER_OF_THREADS) {
            System.out.println("Call-sites invocations :");
            for (int i = 0; i < NUMBER_OF_THREADS; ++i) {
                System.out.format(
                        " Call site instance #%02d was invoked % 2d times\n", i, called[i].get());
            }
        }

        assertEquals(winners, 1);
        assertEquals(votes, NUMBER_OF_THREADS);
    }
}
