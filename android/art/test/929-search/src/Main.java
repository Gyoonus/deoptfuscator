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

import java.util.Arrays;

public class Main {
  public static void main(String[] args) throws Exception {
    doTest();
  }

  private static void doTest() throws Exception {
    doTest(true, DEX1, "B");
    doTest(false, DEX2, "A");
    System.out.println("Done");
  }

  private static void doTest(boolean boot, String segment, String className) throws Exception {
    ClassLoader expectedClassLoader;
    if (boot) {
      expectedClassLoader = Object.class.getClassLoader();
      addToBootClassLoader(segment);
    } else {
      expectedClassLoader = ClassLoader.getSystemClassLoader();
      addToSystemClassLoader(segment);
    }

    Class<?> c = Class.forName(className);
    if (c.getClassLoader() != expectedClassLoader) {
      throw new RuntimeException(className + "(" + boot + "/" + segment + "): " +
          c.getClassLoader() + " vs " + expectedClassLoader);
    }
  }

  private static native void addToBootClassLoader(String s);
  private static native void addToSystemClassLoader(String s);

  private static final String DEX1 = System.getenv("DEX_LOCATION") + "/929-search.jar";
  private static final String DEX2 = System.getenv("DEX_LOCATION") + "/929-search-ex.jar";
}
