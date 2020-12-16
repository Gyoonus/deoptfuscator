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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class Main {
    static final int numberOfThreads = 5;
    static final int totalOperations = 10000;

    final static Object lockObject = new Object();
    static SimpleInterface inf;
    static volatile boolean finish = false;

    public static void main(String[] args) throws Exception {
        inf = (SimpleInterface)Proxy.newProxyInstance(SimpleInterface.class.getClassLoader(),
            new Class[] { SimpleInterface.class }, new EmptyInvocationHandler());

        Thread garbageThread = new Thread(new GarbageRunner());
        garbageThread.start();

        final Thread[] threads = new Thread[numberOfThreads];
        for (int t = 0; t < threads.length; t++) {
            threads[t] = new Thread((t % 2 == 0) ? new ProxyRunner() : new SyncRunner());
        }
        for (Thread t : threads) {
            t.start();
        }

        // Now wait.
        for (Thread t : threads) {
            t.join();
        }
        finish = true;
        garbageThread.join();
    }

    private static interface SimpleInterface {
        // Add some primitives to force some allocation when calling.
        public void foo(int i1, int i2, int i3, int i4, int i5, int i6);
    }

    private static class EmptyInvocationHandler implements InvocationHandler {
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            return null;
        }
    }

    private static class ProxyRunner implements Runnable {
        public void run() {
            int count = totalOperations;
            while (count > 0) {
                synchronized (lockObject) {
                    try {
                        inf.foo(10000 - count, 11000 - count, 12000 - count, 13000 - count,
                                14000 - count, 15000 - count);
                    } catch (OutOfMemoryError e) {
                        // Ignore errors. This is the test for b/69121347 - see an exception
                        // instead of native abort.
                    }
              }
              count--;
            }
        }
    }

    private static class SyncRunner implements Runnable {
        public void run() {
            int count = totalOperations;
            while (count > 0) {
                synchronized (lockObject) {
                    // "Wait" a small amount of time.
                    long start = System.nanoTime();
                    long delta = 10 * 1000;  // 10 us.
                    long elapsed;
                    do {
                      elapsed = System.nanoTime();
                    } while (elapsed - start < delta);
                }
                count--;
            }
        }
    }

    private static class GarbageRunner implements Runnable {
        public void run() {
            while (!finish) {
                // Some random allocations adding up to almost 2M.
                for (int i = 0; i < 188; i++) {
                    try {
                        byte b[] = new byte[i * 100 + 10];
                    } catch (OutOfMemoryError e) {
                        // Ignore. This is just to improve chances that an OOME is thrown during
                        // proxy invocation.
                    }
                }
                try {
                    Thread.sleep(10);
                } catch (Exception e) {
                }
            }
        }
    }
}
