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

class Unrelated {
}

public class Main {

  /// CHECK-START: int Main.inlineMonomorphic(Main) inliner (before)
  /// CHECK:       InvokeVirtual method_name:Main.getValue

  /// CHECK-START: int Main.inlineMonomorphic(Main) inliner (after)
  /// CHECK:   InvokeVirtual method_name:Main.getValue

  public static int inlineMonomorphic(Main a) {
    return a.getValue();
  }

  /// CHECK-START: int Main.inlinePolymorphic(Main) inliner (before)
  /// CHECK:       InvokeVirtual method_name:Main.getValue

  /// CHECK-START: int Main.inlinePolymorphic(Main) inliner (after)
  /// CHECK:   InvokeVirtual method_name:Main.getValue
  public static int inlinePolymorphic(Main a) {
    return a.getValue();
  }

  public int getValue() {
    return 42;
  }

  public static void main(String[] args) {
    inlineMonomorphic(new Main());
  }

}
