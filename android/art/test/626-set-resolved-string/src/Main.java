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

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    // Get all methods. We cannot call getDeclaredMethod("foo") as
    // that would make "foo" a strong root.
    Method[] methods = Main.class.getDeclaredMethods();

    // Call getName on the methods, which is implemented by using the dex
    // cache and  calling setResolvedString.
    for (int i = 0; i < methods.length; i++) {
      methods[i].getName();
    }

    // Compile Main.foo. "foo" needs to be a strong root for JIT compilation.
    // We stress test this:
    //   - avoid strongly interning "foo" by doing "f" + "oo"
    //   - call GC so that weaks can be collected.
    //   - invoke foo() to make sure "foo" hasn't been collected.
    ensureJitCompiled(Main.class, "f" + "oo");
    Runtime.getRuntime().gc();
    foo();
  }

  public static void foo() {
    System.out.println("foo");
  }

  public static native void ensureJitCompiled(Class cls, String method_name);
}
