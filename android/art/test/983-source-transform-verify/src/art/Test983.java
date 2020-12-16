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

package art;

public class Test983 {
  static class Transform {
    public void sayHi() {
      System.out.println("hello");
    }
  }

  public static void run() {
    doTest();
  }

  private native static void setupLoadHook();

  /* called from JNI */
  public static void doPrintln(String str) {
    System.out.println(str);
  }

  public static void doTest() {
    setupLoadHook();
    Redefinition.enableCommonRetransformation(true);
    Redefinition.doCommonClassRetransformation(Transform.class);
    Redefinition.doCommonClassRetransformation(Object.class);
    // NB java.lang.ClassLoader has hidden fields.
    Redefinition.doCommonClassRetransformation(ClassLoader.class);
    Redefinition.enableCommonRetransformation(false);
  }
}
