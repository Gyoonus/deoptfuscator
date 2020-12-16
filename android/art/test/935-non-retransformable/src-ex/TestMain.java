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

import java.lang.reflect.Method;

public class TestMain {
  public static void runTest() throws Exception {
    Transform t = new Transform();
    // Call functions with reflection. Since the sayGoodbye function does not exist in the
    // LTransform; when we compile this for the first time we need to use reflection.
    Method hi = Transform.class.getMethod("sayHi");
    Method bye = Transform.class.getMethod("sayGoodbye");
    hi.invoke(t);
    t.sayHi();
    bye.invoke(t);
  }
}
