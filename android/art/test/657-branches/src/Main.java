/*
 * Copyright (C) 2017 The Android Open Source Project
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

  public static void foo(float f) {
    // The reason this used to break:
    // 1) We inline the 'foo' call, so blocks now only contain HLoadClass instructions.
    // 2) We then run the select_generator pass, which cannot change the
    //    if/else because blocks contain instructions.
    // 3) We run GVN which will remove the HLoadClass instructions in the blocks.
    // 4) At code generation, we are in the unlikely situation that a diamond shape
    //    contains no instruction (usually removed by select_generator). This used
    //    to trip the ARM code generators.
    if (f < 1.2f) {
      foo(Main.class, Object.class);
      if (f < 0.2f) {
        foo(Main.class, Object.class);
      } else {
        foo(Main.class, Object.class);
      }
    } else {
      System.out.println("Hello World: " + f);
    }
  }

  public static void foo(Object a, Object b) {}

  public static void main(String[] args) {
    foo(0f);
    foo(4f);
    foo(0.1f);
  }
}
