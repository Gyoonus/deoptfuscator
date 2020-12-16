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

  // Check that there is no instruction storing to stack.
  /// CHECK-START-X86: int Main.foo(int, int, int, int, int, int) disassembly (after)
  /// CHECK-NOT:  mov [{{\w+}}], {{\w+}}

  // Use enough parameters to ensure we'll need a frame.
  public static int foo(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
  }

  public static void main(String[] args) {
    if (foo(1, 2, 3, 4, 5, 6) != 21) {
      throw new Error("Expected 21");
    }
  }
}
