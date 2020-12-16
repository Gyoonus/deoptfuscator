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

public class IsDefaultTest {
  interface DefaultInterface {
    default void sayHi() {
      System.out.println("hi default");
    }
  }

  interface RegularInterface {
    void sayHi();
  }

  class ImplementsWithDefault implements DefaultInterface {}
  class ImplementsWithRegular implements RegularInterface {
    public void sayHi() {
      System.out.println("hello specific");
    }
  }

  private static void printIsDefault(Class<?> klass) {
    Method m;
    try {
      m = klass.getMethod("sayHi");
    } catch (Throwable t) {
      System.out.println(t);
      return;
    }

    boolean isDefault = m.isDefault();
    System.out.println(klass.getName() + " is default = " + (isDefault ? "yes" : "no"));
  }

  public static void test() {
    System.out.println("==============================");
    System.out.println("Are These Methods Default:");
    System.out.println("==============================");

    printIsDefault(DefaultInterface.class);
    printIsDefault(RegularInterface.class);
    printIsDefault(ImplementsWithDefault.class);
    printIsDefault(ImplementsWithRegular.class);
  }
}
