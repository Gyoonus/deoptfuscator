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

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;

/**
 * PathClassLoader test.
 */
public class Main {

    private static ClassLoader createClassLoader(String dexPath, ClassLoader parent) {
        try {
            Class<?> myClassLoaderClass = Class.forName("MyPathClassLoader");
            Constructor<?> constructor = myClassLoaderClass.getConstructor(String.class,
                                                                           ClassLoader.class);
            return (ClassLoader)constructor.newInstance(dexPath, parent);
        } catch (Exception e) {
            // Ups, not available?!?!
            throw new RuntimeException(e);
        }
    }

    /**
     * Main entry point.
     */
    public static void main(String[] args) throws Exception {
        // Check the class-path for the second file. We'll use that one as the source of the
        // new classloader.
        String cp = System.getProperty("java.class.path");
        if (cp.split(System.getProperty("path.separator")).length != 1) {
            throw new IllegalStateException("Didn't find exactly one classpath element in " + cp);
        }
        if (!cp.endsWith("classloader2.jar")) {
            throw new IllegalStateException("Don't understand classpath " + cp);
        }
        cp = cp.replace("classloader2.jar", "classloader2-ex.jar");

        ClassLoader myClassLoader = createClassLoader(
                cp, ClassLoader.getSystemClassLoader().getParent());

        // Now load our test class.
        Class<?> srcClass = A.class;
        Class<?> exClass = myClassLoader.loadClass("A");

        // First check: classes should be different.
        if (srcClass == exClass) {
            throw new IllegalStateException("Loaded class instances are the same");
        }

        // Secondary checks: get the static field values and make sure they aren't the same.
        String srcValue = (String)srcClass.getDeclaredField("value").get(null);
        if (!"Src-A".equals(srcValue)) {
            throw new IllegalStateException("Expected Src-A, found " + srcValue);
        }
        String exValue = (String)exClass.getDeclaredField("value").get(null);
        if (!"Ex-A".equals(exValue)) {
            throw new IllegalStateException("Expected Ex-A, found " + exValue);
        }

        // Try to load a dex file with bad dex code. Use new instance to force verification.
        try {
          Class<?> badClass = Main.class.getClassLoader().loadClass("B");
          System.out.println("Loaded class B.");
          badClass.newInstance();
          System.out.println("Should not be able to instantiate B with bad dex bytecode.");
        } catch (VerifyError e) {
          System.out.println("Caught VerifyError.");
        }

        // Make sure the same error is rethrown when reloading the bad class.
        try {
          Class<?> badClass = Main.class.getClassLoader().loadClass("B");
          System.out.println("Loaded class B.");
          badClass.newInstance();
          System.out.println("Should not be able to instantiate B with bad dex bytecode.");
        } catch (NoClassDefFoundError e) {
          if (e.getCause() instanceof VerifyError) {
            System.out.println("Caught wrapped VerifyError.");
          } else {
            e.printStackTrace(System.out);
          }
        }

        System.out.println("Everything OK.");
    }
}
