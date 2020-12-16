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

package art;

public class SameThread {
  public static void doTest() throws Exception {
    System.out.println("###################");
    System.out.println("### Same thread ###");
    System.out.println("###################");
    System.out.println("From top");
    Recurse.foo(4, 0, 25, null);
    Recurse.foo(4, 1, 25, null);
    Recurse.foo(4, 0, 5, null);
    Recurse.foo(4, 2, 5, null);

    System.out.println("From bottom");
    Recurse.foo(4, -1, 25, null);
    Recurse.foo(4, -5, 5, null);
    Recurse.foo(4, -7, 5, null);
  }
}
