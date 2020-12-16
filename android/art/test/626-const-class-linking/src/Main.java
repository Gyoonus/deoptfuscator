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
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;

public class Main {
    public static void main(String[] args) throws Exception {
        try {
            // Check if we're running dalvik or RI.
            Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
            System.loadLibrary(args[0]);
        } catch (ClassNotFoundException e) {
            usingRI = true;
            // Add expected JNI_OnLoad log line to match expected.txt.
            System.out.println("JNI_OnLoad called");
        }

        testClearDexCache();
        testMultiDex();
        testRacyLoader();
        testRacyLoader2();
        testMisbehavingLoader();
        testRacyMisbehavingLoader();
        testRacyMisbehavingLoader2();
    }

    private static void testClearDexCache() throws Exception {
        DelegatingLoader delegating_loader = createDelegatingLoader();
        Class<?> helper = delegating_loader.loadClass("Helper1");

        WeakReference<Class<?>> weak_test1 = wrapHelperGet(helper);
        changeInner(delegating_loader);
        clearResolvedTypes(helper);
        Runtime.getRuntime().gc();
        WeakReference<Class<?>> weak_test2 = wrapHelperGet(helper);
        Runtime.getRuntime().gc();

        Class<?> test1 = weak_test1.get();
        if (test1 == null) {
            System.out.println("test1 disappeared");
        }
        Class<?> test2 = weak_test2.get();
        if (test2 == null) {
            System.out.println("test2 disappeared");
        }
        if (test1 != test2) {
            System.out.println("test1 != test2");
        }

        System.out.println("testClearDexCache done");
    }

    private static void testMultiDex() throws Exception {
        DelegatingLoader delegating_loader = createDelegatingLoader();

        Class<?> helper1 = delegating_loader.loadClass("Helper1");
        WeakReference<Class<?>> weak_test1 = wrapHelperGet(helper1);

        changeInner(delegating_loader);

        Class<?> helper2 = delegating_loader.loadClass("Helper2");
        WeakReference<Class<?>> weak_test2 = wrapHelperGet(helper2);

        Runtime.getRuntime().gc();

        Class<?> test1 = weak_test1.get();
        if (test1 == null) {
            System.out.println("test1 disappeared");
        }
        Class<?> test2 = weak_test2.get();
        if (test2 == null) {
            System.out.println("test2 disappeared");
        }
        if (test1 != test2) {
            System.out.println("test1 != test2");
        }

        System.out.println("testMultiDex done");
    }

    private static void testMisbehavingLoader() throws Exception {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        MisbehavingLoader misbehaving_loader =
            new MisbehavingLoader(system_loader, defining_loader);
        Class<?> helper = misbehaving_loader.loadClass("Helper1");

        try {
            WeakReference<Class<?>> weak_test = wrapHelperGet(helper);
        } catch (InvocationTargetException ite) {
            String message = ite.getCause().getMessage();
            if (usingRI && "Test".equals(message)) {
              // Replace RI message with dalvik message to match expected.txt.
              message = "Initiating class loader of type " +
                  misbehaving_loader.getClass().getName() +
                  " returned class Helper2 instead of Test.";
            }
            System.out.println(ite.getCause().getClass().getName() + ": " + message);
        }
        System.out.println("testMisbehavingLoader done");
    }

    private static void testRacyLoader() throws Exception {
        final ClassLoader system_loader = ClassLoader.getSystemClassLoader();

        final Thread[] threads = new Thread[4];
        final Object[] results = new Object[threads.length];

        final RacyLoader racy_loader = new RacyLoader(system_loader, threads.length);
        final Class<?> helper1 = racy_loader.loadClass("Helper1");
        skipVerification(helper1);  // Avoid class loading during verification.

        for (int i = 0; i != threads.length; ++i) {
            final int my_index = i;
            Thread t = new Thread() {
                public void run() {
                    try {
                        Method get = helper1.getDeclaredMethod("get");
                        results[my_index] = get.invoke(null);
                    } catch (InvocationTargetException ite) {
                        results[my_index] = ite.getCause();
                    } catch (Throwable t) {
                        results[my_index] = t;
                    }
                }
            };
            t.start();
            threads[i] = t;
        }
        for (Thread t : threads) {
            t.join();
        }
        dumpResultStats(results, 1);
        System.out.println("testRacyLoader done");
    }

    private static void testRacyLoader2() throws Exception {
        final ClassLoader system_loader = ClassLoader.getSystemClassLoader();

        final Thread[] threads = new Thread[4];
        final Object[] results = new Object[threads.length];

        final RacyLoader racy_loader = new RacyLoader(system_loader, threads.length);
        final Class<?> helper1 = racy_loader.loadClass("Helper1");
        skipVerification(helper1);  // Avoid class loading during verification.
        final Class<?> helper3 = racy_loader.loadClass("Helper3");
        skipVerification(helper3);  // Avoid class loading during verification.

        for (int i = 0; i != threads.length; ++i) {
            final int my_index = i;
            Thread t = new Thread() {
                public void run() {
                    try {
                        Class<?> helper = (my_index < threads.length / 2) ? helper1 : helper3;
                        Method get = helper.getDeclaredMethod("get");
                        results[my_index] = get.invoke(null);
                    } catch (InvocationTargetException ite) {
                        results[my_index] = ite.getCause();
                    } catch (Throwable t) {
                        results[my_index] = t;
                    }
                }
            };
            t.start();
            threads[i] = t;
        }
        for (Thread t : threads) {
            t.join();
        }
        dumpResultStats(results, 2);
        System.out.println("testRacyLoader2 done");
    }

    private static void testRacyMisbehavingLoader() throws Exception {
        final ClassLoader system_loader = ClassLoader.getSystemClassLoader();

        final Thread[] threads = new Thread[4];
        final Object[] results = new Object[threads.length];

        final RacyMisbehavingLoader racy_loader =
            new RacyMisbehavingLoader(system_loader, threads.length, false);
        final Class<?> helper1 = racy_loader.loadClass("RacyMisbehavingHelper");
        skipVerification(helper1);  // Avoid class loading during verification.

        for (int i = 0; i != threads.length; ++i) {
            final int my_index = i;
            Thread t = new Thread() {
                public void run() {
                    try {
                        Method get = helper1.getDeclaredMethod("get");
                        results[my_index] = get.invoke(null);
                    } catch (InvocationTargetException ite) {
                        results[my_index] = ite.getCause();
                    } catch (Throwable t) {
                        results[my_index] = t;
                    }
                }
            };
            t.start();
            threads[i] = t;
        }
        for (Thread t : threads) {
            t.join();
        }
        dumpResultStats(results, 1);
        System.out.println("testRacyMisbehavingLoader done");
    }

    private static void testRacyMisbehavingLoader2() throws Exception {
        final ClassLoader system_loader = ClassLoader.getSystemClassLoader();

        final Thread[] threads = new Thread[4];
        final Object[] results = new Object[threads.length];

        final RacyMisbehavingLoader racy_loader =
            new RacyMisbehavingLoader(system_loader, threads.length, true);
        final Class<?> helper1 = racy_loader.loadClass("RacyMisbehavingHelper");
        skipVerification(helper1);  // Avoid class loading during verification.

        for (int i = 0; i != threads.length; ++i) {
            final int my_index = i;
            Thread t = new Thread() {
                public void run() {
                    try {
                        Method get = helper1.getDeclaredMethod("get");
                        results[my_index] = get.invoke(null);
                    } catch (InvocationTargetException ite) {
                        results[my_index] = ite.getCause();
                    } catch (Throwable t) {
                        results[my_index] = t;
                    }
                }
            };
            t.start();
            threads[i] = t;
        }
        for (Thread t : threads) {
            t.join();
        }
        dumpResultStats(results, 1);
        System.out.println("testRacyMisbehavingLoader2 done");
    }

    private static void dumpResultStats(Object[] results, int expected_unique) throws Exception {
        int throwables = 0;
        int classes = 0;
        int unique_classes = 0;
        for (int i = 0; i != results.length; ++i) {
            Object r = results[i];
            if (r instanceof Throwable) {
                ++throwables;
                System.out.println(((Throwable) r).getMessage());
            } else if (isClassPair(r)) {
                printPair(r);
                Object ref = getSecond(r);
                ++classes;
                ++unique_classes;
                for (int j = 0; j != i; ++j) {
                    Object rj = results[j];
                    if (isClassPair(results[j]) && getSecond(results[j]) == ref) {
                        --unique_classes;
                        break;
                    }
                }
            }
        }
        System.out.println("total: " + results.length);
        System.out.println("  throwables: " + throwables);
        System.out.println("  classes: " + classes
            + " (" + unique_classes + " unique)");
        if (expected_unique != unique_classes) {
            System.out.println("MISMATCH with expected_unique: " + expected_unique);
            ArrayList<Class<?>> list = new ArrayList<Class<?>>();
            for (int i = 0; i != results.length; ++i) {
                Object r = results[i];
                if (isClassPair(r)) {
                    list.add(getSecond(r));
                }
            }
            nativeDumpClasses(list.toArray());
        }
    }

    private static DelegatingLoader createDelegatingLoader() {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        return new DelegatingLoader(system_loader, defining_loader);
    }

    private static void changeInner(DelegatingLoader delegating_loader) {
        ClassLoader system_loader = ClassLoader.getSystemClassLoader();
        DefiningLoader defining_loader = new DefiningLoader(system_loader);
        delegating_loader.resetDefiningLoader(defining_loader);
    }

    private static WeakReference<Class<?>> wrapHelperGet(Class<?> helper) throws Exception {
        Method get = helper.getDeclaredMethod("get");
        Object pair = get.invoke(null);
        printPair(pair);
        return new WeakReference<Class<?>>(getSecond(pair));
    }

    private static void printPair(Object pair) throws Exception {
        Method print = pair.getClass().getDeclaredMethod("print");
        print.invoke(pair);
    }

    private static Class<?> getSecond(Object pair) throws Exception {
        Field second = pair.getClass().getDeclaredField("second");
        return (Class<?>) second.get(pair);
    }

    private static boolean isClassPair(Object r) {
        return r != null && r.getClass().getName().equals("ClassPair");
    }

    public static void clearResolvedTypes(Class<?> c) {
        if (!usingRI) {
            nativeClearResolvedTypes(c);
        }
    }

    // Skip verification of a class on ART. Verification can cause classes to be loaded
    // while holding a lock on the class being verified and holding that lock can interfere
    // with the intent of the "racy" tests. In these tests we're waiting in the loadClass()
    // for all the tested threads to synchronize and they cannot reach that point if they
    // are waiting for the class lock on ClassLinker::InitializeClass(Helper1/Helper3).
    public static void skipVerification(Class<?> c) {
        if (!usingRI) {
            nativeSkipVerification(c);
        }
    }

    public static native void nativeClearResolvedTypes(Class<?> c);
    public static native void nativeSkipVerification(Class<?> c);
    public static native void nativeDumpClasses(Object[] array);

    static boolean usingRI = false;
}
