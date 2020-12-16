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

  public static void main(String[] args){
    foo();
  }

  // Reduced test case inspired by constantPropagationTest() from
  // test/083-compiler-regressions.
  static void foo() {
    int a = 0;
    int b = 1;

    for (int i = 0; i < 3; i++) {
      try {
        a = 1;
        // Would throw an ArithmeticException if b were null (hence
        // the enclosing `try' statement).
        int c = a % b;
      }
      finally {
        System.out.println("In finally");
      }
    }
  }
}
