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
  public static int foo(int start, int[] array) {
    int result = 0;
    // We will create HDeoptimize nodes for this first loop, and a phi
    // for the array length which will only be used within the loop.
    for (int i = start; i < 3; i++) {
      result += array[i];
      for (int j = 0; j < 2; ++j) {
        // The HBoundsCheck for this array access will be updated to access
        // the array length phi created for the deoptimization checks of the
        // first loop. This crashed the compiler which used to DCHECK an array
        // length in a bounds check cannot be a phi.
        result += array[j];
      }
    }
    return result;
  }

  public static int bar(int start, int[] array) {
    int result = 0;
    for (int i = start; i < 3; i++) {
      result += array[i];
      for (int j = 0; j < 2; ++j) {
        result += array[j];
        // The following operations would lead to BCE wanting to add another
        // deoptimization, but it crashed assuming the input of a `HBoundsCheck`
        // must be a `HArrayLength`.
        result += array[0];
        result += array[1];
        result += array[2];
      }
    }
    return result;
  }

  public static void main(String[] args) {
    int[] a = new int[] { 1, 2, 3, 4, 5 };
    int result = foo(1, a);
    if (result != 11) {
      throw new Error("Got " + result + ", expected " + 11);
    }

    result = bar(1, a);
    if (result != 35) {
      throw new Error("Got " + result + ", expected " + 35);
    }
  }
}
