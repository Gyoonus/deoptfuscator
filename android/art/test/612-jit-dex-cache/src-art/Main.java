/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import dalvik.system.PathClassLoader;

// ClassLoader not delegating for non java. packages.
class DelegateLastPathClassLoader extends PathClassLoader {

  public DelegateLastPathClassLoader(String dexPath, ClassLoader parent) {
    super(dexPath, parent);
  }

  @Override
  protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
    if (!name.startsWith("java.")) {
      try {
        return findClass(name);
      } catch (ClassNotFoundException ignore) {
        // Ignore and fall through to parent class loader.
      }
    }
    return super.loadClass(name, resolve);
  }
}

public class Main {

   private static Class<?> classFromDifferentLoader() throws Exception {
     final String DEX_FILE = System.getenv("DEX_LOCATION") + "/612-jit-dex-cache-ex.jar";
     ClassLoader loader = new DelegateLastPathClassLoader(DEX_FILE, Main.class.getClassLoader());
     return loader.loadClass("LoadedByAppClassLoader");
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Class<?> cls = classFromDifferentLoader();
    Method m = cls.getDeclaredMethod("letMeInlineYou", A.class);
    B b = new B();
    // Invoke the method enough times to get an inline cache and get JITted.
    for (int i = 0; i < 10000; ++i) {
      m.invoke(null, b);
    }
    m = cls.getDeclaredMethod("areYouB", null);
    ClassLoader loader = (ClassLoader) m.invoke(null);
    if (loader != cls.getClassLoader()) {
      throw new Error("Wrong class loader");
    }
  }

  public static native void ensureJitCompiled(Class<?> cls, String method_name);
}
