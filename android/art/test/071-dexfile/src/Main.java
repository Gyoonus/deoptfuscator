/*
 * Copyright (C) 2008 The Android Open Source Project
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
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Enumeration;

/**
 * DexFile tests (Dalvik-specific).
 */
public class Main {
    private static final String CLASS_PATH = System.getenv("DEX_LOCATION") + "/071-dexfile-ex.jar";
    private static final String ODEX_DIR = System.getenv("DEX_LOCATION");
    private static final String ODEX_ALT = "/tmp";
    private static final String LIB_DIR = "/nowhere/nothing/";

    private static final String getOdexDir() {
        return new File(ODEX_DIR).isDirectory() ? ODEX_DIR : ODEX_ALT;
    }

    /**
     * Prep the environment then run the test.
     */
    public static void main(String[] args) throws Exception {
        /*
         * Create a sub-process to see if the ProcessManager wait
         * interferes with the dexopt invocation wait.
         *
         * /dev/random never hits EOF, so we're sure that we'll still
         * be waiting for the process to complete.  On the device it
         * stops pretty quickly (which means the child won't be
         * spinning).
         */
        ProcessBuilder pb = new ProcessBuilder("cat", "/dev/random");
        Process p = pb.start();

        testDexClassLoader();
        testDexFile();

        // shouldn't be necessary, but it's good to be tidy
        p.destroy();
        // let the ProcessManager's daemon thread finish before we shut down
        // (avoids the occasional segmentation fault)
        Thread.sleep(500);
        System.out.println("done");
    }

    /**
     * Create a class loader, explicitly specifying the source DEX and
     * the location for the optimized DEX.
     */
    private static void testDexClassLoader() throws Exception {
        ClassLoader dexClassLoader = getDexClassLoader();
        Class<?> Another = dexClassLoader.loadClass("Another");
        Object another = Another.newInstance();
        // not expected to work; just exercises the call
        dexClassLoader.getResource("nonexistent");
    }

    /*
     * Create an instance of DexClassLoader.  The test harness doesn't
     * have visibility into dalvik.system.*, so we do this through
     * reflection.
     */
    private static ClassLoader getDexClassLoader() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> DexClassLoader = classLoader.loadClass("dalvik.system.DexClassLoader");
        Constructor<?> DexClassLoader_init = DexClassLoader.getConstructor(String.class,
                                                                           String.class,
                                                                           String.class,
                                                                           ClassLoader.class);
        // create an instance, using the path we found
        return (ClassLoader) DexClassLoader_init.newInstance(CLASS_PATH,
                                                             getOdexDir(),
                                                             LIB_DIR,
                                                             classLoader);
    }

    private static void testDexFile() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> DexFile = classLoader.loadClass("dalvik.system.DexFile");
        Method DexFile_loadDex = DexFile.getMethod("loadDex",
                                                   String.class,
                                                   String.class,
                                                   Integer.TYPE);
        Method DexFile_entries = DexFile.getMethod("entries");
        Object dexFile = DexFile_loadDex.invoke(null, CLASS_PATH, null, 0);
        Enumeration<String> e = (Enumeration<String>) DexFile_entries.invoke(dexFile);
        while (e.hasMoreElements()) {
            String className = e.nextElement();
            System.out.println(className);
        }
    }
}
