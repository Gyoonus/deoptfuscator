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
    checkLoad("NullArrayFailInt2Object", true);
    checkLoad("NullArrayFailObject2Int", true);
    checkLoad("NullArraySuccessInt", false);
    checkLoad("NullArraySuccessInt2Float", false);
    checkLoad("NullArraySuccessShort", false);
    checkLoad("NullArraySuccessRef", false);
  }

  private static void checkLoad(String className, boolean expectError) throws Exception {
    Class<?> c;
    try {
      c = Class.forName(className);
      if (expectError) {
        throw new RuntimeException("Expected error for " + className);
      }
      Method m = c.getMethod("method");
      try {
        m.invoke(null);
        throw new RuntimeException("Expected an InvocationTargetException");
      } catch (InvocationTargetException e) {
        if (!(e.getCause() instanceof NullPointerException)) {
          throw new RuntimeException("Expected a NullPointerException");
        }
        System.out.println(className);
      }
    } catch (VerifyError e) {
      if (!expectError) {
        throw new RuntimeException(e);
      }
      System.out.println(className);
    }
  }
}
