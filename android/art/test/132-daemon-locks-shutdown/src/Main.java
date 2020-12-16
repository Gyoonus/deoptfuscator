/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * Test that daemon threads still contending for a lock don't make the runtime abort on shutdown.
 */
public class Main {

    public final static int THREAD_COUNT = 32;

    public static void main(String[] args) throws Exception {
        Object sync = new Object();

        for (int i = 0; i < THREAD_COUNT; i++) {
            Thread t = new Thread(new Wait(sync));
            t.setDaemon(true);
            t.start();
        }
    }

    private static class Wait implements Runnable {
        private Object obj;

        public Wait(Object obj) {
            this.obj = obj;
        }

        public void run() {
            for (;;) {
                synchronized(obj) {
                    try {
                        obj.wait(1);
                    } catch (Exception exc) {
                        exc.printStackTrace(System.out);
                    }
                }
            }
        }
    }
}
