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

  /// CHECK-START: void Main.testRedundantPhiCycle(boolean) builder (after)
  /// CHECK-NOT:  Phi
  private void testRedundantPhiCycle(boolean cond) {
    Object o = null;
    while (true) {
      if (cond) {
        o = null;
      }
      System.out.println(o);
    }
  }

  /// CHECK-START: void Main.testLoopPhisWithNullAndCrossUses(boolean) builder (after)
  /// CHECK-NOT:  Phi
  private void testLoopPhisWithNullAndCrossUses(boolean cond) {
    Main a = null;
    Main b = null;
    while (a == null) {
      if (cond) {
        a = b;
      } else {
        b = a;
      }
    }
  }

  public static void main(String[] args) {
  }
}
