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

  static boolean doThrow = false;

  private void inlinedForNull(Iterable it) {
    if (it != null) {
      // We're not inlining throw at the moment.
      if (doThrow) { throw new Error(""); }
    }
  }

  private void inlinedForFalse(boolean value, Iterable it) {
    if (value) {
      // We're not inlining throw at the moment.
      if (doThrow) { throw new Error(""); }
    }
  }

  /// CHECK-START: void Main.testInlinedForFalseInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForFalseInlined(java.lang.Iterable) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface

  public void testInlinedForFalseInlined(Iterable it) {
    inlinedForFalse(false, it);
  }

  /// CHECK-START: void Main.testInlinedForFalseNotInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForFalseNotInlined(java.lang.Iterable) inliner (after)
  /// CHECK:                          InvokeStaticOrDirect

  public void testInlinedForFalseNotInlined(Iterable it) {
    inlinedForFalse(true, it);
  }

  /// CHECK-START: void Main.testInlinedForNullInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForNullInlined(java.lang.Iterable) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface

  public void testInlinedForNullInlined(Iterable it) {
    inlinedForNull(null);
  }

  /// CHECK-START: void Main.testInlinedForNullNotInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForNullNotInlined(java.lang.Iterable) inliner (after)
  /// CHECK:                          InvokeStaticOrDirect

  public void testInlinedForNullNotInlined(Iterable it) {
    inlinedForNull(it);
  }

  public static void main(String[] args) {
    Main m = new Main();
    Iterable it = new Iterable() {
      public java.util.Iterator iterator() { return null; }
    };
    m.testInlinedForFalseInlined(it);
    m.testInlinedForFalseNotInlined(it);
    m.testInlinedForNullInlined(it);
    m.testInlinedForNullNotInlined(it);
  }
}
