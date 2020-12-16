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

package art;

import java.lang.reflect.Method;
import java.util.HashMap;

public class Test986 {
  private static final HashMap<Method, String> SymbolMap = new HashMap<>();

  // A class with a native method we can play with.
  static class Transform {
    private static native void sayHi();
    private static native void sayHi2();
  }

  public static void run() throws Exception {
    setupNativeBindNotify();
    setNativeBindNotify(true);
    doTest();
  }

  private static void setNativeTransform(Method method, String dest) {
    SymbolMap.put(method, dest);
  }

  private static void removeNativeTransform(Method method) {
    SymbolMap.remove(method);
  }

  /**
   * Notifies java that a native method bind has occurred and requests the new symbol to bind to.
   */
  public static String doNativeMethodBind(Method method, String nativeSym) {
    // Disable native bind notify for now to avoid infinite loops.
    setNativeBindNotify(false);
    String transSym = SymbolMap.getOrDefault(method, nativeSym);
    System.out.println(method + " = " + nativeSym + " -> " + transSym);
    setNativeBindNotify(true);
    return transSym;
  }

  public static void doTest() throws Exception {
    Method say_hi_method = Transform.class.getDeclaredMethod("sayHi");

    // Test we will bind fine if we make no changes.
    Transform.sayHi2();

    // Test we can get in the middle of autobind
    setNativeTransform(say_hi_method, "NoReallySayGoodbye");
    Transform.sayHi();

    // Test we can get in between manual bind.
    setNativeTransform(say_hi_method, "Java_art_Test986_00024Transform_sayHi2");
    rebindTransformClass();
    Transform.sayHi();

    // Test we can get rid of transform
    removeNativeTransform(say_hi_method);
    rebindTransformClass();
    Transform.sayHi();
  }

  // Functions called from native code.
  public static void doSayHi() {
    System.out.println("Hello");
  }

  public static void doSayHi2() {
    System.out.println("Hello - 2");
  }

  public static void doSayBye() {
    System.out.println("Bye");
  }

  private static native void setNativeBindNotify(boolean enable);
  private static native void setupNativeBindNotify();
  private static void rebindTransformClass() {
    rebindTransformClass(Transform.class);
  }
  private static native void rebindTransformClass(Class<?> trans);
}
