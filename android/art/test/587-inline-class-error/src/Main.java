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

import java.lang.reflect.InvocationTargetException;

public class Main {
  public static void main(String[] args) throws Exception {
    try {
      Class<?> v = Class.forName("VerifyError");
      throw new Error("Expected LinkageError");
    } catch (LinkageError e) {
      // expected
    }

    try {
      Class.forName("TestCase").getMethod("topLevel").invoke(null);
      throw new Error("Expected InvocationTargetException");
    } catch (InvocationTargetException e) {
      if (!(e.getCause() instanceof NullPointerException)) {
        throw new Error("Expected NullPointerException, got " + e.getCause());
      }
    }
  }
}
