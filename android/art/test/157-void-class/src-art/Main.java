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

import libcore.util.EmptyArray;

public class Main {
    public static void main(String[] args) {
        try {
            // Check if we're running dalvik or RI.
            Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
            System.loadLibrary(args[0]);
        } catch (ClassNotFoundException e) {
            usingRI = true;
            // Add expected JNI_OnLoad log line to match expected.txt.
            System.out.println("JNI_OnLoad called");
        }
        try {
            // Initialize all classes needed for old java.lang.Void.TYPE initialization.
            Runnable.class.getMethod("run", EmptyArray.CLASS).getReturnType();
        } catch (Exception e) {
            throw new Error(e);
        }
        // Clear the resolved types of the ojluni dex file to make sure there is no entry
        // for "V", i.e. void.
        clearResolvedTypes(Integer.class);
        // With java.lang.Void being compile-time verified but uninitialized, initialize
        // it now. Previously, this would indirectly initialize TYPE with the current,
        // i.e. zero-initialized, value of TYPE. The only thing that could prevent the
        // series of calls leading to this was a cache hit in Class.getDexCacheType()
        // which we have prevented by clearing the cache above.
        Class<?> voidClass = void.class;
        System.out.println("void.class = " + voidClass);
    }

    public static void clearResolvedTypes(Class<?> c) {
        if (!usingRI) {
            nativeClearResolvedTypes(c);
        }
    }

    public static native void nativeClearResolvedTypes(Class<?> c);

    static boolean usingRI = false;
}
