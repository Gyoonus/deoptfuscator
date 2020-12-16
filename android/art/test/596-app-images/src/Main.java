/*
 * Copyright 2016 The Android Open Source Project
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
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

class Main {
  static class Inner {
    final public static int abc = 10;
  }

  static class Nested {

  }

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if (!checkAppImageLoaded()) {
      System.out.println("App image is not loaded!");
    } else if (!checkAppImageContains(Inner.class)) {
      System.out.println("App image does not contain Inner!");
    }

    if (!checkInitialized(Inner.class))
      System.out.println("Inner class is not initialized!");

    if (!checkInitialized(Nested.class))
      System.out.println("Nested class is not initialized!");

    if (!checkInitialized(StaticFields.class))
      System.out.println("StaticFields class is not initialized!");

    if (!checkInitialized(StaticFieldsInitSub.class))
      System.out.println("StaticFieldsInitSub class is not initialized!");

    if (!checkInitialized(StaticFieldsInit.class))
      System.out.println("StaticFieldsInit class is not initialized!");

    if (!checkInitialized(StaticInternString.class))
      System.out.println("StaticInternString class is not initialized!");

    StringBuffer sb = new StringBuffer();
    sb.append("java.");
    sb.append("abc.");
    sb.append("Action");

    String tmp = sb.toString();
    String intern = tmp.intern();

    assertNotEqual(tmp, intern, "Dynamically constructed String, not interned.");
    assertEqual(intern, StaticInternString.intent, "Static encoded literal String not interned.");
    assertEqual(BootInternedString.boot, BootInternedString.boot.intern(),
        "Static encoded literal String not moved back to runtime intern table.");

    try {
      Field f = StaticInternString.class.getDeclaredField("intent");
      assertEqual(intern, f.get(null), "String Literals are not interned properly.");

    } catch (Exception e) {
      System.out.println("Exception");
    }

    assertEqual(StaticInternString.getIntent(), StaticInternString2.getIntent(),
        "String Literals are not intenred properly, App image static strings duplicated.");

    // reload the class StaticInternString, check whether static strings interned properly
    final String DEX_FILE = System.getenv("DEX_LOCATION") + "/596-app-images.jar";
    final String LIBRARY_SEARCH_PATH = System.getProperty("java.library.path");

    try {
      Class<?> pathClassLoader = Class.forName("dalvik.system.PathClassLoader");
      if (pathClassLoader == null) {
        throw new AssertionError("Counldn't find path class loader class");
      }
      Constructor<?> ctor =
          pathClassLoader.getDeclaredConstructor(String.class, String.class, ClassLoader.class);
      ClassLoader loader = (ClassLoader) ctor.newInstance(
          DEX_FILE, LIBRARY_SEARCH_PATH, null);

      Class<?> staticInternString = loader.loadClass("StaticInternString");

      if (!checkAppImageContains(staticInternString)) {
        System.out.println("Not loaded again.");
      }
      Method getIntent = staticInternString.getDeclaredMethod("getIntent");

      assertEqual(StaticInternString.getIntent(), getIntent.invoke(staticInternString),
          "Dynamically loaded app image's literal strings not interned properly.");
    } catch (Exception e) {
      e.printStackTrace(System.out);
    }

  }

  public static native boolean checkAppImageLoaded();
  public static native boolean checkAppImageContains(Class<?> klass);
  public static native boolean checkInitialized(Class<?> klass);

  public static void assertEqual(Object a, Object b, String msg) {
    if (a != b)
      System.out.println(msg);
  }

  public static void assertNotEqual(Object a, Object b, String msg) {
    if (a == b)
      System.out.println(msg);
  }

}

class StaticFields{
  public static int abc;
}

class StaticFieldsInitSub extends StaticFieldsInit {
  final public static int def = 10;
}

class StaticFieldsInit{
  final public static int abc = 10;
}

class StaticInternString {
  final public static String intent = "java.abc.Action";
  static public String getIntent() {
    return intent;
  }
}

class BootInternedString {
  final public static String boot = "double";
}

class StaticInternString2 {
  final public static String intent = "java.abc.Action";

  static String getIntent() {
    return intent;
  }
}

