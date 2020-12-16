/*
 * Copyright (C) 2014 The Android Open Source Project
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

  public static void main(String[] args) throws Throwable {
    // Attempt to instantiate an interface.
    $opt$NewInstance("newInstanceInterface", InstantiationError.class.getCanonicalName());
    // Attempt to instantiate an abstract class.
    $opt$NewInstance("newInstanceClass", InstantiationError.class.getCanonicalName());
    // Attempt to instantiate an interface.
    $opt$NewInstance("newInstancePrivateClass", IllegalAccessError.class.getCanonicalName());
    // Attempt to instantiate an abstract class.
    $opt$NewInstance("newInstanceUnknownClass", NoClassDefFoundError.class.getCanonicalName());
  }

  private static void $opt$NewInstance(String method, String errorName) throws Throwable {
    try {
      Class<?> c = Class.forName("NewInstance");
      Method m = c.getMethod(method);
      m.invoke(c.newInstance());
      throw new RuntimeException("Failed to throw " + errorName);
    } catch (InvocationTargetException e) {
      if (!e.getCause().getClass().getCanonicalName().equals(errorName)) {
        throw new RuntimeException("Failed to throw " + errorName
            + ". Threw: " + e.getCause());
      }
    }
  }
}
