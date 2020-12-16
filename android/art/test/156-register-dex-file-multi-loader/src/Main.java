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

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
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

  protected Class<?> loadClass(String className, boolean resolve) throws ClassNotFoundException {
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
    } catch (InvocationTargetException ite) {
      throw new ClassNotFoundException(className, ite.getCause());
    } catch (Exception e) {
      throw new Error(e);
    }
    return getParent().loadClass(className);
  }
}

public class Main {
  public static void main(String[] args) throws Exception {
    MyClassLoader o = new MyClassLoader();
    try {
      Class<?> foo = o.loadClass("Main");
      throw new Error("Unreachable");
    } catch (ClassNotFoundException cnfe) {
      boolean unexpected = false;
      if (!(cnfe.getCause() instanceof InternalError)) {
        unexpected = true;
      } else {
        String message = cnfe.getCause().getMessage();
        unexpected = !message.startsWith("Attempt to register dex file ") ||
                     !message.endsWith(" with multiple class loaders");
      }
      if (unexpected) {
        cnfe.getCause().printStackTrace(System.out);
      }
    }
  }
}
