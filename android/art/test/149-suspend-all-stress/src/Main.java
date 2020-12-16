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

import java.util.Map;

public class Main implements Runnable {
    static final int numberOfThreads = 8;

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        final Thread[] threads = new Thread[numberOfThreads];
        for (int t = 0; t < threads.length; t++) {
            threads[t] = new Thread(new Main());
            threads[t].start();
        }
        for (Thread t : threads) {
            t.join();
        }
        System.out.println("Finishing");
    }

    public void run() {
        suspendAndResume();
    }

    private static native void suspendAndResume();
}
