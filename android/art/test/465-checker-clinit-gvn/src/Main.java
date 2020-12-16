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

class OtherClass {
  static {
    a = 42;
    b = 54;
  }

  static int a;
  static int b;
}

public final class Main {

  /// CHECK-START: int Main.accessTwoStatics() GVN (before)
  /// CHECK-DAG:     <<Class1:l\d+>>  LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class1>>]
  /// CHECK-DAG:     <<Class2:l\d+>>  LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class2>>]

  /// CHECK-START: int Main.accessTwoStatics() GVN (after)
  /// CHECK-DAG:     <<Class:l\d+>>   LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class>>]
  /// CHECK-NOT:                      ClinitCheck

  public static int accessTwoStatics() {
    return OtherClass.b - OtherClass.a;
  }

  /// CHECK-START: int Main.accessTwoStaticsCallInBetween() GVN (before)
  /// CHECK-DAG:     <<Class1:l\d+>>  LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class1>>]
  /// CHECK-DAG:     <<Class2:l\d+>>  LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class2>>]

  /// CHECK-START: int Main.accessTwoStaticsCallInBetween() GVN (after)
  /// CHECK-DAG:     <<Class:l\d+>>   LoadClass
  /// CHECK-DAG:                      ClinitCheck [<<Class>>]
  /// CHECK-NOT:                      ClinitCheck

  public static int accessTwoStaticsCallInBetween() {
    int b = OtherClass.b;
    foo();
    return b - OtherClass.a;
  }

  public static void foo() {
    try {
      Thread.sleep(0);
    } catch (Exception e) {
      throw new Error(e);
    }
  }

  public static void main(String[] args) {
    if (accessTwoStatics() != 12) {
      throw new Error("Expected 12");
    }

    if (accessTwoStaticsCallInBetween() != 12) {
      throw new Error("Expected 12");
    }
  }
}
