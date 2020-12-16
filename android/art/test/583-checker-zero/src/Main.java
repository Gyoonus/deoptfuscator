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
  // Test that by inlining new Float(-0f), we still keep the store of
  // -0f to the instance field. We used to remove it due to wrong assumptions
  // around art::HConstant::IsZero (now replaced with
  // art::HConstant::IsArithmeticZero and art::HConstant::IsZeroBitPattern).

  /// CHECK-START: void Main.main(java.lang.String[]) inliner (after)
  /// CHECK: InstanceFieldSet
  public static void main(String[] args) {
    if (new Float(0f).equals(new Float(-0f))) {
      throw new Error("Expected not equal");
    }
  }
}
