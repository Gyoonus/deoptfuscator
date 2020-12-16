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


// Define enough methods to enter the conflict trampoline.
interface Itf {
  default int $noinline$def1() { return 42; }
  default int $noinline$def2() { return 42; }
  default int $noinline$def3() { return 42; }
  default int $noinline$def4() { return 42; }
  default int $noinline$def5() { return 42; }
  default int $noinline$def6() { return 42; }
  default int $noinline$def7() { return 42; }
  default int $noinline$def8() { return 42; }
  default int $noinline$def9() { return 42; }
  default int $noinline$def10() { return 42; }
  default int $noinline$def11() { return 42; }
  default int $noinline$def12() { return 42; }
  default int $noinline$def13() { return 42; }
  default int $noinline$def14() { return 42; }
  default int $noinline$def15() { return 42; }
  default int $noinline$def16() { return 42; }
  default int $noinline$def17() { return 42; }
  default int $noinline$def18() { return 42; }
  default int $noinline$def19() { return 42; }
  default int $noinline$def20() { return 42; }
  default int $noinline$def21() { return 42; }
  default int $noinline$def22() { return 42; }

  // Known name conflict in OC-dev.
  default int $noinline$defAa() { return 42; }
  default int $noinline$defbb() { return 42; }
}

// Define an abstract class so that the super calls in Main.instanceMethod
// end up finding the miranda method (which was then wrongly stashed into
// the dex cache).
class AbstractItf implements Itf {
}


public class Main extends AbstractItf {
  static Itf itf = new Main();
  public static void main(String[] args) throws Exception {
    new Main().instanceMethod();
  }

  public void instanceMethod() {
    // Do super calls to invoke artQuickResolutionTrampoline, which used to
    // put the copied method AbstractIf.<name> into the slot for the MethodId
    // referencing the Itf method.
    super.$noinline$def1();
    super.$noinline$def2();
    super.$noinline$def3();
    super.$noinline$def4();
    super.$noinline$def5();
    super.$noinline$def6();
    super.$noinline$def7();
    super.$noinline$def8();
    super.$noinline$def9();
    super.$noinline$def10();
    super.$noinline$def11();
    super.$noinline$def12();
    super.$noinline$def13();
    super.$noinline$def14();
    super.$noinline$def15();
    super.$noinline$def16();
    super.$noinline$def17();
    super.$noinline$def18();
    super.$noinline$def19();
    super.$noinline$def20();
    super.$noinline$def21();
    super.$noinline$def22();
    super.$noinline$defAa();
    super.$noinline$defbb();

    // Now call the same methods but through an invoke-interface, which used to crash
    // if the wrong method was in the dex cache.
    itf.$noinline$def1();
    itf.$noinline$def2();
    itf.$noinline$def3();
    itf.$noinline$def4();
    itf.$noinline$def5();
    itf.$noinline$def6();
    itf.$noinline$def7();
    itf.$noinline$def8();
    itf.$noinline$def9();
    itf.$noinline$def10();
    itf.$noinline$def11();
    itf.$noinline$def12();
    itf.$noinline$def13();
    itf.$noinline$def14();
    itf.$noinline$def15();
    itf.$noinline$def16();
    itf.$noinline$def17();
    itf.$noinline$def18();
    itf.$noinline$def19();
    itf.$noinline$def20();
    itf.$noinline$def21();
    itf.$noinline$def22();
    itf.$noinline$defAa();
    itf.$noinline$defbb();
  }
}
