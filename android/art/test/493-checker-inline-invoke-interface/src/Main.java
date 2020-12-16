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

interface Itf {
  public void foo();
}

class ForceStatic {
  static {
    System.out.println("Hello from clinit");
    new Exception().printStackTrace(System.out);
  }
  static int field;
}

public class Main implements Itf {
  public void foo() {
    int a = ForceStatic.field;
  }

  /// CHECK-START: void Main.main(java.lang.String[]) inliner (before)
  /// CHECK:           InvokeStaticOrDirect
  /// CHECK:           InvokeInterface

  /// CHECK-START: void Main.main(java.lang.String[]) inliner (after)
  /// CHECK-NOT:       Invoke{{.*Object\.<init>.*}}
  public static void main(String[] args) {
    Itf itf = bar();
    itf.foo();
  }

  public static Itf bar() {
    return new Main();
  }
}
