/*
 * Copyright (C) 2014 The Android Open Source Project
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

public class Main {
  public static void main(String[] args) {
    System.out.println(foo());
  }

  public static int foo() {
    Main m = new Main();
    int a = m.field;
    if (a == 0) {
      m.field = 42;
      if (m.test) {
        a = 3;
      }
    }
    // The compiler used to GVN this field get with the one line 24,
    // even though the field is updated in the if.
    return m.field;
  }

  public int field;
  public boolean test = true;
}
