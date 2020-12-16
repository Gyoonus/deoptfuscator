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
import java.util.ArrayList;
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
    // Need to make a copy of the dex elements since we don't want an app image with pre-resolved
    // things.
    f = pathList.getClass().getDeclaredField("dexElements");
    f.setAccessible(true);
    Object[] dexElements = (Object[]) f.get(pathList);
    f = dexElements[0].getClass().getDeclaredField("dexFile");
    f.setAccessible(true);
    for (Object element : dexElements) {
      Object dexFile = f.get(element);
      // Make copy.
      Field fileNameField = dexFile.getClass().getDeclaredField("mFileName");
      fileNameField.setAccessible(true);
      dexFiles.add(dexFile.getClass().getDeclaredConstructor(String.class).newInstance(
        fileNameField.get(dexFile)));
    }
  }

  ArrayList<Object> dexFiles = new ArrayList<Object>();
  Field dexFileField;

  protected Class<?> loadClass(String className, boolean resolve) throws ClassNotFoundException {
    // Other classes may also get loaded, ignore those.
    if (className.equals("LoadedByMyClassLoader") || className.equals("FirstSeenByMyClassLoader")) {
      System.out.println("Request for " + className);
    }

    // We're only going to handle LoadedByMyClassLoader.
    if (className != "LoadedByMyClassLoader") {
      return getParent().loadClass(className);
    }

    // Mimic what DexPathList.findClass is doing.
    try {
      for (Object dexFile : dexFiles) {
        Method method = dexFile.getClass().getDeclaredMethod(
            "loadClassBinaryName", String.class, ClassLoader.class, List.class);

        if (dexFile != null) {
          Class<?> clazz = (Class<?>)method.invoke(dexFile, className, this, null);
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
  /// CHECK-START: void LoadedByMyClassLoader.bar() inliner (before)
  /// CHECK:      LoadClass class_name:FirstSeenByMyClassLoader
  /// CHECK-NEXT: ClinitCheck
  /// CHECK-NEXT: InvokeStaticOrDirect
  /// CHECK-NEXT: LoadClass class_name:java.lang.System
  /// CHECK-NEXT: ClinitCheck
  /// CHECK-NEXT: StaticFieldGet
  /// CHECK-NEXT: LoadString
  /// CHECK-NEXT: NullCheck
  /// CHECK-NEXT: InvokeVirtual

  /// CHECK-START: void LoadedByMyClassLoader.bar() inliner (after)
  /// CHECK:      LoadClass class_name:FirstSeenByMyClassLoader
  /// CHECK-NEXT: ClinitCheck
                /* We inlined FirstSeenByMyClassLoader.$inline$bar */
  /// CHECK-NEXT: LoadClass class_name:java.lang.System
  /// CHECK-NEXT: ClinitCheck
  /// CHECK-NEXT: StaticFieldGet
  /// CHECK-NEXT: LoadString
  /// CHECK-NEXT: NullCheck
  /// CHECK-NEXT: InvokeVirtual

  /// CHECK-START: void LoadedByMyClassLoader.bar() register (before)
                /* Load and initialize FirstSeenByMyClassLoader */
  /// CHECK:      LoadClass class_name:FirstSeenByMyClassLoader gen_clinit_check:true
                /* Load and initialize System */
  // There may be MipsComputeBaseMethodAddress here.
  /// CHECK:      LoadClass class_name:java.lang.System
  // The ClinitCheck may (PIC) or may not (non-PIC) be merged into the LoadClass.
  // (The merging checks for environment match but HLoadClass/kBootImageAddress
  // used for non-PIC mode does not have an environment at all.)
  /// CHECK:      StaticFieldGet
  // There may be HX86ComputeBaseMethodAddress or MipsComputeBaseMethodAddress here.
  /// CHECK:      LoadString
  /// CHECK-NEXT: NullCheck
  /// CHECK-NEXT: InvokeVirtual
  public static void bar() {
    FirstSeenByMyClassLoader.$inline$bar();
    System.out.println("In between the two calls.");
    FirstSeenByMyClassLoader.$noinline$bar();
  }
}

public class Main {
  public static void main(String[] args) throws Exception {
    MyClassLoader o = new MyClassLoader();
    Class<?> foo = o.loadClass("LoadedByMyClassLoader");
    Method m = foo.getDeclaredMethod("bar");
    m.invoke(null);
  }
}
