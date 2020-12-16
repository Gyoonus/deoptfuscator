/*
 * Copyright (C) 2014 The Android Open Source Project
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
    crash();
    npe();
  }

  static void crash() {
    boolean b = baz();
    // Create many objects to starve registers.
    Main foo1 = create();
    Main foo2 = create();
    Main foo3 = create();
    Main foo4 = create();
    foo1.otherField = null;
    // On X86, we would force b to be in a byte register, which
    // would generate moves. This code exposed a bug in the
    // register allocator, where an input move was not just before
    // the instruction itself, and its destination was overridden
    // by another value.
    foo1.field = b;
    foo2.field = b;
    foo3.field = b;
    foo4.field = b;
    foo1.lastField = b;
  }

  // Similar to `crash` but generated an NPE.
  static void npe() {
    boolean b = baz();
    Main foo1 = create();
    Main foo2 = create();
    Main foo3 = create();
    Main foo4 = create();
    foo1.field = b;
    foo2.field = b;
    foo3.field = b;
    foo4.field = b;
    foo1.lastField = b;
  }

  static Main create() {
    return new Main();
  }

  static boolean baz() {
    return false;
  }

  boolean field;
  Object otherField;
  boolean lastField;
}
