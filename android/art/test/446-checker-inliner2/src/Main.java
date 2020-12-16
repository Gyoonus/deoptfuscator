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

  /// CHECK-START: int Main.inlineInstanceCall(Main) inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineInstanceCall(Main) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineInstanceCall(Main) inliner (after)
  /// CHECK-DAG:     <<Field:i\d+>>   InstanceFieldGet
  /// CHECK-DAG:                      Return [<<Field>>]

  public static int inlineInstanceCall(Main m) {
    return m.foo();
  }

  private int foo() {
    return field;
  }

  int field = 42;

  /// CHECK-START: int Main.inlineNestedCall() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineNestedCall() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineNestedCall() inliner (after)
  /// CHECK-DAG:     <<Const38:i\d+>> IntConstant 38
  /// CHECK-DAG:                      Return [<<Const38>>]

  public static int inlineNestedCall() {
    return nestedCall();
  }

  public static int nestedCall() {
    return bar();
  }

  public static int bar() {
    return 38;
  }

  public static void main(String[] args) {
    if (inlineInstanceCall(new Main()) != 42) {
      throw new Error("Expected 42");
    }

    if (inlineNestedCall() != 38) {
      throw new Error("Expected 38");
    }
  }
}
