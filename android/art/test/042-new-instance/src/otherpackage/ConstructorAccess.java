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

package otherpackage;

import java.lang.reflect.Constructor;

public class ConstructorAccess {

    static class Inner {
      Inner() {}
    }

    // Test for regression in b/25817515. Inner class constructor should
    // be accessible from this static method, but if we over-shoot and check
    // accessibility using the frame below (in Main class), we will see an
    // IllegalAccessException from #newInstance
    static public void newConstructorInstance() throws Exception {
      Class<?> c = Inner.class;
      Constructor cons = c.getDeclaredConstructor();
      Object obj = cons.newInstance();
    }
}
