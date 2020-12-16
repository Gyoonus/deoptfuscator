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

import java.lang.reflect.Method;

public class Main  {

  class MyClassA {}
  class MyClassB extends MyClassA {}

  public static void main(String[] args) throws Exception {
    testReferenceTypePropagation();
    invokeTestInliner();
  }

  // Reference type propagation (RTP) used to assume that if a class is final,
  // then the type must be exact. This does not hold for arrays which are always
  // final, i.e. not extendable, but may be assigned to from values of the
  // components type subclasses.

  public static void testReferenceTypePropagation() throws Exception {
    boolean expectTrue;

    // Bug #1: RTP would set the type of `array` to exact Object[]. Instruction
    // simplifier would then simplify the instanceof to `false`.
    Object[] array = $noinline$getArray();
    expectTrue = array instanceof MyClassA[];
    if (!expectTrue) {
      throw new Exception("Incorrect type check.");
    }

    // Bug #2: This is the true-branch of the instanceof above. The bound type
    // for `array` would be again set to exact MyClassA[] and incorrectly
    // simplify the second instanceof to `false`.
    expectTrue = array instanceof MyClassB[];
    if (!expectTrue) {
      throw new Exception("Incorrect type bound.");
    }
  }

  public static void invokeTestInliner() throws Exception {
    Class<?> c = Class.forName("TestCase");
    Method m = c.getMethod("testInliner");
    m.invoke(null);
  }

  public static Object[] $noinline$getArray() {
    if (doThrow) throw new Error();
    return new MyClassB[2];
  }

  static boolean doThrow = false;
}
