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

public class Main {

  /// CHECK-START: int Main.bar(Main) inliner (before)
  /// CHECK: InvokeVirtual
  /// CHECK: InvokeVirtual

  /// CHECK-START: int Main.bar(Main) inliner (after)
  /// CHECK-NOT: InvokeVirtual
  public static int bar(Main m) {
    if (m.getClass() == Main.class) {
      return m.foo();
    }
    return 4;
  }

  public int foo() {
    return 42;
  }

  /// CHECK-START: boolean Main.classEquality1() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Eq:z\d+>>     {{Equal|NotEqual}}
  /// CHECK-DAG: <<Select:i\d+>> Select [{{i\d+}},{{i\d+}},<<Eq>>]
  /// CHECK-DAG:                 Return [<<Select>>]

  /// CHECK-START: boolean Main.classEquality1() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Constant:i\d+>> IntConstant 1
  /// CHECK-DAG:                   Return [<<Constant>>]
  public static boolean classEquality1() {
    return new Main().getClass() == Main.class;
  }

  /// CHECK-START: boolean Main.classEquality2() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Eq:z\d+>>     {{Equal|NotEqual}}
  /// CHECK-DAG: <<Select:i\d+>> Select [{{i\d+}},{{i\d+}},<<Eq>>]
  /// CHECK-DAG:                 Return [<<Select>>]

  /// CHECK-START: boolean Main.classEquality2() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Constant:i\d+>> IntConstant 0
  /// CHECK-DAG:                   Return [<<Constant>>]
  public static boolean classEquality2() {
    Object o = new SubMain();
    return o.getClass() == Main.class;
  }

  /// CHECK-START: boolean Main.classEquality3() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Eq:z\d+>>     {{Equal|NotEqual}}
  /// CHECK-DAG: <<Select:i\d+>> Select [{{i\d+}},{{i\d+}},<<Eq>>]
  /// CHECK-DAG:                 Return [<<Select>>]

  /// CHECK-START: boolean Main.classEquality3() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Constant:i\d+>> IntConstant 0
  /// CHECK-DAG:                   Return [<<Constant>>]
  public static boolean classEquality3() {
    return new Main().getClass() != Main.class;
  }

  /// CHECK-START: boolean Main.classEquality4() instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Eq:z\d+>>     {{Equal|NotEqual}}
  /// CHECK-DAG: <<Select:i\d+>> Select [{{i\d+}},{{i\d+}},<<Eq>>]
  /// CHECK-DAG:                 Return [<<Select>>]

  /// CHECK-START: boolean Main.classEquality4() instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Constant:i\d+>> IntConstant 1
  /// CHECK-DAG:                   Return [<<Constant>>]
  public static boolean classEquality4() {
    Object o = new SubMain();
    return o.getClass() != Main.class;
  }

  public static void main(String[] args) {
    int actual = bar(new Main());
    if (actual != 42) {
      throw new Error("Expected 42, got " + actual);
    }
    actual = bar(new SubMain());
    if (actual != 4) {
      throw new Error("Expected 4, got " + actual);
    }
  }
}

class SubMain extends Main {
}
