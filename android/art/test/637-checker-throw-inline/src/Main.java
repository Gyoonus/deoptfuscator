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

  public static void $inline$doCall() {
    if (doThrow) throw new Error("");
  }

  public static void tryInline() {
    if (doThrow) throw new Error("");
  }

  /// CHECK-START: void Main.test() inliner (before)
  /// CHECK:      InvokeStaticOrDirect method_name:Main.$inline$doCall loop:none

  /// CHECK-START: void Main.test() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect method_name:Main.$inline$doCall
  public static void test() {
    $inline$doCall();
  }

  /// CHECK-START: void Main.testInLoop() inliner (before)
  /// CHECK:      InvokeStaticOrDirect method_name:Main.$inline$doCall loop:{{B\d+}}

  /// CHECK-START: void Main.testInLoop() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect method_name:Main.$inline$doCall
  public static void testInLoop() {
    for (int i = 0; i < 10; ++i) {
      $inline$doCall();
    }
  }

  /// CHECK-START: void Main.testInInfiniteLoop() inliner (before)
  /// CHECK:      InvokeStaticOrDirect method_name:Main.tryInline loop:{{B\d+}}

  /// CHECK-START: void Main.testInInfiniteLoop() inliner (after)
  /// CHECK:      InvokeStaticOrDirect method_name:Main.tryInline loop:{{B\d+}}
  public static void testInInfiniteLoop() {
    while (true) {
      tryInline();
    }
  }

  public static void main(String[] args) {
    test();
    testInLoop();
  }

  static boolean doThrow = false;
}
