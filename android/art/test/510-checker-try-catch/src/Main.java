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

public class Main {

  // Workaround for b/18051191.
  class InnerClass {}

  public enum TestPath {
    ExceptionalFlow1(true, false, 3),
    ExceptionalFlow2(false, true, 8),
    NormalFlow(false, false, 42);

    TestPath(boolean arg1, boolean arg2, int expected) {
      this.arg1 = arg1;
      this.arg2 = arg2;
      this.expected = expected;
    }

    public boolean arg1;
    public boolean arg2;
    public int expected;
  }

  public static void testMethod(String method) throws Exception {
    Class<?> c = Class.forName("Runtime");
    Method m = c.getMethod(method, boolean.class, boolean.class);

    for (TestPath path : TestPath.values()) {
      Object[] arguments = new Object[] { path.arg1, path.arg2 };
      int actual = (Integer) m.invoke(null, arguments);

      if (actual != path.expected) {
        throw new Error("Method: \"" + method + "\", path: " + path + ", " +
                        "expected: " + path.expected + ", actual: " + actual);
      }
    }
  }

  public static void main(String[] args) throws Exception {
    testMethod("testUseAfterCatch_int");
    testMethod("testUseAfterCatch_long");
    testMethod("testUseAfterCatch_float");
    testMethod("testUseAfterCatch_double");
    testMethod("testCatchPhi_const");
    testMethod("testCatchPhi_int");
    testMethod("testCatchPhi_long");
    testMethod("testCatchPhi_float");
    testMethod("testCatchPhi_double");
    testMethod("testCatchPhi_singleSlot");
    testMethod("testCatchPhi_doubleSlot");
  }
}
