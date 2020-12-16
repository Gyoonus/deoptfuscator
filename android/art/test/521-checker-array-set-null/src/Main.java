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
  public static void main(String[] args) {
    testWithNull(new Object[2]);
    testWithUnknown(new Object[2], new Object());
    testWithSame(new Object[2]);
  }

  /// CHECK-START: void Main.testWithNull(java.lang.Object[]) disassembly (after)
  /// CHECK-NOT:      pAputObject
  public static void testWithNull(Object[] o) {
    o[0] = null;
  }

  /// CHECK-START: void Main.testWithUnknown(java.lang.Object[], java.lang.Object) disassembly (after)
  /// CHECK:          pAputObject
  public static void testWithUnknown(Object[] o, Object obj) {
    o[0] = obj;
  }

  /// CHECK-START: void Main.testWithSame(java.lang.Object[]) disassembly (after)
  /// CHECK-NOT:      pAputObject
  public static void testWithSame(Object[] o) {
    o[0] = o[1];
  }
}
