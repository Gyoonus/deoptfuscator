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

import java.lang.reflect.*;

public class Main {
  public static final String TEST_NAME = "998-redefine-use-after-free";
  public static final int REPS = 1000;
  public static final int STEP = 100;

  public static void main(String[] args) throws Exception {
    for (int i = 0; i < REPS; i += STEP) {
      runSeveralTimes(STEP);
    }
  }

  public static ClassLoader getClassLoaderFor(String location) throws Exception {
    try {
      Class<?> class_loader_class = Class.forName("dalvik.system.PathClassLoader");
      Constructor<?> ctor = class_loader_class.getConstructor(String.class, ClassLoader.class);
      return (ClassLoader)ctor.newInstance(location + "/" + TEST_NAME + "-ex.jar",
                                           Main.class.getClassLoader());
    } catch (ClassNotFoundException e) {
      // Running on RI. Use URLClassLoader.
      return new java.net.URLClassLoader(
          new java.net.URL[] { new java.net.URL("file://" + location + "/classes-ex/") });
    }
  }

  // Run the redefinition several times on a single class-loader to try to trigger the
  // Use-after-free bug b/62237378
  public static void runSeveralTimes(int times) throws Exception {
    ClassLoader c = getClassLoaderFor(System.getenv("DEX_LOCATION"));

    Class<?> klass = (Class<?>)c.loadClass("DexCacheSmash");
    Method m = klass.getDeclaredMethod("run");
    for (int i = 0 ; i < times; i++) {
      m.invoke(null);
    }
  }
}
