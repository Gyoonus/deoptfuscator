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
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

public class Reflection {
  public static boolean canDiscoverWithGetDeclaredField(Class<?> klass, String name) {
    try {
      klass.getDeclaredField(name);
      return true;
    } catch (NoSuchFieldException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetDeclaredFields(Class<?> klass, String name) {
    for (Field f : klass.getDeclaredFields()) {
      if (f.getName().equals(name)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canDiscoverWithGetField(Class<?> klass, String name) {
    try {
      klass.getField(name);
      return true;
    } catch (NoSuchFieldException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetFields(Class<?> klass, String name) {
    for (Field f : klass.getFields()) {
      if (f.getName().equals(name)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canGetField(Class<?> klass, String name) {
    try {
      Field f = klass.getDeclaredField(name);
      f.setAccessible(true);
      f.getInt(Modifier.isStatic(f.getModifiers()) ? null : klass.newInstance());
      return true;
    } catch (Exception ex) {
      ex.printStackTrace();
      return false;
    }
  }

  public static boolean canSetField(Class<?> klass, String name) {
    try {
      Field f = klass.getDeclaredField(name);
      f.setAccessible(true);
      f.setInt(Modifier.isStatic(f.getModifiers()) ? null : klass.newInstance(), 42);
      return true;
    } catch (Exception ex) {
      ex.printStackTrace();
      return false;
    }
  }

  public static boolean canDiscoverWithGetDeclaredMethod(Class<?> klass, String name) {
    try {
      klass.getDeclaredMethod(name);
      return true;
    } catch (NoSuchMethodException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetDeclaredMethods(Class<?> klass, String name) {
    for (Method m : klass.getDeclaredMethods()) {
      if (m.getName().equals(name)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canDiscoverWithGetMethod(Class<?> klass, String name) {
    try {
      klass.getMethod(name);
      return true;
    } catch (NoSuchMethodException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetMethods(Class<?> klass, String name) {
    for (Method m : klass.getMethods()) {
      if (m.getName().equals(name)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canInvokeMethod(Class<?> klass, String name) {
    try {
      Method m = klass.getDeclaredMethod(name);
      m.setAccessible(true);
      m.invoke(klass.isInterface() ? null : klass.newInstance());
      return true;
    } catch (Exception ex) {
      ex.printStackTrace();
      return false;
    }
  }

  public static boolean canDiscoverWithGetDeclaredConstructor(Class<?> klass, Class<?> args[]) {
    try {
      klass.getDeclaredConstructor(args);
      return true;
    } catch (NoSuchMethodException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetDeclaredConstructors(Class<?> klass, Class<?> args[]) {
    for (Constructor c : klass.getDeclaredConstructors()) {
      if (Arrays.equals(c.getParameterTypes(), args)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canDiscoverWithGetConstructor(Class<?> klass, Class<?> args[]) {
    try {
      klass.getConstructor(args);
      return true;
    } catch (NoSuchMethodException ex) {
      return false;
    }
  }

  public static boolean canDiscoverWithGetConstructors(Class<?> klass, Class<?> args[]) {
    for (Constructor c : klass.getConstructors()) {
      if (Arrays.equals(c.getParameterTypes(), args)) {
        return true;
      }
    }
    return false;
  }

  public static boolean canInvokeConstructor(Class<?> klass, Class<?> args[], Object[] initargs) {
    try {
      Constructor c = klass.getDeclaredConstructor(args);
      c.setAccessible(true);
      c.newInstance(initargs);
      return true;
    } catch (Exception ex) {
      ex.printStackTrace();
      return false;
    }
  }

  public static boolean canUseNewInstance(Class<?> klass) throws IllegalAccessException {
    try {
      klass.newInstance();
      return true;
    } catch (InstantiationException ex) {
      return false;
    }
  }

  private static native int getHiddenApiAccessFlags();

  public static boolean canObserveFieldHiddenAccessFlags(Class<?> klass, String name)
      throws Exception {
    return (klass.getDeclaredField(name).getModifiers() & getHiddenApiAccessFlags()) != 0;
  }

  public static boolean canObserveMethodHiddenAccessFlags(Class<?> klass, String name)
      throws Exception {
    return (klass.getDeclaredMethod(name).getModifiers() & getHiddenApiAccessFlags()) != 0;
  }

  public static boolean canObserveConstructorHiddenAccessFlags(Class<?> klass, Class<?> args[])
      throws Exception {
    return (klass.getConstructor(args).getModifiers() & getHiddenApiAccessFlags()) != 0;
  }
}
