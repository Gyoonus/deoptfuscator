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
import p1.InP1;
import p1.PlaceHolder;


// Custom class loader to prevent loading while verifying.
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
    if (className.equals("p1.OtherInP1") && !p1.PlaceHolder.entered) {
      // The request comes from the verifier. Return null to get the access check entry
      // point in the compiled code.
      return null;
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
    return getParent().loadClass(className);
  }
}

public class Main {
    public static void main(String[] args) throws Exception {
      MyClassLoader o = new MyClassLoader();
      Class<?> foo = o.loadClass("LoadedByMyClassLoader");
      Method m = foo.getDeclaredMethod("main");
      m.invoke(null);
    }
}

class LoadedByMyClassLoader {
    public static void main() throws Exception {
      for (int i = 0; i < 10000; ++i) {
        // Warm up the JIT.
        doTheCall(i);
      }
      // Sleep a while to let the JIT compile things.
      // TODO(ngeoffray): Remove the sleep. b/25414532
      Thread.sleep(2000);
      doTheCall(10001);
    }

    public static void doTheCall(int i) {
      InP1.$inline$AllocateOtherInP1(i);
      InP1.$inline$AllocateArrayOtherInP1(i);
      InP1.$inline$UseStaticFieldOtherInP1(i);
      InP1.$inline$SetStaticFieldOtherInP1(i);
      InP1.$inline$UseInstanceFieldOtherInP1(i);
      InP1.$inline$SetInstanceFieldOtherInP1(i);
      InP1.$inline$LoadOtherInP1(i);
      InP1.$inline$StaticCallOtherInP1(i);
      InP1.$inline$InstanceCallOtherInP1(i);
    }
}
