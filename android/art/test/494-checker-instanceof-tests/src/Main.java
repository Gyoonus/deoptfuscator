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
    return o instanceof SubMain;
  }

  public static boolean $inline$interfaceTypeTest(Object o) {
    return o instanceof Itf;
  }

  public static SubMain subMain;
  public static Main mainField;
  public static Unrelated unrelatedField;
  public static FinalUnrelated finalUnrelatedField;

  /// CHECK-START: boolean Main.classTypeTestNull() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean classTypeTestNull() {
    return $inline$classTypeTest(null);
  }

  /// CHECK-START: boolean Main.classTypeTestExactMain() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean classTypeTestExactMain() {
    return $inline$classTypeTest(new Main());
  }

  /// CHECK-START: boolean Main.classTypeTestExactSubMain() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 1
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean classTypeTestExactSubMain() {
    return $inline$classTypeTest(new SubMain());
  }

  /// CHECK-START: boolean Main.classTypeTestSubMainOrNull() register (after)
  /// CHECK-DAG: <<Value:z\d+>> NotEqual
  /// CHECK-DAG:                Return [<<Value>>]
  public static boolean classTypeTestSubMainOrNull() {
    return $inline$classTypeTest(subMain);
  }

  /// CHECK-START: boolean Main.classTypeTestMainOrNull() register (after)
  /// CHECK-DAG: <<Value:z\d+>> InstanceOf
  /// CHECK-DAG:                Return [<<Value>>]
  public static boolean classTypeTestMainOrNull() {
    return $inline$classTypeTest(mainField);
  }

  /// CHECK-START: boolean Main.classTypeTestUnrelated() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean classTypeTestUnrelated() {
    return $inline$classTypeTest(unrelatedField);
  }

  /// CHECK-START: boolean Main.classTypeTestFinalUnrelated() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean classTypeTestFinalUnrelated() {
    return $inline$classTypeTest(finalUnrelatedField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestNull() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean interfaceTypeTestNull() {
    return $inline$interfaceTypeTest(null);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestExactMain() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean interfaceTypeTestExactMain() {
    return $inline$interfaceTypeTest(new Main());
  }

  /// CHECK-START: boolean Main.interfaceTypeTestExactSubMain() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 1
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean interfaceTypeTestExactSubMain() {
    return $inline$interfaceTypeTest(new SubMain());
  }

  /// CHECK-START: boolean Main.interfaceTypeTestSubMainOrNull() register (after)
  /// CHECK-DAG: <<Value:z\d+>> NotEqual
  /// CHECK-DAG:                Return [<<Value>>]
  public static boolean interfaceTypeTestSubMainOrNull() {
    return $inline$interfaceTypeTest(subMain);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestMainOrNull() register (after)
  /// CHECK-DAG: <<Value:z\d+>> InstanceOf
  /// CHECK-DAG:                Return [<<Value>>]
  public static boolean interfaceTypeTestMainOrNull() {
    return $inline$interfaceTypeTest(mainField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestUnrelated() register (after)
  /// CHECK-DAG: <<Value:z\d+>> InstanceOf
  /// CHECK-DAG:                Return [<<Value>>]
  public static boolean interfaceTypeTestUnrelated() {
    // This method is the main difference between doing an instanceof on an interface
    // or a class. We have to keep the instanceof in case a subclass of Unrelated
    // implements the interface.
    return $inline$interfaceTypeTest(unrelatedField);
  }

  /// CHECK-START: boolean Main.interfaceTypeTestFinalUnrelated() register (after)
  /// CHECK-DAG: <<Const:i\d+>> IntConstant 0
  /// CHECK-DAG:                Return [<<Const>>]
  public static boolean interfaceTypeTestFinalUnrelated() {
    return $inline$interfaceTypeTest(finalUnrelatedField);
  }

  // Check that we remove the LoadClass instruction from the graph.
  /// CHECK-START: boolean Main.knownTestWithLoadedClass() register (after)
  /// CHECK-NOT: LoadClass
  public static boolean knownTestWithLoadedClass() {
    return new String() instanceof String;
  }

  // Check that we do not remove the LoadClass instruction from the graph.
  /// CHECK-START: boolean Main.knownTestWithUnloadedClass() register (after)
  /// CHECK: <<Const:i\d+>> IntConstant 0
  /// CHECK:                LoadClass
  /// CHECK:                Return [<<Const>>]
  public static boolean knownTestWithUnloadedClass() {
    return $inline$returnUnrelated() instanceof String;
  }

  public static Object $inline$returnUnrelated() {
    return new Unrelated();
  }

  public static void expect(boolean expected, boolean actual) {
    if (expected != actual) {
      throw new Error("Unexpected result");
    }
  }

  public static void main(String[] args) {
    expect(false, classTypeTestNull());
    expect(false, classTypeTestExactMain());
    expect(true, classTypeTestExactSubMain());

    subMain = null;
    expect(false, classTypeTestSubMainOrNull());
    subMain = new SubMain();
    expect(true, classTypeTestSubMainOrNull());

    mainField = null;
    expect(false, classTypeTestMainOrNull());
    mainField = new Main();
    expect(false, classTypeTestMainOrNull());
    mainField = new SubMain();
    expect(true, classTypeTestMainOrNull());

    unrelatedField = null;
    expect(false, classTypeTestUnrelated());
    unrelatedField = new Unrelated();
    expect(false, classTypeTestUnrelated());

    finalUnrelatedField = null;
    expect(false, classTypeTestFinalUnrelated());
    finalUnrelatedField = new FinalUnrelated();
    expect(false, classTypeTestFinalUnrelated());

    expect(false, interfaceTypeTestNull());
    expect(false, interfaceTypeTestExactMain());
    expect(true, interfaceTypeTestExactSubMain());

    subMain = null;
    expect(false, interfaceTypeTestSubMainOrNull());
    subMain = new SubMain();
    expect(true, interfaceTypeTestSubMainOrNull());

    mainField = null;
    expect(false, interfaceTypeTestMainOrNull());
    mainField = new Main();
    expect(false, interfaceTypeTestMainOrNull());
    mainField = new SubMain();
    expect(true, interfaceTypeTestMainOrNull());

    unrelatedField = null;
    expect(false, interfaceTypeTestUnrelated());
    unrelatedField = new Unrelated();
    expect(false, interfaceTypeTestUnrelated());

    finalUnrelatedField = null;
    expect(false, interfaceTypeTestFinalUnrelated());
    finalUnrelatedField = new FinalUnrelated();
    expect(false, interfaceTypeTestFinalUnrelated());
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
