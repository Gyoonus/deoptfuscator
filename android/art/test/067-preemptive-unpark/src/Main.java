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

import sun.misc.Unsafe;

import java.lang.reflect.Field;

public class Main {
    private static Unsafe UNSAFE;

    public static void main(String[] args) throws Exception {
        setUp();

        ParkTester test = new ParkTester();

        System.out.println("Test starting");

        test.start();
        UNSAFE.unpark(test);
        clearStack(10);

        System.out.println("GC'ing");
        System.gc();
        System.runFinalization();
        System.gc();

        System.out.println("Asking thread to park");
        test.parkNow = true;

        try {
            // Give some time to the ParkTester thread to honor the park command.
            Thread.sleep(3000);
        } catch (InterruptedException ex) {
            System.out.println("Main thread interrupted!");
            System.exit(1);
        }

        if (test.success) {
            System.out.println("Test succeeded!");
        } else {
            System.out.println("Test failed.");
        }
    }

    /**
     * Set up {@link #UNSAFE}.
     */
    public static void setUp() throws Exception{
        /*
         * Subvert the access check to get the unique Unsafe instance.
         * We can do this because there's no security manager
         * installed when running the test.
         */
        Field field = null;
        try {
            field = Unsafe.class.getDeclaredField("THE_ONE");
        } catch (NoSuchFieldException e1) {
            try {
                field = Unsafe.class.getDeclaredField("theUnsafe");
            } catch (NoSuchFieldException e2) {
                throw new RuntimeException("Failed to find THE_ONE or theUnsafe");
            }
        }
        field.setAccessible(true);
        UNSAFE = (Unsafe) field.get(null);
    }

    /**
     * Scribbles on the stack to help ensure we don't have a fake
     * pointer that would keep would-be garbage alive.
     */
    private static void clearStack(int depth) {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        int g = 0;
        int h = 0;
        int i = 0;
        int j = 0;

        if (depth > 0) {
            clearStack(depth - 1);
        }
    }

    private static class ParkTester extends Thread {
        public volatile boolean parkNow = false;
        public volatile boolean success = false;

        public void run() {
            while (!parkNow) {
                try {
                    Thread.sleep(500);
                } catch (InterruptedException ex) {
                    // Ignore it.
                }
            }

            long start = System.currentTimeMillis();
            UNSAFE.park(false, 500 * 1000000); // 500 msec
            long elapsed = System.currentTimeMillis() - start;

            if (elapsed > 200) {
                System.out.println("park()ed for " + elapsed + " msec");
                success = false;
            } else {
                System.out.println("park() returned quickly");
                success = true;
            }
        }
    }
}
