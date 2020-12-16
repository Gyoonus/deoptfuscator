/*
 * Copyright (C) 2016 The Android Open Source Project
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

class Main {
  private static boolean $inline$false() { return false; }

  /// CHECK-START: void Main.main(java.lang.String[]) dead_code_elimination$initial (before)
  /// CHECK-DAG:     <<Const0:i\d+>> IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>> IntConstant 1
  /// CHECK-DAG:     <<Phi:i\d+>>    Phi [<<Const0>>,<<Add:i\d+>>] loop:{{B\d+}}
  /// CHECK-DAG:                     InvokeVirtual [{{l\d+}},<<Phi>>] method_name:java.io.PrintStream.println
  /// CHECK-DAG:     <<Add>>         Add [<<Phi>>,<<Const1>>]

  public static void main(String[] args) {
    if ($inline$false()) {
      int x = 0;
      while (true) {
        System.out.println(x++);
      }
    }
  }
}
