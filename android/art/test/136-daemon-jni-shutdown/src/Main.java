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

/**
 * Test that daemon threads that call into a JNI env after the runtime is shutdown do not crash.
 */
public class Main {

    public final static int THREAD_COUNT = 4;

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);

        for (int i = 0; i < THREAD_COUNT; i++) {
            Thread t = new Thread(new DaemonRunnable());
            t.setDaemon(true);
            t.start();
        }
        // Give threads time to start and become stuck in waitAndCallIntoJniEnv.
        Thread.sleep(1000);
        destroyJavaVMAndExit();
    }

    static native void waitAndCallIntoJniEnv();
    static native void destroyJavaVMAndExit();

    private static class DaemonRunnable implements Runnable {
        public void run() {
            for (;;) {
                waitAndCallIntoJniEnv();
            }
        }
    }
}
