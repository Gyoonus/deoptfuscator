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

public class Main implements Runnable {
    static final int numberOfThreads = 2;
    static final int totalOperations = 40000;
    static boolean sFlag = false;
    static volatile boolean done = false;
    int threadIndex;

    public static native void deoptimizeAll();
    public static native void undeoptimizeAll();

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

    public String $noinline$run0() {
        // Prevent inlining.
        if (sFlag) {
            throw new Error();
        }
        char[] arr = {'a', 'b', 'c'};
        String str = new String(arr, 0, arr.length);
        if (!str.equals("abc")) {
            System.out.println("Failure 1! " + str);
            System.exit(0);
        }
        return str;
    }

    public void run() {
        if (threadIndex == 0) {
            // This thread keeps doing deoptimization of all threads.
            // Hopefully that will trigger one deoptimization when returning from
            // StringFactory.newEmptyString() in one of the other threads.
            for (int i = 0; i < totalOperations; ++i) {
                if (i % 50 == 0) {
                    deoptimizeAll();
                }
                if (i % 50 == 25) {
                    undeoptimizeAll();
                }
            }
            done = true;
        } else {
            // This thread keeps doing new String() from a char array.
            while (!done) {
                String str = $noinline$run0();
                if (!str.equals("abc")) {
                    System.out.println("Failure 2! " + str);
                    System.exit(0);
                }
            }
        }
    }
}
