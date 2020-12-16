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

import java.lang.reflect.Method;

public class Main {

  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    test();

    try {
      if (testCompiled(Main.class.getDeclaredMethod("test"))) {
        System.out.println("test method successfully verified/compiled.");
      } else {
        System.out.println("test method failed to verify/compile.");
      }
    } catch (Exception e) {
      System.out.println("Got unexpected exception: " + e);
    }
  }

  public static void test() {
    int[] maybe_null_array = null;
    for (int i = 0; i < 2; i++) {
      int[] non_null_array = new int[1];
      if (maybe_null_array != null) {
        i = maybe_null_array[0] + 1;
      }
      maybe_null_array = non_null_array;
    }
  }

  public static native boolean testCompiled(Method method);
}
