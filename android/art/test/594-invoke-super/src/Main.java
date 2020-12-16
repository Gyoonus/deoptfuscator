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

//
// Two classes A and B with method foo().
//

class A {
  A() { System.out.println("new A"); }

  public void foo() { System.out.println("I am A's foo"); }

  // We previously used to invoke this method with a Y instance, due
  // to invoke-super underspecified behavior.
  public void bar() { System.out.println("I am A's bar"); }
}

class B {
  B() { System.out.println("new B"); }

  public void foo() { System.out.println("I am B's foo"); }
}

//
// Two subclasses X and Y that call foo() on super.
//

class X extends A {
  public void foo() { super.foo(); }
}

class Y extends B {
  public void foo() { super.foo(); }
}

//
// Driver class.
//

public class Main {

  public static void main(String[] args) throws Exception {
    // The normal stuff, X's super goes to A, Y's super goes to B.
    new X().foo();
    new Y().foo();

    // And now it gets interesting.

    // In bytecode, we define a class Z that is a subclass of A, and we call
    // invoke-super on an instance of Y.
    Class<?> z = Class.forName("Z");
    Method m = z.getMethod("foo");
    try {
      m.invoke(z.newInstance());
      throw new Error("Expected InvocationTargetException");
    } catch (InvocationTargetException e) {
      if (!(e.getCause() instanceof NoSuchMethodError)) {
        throw new Error("Expected NoSuchMethodError");
      }
    }

    System.out.println("passed");
  }
}
