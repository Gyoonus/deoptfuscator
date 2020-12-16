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
    try {
      doTopCall(true);
    } catch (Error e) {
      e.printStackTrace(System.out);
    }
  }

  /// CHECK-START: void Main.doTopCall(boolean) inliner (before)
  /// CHECK-NOT:   InvokeStaticOrDirect method_load_kind:Recursive

  /// CHECK-START: void Main.doTopCall(boolean) inliner (after)
  /// CHECK:       InvokeStaticOrDirect method_load_kind:Recursive
  public static void doTopCall(boolean first_call) {
    if (first_call) {
      inline1();
    } else {
      while (true) {
        inline3();
      }
    }
  }

  public static void inline1() {
    inline2();
  }

  public static void inline2() {
    doTopCall(false);
  }

  public static void inline3() {
    throw new Error();
  }
}
