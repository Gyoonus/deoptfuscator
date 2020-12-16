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

public class Test987 {
  // A class with a native method we can play with.
  static class Transform {
    private static native void sayHi();
    private static native void sayHi2();
  }

  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    Transform.sayHi();
    Transform.sayHi2();
  }
  // Functions called from native code.
  public static void doSayHi() {
    System.out.println("Hello");
  }

  public static void doSayHi2() {
    System.out.println("Hello - 2");
  }
}
