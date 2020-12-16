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

import java.io.File;
import java.lang.reflect.Method;
import java.util.Arrays;

public class Main {
  public static void main(String[] args) throws Exception {
    // Check whether we get the BootClassLoader (not null).
    ClassLoader bootClassLoader = Object.class.getClassLoader();
    if (bootClassLoader == null) {
      throw new IllegalStateException("Expected non-null classloader for Object");
    }

    // Try to load libarttest(d) with the BootClassLoader. First construct the filename.
    String libName = System.mapLibraryName(args[0]);
    Method libPathsMethod = Runtime.class.getDeclaredMethod("getLibPaths");
    libPathsMethod.setAccessible(true);
    String[] libPaths = (String[])libPathsMethod.invoke(Runtime.getRuntime());
    String fileName = null;
    for (String p : libPaths) {
      String candidate = p + libName;
      if (new File(candidate).exists()) {
          fileName = candidate;
          break;
      }
    }
    if (fileName == null) {
      throw new IllegalStateException("Didn't find " + libName + " in " +
          Arrays.toString(libPaths));
    }

    // Then call an internal function that accepts the classloader. Do not use load(), as it
    // is deprecated and only there for backwards compatibility, and prints a warning to the
    // log that we'd have to strip (it contains the pid).
    Method m = Runtime.class.getDeclaredMethod("nativeLoad", String.class, ClassLoader.class);
    m.setAccessible(true);
    Object result = m.invoke(Runtime.getRuntime(), fileName, bootClassLoader);
    if (result != null) {
      throw new IllegalStateException(result.toString());
    }

    System.out.println("Success.");
  }
}
