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

public class Main {
    public static String TEST_NAME = "158-app-image-class-table";

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
            // Resolve but do not initialize TestImplementation. During the resolution,
            // we see the Cloneable in the dex cache, so we do not try to look it up
            // or resolve it.
            Class<?> timpl =
                Class.forName("TestImplementation", false, Main.class.getClassLoader());
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

    public static void clearResolvedTypes(Class<?> c) {
        if (!usingRI) {
            nativeClearResolvedTypes(c);
        }
    }

    private static boolean usingRI = false;

    public static native void nativeClearResolvedTypes(Class<?> c);
}
