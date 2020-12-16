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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
    double[] array = new double[5];
    try {
      Class<?> c = Class.forName("TestCase");
      Method m = c.getMethod("run", double[].class, int.class);
      m.invoke(null, array, 42);
    } catch (InvocationTargetException e) {
      // expected
      if (!(e.getCause() instanceof ArrayIndexOutOfBoundsException)) {
        throw new Error("Expected ArrayIndexOutOfBoundsException, got " + e.getCause());
      }
      return;
    }
    throw new Error("Expected InvocationTargetException");
  }
}
