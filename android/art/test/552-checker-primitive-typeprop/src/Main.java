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

import java.lang.reflect.Method;

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  private static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Wrong result, expected=" + expected + ", actual=" + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    Class<?> c = Class.forName("SsaBuilder");
    Method m = c.getMethod("environmentPhi", boolean.class, int[].class);

    int[] array = new int[3];
    int result;

    result = (Integer) m.invoke(null, true, array);
    assertEquals(2, result);

    result = (Integer) m.invoke(null, false, array);
    assertEquals(0, result);
  }
}
