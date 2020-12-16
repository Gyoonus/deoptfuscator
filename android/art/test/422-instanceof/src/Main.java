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

public class Main {
  public static Object a;

  public static void assertTrue(boolean value) {
    if (!value) {
      throw new Error("Wrong result");
    }
  }

  public static void assertFalse(boolean value) {
    if (value) {
      throw new Error("Wrong result");
    }
  }

  public static boolean $opt$InstanceOfMain() {
    return a instanceof Main;
  }

  public static boolean $opt$InstanceOfFinalClass() {
    return a instanceof FinalClass;
  }

  public static void main(String[] args) {
    $opt$TestMain();
    $opt$TestFinalClass();
  }

  public static void $opt$TestMain() {
    a = new Main();
    assertTrue($opt$InstanceOfMain());
    a = null;
    assertFalse($opt$InstanceOfMain());
    a = new MainChild();
    assertTrue($opt$InstanceOfMain());
    a = new Object();
    assertFalse($opt$InstanceOfMain());
  }

  public static void $opt$TestFinalClass() {
    a = new FinalClass();
    assertTrue($opt$InstanceOfFinalClass());
    a = null;
    assertFalse($opt$InstanceOfFinalClass());
    a = new Main();
    assertFalse($opt$InstanceOfFinalClass());
    a = new Object();
    assertFalse($opt$InstanceOfFinalClass());
  }

  static class MainChild extends Main {}

  static final class FinalClass {}
}
