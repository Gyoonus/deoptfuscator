/*
 * Copyright (C) 2018 The Android Open Source Project
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

import dalvik.system.PathClassLoader;
import java.io.File;

public class Main {
  public static void main(String[] args) throws Exception {
    ClassLoader parentLoader = Main.class.getClassLoader();
    ClassLoader childLoader = new PathClassLoader(DEX_CHILD, parentLoader);
    Class.forName("ChildClass", true, childLoader).getDeclaredMethod("runTest").invoke(null);
  }

  private static final String DEX_CHILD =
      new File(System.getenv("DEX_LOCATION"), "676-resolve-field-type-ex.jar").getAbsolutePath();

  public static void staticMethod() {}
}
