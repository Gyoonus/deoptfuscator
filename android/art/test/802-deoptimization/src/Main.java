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
  private static final int EXPECTED_RESULT = 100;
  private static final int PARAMETER_VALUE = 0;

  public static void main(String[] args) throws Throwable {
    testCatchHandlerOnEntryWithoutMoveException();
  }

  /**
   * Tests we correctly execute a method starting with a catch handler without
   * move-exception instruction when throwing an exception during deoptimization.
   */
  private static void testCatchHandlerOnEntryWithoutMoveException() throws Throwable {
    Class<?> c = Class.forName("CatchHandlerOnEntry");
    Method m = c.getMethod("catchHandlerOnEntry", int.class);
    Object result = m.invoke(null, new Object[]{PARAMETER_VALUE});
    int intResult = ((Integer) result).intValue();
    if (intResult == EXPECTED_RESULT) {
      System.out.println("CatchHandlerOnEntryWithoutMoveException OK");
    } else {
      System.out.println("CatchHandlerOnEntryWithoutMoveException KO: result==" + intResult);
    }
  }
}

