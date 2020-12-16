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

  /// CHECK-START: int Main.inlineIfThenElse() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineIfThenElse() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static int inlineIfThenElse() {
    return foo(true);
  }

  private static int foo(boolean value) {
    if (value) {
      return 1;
    } else {
      return 0;
    }
  }

  /// CHECK-START: int Main.inlineInLoop() inliner (before)
  /// CHECK-DAG:     InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineInLoop() inliner (after)
  /// CHECK-NOT:     InvokeStaticOrDirect

  public static int inlineInLoop() {
    int result = 0;
    for (int i = 0; i < 32; ++i) {
      result += foo(i % 2 == 0);
    }
    return result;
  }

  /// CHECK-START: int Main.inlineInLoopHeader() inliner (before)
  /// CHECK-DAG:     InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineInLoopHeader() inliner (after)
  /// CHECK-NOT:     InvokeStaticOrDirect

  public static int inlineInLoopHeader() {
    int result = 0;
    for (int i = 0; i < foo(i % 2 == 0); ++i) {
      result += 42;
    }
    return result;
  }

  public static void main(String[] args) {
    if (inlineIfThenElse() != 1) {
      throw new Error("Expected 1");
    }
    if (inlineInLoop() != 16) {
      throw new Error("Expected 16");
    }
    if (inlineInLoopHeader() != 42) {
      throw new Error("Expected 16");
    }
  }
}
