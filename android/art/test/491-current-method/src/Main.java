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

class Main {

  // The code below is written in a way that would crash
  // the generated code at the time of submission of this test.
  // Therefore, changes to the register allocator may
  // affect the reproducibility of the crash.
  public static void $noinline$foo(int a, int b, int c) {
    // The division on x86 will take EAX and EDX, leaving ECX
    // to put the ART current method.
    c = c / 42;
    // We use the empty string for forcing the slow path.
    // The slow path for charAt, when it is intrinsified, will
    // move the parameter to ECX and therefore overwrite the ART
    // current method.
    "".charAt(c);

    // Do more things in the method to prevent inlining.
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
    c = c / 42;
    "".charAt(c);
  }

  public static void main(String[] args) {
    boolean didThrow = false;
    try {
      $noinline$foo(1, 2, 3);
    } catch (Throwable e) {
      didThrow = true;
    }

    if (!didThrow) {
      throw new Error("Expected an exception from charAt");
    }
  }
}
