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

import java.lang.ref.WeakReference;

public class Main {
    static final int numWeakReferences = 16 * 1024;
    static WeakReference[] weakReferences = new WeakReference[numWeakReferences];
    static volatile boolean done = false;
    static Object keepAlive;

    public static void main(String[] args) throws Exception {
        // Try to call Reference.get repeatedly while the GC is running.
        Thread gcThread = new GcThread();
        Thread[] readerThread = new ReaderThread[4];
        for (int i = 0; i < readerThread.length; ++i) {
            readerThread[i] = new ReaderThread();
        }
        gcThread.start();
        for (int i = 0; i < readerThread.length; ++i) {
            readerThread[i].start();
        }
        gcThread.join();
        for (int i = 0; i < readerThread.length; ++i) {
            readerThread[i].join();
        }
        System.out.println("PASS");
    }

    static class GcThread extends Thread {
        GcThread() {
            Object temp = new Object();
            for (int j = 0; j < weakReferences.length; ++j) {
                weakReferences[j] = new WeakReference(temp);
            }
        }
        public void run() {
            for (int i = 0; i < 1000; ++i) {
                Object o = new Object();
                for (int j = 0; j < weakReferences.length; ++j) {
                    weakReferences[j] = new WeakReference(o);
                }
            }
            done = true;
        }
    }

    static class ReaderThread extends Thread {
        public void run() {
            while (!done) {
                for (int j = 0; j < weakReferences.length; ++j) {
                    keepAlive = weakReferences[j].get();
                }
                for (int j = 0; j < weakReferences.length; ++j) {
                    weakReferences[j].clear();
                }
            }
        }
    }
}
