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

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;

class DummyObject {
    public static boolean sHashCodeInvoked = false;
    private int i;

    public DummyObject(int i) {
        this.i = i;
    }

    public boolean equals(Object obj) {
        return (obj instanceof DummyObject) && (i == ((DummyObject)obj).i);
    }

    public int hashCode() {
        sHashCodeInvoked = true;
        Main.assertIsManaged();
        Main.deoptimizeAll();
        Main.assertIsInterpreted();
        Main.assertCallerIsManaged();  // Caller is from framework code HashMap.
        return i % 64;
    }
}

public class Main {
    static boolean sFlag = false;

    public static native void deoptimizeAll();
    public static native void undeoptimizeAll();
    public static native void assertIsInterpreted();
    public static native void assertIsManaged();
    public static native void assertCallerIsInterpreted();
    public static native void assertCallerIsManaged();
    public static native void disableStackFrameAsserts();
    public static native boolean hasOatFile();
    public static native boolean isInterpreted();

    public static void execute(Runnable runnable) throws Exception {
      Thread t = new Thread(runnable);
      t.start();
      t.join();
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        // Only test stack frames in compiled mode.
        if (!hasOatFile() || isInterpreted()) {
          disableStackFrameAsserts();
        }
        final HashMap<DummyObject, Long> map = new HashMap<DummyObject, Long>();

        // Single-frame deoptimization that covers partial fragment.
        execute(new Runnable() {
            public void run() {
                int[] arr = new int[3];
                assertIsManaged();
                int res = $noinline$run1(arr);
                assertIsManaged();  // Only single frame is deoptimized.
                if (res != 79) {
                    System.out.println("Failure 1!");
                    System.exit(0);
                }
            }
        });

        // Single-frame deoptimization that covers a full fragment.
        execute(new Runnable() {
            public void run() {
                try {
                    int[] arr = new int[3];
                    assertIsManaged();
                    // Use reflection to call $noinline$run2 so that it does
                    // full-fragment deoptimization since that is an upcall.
                    Class<?> cls = Class.forName("Main");
                    Method method = cls.getDeclaredMethod("$noinline$run2", int[].class);
                    double res = (double)method.invoke(Main.class, arr);
                    assertIsManaged();  // Only single frame is deoptimized.
                    if (res != 79.3d) {
                        System.out.println("Failure 2!");
                        System.exit(0);
                    }
                } catch (Exception e) {
                    e.printStackTrace(System.out);
                }
            }
        });

        // Full-fragment deoptimization.
        execute(new Runnable() {
            public void run() {
                assertIsManaged();
                float res = $noinline$run3B();
                assertIsInterpreted();  // Every deoptimizeable method is deoptimized.
                if (res != 0.034f) {
                    System.out.println("Failure 3!");
                    System.exit(0);
                }
            }
        });

        undeoptimizeAll();  // Make compiled code useable again.

        // Partial-fragment deoptimization.
        execute(new Runnable() {
            public void run() {
                try {
                    assertIsManaged();
                    map.put(new DummyObject(10), Long.valueOf(100));
                    assertIsInterpreted();  // Every deoptimizeable method is deoptimized.
                } catch (Exception e) {
                    e.printStackTrace(System.out);
                }
            }
        });

        undeoptimizeAll();  // Make compiled code useable again.

        if (!DummyObject.sHashCodeInvoked) {
            System.out.println("hashCode() method not invoked!");
        }
        if (map.get(new DummyObject(10)) != 100) {
            System.out.println("Wrong hashmap value!");
        }
        System.out.println("Finishing");
    }

    public static int $noinline$run1(int[] arr) {
        assertIsManaged();
        // Prevent inlining.
        if (sFlag) {
            throw new Error();
        }
        boolean caught = false;
        // BCE will use deoptimization for the code below.
        try {
            arr[0] = 1;
            arr[1] = 1;
            arr[2] = 1;
            // This causes AIOOBE and triggers deoptimization from compiled code.
            arr[3] = 1;
        } catch (ArrayIndexOutOfBoundsException e) {
            assertIsInterpreted(); // Single-frame deoptimization triggered.
            caught = true;
        }
        if (!caught) {
            System.out.println("Expected exception");
        }
        assertIsInterpreted();
        return 79;
    }

    public static double $noinline$run2(int[] arr) {
        assertIsManaged();
        // Prevent inlining.
        if (sFlag) {
            throw new Error();
        }
        boolean caught = false;
        // BCE will use deoptimization for the code below.
        try {
            arr[0] = 1;
            arr[1] = 1;
            arr[2] = 1;
            // This causes AIOOBE and triggers deoptimization from compiled code.
            arr[3] = 1;
        } catch (ArrayIndexOutOfBoundsException e) {
            assertIsInterpreted();  // Single-frame deoptimization triggered.
            caught = true;
        }
        if (!caught) {
            System.out.println("Expected exception");
        }
        assertIsInterpreted();
        return 79.3d;
    }

    public static float $noinline$run3A() {
        assertIsManaged();
        // Prevent inlining.
        if (sFlag) {
            throw new Error();
        }
        // Deoptimize callers.
        deoptimizeAll();
        assertIsInterpreted();
        assertCallerIsInterpreted();  // $noinline$run3B is deoptimizeable.
        return 0.034f;
    }

    public static float $noinline$run3B() {
        assertIsManaged();
        // Prevent inlining.
        if (sFlag) {
            throw new Error();
        }
        float res = $noinline$run3A();
        assertIsInterpreted();
        return res;
    }
}
