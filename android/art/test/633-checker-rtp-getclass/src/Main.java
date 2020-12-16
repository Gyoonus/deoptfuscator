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

public class Main {
  public static void main(String[] args) {
    System.out.println($noinline$runSmaliTest("$opt$noinline$foo", new Main()));
    System.out.println($noinline$runSmaliTest("$opt$noinline$foo", new SubMain()));
    System.out.println($noinline$runSmaliTest("$opt$noinline$foo", new SubSubMain()));
  }

  public int bar() {
    return 1;
  }

  public int foo() {
    return 2;
  }

  public static boolean doThrow = false;

  public static int $noinline$runSmaliTest(String name, Main input) {
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name, Main.class);
      return (Integer) m.invoke(null, input);
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }
}

class SubMain extends Main {
  public int bar() {
    return 3;
  }

  public int foo() {
    return 4;
  }
}

class SubSubMain extends SubMain {
  public int bar() {
    return 5;
  }

  public int foo() {
    return 6;
  }
}
