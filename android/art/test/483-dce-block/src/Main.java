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
  public static void foo(Object o, int a) {
    Object result = null;
    if (o instanceof Main) {
      // The compiler optimizes the type of `o` by introducing
      // a `HBoundType` in this block.
      while (a != 3) {
        if (a == 2) {
          a++;
          result = o;
          continue;
        } else if (willInline()) {
          // This block will be detected as dead after inlining.
          result = new Object();
          continue;
        }
        result = new Object();
      }
      // The compiler produces a phi at the back edge for `result`.
      // Before dead block elimination, the phi has three inputs:
      // result = (new Object(), new Object(), HBoundType)
      //
      // After dead block elimination, the phi has now two inputs:
      // result = (new Object(), HBoundType)
      //
      // Our internal data structure for linking users and inputs expect
      // the input index stored in that data structure to be the index
      // in the inputs array. So the index before dead block elimination
      // of the `HBoundType` would be 2. Dead block elimination must update
      // that index to be 1.
    }
    System.out.println(result.getClass());
  }

  public static boolean willInline() {
    return false;
  }

  public static void main(String[] args) {
    foo(new Main(), 2);
  }
}
