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

public class Main {

  public static void main(String[] args) {
    try {
      foo(intField);
      throw new Error("Expected ArithmeticException");
    } catch (ArithmeticException e) {
      // expected
    }
  }

  /// CHECK-START: int Main.foo(int) register (after)
  /// CHECK: <<BoolField:z\d+>> StaticFieldGet
  /// CHECK:                    DivZeroCheck [<<BoolField>>]
  public static int foo(int a) {
    return a / bar();
  }

  public static int bar() {
    return booleanField ? 1 : 0;
  }

  public static boolean booleanField;
  public static int intField;
}
