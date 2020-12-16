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

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    final String DEX_FILE = System.getenv("DEX_LOCATION") + "/613-inlining-dex-cache-ex.jar";
    ClassLoader loader = new DelegateLastPathClassLoader(DEX_FILE, Main.class.getClassLoader());
    Class cls = loader.loadClass("LoadedByAppClassLoader");
    Method m = cls.getDeclaredMethod("letMeInlineYou");
    // Invoke the method enough times to get JITted.
    for (int i = 0; i < 10000; ++i) {
      m.invoke(null);
    }
    ensureJitCompiled(cls, "letMeInlineYou");
    ClassLoader bLoader = areYouB();
    if (bLoader != Main.class.getClassLoader()) {
      throw new Error("Wrong class loader");
    }
  }

  public static void foo(Main o) {
    // LoadedByAppClassLoader.letMeInlineYou will try to inline this
    // method but used to pass the wrong class loader. As a result,
    // the lookup of B.foo was updating the dex cache with the other
    // class loader's B class.
    if (o != null) {
      o.myField.foo();
    }
  }

  public B myField;

  public static ClassLoader areYouB() {
    return OtherClass.getB().getClassLoader();
  }

  public static native void ensureJitCompiled(Class cls, String method_name);
}

class OtherClass {
  public static Class getB() {
    // This used to return the B class of another class loader.
    return B.class;
  }
}
