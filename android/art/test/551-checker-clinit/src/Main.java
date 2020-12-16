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

  public static void main(String[] args) {}
  public static int foo = 42;

  /// CHECK-START: void Main.inlinedMethod() builder (after)
  /// CHECK:                        ClinitCheck

  /// CHECK-START: void Main.inlinedMethod() inliner (after)
  /// CHECK:                        ClinitCheck
  /// CHECK-NOT:                    ClinitCheck
  /// CHECK-NOT:                    InvokeStaticOrDirect
  public void inlinedMethod() {
    SubSub.bar();
  }
}

class Sub extends Main {
  /// CHECK-START: void Sub.invokeSuperClass() builder (after)
  /// CHECK-NOT:                        ClinitCheck
  public void invokeSuperClass() {
    int a = Main.foo;
  }

  /// CHECK-START: void Sub.invokeItself() builder (after)
  /// CHECK-NOT:                        ClinitCheck
  public void invokeItself() {
    int a = foo;
  }

  /// CHECK-START: void Sub.invokeSubClass() builder (after)
  /// CHECK:                            ClinitCheck
  public void invokeSubClass() {
    int a = SubSub.foo;
  }

  public static int foo = 42;
}

class SubSub {
  public static void bar() {
    int a = Main.foo;
  }
  public static int foo = 42;
}
