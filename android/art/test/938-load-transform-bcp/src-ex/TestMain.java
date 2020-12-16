/*
 * Copyright (C) 2017 The Android Open Source Project
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
import java.util.OptionalLong;
public class TestMain {
  public static void runTest() {
    // This should be our redefined OptionalLong.
    OptionalLong ol = OptionalLong.of(123);
    try {
      // OptionalLong is a class that is unlikely to be used by the time this test starts.
      Method foo = OptionalLong.class.getMethod("foo");
      System.out.println("ol.foo() -> '" + (String)foo.invoke(ol) + "'");
      System.out.println("ol.toString() -> '" + ol.toString() + "'");
    } catch (Exception e) {
      System.out.println(
          "Exception occured (did something load OptionalLong before this test method!: "
          + e.toString());
      e.printStackTrace(System.out);
    }
  }
}
