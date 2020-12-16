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
  public static void $opt$bar() {
    try {
      $opt$foo(1);
    } catch (NullPointerException e) {
      $opt$foo(2);
    } catch (RuntimeException e) {
      $opt$foo(3);
    } finally {
      $opt$foo(4);
    }
  }

  static int barState;
  static int fooState;

  public static void main(String[] args) {
    fooState = 0;
    $opt$runTest();
    fooState = 1;
    $opt$runTest();
  }

  public static void $opt$runTest() {
    barState = 1;
    $opt$bar();
    barState = 2;
    $opt$bar();
    barState = 3;
    $opt$bar();
    barState = 4;
    try {
      $opt$bar();
    } catch (RuntimeException e) {
      System.out.println("Caught " + e.getClass());
    }
  }

  public static void $opt$foo(int value) {
    System.out.println(value);
    if (value == barState) {
      if (fooState == 0) {
        throw new RuntimeException();
      } else {
        throw new NullPointerException();
      }
    }
  }
}
