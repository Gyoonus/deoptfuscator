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

  public static Object $opt$CheckCastMain() {
    return (Main)a;
  }

  public static Object $opt$CheckCastFinalClass() {
    return (FinalClass)a;
  }

  public static void main(String[] args) {
    $opt$TestMain();
    $opt$TestFinalClass();
  }

  public static void $opt$TestMain() {
    a = new Main();
    $opt$CheckCastMain();

    a = null;
    $opt$CheckCastMain();

    a = new MainChild();
    $opt$CheckCastMain();

    a = new Object();
    try {
      $opt$CheckCastMain();
      throw new Error("Should have gotten a ClassCastException");
    } catch (ClassCastException ex) {}
  }

  public static void $opt$TestFinalClass() {
    a = new FinalClass();
    $opt$CheckCastFinalClass();

    a = null;
    $opt$CheckCastFinalClass();

    a = new Main();
    try {
      $opt$CheckCastFinalClass();
      throw new Error("Should have gotten a ClassCastException");
    } catch (ClassCastException ex) {}

    a = new Object();
    try {
      $opt$CheckCastFinalClass();
      throw new Error("Should have gotten a ClassCastException");
    } catch (ClassCastException ex) {}
  }

  static class MainChild extends Main {}

  static final class FinalClass {}
}
