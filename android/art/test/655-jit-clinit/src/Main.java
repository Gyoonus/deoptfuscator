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

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    if (!hasJit()) {
      return;
    }
    Foo.hotMethod();
  }

  public native static boolean hasJitCompiledEntrypoint(Class<?> cls, String methodName);
  private native static boolean hasJit();
}

class Foo {
  static void hotMethod() {
    for (int i = 0; i < array.length; ++i) {
      array[i] = array;
    }
  }

  static {
    array = new Object[10000];
    while (!Main.hasJitCompiledEntrypoint(Foo.class, "hotMethod")) {
      Foo.hotMethod();
      try {
        // Sleep to give a chance for the JIT to compile `hotMethod`.
        Thread.sleep(100);
      } catch (Exception e) {
        // Ignore
      }
    }
  }

  static Object[] array;
}
