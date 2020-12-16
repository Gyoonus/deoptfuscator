/*
 * Copyright (C) 2010 The Android Open Source Project
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/*
 * Entry point and tests that are expected to succeed.
 */
public class Main {
    /**
     * Drives tests.
     */
    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        if (!hasOatFile() || runtimeIsSoftFail() || isInterpreted()) {
            // Some tests ensure that the verifier was able to guarantee balanced locking by
            // asserting that the test function is running as compiled code. But skip this now,
            // as this seems to be a non-compiled code test configuration.
            disableStackFrameAsserts();
        }

        ensureJitCompiled(Main.class, "recursiveSync");
        ensureJitCompiled(Main.class, "nestedMayThrow");
        ensureJitCompiled(Main.class, "constantLock");
        ensureJitCompiled(Main.class, "notExcessiveNesting");
        ensureJitCompiled(Main.class, "notNested");
        ensureJitCompiled(TwoPath.class, "twoPath");
        ensureJitCompiled(Class.forName("OK"), "runNoMonitors");
        ensureJitCompiled(Class.forName("OK"), "runStraightLine");
        ensureJitCompiled(Class.forName("OK"), "runBalancedJoin");
        ensureJitCompiled(Class.forName("NullLocks"), "run");

        Main m = new Main();

        m.recursiveSync(0);

        m.nestedMayThrow(false);
        try {
            m.nestedMayThrow(true);
            System.out.println("nestedThrow(true) did not throw");
        } catch (MyException me) {}
        System.out.println("nestedMayThrow ok");

        m.constantLock();
        System.out.println("constantLock ok");

        m.notExcessiveNesting();

        m.notNested();
        System.out.println("notNested ok");

        Object obj1 = new Object();
        Object obj2 = new Object();

        TwoPath.twoPath(obj1, obj2, 0);
        System.out.println("twoPath ok");

        m.triplet(obj1, obj2, 0);
        System.out.println("triplet ok");

        runSmaliTests();
    }

    /**
     * Recursive synchronized method.
     */
    synchronized void recursiveSync(int iter) {
        assertIsManaged();
        if (iter < 40) {
            recursiveSync(iter+1);
        } else {
            System.out.println("recursiveSync ok");
        }
    }

    /**
     * Tests simple nesting, with and without a throw.
     */
    void nestedMayThrow(boolean doThrow) {
        assertIsManaged();
        synchronized (this) {
            synchronized (Main.class) {
                synchronized (new Object()) {
                    synchronized(Class.class) {
                        if (doThrow) {
                            throw new MyException();
                        }
                    }
                }
            }
        }
    }

    /**
     * Exercises bug 3215458.
     */
    void constantLock() {
        assertIsManaged();
        Class<?> thing = Thread.class;
        synchronized (Thread.class) {}
    }

    /**
     * Confirms that we can have 32 nested monitors on one method.
     */
    void notExcessiveNesting() {
        assertIsManaged();
        synchronized (this) {   // 1
        synchronized (this) {   // 2
        synchronized (this) {   // 3
        synchronized (this) {   // 4
        synchronized (this) {   // 5
        synchronized (this) {   // 6
        synchronized (this) {   // 7
        synchronized (this) {   // 8
        synchronized (this) {   // 9
        synchronized (this) {   // 10
        synchronized (this) {   // 11
        synchronized (this) {   // 12
        synchronized (this) {   // 13
        synchronized (this) {   // 14
        synchronized (this) {   // 15
        synchronized (this) {   // 16
        synchronized (this) {   // 17
        synchronized (this) {   // 18
        synchronized (this) {   // 19
        synchronized (this) {   // 20
        synchronized (this) {   // 21
        synchronized (this) {   // 22
        synchronized (this) {   // 23
        synchronized (this) {   // 24
        synchronized (this) {   // 25
        synchronized (this) {   // 26
        synchronized (this) {   // 27
        synchronized (this) {   // 28
        synchronized (this) {   // 29
        synchronized (this) {   // 30
        synchronized (this) {   // 31
        synchronized (this) {   // 32
        }}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
    }

    /**
     * Confirms that we can have more than 32 non-nested monitors in one
     * method.
     */
    void notNested() {
        assertIsManaged();
        synchronized (this) {}  // 1
        synchronized (this) {}  // 2
        synchronized (this) {}  // 3
        synchronized (this) {}  // 4
        synchronized (this) {}  // 5
        synchronized (this) {}  // 6
        synchronized (this) {}  // 7
        synchronized (this) {}  // 8
        synchronized (this) {}  // 9
        synchronized (this) {}  // 10
        synchronized (this) {}  // 11
        synchronized (this) {}  // 12
        synchronized (this) {}  // 13
        synchronized (this) {}  // 14
        synchronized (this) {}  // 15
        synchronized (this) {}  // 16
        synchronized (this) {}  // 17
        synchronized (this) {}  // 18
        synchronized (this) {}  // 19
        synchronized (this) {}  // 20
        synchronized (this) {}  // 21
        synchronized (this) {}  // 22
        synchronized (this) {}  // 23
        synchronized (this) {}  // 24
        synchronized (this) {}  // 25
        synchronized (this) {}  // 26
        synchronized (this) {}  // 27
        synchronized (this) {}  // 28
        synchronized (this) {}  // 29
        synchronized (this) {}  // 30
        synchronized (this) {}  // 31
        synchronized (this) {}  // 32
        synchronized (this) {}  // 33
        synchronized (this) {}  // 34
    }

    /* does nothing but ensure that the compiler doesn't discard an object */
    private void doNothing(Object obj) {}

    /**
     * Lock the monitor two or three times, and make use of the locked or
     * unlocked object.
     */
    public void triplet(Object obj1, Object obj2, int x) {
        Object localObj;

        synchronized (obj1) {
            synchronized(obj1) {
                if (x == 0) {
                    synchronized(obj1) {
                        localObj = obj2;
                    }
                } else {
                    localObj = obj1;
                }
            }
        }

        doNothing(localObj);
    }

    // Smali testing code.
    private static void runSmaliTests() {
        runTest("OK", new Object[] { new Object(), new Object() }, null);
        runTest("TooDeep", new Object[] { new Object() }, null);
        runTest("NotStructuredOverUnlock", new Object[] { new Object() },
                IllegalMonitorStateException.class);
        runTest("NotStructuredUnderUnlock", new Object[] { new Object() },
                IllegalMonitorStateException.class);
        runTest("UnbalancedJoin", new Object[] { new Object(), new Object() }, null);
        runTest("UnbalancedStraight", new Object[] { new Object(), new Object() }, null);
        runTest("NullLocks", new Object[] { false }, null);
        runTest("NullLocks", new Object[] { true }, NullPointerException.class);
    }

    private static void runTest(String className, Object[] parameters, Class<?> excType) {
        try {
            Class<?> c = Class.forName(className);

            Method[] methods = c.getDeclaredMethods();

            // For simplicity we assume that test methods are not overloaded. So searching by name
            // will give us the method we need to run.
            Method method = null;
            for (Method m : methods) {
                if (m.getName().equals("run")) {
                    method = m;
                    break;
                }
            }

            if (method == null) {
                System.out.println("Could not find test method for " + className);
            } else if (!Modifier.isStatic(method.getModifiers())) {
                System.out.println("Test method for " + className + " is not static.");
            } else {
                method.invoke(null, parameters);
                if (excType != null) {
                    System.out.println("Expected an exception in " + className);
                }
            }
        } catch (Throwable exc) {
            if (excType == null) {
                System.out.println("Did not expect exception " + exc + " for " + className);
                exc.printStackTrace(System.out);
            } else if (exc instanceof InvocationTargetException && exc.getCause() != null &&
                       exc.getCause().getClass().equals(excType)) {
                // Expected exception is wrapped in InvocationTargetException.
            } else if (!excType.equals(exc.getClass())) {
                System.out.println("Expected " + excType.getName() + ", but got " + exc.getClass());
            } else {
              // Expected exception, do nothing.
            }
        }
    }

    // Helpers for the smali code.
    public static native void assertIsInterpreted();
    public static native void assertIsManaged();
    public static native boolean hasOatFile();
    public static native boolean runtimeIsSoftFail();
    public static native boolean isInterpreted();
    public static native void disableStackFrameAsserts();
    private static native void ensureJitCompiled(Class<?> itf, String method_name);
}
