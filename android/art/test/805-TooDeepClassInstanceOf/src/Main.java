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

class A {}
class B1 extends A {}
class B2 extends A {}
class C1 extends B1 {}
class C2 extends B1 {}
class D1 extends C1 {}
class D2 extends C2 {}
class E1 extends D1 {}
class E2 extends D2 {}
class F1 extends E1 {}
class F2 extends E2 {}
class G1 extends F1 {}
class G2 extends F2 {}

public class Main {
  public static void main(String[] args) {
    String yes = "Yes";
    String no = "No";

    A a = new A();
    A b1 = new B1();
    A b2 = new B2();
    A c1 = new C1();
    A c2 = new C2();
    A f1 = new F1();
    A f2 = new F2();
    A g1 = new G1();
    A g2 = new G2();

    expectFalse(b1 instanceof G1);
    expectTrue(g1 instanceof B1);
    expectFalse(b1 instanceof F1);
    expectTrue(f1 instanceof B1);

    expectFalse(b2 instanceof G1);
    expectFalse(g1 instanceof B2);
    expectFalse(b2 instanceof F1);
    expectFalse(f1 instanceof B2);

    expectFalse(g2 instanceof G1);
    expectFalse(g1 instanceof G2);
    expectFalse(f2 instanceof F1);
    expectFalse(f1 instanceof F2);

    expectTrue(g1 instanceof F1);
    expectFalse(g1 instanceof F2);
    expectFalse(g2 instanceof F1);
    expectTrue(g2 instanceof F2);

    System.out.println("passed");
  }

  private static void expectTrue(boolean value) {
    if (!value) {
      throw new Error("Expected True");
    }
  }

  private static void expectFalse(boolean value) {
    if (value) {
      throw new Error("Expected False");
    }
  }
}
