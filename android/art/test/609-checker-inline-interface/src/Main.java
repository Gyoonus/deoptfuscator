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

public final class Main implements Interface {

  static void methodWithInvokeInterface(Interface interf) {
    interf.doCall();
  }

  public void doCall() {
    // We do not inline methods that always throw.
    throw new Error("");
  }

  public static void main(String[] args) {
    try {
      testInlineInterfaceCall();
    } catch (Error e) {
      // Expected
    }
    try {
      testInterfaceToVirtualCall();
    } catch (Error e) {
      // Expected.
    }
  }

  /// CHECK-START: void Main.testInlineInterfaceCall() inliner (before)
  /// CHECK:                          InvokeStaticOrDirect method_name:Main.methodWithInvokeInterface

  /// CHECK-START: void Main.testInlineInterfaceCall() inliner (before)
  /// CHECK-NOT:                      InvokeInterface

  /// CHECK-START: void Main.testInlineInterfaceCall() inliner (after)
  /// CHECK:                          InvokeInterface method_name:Interface.doCall

  /// CHECK-START: void Main.testInlineInterfaceCall() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  public static void testInlineInterfaceCall() {
    methodWithInvokeInterface(itf);
  }

  /// CHECK-START: void Main.testInterfaceToVirtualCall() inliner (before)
  /// CHECK:                          InvokeStaticOrDirect method_name:Main.methodWithInvokeInterface

  /// CHECK-START: void Main.testInterfaceToVirtualCall() inliner (before)
  /// CHECK-NOT:                      InvokeInterface

  /// CHECK-START: void Main.testInterfaceToVirtualCall() inliner (after)
  /// CHECK:                          InvokeVirtual method_name:Main.doCall

  /// CHECK-START: void Main.testInterfaceToVirtualCall() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface
  public static void testInterfaceToVirtualCall() {
    methodWithInvokeInterface(m);
  }

  static Interface itf = new Main();
  static Main m = new Main();
}

interface Interface {
  public void doCall();
}
