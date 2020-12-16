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

  private static int willInline(int a, int b) {
    return a & b;
  }

  static int[] a = new int[4];
  static int field = 42;

  public static void main(String[] args) throws Exception {
    // The order of optimizations that would lead to the problem was:
    // 1) Inlining of `willInline`.
    // 2) Bounds check elimination inserting a deopt at a[0] and removing the HBoundsCheck.
    // 3) Instruction simplifier simpilifying the inlined willInline to just `field`.
    //
    // At this point, if the environment of the HDeoptimization instruction was
    // just a pointer to the one in a[0], the uses lists would have not been updated
    // and the HBoundsCheck being dead code after the HDeoptimization, the simplifcation
    // at step 3) would not updated that environment.
    int inEnv = willInline(field, field);
    int doAdds = a[0] + a[1] + a[2] + a[3];

    if (inEnv != 42) {
      throw new Error("Expected 42");
    }

    if (doAdds != 0) {
      throw new Error("Expected 0");
    }
  }
}
