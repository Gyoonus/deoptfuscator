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

public final class Main {

  /// CHECK-START: void Main.main(String[]) builder (after)
  /// CHECK: StaticFieldGet klass:Main[] exact: true
  /// CHECK: ArrayGet klass:Main exact:true
  /// CHECK: BoundType klass:Main exact:true
  public static void main(String[] args) {
    Object o = null;
    Main f = a[0];
    for (int i = 0; i < 2; ++i) {
      // We used to crash in the fixed point iteration of
      // the reference type propagation while handling the instanceof:
      // we were expecting `o` to get the same exact-ness as the
      // `HBoundType` but the typing of the `ArrayGet` used to not
      // propagate the exact-ness.
      if (o instanceof Main) {
        field = o;
      }
      o = f;
    }
    if (field != null) {
      throw new Error("Expected null");
    }
  }

  static Main[] a = new Main[1];
  static Object field;
}
