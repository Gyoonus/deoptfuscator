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

  /// CHECK-START: Main Main.keepTest(Main) instruction_simplifier (before)
  /// CHECK:         NullCheck
  /// CHECK:         InvokeStaticOrDirect

  /// CHECK-START: Main Main.keepTest(Main) instruction_simplifier (after)
  /// CHECK:         NullCheck
  /// CHECK:         InvokeStaticOrDirect
  public Main keepTest(Main m) {
    return m.g();
  }

  /// CHECK-START: Main Main.thisTest() builder (after)
  /// CHECK-NOT:     NullCheck
  /// CHECK:         InvokeStaticOrDirect
  public Main thisTest() {
    return g();
  }

  /// CHECK-START: Main Main.newInstanceRemoveTest() builder (after)
  /// CHECK:         NewInstance
  /// CHECK:         InvokeStaticOrDirect
  /// CHECK:         InvokeStaticOrDirect

  /// CHECK-START: Main Main.newInstanceRemoveTest() builder (after)
  /// CHECK-NOT:     NullCheck
  public Main newInstanceRemoveTest() {
    Main m = new Main();
    return m.g();
  }

  /// CHECK-START: Main Main.newArrayRemoveTest() builder (after)
  /// CHECK:         NewArray
  /// CHECK:         ArrayGet

  /// CHECK-START: Main Main.newArrayRemoveTest() builder (after)
  /// CHECK-NOT:     NullCheck
  public Main newArrayRemoveTest() {
    Main[] ms = new Main[1];
    return ms[0];
  }

  /// CHECK-START: Main Main.ifRemoveTest(boolean) instruction_simplifier (before)
  /// CHECK:         NewInstance
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.ifRemoveTest(boolean) instruction_simplifier (after)
  /// CHECK:         NewInstance
  /// CHECK-NOT:     NullCheck
  public Main ifRemoveTest(boolean flag) {
    Main m = null;
    if (flag) {
      m = new Main();
    } else {
      m = new Main(1);
    }
    return m.g();
  }

  /// CHECK-START: Main Main.ifKeepTest(boolean) instruction_simplifier (before)
  /// CHECK:         NewInstance
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.ifKeepTest(boolean) instruction_simplifier (after)
  /// CHECK:         NewInstance
  /// CHECK:         NullCheck
  public Main ifKeepTest(boolean flag) {
    Main m = null;
    if (flag) {
      m = new Main(1);
    }
    return m.g();
  }

  /// CHECK-START: Main Main.forRemoveTest(int) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.forRemoveTest(int) instruction_simplifier (after)
  /// CHECK-NOT:     NullCheck
  public Main forRemoveTest(int count) {
    Main a = new Main();
    Main m = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = a;
      }
    }
    return m.g();
  }

  /// CHECK-START: Main Main.forKeepTest(int) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.forKeepTest(int) instruction_simplifier (after)
  /// CHECK:         NullCheck
  public Main forKeepTest(int count) {
    Main a = new Main();
    Main m = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = a;
      } else {
        m = null;
      }
    }
    return m.g();
  }

  /// CHECK-START: Main Main.phiFlowRemoveTest(int) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.phiFlowRemoveTest(int) instruction_simplifier (after)
  /// CHECK-NOT:     NullCheck
  public Main phiFlowRemoveTest(int count) {
    Main a = new Main();
    Main m = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = a;
      }
    }
    Main n = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 3 == 0) {
        n = m;
      }
    }
    return n.g();
  }

  /// CHECK-START: Main Main.phiFlowKeepTest(int) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.phiFlowKeepTest(int) instruction_simplifier (after)
  /// CHECK:         NullCheck
  public Main phiFlowKeepTest(int count) {
    Main a = new Main();
    Main m = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = a;
      } else {
        m = null;
      }
    }
    Main n = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 3 == 0) {
        n = m;
      }
    }
    return n.g();
  }

  /// CHECK-START: Main Main.scopeRemoveTest(int, Main) builder (after)
  /// CHECK-NOT:     NullCheck
  public Main scopeRemoveTest(int count, Main a) {
    Main m = null;
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = new Main();
        m.g();
      } else {
        m = a;
      }
    }
    return m;
  }

  /// CHECK-START: Main Main.scopeKeepTest(int, Main) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.scopeKeepTest(int, Main) instruction_simplifier (after)
  /// CHECK:         NullCheck
  public Main scopeKeepTest(int count, Main a) {
    Main m = new Main();
    for (int i = 0; i < count; i++) {
      if (i % 2 == 0) {
        m = a;
      } else {
        m = a;
        m.g();
      }
    }
    return m;
  }

  /// CHECK-START: Main Main.scopeIfNotNullRemove(Main) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.scopeIfNotNullRemove(Main) instruction_simplifier (after)
  /// CHECK-NOT:     NullCheck
  public Main scopeIfNotNullRemove(Main m) {
    if (m != null) {
      return m.g();
    }
    return m;
  }

  /// CHECK-START: Main Main.scopeIfKeep(Main) instruction_simplifier (before)
  /// CHECK:         NullCheck

  /// CHECK-START: Main Main.scopeIfKeep(Main) instruction_simplifier (after)
  /// CHECK:         NullCheck
  public Main scopeIfKeep(Main m) {
    if (m == null) {
      m = new Main();
    }
    return m.g();
  }

  public Main() {}
  public Main(int dummy) {}

  private Main g() {
    // avoids inlining
    throw new RuntimeException();
  }

  public static void main(String[] args) {
    new Main();
  }

}

// Regression for when we created and kept equivalent phis with the same type.
// The phi used in comparison would be different then the one used for access
// so we could not safely discard it.
class ListElement {
  private ListElement next;

  /// CHECK-START: boolean ListElement.isShorter(ListElement, ListElement) instruction_simplifier (before)
  /// CHECK:         NullCheck
  /// CHECK:         NullCheck

  /// CHECK-START: boolean ListElement.isShorter(ListElement, ListElement) instruction_simplifier (after)
  /// CHECK-NOT:     NullCheck
  static boolean isShorter(ListElement x, ListElement y) {
    ListElement xTail = x;
    ListElement yTail = y;
    while (yTail != null) {
      if (xTail == null) return true;
      xTail = xTail.next;
      yTail = yTail.next;
    }
    return false;
  }
}
