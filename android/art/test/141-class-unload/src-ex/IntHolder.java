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

// Simple class that holds a static int for testing that class unloading works
// and re-runs the class initializer.
public class IntHolder {
    private static int value = 1;

    public static void setValue(int newValue) {
        value = newValue;
    }

    public static int getValue() {
        return value;
    }

    public static void runGC() {
        Runtime.getRuntime().gc();
    }

    public static void loadLibrary(String name) {
        System.loadLibrary(name);
    }

    public static native void waitForCompilation();

    public static Throwable generateStackTrace() {
      return new Exception("test");
    }
}
