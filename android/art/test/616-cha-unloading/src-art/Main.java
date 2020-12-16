/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class Main {
  static final String DEX_FILE = System.getenv("DEX_LOCATION") + "/616-cha-unloading-ex.jar";
  static final String LIBRARY_SEARCH_PATH = System.getProperty("java.library.path");
  static Constructor<? extends ClassLoader> sConstructor;

  private static class CHAUnloaderRetType {
    private CHAUnloaderRetType(WeakReference<ClassLoader> cl,
                              AbstractCHATester obj,
                              long methodPtr) {
      this.cl = cl;
      this.obj = obj;
      this.methodPtr = methodPtr;
    }
    public WeakReference<ClassLoader> cl;
    public AbstractCHATester obj;
    public long methodPtr;
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    Class<ClassLoader> pathClassLoader = (Class<ClassLoader>) Class.forName("dalvik.system.PathClassLoader");
    sConstructor =
        pathClassLoader.getDeclaredConstructor(String.class, String.class, ClassLoader.class);

    testUnload();
  }

  private static void testUnload() throws Exception {
    // Load a concrete class, then unload it. Get a deleted ArtMethod to test if it'll be inlined.
    CHAUnloaderRetType result = doUnloadLoader();
    WeakReference<ClassLoader> loader = result.cl;
    long methodPtr = result.methodPtr;
    // Check that the classloader is indeed unloaded.
    if (loader.get() != null) {
      throw new Error("Expected class loader to be unloaded");
    }

    // Reuse the linear alloc used by the unloaded class loader.
    reuseArenaOfMethod(methodPtr);

    // Try to JIT-compile under dangerous conditions.
    ensureJitCompiled(Main.class, "targetMethodForJit");
    System.out.println("Done");
  }

  private static void doUnloading() {
    // Do multiple GCs to prevent rare flakiness if some other thread is keeping the
    // classloader live.
    for (int i = 0; i < 5; ++i) {
       Runtime.getRuntime().gc();
    }
  }

  private static CHAUnloaderRetType setupLoader()
      throws Exception {
    ClassLoader loader = sConstructor.newInstance(
        DEX_FILE, LIBRARY_SEARCH_PATH, ClassLoader.getSystemClassLoader());
    Class<?> concreteCHATester = loader.loadClass("ConcreteCHATester");

    // Preemptively compile methods to prevent delayed JIT tasks from blocking the unloading.
    ensureJitCompiled(concreteCHATester, "<init>");
    ensureJitCompiled(concreteCHATester, "lonelyMethod");

    Object obj = concreteCHATester.newInstance();
    Method lonelyMethod = concreteCHATester.getDeclaredMethod("lonelyMethod");

    // Get a pointer to a region that shall be not used after the unloading.
    long artMethod = getArtMethod(lonelyMethod);

    AbstractCHATester ret = null;
    return new CHAUnloaderRetType(new WeakReference(loader), ret, artMethod);
  }

  private static CHAUnloaderRetType targetMethodForJit(int mode)
      throws Exception {
    CHAUnloaderRetType ret = new CHAUnloaderRetType(null, null, 0);
    if (mode == 0) {
      ret = setupLoader();
    } else if (mode == 1) {
      // This branch is not supposed to be executed. It shall trigger "lonelyMethod" inlining
      // during jit compilation of "targetMethodForJit".
      ret = setupLoader();
      AbstractCHATester obj = ret.obj;
      obj.lonelyMethod();
    }
    return ret;
  }

  private static CHAUnloaderRetType doUnloadLoader()
      throws Exception {
    CHAUnloaderRetType result = targetMethodForJit(0);
    doUnloading();
    return result;
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
  private static native long getArtMethod(Object javaMethod);
  private static native void reuseArenaOfMethod(long artMethod);
}
