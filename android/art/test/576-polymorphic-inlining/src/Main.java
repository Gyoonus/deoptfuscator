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
  public static void main(String[] args) {
    for (int i = 0; i < 20000; ++i) {
      $noinline$testVoid(new Main());
      $noinline$testVoid(new SubMain());
      $noinline$testVoid(new SubSubMain());

      $noinline$testWithReturnValue(new Main());
      $noinline$testWithReturnValue(new SubMain());
      $noinline$testWithReturnValue(new SubSubMain());

      $noinline$testWithBackEdge(new Main());
      $noinline$testWithBackEdge(new SubMain());
      $noinline$testWithBackEdge(new SubSubMain());
    }
  }

  public static void assertIdentical(Object expected, Object actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void $noinline$testVoid(Main m) {
    if (doThrow) throw new Error("");
    m.willInlineVoid();
    m.willOnlyInlineForMainVoid();
  }

  public static void $noinline$testWithReturnValue(Main m) {
    if (doThrow) throw new Error("");
    assertIdentical(m.getClass(), m.willInlineWithReturnValue());
    assertIdentical(m.getClass(), m.willOnlyInlineForMainWithReturnValue());
  }

  public static void $noinline$testWithBackEdge(Main m) {
    if (doThrow) throw new Error("");
    for (int i = 0; i < 10; ++i) {
      m.willInlineVoid();
    }
    for (int i = 0; i < 10; ++i) {
      m.willOnlyInlineForMainVoid();
    }
  }

  public void willInlineVoid() {
  }

  public void willOnlyInlineForMainVoid() {
  }

  public Class<?> willInlineWithReturnValue() {
    return Main.class;
  }

  public Class<?> willOnlyInlineForMainWithReturnValue() {
    return Main.class;
  }
  public static boolean doThrow;
}

class SubMain extends Main {
  public void willOnlyInlineForMainVoid() {
    if (doThrow) throw new Error("");
  }

  public void willInlineVoid() {
  }

  public Class<?> willInlineWithReturnValue() {
    return SubMain.class;
  }

  public Class<?> willOnlyInlineForMainWithReturnValue() {
    return SubMain.class;
  }
}

class SubSubMain extends SubMain {
  public Class<?> willInlineWithReturnValue() {
    return SubSubMain.class;
  }

  public Class<?> willOnlyInlineForMainWithReturnValue() {
    return SubSubMain.class;
  }
}
