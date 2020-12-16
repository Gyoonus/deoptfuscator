/*
 * Copyright 2017 The Android Open Source Project
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

import java.util.*;

public class Main {

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    if (!checkAppImageLoaded()) {
      System.out.println("AppImage not loaded.");
    }

    expectNotPreInit(Day.class);
    expectNotPreInit(ClInit.class); // should pass
    expectNotPreInit(A.class); // should pass
    expectNotPreInit(B.class); // should fail
    expectNotPreInit(C.class); // should fail
    expectNotPreInit(G.class); // should fail
    expectNotPreInit(Gs.class); // should fail
    expectNotPreInit(Gss.class); // should fail

    expectNotPreInit(Add.class);
    expectNotPreInit(Mul.class);
    expectNotPreInit(ObjectRef.class);

    A x = new A();
    System.out.println("A.a: " + A.a);

    B y = new B();
    C z = new C();
    System.out.println("A.a: " + A.a);
    System.out.println("B.b: " + B.b);
    System.out.println("C.c: " + C.c);

    ClInit c = new ClInit();
    int aa = c.a;

    System.out.println("X: " + c.getX());
    System.out.println("Y: " + c.getY());
    System.out.println("str: " + c.str);
    System.out.println("ooo: " + c.ooo);
    System.out.println("Z: " + c.getZ());
    System.out.println("A: " + c.getA());
    System.out.println("AA: " + aa);

    if (c.a != 101) {
      System.out.println("a != 101");
    }

    return;
  }

  static void expectPreInit(Class<?> klass) {
    if (checkInitialized(klass) == false) {
      System.out.println(klass.getName() + " should be initialized!");
    }
  }

  static void expectNotPreInit(Class<?> klass) {
    if (checkInitialized(klass) == true) {
      System.out.println(klass.getName() + " should not be initialized!");
    }
  }

  public static native boolean checkAppImageLoaded();
  public static native boolean checkAppImageContains(Class<?> klass);
  public static native boolean checkInitialized(Class<?> klass);
}

enum Day {
    SUNDAY, MONDAY, TUESDAY, WEDNESDAY,
    THURSDAY, FRIDAY, SATURDAY
}

class ClInit {

  static String ooo = "OoooooO";
  static String str;
  static int z;
  static int x, y;
  public static volatile int a = 100;

  static {
    StringBuilder sb = new StringBuilder();
    sb.append("Hello ");
    sb.append("World!");
    str = sb.toString();

    z = 0xFF;
    z += 0xFF00;
    z += 0xAA0000;

    for(int i = 0; i < 100; i++) {
      x += i;
    }

    y = x;
    for(int i = 0; i < 40; i++) {
      y += i;
    }
  }

  int getX() {
    return x;
  }

  int getZ() {
    return z;
  }

  int getY() {
    return y;
  }

  int getA() {
    return a;
  }
}

class A {
  public static int a = 2;
  static {
    a = 5;  // self-updating, pass
  }
}

class B {
  public static int b;
  static {
    A.a = 10;  // write other's static field, fail
    b = A.a;   // read other's static field, fail
  }
}

class C {
  public static int c;
  static {
    c = A.a; // read other's static field, fail
  }
}

class G {
  static G g;
  static int i;
  static {
    g = new Gss(); // fail because recursive dependency
    i = A.a;  // read other's static field, fail
  }
}

// Gs will be successfully initialized as G's status is initializing at that point, which will
// later aborted but Gs' transaction is already committed.
// Instantiation of Gs will fail because we try to invoke G's <init>
// but G's status will be StatusVerified. INVOKE_DIRECT will not initialize class.
class Gs extends G {}  // fail because super class can't be initialized
class Gss extends Gs {}

// pruned because holding reference to non-image class
class ObjectRef {
  static Class<?> klazz[] = new Class<?>[]{Add.class, Mul.class};
}

// non-image
class Add {
  static int exec(int a, int b) {
    return a + b;
  }
}

// non-image
class Mul {
  static int exec(int a, int b) {
    return a * b;
  }
}
