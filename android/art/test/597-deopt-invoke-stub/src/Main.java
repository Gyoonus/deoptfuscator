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

public class Main implements Runnable {
    static final int numberOfThreads = 2;
    static boolean sExitFlag = false;
    static boolean sEntered = false;
    int threadIndex;

    private static native void deoptimizeAll();
    private static native void assertIsInterpreted();
    private static native void assertIsManaged();
    private static native void ensureJitCompiled(Class<?> cls, String methodName);

    Main(int index) {
        threadIndex = index;
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);

        final Thread[] threads = new Thread[numberOfThreads];
        for (int t = 0; t < threads.length; t++) {
            threads[t] = new Thread(new Main(t));
            threads[t].start();
        }
        for (Thread t : threads) {
            t.join();
        }
        System.out.println("Finishing");
    }

    private static int $noinline$bar() {
        // Should be entered via interpreter bridge.
        assertIsInterpreted();
        synchronized (Main.class) {
            sEntered = true;
            Main.class.notify();
            while (!sExitFlag) {
                try {
                    Main.class.wait();
                } catch (InterruptedException e) {
                    throw new Error("Unexpected exception.");
                }
            }
        }
        assertIsInterpreted();
        return 0x1234;
    }

    public void $noinline$foo() {
        assertIsManaged();
        if ($noinline$bar() != 0x1234) {
            System.out.println("Bad return value");
        }
        assertIsInterpreted();
    }

    public void run() {
        if (threadIndex == 0) {
            synchronized (Main.class) {
                while (!sEntered) {
                    try {
                        Main.class.wait();
                    } catch (InterruptedException e) {
                        throw new Error("Unexpected exception.");
                    }
                }
            }
            deoptimizeAll();
            synchronized (Main.class) {
                sExitFlag = true;
                Main.class.notify();
            }
        } else {
            ensureJitCompiled(Main.class, "$noinline$foo");
            $noinline$foo();
        }
    }
}
