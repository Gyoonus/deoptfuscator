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

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;

class MyClassLoader extends ClassLoader {
  MyClassLoader() throws Exception {
    super(MyClassLoader.class.getClassLoader());

    // Some magic to get access to the pathList field of BaseDexClassLoader.
    ClassLoader loader = getClass().getClassLoader();
    Class<?> baseDexClassLoader = loader.getClass().getSuperclass();
    Field f = baseDexClassLoader.getDeclaredField("pathList");
    f.setAccessible(true);
    Object pathList = f.get(loader);

    // Some magic to get access to the dexField field of pathList.
    f = pathList.getClass().getDeclaredField("dexElements");
    f.setAccessible(true);
    dexElements = (Object[]) f.get(pathList);
    dexFileField = dexElements[0].getClass().getDeclaredField("dexFile");
    dexFileField.setAccessible(true);
  }

  Object[] dexElements;
  Field dexFileField;

  static ClassLoader level1ClassLoader;

  protected Class<?> loadClass(String className, boolean resolve) throws ClassNotFoundException {
    if (this != level1ClassLoader) {
      if (className.equals("Level1")) {
        return level1ClassLoader.loadClass(className);
      } else if (className.equals("Level2")) {
        throw new ClassNotFoundException("None of my methods require Level2!");
      } else if (!className.equals("LoadedByMyClassLoader")) {
        // We're only going to handle LoadedByMyClassLoader.
        return getParent().loadClass(className);
      }
    } else {
      if (className != "Level1" && className != "Level2") {
        return getParent().loadClass(className);
      }
    }

    // Mimic what DexPathList.findClass is doing.
    try {
      for (Object element : dexElements) {
        Object dex = dexFileField.get(element);
        Method method = dex.getClass().getDeclaredMethod(
            "loadClassBinaryName", String.class, ClassLoader.class, List.class);

        if (dex != null) {
          Class<?> clazz = (Class<?>)method.invoke(dex, className, this, null);
          if (clazz != null) {
            return clazz;
          }
        }
      }
    } catch (Exception e) { /* Ignore */ }
    return null;
  }
}

class LoadedByMyClassLoader {
  public static void bar() {
    Level1.$inline$bar();
  }
}

class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    // Clone resolved methods, to restore the original version just
    // before we walk the stack in $noinline$bar.
    savedResolvedMethods = cloneResolvedMethods(Main.class);

    MyClassLoader o = new MyClassLoader();
    MyClassLoader.level1ClassLoader = new MyClassLoader();
    Class<?> foo = o.loadClass("LoadedByMyClassLoader");
    Method m = foo.getDeclaredMethod("bar");
    try {
      m.invoke(null);
    } catch (Error e) { /* Ignore */ }
  }

  public static void $inline$bar() {
  }

  public static void $noinline$bar() {
    try {
      // Be evil and clear all dex cache entries.
      Field f = Class.class.getDeclaredField("dexCache");
      f.setAccessible(true);
      Object dexCache = f.get(Main.class);
      f = dexCache.getClass().getDeclaredField("resolvedTypes");
      f.setAccessible(true);
      Object[] array = (Object[]) f.get(dexCache);
      for (int i = 0; i < array.length; i++) {
        array[i] = null;
      }
      restoreResolvedMethods(Main.class, savedResolvedMethods);
    } catch (Throwable t) { /* Ignore */ }

    // This will walk the stack, trying to resolve methods in it.
    // Because we cleared dex cache entries, we will have to find
    // classes again, which require to use the correct class loader
    // in the presence of inlining.
    new Exception().printStackTrace(System.out);
  }
  static Object savedResolvedMethods;

  static native Object cloneResolvedMethods(Class<?> cls);
  static native void restoreResolvedMethods(Class<?> cls, Object saved);
}
