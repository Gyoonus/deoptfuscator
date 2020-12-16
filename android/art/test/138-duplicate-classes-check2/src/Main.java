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

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * Structural hazard test.
 */
public class Main {
    public static String TEST_NAME = "138-duplicate-classes-check2";

    public static ClassLoader getClassLoaderFor(String location) throws Exception {
        try {
            Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
            Constructor<?> ctor =
                    class_loader_class.getConstructor(String.class, ClassLoader.class);
            /* on Dalvik, this is a DexFile; otherwise, it's null */
            return (ClassLoader) ctor.newInstance(location + "/" + TEST_NAME + "-ex.jar",
                                                  Main.class.getClassLoader());
        } catch (ClassNotFoundException e) {
            // Running on RI. Use URLClassLoader.
            return new java.net.URLClassLoader(
                    new java.net.URL[] { new java.net.URL("file://" + location + "/classes-ex/") });
        }
    }

    public static void main(String[] args) {
        new Main().run();
    }

    private void run() {
        System.out.println(new A().i);

        // Now run the class from the -ex file.
        try {
            /* this is the "alternate" DEX/Jar file */
            ClassLoader new_loader = getClassLoaderFor(System.getenv("DEX_LOCATION"));
            Class<?> klass = (Class<?>) new_loader.loadClass("TestEx");
            if (klass == null) {
                throw new AssertionError("loadClass failed");
            }
            Method run_test = klass.getMethod("test");
            run_test.invoke(null);
        } catch (Exception e) {
            System.out.println(e.toString());
            e.printStackTrace(System.out);
        }
    }
}
