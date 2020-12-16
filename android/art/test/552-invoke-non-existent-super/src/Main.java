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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  // Workaround for b/18051191.
  class InnerClass {}

  public static void main(String[] args) throws Exception {
    Class<?> c = Class.forName("InvokeSuper");
    try {
      Method m = c.getMethod("run");
      m.invoke(c.newInstance(), new Object[0]);
      throw new Error("Expected NoSuchMethodError");
    } catch (InvocationTargetException e) {
      if (!(e.getCause() instanceof NoSuchMethodError)) {
        throw new Error("Expected NoSuchMethodError");
      }
    }
  }
}
