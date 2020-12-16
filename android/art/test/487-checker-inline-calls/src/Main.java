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
      doTopCall();
    } catch (Error e) {
      e.printStackTrace(System.out);
    }
  }

  // We check that some inlining happened by checking the
  // method index of the called method.

  /// CHECK-START: void Main.doTopCall() inliner (before)
  /// CHECK:     InvokeStaticOrDirect dex_file_index:2

  /// CHECK-START: void Main.doTopCall() inliner (after)
  /// CHECK:     InvokeStaticOrDirect dex_file_index:4
  public static void doTopCall() {
    inline1();
  }

  public static void inline1() {
    inline2();
  }

  public static void inline2() {
    inline3();
  }

  public static void inline3() {
    throw new Error();
  }
}
