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
  public void invokeInterface();
}

public class Main implements Itf {

  public void invokeInterface () {
  }

  public void invokeVirtual() {
  }

  public static Main createMain() {
    return new Main();
  }

  public static Itf createItf() {
    return new Main();
  }

  /// CHECK-START: void Main.testMethod() inliner (before)
  /// CHECK-DAG:     InvokeVirtual
  /// CHECK-DAG:     InvokeInterface

  /// CHECK-START: void Main.testMethod() inliner (after)
  /// CHECK-NOT:     Invoke{{.*Object\.<init>.*}}

  public static void testMethod() {
    createMain().invokeVirtual();
    createItf().invokeInterface();
  }

  public static void main(String[] args) {
    testMethod();
  }
}
