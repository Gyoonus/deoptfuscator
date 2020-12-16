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

final class Final {
  public String toString() {
    return "final";
  }
}

public class Main {
  /// CHECK-START: Final Main.testKeepCheckCast(java.lang.Object, boolean) builder (after)
  /// CHECK:    <<Phi:l\d+>>     Phi klass:java.lang.Object
  /// CHECK:    <<Class:l\d+>>   LoadClass
  /// CHECK:                     CheckCast [<<Phi>>,<<Class>>]
  /// CHECK:    <<Ret:l\d+>>     BoundType [<<Phi>>] klass:Final
  /// CHECK:                     Return [<<Ret>>]

  /// CHECK-START: Final Main.testKeepCheckCast(java.lang.Object, boolean) instruction_simplifier (after)
  /// CHECK:    <<Phi:l\d+>>     Phi
  /// CHECK:    <<Class:l\d+>>   LoadClass
  /// CHECK:                     CheckCast [<<Phi>>,<<Class>>]
  /// CHECK:    <<Ret:l\d+>>     BoundType [<<Phi>>]
  /// CHECK:                     Return [<<Ret>>]
  public static Final testKeepCheckCast(Object o, boolean cond) {
    Object x = new Final();
    while (cond) {
      x = o;
      cond = false;
    }
    return (Final) x;
  }

  /// CHECK-START: void Main.testKeepInstanceOf(java.lang.Object, boolean) builder (after)
  /// CHECK:    <<Phi:l\d+>>     Phi klass:java.lang.Object
  /// CHECK:    <<Class:l\d+>>   LoadClass
  /// CHECK:                     InstanceOf [<<Phi>>,<<Class>>]

  /// CHECK-START: void Main.testKeepInstanceOf(java.lang.Object, boolean) dead_code_elimination$initial (after)
  /// CHECK:    <<Phi:l\d+>>     Phi
  /// CHECK:    <<Class:l\d+>>   LoadClass
  /// CHECK:                     InstanceOf [<<Phi>>,<<Class>>]
  public static void testKeepInstanceOf(Object o, boolean cond) {
    Object x = new Final();
    while (cond) {
      x = o;
      cond = false;
    }
    if (x instanceof Final) {
      System.out.println("instanceof succeed");
    } else {
      System.out.println("instanceof failed");
    }
  }

  /// CHECK-START: java.lang.String Main.testNoInline(java.lang.Object, boolean) builder (after)
  /// CHECK:    <<Phi:l\d+>>     Phi klass:java.lang.Object
  /// CHECK:    <<NC:l\d+>>      NullCheck [<<Phi>>]
  /// CHECK:    <<Ret:l\d+>>     InvokeVirtual [<<NC>>] method_name:java.lang.Object.toString
  /// CHECK:                     Return [<<Ret>>]

  /// CHECK-START: java.lang.String Main.testNoInline(java.lang.Object, boolean) inliner (after)
  /// CHECK:    <<Phi:l\d+>>     Phi
  /// CHECK:    <<NC:l\d+>>      NullCheck [<<Phi>>]
  /// CHECK:    <<Ret:l\d+>>     InvokeVirtual [<<NC>>] method_name:java.lang.Object.toString
  /// CHECK:                     Return [<<Ret>>]
  public static String testNoInline(Object o, boolean cond) {
    Object x = new Final();
    while (cond) {
      x = o;
      cond = false;
    }
    return x.toString();
  }

  public static void main(String[] args) {
    try {
      testKeepCheckCast(new Object(), true);
      throw new Error("Expected check cast exception");
    } catch (ClassCastException e) {
      // expected
    }

    testKeepInstanceOf(new Object(), true);

    if ("final".equals(testNoInline(new Object(), true))) {
      throw new Error("Bad inlining");
    }
  }
}
