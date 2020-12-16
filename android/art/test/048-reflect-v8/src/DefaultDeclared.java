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

public class DefaultDeclared {
  interface DefaultInterface {
    default void sayHi() {
      System.out.println("hi default");
    }
  }

  interface RegularInterface {
    void sayHi();
  }

  class ImplementsWithDefault implements DefaultInterface {}

  class ImplementsWithDeclared implements DefaultInterface {
    public void sayHi() {
      System.out.println("hello specific from default");
    }
  }

  abstract class UnimplementedWithRegular implements RegularInterface { }

  class ImplementsWithRegular implements RegularInterface {
    public void sayHi() {
      System.out.println("hello specific");
    }
  }

  private static void printGetMethod(Class<?> klass) {
    Method m;
    try {
      m = klass.getDeclaredMethod("sayHi");
      System.out.println("No error thrown for class " + klass.toString());
    } catch (NoSuchMethodException e) {
      System.out.println("NoSuchMethodException thrown for class " + klass.toString());
    } catch (Throwable t) {
      System.out.println("Unknown error thrown for class " + klass.toString());
      t.printStackTrace(System.out);
    }
  }

  public static void test() {
    System.out.println("==============================");
    System.out.println("Are These Methods found by getDeclaredMethod:");
    System.out.println("==============================");

    printGetMethod(DefaultInterface.class);
    printGetMethod(RegularInterface.class);
    printGetMethod(ImplementsWithDefault.class);
    printGetMethod(ImplementsWithDeclared.class);
    printGetMethod(ImplementsWithRegular.class);
    printGetMethod(UnimplementedWithRegular.class);
  }
}
