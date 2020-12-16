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


/*
 * A test that checks that the inliner does not inline functions that contain
 * a loop with no exit.  This because the incremental update to
 * HLoopInformation done by the inliner does not work with the LinearOrder
 * computation if the inlined function does not always return.
 */

public class Main {

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static int $opt$noinline$Function(int x, int y) {
    int result;
    if (x <= y) {
      result = 42;
    } else {
      while (true);
    }
    return result;
  }

  /// CHECK-START: int Main.callerLoop(int, int) inliner (before)
  /// CHECK:         InvokeStaticOrDirect method_name:Main.$opt$noinline$Function  loop:{{B\d+}}

  /// CHECK-START: int Main.callerLoop(int, int) inliner (after)
  /// CHECK:         InvokeStaticOrDirect method_name:Main.$opt$noinline$Function  loop:{{B\d+}}

  public static int callerLoop(int max_x, int max_y) {
    int total = 0;
    for (int x = 0; x < max_x; ++x) {
      total += $opt$noinline$Function(x, max_y);
    }
    return total;
  }

  public static void main(String[] args) {
    assertIntEquals(42, callerLoop(1, 1));
  }
}
