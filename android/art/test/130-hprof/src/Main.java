/*
 * Copyright (C) 2009 The Android Open Source Project
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

import java.io.File;
import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class Main {
    private static final int TEST_LENGTH = 100;

    private static boolean makeArray(int i) {
        return i % 10 == 0;
    }

    private static void fillArray(Object global[], Object local[], int i) {
        // Very stupid linking.
        local[0] = global;
        for (int j = 1; j < local.length; j++) {
            local[j] = global[j];
        }
    }

    private static Object allocInDifferentLoader() throws Exception {
        final String DEX_FILE = System.getenv("DEX_LOCATION") + "/130-hprof-ex.jar";
        Class<?> pathClassLoader = Class.forName("dalvik.system.PathClassLoader");
        if (pathClassLoader == null) {
            throw new AssertionError("Couldn't find path class loader class");
        }
        Constructor<?> constructor =
            pathClassLoader.getDeclaredConstructor(String.class, ClassLoader.class);
        ClassLoader loader = (ClassLoader)constructor.newInstance(
                DEX_FILE, ClassLoader.getSystemClassLoader());
        Class<?> allocator = loader.loadClass("Allocator");
        return allocator.getDeclaredMethod("allocObject", null).invoke(null);
    }

    private static void createDumpAndConv() throws RuntimeException {
        File dumpFile = null;
        File convFile = null;

        try {
            // Now dump the heap.
            dumpFile = createDump();

            // Run hprof-conv on it.
            convFile = getConvFile();

            File hprof_conv = getHprofConf();
            try {
                ProcessBuilder pb = new ProcessBuilder(
                        hprof_conv.getAbsoluteFile().toString(),
                        dumpFile.getAbsoluteFile().toString(),
                        convFile.getAbsoluteFile().toString());
                pb.redirectErrorStream(true);
                Process process = pb.start();
                int ret = process.waitFor();
                if (ret != 0) {
                    throw new RuntimeException("Exited abnormally with " + ret);
                }
            } catch (Exception exc) {
                throw new RuntimeException(exc);
            }
        } finally {
            // Delete the files.
            if (dumpFile != null) {
                dumpFile.delete();
            }
            if (convFile != null) {
                convFile.delete();
            }
        }
    }

    public static void main(String[] args) throws Exception {
        testBasicDump();
        testAllocationTrackingAndClassUnloading();
        testGcAndDump();
    }

    private static void testBasicDump() throws Exception {
        // Create some data.
        Object data[] = new Object[TEST_LENGTH];
        for (int i = 0; i < data.length; i++) {
            if (makeArray(i)) {
                data[i] = new Object[TEST_LENGTH];
            } else {
                data[i] = String.valueOf(i);
            }
        }
        for (int i = 0; i < data.length; i++) {
            if (makeArray(i)) {
                Object data2[] = (Object[]) data[i];
                fillArray(data, data2, i);
            }
        }
        System.out.println("Generated data.");
        createDumpAndConv();
    }

    private static void testAllocationTrackingAndClassUnloading() throws Exception {
        Class<?> klass = Class.forName("org.apache.harmony.dalvik.ddmc.DdmVmInternal");
        if (klass == null) {
            throw new AssertionError("Couldn't find path class loader class");
        }
        Method enableMethod = klass.getDeclaredMethod("enableRecentAllocations",
                Boolean.TYPE);
        if (enableMethod == null) {
            throw new AssertionError("Couldn't find path class loader class");
        }
        enableMethod.invoke(null, true);
        Object o = allocInDifferentLoader();
        // Run GC to cause class unloading.
        Runtime.getRuntime().gc();
        createDumpAndConv();
        // TODO: Somehow check contents of hprof file.
        enableMethod.invoke(null, false);
    }

    private static void testGcAndDump() throws Exception {
        Allocator allocator = new Allocator();
        Dumper dumper = new Dumper(allocator);
        allocator.start();
        dumper.start();
        try {
            allocator.join();
            dumper.join();
        } catch (InterruptedException e) {
            System.out.println("join interrupted");
        }
    }

    private static class Allocator extends Thread {
        private static int ARRAY_SIZE = 1024;
        public volatile boolean running = true;
        public void run() {
            Object[] array = new Object[ARRAY_SIZE];
            int i = 0;
            while (running) {
                array[i] = new byte[1024];
                if (i % ARRAY_SIZE == 0) {
                    Main.sleep(100L);
                }
                i = (i + 1) % ARRAY_SIZE;
            }
        }
    }

    private static class Dumper extends Thread {
        Dumper(Allocator allocator) {
            this.allocator = allocator;
        }
        Allocator allocator;
        public void run() {
            for (int i = 0; i < 5; ++i) {
                Main.sleep(1000L);
                createDumpAndConv();
            }
            allocator.running = false;
        }
    }

    public static void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            System.out.println("sleep interrupted");
        }
    }

    private static File getHprofConf() {
        // Use the java.library.path. It points to the lib directory.
        File libDir = new File(System.getProperty("java.library.path").split(":")[0]);
        return new File(new File(libDir.getParentFile(), "bin"), "hprof-conv");
    }

    private static File createDump() {
        java.lang.reflect.Method dumpHprofDataMethod = getDumpHprofDataMethod();
        if (dumpHprofDataMethod != null) {
            File f = getDumpFile();
            try {
                dumpHprofDataMethod.invoke(null, f.getAbsoluteFile().toString());
                return f;
            } catch (Exception exc) {
                exc.printStackTrace(System.out);
            }
        } else {
            System.out.println("Could not find dump method!");
        }
        return null;
    }

    /**
     * Finds VMDebug.dumpHprofData() through reflection.  In the reference
     * implementation this will not be available.
     *
     * @return the reflection object, or null if the method can't be found
     */
    private static Method getDumpHprofDataMethod() {
        ClassLoader myLoader = Main.class.getClassLoader();
        Class<?> vmdClass;
        try {
            vmdClass = myLoader.loadClass("dalvik.system.VMDebug");
        } catch (ClassNotFoundException cnfe) {
            return null;
        }

        Method meth;
        try {
            meth = vmdClass.getMethod("dumpHprofData", String.class);
        } catch (NoSuchMethodException nsme) {
            System.out.println("Found VMDebug but not dumpHprofData method");
            return null;
        }

        return meth;
    }

    private static File getDumpFile() {
        try {
            return File.createTempFile("test-130-hprof", "dump");
        } catch (Exception exc) {
            return null;
        }
    }

    private static File getConvFile() {
        try {
            return File.createTempFile("test-130-hprof", "conv");
        } catch (Exception exc) {
            return null;
        }
    }
}
