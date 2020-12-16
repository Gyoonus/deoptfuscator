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

public class Main {
  static final String DEX_FILE =
      System.getenv("DEX_LOCATION") + "/661-classloader-allocator-ex.jar";
  static final String LIBRARY_SEARCH_PATH = System.getProperty("java.library.path");

  private static void doUnloading() {
    // Stop the JIT to ensure its threads and work queue are not keeping classes
    // artifically alive.
    stopJit();
    // Do multiple GCs to prevent rare flakiness if some other thread is keeping the
    // classloader live.
    for (int i = 0; i < 5; ++i) {
      Runtime.getRuntime().gc();
    }
    startJit();
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    loadClass();
    doUnloading();
    // fetchProfiles iterate over the ProfilingInfo, we used to crash in the presence
    // of unloaded copied methods.
    fetchProfiles();
  }

  public static void loadClass() throws Exception {
    Class<?> pathClassLoader = Class.forName("dalvik.system.PathClassLoader");
    if (pathClassLoader == null) {
      throw new AssertionError("Couldn't find path class loader class");
    }
    Constructor<?> constructor =
      pathClassLoader.getDeclaredConstructor(String.class, String.class, ClassLoader.class);
    ClassLoader loader = (ClassLoader) constructor.newInstance(
      DEX_FILE, LIBRARY_SEARCH_PATH, ClassLoader.getSystemClassLoader());
    Class<?> otherClass = loader.loadClass("p1.OtherClass");
    ensureJitCompiled(otherClass, "foo");
  }

  public static native void ensureJitCompiled(Class<?> cls, String methodName);
  public static native void fetchProfiles();
  public static native void stopJit();
  public static native void startJit();
}
