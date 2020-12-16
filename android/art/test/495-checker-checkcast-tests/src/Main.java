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

public class Main {
  public static boolean $inline$classTypeTest(Object o) {
    return ((SubMain)o) == o;
  }

  public static boolean $inline$interfaceTypeTest(Object o) {
    return ((Itf)o) == o;
  }

  public static SubMain subMain;
  public static Main mainField;
  public static Unrelated unrelatedField;
  public static FinalUnrelated finalUnrelatedField;

  /// CHECK-START: boolean Main.classTypeTestNull() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean classTypeTestNull() {
    return $inline$classTypeTest(null);
  }

  /// CHECK-START: boolean Main.classTypeTestExactMain() register (after)
  /// CHECK: CheckCast
  public static boolean classTypeTestExactMain() {
    return $inline$classTypeTest(new Main());
  }

  /// CHECK-START: boolean Main.classTypeTestExactSubMain() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean classTypeTestExactSubMain() {
    return $inline$classTypeTest(new SubMain());
  }

  /// CHECK-START: boolean Main.classTypeTestSubMainOrNull() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean classTypeTestSubMainOrNull() {
    return $inline$classTypeTest(subMain);
  }

  /// CHECK-START: boolean Main.classTypeTestMainOrNull() register (after)
  /// CHECK: CheckCast
  public static boolean classTypeTestMainOrNull() {
    return $inline$classTypeTest(mainField);
  }

  /// CHECK-START: boolean Main.classTypeTestUnrelated() register (after)
  /// CHECK: CheckCast
  public static boolean classTypeTestUnrelated() {
    return $inline$classTypeTest(unrelatedField);
  }

  /// CHECK-START: boolean Main.classTypeTestFinalUnrelated() register (after)
  /// CHECK: CheckCast
  public static boolean classTypeTestFinalUnrelated() {
    return $inline$classTypeTest(finalUnrelatedField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestNull() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean interfaceTypeTestNull() {
    return $inline$interfaceTypeTest(null);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestExactMain() register (after)
  /// CHECK: CheckCast
  public static boolean interfaceTypeTestExactMain() {
    return $inline$interfaceTypeTest(new Main());
  }

  /// CHECK-START: boolean Main.interfaceTypeTestExactSubMain() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean interfaceTypeTestExactSubMain() {
    return $inline$interfaceTypeTest(new SubMain());
  }

  /// CHECK-START: boolean Main.interfaceTypeTestSubMainOrNull() register (after)
  /// CHECK-NOT: CheckCast
  public static boolean interfaceTypeTestSubMainOrNull() {
    return $inline$interfaceTypeTest(subMain);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestMainOrNull() register (after)
  /// CHECK: CheckCast
  public static boolean interfaceTypeTestMainOrNull() {
    return $inline$interfaceTypeTest(mainField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestUnrelated() register (after)
  /// CHECK: CheckCast
  public static boolean interfaceTypeTestUnrelated() {
    return $inline$interfaceTypeTest(unrelatedField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestFinalUnrelated() register (after)
  /// CHECK: CheckCast
  public static boolean interfaceTypeTestFinalUnrelated() {
    return $inline$interfaceTypeTest(finalUnrelatedField);
  }

  /// CHECK-START: java.lang.String Main.knownTestWithLoadedClass() register (after)
  /// CHECK-NOT: CheckCast
  public static String knownTestWithLoadedClass() {
    return (String)$inline$getString();
  }

  /// CHECK-START: Itf Main.knownTestWithUnloadedClass() register (after)
  /// CHECK: CheckCast
  public static Itf knownTestWithUnloadedClass() {
    return (Itf)$inline$getString();
  }

  public static Object $inline$getString() {
    return new String();
  }

  public static Object $inline$getMain() {
    return new Main();
  }

  /// CHECK-START: void Main.nonNullBoundType() register (after)
  /// CHECK-NOT: NullCheck
  public static void nonNullBoundType() {
    Main main = (Main)$inline$getMain();
    main.getClass();
  }

  public static void main(String[] args) {
    classTypeTestNull();
    try {
      classTypeTestExactMain();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}
    classTypeTestExactSubMain();

    subMain = null;
    classTypeTestSubMainOrNull();
    subMain = new SubMain();
    classTypeTestSubMainOrNull();

    mainField = null;
    classTypeTestMainOrNull();
    mainField = new Main();
    try {
      classTypeTestMainOrNull();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}
    mainField = new SubMain();
    classTypeTestMainOrNull();

    unrelatedField = null;
    classTypeTestUnrelated();
    unrelatedField = new Unrelated();
    try {
      classTypeTestUnrelated();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}

    finalUnrelatedField = null;
    classTypeTestFinalUnrelated();
    finalUnrelatedField = new FinalUnrelated();
    try {
      classTypeTestFinalUnrelated();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}

    interfaceTypeTestNull();
    try {
      interfaceTypeTestExactMain();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}
    interfaceTypeTestExactSubMain();

    subMain = null;
    interfaceTypeTestSubMainOrNull();
    subMain = new SubMain();
    interfaceTypeTestSubMainOrNull();

    mainField = null;
    interfaceTypeTestMainOrNull();
    mainField = new Main();
    try {
      interfaceTypeTestMainOrNull();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}
    mainField = new SubMain();
    interfaceTypeTestMainOrNull();

    unrelatedField = null;
    interfaceTypeTestUnrelated();
    unrelatedField = new Unrelated();
    try {
      interfaceTypeTestUnrelated();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}

    finalUnrelatedField = null;
    interfaceTypeTestFinalUnrelated();
    finalUnrelatedField = new FinalUnrelated();
    try {
      interfaceTypeTestFinalUnrelated();
      throw new Error("ClassCastException expected");
    } catch (ClassCastException e) {}
  }
}

interface Itf {
}

class SubMain extends Main implements Itf {
}

class Unrelated {
}

final class FinalUnrelated {
}
