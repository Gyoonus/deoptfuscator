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

public class Main {

  public static void main(String[] args) {
    checkClinitCheckBeforeStaticMethodInvoke();
  }

  static void checkClinitCheckBeforeStaticMethodInvoke() {
    System.out.println("checkClinitCheckBeforeStaticMethodInvoke START");

    // Call static method to cause implicit class initialization, even
    // if it is inlined.
    ClassWithClinit.$opt$inline$StaticMethod();
    if (!classWithClinitInitialized) {
      System.out.println("checkClinitCheckBeforeStaticMethodInvoke FAILED");
      return;
    }

    System.out.println("checkClinitCheckBeforeStaticMethodInvoke PASSED");
  }

  static class ClassWithClinit {
    static {
      Main.classWithClinitInitialized = true;
    }

    static void $opt$inline$StaticMethod() {
    }
  }

  static boolean classWithClinitInitialized = false;
}
