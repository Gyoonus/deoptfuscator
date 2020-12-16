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

  public static void run() {
    // Loop enough to get JIT compilation.
    for (int i = 0; i < 10000; ++i) {
      doCall(new int[0]);
    }
  }

  public static void main(String[] args) throws Exception {
    run();
  }

  public static void doCall(int[] array) {
    try {
      deopt(array);
    } catch (IndexOutOfBoundsException ioobe) {
      // Expected
    }
  }

  public static void deopt(int[] array) {
    // Invoke `deopt` much more than `$inline$deopt` so that only `deopt` gets
    // initially JITted.
    if (call == 100) {
      call = 0;
      $inline$deopt(array);
    } else {
      call++;
    }
  }

  public static void $inline$deopt(int[] array) {
    array[0] = 1;
    array[1] = 1;
  }

  static int call = 0;
}
