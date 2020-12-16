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

  /// CHECK-START: int Main.inlineLoop() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineLoop() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineLoop() inliner (after)
  /// CHECK-DAG:     <<Constant:i\d+>>   IntConstant 42
  /// CHECK-DAG:                         Return [<<Constant>>]

  /// CHECK-START: int Main.inlineLoop() licm (after)
  /// CHECK:                         Goto loop:{{B\d+}}

  public static int inlineLoop() {
    return loopMethod();
  }

  /// CHECK-START: void Main.inlineWithinLoop() inliner (before)
  /// CHECK:      InvokeStaticOrDirect

  /// CHECK-START: void Main.inlineWithinLoop() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect

  /// CHECK-START: void Main.inlineWithinLoop() licm (after)
  /// CHECK-DAG:  Goto loop:<<OuterLoop:B\d+>> outer_loop:none
  /// CHECK-DAG:  Goto outer_loop:<<OuterLoop>>

  public static void inlineWithinLoop() {
    while (doLoop) {
      loopMethod();
    }
  }

  public static int loopMethod() {
    while (doLoop) {}
    return 42;
  }

  public static boolean doLoop = false;

  public static void main(String[] args) {
    inlineLoop();
    inlineWithinLoop();
  }
}
