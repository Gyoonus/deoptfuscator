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

public class LoadedByAppClassLoader {
  public static void letMeInlineYou(A a) {
    a.foo();
  }

  public static ClassLoader areYouB() {
    // Ensure letMeInlineYou is JITted and tries to do inlining of A.foo.
    // The compiler used to wrongly update the dex cache of letMeInlineYou's
    // class loader.
    Main.ensureJitCompiled(LoadedByAppClassLoader.class, "letMeInlineYou");
    return OtherClass.getB().getClassLoader();
  }
}

class OtherClass {
  public static Class<?> getB() {
    // This used to return the B class of another class loader.
    return B.class;
  }
}
