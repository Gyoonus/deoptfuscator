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
import java.lang.reflect.Method;

public class Main {

  public static void main(String[] args) throws Exception {
    Class<?> c = Class.forName("java.lang.StringFactory");
    Method m = c.getDeclaredMethod("newStringFromBytes", byte[].class, int.class);

    // Loop over allocations to get more chances of doing GC while in the
    // newStringFromBytes intrinsic.
    for (int i = 0; i < 10; i++) {
      try {
        byte[] f = new byte[100000000];
        f[0] = (byte)i;
        f[1] = (byte)i;
        m.invoke(null, f, 0);
      } catch (InvocationTargetException e) {
        if (e.getCause() instanceof OutOfMemoryError) {
          // Ignore, this is a stress test.
        } else {
          throw e;
        }
      } catch (OutOfMemoryError e) {
        // Ignore, this is a stress test.
      }
    }
  }
}
