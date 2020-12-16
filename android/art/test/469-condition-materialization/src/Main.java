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

  // This tests a specific situation when condition ends up
  // not getting materialized if only used by an environment.

  private static Object obj;

  private static int useValue(boolean value) {
    return 42;
  }

  private static int runTest(boolean input1) {
    boolean negation = !input1;
    // Need the negation to appear in front of an If, and
    // its condition to disappear. 'javac' will generate
    // "if (!input1)" here and GVN will collapse the two
    // conditions.
    if (input1) {
      // Generates an environment use of 'negation'.
      obj = new Object();
    }
    // Uses 'negation' but disappears with inlining.
    return useValue(negation);
  }

  public static void main(String[] args) throws Exception {
    int result = runTest(true);
    if (result != 42) {
      throw new Error("Expected 42, got " + result);
    }
  }
}
