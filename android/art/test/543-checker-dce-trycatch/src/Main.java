/*
 * Copyright (C) 2015 The Android Open Source Project
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

  // Workaround for b/18051191.
  class InnerClass {}

  static boolean $inline$False() { return false; }

  // DCE should only merge blocks where the first ends with a Goto.
  // SSAChecker will fail if the following Throw->TryBoundary blocks are merged.
  public static void doNotMergeThrow(String str) {
    try {
      throw new Exception(str);
    } catch (Exception ex) {
      return;
    }
  }

  // Test deletion of all try/catch blocks. Multiple catch blocks test deletion
  // where TryBoundary still has exception handler successors after having removed
  // some already.

  /// CHECK-START: void Main.testDeadTryCatch(boolean) dead_code_elimination$after_inlining (after)
  /// CHECK-NOT: TryBoundary

  /// CHECK-START: void Main.testDeadTryCatch(boolean) dead_code_elimination$after_inlining (after)
  /// CHECK: begin_block
  /// CHECK: begin_block
  /// CHECK: begin_block
  /// CHECK-NOT: begin_block

  public static void testDeadTryCatch(boolean val) {
    if ($inline$False()) {
      try {
        if (val) {
          throw new ArithmeticException();
        } else {
          throw new ArrayIndexOutOfBoundsException();
        }
      } catch (ArithmeticException ex) {
        System.out.println("Unexpected AE catch");
      } catch (ArrayIndexOutOfBoundsException ex) {
        System.out.println("Unexpected AIIOB catch");
      }
    }
  }

  public static void main(String[] args) {

  }
}
