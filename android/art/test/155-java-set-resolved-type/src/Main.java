/*
 * Copyright (C) 2017 The Android Open Source Project
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
import java.lang.reflect.Method;

public class Main {
    public static String TEST_NAME = "155-java-set-resolved-type";

    public static void main(String[] args) {
        try {
            Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
            System.loadLibrary(args[0]);
        } catch (ClassNotFoundException e) {
            usingRI = true;
            // Add expected JNI_OnLoad log line to match expected.txt.
            System.out.println("JNI_OnLoad called");
        }
        try {
            String dex_location = System.getenv("DEX_LOCATION");
            ClassLoader systemLoader = ClassLoader.getSystemClassLoader().getParent();
            ClassLoader exLoader = getClassLoaderFor(dex_location, systemLoader, /* ex */ true);
            ClassLoader mainLoader = getClassLoaderFor(dex_location, exLoader, /* ex */ false);

            // Resolve TestParameter class. It shall be defined by mainLoader.
            // This does not resolve method parameter types.
            Class<?> tpc = Class.forName("TestParameter", false, mainLoader);
            // Get declared methods of TestParameter.
            // This still does not resolve method parameter types.
            Method[] ms = tpc.getDeclaredMethods();
            if (ms == null || ms.length != 1) { throw new Error("Unexpected methods"); };
            // Call getParameterTypes() to resolve parameter types. The parameter type
            // TestInterface shall be defined by the exLoader. This used to store the
            // TestInterface class in the dex cache resolved types for the mainLoader
            // but not in the mainLoader's class table. This discrepancy used to cause
            // a crash further down.
            ms[0].getParameterTypes();

            // Resolve but do not initialize TestImplementation. During the resolution,
            // we see the TestInterface in the dex cache, so we do not try to look it up
            // or resolve it using the mainLoader.
            Class<?> timpl = Class.forName("TestImplementation", false, mainLoader);
            // Clear the dex cache resolved types to force a proper lookup the next time
            // we need to find TestInterface.
            clearResolvedTypes(timpl);

            // Force intialization of TestImplementation. This expects the interface type
            // to be resolved and found through simple lookup.
            timpl.newInstance();
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    public static ClassLoader getClassLoaderFor(String location, ClassLoader parent, boolean ex)
            throws Exception {
        try {
            Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
            Constructor<?> ctor =
                    class_loader_class.getConstructor(String.class, ClassLoader.class);
            /* on Dalvik, this is a DexFile; otherwise, it's null */
            String path = location + "/" + TEST_NAME + (ex ? "-ex.jar" : ".jar");
            return (ClassLoader)ctor.newInstance(path, parent);
        } catch (ClassNotFoundException e) {
            // Running on RI. Use URLClassLoader.
            String url = "file://" + location + (ex ? "/classes-ex/" : "/classes/");
            return new java.net.URLClassLoader(
                    new java.net.URL[] { new java.net.URL(url) }, parent);
        }
    }

    public static void clearResolvedTypes(Class<?> c) {
        if (!usingRI) {
            nativeClearResolvedTypes(c);
        }
    }

    private static boolean usingRI = false;

    public static native void nativeClearResolvedTypes(Class<?> c);
}
