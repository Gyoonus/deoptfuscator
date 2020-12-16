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
    public static String TEST_NAME = "164-resolution-trampoline-dex-cache";

    public static void main(String[] args) {
        // Load the test JNI for ensureJitCompiled(). Note that classes Main loaded
        // by other class loaders do not have the binding, so we need to pass the
        // current Main.class around and call ensureJitCompiled() via reflection.
        System.loadLibrary(args[0]);
        try {
            String dex_location = System.getenv("DEX_LOCATION");
            ClassLoader systemLoader = ClassLoader.getSystemClassLoader().getParent();
            ClassLoader baseLoader = getClassLoaderFor(dex_location, systemLoader, /* ex */ false);
            ClassLoader mainLoader = getClassLoaderFor(dex_location, baseLoader, /* ex */ true);

            Class<?> tc = Class.forName("MostDerived", true, mainLoader);
            Method m = tc.getDeclaredMethod("test", Class.class);
            m.invoke(null, Main.class);
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
            String path = location + "/" + TEST_NAME + (ex ? "-ex.jar" : ".jar");
            return (ClassLoader)ctor.newInstance(path, parent);
        } catch (ClassNotFoundException e) {
            // Running on RI. Use URLClassLoader.
            String url = "file://" + location + (ex ? "/classes-ex/" : "/classes/");
            return new java.net.URLClassLoader(
                    new java.net.URL[] { new java.net.URL(url) }, parent);
        }
    }

    public static native void ensureJitCompiled(Class<?> klass, String method_name);
}
