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

public class OtherDex {
  public static void emptyMethod() {
  }

  public static int returnIntMethod() {
    return 38;
  }

  public static int returnOtherDexStatic() {
    return myStatic;
  }

  public static int returnMainStatic() {
    return Main.myStatic;
  }

  public static int recursiveCall() {
    return recursiveCall();
  }

  public static String returnString() {
    return "OtherDex";
  }

  public static Class<?> returnOtherDexClass() {
    return OtherDex.class;
  }

  public static Class<?> returnMainClass() {
    return Main.class;
  }

  private static Class<?> returnOtherDexClass2() {
    return OtherDex.class;
  }

  public static Class<?> returnOtherDexClassStaticCall() {
    // Do not call returnOtherDexClass, as it may have been flagged
    // as non-inlineable.
    return returnOtherDexClass2();
  }

  public static Class<?> returnOtherDexCallingMain() {
    return Main.getOtherClass();
  }

  static int myStatic = 1;
}
