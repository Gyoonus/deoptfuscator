/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.*;

public class Main {

  public static void main(String args[]) throws Throwable {
    Class<?> c = Class.forName("Merge");
    Method m = c.getMethod("select", boolean.class);
    Object x = m.invoke(null, true);
    if (x == null) {
      throw new Error("Did not get array");
    }
    System.out.println("passed");
  }
}
