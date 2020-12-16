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

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    Class<?> c = Class.forName("PhiLiveness");
    Method m = c.getMethod("mergeOk", boolean.class, byte.class);
    m.invoke(null, new Boolean(true), new Byte((byte)2));

    m = c.getMethod("mergeNotOk", boolean.class, float.class);
    m.invoke(null, new Boolean(true), new Float(4.0f));

    m = c.getMethod("mergeReferences", Main.class);
    m.invoke(null, new Main());

    m = c.getMethod("phiEquivalent");
    m.invoke(null);

    m = c.getMethod("phiAllEquivalents", Main.class);
    m.invoke(null, new Main());
  }
}
